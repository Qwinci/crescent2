#include "spinlock.hpp"

NO_THREAD_SAFETY_ANALYSIS
NTAPI void KeAcquireSpinLockAtDpcLevel(KSPIN_LOCK* lock) {
	while (true) {
		if (!lock->value.exchange(true, hz::memory_order::acquire)) {
			break;
		}

		while (lock->value.load(hz::memory_order::relaxed)) {
#ifdef __x86_64__
			__builtin_ia32_pause();
#elif defined(__aarch64__)
			asm volatile("wfe");
#endif
		}
	}
}

NO_THREAD_SAFETY_ANALYSIS
NTAPI void KeReleaseSpinLockFromDpcLevel(KSPIN_LOCK* lock) {
	lock->value.store(false, hz::memory_order::release);
}

NTAPI KIRQL KeAcquireSpinLockRaiseToDpc(KSPIN_LOCK* lock) {
	auto old = KfRaiseIrql(DISPATCH_LEVEL);
	KeAcquireSpinLockAtDpcLevel(lock);
	return old;
}

NTAPI void KeReleaseSpinLock(KSPIN_LOCK* lock, KIRQL new_irql) {
	KeReleaseSpinLockFromDpcLevel(lock);
	KeLowerIrql(new_irql);
}

namespace {
	constexpr u32 WRITER_WAITING = 1U << 31;
	constexpr u32 READER_MASK = (1U << 31) - 1;
}

NO_THREAD_SAFETY_ANALYSIS
NTAPI KIRQL ExAcquireSpinLockExclusive(EX_SPIN_LOCK* lock) {
	auto old = KfRaiseIrql(DISPATCH_LEVEL);

	auto value = lock->value.load(hz::memory_order::acquire);
	if ((value & READER_MASK) && !(value & WRITER_WAITING)) {
		while (true) {
			if (lock->value.compare_exchange_weak(
				value,
				value | WRITER_WAITING,
				hz::memory_order::relaxed,
				hz::memory_order::relaxed)) {
				break;
			}
		}
	}

	while (true) {
		while (value & READER_MASK) {
#ifdef __x86_64__
			__builtin_ia32_pause();
#elif defined(__aarch64__)
			asm volatile("wfe");
#endif
			value = lock->value.load(hz::memory_order::relaxed);
		}

		if (lock->value.compare_exchange_weak(
			value,
			READER_MASK,
			hz::memory_order::relaxed,
			hz::memory_order::acquire)) {
			break;
		}
	}

	return old;
}

NO_THREAD_SAFETY_ANALYSIS
NTAPI KIRQL ExAcquireSpinLockShared(EX_SPIN_LOCK* lock) {
	auto old = KfRaiseIrql(DISPATCH_LEVEL);

	auto value = lock->value.load(hz::memory_order::acquire);

	if (value & WRITER_WAITING) {
		while (value & WRITER_WAITING) {
#ifdef __x86_64__
			__builtin_ia32_pause();
#elif defined(__aarch64__)
			asm volatile("wfe");
#endif
			value = lock->value.load(hz::memory_order::relaxed);
		}
	}

	while (true) {
		while (value == READER_MASK) {
#ifdef __x86_64__
			__builtin_ia32_pause();
#elif defined(__aarch64__)
			asm volatile("wfe");
#endif
			value = lock->value.load(hz::memory_order::relaxed);
		}

		if (lock->value.compare_exchange_weak(
			value,
			value + 1,
			hz::memory_order::relaxed,
			hz::memory_order::acquire
			)) {
			break;
		}
	}

	return old;
}

NO_THREAD_SAFETY_ANALYSIS
NTAPI void ExReleaseSpinLockExclusive(EX_SPIN_LOCK* lock, KIRQL old_irql) {
	lock->value.store(0, hz::memory_order::release);
	KeLowerIrql(old_irql);
}

NO_THREAD_SAFETY_ANALYSIS
NTAPI void ExReleaseSpinLockShared(EX_SPIN_LOCK* lock, KIRQL old_irql) {
	lock->value.fetch_sub(1, hz::memory_order::release);
	KeLowerIrql(old_irql);
}

NO_THREAD_SAFETY_ANALYSIS
NTAPI u32 ExTryConvertSharedSpinLockExclusive(EX_SPIN_LOCK* lock) {
	auto value = lock->value.load(hz::memory_order::acquire);

	while (true) {
		if (value != 1) {
			return false;
		}

		if (lock->value.compare_exchange_weak(
			value,
			READER_MASK,
			hz::memory_order::relaxed,
			hz::memory_order::acquire)) {
			return true;
		}
	}
}
