#pragma once
#include "types.hpp"
#include "ntdef.h"

#define DPC_NORMAL 0
#define DPC_THREADED 1

struct KDPC;

using PKDEFERRED_ROUTINE = void (*)(KDPC* dpc, void* deferred_ctx, void* system_arg1, void* system_arg2);

enum class DpcImportance {
	Low,
	Medium,
	High,
	MediumHigh
};

struct KDPC {
	union {
		u32 target_info;
		struct {
			u8 type;
			u8 importance;
			volatile u16 number;
		};
	};
	SINGLE_LIST_ENTRY dpc_list_entry;
	KAFFINITY processor_history;
	PKDEFERRED_ROUTINE deferred_routine;
	void* deferred_ctx;
	void* system_arg1;
	void* system_arg2;
	volatile void* dpc_data;
};

NTAPI extern "C" void KeInitializeDpc(KDPC* dpc, PKDEFERRED_ROUTINE deferred_routine, void* deferred_ctx);
NTAPI extern "C" bool KeInsertQueueDpc(KDPC* dpc, void* system_arg1, void* system_arg2);
NTAPI extern "C" BOOLEAN KeRemoveQueueDpc(KDPC* dpc);
NTAPI extern "C" void KeSetImportanceDpc(KDPC* dpc, DpcImportance importance);
