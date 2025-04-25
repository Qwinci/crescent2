#pragma once
#include "types.hpp"
#include "mem/mem.hpp"
#include "arch/caching.hpp"
#include <hz/list.hpp>
#include <hz/slist.hpp>

struct Page;

extern Page* PAGE_REGION;

struct Page {
	hz::list_hook hook {};

	union {
		struct {
			usize count {};
		} pm {};

		struct {
			hz::slist_hook freelist;
			u16 count;
		} slab;

		struct {
			CacheMode cache_mode;
		} allocated;
	};

	[[nodiscard]] inline usize phys() const {
		return (this - PAGE_REGION) * PAGE_SIZE;
	}

	static inline Page* from_phys(usize phys) {
		return &PAGE_REGION[phys / PAGE_SIZE];
	}
};

usize pmalloc();
usize pmalloc_in_range(usize low, usize high);
usize pmalloc_contiguous(usize low, usize high, usize count, usize boundary);
void pfree(usize phys);
void pfree_contiguous(usize phys, usize count);
void pmalloc_add_from_early();
void pmalloc_create_struct_pages(usize base, usize size);
void pmalloc_init(usize max_usable_phys_addr);

extern usize MAX_USABLE_PHYS_ADDR;
