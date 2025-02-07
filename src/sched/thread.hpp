#pragma once
#include "arch/arch_thread.hpp"
#include "process.hpp"
#include "arch/arch_irql.hpp"
#include "string_view.hpp"
#include "ntdef.h"
#include <hz/list.hpp>

enum class ThreadPriority {
	Idle = -15,
	Lowest = -2,
	BelowNormal = -1,
	Normal = 0,
	AboveNormal,
	Highest,
	TimeCritical = 15
};

enum class ThreadStatus {
	Ready,
	Running,
	Waiting,
	Sleeping,
	Terminated
};

struct Cpu;

struct Thread : public ArchThread {
	Thread(kstd::wstring_view name, Cpu* cpu, Process* process);
	Thread(kstd::wstring_view name, Cpu* cpu, Process* process, void (*fn)(void*), void* arg);

	union {
		hz::list_hook hook {};
		LIST_ENTRY list_entry;
	};

	kstd::wstring name;
	Cpu* cpu;
	Process* process {};
	u64 last_run_start_cycles {};
	u64 cycle_quota {};
	ThreadPriority priority {ThreadPriority::Normal};
	ThreadStatus status {};
	KIRQL saved_irql {PASSIVE_LEVEL};
	KSPIN_LOCK lock {};
	volatile bool dont_block {};
	u64 sleep_end_ns {};
};

#ifdef __x86_64__
static_assert(offsetof(Thread, lock) == 184);
#else
#error unsupported architecture
#endif
