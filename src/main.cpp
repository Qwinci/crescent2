#include "stdio.hpp"
#include "arch/cpu.hpp"
#include "fs/tmpfs.hpp"
#include "fs/tar.hpp"
#include "exe/pe.hpp"
#include "assert.hpp"
#include "sched/semaphore.hpp"
#include <uacpi/uacpi.h>
#include <uacpi/context.h>

// FILE_AUTOGENERATED_DEVICE_NAME == \Device\%08x

#include "utils/profiler.hpp"
#include "exe/driver_loader.hpp"

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
	ROOT_VFS = std::move(vfs);

	load_predefined_bus_driver(u"pci driver", u"drivers/pci.sys");

	auto* process = new Process {u"init", true};

	auto ntdll_file = vfs_lookup(nullptr, u"libs/ntdll.dll");
	assert(ntdll_file);
	auto ntdll_res = pe_load(process, ntdll_file, true);
	assert(ntdll_res);
	ntdll_file = nullptr;

	auto exe_file = vfs_lookup(nullptr, u"apps/hello_world.exe");
	assert(exe_file);
	auto exe_res = pe_load(process, exe_file, true);
	assert(exe_res);
	exe_file = nullptr;

	auto cpu = get_current_cpu();
	auto* thread = new Thread {
		u"user thread",
		cpu,
		process,
		reinterpret_cast<void (*)(void*)>(ntdll_res->entry),
		reinterpret_cast<void*>(exe_res->base)};

	cpu->scheduler.queue(cpu, thread);

	cpu->scheduler.block();
	panic("[kernel]: returned to kmain");
}
