#pragma once
#include "types.hpp"
#include "arch/irql.hpp"
#include "utils/thread_safety.hpp"
#include <hz/atomic.hpp>

struct CAPABILITY("spinlock") KSPIN_LOCK {
	hz::atomic<usize> value;
};

struct CAPABILITY("Shared spinlock") EX_SPIN_LOCK {
	hz::atomic<u32> value;
};

NTAPI extern "C" void KeAcquireSpinLockAtDpcLevel(KSPIN_LOCK* lock) ACQUIRE(lock);
NTAPI extern "C" void KeReleaseSpinLockFromDpcLevel(KSPIN_LOCK* lock) RELEASE(lock);
NTAPI extern "C" KIRQL KeAcquireSpinLockRaiseToDpc(KSPIN_LOCK* lock) ACQUIRE(lock);
NTAPI extern "C" void KeReleaseSpinLock(KSPIN_LOCK* lock, KIRQL new_irql) RELEASE(lock);

NTAPI extern "C" KIRQL ExAcquireSpinLockExclusive(EX_SPIN_LOCK* lock) ACQUIRE(lock);
NTAPI extern "C" KIRQL ExAcquireSpinLockShared(EX_SPIN_LOCK* lock) ACQUIRE_SHARED(lock);
NTAPI extern "C" void ExReleaseSpinLockExclusive(EX_SPIN_LOCK* lock, KIRQL old_irql) RELEASE(lock);
NTAPI extern "C" void ExReleaseSpinLockShared(EX_SPIN_LOCK* lock, KIRQL old_irql) RELEASE_SHARED(lock);
NTAPI extern "C" u32 ExTryConvertSharedSpinLockExclusive(EX_SPIN_LOCK* lock) RELEASE_SHARED(lock) ACQUIRE(lock);

#define KeAcquireSpinLock(lock, old_irql) *(old_irql) = KeAcquireSpinLockRaiseToDpc(lock)
