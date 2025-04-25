#pragma once
#include "types.hpp"
#include "ntdef.h"
#include "thread.hpp"
#include "dpc.hpp"
#include "dispatch_header.hpp"

struct KWAIT_BLOCK {
	LIST_ENTRY wait_list_entry;
	WAIT_TYPE wait_type;
	volatile u8 block_state;
	u16 wait_key;
	u32 spare;
	union {
		Thread* thread;
		KDPC* dpc;
	};
	void* object;
	void* spare_ptr;
};

enum KWAIT_REASON {
	Executive,
	FreePage,
	PageIn,
	PoolAllocation,
	DelayExecution,
	Suspended,
	UserRequest
};

void dispatch_header_queue_one_waiter(DISPATCHER_HEADER* header) REQUIRES(header);
void dispatch_header_queue_all_waiters(DISPATCHER_HEADER* header) REQUIRES(header);
NTAPI extern "C" NTSTATUS KeWaitForMultipleObjects(
	u32 count,
	void* objects[],
	WAIT_TYPE type,
	KWAIT_REASON wait_reason,
	KPROCESSOR_MODE wait_mode,
	bool alertable,
	i64* timeout,
	KWAIT_BLOCK* wait_block_array);
NTAPI extern "C" NTSTATUS KeWaitForSingleObject(
	void* object,
	KWAIT_REASON wait_reason,
	KPROCESSOR_MODE wait_mode,
	bool alertable,
	i64* timeout);

NTAPI extern "C" NTSTATUS NtWaitForMultipleObjects(
	ULONG object_count,
	PHANDLE object_array,
	WAIT_TYPE wait_type,
	BOOLEAN alertable,
	PLARGE_INTEGER timeout);
NTAPI extern "C" NTSTATUS NtWaitForSingleObject(
	HANDLE handle,
	BOOLEAN alertable,
	PLARGE_INTEGER timeout);
