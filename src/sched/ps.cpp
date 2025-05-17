#include "ps.hpp"
#include "thread.hpp"
#include "arch/cpu.hpp"
#include "handle_table.hpp"

NTAPI NTSTATUS PsCreateSystemThread(
	PHANDLE thread_handle,
	ULONG desired_access,
	OBJECT_ATTRIBUTES* object_attribs,
	HANDLE process_handle,
	CLIENT_ID* client_id,
	PKSTART_ROUTINE start_routine,
	PVOID start_ctx) {
	/*auto* cpu = get_current_cpu();

	Thread* thread;

	hz::optional<Handle> generic_proc_handle {};
	if (process_handle != nullptr) {
		generic_proc_handle = KERNEL_PROCESS->handle_table.get(process_handle);
		ObjectWrapper<ProcessDescriptor>* desc_ptr;
		if (!generic_proc_handle || !(desc_ptr = generic_proc_handle->get<ObjectWrapper<ProcessDescriptor>>())) {
			return STATUS_INVALID_PARAMETER;
		}

		auto& desc = *desc_ptr;

		auto old = KeAcquireSpinLockRaiseToDpc(&desc->lock);

		// todo add thread to process
		thread = new Thread {u"system thread", cpu, desc->process, false, start_routine, start_ctx};

		KeReleaseSpinLock(&desc->lock, old);
	}
	else {
		thread = new Thread {u"system thread", cpu, &*KERNEL_PROCESS, false, start_routine, start_ctx};
	}

	thread->kernel_apc_disable = true;

	auto desc = thread->create_descriptor();

	auto result_handle = KERNEL_HANDLE_TABLE.insert(Handle {std::move(desc)});
	if (result_handle == INVALID_HANDLE_VALUE) {
		delete thread;
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	*thread_handle = result_handle;

	assert(client_id == nullptr);

	cpu->scheduler.queue(cpu, thread);
	return STATUS_SUCCESS;*/
	return STATUS_SUCCESS;
}
