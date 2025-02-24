#include "driver_loader.hpp"
#include "pe_headers.hpp"
#include "dev/pnp.hpp"
#include "fs/vfs.hpp"
#include "assert.hpp"
#include "pe.hpp"
#include "arch/cpu.hpp"

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

void load_predefined_bus_driver(kstd::wstring_view name, kstd::wstring_view path) {
	auto file = vfs_lookup(ROOT_VFS->get_root(), path);
	assert(file);
	auto load_res = pe_load(&*KERNEL_PROCESS, file, false);
	assert(load_res);

	struct DriverData {
		NTSTATUS (*driver_entry)(DRIVER_OBJECT* driver, PUNICODE_STRING registry_path);
	};
	auto* data = new DriverData {
		.driver_entry = reinterpret_cast<decltype(DriverData::driver_entry)>(load_res->entry)
	};

	auto cpu = get_current_cpu();

	auto* thread = new Thread {
		name,
		cpu,
		&*KERNEL_PROCESS,
		[](void* arg) {
			auto* driver = new DRIVER_OBJECT {};
			auto* ext = new DRIVER_EXTENSION {};
			ext->driver = driver;
			driver->driver_extension = ext;

			auto* data = static_cast<DriverData*>(arg);
			data->driver_entry(driver, nullptr);
			delete data;

			DEVICE_OBJECT* dev;
			auto status = IoCreateDevice(driver, 0, nullptr, 0, 0, false, &dev);
			assert(status == STATUS_SUCCESS);

			enumerate_bus(dev);

			KfRaiseIrql(DISPATCH_LEVEL);
			get_current_cpu()->scheduler.block();
		},
		data};
	cpu->scheduler.queue(cpu, thread);
}

hz::list<LoadedImage, &LoadedImage::hook> LOADED_IMAGE_LIST;
