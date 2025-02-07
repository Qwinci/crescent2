#pragma once
#include "arch/arch_cpu.hpp"
#include "sched/sched.hpp"
#include "ntdef.h"
#include "utils/spinlock.hpp"

[[noreturn]] void sched_idle_fn(void*);

struct Cpu : public ArchCpu {
	constexpr explicit Cpu(u32 number) : ArchCpu {number} {}

	Scheduler scheduler {};
	Thread idle_thread {u"idle", this, &*KERNEL_PROCESS, sched_idle_fn, nullptr};
	u64 cycles_per_clock_interval {};
	SINGLE_LIST_ENTRY dpc_list_head {};
	SINGLE_LIST_ENTRY* dpc_list_tail {};
	KSPIN_LOCK dpc_list_lock {};
	bool quantum_end {};
};

extern kstd::vector<Cpu*> CPUS;
