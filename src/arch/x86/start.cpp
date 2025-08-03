#include "smp.hpp"
#include "dev/hpet.hpp"
#include "dev/tsc.hpp"
#include "acpi/acpi.hpp"
#include "mem/vspace.hpp"
#include "utils/shared_data.hpp"
#include "cstring.hpp"
#include "string_view.hpp"
#include "assert.hpp"
#include "dev/driver.hpp"
#include "arch/x86/dev/rtc.hpp"
#include "sched/ps.hpp"
#include <hz/optional.hpp>
#include <hz/pair.hpp>

[[noreturn]] void kmain(const void* initrd);

hz::optional<hz::pair<void*, usize>> x86_get_module(kstd::string_view name);

#pragma section(".drivers$a", read)
#pragma section(".drivers$z", read)
__declspec(allocate(".drivers$a")) Driver DRIVERS_START {};
__declspec(allocate(".drivers$z")) Driver DRIVERS_END {};

void init_drivers() {
	for (auto* driver = &DRIVERS_START + 1; driver != &DRIVERS_END; ++driver) {
		driver->do_init();
	}
}

void x86_madt_parse();

void arch_start(void* rsdp) {
	auto info_page = KERNEL_VSPACE.alloc_backed(
		reinterpret_cast<usize>(SharedUserData),
		PAGE_SIZE,
		PageFlags::Read | PageFlags::Write);
	memset(info_page, 0xFF, PAGE_SIZE);

	ps_init();

	acpi::init(rsdp);
	hpet_init();
	tsc_init();
	rtc_init();
	x86_smp_init();
	x86_madt_parse();

	init_drivers();

	auto initrd = x86_get_module("initramfs.tar");
	assert(initrd);

	kmain(initrd->first);
}
