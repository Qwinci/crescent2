#include "windef.h"
#include <stdint.h>

using PTHREAD_START_ROUTINE = DWORD (*)(LPVOID ctx);

// todo maybe match this?
#if defined(__x86_64__)
struct AMD64_CONTEXT {
	uint64_t rip;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t r8;
	uint64_t r9;
};
using PCONTEXT = AMD64_CONTEXT*;
#endif

extern "C" void LdrInitializeThunk(PCONTEXT ctx, PVOID system_arg1, PVOID system_arg2) {
	asm volatile("syscall" : : "a"(0));
}

extern "C" void RtlUserThreadStart(PTHREAD_START_ROUTINE start_routine, PVOID ctx) {
	start_routine(ctx);
	// todo exit
}

extern "C" void _DllMainCRTStartup() {
	asm volatile("syscall" : : "a"(2));
}
