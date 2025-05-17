#include "mutex.hpp"
#include "thread.hpp"
#include "wait.hpp"

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
