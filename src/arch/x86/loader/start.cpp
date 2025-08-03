#include "mem/mem.hpp"
#include "mem/pmalloc.hpp"
#include "arch/paging.hpp"
#include "mem/vspace.hpp"
#include "cxx.hpp"
#include "limine.h"
#include "arch/x86/cpu.hpp"
#include "dev/fb.hpp"
#include "mem/early_pmalloc.hpp"
#include "sched/process.hpp"
#include "arch/cpu.hpp"
#include "exe/pe_headers.hpp"
#include <hz/optional.hpp>
#include <hz/algorithm.hpp>
#include <hz/pair.hpp>

#pragma section(".limine_requests$a", write)
#pragma section(".limine_requests$b", write)
#pragma section(".limine_requests$c", write)

namespace {
	[[gnu::section(".limine_requests$a"), gnu::used]] volatile LIMINE_REQUESTS_START_MARKER

	[[gnu::section(".limine_requests$b"), gnu::used]] volatile LIMINE_BASE_REVISION(3)

	[[gnu::section(".limine_requests$b")]] volatile limine_memmap_request MMAP_REQ {
		.id = LIMINE_MEMMAP_REQUEST,
		.revision = 0,
		.response = nullptr
	};

	[[gnu::section(".limine_requests$b")]] volatile limine_framebuffer_request FB_REQ {
		.id = LIMINE_FRAMEBUFFER_REQUEST,
		.revision = 0,
		.response = nullptr
	};

	[[gnu::section(".limine_requests$b")]] volatile limine_rsdp_request RSDP_REQ {
		.id = LIMINE_RSDP_REQUEST,
		.revision = 0,
		.response = nullptr
	};

	[[gnu::section(".limine_requests$b")]] volatile limine_hhdm_request HHDM_REQ {
		.id = LIMINE_HHDM_REQUEST,
		.revision = 0,
		.response = nullptr
	};

	[[gnu::section(".limine_requests$b")]] volatile limine_kernel_address_request KERNEL_ADDR_REQ {
		.id = LIMINE_KERNEL_ADDRESS_REQUEST,
		.revision = 0,
		.response = nullptr
	};

	[[gnu::section(".limine_requests$b")]] volatile limine_module_request MODULE_REQ {
		.id = LIMINE_MODULE_REQUEST,
		.revision = 0,
		.response = nullptr,
		.internal_module_count = 0,
		.internal_modules = nullptr
	};

	[[gnu::section(".limine_requests$c"), gnu::used]] volatile LIMINE_REQUESTS_END_MARKER
}

hz::optional<hz::pair<void*, usize>> x86_get_module(kstd::string_view name) {
	if (!MODULE_REQ.response) {
		return hz::nullopt;
	}

	for (usize i = 0; i < MODULE_REQ.response->module_count; ++i) {
		auto mod = MODULE_REQ.response->modules[i];
		if (kstd::string_view {mod->cmdline} == name) {
			return {hz::pair {mod->address, mod->size}};
		}
	}

	return hz::nullopt;
}

static constexpr usize SIZE_2MB = 1024 * 1024 * 2;

static void setup_pat() {
	u64 value = 0;
	// PA0 write back
	value |= 0x6;
	// PA1 write combining
	value |= 0x1 << 8;
	// PA2 write through
	value |= 0x4 << 16;
	// PA3 uncached
	value |= 0x7 << 24;

	msrs::IA32_PAT.write(value);
}

extern "C" DosHeader __ImageBase;

extern usize HHDM_START;

