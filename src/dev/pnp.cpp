#include "pnp.hpp"
#include "cstring.hpp"
#include "driver.hpp"
#include "fs/vfs.hpp"
#include "exe/pe.hpp"
#include "x86/irq.hpp"
#include "fs/registry.hpp"
#include "rtl.hpp"
#include "pnp_internals.hpp"
#include "arch/arch_syscall.hpp"
#include "sched/process.hpp"

NTAPI extern "C" OBJECT_TYPE* IoDeviceObjectType = nullptr;
NTAPI extern "C" OBJECT_TYPE* IoDriverObjectType = nullptr;
NTAPI extern "C" OBJECT_TYPE* IoFileObjectType = nullptr;

static NTSTATUS device_parse_routine(
	PVOID parse_object,
	PVOID object_type,
	PACCESS_STATE access_state,
	KPROCESSOR_MODE access_mode,
	ULONG attribs,
	PUNICODE_STRING complete_name,
	PUNICODE_STRING remaining_name,
	PVOID context,
	SECURITY_QUALITY_OF_SERVICE* security_qos,
	PVOID* object) {
	auto* ctx = static_cast<FileParseCtx*>(context);

	auto* dev = static_cast<DEVICE_OBJECT*>(parse_object);

	FILE_OBJECT* file;
	auto status = ObCreateObject(
		KernelMode,
		IoFileObjectType,
		nullptr,
		access_mode,
		nullptr,
		sizeof(FILE_OBJECT),
		0,
		sizeof(FILE_OBJECT),
		reinterpret_cast<PVOID*>(&file));
	if (!NT_SUCCESS(status)) {
		return status;
	}

	file->device = dev;
	file->file_name = *remaining_name;
	KeInitializeEvent(&file->lock, EVENT_TYPE::Synchronization, true);
	KeInitializeEvent(&file->event, EVENT_TYPE::Notification, false);

	IRP* irp = IoAllocateIrp(dev->stack_size, false);
	if (!irp) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	irp->flags = IRP_CREATE_OPERATION | IRP_DEFER_IO_COMPLETION | IRP_SYNCHRONOUS_API;
	irp->requestor_mode = access_mode;
	irp->associated_irp.system_buffer = ctx->ea_buffer;
	irp->user_iosb = ctx->status_block;
	irp->overlay.allocation_size = ctx->allocation_size.QuadPart;

	auto* slot = IoGetNextIrpStackLocation(irp);
	slot->major_function = IRP_MJ_CREATE;
	slot->parameters.create = {
		.security_context = nullptr,
		.options = ctx->create_options | ctx->create_disposition << 24,
		.file_attributes = static_cast<USHORT>(ctx->file_attribs),
		.share_access = static_cast<USHORT>(ctx->share_access),
		.ea_length = ctx->ea_length
	};
	slot->file = file;

	*object = file;

	return IofCallDriver(dev, irp);
}

void pnp_init() {
	UNICODE_STRING name = RTL_CONSTANT_STRING(u"Device Object");
	OBJECT_TYPE_INITIALIZER init {};
	init.parse_proc = device_parse_routine;
	auto status = ObCreateObjectType(&name, &init, nullptr, &IoDeviceObjectType);
	assert(NT_SUCCESS(status));
	name = RTL_CONSTANT_STRING(u"Driver Object");
	init = {};
	status = ObCreateObjectType(&name, &init, nullptr, &IoDriverObjectType);
	assert(NT_SUCCESS(status));

	name = RTL_CONSTANT_STRING(u"File Object");
	status = ObCreateObjectType(&name, &init, nullptr, &IoFileObjectType);
	assert(NT_SUCCESS(status));
}

extern Driver DRIVERS_START;
extern Driver DRIVERS_END;

