#include "stdio.hpp"
#include "arch/cpu.hpp"
#include "fs/tmpfs.hpp"
#include "fs/tar.hpp"
#include "exe/pe.hpp"
#include "assert.hpp"
#include "sched/semaphore.hpp"
#include <uacpi/uacpi.h>
#include <uacpi/context.h>

#include "utils/export.hpp"

extern "C" EXPORT ULONG DbgPrintEx(ULONG component_id, ULONG level, PCSTR format, ...) {
	println(format);
	return 0;
}

// FILE_AUTOGENERATED_DEVICE_NAME == \Device\%08x

#include "dev/device.hpp"
#include "utils/profiler.hpp"

[[noreturn]] void kmain(const void* initrd) {
	println("[kernel]: entered kmain");
	uacpi_initialize(0);
	//uacpi_context_set_log_level(UACPI_LOG_TRACE);

	profiler_start();
	uacpi_namespace_load();
	profiler_stop();
	profiler_show("uacpi");

	auto vfs = tmpfs_create();
	init_vfs_from_tar(*vfs, initrd);

	auto* cpu = get_current_cpu();

	auto pci_file = vfs_lookup(vfs->get_root(), u"drivers/pci.sys");
	assert(pci_file);
	auto pci_res = pe_load(&*KERNEL_PROCESS, pci_file, false);
	assert(pci_res);
	pci_file = nullptr;

	struct DriverData {
		NTSTATUS (*driver_entry)(DRIVER_OBJECT* driver, PUNICODE_STRING registry_path);
	};
	auto* data = new DriverData {
		.driver_entry = reinterpret_cast<decltype(DriverData::driver_entry)>(pci_res->entry)
	};
	auto* pci_thread = new Thread {
		u"pci driver",
		cpu,
		&*KERNEL_PROCESS,
		[](void* arg) {
			auto* driver = new DRIVER_OBJECT {};

			auto* data = static_cast<DriverData*>(arg);
			data->driver_entry(driver, nullptr);
			delete data;
			KfRaiseIrql(DISPATCH_LEVEL);
			get_current_cpu()->scheduler.block();
		},
		data};
	cpu->scheduler.queue(cpu, pci_thread);

	auto* process = new Process {u"init", true};

	auto ntdll_file = vfs_lookup(vfs->get_root(), u"libs/ntdll.dll");
	assert(ntdll_file);
	auto ntdll_res = pe_load(process, ntdll_file, true);
	assert(ntdll_res);
	ntdll_file = nullptr;

	auto* thread = new Thread {
		u"user thread",
		cpu,
		process,
		reinterpret_cast<void (*)(void*)>(ntdll_res->entry),
		nullptr};

	cpu->scheduler.queue(cpu, thread);

	cpu->scheduler.block();
	panic("[kernel]: returned to kmain");
}