static void setup_memory(usize max_addr) {
	setup_pat();

	const auto kernel_phys = KERNEL_ADDR_REQ.response->physical_base;
	const auto kernel_virt = reinterpret_cast<usize>(&__ImageBase);

	auto* kernel_hdr = offset(&__ImageBase, const PeHeader64*, __ImageBase.e_lfanew);
	assert(kernel_hdr->signature == IMAGE_NT_SIGNATURE);
	auto* sections = offset(&kernel_hdr->opt, const PeSectionHeader*, kernel_hdr->coff.size_of_opt_hdr);

	for (usize i = 0; i < ALIGNUP(kernel_hdr->opt.size_of_headers, PAGE_SIZE); i += PAGE_SIZE) {
		(void) KERNEL_MAP->map(kernel_virt + i, kernel_phys + i, PageFlags::Read, CacheMode::WriteBack);
	}

	for (usize i = 0; i < kernel_hdr->coff.num_of_sections; ++i) {
		auto& section = sections[i];

		auto aligned_offset = ALIGNDOWN(section.virt_addr, PAGE_SIZE);
		usize size = ALIGNUP(section.virt_size + section.virt_addr % PAGE_SIZE, PAGE_SIZE);

		PageFlags flags {};
		if (section.characteristics & IMAGE_SCN_READ) {
			flags |= PageFlags::Read;
		}
		if (section.characteristics & IMAGE_SCN_WRITE) {
			flags |= PageFlags::Write;
		}
		if (section.characteristics & IMAGE_SCN_EXECUTE) {
			flags |= PageFlags::Execute;
		}

		for (usize j = 0; j < size; j += PAGE_SIZE) {
			(void) KERNEL_MAP->map(
				kernel_virt + aligned_offset + j,
				kernel_phys + aligned_offset + j,
				flags,
				CacheMode::WriteBack);
		}
	}

	for (usize i = 0; i < max_addr; i += SIZE_2MB) {
		(void) KERNEL_MAP->map_2mb(HHDM_START + i, i, PageFlags::Read | PageFlags::Write, CacheMode::WriteBack);
	}

	KERNEL_MAP->use();
}

[[noreturn]] void arch_start(void* rsdp);

static u64 BSP_CPU_DUMMY[sizeof(Cpu) / 8] {reinterpret_cast<u64>(&BSP_CPU_DUMMY)};

extern "C" [[noreturn, gnu::used]] void early_start() {
	call_constructors();

	if (FB_REQ.response && FB_REQ.response->framebuffer_count) {
		auto fb = FB_REQ.response->framebuffers[0];
		FB_LOG.initialize(
			to_phys(fb->address),
			fb->address,
			static_cast<u32>(fb->width),
			static_cast<u32>(fb->height),
			fb->pitch,
			fb->bpp);
	}

	HHDM_START = HHDM_REQ.response->offset;

	auto rsdp = reinterpret_cast<usize>(RSDP_REQ.response->address);

	usize max_usable_phys_addr = 0;
	usize max_phys_addr = 0;

	for (usize i = 0; i < MMAP_REQ.response->entry_count; ++i) {
		auto entry = MMAP_REQ.response->entries[i];
		if (entry->type == LIMINE_MEMMAP_USABLE) {
			max_usable_phys_addr = hz::max(max_usable_phys_addr, entry->base + entry->length);

			early_pmalloc_add_region(entry->base, entry->length);
		}

		max_phys_addr = hz::max(max_phys_addr, entry->base + entry->length);
	}

	HHDM_END = HHDM_START + max_phys_addr;

	KERNEL_MAP.initialize(nullptr);
	KERNEL_MAP->fill_high_half();

	max_phys_addr = ALIGNUP(max_phys_addr, SIZE_2MB);
	KERNEL_VSPACE.init(HHDM_START + max_phys_addr, KERNEL_ADDR_REQ.response->virtual_base - (HHDM_START + max_phys_addr));

	msrs::IA32_GSBASE.write(reinterpret_cast<u64>(&BSP_CPU_DUMMY));

	pmalloc_init(max_usable_phys_addr);

	for (usize i = 0; i < MMAP_REQ.response->entry_count; ++i) {
		auto entry = MMAP_REQ.response->entries[i];

		if (entry->type == LIMINE_MEMMAP_USABLE) {
			pmalloc_create_struct_pages(entry->base, entry->length);
		}

		max_phys_addr = hz::max(max_phys_addr, entry->base + entry->length);
	}

	asm volatile("" : : : "memory");

	setup_memory(max_phys_addr);

	early_pmalloc_finalize();

	arch_start(to_virt<void>(reinterpret_cast<usize>(rsdp)));
}
