#include "arch/arch_syscall.hpp"
#include "arch/arch_sched.hpp"
#include "sched/thread.hpp"
#include "syscalls.hpp"
#include "misc.hpp"
#include "utils/except_internals.hpp"
#include <hz/array.hpp>
#include <hz/pair.hpp>

using SyscallFn = void (*)(SyscallFrame* frame);

extern "C" NTAPI NTSTATUS NtContinue(CONTEXT* ctx, BOOLEAN test_alert) {
	if (test_alert) {
		KeTestAlertThread(ExGetPreviousMode());
	}

	enable_user_access();
	__try {
		if (ExGetPreviousMode() == UserMode) {
			ProbeForRead(ctx, sizeof(CONTEXT), 8);
		}

		CONTEXT copy = *ctx;
		disable_user_access();
		RtlRestoreContext(&copy, nullptr);
	}
	__except (1) {
		disable_user_access();
		return GetExceptionCode();
	}
}

static constexpr auto SYSCALL_ARRAY = []() {
	hz::array<hz::pair<kstd::string_view, SyscallFn>, SYS_MAX> arr {};
	arr[SYS_ALLOCATE_VIRTUAL_MEMORY] = {"NtAllocateVirtualMemory", nullptr};
	arr[SYS_FREE_VIRTUAL_MEMORY] = {"NtFreeVirtualMemory", nullptr};
	arr[SYS_PROTECT_VIRTUAL_MEMORY] = {"NtProtectVirtualMemory", nullptr};
	arr[SYS_OPEN_FILE] = {"NtOpenFile", nullptr};
	arr[SYS_READ_FILE] = {"NtReadFile", nullptr};
	arr[SYS_CLOSE] = {"NtClose", nullptr};
	arr[SYS_CONTINUE] = {"NtContinue", [](SyscallFrame* frame) {
		*frame->ret() = NtContinue(reinterpret_cast<CONTEXT*>(*frame->arg0()), frame->arg1());
	}};
	return arr;
}();

extern "C" [[gnu::used]] void syscall_handler(SyscallFrame* frame) {
	auto thread = get_current_thread();
	thread->previous_mode = UserMode;

	if (frame->num() >= SYSCALL_ARRAY.size()) {
		panic("[kernel]: unimplemented syscall ", frame->num());
		*frame->ret() = STATUS_INVALID_PARAMETER;
		return;
	}
	auto& data = SYSCALL_ARRAY[frame->num()];
	if (!data.second) {
		if (data.first.size()) {
			panic("[kernel]: unimplemented syscall ", data.first);
		}
		else {
			panic("[kernel]: unimplemented syscall ", frame->num());
		}
	}

	println("[kernel]: syscall ", data.first);
	data.second(frame);
}
