#include "dispatch_header.hpp"

NO_THREAD_SAFETY_ANALYSIS
void acquire_dispatch_header_lock(DISPATCHER_HEADER* header) {
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

NO_THREAD_SAFETY_ANALYSIS
void release_dispatch_header_lock(DISPATCHER_HEADER* header) {
	header->reserved.store(0, hz::memory_order::release);
}
