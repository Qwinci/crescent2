#include "smp.hpp"
#include "dev/hpet.hpp"
#include "dev/tsc.hpp"
#include "acpi.hpp"
#include "mem/vspace.hpp"
#include "utils/shared_data.hpp"
#include "cstring.hpp"
#include "string_view.hpp"
#include "assert.hpp"
#include <hz/optional.hpp>
#include <hz/pair.hpp>

[[noreturn]] void kmain(const void* initrd);

hz::optional<hz::pair<void*, usize>> x86_get_module(kstd::string_view name);

void arch_start(void* rsdp) {
	auto info_page = KERNEL_VSPACE.alloc_backed(
		reinterpret_cast<usize>(SharedUserData),
		PAGE_SIZE,
		PageFlags::Read | PageFlags::Write);
	memset(info_page, 0xFF, PAGE_SIZE);

	acpi::init(rsdp);
	hpet_init();
	tsc_init();
	x86_smp_init();

	auto initrd = x86_get_module("initramfs.tar");
	assert(initrd);

	kmain(initrd->first);
}
