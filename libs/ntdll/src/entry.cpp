#include "windef.h"
#include "ntpsapi.h"
#include "ntmmapi.h"

using PTHREAD_START_ROUTINE = DWORD (*)(LPVOID ctx);

using PPS_APC_ROUTINE = void (*)(PVOID arg1, PVOID arg2, PVOID arg3);

__declspec(dllexport) extern "C" [[gnu::used]]
void KiUserApcDispatcher(PCONTEXT ctx) {
	NTSTATUS status;
	do {
		auto apc_routine = reinterpret_cast<PPS_APC_ROUTINE>(ctx->P4Home);
		apc_routine(
			reinterpret_cast<PVOID>(ctx->P1Home),
			reinterpret_cast<PVOID>(ctx->P2Home),
			reinterpret_cast<PVOID>(ctx->P3Home));

		status = NtContinue(ctx, true);
	} while (status == STATUS_SUCCESS);
}

// apc executed by the kernel in each new thread
__declspec(dllexport) extern "C" [[gnu::used]]
void LdrInitializeThunk(PCONTEXT ctx, PVOID system_arg1, PVOID system_arg2) {
	// NtTestAlert to dispatch any pending apc's on the new thread
	NtContinue(ctx, true);
}

// used in NtCreateThreadEx context
__declspec(dllexport) extern "C" [[gnu::used]]
void RtlUserThreadStart(PTHREAD_START_ROUTINE start_routine, PVOID ctx) {
	start_routine(ctx);
	// todo exit
}
