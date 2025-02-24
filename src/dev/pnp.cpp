#include "pnp.hpp"
#include "cstring.hpp"
#include "driver.hpp"

extern Driver DRIVERS_START;
extern Driver DRIVERS_END;

static void find_driver(DEVICE_OBJECT* device) {
	IRP* irp = IoAllocateIrp(device->stack_size, false);
	if (!irp) {
		return;
	}

	WCHAR* compatibles = nullptr;

	auto slot = IoGetNextIrpStackLocation(irp);
	slot->major_function = IRP_MJ_PNP;
	slot->minor_function = IRP_MN_QUERY_ID;
	slot->parameters.query_id.id_type = BUS_QUERY_ID_TYPE::CompatibleIds;
	IoSetCompletionRoutine(irp, [](DEVICE_OBJECT*, IRP* irp, void* ctx) {
		*static_cast<WCHAR**>(ctx) = reinterpret_cast<WCHAR*>(irp->io_status.info);
		return STATUS_SUCCESS;
	}, &compatibles, true, false, false);
	IofCallDriver(device, irp);

	size_t offset = 0;
	while (true) {
		kstd::wstring_view compatible_str {compatibles + offset};
		if (compatible_str.size() == 0) {
			break;
		}

		for (auto* driver = &DRIVERS_START + 1; driver != &DRIVERS_END; ++driver) {
			for (auto& compatible : driver->compatibles) {
				if (compatible == compatible_str) {
					driver->add_device(driver->object, device);
					break;
				}
			}
		}

		offset += compatible_str.size() + 1;
	}

	ExFreePool(compatibles);
}

void enumerate_bus(DEVICE_OBJECT* device) {
	IRP* irp = IoAllocateIrp(device->stack_size, false);
	if (!irp) {
		return;
	}

	auto slot = IoGetNextIrpStackLocation(irp);
	slot->major_function = IRP_MJ_PNP;
	slot->minor_function = IRP_MN_QUERY_DEVICE_RELATIONS;
	slot->parameters.query_device_relations.type = DEVICE_RELATION_TYPE::Bus;
	IoSetCompletionRoutine(irp, [](DEVICE_OBJECT* device, IRP* irp, void* ctx) {
		auto* ptr = reinterpret_cast<DEVICE_RELATIONS*>(irp->io_status.info);

		for (u32 i = 0; i < ptr->count; ++i) {
			auto dev = ptr->objects[i];
			find_driver(dev);
		}

		ExFreePool(ptr);
		return STATUS_SUCCESS;
	}, nullptr, true, false, false);

	IofCallDriver(device, irp);
}

NTSTATUS IoCreateDevice(
	DRIVER_OBJECT* driver,
	ULONG device_ext_size,
	PUNICODE_STRING device_name,
	DEVICE_TYPE type,
	ULONG characteristics,
	BOOLEAN exclusive,
	DEVICE_OBJECT** device) {
	auto* ptr = static_cast<DEVICE_OBJECT*>(kmalloc(sizeof(DEVICE_OBJECT) + device_ext_size));
	if (!ptr) {
		return STATUS_NO_MEMORY;
	}

	memset(ptr, 0, sizeof(DEVICE_OBJECT) + device_ext_size);

	ptr->driver = driver;
	ptr->next_device = driver->device;
	ptr->flags = exclusive ? DO_EXCLUSIVE : 0;
	ptr->characteristics = characteristics;
	ptr->device_type = type;
	ptr->device_extension = &ptr[1];
	ptr->stack_size = 1;

	driver->device = ptr;

	*device = ptr;
	return STATUS_SUCCESS;
}

IRP* IoAllocateIrp(CCHAR stack_size, BOOLEAN charge_quota) {
	IRP* irp = static_cast<IRP*>(kmalloc(IoSizeOfIrp(stack_size)));
	if (!irp) {
		return nullptr;
	}
	IoInitializeIrp(irp, IoSizeOfIrp(stack_size), stack_size);
	return irp;
}

void IoFreeIrp(IRP* irp) {
	kfree(irp, IoSizeOfIrp(irp->stack_count));
}

void IoInitializeIrp(IRP* irp, USHORT packet_size, CCHAR stack_size) {
	memset(irp, 0, packet_size);
	irp->size = packet_size;
	irp->stack_count = stack_size;
	irp->current_location = stack_size;
	irp->tail.overlay.current_stack_location = reinterpret_cast<IO_STACK_LOCATION*>(&irp[1]);
}

IRP* IoBuildSynchronousFsdRequest(
	ULONG major_function,
	DEVICE_OBJECT* device,
	PVOID buffer,
	ULONG length,
	LARGE_INTEGER* starting_offset,
	KEVENT* event,
	IO_STATUS_BLOCK* io_status_block) {
	IRP* irp = IoAllocateIrp(device->stack_size, false);
	if (!irp) {
		return nullptr;
	}

	irp->associated_irp.system_buffer = buffer;
	irp->user_iosb = io_status_block;
	irp->user_event = event;

	auto slot = IoGetNextIrpStackLocation(irp);
	slot->major_function = major_function;

	switch (major_function) {
		case IRP_MJ_READ:
		case IRP_MJ_WRITE:
		{
			slot->parameters.read.length = length;
			slot->parameters.read.byte_offset = starting_offset->QuadPart;
			break;
		}
		default:
			break;
	}

	slot->device = device;

	return irp;
}

NTSTATUS IofCallDriver(DEVICE_OBJECT* device, IRP* irp) {
	// mark the next lower location as the current
	IoSetNextIrpStackLocation(irp);

	auto slot = IoGetCurrentIrpStackLocation(irp);
	slot->device = device;

	auto driver = device->driver;

	auto status = driver->major_function[slot->major_function](device, irp);
	return status;
}

void IofCompleteRequest(IRP* irp, CCHAR priority_boost) {
	auto slot = IoGetCurrentIrpStackLocation(irp);
	for (; irp->current_location < irp->stack_count; ++slot) {
		if (slot->control & SL_PENDING_RETURNED) {
			irp->pending_returned = true;
		}

		IoSkipCurrentIrpStackLocation(irp);

		DEVICE_OBJECT* device = nullptr;
		if (irp->current_location < irp->stack_count) {
			device = IoGetCurrentIrpStackLocation(irp)->device;
		}

		if (slot->completion_routine &&
			(((slot->control & SL_INVOKE_ON_SUCCESS) && irp->io_status.status == STATUS_SUCCESS) ||
			((slot->control & SL_INVOKE_ON_ERROR) && irp->io_status.status != STATUS_SUCCESS) ||
			((slot->control & SL_INVOKE_ON_CANCEL) && irp->cancel))) {
			auto status = slot->completion_routine(device, irp, slot->ctx);
			if (status == STATUS_MORE_PROCESSING_REQUIRED) {
				return;
			}
		}
		else if (irp->pending_returned && irp->current_location < irp->stack_count) {
			IoMarkIrpPending(irp);
		}
	}

	if (irp->user_iosb) {
		irp->user_iosb->status = irp->io_status.status;
		irp->user_iosb->info = irp->io_status.info;
	}

	if (irp->user_event) {
		KeSetEvent(irp->user_event, 0, false);
	}

	// todo user status + event

	IoFreeIrp(irp);
}
