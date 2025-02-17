#pragma once
#include "types.hpp"
#include "ntdef.h"
#include "thread.hpp"
#include "dpc.hpp"
#include "dispatch_header.hpp"

enum class WaitType : u8 {
	All = 0,
	Any = 1
};

struct KWAIT_BLOCK {
	LIST_ENTRY wait_list_entry;
	WaitType wait_type;
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

enum class KWAIT_REASON {
	Executive
};

void dispatch_header_queue_one_waiter(DISPATCHER_HEADER* header) REQUIRES(header);
void dispatch_header_queue_all_waiters(DISPATCHER_HEADER* header) REQUIRES(header);
extern "C" NTSTATUS KeWaitForMultipleObjects(
	u32 count,
	void* objects[],
	WaitType type,
	KWAIT_REASON wait_reason,
	KPROCESSOR_MODE wait_mode,
	bool alertable,
	i64* timeout,
	KWAIT_BLOCK* wait_block_array);
extern "C" NTSTATUS KeWaitForSingleObject(
	void* object,
	KWAIT_REASON wait_reason,
	KPROCESSOR_MODE wait_mode,
	bool alertable,
	i64* timeout);
