#pragma once
#include "types.hpp"
#include "ntdef.h"
#include "utils/thread_safety.hpp"
#include <hz/atomic.hpp>

enum KOBJECTS : uint8_t {
	EventNotificationObject,
	EventSynchronizationObject,
	MutantObject,
	ProcessObject,
	QueueObject,
	SemaphoreObject,
	ThreadObject,
	GateObject,
	TimerNotificationObject,
	TimerSynchronizationObject
};

struct CAPABILITY("dispatch header") DISPATCHER_HEADER {
	union {
		u32 align;

		struct {
			KOBJECTS type;
			u8 signalling;
			u8 size;
			hz::atomic<u8> reserved;
		};
	};

	i32 signal_state;
	LIST_ENTRY wait_list_head;
};

void acquire_dispatch_header_lock(DISPATCHER_HEADER* header) ACQUIRE(header);
void release_dispatch_header_lock(DISPATCHER_HEADER* header) RELEASE(header);
