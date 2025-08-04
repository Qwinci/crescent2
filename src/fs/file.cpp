#include "file.hpp"
#include "sys/misc.hpp"
#include "arch/arch_sched.hpp"
#include "sched/thread.hpp"
#include "dev/pnp.hpp"
#include "sys/user_access.hpp"
#include "arch/arch_syscall.hpp"
#include "utils/except.hpp"
#include "cstring.hpp"

NTAPI extern "C" OBJECT_TYPE* IoFileObjectType;

NTAPI NTSTATUS NtCreateFile(
	PHANDLE handle,
	ACCESS_MASK desired_access,
	OBJECT_ATTRIBUTES* attribs,
	IO_STATUS_BLOCK* io_status_block,
	PLARGE_INTEGER allocation_size,
	ULONG file_attribs,
	ULONG share_access,
	ULONG create_disposition,
	ULONG create_options,
	PVOID ea_buffer,
	ULONG ea_length) {
	FileParseCtx ctx {
		.status_block = io_status_block,
		.allocation_size = allocation_size ? *allocation_size : LARGE_INTEGER {},
		.file_attribs = file_attribs,
		.share_access = share_access,
		.create_disposition = create_disposition,
		.create_options = create_options,
		.ea_buffer = ea_buffer,
		.ea_length = ea_length
	};
	return ObOpenObjectByName(
		attribs,
		IoFileObjectType,
		ExGetPreviousMode(),
		nullptr,
		desired_access,
		&ctx,
		handle);
}

NTAPI NTSTATUS ZwCreateFile(
	PHANDLE handle,
	ACCESS_MASK desired_access,
	OBJECT_ATTRIBUTES* attribs,
	IO_STATUS_BLOCK* io_status_block,
	PLARGE_INTEGER allocation_size,
	ULONG file_attribs,
	ULONG share_access,
	ULONG create_disposition,
	ULONG create_options,
	PVOID ea_buffer,
	ULONG ea_length) {
	auto* thread = get_current_thread();
	auto old = thread->previous_mode;
	thread->previous_mode = KernelMode;
	auto ret = NtCreateFile(
		handle,
		desired_access,
		attribs,
		io_status_block,
		allocation_size,
		file_attribs,
		share_access,
		create_disposition,
		create_options,
		ea_buffer,
		ea_length);
	thread->previous_mode = old;
	return ret;
}

NTAPI NTSTATUS NtOpenFile(
	PHANDLE handle,
	ACCESS_MASK desired_access,
	OBJECT_ATTRIBUTES* attribs,
	IO_STATUS_BLOCK* io_status_block,
	ULONG share_access,
	ULONG open_options) {
	return NtCreateFile(
		handle,
		desired_access,
		attribs,
		io_status_block,
		nullptr,
		0,
		share_access,
		0,
		open_options,
		nullptr,
		0);
}

NTAPI NTSTATUS ZwOpenFile(
	PHANDLE handle,
	ACCESS_MASK desired_access,
	OBJECT_ATTRIBUTES* attribs,
	IO_STATUS_BLOCK* io_status_block,
	ULONG share_access,
	ULONG open_options) {
	return ZwCreateFile(
		handle,
		desired_access,
		attribs,
		io_status_block,
		nullptr,
		0,
		share_access,
		0,
		open_options,
		nullptr,
		0);
}

