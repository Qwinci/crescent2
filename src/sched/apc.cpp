#include "apc.hpp"
#include "arch/arch_sched.hpp"
#include "arch/arch_syscall.hpp"
#include "arch/cpu.hpp"
#include "sched/sched.hpp"
#include "arch/arch_irq.hpp"
#include "assert.hpp"
#include "exe/pe.hpp"
#include "dev/irq.hpp"
#include "atomic.hpp"
#include <hz/container_of.hpp>

NTAPI void KeInitializeApc(
	KAPC* apc,
	Thread* thread,
	KAPC_ENVIRONMENT environment,
	PKKERNEL_ROUTINE kernel_routine,
	PKRUNDOWN_ROUTINE rundown_routine,
	PKNORMAL_ROUTINE normal_routine,
	KPROCESSOR_MODE apc_mode,
	PVOID normal_ctx) {
	if (!normal_routine) {
		apc_mode = KernelMode;
		normal_ctx = nullptr;
	}

	*apc = {};
	apc->thread = thread;
	apc->kernel_routine = kernel_routine;
	apc->rundown_routine = rundown_routine;
	apc->normal_routine = normal_routine;
	apc->normal_ctx = normal_ctx;
	apc->apc_mode = apc_mode;
}

NTAPI BOOLEAN KeInsertQueueApc(
	KAPC* apc,
	PVOID system_arg1,
	PVOID system_arg2,
	KPRIORITY increment) {
	auto thread = apc->thread;

	auto old = KeAcquireSpinLockRaiseToDpc(&thread->lock);

	bool inserted = false;

	if (!apc->inserted && !thread->exiting) {
		apc->system_arg1 = system_arg1;
		apc->system_arg2 = system_arg2;
		apc->inserted = true;

		// special kernel mode apc's take priority
		if (!apc->normal_routine) {
			InsertHeadList(&thread->apc_state.apc_list_head[KernelMode], &apc->apc_list_entry);
		}
		else {
			InsertTailList(&thread->apc_state.apc_list_head[apc->apc_mode], &apc->apc_list_entry);
		}

		if (apc->apc_mode == KernelMode) {
			thread->apc_state.kernel_apc_pending = true;
		}
		else if (thread != get_current_thread()) {
			// todo also check if thread is suspended
			if (thread->status == ThreadStatus::Waiting &&
				thread->wait_mode == UserMode &&
				(thread->alertable || thread->apc_state.user_apc_pending)) {
				thread->wait_status = STATUS_USER_APC;
				if (thread->cpu->scheduler.unblock(thread)) {
					thread->apc_state.user_apc_pending = true;
				}
			}
		}

		inserted = true;
	}

	KeReleaseSpinLock(&thread->lock, old);

	return inserted;
}

NTAPI BOOLEAN KeAreAllApcsDisabled() {
	if (KeGetCurrentIrql() >= APC_LEVEL) {
		return true;
	}

	auto* thread = get_current_thread();
	return atomic_load(&thread->kernel_apc_disable, __ATOMIC_RELAXED);
}

void trap_frame_to_context(
	KTRAP_FRAME* trap_frame,
	KEXCEPTION_FRAME* exception_frame,
	CONTEXT* ctx);

