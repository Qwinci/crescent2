#pragma once
#include "spinlock.hpp"

struct KSPIN_LOCK_QUEUE {
	hz::atomic<KSPIN_LOCK_QUEUE*> next;
	KSPIN_LOCK* lock;
};

struct KLOCK_QUEUE_HANDLE {
	KSPIN_LOCK_QUEUE lock_queue;
	KIRQL old_irql;
};

NTAPI extern "C" void KeAcquireInStackQueuedSpinLockAtDpcLevel(KSPIN_LOCK* lock, KLOCK_QUEUE_HANDLE* lock_handle);
NTAPI extern "C" void KeReleaseInStackQueuedSpinLockFromDpcLevel(KLOCK_QUEUE_HANDLE* lock_handle);
NTAPI extern "C" void KeAcquireInStackQueuedSpinLock(KSPIN_LOCK* lock, KLOCK_QUEUE_HANDLE* lock_handle);
NTAPI extern "C" void KeReleaseInStackQueuedSpinLock(KLOCK_QUEUE_HANDLE* lock_handle);