static void start_device(DRIVER_OBJECT* driver, DEVICE_OBJECT* pdo) {
	IRP* irp = IoAllocateIrp(pdo->stack_size, false);
	assert(irp);

	IO_RESOURCE_REQUIREMENTS_LIST* req_list;

	auto slot = IoGetNextIrpStackLocation(irp);
	slot->major_function = IRP_MJ_PNP;
	slot->minor_function = IRP_MN_QUERY_RESOURCE_REQUIREMENTS;
	IoSetCompletionRoutine(irp, [](DEVICE_OBJECT*, IRP* irp, void* ctx) {
		*static_cast<IO_RESOURCE_REQUIREMENTS_LIST**>(ctx) =
			reinterpret_cast<IO_RESOURCE_REQUIREMENTS_LIST*>(irp->io_status.info);
		return STATUS_SUCCESS;
	}, &req_list, true, false, false);
	IofCallDriver(pdo, irp);

	auto* ext = driver->driver_extension;
	auto status = ext->add_device(driver, pdo);

	println("[kernel][pnp]: add device status ", Fmt::Hex, status, Fmt::Reset);
	if (status != STATUS_SUCCESS) {
		ExFreePool(req_list);
		return;
	}

	auto top = IoGetAttachedDeviceReference(pdo);

	irp = IoAllocateIrp(top->stack_size, false);
	assert(irp);

	slot = IoGetNextIrpStackLocation(irp);
	slot->major_function = IRP_MJ_PNP;
	slot->minor_function = IRP_MN_FILTER_RESOURCE_REQUIREMENTS;
	slot->parameters.filter_resource_requirements.list = req_list;
	irp->io_status.info = reinterpret_cast<ULONG_PTR>(req_list);
	IoSetCompletionRoutine(irp, [](DEVICE_OBJECT*, IRP* irp, void* ctx) {
		*static_cast<IO_RESOURCE_REQUIREMENTS_LIST**>(ctx) =
			reinterpret_cast<IO_RESOURCE_REQUIREMENTS_LIST*>(irp->io_status.info);
		return STATUS_SUCCESS;
	}, &req_list, true, false, false);
	IofCallDriver(top, irp);

	assert(req_list->alternative_lists);
	auto& alt_list = req_list->list[0];

	auto* res_list = static_cast<CM_RESOURCE_LIST*>(ExAllocatePool2(
		0,
		sizeof(CM_RESOURCE_LIST) + sizeof(CM_FULL_RESOURCE_DESCRIPTOR) +
		alt_list.count * sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR),
		0));
	assert(res_list);
	res_list->count = 1;
	res_list->list[0].interface_type = req_list->interface_type;
	res_list->list[0].bus_number = req_list->bus_number;

	auto& partial_list = res_list->list[0].PartialResourceList;
	partial_list.version = 1;
	partial_list.revision = 1;
	partial_list.count = alt_list.count;

	for (u32 i = 0; i < alt_list.count; ++i) {
		auto& desc = alt_list.descriptors[i];
		auto& new_desc = partial_list.partial_descriptors[i];

		new_desc.type = desc.type;
		new_desc.share_disposition = desc.share_disposition;
		new_desc.flags = desc.flags;

		if (desc.type == CmResourceTypePort) {
			new_desc.u.port.start = desc.u.port.minimum_address;
			new_desc.u.port.length = desc.u.port.length;
		}
		else if (desc.type == CmResourceTypeInterrupt) {
			bool edge_trigger = desc.flags & CM_RESOURCE_INTERRUPT_LATCHED;
			bool msi = desc.flags & CM_RESOURCE_INTERRUPT_MESSAGE;
			bool shared = desc.share_disposition == CmResourceShareShared;

			auto& params = desc.u.interrupt;

			if (msi) {
				uint32_t msi_count = params.maximum_vector - params.minimum_vector + 1;

				uint8_t vector = x86_alloc_irq(msi_count, DEVICE_LEVEL, shared);
				assert(vector);

				new_desc.u.message_interrupt.raw = {
					.group = params.group,
					.message_count = static_cast<u16>(msi_count),
					.vector = vector,
					.affinity = params.targeted_processors
				};

				// todo verify whether this is correct (level == vector)
				new_desc.u.message_interrupt.translated = {
					.level = vector,
					.group = params.group,
					.vector = vector,
					.affinity = params.targeted_processors
				};
			}
			else {
				uint32_t pin = params.minimum_vector;
			}
		}
		else if (desc.type == CmResourceTypeMemory || desc.type == CmResourceTypeMemoryLarge) {
			new_desc.u.memory.start = desc.u.memory.minimum_address;
			new_desc.u.memory.length = desc.u.memory.length;
		}
		else {
			panic("[kernel][pnp]: unsupported resource type ", desc.type);
		}
	}

	irp = IoAllocateIrp(top->stack_size, false);
	assert(irp);

	slot = IoGetNextIrpStackLocation(irp);
	slot->major_function = IRP_MJ_PNP;
	slot->minor_function = IRP_MN_START_DEVICE;
	slot->parameters.start_device.allocated_resources = res_list;
	slot->parameters.start_device.allocated_resources_translated = res_list;
	IofCallDriver(top, irp);

	ObfDereferenceObject(top);
}

