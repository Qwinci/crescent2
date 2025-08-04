#include "mutex.hpp"
#include "thread.hpp"
#include "wait.hpp"
#include "arch/arch_sched.hpp"
#include "atomic.hpp"

NTAPI void KeInitializeGuardedMutex(KGUARDED_MUTEX* mutex) {
	mutex->count = 0;
	mutex->owner = nullptr;
	mutex->contention = 0;
	KeInitializeEvent(&mutex->event, EVENT_TYPE::Synchronization, true);
	mutex->old_irql = 0;
}

NTAPI void KeAcquireGuardedMutex(KGUARDED_MUTEX* mutex) {
	KeEnterGuardedRegion();
	// todo this is expensive, try atomics first
	KeWaitForSingleObject(&mutex->event, Executive, KernelMode, false, nullptr);
}

NTAPI void KeReleaseGuardedMutex(KGUARDED_MUTEX* mutex) {
	KeSetEvent(&mutex->event, 0, false);
	KeLeaveGuardedRegion();
}

NTAPI void KeInitializeMutex(KMUTANT* mutex, ULONG) {
	mutex->header.type = MutantObject;
	mutex->header.size = sizeof(KMUTANT);
	mutex->header.reserved.store(0, hz::memory_order::relaxed);
	mutex->header.signal_state = 1;
	InitializeListHead(&mutex->header.wait_list_head);
	InitializeListHead(&mutex->mutant_list_entry);
	mutex->owner_thread = nullptr;
	mutex->mutant_flags = 0;
	mutex->apc_disable = true;
}

NTAPI LONG KeReleaseMutex(KMUTANT* mutex, BOOLEAN wait) {
	assert(mutex->owner_thread == get_current_thread());
	int old_state = atomic_fetch_add(&mutex->header.signal_state, 1, memory_order::release);

	bool enable_apc = false;

	if (old_state == 0) {
		auto old = KfRaiseIrql(DISPATCH_LEVEL);
		acquire_dispatch_header_lock(&mutex->header);

		mutex->owner_thread = nullptr;
		enable_apc = mutex->apc_disable;
		dispatch_header_queue_one_waiter(&mutex->header);

		release_dispatch_header_lock(&mutex->header);

		KeLowerIrql(old);
	}

	if (enable_apc) {
		KeLeaveCriticalRegion();
	}

	return old_state;
}

NTAPI void ExAcquireFastMutex(FAST_MUTEX* mutex) {
	return KeAcquireGuardedMutex(mutex);
}

NTAPI void ExReleaseFastMutex(FAST_MUTEX* mutex) {
	return KeReleaseGuardedMutex(mutex);
}
