#include "iospace.hpp"
#include "vspace.hpp"
#include "mem/mem.hpp"
#include "stdio.hpp"
#include "sched/process.hpp"

bool IoSpace::map(CacheMode cache_mode) {
	base = reinterpret_cast<usize>(KERNEL_VSPACE.alloc(0, size));
	if (!base) {
		println("[kernel]: failed to allocate mapping for io space");
		return false;
	}

	usize align = phys & (PAGE_SIZE - 1);
	usize aligned_phys = phys & ~(PAGE_SIZE - 1);

	auto& kernel_map = KERNEL_PROCESS->page_map;

	for (usize i = 0; i < size + align; i += PAGE_SIZE) {
		auto virt = base + i;
		if (!kernel_map.map(virt, aligned_phys + i, PageFlags::Read | PageFlags::Write, cache_mode)) {
			println("[kernel]: io space map failed");
			for (usize j = 0; j < i; j += PAGE_SIZE) {
				kernel_map.unmap(base + j);
			}
			return false;
		}
	}

	base += align;

	return true;
}