static bool amdgpu_loaded = false;

static void start_amdgpu(DEVICE_OBJECT* device, kstd::wstring_view compatible) {
	if (amdgpu_loaded) {
		return;
	}

	auto file = vfs_lookup(ROOT_VFS->get_root(), u"drivers/amdkmdag.sys");
	assert(file);
	auto load_res = pe_load(&*KERNEL_PROCESS, file, false);
	assert(load_res);

	DRIVER_OBJECT* driver;
	OBJECT_ATTRIBUTES attribs {};
	auto status = ObCreateObject(
		KernelMode,
		IoDriverObjectType,
		nullptr,
		KernelMode,
		nullptr,
		sizeof(DRIVER_OBJECT) + sizeof(DRIVER_EXTENSION),
		0,
		sizeof(DRIVER_OBJECT) + sizeof(DRIVER_EXTENSION),
		reinterpret_cast<PVOID*>(&driver));
	assert(NT_SUCCESS(status));

	status = ObInsertObject(driver, nullptr, 0, 0, nullptr, nullptr);
	assert(NT_SUCCESS(status));

	auto* ext = reinterpret_cast<DRIVER_EXTENSION*>(&driver[1]);
	ext->driver = driver;
	driver->driver_extension = ext;

	auto init = reinterpret_cast<PDRIVER_INITIALIZE>(load_res->entry);

	UNICODE_STRING registry_path = RTL_CONSTANT_STRING(
		u"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\amdkmdag");
	InitializeObjectAttributes(&attribs, &registry_path, OBJ_CASE_INSENSITIVE, nullptr, nullptr);
	HANDLE reg_handle;
	status = ZwCreateKey(
		&reg_handle,
		0,
		&attribs,
		0,
		nullptr,
		REG_OPTION_NON_VOLATILE,
		nullptr);
	assert(NT_SUCCESS(status));

	status = init(driver, &registry_path);
	println("[kernel][pnp]: amdgpu init status ", Fmt::Hex, status, Fmt::Reset);

	start_device(driver, device);

	amdgpu_loaded = true;
}

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

		bool found = false;
		for (auto* driver = &DRIVERS_START + 1; driver != &DRIVERS_END; ++driver) {
			for (auto& compatible : driver->compatibles) {
				if (compatible == compatible_str) {
					start_device(driver->object, device);
					found = true;
					break;
				}
			}
		}

		if (!found) {
			start_amdgpu(device, compatible_str);
		}

		offset += compatible_str.size() + 1;
	}

	ExFreePool(compatibles);
}

struct PnpDeviceNode {
	INTERFACE_TYPE interface_type;
	ULONG bus_number;
};

static void create_device_node(DEVICE_OBJECT* device) {
	auto* node = static_cast<PnpDeviceNode*>(kcalloc(sizeof(PnpDeviceNode)));
	if (!node) {
		return;
	}

	PnpIrpBuilder::create(device, IRP_MN_QUERY_BUS_INFORMATION)
		->send([&](IRP* irp) {
			auto* info = reinterpret_cast<PNP_BUS_INFORMATION*>(irp->io_status.info);

			node->interface_type = info->legacy_bus_type;
			node->bus_number = info->bus_number;

			ExFreePool(info);
			return STATUS_SUCCESS;
		});

	device->device_object_extension->device_node = node;
}

void enumerate_bus(DEVICE_OBJECT* device) {
	PnpIrpBuilder::create(device, IRP_MN_QUERY_DEVICE_RELATIONS)
		->params([](IO_STACK_LOCATION* slot) {
			slot->parameters.query_device_relations.type = DEVICE_RELATION_TYPE::Bus;
		})
		.send([](IRP* irp) {
			auto* ptr = reinterpret_cast<DEVICE_RELATIONS*>(irp->io_status.info);

			for (u32 i = 0; i < ptr->count; ++i) {
				auto dev = ptr->objects[i];
				create_device_node(dev);
				find_driver(dev);
				dev->flags |= DO_BUS_ENUMERATED_DEVICE;
			}

			ExFreePool(ptr);
			return STATUS_SUCCESS;
		});
}

