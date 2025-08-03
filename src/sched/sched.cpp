#include "sched.hpp"
#include "arch/cpu.hpp"
#include "assert.hpp"
#include "utils/thread_safety.hpp"
#include "utils/shared_data.hpp"
#include "atomic.hpp"
#include <hz/bit.hpp>

namespace {
	constexpr u32 find_level(u32 mask) {
		return hz::bit_width(mask);
	}

	void set_thread_expiry(Cpu* cpu, Thread* thread) {
		thread->cycle_quota = Scheduler::QUANTUM_CLOCK_INTERVALS * cpu->cycles_per_clock_interval;
	}
}

[[noreturn]] void sched_idle_fn(void*) {
	while (true) {
		arch_hlt();
	}
}

Scheduler::Scheduler(Cpu* cpu) {
	// KeInitializeDpc can't be used here as the current cpu is not yet initialized
	dpc.importance = static_cast<u8>(DpcImportance::Medium);
	dpc.number = cpu->number;
	dpc.deferred_routine = [](KDPC* dpc, void* deferred_ctx, void* system_arg1, void* system_arg2) {
		auto* cpu = static_cast<Cpu*>(deferred_ctx);
		cpu->scheduler.handle_quantum_end(cpu);
	};
	dpc.deferred_ctx = cpu;
}

void Scheduler::queue_private(Cpu* cpu, Thread* thread) {
	thread->status = ThreadStatus::Ready;

	auto level = get_thread_level(thread);
	levels[level].threads.push(thread);
	ready_summary |= 1 << level;

	set_thread_expiry(cpu, thread);
}

void Scheduler::update_schedule(Cpu* cpu) {
	auto index = find_level(ready_summary);
	if (!index) {
		if (cpu->current_thread->status != ThreadStatus::Running) {
			cpu->next_thread = &cpu->idle_thread;
			return;
		}

		cpu->next_thread = nullptr;
		return;
	}

	auto& level = levels[index - 1];
	auto thread = level.threads.pop_front();
	assert(thread);
	if (level.threads.is_empty()) {
		ready_summary &= ~(1 << (index - 1));
	}

	cpu->next_thread = thread;
}

void sched_before_switch(Thread* prev, Thread* next);
extern "C" void sched_switch_thread(Thread* prev, Thread* next) RELEASE(prev->lock);

void Scheduler::yield() {
	auto current = get_current_thread();

	auto old = KfRaiseIrql(DISPATCH_LEVEL);
	auto cpu = current->cpu;

	KeAcquireSpinLockAtDpcLevel(&lock);
	KeAcquireSpinLockAtDpcLevel(&current->lock);

	bool current_affinity_allowed = current->affinity & 1ULL << current->cpu->number;

	if (!current_affinity_allowed) {
		current->status = ThreadStatus::Waiting;
	}

	update_schedule(cpu);
	cpu->quantum_end = false;

	if (current_affinity_allowed) {
		if (!cpu->next_thread) {
			set_thread_expiry(cpu, current);
			KeReleaseSpinLockFromDpcLevel(&current->lock);
			KeReleaseSpinLock(&lock, old);
			return;
		}

		queue_private(cpu, current);
	}
	else {
		for (auto cpu_it : CPUS) {
			if (current->affinity & 1ULL << cpu_it->number) {
				KeAcquireSpinLockAtDpcLevel(&cpu_it->scheduler.lock);
				cpu_it->scheduler.queue_private(cpu_it, current);
				KeReleaseSpinLockFromDpcLevel(&cpu_it->scheduler.lock);
				break;
			}
		}
	}

	cpu->current_thread = cpu->next_thread;
	cpu->next_thread = nullptr;

	KeReleaseSpinLockFromDpcLevel(&lock);

	cpu->current_thread->status = ThreadStatus::Running;
	cpu->current_thread->process->page_map.use();

	sched_before_switch(current, cpu->current_thread);
	cpu->current_thread->last_run_start_cycles = get_cycle_count();
	sched_switch_thread(current, cpu->current_thread);

	KeLowerIrql(old);
}

void Scheduler::block() {
	auto current = get_current_thread();

	// raising the irql to DISPATCH_LEVEL also prevents preemption
	auto old = KfRaiseIrql(DISPATCH_LEVEL);

	KeAcquireSpinLockAtDpcLevel(&current->lock);
	if (current->dont_block) {
		current->dont_block = false;
		KeReleaseSpinLock(&current->lock, old);
		return;
	}

	current->status = ThreadStatus::Waiting;

	auto cpu = get_current_cpu();

	KeAcquireSpinLockAtDpcLevel(&lock);

	update_schedule(cpu);

	cpu->current_thread = cpu->next_thread;
	cpu->next_thread = nullptr;

	KeReleaseSpinLockFromDpcLevel(&lock);

	cpu->current_thread->status = ThreadStatus::Running;
	cpu->current_thread->process->page_map.use();

	sched_before_switch(current, cpu->current_thread);
	cpu->current_thread->last_run_start_cycles = get_cycle_count();
	cpu->quantum_end = false;
	sched_switch_thread(current, cpu->current_thread);

	// restore old irql
	KeLowerIrql(old);
}

