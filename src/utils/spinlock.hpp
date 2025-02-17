#pragma once
#include "types.hpp"
#include "arch/irql.hpp"
#include "utils/thread_safety.hpp"
#include <hz/atomic.hpp>

struct CAPABILITY("spinlock") KSPIN_LOCK {
	hz::atomic<usize> value;
};

struct EX_SPIN_LOCK {
	hz::atomic<u32> value;
};

extern "C" void KeAcquireSpinLockAtDpcLevel(KSPIN_LOCK* lock) ACQUIRE(lock);
extern "C" void KeReleaseSpinLockFromDpcLevel(KSPIN_LOCK* lock) RELEASE(lock);
extern "C" KIRQL KeAcquireSpinLockRaiseToDpc(KSPIN_LOCK* lock) ACQUIRE(lock);
extern "C" void KeReleaseSpinLock(KSPIN_LOCK* lock, KIRQL new_irql) RELEASE(lock);

extern "C" KIRQL ExAcquireSpinLockExclusive(EX_SPIN_LOCK* lock);
extern "C" KIRQL ExAcquireSpinLockShared(EX_SPIN_LOCK* lock);
extern "C" void ExReleaseSpinLockExclusive(EX_SPIN_LOCK* lock, KIRQL old_irql);
extern "C" void ExReleaseSpinLockShared(EX_SPIN_LOCK* lock, KIRQL old_irql);
extern "C" u32 ExTryConvertSharedSpinLockExclusive(EX_SPIN_LOCK* lock);

#define KeAcquireSpinLock(lock, old_irql) *(old_irql) = KeAcquireSpinLockRaiseToDpc(lock)
