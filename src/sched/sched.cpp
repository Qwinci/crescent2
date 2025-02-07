#include "sched.hpp"
#include "arch/cpu.hpp"
#include "assert.hpp"
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
extern "C" void sched_switch_thread(Thread* prev, Thread* next, KIRQL new_irql);

void Scheduler::block() {
	auto current = get_current_thread();

	// raising the irql to DISPATCH_LEVEL also prevents preemption
	KIRQL irql = KeAcquireSpinLockRaiseToDpc(&lock);

	KeAcquireSpinLockAtDpcLevel(&current->lock);
	if (current->dont_block) {
		current->dont_block = false;
		KeReleaseSpinLockFromDpcLevel(&current->lock);
		KeReleaseSpinLock(&lock, irql);
		return;
	}

	current->status = ThreadStatus::Waiting;

	auto cpu = get_current_cpu();
	update_schedule(cpu);

	current->saved_irql = irql;

	// todo atomic relaxed store
	cpu->current_thread = cpu->next_thread;
	cpu->next_thread = nullptr;

	// keep irql at DISPATCH_LEVEL to prevent preemption
	KeReleaseSpinLock(&lock, DISPATCH_LEVEL);

	cpu->current_thread->status = ThreadStatus::Running;
	cpu->current_thread->process->page_map.use();

	sched_before_switch(current, cpu->current_thread);
	// todo make these relaxed atomic stores
	cpu->current_thread->last_run_start_cycles = get_cycle_count();
	asm volatile("" : : : "memory");
	cpu->quantum_end = false;
	asm volatile("" : : : "memory");
	sched_switch_thread(current, cpu->current_thread, cpu->current_thread->saved_irql);

	// restore old irql
	KeLowerIrql(irql);
}

void Scheduler::unblock(Thread* thread) {
	KIRQL irql = KeAcquireSpinLockRaiseToDpc(&thread->lock);

	if (thread->status != ThreadStatus::Waiting && thread->status != ThreadStatus::Sleeping) {
		if (thread->status == ThreadStatus::Running) {
			thread->dont_block = true;
		}
		KeReleaseSpinLock(&thread->lock, irql);
		return;
	}

	if (thread->status == ThreadStatus::Sleeping) {
		sleeping_threads.remove(thread);
	}

	queue(thread->cpu, thread);

	KeReleaseSpinLock(&thread->lock, irql);
}

void Scheduler::sleep(u64 ns) {
	if (!ns) {
		return;
	}

	auto current = get_current_thread();

	// raising the irql to DISPATCH_LEVEL also prevents preemption
	KIRQL irql = KeAcquireSpinLockRaiseToDpc(&lock);

	KeAcquireSpinLockAtDpcLevel(&current->lock);
	if (current->dont_block) {
		current->dont_block = false;
		KeReleaseSpinLockFromDpcLevel(&current->lock);
		KeReleaseSpinLock(&lock, irql);
		return;
	}

	u64 sleep_end = CLOCK_SOURCE->get_ns() + ns;

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

	current->saved_irql = irql;

	// todo atomic relaxed store
	cpu->current_thread = cpu->next_thread;
	cpu->next_thread = nullptr;

	// keep irql at DISPATCH_LEVEL to prevent preemption
	KeReleaseSpinLock(&lock, DISPATCH_LEVEL);

	cpu->current_thread->status = ThreadStatus::Running;
	cpu->current_thread->process->page_map.use();

	sched_before_switch(current, cpu->current_thread);
	// todo make these relaxed atomic stores
	cpu->current_thread->last_run_start_cycles = get_cycle_count();
	asm volatile("" : : : "memory");
	cpu->quantum_end = false;
	asm volatile("" : : : "memory");
	sched_switch_thread(current, cpu->current_thread, cpu->current_thread->saved_irql);

	// restore old irql
	KeLowerIrql(irql);
}

void Scheduler::queue(Cpu* cpu, Thread* thread) {
	KIRQL irql = KeAcquireSpinLockRaiseToDpc(&lock);
	queue_private(cpu, thread);
	KeReleaseSpinLock(&lock, irql);
}

void Scheduler::handle_quantum_end(Cpu* cpu) {
	KIRQL irql = KeAcquireSpinLockRaiseToDpc(&lock);

	auto now = CLOCK_SOURCE->get_ns();
	for (auto& thread : sleeping_threads) {
		if (thread.sleep_end_ns > now) {
			break;
		}

		KeAcquireSpinLockAtDpcLevel(&thread.lock);
		sleeping_threads.remove(&thread);
		queue(thread.cpu, &thread);
		KeReleaseSpinLockFromDpcLevel(&thread.lock);
	}

	update_schedule(cpu);

	cpu->quantum_end = false;

	if (!cpu->next_thread) {
		set_thread_expiry(cpu, cpu->current_thread);
		KeReleaseSpinLock(&lock, irql);
		return;
	}

	KeAcquireSpinLockAtDpcLevel(&cpu->current_thread->lock);

	auto prev = cpu->current_thread;
	prev->saved_irql = irql;

	queue_private(cpu, prev);
	cpu->current_thread = cpu->next_thread;
	cpu->next_thread = nullptr;

	KeReleaseSpinLock(&lock, irql);

	cpu->current_thread->status = ThreadStatus::Running;
	cpu->current_thread->process->page_map.use();

	sched_before_switch(prev, cpu->current_thread);
	cpu->current_thread->last_run_start_cycles = get_cycle_count();
	sched_switch_thread(prev, cpu->current_thread, cpu->current_thread->saved_irql);
}

void Scheduler::on_timer(Cpu* cpu) {
	auto now = get_cycle_count();

	auto current = cpu->current_thread;

	assert(now >= current->last_run_start_cycles);
	auto elapsed = now - current->last_run_start_cycles;

	if (elapsed < current->cycle_quota) {
		current->cycle_quota -= elapsed;
	}
	else {
		current->cycle_quota = 0;
		cpu->quantum_end = true;
		// quantum end processing is triggered when irql is lowered later on in arch-specific irq handler
	}

	cpu->tick_source->oneshot(Scheduler::CLOCK_INTERVAL_MS * 1000);
}

void sched_init() {

}
