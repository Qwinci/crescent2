#include "pmalloc.hpp"
#include "mem/mem.hpp"
#include "utils/irq_guard.hpp"
#include "vspace.hpp"
#include "assert.hpp"
#include "early_pmalloc.hpp"
#include "cstring.hpp"
#include "sched/process.hpp"
#include <hz/new.hpp>
#include <hz/spinlock.hpp>

struct Node {
	Node* next;
};

namespace {
	hz::list<Page, &Page::hook> LIST {};
	KSPIN_LOCK LOCK {};
}

Page* PAGE_REGION;
bool EARLY_PMALLOC = true;

void pmalloc_init(usize max_usable_phys_addr) {
	PAGE_REGION = static_cast<Page*>(KERNEL_VSPACE.alloc(0, max_usable_phys_addr / PAGE_SIZE * sizeof(Page)));
	assert(PAGE_REGION);
}

void pmalloc_create_struct_pages(usize base, usize size) {
	auto start_entry = base / PAGE_SIZE;
	auto end_entry = start_entry + (size / PAGE_SIZE);

	usize start_offset = start_entry * sizeof(Page);
	usize aligned_start = ALIGNDOWN(start_offset, PAGE_SIZE);
	usize aligned_end = ALIGNUP(end_entry * sizeof(Page) + (start_offset & (PAGE_SIZE - 1)), PAGE_SIZE);

	auto& kernel_map = KERNEL_PROCESS->page_map;

	for (usize j = aligned_start; j < aligned_end; j += PAGE_SIZE) {
		auto page_entry = reinterpret_cast<usize>(PAGE_REGION) + j;

		auto addr = kernel_map.get_phys(page_entry);
		if (!addr) {
			auto page = pmalloc();
			assert(page);
			memset(to_virt<void>(page), 0, PAGE_SIZE);

			auto status = kernel_map.map(
				page_entry,
				page,
				PageFlags::Read | PageFlags::Write,
				CacheMode::WriteBack);
			assert(status);
		}
	}
}

void pmalloc_add_mem(usize base, usize size) {
	auto page_base = base / PAGE_SIZE;

	auto page = &PAGE_REGION[page_base];
	page->pm.count = size / PAGE_SIZE;

	LIST.push(page);
}

usize pmalloc() {
	if (EARLY_PMALLOC) {
		return early_pmalloc();
	}

	auto old = KeAcquireSpinLockRaiseToDpc(&LOCK);

	auto page = LIST.pop();
	if (!page) {
		KeReleaseSpinLock(&LOCK, old);
		return 0;
	}

	if (page->pm.count > 1) {
		auto next = &page[1];
		next->pm.count = page->pm.count - 1;
		LIST.push(next);
	}

	KeReleaseSpinLock(&LOCK, old);
	return page->phys();
}

void pfree(usize phys) {
	auto old = KeAcquireSpinLockRaiseToDpc(&LOCK);
	auto page = Page::from_phys(phys);
	page->pm.count = 1;
	LIST.push(page);
	KeReleaseSpinLock(&LOCK, old);
}
