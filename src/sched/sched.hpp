#pragma once
#include "thread.hpp"
#include "types.hpp"
#include "cstring.hpp"
#include "arch/arch_sched.hpp"
#include "utils/spinlock.hpp"
#include "dpc.hpp"
#include <hz/list.hpp>
#include <hz/new.hpp>
#include <hz/container_of.hpp>

struct Scheduler {
	explicit Scheduler(Cpu* cpu);

	void block();

	bool unblock(Thread* thread) REQUIRES(thread->lock);
	void sleep(u64 ns);

	void queue(Cpu* cpu, Thread* thread) EXCLUDES(thread->lock);

	static void on_timer(Cpu* cpu);

	void handle_quantum_end(Cpu* cpu);

	static constexpr usize REALTIME_START = 16;
	static constexpr usize CLOCK_INTERVAL_MS = 15;
	static constexpr usize QUANTUM_CLOCK_INTERVALS = 2;

private:
	void queue_private(Cpu* cpu, Thread* thread) REQUIRES(lock, thread->lock);
	void update_schedule(Cpu* cpu) REQUIRES(lock);

	struct Level {
		hz::list<Thread, &Thread::hook> threads;
	};

	static constexpr usize PROCESS_LEVEL_TABLE[] {
		// Idle
		4,
		// Normal
		8,
		// High
		13,
		// RealTime
		14,
		// BelowNormal
		6,
		// AboveNormal
		10
	};

	static usize get_thread_level(Thread* thread) {
		if (thread->priority == ThreadPriority::Idle) {
			if (thread->process->priority == ProcessPriority::RealTime) {
				return 16;
			}
			else {
				return 1;
			}
		}
		else if (thread->priority == ThreadPriority::TimeCritical) {
			if (thread->process->priority == ProcessPriority::RealTime) {
				return 31;
			}
			else {
				return 15;
			}
		}

		usize base = PROCESS_LEVEL_TABLE[static_cast<int>(thread->process->priority) - 1];
		base += static_cast<int>(thread->priority);
		return base;
	}

	Level levels[32] GUARDED_BY(lock) {};
	u32 ready_summary GUARDED_BY(lock) {};
	hz::list<Thread, &Thread::hook> sleeping_threads GUARDED_BY(lock) {};
	KSPIN_LOCK lock {};
	KDPC dpc {};
};

void sched_init();
