#include "wait.hpp"
#include "arch/cpu.hpp"
#include "utils/export.hpp"
#include "assert.hpp"
#include <hz/container_of.hpp>

void dispatch_header_queue_one_waiter(DISPATCHER_HEADER* header) {
	if (IsListEmpty(&header->wait_list_head)) {
		return;
	}

	auto block = hz::container_of(RemoveHeadList(&header->wait_list_head), &KWAIT_BLOCK::wait_list_entry);
	auto thread = block->thread;
	block->thread = nullptr;
	KIRQL old = KeAcquireSpinLockRaiseToDpc(&thread->lock);
	thread->cpu->scheduler.unblock(thread);
	KeReleaseSpinLock(&thread->lock, old);
}

void dispatch_header_queue_all_waiters(DISPATCHER_HEADER* header) {
	KIRQL old = KfRaiseIrql(DISPATCH_LEVEL);

	while (!IsListEmpty(&header->wait_list_head)) {
		auto block = hz::container_of(RemoveHeadList(&header->wait_list_head), &KWAIT_BLOCK::wait_list_entry);
		auto thread = block->thread;
		block->thread = nullptr;
		KeAcquireSpinLockAtDpcLevel(&thread->lock);
		thread->cpu->scheduler.unblock(thread);
		KeReleaseSpinLockFromDpcLevel(&thread->lock);
	}

	KeLowerIrql(old);
}

static constexpr usize THREAD_WAIT_OBJECTS = 3;

EXPORT NTSTATUS KeWaitForMultipleObjects(
	u32 count,
	void* objects[],
	WaitType type,
	KWAIT_REASON wait_reason,
	KPROCESSOR_MODE wait_mode,
	bool alertable,
	i64* timeout,
	KWAIT_BLOCK* wait_block_array) {
	i64 real_timeout = 0;
	if (timeout) {
		real_timeout = *timeout;
	}

	KWAIT_BLOCK builtin_block[THREAD_WAIT_OBJECTS];
	if (count <= THREAD_WAIT_OBJECTS) {
		wait_block_array = builtin_block;
	}
	else {
		assert(wait_block_array);
	}

	auto thread = get_current_thread();

	for (u32 i = 0; i < count; ++i) {
		auto& block = wait_block_array[i];
		block.thread = thread;
	}

	auto old = KfRaiseIrql(DISPATCH_LEVEL);

	NTSTATUS status;
	again:
	u32 actual;
	if (type == WaitType::Any) {
		for (u32 i = 0; i < count; ++i) {
			auto header = static_cast<DISPATCHER_HEADER*>(objects[i]);
			auto* ptr = reinterpret_cast<hz::atomic<i32>*>(&header->signal_state);

			auto value = ptr->load(hz::memory_order::acquire);
			while (value) {
				if (ptr->compare_exchange_weak(
					value,
					value - 1,
					hz::memory_order::acquire,
					hz::memory_order::acquire)) {
					for (u32 j = 0; j < i; ++j) {
						auto& block = wait_block_array[j];
						header = static_cast<DISPATCHER_HEADER*>(objects[j]);

						acquire_dispatch_header_lock(header);

						if (block.thread) {
							RemoveEntryList(&block.wait_list_entry);
						}

						release_dispatch_header_lock(header);
					}

					thread->dont_block = false;
					KeLowerIrql(old);
					return static_cast<NTSTATUS>(i);
				}
			}

			auto& block = wait_block_array[i];
			acquire_dispatch_header_lock(header);
			InsertTailList(&header->wait_list_head, &block.wait_list_entry);
			release_dispatch_header_lock(header);
		}

		actual = count;
	}
	else {
		actual = 0;
		for (u32 i = 0; i < count; ++i) {
			auto header = static_cast<DISPATCHER_HEADER*>(objects[i]);
			auto* ptr = reinterpret_cast<hz::atomic<i32>*>(&header->signal_state);

			auto value = ptr->load(hz::memory_order::acquire);
			bool success = false;
			while (value) {
				if (ptr->compare_exchange_weak(
					value,
					value - 1,
					hz::memory_order::acquire,
					hz::memory_order::acquire)) {
					if (i == count - 1) {
						KeLowerIrql(old);
						return STATUS_SUCCESS;
					}
					success = true;
					break;
				}
			}

			if (success) {
				continue;
			}

			auto& block = wait_block_array[actual++];
			acquire_dispatch_header_lock(header);
			InsertTailList(&header->wait_list_head, &block.wait_list_entry);
			block.object = header;
			release_dispatch_header_lock(header);
		}
	}

	if (timeout) {
		if (real_timeout == 0) {
			status = STATUS_TIMEOUT;
			goto cleanup;
		}

		if (real_timeout > 0) {
			// todo
			panic("absolute timeout is not supported");
		}
		real_timeout *= -1;
		// timeout is in 100ns units
		real_timeout *= 100;

		auto value = real_timeout;
		real_timeout = 0;

		thread->cpu->scheduler.sleep(value);
		goto again;
	}
	else {
		thread->cpu->scheduler.block();
		goto again;
	}

	cleanup:
	for (u32 i = 0; i < actual; ++i) {
		auto& block = wait_block_array[i];
		auto header = static_cast<DISPATCHER_HEADER*>(block.object);

		acquire_dispatch_header_lock(header);

		if (block.thread) {
			RemoveEntryList(&block.wait_list_entry);
		}

		release_dispatch_header_lock(header);
	}

	thread->dont_block = false;
	KeLowerIrql(old);
	return status;
}

EXPORT NTSTATUS KeWaitForSingleObject(
	void* object,
	KWAIT_REASON wait_reason,
	KPROCESSOR_MODE wait_mode,
	bool alertable,
	i64* timeout) {
	return KeWaitForMultipleObjects(1, &object, WaitType::Any, wait_reason, wait_mode, alertable, timeout, nullptr);
}

