#include "event.hpp"
#include "arch/irql.hpp"
#include "wait.hpp"
#include "rtl.hpp"

NTAPI extern "C" OBJECT_TYPE* ExEventObjectType = nullptr;

void event_init() {
	UNICODE_STRING name = RTL_CONSTANT_STRING(u"Event Object");
	OBJECT_TYPE_INITIALIZER init {};
	auto status = ObCreateObjectType(&name, &init, nullptr, &ExEventObjectType);
	assert(NT_SUCCESS(status));
}

void KeInitializeEvent(KEVENT* event, EVENT_TYPE type, BOOLEAN state) {
	event->header.type = static_cast<u8>(type);
	event->header.reserved.store(0, hz::memory_order::relaxed);
	event->header.signal_state = state;
	InitializeListHead(&event->header.wait_list_head);
}

LONG KeSetEvent(KEVENT* event, KPRIORITY increment, BOOLEAN wait) {
	int old_state;
	if (event->header.type == static_cast<u8>(EVENT_TYPE::Synchronization)) {
		old_state = reinterpret_cast<hz::atomic<i32>*>(
			&event->header.signal_state)->fetch_add(1, hz::memory_order::release);
	}
	else {
		old_state = reinterpret_cast<hz::atomic<i32>*>(
			&event->header.signal_state)->exchange(INT32_MAX, hz::memory_order::release);
	}

	if (old_state > 0) {
		return old_state;
	}

	auto old = KfRaiseIrql(DISPATCH_LEVEL);
	acquire_dispatch_header_lock(&event->header);

	if (event->header.type == static_cast<u8>(EVENT_TYPE::Synchronization)) {
		dispatch_header_queue_one_waiter(&event->header);
	}
	else {
		dispatch_header_queue_all_waiters(&event->header);
	}

	release_dispatch_header_lock(&event->header);
	KeLowerIrql(old);

	return 0;
}

void KeClearEvent(KEVENT* event) {
	reinterpret_cast<hz::atomic<i32>*>(
		&event->header.signal_state)->store(0, hz::memory_order::acquire);
}

LONG KeResetEvent(KEVENT* event) {
	return reinterpret_cast<hz::atomic<i32>*>(
		&event->header.signal_state)->exchange(0, hz::memory_order::acquire);
}

NTAPI LONG KeReadStateEvent(KEVENT* event) {
	return reinterpret_cast<hz::atomic<i32>*>(
		&event->header.signal_state)->load(hz::memory_order::relaxed);
}