NTAPI NTSTATUS IoCreateDevice(
	DRIVER_OBJECT* driver,
	ULONG device_ext_size,
	PUNICODE_STRING device_name,
	DEVICE_TYPE type,
	ULONG characteristics,
	BOOLEAN exclusive,
	DEVICE_OBJECT** device) {
	OBJECT_ATTRIBUTES attribs {};
	InitializeObjectAttributes(&attribs, device_name, 0, nullptr, nullptr);

	auto size = sizeof(DEVICE_OBJECT) + sizeof(DEVOBJ_EXTENSION) + device_ext_size;

	auto status = ObCreateObject(
		KernelMode,
		IoDeviceObjectType,
		&attribs,
		KernelMode,
		nullptr,
		size,
		0,
		size,
		reinterpret_cast<PVOID*>(device));
	if (!NT_SUCCESS(status)) {
		return status;
	}

	status = ObInsertObject(*device, nullptr, 0, 0, nullptr, nullptr);
	assert(NT_SUCCESS(status));

	auto* dev = *device;
	dev->driver = driver;
	dev->next_device = driver->device;
	dev->flags = exclusive ? DO_EXCLUSIVE : 0;
	dev->characteristics = characteristics;
	dev->device_type = type;
	dev->device_extension = offset(&dev[1], void*, sizeof(DEVOBJ_EXTENSION));
	dev->stack_size = 1;
	dev->device_object_extension = reinterpret_cast<DEVOBJ_EXTENSION*>(&dev[1]);

	driver->device = dev;

	return STATUS_SUCCESS;
}

NTAPI DEVICE_OBJECT* IoAttachDeviceToDeviceStack(DEVICE_OBJECT* source, DEVICE_OBJECT* target) {
	while (target->attached_device) {
		target = target->attached_device;
	}

	target->attached_device = source;
	source->alignment_requirement = target->alignment_requirement;
	source->stack_size = static_cast<i8>(target->stack_size + 1);
	source->device_object_extension->attached_to = target;
	return target;
}

NTAPI DEVICE_OBJECT* IoGetAttachedDevice(DEVICE_OBJECT* device) {
	auto* target = device->attached_device;
	while (target) {
		target = target->attached_device;
	}
	return target;
}

NTAPI DEVICE_OBJECT* IoGetAttachedDeviceReference(DEVICE_OBJECT* device) {
	device = IoGetAttachedDevice(device);
	ObfReferenceObject(device);
	return device;
}

NTAPI DEVICE_OBJECT* IoGetLowerDeviceObject(DEVICE_OBJECT* device) {
	device = device->device_object_extension->attached_to;
	if (device) {
		ObfReferenceObject(device);
	}
	return device;
}

NTAPI DEVICE_OBJECT* IoGetRelatedDeviceObject(FILE_OBJECT* file) {
	auto* device = file->device;
	while (device->attached_device) {
		device = device->attached_device;
	}
	return device;
}

NTAPI NTSTATUS IoGetDeviceObjectPointer(
	PUNICODE_STRING object_name,
	ACCESS_MASK desired_access,
	FILE_OBJECT** file_object,
	DEVICE_OBJECT** device) {
	HANDLE handle = nullptr;
	OBJECT_ATTRIBUTES attribs {};
	InitializeObjectAttributes(
		&attribs,
		object_name,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		nullptr,
		nullptr);
	IO_STATUS_BLOCK status_block {};
	auto status = ZwOpenFile(
		&handle,
		desired_access,
		&attribs,
		&status_block,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		FILE_NON_DIRECTORY_FILE);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	FILE_OBJECT* file;
	status = ObReferenceObjectByHandle(
		handle,
		0,
		IoFileObjectType,
		KernelMode,
		reinterpret_cast<PVOID*>(&file),
		nullptr);
	if (NT_SUCCESS(status)) {
		*file_object = file;
		*device = IoGetRelatedDeviceObject(file);
	}

	ZwClose(handle);

	return status;
}

struct PciAddress {
	uint16_t seg;
	uint8_t bus;
	uint8_t dev;
	uint8_t func;
};

