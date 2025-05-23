#include "queued_spinlock.hpp"

NTAPI void KeAcquireInStackQueuedSpinLockAtDpcLevel(KSPIN_LOCK* lock, KLOCK_QUEUE_HANDLE* lock_handle) {
	lock_handle->lock_queue.next.store(nullptr, hz::memory_order::relaxed);
	lock_handle->lock_queue.lock = lock;

	auto old_tail = reinterpret_cast<KSPIN_LOCK_QUEUE*>(lock->value.exchange(
		reinterpret_cast<usize>(&lock_handle->lock_queue),
		hz::memory_order::acquire));
	if (!old_tail) {
		return;
	}

	// set the wait bit
	auto value = reinterpret_cast<usize>(lock_handle->lock_queue.lock);
	value |= 1;
	*reinterpret_cast<usize*>(lock_handle->lock_queue.lock) = value;

	old_tail->next.store(&lock_handle->lock_queue, hz::memory_order::relaxed);

	while (true) {
		value = reinterpret_cast<hz::atomic<usize>*>(lock_handle->lock_queue.lock)
			->load(hz::memory_order::relaxed);
		if (!(value & 1)) {
			break;
		}

#ifdef __x86_64__
		__builtin_ia32_pause();
#elif defined(__aarch64__)
		asm volatile("wfe");
#endif
	}
}

NTAPI void KeReleaseInStackQueuedSpinLockFromDpcLevel(KLOCK_QUEUE_HANDLE* lock_handle) {
	auto next = lock_handle->lock_queue.next.load(hz::memory_order::relaxed);
	if (next) {
		auto value = next->lock->value.load(hz::memory_order::relaxed);
		value &= ~1;
		next->lock->value.store(value, hz::memory_order::relaxed);
		return;
	}

	auto expected = reinterpret_cast<usize>(&lock_handle->lock_queue);
	auto success = lock_handle->lock_queue.lock->value.compare_exchange_strong(
		expected,
		0,
		hz::memory_order::release,
		hz::memory_order::relaxed);
	if (success) {
		return;
	}

	while (!(next = lock_handle->lock_queue.next.load(hz::memory_order::relaxed)));
	auto value = next->lock->value.load(hz::memory_order::relaxed);
	value &= ~1;
	next->lock->value.store(value, hz::memory_order::relaxed);
}

NTAPI void KeAcquireInStackQueuedSpinLock(KSPIN_LOCK* lock, KLOCK_QUEUE_HANDLE* lock_handle) {
	lock_handle->old_irql = KfRaiseIrql(DISPATCH_LEVEL);
	KeAcquireInStackQueuedSpinLockAtDpcLevel(lock, lock_handle);
}

NTAPI void KeReleaseInStackQueuedSpinLock(KLOCK_QUEUE_HANDLE* lock_handle) {
	auto old_irql = lock_handle->old_irql;
	KeReleaseInStackQueuedSpinLockFromDpcLevel(lock_handle);
	KeLowerIrql(old_irql);
}
