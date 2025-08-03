#include "thread.hpp"
#include "arch/arch_sched.hpp"
#include "priv/peb.h"
#include "assert.hpp"
#include "fs/object.hpp"
#include "arch/cpu.hpp"
#include "apc.hpp"
#include "arch/irq.hpp"

Thread::Thread(kstd::wstring_view name, Cpu* cpu, Process* process)
	: ArchThread {}, name {name}, cpu {cpu}, process {process} {
	InitializeListHead(&apc_state.apc_list_head[0]);
	InitializeListHead(&apc_state.apc_list_head[1]);
}

Thread::Thread(kstd::wstring_view name, Cpu* cpu, Process* process, bool user, void (*fn)(void*), void* arg)
	: ArchThread {fn, arg, process, user}, name {name}, cpu {cpu}, process {process} {
	InitializeListHead(&apc_state.apc_list_head[0]);
	InitializeListHead(&apc_state.apc_list_head[1]);

	if (user) {
		UniqueKernelMapping tmp_teb_mapping;
		teb = reinterpret_cast<TEB*>(process->allocate(
			nullptr,
			sizeof(TEB),
			PageFlags::Read | PageFlags::Write,
			MappingFlags::Backed,
			&tmp_teb_mapping));
		assert(teb);
		auto* tmp_teb = new (tmp_teb_mapping.data()) TEB {};
		tmp_teb->ProcessEnvironmentBlock = process->peb;
		tmp_teb->NtTib.Self = &teb->NtTib;
	}
}

ThreadDescriptor::~ThreadDescriptor() {
	auto old = KeAcquireSpinLockRaiseToDpc(&lock);

	if (thread) {
		KeAcquireSpinLockAtDpcLevel(&thread->desc_lock);
		thread->descriptors.remove(this);
		KeReleaseSpinLockFromDpcLevel(&thread->desc_lock);
	}

	KeReleaseSpinLock(&lock, old);
}

void Thread::mark_as_exiting(int exit_status, ThreadDescriptor* skip_lock) {
	assert(KeGetCurrentIrql() >= DISPATCH_LEVEL);
	KeAcquireSpinLockAtDpcLevel(&lock);
	exiting = true;
	KeReleaseSpinLockFromDpcLevel(&lock);

	KeAcquireSpinLockAtDpcLevel(&desc_lock);

	for (auto& desc : descriptors) {
		if (&desc != skip_lock) {
			KeAcquireSpinLockAtDpcLevel(&desc.lock);
			assert(desc.thread == this);
			desc.thread = nullptr;
			desc.status = exit_status;
			KeReleaseSpinLockFromDpcLevel(&desc.lock);
		}
		else {
			assert(desc.thread == this);
			desc.thread = nullptr;
			desc.status = exit_status;
		}
	}

	descriptors.clear();

	KeReleaseSpinLockFromDpcLevel(&desc_lock);
}

ThreadDescriptor* Thread::create_descriptor() {
	/*ObjectWrapper<ThreadDescriptor> desc {};
	desc->thread = this;

	auto old = KeAcquireSpinLockRaiseToDpc(&desc_lock);

	descriptors.push(desc.get());

	KeReleaseSpinLock(&desc_lock, old);

	return desc;*/
	assert(false);
	// todo
}

BOOLEAN KeTestAlertThread(KPROCESSOR_MODE alert_mode) {
	auto thread = get_current_thread();
	auto old = KeAcquireSpinLockRaiseToDpc(&thread->lock);

	auto state = thread->alerted[alert_mode];
	if (state) {
		thread->alerted[alert_mode] = false;
	}
	else if (alert_mode == UserMode) {
		if (!IsListEmpty(&thread->apc_state.apc_list_head[UserMode])) {
			thread->apc_state.user_apc_pending = true;
		}
	}

	KeReleaseSpinLock(&thread->lock, old);
	return state;
}

NTAPI void KeDelayExecutionThread(
	KPROCESSOR_MODE wait_mode,
	BOOLEAN alertable,
	PLARGE_INTEGER interval) {
	KIRQL old = KfRaiseIrql(DISPATCH_LEVEL);

	auto* thread = get_current_thread();
	thread->alertable = alertable;
	thread->wait_mode = wait_mode;
	thread->wait_status = STATUS_SUCCESS;

	assert(interval->QuadPart <= 0);
	u64 ns = static_cast<u64>(-interval->QuadPart) * 100;

	thread->cpu->scheduler.sleep(ns);
	KeLowerIrql(old);
}

NTAPI void KeEnterGuardedRegion() {
	assert(KeGetCurrentIrql() <= APC_LEVEL);
	auto* thread = get_current_thread();
	// todo maybe atomic instead of lock here
	auto old = KeAcquireSpinLockRaiseToDpc(&thread->lock);
	++thread->kernel_apc_disable;
	KeReleaseSpinLock(&thread->lock, old);
}

NTAPI void KeLeaveGuardedRegion() {
	assert(KeGetCurrentIrql() <= APC_LEVEL);
	auto* thread = get_current_thread();
	// todo maybe atomic instead of lock here
	auto old = KeAcquireSpinLockRaiseToDpc(&thread->lock);

	if (--thread->kernel_apc_disable == 0) {
		if (!IsListEmpty(&thread->apc_state.apc_list_head[KernelMode])) {
			if (KeGetCurrentIrql() == PASSIVE_LEVEL) {
				KeReleaseSpinLock(&thread->lock, old);
				KfRaiseIrql(APC_LEVEL);
				deliver_apc(KernelMode, nullptr, nullptr);
				KeLowerIrql(PASSIVE_LEVEL);
			}
			else {
				thread->apc_state.kernel_apc_pending = true;
				KeReleaseSpinLock(&thread->lock, old);
				arch_request_software_irq(APC_LEVEL);
			}
		}
		else {
			KeReleaseSpinLock(&thread->lock, old);
		}
	}
	else {
		KeReleaseSpinLock(&thread->lock, old);
	}
}

NTAPI KAFFINITY KeSetSystemAffinityThreadEx(KAFFINITY affinity) {
	auto* thread = get_current_thread();
	auto old = KeAcquireSpinLockRaiseToDpc(&thread->lock);

	KAFFINITY ret;
	if (!thread->saved_user_affinity) {
		thread->saved_user_affinity = thread->affinity;
		ret = 0;
	}
	else {
		ret = thread->affinity;
	}

	thread->affinity = affinity;

	KeReleaseSpinLock(&thread->lock, old);

	if (old < DISPATCH_LEVEL) {
		thread->cpu->scheduler.yield();
	}

	return ret;
}

NTAPI void KeRevertToUserAffinityThreadEx(KAFFINITY affinity) {
	auto* thread = get_current_thread();
	auto old = KeAcquireSpinLockRaiseToDpc(&thread->lock);

	if (affinity) {
		thread->affinity = affinity;
	}
	else {
		thread->affinity = thread->saved_user_affinity.value();
		thread->saved_user_affinity.reset();
	}

	KeReleaseSpinLock(&thread->lock, old);

	if (old < DISPATCH_LEVEL) {
		thread->cpu->scheduler.yield();
	}
}
