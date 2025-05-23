#pragma once
#include "arch/arch_thread.hpp"
#include "process.hpp"
#include "arch/arch_irql.hpp"
#include "string_view.hpp"
#include "ntdef.h"
#include "fs/object.hpp"
#include "descriptors.hpp"
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

struct KAPC_STATE {
	LIST_ENTRY apc_list_head[MaxMode];
	Process* process;
	union {
		UCHAR in_progress_flags;
		struct {
			BOOLEAN kernel_apc_in_progress : 1;
			BOOLEAN special_apc_in_progress : 1;
		};
	};
	BOOLEAN kernel_apc_pending;
	union {
		BOOLEAN user_apc_pending_all;
		struct {
			BOOLEAN special_user_apc_pending : 1;
			BOOLEAN user_apc_pending : 1;
		};
	};
};

struct _TEB;

struct Thread : public ArchThread {
	Thread(kstd::wstring_view name, Cpu* cpu, Process* process);
	Thread(kstd::wstring_view name, Cpu* cpu, Process* process, bool user, void (*fn)(void*), void* arg);

	ThreadDescriptor* create_descriptor();

	void mark_as_exiting(int exit_status, ThreadDescriptor* skip_lock);

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
	KSPIN_LOCK lock {};
	bool dont_block {};
	u64 sleep_end_ns {};
	KPROCESSOR_MODE previous_mode {};
	KAPC_STATE apc_state {};
	KPROCESSOR_MODE wait_mode {};
	NTSTATUS wait_status {};
	bool exiting {};
	bool alertable {};
	// inside critical sections
	u32 kernel_apc_disable {};
	bool alerted[2] {};
	_TEB* teb {};
	hz::list<ThreadDescriptor, &ThreadDescriptor::hook> descriptors {};
	KSPIN_LOCK desc_lock {};
};

#ifdef __x86_64__
static_assert(offsetof(Thread, lock) == 176);
#else
#error unsupported architecture
#endif

NTAPI extern "C" BOOLEAN KeTestAlertThread(KPROCESSOR_MODE alert_mode);
NTAPI extern "C" void KeDelayExecutionThread(
	KPROCESSOR_MODE wait_mode,
	BOOLEAN alertable,
	PLARGE_INTEGER interval);

NTAPI extern "C" void KeEnterGuardedRegion();
NTAPI extern "C" void KeLeaveGuardedRegion();

extern "C" void user_thread_entry();
