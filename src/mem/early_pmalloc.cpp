#include "early_pmalloc.hpp"
#include "mem/mem.hpp"

namespace {
	struct Region {
		usize base;
		usize size;
	};
}

namespace {
	Region REGIONS[64];
	usize REGION_COUNT = 0;
}

void early_pmalloc_add_region(usize base, usize size) {
	if (REGION_COUNT == 64) {
		return;
	}
	REGIONS[REGION_COUNT++] = {
		.base = base,
		.size = size
	};
}

void pmalloc_add_mem(usize base, usize size);

extern bool EARLY_PMALLOC;

void early_pmalloc_finalize() {
	for (usize i = 0; i < REGION_COUNT; ++i) {
		auto& region = REGIONS[i];
		if (region.size) {
			pmalloc_add_mem(region.base, region.size);
		}
	}

	EARLY_PMALLOC = false;
}

usize early_pmalloc() {
	for (usize i = 0; i < REGION_COUNT; ++i) {
		auto& region = REGIONS[i];
		if (region.size) {
			auto page = region.base;
			region.base += PAGE_SIZE;
			region.size -= PAGE_SIZE;
			return page;
		}
	}

	return 0;
}
