#include "dispatch_header.hpp"

void acquire_dispatch_header_lock(DISPATCHER_HEADER* header) ACQUIRE() {
	while (true) {
		if (!header->reserved.exchange(1, hz::memory_order::acquire)) {
			break;
		}

		while (header->reserved.load(hz::memory_order::relaxed)) {
#ifdef __x86_64__
			__builtin_ia32_pause();
#elif defined(__aarch64__)
			asm volatile("wfe");
#endif
		}
	}
}

void release_dispatch_header_lock(DISPATCHER_HEADER* header) RELEASE() {
	header->reserved.store(0, hz::memory_order::release);
}
