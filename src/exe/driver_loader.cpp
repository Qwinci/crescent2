#include "driver_loader.hpp"
#include "pe_headers.hpp"
#include "dev/pnp.hpp"
#include "dev/irq.hpp"
#include "fs/vfs.hpp"
#include "pe.hpp"
#include "arch/cpu.hpp"
#include "arch/x86/dev/io_apic.hpp"
#include "x86/irq.hpp"

static LoadedImage KERNEL_LOADED_IMAGE {};

extern "C" DosHeader __ImageBase;

void init_driver_loader() {
	auto* hdr = offset(&__ImageBase, const PeHeader64*, __ImageBase.e_lfanew);
	auto dir = hdr->opt.data_dirs[IMAGE_DIRECTORY_ENTRY_EXCEPTION];

	KERNEL_LOADED_IMAGE.image_base = reinterpret_cast<usize>(&__ImageBase);
	KERNEL_LOADED_IMAGE.image_size = hdr->opt.size_of_image;
	KERNEL_LOADED_IMAGE.function_table = offset(&__ImageBase, RUNTIME_FUNCTION*, dir.virt_addr);
	KERNEL_LOADED_IMAGE.function_table_len = dir.size;

	LOADED_IMAGE_LIST.push(&KERNEL_LOADED_IMAGE);
}

hz::pair<LoadedPe, DEVICE_OBJECT*> load_predefined_bus_driver(kstd::wstring_view name, kstd::wstring_view path) {
	auto file = vfs_lookup(ROOT_VFS->get_root(), path);
	assert(file);
	auto load_res = pe_load(&*KERNEL_PROCESS, file, false);
	assert(load_res);

	auto* driver = new DRIVER_OBJECT {};
	auto* ext = new DRIVER_EXTENSION {};
	ext->driver = driver;
	driver->driver_extension = ext;

	DEVICE_OBJECT* dev;
	auto status = IoCreateDevice(driver, 0, nullptr, 0, 0, false, &dev);
	assert(status == STATUS_SUCCESS);

	struct DriverData {
		NTSTATUS (*driver_entry)(DRIVER_OBJECT* driver, PUNICODE_STRING registry_path);
		DEVICE_OBJECT* device;
	};

	auto* data = new DriverData {
		.driver_entry = reinterpret_cast<decltype(DriverData::driver_entry)>(load_res->entry),
		.device = dev
	};

	auto cpu = get_current_cpu();

	auto* thread = new Thread {
		name,
		cpu,
		&*KERNEL_PROCESS,
		false,
		[](void* arg) {
			auto* data = static_cast<DriverData*>(arg);

			auto dev = data->device;

			data->driver_entry(dev->driver, nullptr);
			delete data;

			enumerate_bus(dev);

			KfRaiseIrql(DISPATCH_LEVEL);
			get_current_cpu()->scheduler.block();
		},
		data};
	cpu->scheduler.queue(cpu, thread);

	return {load_res.value(), dev};
}

extern void* GLOBAL_RSDP;

#define CTL_CODE(DeviceType, Function, Method, Access) \
	((ULONG) ((DeviceType) << 16 | (Access) << 14 | (Function) << 2 | (Method)))

#define METHOD_BUFFERED 0
#define METHOD_IN_DIRECT 1
#define METHOD_OUT_DIRECT 2
#define METHOD_NEITHER 3

#define METHOD_DIRECT_TO_HARDWARE METHOD_IN_DIRECT
#define METHOD_DIRECT_FROM_HARDWARE METHOD_OUT_DIRECT

#define FILE_ANY_ACCESS 0
#define FILE_SPECIAL_ACCESS FILE_ANY_ACCESS
#define FILE_READ_ACCESS 1
#define FILE_WRITE_ACCESS 2

#define IOCTL_PROVIDE_RSDP CTL_CODE(0x8000, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