bool Scheduler::unblock(Thread* thread) {
	assert(KeGetCurrentIrql() == DISPATCH_LEVEL);
	assert(thread->lock.value.load(hz::memory_order::relaxed));

	assert(&thread->cpu->scheduler == this);

	if (thread->status != ThreadStatus::Waiting && thread->status != ThreadStatus::Sleeping) {
		if (thread->status == ThreadStatus::Running) {
			thread->dont_block = true;
		}
		return false;
	}

	KeAcquireSpinLockAtDpcLevel(&lock);

	if (thread->status == ThreadStatus::Sleeping) {
		sleeping_threads.remove(thread);
	}

	queue_private(thread->cpu, thread);

	KeReleaseSpinLockFromDpcLevel(&lock);

	return true;
}

void Scheduler::sleep(u64 ns) {
	if (!ns) {
		return;
	}

	auto current = get_current_thread();

	// raising the irql to DISPATCH_LEVEL also prevents preemption
	auto old = KfRaiseIrql(DISPATCH_LEVEL);

	KeAcquireSpinLockAtDpcLevel(&current->lock);
	if (current->dont_block) {
		current->dont_block = false;
		KeReleaseSpinLock(&current->lock, old);
		return;
	}

	u64 sleep_end = CLOCK_SOURCE->get_ns() + ns;

	KeAcquireSpinLockAtDpcLevel(&lock);

	Thread* next_sleeping = nullptr;
	for (auto& thread : sleeping_threads) {
		if (thread.sleep_end_ns > sleep_end) {
			next_sleeping = &thread;
			break;
		}
	}

	sleeping_threads.insert_before(next_sleeping, current);

	current->sleep_end_ns = sleep_end;
	current->status = ThreadStatus::Sleeping;

	auto cpu = get_current_cpu();
	update_schedule(cpu);

	cpu->current_thread = cpu->next_thread;
	cpu->next_thread = nullptr;

	KeReleaseSpinLockFromDpcLevel(&lock);

	cpu->current_thread->status = ThreadStatus::Running;
	cpu->current_thread->process->page_map.use();

	sched_before_switch(current, cpu->current_thread);
	cpu->current_thread->last_run_start_cycles = get_cycle_count();
	cpu->quantum_end = false;
	sched_switch_thread(current, cpu->current_thread);

	// restore old irql
	KeLowerIrql(old);
}

void Scheduler::queue(Cpu* cpu, Thread* thread) {
	KIRQL irql = KeAcquireSpinLockRaiseToDpc(&lock);
	KeAcquireSpinLockAtDpcLevel(&thread->lock);
	queue_private(cpu, thread);
	KeReleaseSpinLockFromDpcLevel(&thread->lock);
	KeReleaseSpinLock(&lock, irql);
}

void Scheduler::handle_quantum_end(Cpu* cpu) {
	assert(KeGetCurrentIrql() == DISPATCH_LEVEL);
	KeAcquireSpinLockAtDpcLevel(&lock);

	auto now = CLOCK_SOURCE->get_ns();
	for (auto& thread : sleeping_threads) {
		if (thread.sleep_end_ns > now) {
			break;
		}

		KeAcquireSpinLockAtDpcLevel(&thread.lock);
		sleeping_threads.remove(&thread);
		queue_private(thread.cpu, &thread);
		KeReleaseSpinLockFromDpcLevel(&thread.lock);
	}

	update_schedule(cpu);

	cpu->quantum_end = false;

	if (!cpu->next_thread) {
		set_thread_expiry(cpu, cpu->current_thread);
		KeReleaseSpinLockFromDpcLevel(&lock);
		return;
	}

	auto prev = cpu->current_thread;
	KeAcquireSpinLockAtDpcLevel(&prev->lock);

	queue_private(cpu, prev);
	cpu->current_thread = cpu->next_thread;
	cpu->next_thread = nullptr;

	KeReleaseSpinLockFromDpcLevel(&lock);

	cpu->current_thread->status = ThreadStatus::Running;
	cpu->current_thread->process->page_map.use();

	sched_before_switch(prev, cpu->current_thread);
	cpu->current_thread->last_run_start_cycles = get_cycle_count();
	sched_switch_thread(prev, cpu->current_thread);
}

void Scheduler::on_timer(Cpu* cpu) {
	auto now = get_cycle_count();

	if (cpu->number == 0) {
		u64 ticks = Scheduler::CLOCK_INTERVAL_MS * (NS_IN_MS / 100);
		atomic_fetch_add(&SharedUserData->system_time.u64, ticks, memory_order::relaxed);
	}

	auto current = cpu->current_thread;

	assert(now >= current->last_run_start_cycles);
	auto elapsed = now - current->last_run_start_cycles;

	if (elapsed < current->cycle_quota) {
		current->cycle_quota -= elapsed;
	}
	else {
		current->cycle_quota = 0;
		cpu->quantum_end = true;
		KeInsertQueueDpc(&cpu->scheduler.dpc, nullptr, nullptr);
	}

	cpu->tick_source->oneshot(Scheduler::CLOCK_INTERVAL_MS * 1000);
}

void sched_init() {

}
