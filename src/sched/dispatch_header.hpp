#pragma once
#include "types.hpp"
#include "ntdef.h"
#include <hz/atomic.hpp>

struct DISPATCHER_HEADER {
	union {
		hz::atomic<i32> lock;

		struct {
			u8 type;
			u8 signalling;
			u8 size;
			u8 reserved;
		};
	};

	i32 signal_state;
	LIST_ENTRY wait_list_head;
};

inline void acquire_dispatch_header_lock(DISPATCHER_HEADER* header) {
	while (true) {
		if (!header->lock.exchange(1, hz::memory_order::acquire)) {
			break;
		}

		while (header->lock.load(hz::memory_order::relaxed)) {
#ifdef __x86_64__
			__builtin_ia32_pause();
#elif defined(__aarch64__)
			asm volatile("wfe");
#endif
		}
	}
}

inline void release_dispatch_header_lock(DISPATCHER_HEADER* header) {
	header->lock.store(0, hz::memory_order::release);
}