NTAPI extern "C" NTSTATUS NtReadFile(
	HANDLE file_handle,
	HANDLE event,
	PIO_APC_ROUTINE apc_routine,
	PVOID apc_ctx,
	IO_STATUS_BLOCK* io_status_block,
	PVOID buffer,
	ULONG length,
	PLARGE_INTEGER byte_offset,
	PULONG key) {
	auto mode = ExGetPreviousMode();

	LARGE_INTEGER byte_offset_value {};
	ULONG key_value {};
	if (mode == UserMode) {
		__try {
			enable_user_access();

			ProbeForRead(io_status_block, sizeof(IO_STATUS_BLOCK), alignof(IO_STATUS_BLOCK));
			if (byte_offset) {
				ProbeForRead(byte_offset, sizeof(LARGE_INTEGER), alignof(LARGE_INTEGER));
				byte_offset_value = *byte_offset;
			}

			if (key) {
				ProbeForRead(key, sizeof(ULONG), alignof(ULONG));
				key_value = *key;
			}

			ProbeForRead(buffer, length, 1);

			disable_user_access();
		}
		__except (1) {
			disable_user_access();
			return GetExceptionCode();
		}
	}

	FILE_OBJECT* file;
	auto status = ObReferenceObjectByHandle(
		file_handle,
		0,
		IoFileObjectType,
		mode,
		reinterpret_cast<PVOID*>(&file),
		nullptr);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	KEVENT* event_obj = nullptr;
	if (event) {
		status = ObReferenceObjectByHandle(
			event,
			0,
			ExEventObjectType,
			mode,
			reinterpret_cast<PVOID*>(&event_obj),
			nullptr);
		if (!NT_SUCCESS(status)) {
			ObfDereferenceObject(file);
			return status;
		}
	}

	auto* dev = file->device;

	IRP* irp = IoAllocateIrp(dev->stack_size, false);
	if (!irp) {
		ObfDereferenceObject(file);
		ObfDereferenceObject(event_obj);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	if (dev->flags & DO_BUFFERED_IO) {
		void* system_buffer = ExAllocatePool2(0, length, 0);
		if (!system_buffer) {
			ObfDereferenceObject(file);
			ObfDereferenceObject(event_obj);
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		irp->associated_irp.system_buffer = system_buffer;

		IoSetCompletionRoutine(irp, [](DEVICE_OBJECT* device, IRP* irp, void* ctx) {
			if (irp->io_status.status == STATUS_SUCCESS) {
				__try {
					enable_user_access();
					memcpy(ctx, irp->associated_irp.system_buffer, irp->io_status.info);
					disable_user_access();
				}
				__except (1) {
					disable_user_access();
					irp->io_status.status = GetExceptionCode();
				}
			}

			ExFreePool(irp->associated_irp.system_buffer);

			return STATUS_SUCCESS;
		}, buffer, true, true, true);
	}
	else {
		// todo
		assert(false);
	}

	irp->user_iosb = io_status_block;
	irp->user_event = event_obj;
	irp->overlay.asynchronous_parameters.user_apc_routine = apc_routine;
	irp->overlay.asynchronous_parameters.user_apc_context = apc_ctx;

	if (byte_offset) {
		if (byte_offset_value.HighPart == -1 && byte_offset_value.LowPart == FILE_USE_FILE_POINTER_POSITION) {
			byte_offset_value.QuadPart = file->current_byte_offset;
		}
		else {
			file->current_byte_offset = byte_offset_value.QuadPart;
		}
	}

	auto* slot = IoGetNextIrpStackLocation(irp);
	slot->major_function = IRP_MJ_READ;
	slot->parameters.read = {
		.length = length,
		.key = key_value,
		.flags = 0,
		.byte_offset = byte_offset_value.QuadPart
	};
	slot->file = file;

	status = IofCallDriver(dev, irp);

	if (status == STATUS_PENDING) {
		if (file->flags & (FILE_SYNCHRONOUS_IO_ALERT | FILE_SYNCHRONOUS_IO_NONALERT)) {
			status = NtWaitForSingleObject(
				file_handle,
				file->flags & FILE_SYNCHRONOUS_IO_ALERT,
				nullptr);
			file->current_byte_offset += length;
		}
	}

	ObfDereferenceObject(file);
	// event_obj is dereferenced when irp is completed

	return status;
}

NTAPI extern "C" NTSTATUS ZwReadFile(
	HANDLE file_handle,
	HANDLE event,
	PIO_APC_ROUTINE apc_routine,
	PVOID apc_ctx,
	IO_STATUS_BLOCK* io_status_block,
	PVOID buffer,
	ULONG length,
	PLARGE_INTEGER byte_offset,
	PULONG key) {
	auto* thread = get_current_thread();
	auto old = thread->previous_mode;
	thread->previous_mode = KernelMode;
	auto ret = NtReadFile(
		file_handle,
		event,
		apc_routine,
		apc_ctx,
		io_status_block,
		buffer,
		length,
		byte_offset,
		key);
	thread->previous_mode = old;
	return ret;
}

NTAPI extern "C" NTSTATUS NtWriteFile(
	HANDLE file_handle,
	HANDLE event,
	PIO_APC_ROUTINE apc_routine,
	PVOID apc_ctx,
	IO_STATUS_BLOCK* io_status_block,
	PVOID buffer,
	ULONG length,
	PLARGE_INTEGER byte_offset,
	PULONG key) {
	auto mode = ExGetPreviousMode();

	LARGE_INTEGER byte_offset_value {};
	ULONG key_value {};
	if (mode == UserMode) {
		__try {
			enable_user_access();

			ProbeForRead(io_status_block, sizeof(IO_STATUS_BLOCK), alignof(IO_STATUS_BLOCK));
			if (byte_offset) {
				ProbeForRead(byte_offset, sizeof(LARGE_INTEGER), alignof(LARGE_INTEGER));
				byte_offset_value = *byte_offset;
			}

			if (key) {
				ProbeForRead(key, sizeof(ULONG), alignof(ULONG));
				key_value = *key;
			}

			ProbeForRead(buffer, length, 1);

			disable_user_access();
		}
		__except (1) {
			disable_user_access();
			return GetExceptionCode();
		}
	}

	FILE_OBJECT* file;
	auto status = ObReferenceObjectByHandle(
		file_handle,
		0,
		IoFileObjectType,
		mode,
		reinterpret_cast<PVOID*>(&file),
		nullptr);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	KEVENT* event_obj = nullptr;
	if (event) {
		status = ObReferenceObjectByHandle(
			event,
			0,
			ExEventObjectType,
			mode,
			reinterpret_cast<PVOID*>(&event_obj),
			nullptr);
		if (!NT_SUCCESS(status)) {
			ObfDereferenceObject(file);
			return status;
		}
	}

	auto* dev = file->device;

	IRP* irp = IoAllocateIrp(dev->stack_size, false);
	if (!irp) {
		ObfDereferenceObject(file);
		ObfDereferenceObject(event_obj);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	if (dev->flags & DO_BUFFERED_IO) {
		void* system_buffer = ExAllocatePool2(0, length, 0);
		if (!system_buffer) {
			ObfDereferenceObject(file);
			ObfDereferenceObject(event_obj);
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		irp->associated_irp.system_buffer = system_buffer;

		__try {
			enable_user_access();
			memcpy(system_buffer, buffer, length);
			disable_user_access();
		}
		__except (1) {
			disable_user_access();
			ExFreePool(system_buffer);
			ObfDereferenceObject(file);
			ObfDereferenceObject(event_obj);
			return GetExceptionCode();
		}

		IoSetCompletionRoutine(irp, [](DEVICE_OBJECT* device, IRP* irp, void* ctx) {
			ExFreePool(irp->associated_irp.system_buffer);
			return STATUS_SUCCESS;
		}, buffer, true, true, true);
	}
	else {
		// todo
		assert(false);
	}

	irp->user_iosb = io_status_block;
	irp->user_event = event_obj;
	irp->overlay.asynchronous_parameters.user_apc_routine = apc_routine;
	irp->overlay.asynchronous_parameters.user_apc_context = apc_ctx;

	if (byte_offset) {
		if (byte_offset_value.HighPart == -1 && byte_offset_value.LowPart == FILE_USE_FILE_POINTER_POSITION) {
			byte_offset_value.QuadPart = file->current_byte_offset;
		}
		else {
			file->current_byte_offset = byte_offset_value.QuadPart;
		}
	}

	auto* slot = IoGetNextIrpStackLocation(irp);
	slot->major_function = IRP_MJ_WRITE;
	slot->parameters.write = {
		.length = length,
		.key = key_value,
		.flags = 0,
		.byte_offset = byte_offset_value.QuadPart
	};
	slot->file = file;

	status = IofCallDriver(dev, irp);

	if (status == STATUS_PENDING) {
		if (file->flags & (FILE_SYNCHRONOUS_IO_ALERT | FILE_SYNCHRONOUS_IO_NONALERT)) {
			status = NtWaitForSingleObject(
				file_handle,
				file->flags & FILE_SYNCHRONOUS_IO_ALERT,
				nullptr);
			file->current_byte_offset += length;
		}
	}

	ObfDereferenceObject(file);
	// event_obj is dereferenced when irp is completed

	return status;
}

NTAPI extern "C" NTSTATUS ZwWriteFile(
	HANDLE file_handle,
	HANDLE event,
	PIO_APC_ROUTINE apc_routine,
	PVOID apc_ctx,
	IO_STATUS_BLOCK* io_status_block,
	PVOID buffer,
	ULONG length,
	PLARGE_INTEGER byte_offset,
	PULONG key) {
	auto* thread = get_current_thread();
	auto old = thread->previous_mode;
	thread->previous_mode = KernelMode;
	auto ret = NtWriteFile(
		file_handle,
		event,
		apc_routine,
		apc_ctx,
		io_status_block,
		buffer,
		length,
		byte_offset,
		key);
	thread->previous_mode = old;
	return ret;
}

NTAPI extern "C" NTSTATUS NtClose(HANDLE handle) {
	return ob_close_handle(handle, ExGetPreviousMode());
}

NTAPI extern "C" NTSTATUS ZwClose(HANDLE handle) {
	auto* thread = get_current_thread();
	auto old = thread->previous_mode;
	thread->previous_mode = KernelMode;
	auto ret = NtClose(handle);
	thread->previous_mode = old;
	return ret;
}