void deliver_apc(
	KPROCESSOR_MODE previous_mode,
	KTRAP_FRAME* trap_frame,
	KEXCEPTION_FRAME* exception_frame) {
	assert(KeGetCurrentIrql() == APC_LEVEL);

	auto thread = get_current_thread();

	auto& kernel_list = thread->apc_state.apc_list_head[KernelMode];
	auto& user_list = thread->apc_state.apc_list_head[UserMode];

	KIRQL old = KeAcquireSpinLockRaiseToDpc(&thread->lock);

	thread->apc_state.kernel_apc_pending = false;

	while (!IsListEmpty(&kernel_list)) {
		thread->apc_state.kernel_apc_pending = false;

		auto* apc = hz::container_of(kernel_list.Flink, &KAPC::apc_list_entry);

		auto normal_routine = apc->normal_routine;
		auto kernel_routine = apc->kernel_routine;
		auto normal_ctx = apc->normal_ctx;
		auto system_arg1 = apc->system_arg1;
		auto system_arg2 = apc->system_arg2;

		if (!normal_routine) {
			// special kernel apc
			RemoveHeadList(&kernel_list);
			apc->inserted = false;

			KeReleaseSpinLock(&thread->lock, APC_LEVEL);

			kernel_routine(
				apc,
				&normal_routine,
				&normal_ctx,
				&system_arg1,
				&system_arg2);

			KeAcquireSpinLockRaiseToDpc(&thread->lock);
		}
		else {
			if (thread->apc_state.kernel_apc_in_progress ||
			    thread->kernel_apc_disable) {
				KeReleaseSpinLock(&thread->lock, APC_LEVEL);
				return;
			}

			RemoveHeadList(&kernel_list);
			apc->inserted = false;

			thread->alerted[KernelMode] = false;

			KeReleaseSpinLock(&thread->lock, APC_LEVEL);

			kernel_routine(
				apc,
				&normal_routine,
				&normal_ctx,
				&system_arg1,
				&system_arg2);

			if (normal_routine) {
				KeAcquireSpinLockRaiseToDpc(&thread->lock);
				thread->apc_state.kernel_apc_in_progress = true;
				KeReleaseSpinLock(&thread->lock, PASSIVE_LEVEL);

				normal_routine(
					normal_ctx,
					system_arg1,
					system_arg2);
			}

			KeAcquireSpinLockRaiseToDpc(&thread->lock);
			thread->apc_state.kernel_apc_in_progress = false;
		}
	}

	if (previous_mode != UserMode ||
		!thread->apc_state.user_apc_pending) {
		KeReleaseSpinLock(&thread->lock, old);
		return;
	}

	thread->apc_state.user_apc_pending = false;

	auto* apc = hz::container_of(RemoveHeadList(&user_list), &KAPC::apc_list_entry);
	apc->inserted = false;

	auto kernel_routine = apc->kernel_routine;
	auto normal_routine = apc->normal_routine;
	auto normal_ctx = apc->normal_ctx;
	auto system_arg1 = apc->system_arg1;
	auto system_arg2 = apc->system_arg2;

	thread->alerted[KernelMode] = false;

	KeReleaseSpinLock(&thread->lock, APC_LEVEL);

	kernel_routine(
		apc,
		&normal_routine,
		&normal_ctx,
		&system_arg1,
		&system_arg2);

	if (!normal_routine) {
		KeTestAlertThread(UserMode);
		return;
	}
	else {
		CONTEXT ctx {};
		ctx.context_flags = CONTEXT_FULL;
		trap_frame_to_context(trap_frame, exception_frame, &ctx);

		auto ptr = reinterpret_cast<CONTEXT*>(ALIGNDOWN(trap_frame->rsp - sizeof(CONTEXT), 16));

		ctx.p1_home = reinterpret_cast<ULONG64>(normal_ctx);
		ctx.p2_home = reinterpret_cast<ULONG64>(system_arg1);
		ctx.p3_home = reinterpret_cast<ULONG64>(system_arg2);
		ctx.p4_home = reinterpret_cast<ULONG64>(normal_routine);

		enable_user_access();
		__try {
			ProbeForRead(ptr, sizeof(CONTEXT), 8);
			*ptr = ctx;
			disable_user_access();
			trap_frame->rcx = reinterpret_cast<usize>(ptr);
			trap_frame->rip = thread->process->ntdll_base + NTDLL_OFFSETS.user_apc_dispatcher;
		}
		__except (1) {
			disable_user_access();
		}
	}

	KeLowerIrql(old);
}

bool apc_interrupt(KINTERRUPT*, void*, ULONG, KTRAP_FRAME* frame) {
	assert(KeGetCurrentIrql() == APC_LEVEL);
	deliver_apc(frame->previous_mode, frame, nullptr);
	return true;
}

KINTERRUPT APC_IRQ_HANDLER {
	.fn = reinterpret_cast<PKSERVICE_ROUTINE>(reinterpret_cast<void*>(apc_interrupt))
};

void deliver_user_start_apc(KTRAP_FRAME* trap_frame) {
	CONTEXT ctx {};
	ctx.context_flags = CONTEXT_FULL;
	trap_frame_to_context(trap_frame, nullptr, &ctx);

	auto ptr = reinterpret_cast<CONTEXT*>(ALIGNDOWN(trap_frame->rsp - sizeof(CONTEXT), 16));

	auto thread = get_current_thread();

	ctx.p1_home = reinterpret_cast<ULONG64>(ptr);
	ctx.p2_home = 0;
	ctx.p3_home = 0;
	ctx.p4_home = thread->process->ntdll_base + NTDLL_OFFSETS.ldr_initialize_thunk;

	enable_user_access();
	__try {
		ProbeForRead(ptr, sizeof(CONTEXT), 8);
		*ptr = ctx;
		disable_user_access();
		trap_frame->rcx = reinterpret_cast<usize>(ptr);
		trap_frame->rip = thread->process->ntdll_base + NTDLL_OFFSETS.user_apc_dispatcher;
	}
	__except (1) {
		disable_user_access();
	}
}
