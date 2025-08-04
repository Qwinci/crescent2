#include "wait.hpp"
#include "arch/cpu.hpp"
#include "assert.hpp"
#include "sys/misc.hpp"
#include "utils/except.hpp"
#include "arch/arch_syscall.hpp"
#include "mutex.hpp"
#include "atomic.hpp"
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

NTAPI NTSTATUS KeWaitForMultipleObjects(
	u32 count,
	void* objects[],
	WAIT_TYPE type,
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

	thread->alertable = alertable;
	thread->wait_mode = wait_mode;
	thread->wait_status = STATUS_SUCCESS;

	for (u32 i = 0; i < count; ++i) {
		auto& block = wait_block_array[i];
		block.thread = thread;
	}

	auto old = KfRaiseIrql(DISPATCH_LEVEL);

	if (alertable && wait_mode == UserMode) {
		KeAcquireSpinLockAtDpcLevel(&thread->lock);

		if (!IsListEmpty(&thread->apc_state.apc_list_head[UserMode])) {
			thread->apc_state.user_apc_pending = true;
			KeReleaseSpinLockFromDpcLevel(&thread->lock);
			return STATUS_USER_APC;
		}

		KeReleaseSpinLockFromDpcLevel(&thread->lock);
	}

	NTSTATUS status;
	again:
	if (wait_mode == UserMode) {
		KeAcquireSpinLockAtDpcLevel(&thread->lock);
		if (thread->wait_status == STATUS_USER_APC) {
			status = STATUS_USER_APC;
			thread->apc_state.user_apc_pending = true;
			KeReleaseSpinLockFromDpcLevel(&thread->lock);
			goto cleanup;
		}
		KeReleaseSpinLockFromDpcLevel(&thread->lock);
	}

	u32 actual;
	if (type == WaitAny) {
		for (u32 i = 0; i < count; ++i) {
			auto header = static_cast<DISPATCHER_HEADER*>(objects[i]);
			auto* ptr = reinterpret_cast<hz::atomic<i32>*>(&header->signal_state);

			auto value = ptr->load(hz::memory_order::acquire);

			if (header->type == MutantObject && value <= 0) {
				auto* mutant = reinterpret_cast<KMUTANT*>(header);
				if (atomic_load(&mutant->owner_thread, memory_order::relaxed) == thread) {
					ptr->fetch_sub(1, hz::memory_order::relaxed);

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

			while (value > 0) {
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

					if (header->type == MutantObject) {
						auto* mutant = reinterpret_cast<KMUTANT*>(header);
						mutant->owner_thread = thread;

						if (mutant->apc_disable) {
							KeEnterCriticalRegion();
						}
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

			if (header->type == MutantObject && value <= 0) {
				auto* mutant = reinterpret_cast<KMUTANT*>(header);
				if (atomic_load(&mutant->owner_thread, memory_order::relaxed) == thread) {
					ptr->fetch_sub(1, hz::memory_order::relaxed);

					if (i == count - 1) {
						KeLowerIrql(old);
						return STATUS_SUCCESS;
					}

					continue;
				}
			}

			bool success = false;
			while (value > 0) {
				if (ptr->compare_exchange_weak(
					value,
					value - 1,
					hz::memory_order::acquire,
					hz::memory_order::acquire)) {
					if (header->type == MutantObject) {
						auto* mutant = reinterpret_cast<KMUTANT*>(header);
						mutant->owner_thread = thread;

						if (mutant->apc_disable) {
							KeEnterCriticalRegion();
						}
					}

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
		thread->cpu->scheduler.block(ThreadStatus::Waiting);
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

NTAPI NTSTATUS KeWaitForSingleObject(
	void* object,
	KWAIT_REASON wait_reason,
	KPROCESSOR_MODE wait_mode,
	bool alertable,
	i64* timeout) {
	return KeWaitForMultipleObjects(1, &object, WaitAny, wait_reason, wait_mode, alertable, timeout, nullptr);
}

NTAPI NTSTATUS NtWaitForMultipleObjects(
	ULONG object_count,
	PHANDLE object_array,
	WAIT_TYPE wait_type,
	BOOLEAN alertable,
	PLARGE_INTEGER timeout) {
	auto* objects = static_cast<PVOID*>(kmalloc(object_count * sizeof(PVOID)));
	if (!objects) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	auto* blocks = static_cast<KWAIT_BLOCK*>(kcalloc(object_count * sizeof(KWAIT_BLOCK)));
	if (!blocks) {
		kfree(objects, object_count * sizeof(PVOID));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	auto mode = ExGetPreviousMode();

	LARGE_INTEGER timeout_value;
	if (mode == UserMode) {
		usize i = 0;
		__try {
			enable_user_access();

			ProbeForRead(object_array, object_count * sizeof(HANDLE), alignof(HANDLE));

			for (;i < object_count; ++i) {
				auto handle = object_array[i];
				auto status = ObReferenceObjectByHandle(
					handle,
					SYNCHRONIZE,
					nullptr,
					UserMode,
					&objects[i],
					nullptr);
				if (!NT_SUCCESS(status)) {
					for (usize j = 0; j < i; ++j) {
						ObfDereferenceObject(blocks[j].spare_ptr);
					}

					kfree(objects, object_count * sizeof(PVOID));
					kfree(blocks, object_count * sizeof(KWAIT_BLOCK));
					return status;
				}

				auto* type_info = object_get_type(objects[i]);
				blocks[i].spare_ptr = objects[i];
				objects[i] = offset(objects[i], PVOID, type_info->wait_object_pointer_offset);
			}

			if (timeout) {
				ProbeForRead(timeout, sizeof(LARGE_INTEGER), alignof(LARGE_INTEGER));
				timeout_value = *timeout;
			}

			disable_user_access();
		}
		__except (1) {
			disable_user_access();

			for (usize j = 0; j < i; ++j) {
				ObfDereferenceObject(blocks[j].spare_ptr);
			}

			kfree(objects, object_count * sizeof(PVOID));
			kfree(blocks, object_count * sizeof(KWAIT_BLOCK));
			return GetExceptionCode();
		}
	}

	auto status = KeWaitForMultipleObjects(
		object_count,
		objects,
		wait_type,
		UserRequest,
		mode,
		alertable,
		timeout ? &timeout_value.QuadPart : nullptr,
		blocks);

	for (usize j = 0; j < object_count; ++j) {
		ObfDereferenceObject(blocks[j].spare_ptr);
	}

	kfree(objects, object_count * sizeof(PVOID));
	kfree(blocks, object_count * sizeof(KWAIT_BLOCK));
	return status;
}

NTAPI NTSTATUS NtWaitForSingleObject(
	HANDLE handle,
	BOOLEAN alertable,
	PLARGE_INTEGER timeout) {
	PVOID object;
	auto mode = ExGetPreviousMode();

	LARGE_INTEGER timeout_value;
	if (mode == UserMode) {
		if (timeout) {
			__try {
				enable_user_access();
				ProbeForRead(timeout, sizeof(LARGE_INTEGER), alignof(LARGE_INTEGER));
				timeout_value = *timeout;
				disable_user_access();
			}
			__except (1) {
				disable_user_access();
				return GetExceptionCode();
			}
		}
	}

	auto status = ObReferenceObjectByHandle(
		handle,
		SYNCHRONIZE,
		nullptr,
		mode,
		&object,
		nullptr);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	auto* type_info = object_get_type(object);

	auto dispatch_object = offset(object, PVOID, type_info->wait_object_pointer_offset);
	status = KeWaitForSingleObject(
		dispatch_object,
		UserRequest,
		mode,
		alertable,
		timeout ? &timeout_value.QuadPart : nullptr);

	ObfDereferenceObject(object);
	return status;
}
