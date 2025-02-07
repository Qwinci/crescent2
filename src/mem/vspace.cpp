#include "vspace.hpp"
#include "arch/paging.hpp"
#include "mem/mem.hpp"
#include "pmalloc.hpp"
#include "utils/irq_guard.hpp"
#include "assert.hpp"
#include "sched/process.hpp"

VirtualSpace KERNEL_VSPACE;

void VirtualSpace::init(usize base, usize size) {
	vmem.init(base, size, PAGE_SIZE);
}

void VirtualSpace::destroy() {
	vmem.destroy(true);
}

void* VirtualSpace::alloc(usize hint, usize size) {
	auto ret = vmem.xalloc(size, hint, hint);
	if (!ret) {
		return nullptr;
	}

	return reinterpret_cast<void*>(ret);
}

void VirtualSpace::free(void* ptr, usize size) {
	vmem.xfree(reinterpret_cast<usize>(ptr), size);
}

void* VirtualSpace::alloc_backed(usize hint, usize size, PageFlags flags, CacheMode cache_mode) {
	auto vm = alloc(hint, size);
	if (!vm) {
		return nullptr;
	}

	auto& kernel_map = KERNEL_PROCESS->page_map;

	auto pages = ALIGNUP(size, PAGE_SIZE) / PAGE_SIZE;
	for (usize i = 0; i < pages; ++i) {
		auto virt = reinterpret_cast<u64>(vm) + i * PAGE_SIZE;

		auto phys = pmalloc();
		if (!phys) {
			goto cleanup;
		}

		if (!kernel_map.map(virt, phys, flags, cache_mode)) {
			goto cleanup;
		}

		continue;

		cleanup:
		if (phys) {
			pfree(phys);
		}
		for (usize j = 0; j < i; ++j) {
			virt = reinterpret_cast<u64>(vm) + j * PAGE_SIZE;
			auto p = kernel_map.get_phys(virt);
			kernel_map.unmap(virt);
			pfree(p);
		}

		free(vm, size);
		return nullptr;
	}

	return vm;
}

void VirtualSpace::free_backed(void* ptr, usize size) {
	auto aligned = ALIGNUP(size, PAGE_SIZE);

	auto& kernel_map = KERNEL_PROCESS->page_map;

	for (usize i = 0; i < aligned; i += PAGE_SIZE) {
		auto virt = reinterpret_cast<u64>(ptr) + i;
		auto phys = kernel_map.get_phys(virt);
		kernel_map.unmap(virt);
		pfree(phys);
	}

	free(ptr, size);
}
