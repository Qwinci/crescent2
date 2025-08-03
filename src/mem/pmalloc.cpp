#include "pmalloc.hpp"
#include "vspace.hpp"
#include "assert.hpp"
#include "early_pmalloc.hpp"
#include "cstring.hpp"
#include "sched/process.hpp"

namespace {
	hz::list<Page, &Page::hook> LIST {};
	KSPIN_LOCK LOCK {};
}

Page* PAGE_REGION;
bool EARLY_PMALLOC = true;
usize MAX_USABLE_PHYS_ADDR = 0;

void pmalloc_init(usize max_usable_phys_addr) {
	MAX_USABLE_PHYS_ADDR = ALIGNUP(max_usable_phys_addr, PAGE_SIZE);
	PAGE_REGION = static_cast<Page*>(KERNEL_VSPACE.alloc(0, MAX_USABLE_PHYS_ADDR / PAGE_SIZE * sizeof(Page)));
	assert(PAGE_REGION);
}

void pmalloc_create_struct_pages(usize base, usize size) {
	auto start_entry = base / PAGE_SIZE;
	auto end_entry = start_entry + (size / PAGE_SIZE);

	usize start_offset = start_entry * sizeof(Page);
	usize aligned_start = ALIGNDOWN(start_offset, PAGE_SIZE);
	usize aligned_end = ALIGNUP(end_entry * sizeof(Page) + (start_offset & (PAGE_SIZE - 1)), PAGE_SIZE);

	for (usize j = aligned_start; j < aligned_end; j += PAGE_SIZE) {
		auto page_entry = reinterpret_cast<usize>(PAGE_REGION) + j;

		auto addr = KERNEL_MAP->get_phys(page_entry);
		if (!addr) {
			auto page = pmalloc();
			assert(page);
			memset(to_virt<void>(page), 0, PAGE_SIZE);

			auto status = KERNEL_MAP->map(
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

usize pmalloc_in_range(usize low, usize high) {
	auto old = KeAcquireSpinLockRaiseToDpc(&LOCK);

	usize ret = 0;
	for (auto& page : LIST) {
		auto phys = page.phys();
		if (phys >= low && phys < high) {
			LIST.remove(&page);

			if (page.pm.count > 1) {
				auto next = &(&page)[1];
				next->pm.count = page.pm.count - 1;
				LIST.push(next);
			}

			ret = phys;
			break;
		}
	}

	KeReleaseSpinLock(&LOCK, old);
	return ret;
}

usize pmalloc_contiguous(usize low, usize high, usize count, usize boundary) {
	assert(count);

	auto old = KeAcquireSpinLockRaiseToDpc(&LOCK);

	usize ret = 0;
	Page* next;
	for (auto* page = LIST.front(); page; page = next) {
		next = static_cast<Page*>(page->hook.next);

		auto start_phys = page->phys();

		if (boundary) {
			if (page->pm.count == 1 && (boundary - start_phys % boundary) < (count * PAGE_SIZE)) {
				continue;
			}
			else {
				if (boundary - start_phys % boundary < (count * PAGE_SIZE)) {
					auto align = ALIGNUP(start_phys, boundary);
					align -= start_phys;
					align /= PAGE_SIZE;
					if (page->pm.count > align) {
						auto remaining = page->pm.count - align;
						page->pm.count = align;

						page = &page[align];
						page->pm.count = remaining;
						start_phys = page->phys();
						LIST.push_front(page);
					}
					else {
						continue;
					}
				}
			}
		}

		if (start_phys < low ||
		    start_phys + count * PAGE_SIZE > high) {
			continue;
		}

		LIST.remove(page);

		if (page->pm.count > count) {
			auto* new_page = &page[count];
			new_page->pm.count = page->pm.count - count;
			LIST.push(new_page);
			ret = start_phys;
			break;
		}
		else {
			bool success = true;

			for (usize i = page->pm.count; i < count;) {
				auto expected = start_phys + i * PAGE_SIZE;

				bool found = false;
				for (auto* page2 = LIST.front(); page2; page2 = static_cast<Page*>(page2->hook.next)) {
					auto phys = page2->phys();
					auto end = phys + page2->pm.count * PAGE_SIZE;

					if (expected >= phys && expected < end) {
						auto before = expected - phys;
						if (before) {
							page2->pm.count = before / PAGE_SIZE;
						}
						else {
							LIST.remove(page2);
						}

						usize pages_available = (end - expected) / PAGE_SIZE;
						usize needed_pages = count - i;
						if (pages_available > needed_pages) {
							auto after_index = before / PAGE_SIZE + needed_pages;
							auto* page3 = &page2[after_index];
							page3->pm.count = pages_available - needed_pages;
							LIST.push(page3);
							i = count;
						}
						else {
							i += pages_available;
						}

						found = true;
						break;
					}
				}

				if (!found) {
					auto* page2 = Page::from_phys(start_phys);
					page2->pm.count = i;
					LIST.push_front(page2);
					success = false;
					break;
				}
			}

			if (success) {
				ret = start_phys;
				break;
			}
		}
	}

	KeReleaseSpinLock(&LOCK, old);
	return ret;
}

void pfree(usize phys) {
	auto old = KeAcquireSpinLockRaiseToDpc(&LOCK);
	auto page = Page::from_phys(phys);
	page->pm.count = 1;
	LIST.push(page);
	KeReleaseSpinLock(&LOCK, old);
}

void pfree_contiguous(usize phys, usize count) {
	auto old = KeAcquireSpinLockRaiseToDpc(&LOCK);
	auto page = Page::from_phys(phys);
	page->pm.count = count;
	LIST.push(page);
	KeReleaseSpinLock(&LOCK, old);
}
