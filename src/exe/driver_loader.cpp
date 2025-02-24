#include "driver_loader.hpp"
#include "pe_headers.hpp"
#include "dev/pnp.hpp"
#include "fs/vfs.hpp"
#include "assert.hpp"
#include "pe.hpp"
#include "arch/cpu.hpp"

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