NTAPI NTSTATUS IoGetDeviceProperty(
	DEVICE_OBJECT* device,
	DEVICE_REGISTRY_PROPERTY device_property,
	ULONG buffer_length,
	PVOID property_buffer,
	PULONG result_length) {
	if (!device || !device->device_object_extension->device_node) {
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	auto* node = static_cast<PnpDeviceNode*>(device->device_object_extension->device_node);

	void* data = nullptr;
	ULONG len = 0;
	ULONG ulong_value = 0;

	switch (device_property) {
		case DevicePropertyDeviceDescription:
		case DevicePropertyHardwareID:
		case DevicePropertyCompatibleIDs:
		case DevicePropertyBootConfiguration:
		case DevicePropertyBootConfigurationTranslated:
		case DevicePropertyClassName:
		case DevicePropertyClassGuid:
			panic("[kernel][pnp]: unsupported key ", device_property, " in IoGetDeviceProperty");
		case DevicePropertyDriverKeyName:
		{
			auto& name = device->driver->driver_extension->service_key_name;
			data = name.Buffer;
			len = name.Length + 2;
			break;
		}
		case DevicePropertyManufacturer:
		case DevicePropertyFriendlyName:
		case DevicePropertyLocationInformation:
		case DevicePropertyPhysicalDeviceObjectName:
		case DevicePropertyBusTypeGuid:
			panic("[kernel][pnp]: unsupported key ", device_property, " in IoGetDeviceProperty");
		case DevicePropertyLegacyBusType:
			data = &node->interface_type;
			len = sizeof(INTERFACE_TYPE);
			break;
		case DevicePropertyBusNumber:
			data = &node->bus_number;
			len = 4;
			break;
		case DevicePropertyEnumeratorName:
			panic("[kernel][pnp]: unsupported key ", device_property, " in IoGetDeviceProperty");
		case DevicePropertyAddress:
			if (node->interface_type == PCIBus) {
				// this depends on internals of pci.sys
				auto addr = *static_cast<PciAddress*>(device->device_extension);
				ulong_value = addr.dev << 16 | addr.func;
			}
			else {
				ulong_value = 0xFFFFFFFF;
			}

			data = &ulong_value;
			len = 4;
			break;
		case DevicePropertyUINumber:
		case DevicePropertyInstallState:
		case DevicePropertyRemovalPolicy:
		case DevicePropertyResourceRequirements:
		case DevicePropertyAllocatedResources:
		case DevicePropertyContainerID:
			panic("[kernel][pnp]: unsupported key ", device_property, " in IoGetDeviceProperty");
	}

	if (buffer_length < len) {
		*result_length = len;
		return STATUS_BUFFER_TOO_SMALL;
	}

	memcpy(property_buffer, data, len);
	return STATUS_SUCCESS;
}

NTAPI IRP* IoAllocateIrp(CCHAR stack_size, BOOLEAN charge_quota) {
	IRP* irp = static_cast<IRP*>(kmalloc(IoSizeOfIrp(stack_size)));
	if (!irp) {
		return nullptr;
	}
	IoInitializeIrp(irp, IoSizeOfIrp(stack_size), stack_size);
	return irp;
}

NTAPI void IoFreeIrp(IRP* irp) {
	kfree(irp, IoSizeOfIrp(irp->stack_count));
}

NTAPI void IoInitializeIrp(IRP* irp, USHORT packet_size, CCHAR stack_size) {
	memset(irp, 0, packet_size);
	irp->size = packet_size;
	irp->stack_count = stack_size;
	irp->current_location = stack_size;
	irp->tail.overlay.current_stack_location = reinterpret_cast<IO_STACK_LOCATION*>(&irp[1]);
}

NTAPI IRP* IoBuildSynchronousFsdRequest(
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

NTAPI NTSTATUS IofCallDriver(DEVICE_OBJECT* device, IRP* irp) {
	// mark the next lower location as the current
	IoSetNextIrpStackLocation(irp);

	auto slot = IoGetCurrentIrpStackLocation(irp);
	slot->device = device;

	auto driver = device->driver;

	auto status = driver->major_function[slot->major_function](device, irp);
	return status;
}

NTAPI void IofCompleteRequest(IRP* irp, CCHAR priority_boost) {
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
		__try {
			enable_user_access();
			irp->user_iosb->status = irp->io_status.status;
			irp->user_iosb->info = irp->io_status.info;
			disable_user_access();
		}
		__except (1) {
			disable_user_access();
		}
	}

	if (irp->user_event) {
		KeSetEvent(irp->user_event, 0, false);
		ObfDereferenceObject(irp->user_event);
	}

	IoFreeIrp(irp);
}
