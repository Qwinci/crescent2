#include "process.hpp"
#include "mem/vspace.hpp"

Process::Process(kstd::wstring_view name, bool user)
	: name {nullptr}, page_map {user ? &KERNEL_PROCESS->page_map : nullptr}, user {user} {
	if (user) {
		this->name = name;
	}
	usize start = 0x200000;
	vmem.init(start, 0x7FFFFFFFE000 - start, PAGE_SIZE);
}

Process::~Process() {
	vmem.destroy(true);
}

usize Process::allocate(void* base, usize size, PageFlags flags, UniqueKernelMapping* kernel_mapping) {
	auto real_base = ALIGNDOWN(reinterpret_cast<usize>(base), PAGE_SIZE);
	size = ALIGNUP(size, PAGE_SIZE);

	if (!size) {
		return 0;
	}

	auto virt = vmem.xalloc(size, real_base, real_base);
	if (!virt) {
		return 0;
	}

	UniqueKernelMapping unique_kernel_mapping {};
	usize kernel_virt = 0;
	auto& KERNEL_MAP = KERNEL_PROCESS->page_map;
	if (kernel_mapping) {
		auto* ptr = KERNEL_VSPACE.alloc(0, size);
		if (!ptr) {
			vmem.xfree(virt, size);
			return 0;
		}
		kernel_virt = reinterpret_cast<usize>(ptr);
		unique_kernel_mapping = {
			ptr,
			size
		};
	}

	CacheMode cache_mode = CacheMode::WriteBack;

	auto page_flags = flags | PageFlags::User;
	for (usize i = 0; i < size; i += PAGE_SIZE) {
		auto phys = pmalloc();
		if (!phys || !page_map.map(virt + i, phys, page_flags, cache_mode) ||
			(kernel_mapping && !KERNEL_MAP.map(kernel_virt + i, phys, PageFlags::Read | PageFlags::Write, CacheMode::WriteBack))) {
			for (usize j = 0; j < i; j += PAGE_SIZE) {
				auto page_phys = page_map.get_phys(virt + j);
				page_map.unmap(virt + j);
				pfree(page_phys);
			}

			if (kernel_mapping) {
				for (usize j = 0; j < i; j += PAGE_SIZE) {
					KERNEL_MAP.unmap(kernel_virt + j);
				}
				KERNEL_VSPACE.free(reinterpret_cast<void*>(kernel_virt), size);
			}

			vmem.xfree(virt, size);
			return 0;
		}
	}

	auto mapping = new Mapping {
		.base = virt,
		.size = size,
		.flags = flags
	};

	KIRQL old = KeAcquireSpinLockRaiseToDpc(&mapping_lock);
	mappings.insert(mapping);
	KeReleaseSpinLock(&mapping_lock, old);

	if (kernel_mapping) {
		*kernel_mapping = std::move(unique_kernel_mapping);
	}

	return virt;
}

void Process::free(usize ptr, usize) {
	KIRQL old = KeAcquireSpinLockRaiseToDpc(&mapping_lock);

	auto mapping = mappings.find<usize, &Mapping::base>(ptr);
	if (!mapping) {
		KeReleaseSpinLock(&mapping_lock, old);
		return;
	}

	usize base = mapping->base;
	for (usize i = 0; i < mapping->size; i += PAGE_SIZE) {
		auto page_phys = page_map.get_phys(base + i);
		page_map.unmap(base + i);
		pfree(page_phys);
	}

	vmem.xfree(base, mapping->size);

	mappings.remove(mapping);
	delete mapping;
	KeReleaseSpinLock(&mapping_lock, old);
}

UniqueKernelMapping::~UniqueKernelMapping() {
	if (ptr) {
		auto& KERNEL_MAP = KERNEL_PROCESS->page_map;
		for (usize i = 0; i < size; ++i) {
			KERNEL_MAP.unmap(reinterpret_cast<u64>(ptr) + i);
		}
		KERNEL_VSPACE.free(ptr, size);
	}
}

hz::manually_init<Process> KERNEL_PROCESS;
