#include "semaphore.hpp"
#include "utils/export.hpp"
#include "arch/irql.hpp"
#include "wait.hpp"

EXPORT void KeInitializeSemaphore(KSEMAPHORE* semaphore, i32 count, i32 limit) {
	semaphore->header.reserved.store(0, hz::memory_order::relaxed);
	semaphore->header.signal_state = count;
	semaphore->limit = limit;
	InitializeListHead(&semaphore->header.wait_list_head);
}

EXPORT i32 KeReleaseSemaphore(KSEMAPHORE* semaphore, KPRIORITY increment, i32 adjustment, bool wait) {
	auto old_state = reinterpret_cast<hz::atomic<i32>*>(
		&semaphore->header.signal_state)->fetch_add(adjustment, hz::memory_order::release);
	if (old_state > 0) {
		return old_state;
	}

	auto old = KfRaiseIrql(DISPATCH_LEVEL);
	acquire_dispatch_header_lock(&semaphore->header);

	dispatch_header_queue_one_waiter(&semaphore->header);

	release_dispatch_header_lock(&semaphore->header);
	KeLowerIrql(old);

	return 0;
}