DEVICE_OBJECT* load_predefined_acpi_driver(kstd::wstring_view name, kstd::wstring_view path) {
	auto file = vfs_lookup(ROOT_VFS->get_root(), path);
	assert(file);
	auto load_res = pe_load(&*KERNEL_PROCESS, file, false);
	assert(load_res);

	auto* driver = new DRIVER_OBJECT {};
	auto* ext = new DRIVER_EXTENSION {};
	ext->driver = driver;
	driver->driver_extension = ext;

	DEVICE_OBJECT* dev;
	auto status = IoCreateDevice(driver, 0, nullptr, 0, 0, false, &dev);
	assert(status == STATUS_SUCCESS);

	struct DriverData {
		NTSTATUS (*driver_entry)(DRIVER_OBJECT* driver, PUNICODE_STRING registry_path);
		DEVICE_OBJECT* device;
	};

	auto* data = new DriverData {
		.driver_entry = reinterpret_cast<decltype(DriverData::driver_entry)>(load_res->entry),
		.device = dev
	};

	auto cpu = get_current_cpu();

	auto* thread = new Thread {
		name,
		cpu,
		&*KERNEL_PROCESS,
		false,
		[](void* arg) {
			auto* data = static_cast<DriverData*>(arg);

			auto dev = data->device;

			data->driver_entry(dev->driver, nullptr);
			delete data;

			IRP* irp = IoAllocateIrp(dev->stack_size, false);
			assert(irp);

			auto slot = IoGetNextIrpStackLocation(irp);
			slot->major_function = IRP_MJ_DEVICE_CONTROL;
			slot->parameters.device_io_control.io_control_code = IOCTL_PROVIDE_RSDP;
			usize rsdp_phys = to_phys(GLOBAL_RSDP);
			irp->associated_irp.system_buffer = &rsdp_phys;
			auto status = IofCallDriver(dev, irp);
			assert(status == STATUS_SUCCESS);

			irp = IoAllocateIrp(dev->stack_size, false);
			assert(irp);
			slot = IoGetNextIrpStackLocation(irp);
			slot->major_function = IRP_MJ_PNP;
			slot->minor_function = IRP_MN_QUERY_RESOURCES;

			u32 irq;

			IoSetCompletionRoutine(irp, [](DEVICE_OBJECT* device, IRP* irp, void* ctx) {
				auto* ptr = reinterpret_cast<CM_RESOURCE_LIST*>(irp->io_status.info);

				assert(ptr->count == 1);
				assert(ptr->list[0].PartialResourceList.count == 1);
				auto& desc = ptr->list[0].PartialResourceList.partial_descriptors[0];
				assert(desc.type == CmResourceTypeInterrupt);

				auto irq = desc.u.interrupt.vector;
				*static_cast<u32*>(ctx) = irq;

				auto vec = x86_alloc_irq(1, DEVICE_LEVEL, true);
				assert(vec);

				ExFreePool(ptr);
				return STATUS_SUCCESS;
			}, &irq, true, false, false);
			status = IofCallDriver(dev, irp);
			assert(status == STATUS_SUCCESS);

			auto vec = x86_alloc_irq(1, DEVICE_LEVEL, true);
			assert(vec);

			auto* info = static_cast<IrqInfo*>(kmalloc(sizeof(IrqInfo)));
			assert(info);
			info->is_pin = true;
			info->pin.vector = vec;
			info->pin.pin = irq;
			info->pin.default_level_trigger = true;
			info->pin.default_active_low = true;

			dev->device_object_extension->interrupt_count = 1;
			dev->device_object_extension->interrupts = info;

			auto* list = static_cast<CM_RESOURCE_LIST*>(ExAllocatePool2(
				0,
				sizeof(CM_RESOURCE_LIST) +
				sizeof(CM_FULL_RESOURCE_DESCRIPTOR) +
				sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR),
				0));
			assert(list);

			list->count = 1;
			list->list[0].PartialResourceList.count = 1;
			auto& desc = list->list[0].PartialResourceList.partial_descriptors[0];
			desc.type = CmResourceTypeInterrupt;
			desc.flags = 0;
			desc.u.interrupt.level = vec / 16;
			desc.u.interrupt.group = 0;
			desc.u.interrupt.vector = vec;
			desc.u.interrupt.affinity = 1;

			irp = IoAllocateIrp(dev->stack_size, false);
			assert(irp);
			slot = IoGetNextIrpStackLocation(irp);
			slot->major_function = IRP_MJ_PNP;
			slot->minor_function = IRP_MN_START_DEVICE;
			slot->parameters.start_device.allocated_resources_translated = list;
			IofCallDriver(dev, irp);
			assert(status == STATUS_SUCCESS);

			// todo keep track of resource list to not leak it
			//enumerate_bus(dev);

			KfRaiseIrql(DISPATCH_LEVEL);
			get_current_cpu()->scheduler.block();
		},
		data};
	cpu->scheduler.queue(cpu, thread);

	return dev;
}

hz::list<LoadedImage, &LoadedImage::hook> LOADED_IMAGE_LIST;
