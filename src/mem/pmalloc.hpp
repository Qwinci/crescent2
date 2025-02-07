#pragma once
#include "types.hpp"
#include "mem/mem.hpp"
#include <hz/list.hpp>
#include <hz/slist.hpp>

struct Page;

extern Page* PAGE_REGION;

struct Page {
	hz::list_hook hook {};

	union {
		struct {
			usize count {};
		} pm;

		struct {
			hz::slist_hook freelist;
			u16 count;
		} slab;
	};

	[[nodiscard]] inline usize phys() const {
		return (this - PAGE_REGION) * PAGE_SIZE;
	}

	static inline Page* from_phys(usize phys) {
		return &PAGE_REGION[phys / PAGE_SIZE];
	}
};

usize pmalloc();
void pfree(usize phys);
void pmalloc_add_from_early();
void pmalloc_create_struct_pages(usize base, usize size);
void pmalloc_init(usize max_usable_phys_addr);
