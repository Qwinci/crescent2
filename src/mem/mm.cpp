#include "mm.hpp"
#include "mem/mem.hpp"
#include "vspace.hpp"
#include "sched/process.hpp"
#include "arch/arch_sched.hpp"
#include "sched/thread.hpp"
#include "utils/except.hpp"
#include "cstring.hpp"

NTAPI PVOID MmMapIoSpace(
	PHYSICAL_ADDRESS addr,
	SIZE_T num_of_bytes,
	MEMORY_CACHING_TYPE cache_type) {
	if (cache_type == MmCached || cache_type == MmHardwareCoherentCached) {
		return to_virt<void>(static_cast<usize>(addr.QuadPart));
	}

	assert(addr.QuadPart % PAGE_SIZE == 0);

	auto* virt = KERNEL_VSPACE.alloc(0, num_of_bytes);
	if (!virt) {
		return nullptr;
	}

	CacheMode cache_mode;
	if (cache_type == MmWriteCombined) {
		cache_mode = CacheMode::WriteCombine;
	}
	else {
		cache_mode = CacheMode::Uncached;
	}

	for (usize i = 0; i < num_of_bytes; i += PAGE_SIZE) {
		auto status = KERNEL_PROCESS->page_map.map(
			reinterpret_cast<u64>(virt) + i,
			static_cast<u64>(addr.QuadPart) + i,
			PageFlags::Read | PageFlags::Write,
			cache_mode);
		if (!status) {
			for (usize j = 0; j < i; j += PAGE_SIZE) {
				KERNEL_PROCESS->page_map.unmap(reinterpret_cast<u64>(virt) + j);
			}
			KERNEL_VSPACE.free(virt, num_of_bytes);
			return nullptr;
		}
	}

	return virt;
}

NTAPI void MmUnmapIoSpace(PVOID addr, SIZE_T num_of_bytes) {
	auto ptr = reinterpret_cast<u64>(addr);
	if (ptr <= HHDM_END) {
		return;
	}

	assert(ptr % PAGE_SIZE == 0);

	for (usize i = 0; i < num_of_bytes; i += PAGE_SIZE) {
		KERNEL_PROCESS->page_map.unmap(ptr + i);
	}
}

NTAPI PVOID MmMapLockedPagesSpecifyCache(
	MDL* mdl,
	KPROCESSOR_MODE access_mode,
	MEMORY_CACHING_TYPE cache_type,
	PVOID requested_addr,
	ULONG bug_check_on_failure,
	ULONG priority) {
	assert(mdl->mdl_flags & MDL_PAGES_LOCKED);
	assert(!(mdl->mdl_flags & MDL_MAPPED_TO_SYSTEM_VA));
	assert(!bug_check_on_failure);
	assert(mdl->byte_offset == 0);

	auto* pfn = reinterpret_cast<PFN_NUMBER*>(&mdl[1]);

	PageFlags flags = PageFlags::Read | PageFlags::Write | PageFlags::Execute;
	if (priority & MdlMappingNoWrite) {
		flags &= ~PageFlags::Write;
	}
	if (priority & MdlMappingNoExecute) {
		flags &= ~PageFlags::Execute;
	}

	CacheMode default_cache_mode;
	switch (cache_type) {
		case MmCached:
		case MmHardwareCoherentCached:
			default_cache_mode = CacheMode::WriteBack;
			break;
		case MmWriteCombined:
			default_cache_mode = CacheMode::WriteCombine;
			break;
		default:
			default_cache_mode = CacheMode::Uncached;
			break;
	}

	if (access_mode == KernelMode) {
		auto* virt = KERNEL_VSPACE.alloc(0, mdl->byte_count);
		if (!virt) {
			return nullptr;
		}

		for (usize i = 0; i < mdl->byte_count; i += PAGE_SIZE) {
			u64 phys = static_cast<u64>(pfn[i / PAGE_SIZE]) << 12;

			auto page = Page::from_phys(phys);
			assert(page);

			CacheMode cache_mode = page->allocated.cache_mode;
			if (cache_mode == CacheMode::None) {
				cache_mode = default_cache_mode;
				page->allocated.cache_mode = default_cache_mode;
			}

			auto status = KERNEL_PROCESS->page_map.map(
				reinterpret_cast<u64>(virt) + i,
				phys,
				flags,
				cache_mode);
			if (!status) {
				for (usize j = 0; j < i; j += PAGE_SIZE) {
					KERNEL_PROCESS->page_map.unmap(reinterpret_cast<u64>(virt) + j);
				}
				KERNEL_VSPACE.free(virt, mdl->byte_count);
				return nullptr;
			}
		}

		mdl->mdl_flags |= MDL_MAPPED_TO_SYSTEM_VA;
		mdl->mapped_system_va = virt;

		return virt;
	}
	else {
		assert(!mdl->start_va);

		flags |= PageFlags::User;
		flags &= ~PageFlags::Execute;

		auto* thread = get_current_thread();
		auto* process = thread->process;

		auto virt = process->allocate(
			requested_addr,
			mdl->byte_count,
			{},
			MappingFlags::None,
			nullptr);
		if (!virt) {
			ExRaiseStatus(STATUS_INSUFFICIENT_RESOURCES);
		}

		for (usize i = 0; i < mdl->byte_count; i += PAGE_SIZE) {
			u64 phys = static_cast<u64>(pfn[i / PAGE_SIZE]) << 12;

			auto page = Page::from_phys(phys);
			assert(page);

			CacheMode cache_mode = page->allocated.cache_mode;
			if (cache_mode == CacheMode::None) {
				cache_mode = default_cache_mode;
				page->allocated.cache_mode = default_cache_mode;
			}

			auto status = process->page_map.map(
				virt + i,
				phys,
				flags,
				cache_mode);
			if (!status) {
				for (usize j = 0; j < i; j += PAGE_SIZE) {
					process->page_map.unmap(virt + j);
				}
				process->free(virt, mdl->byte_count);
				ExRaiseStatus(STATUS_INSUFFICIENT_RESOURCES);
			}
		}

		mdl->process = process;
		mdl->start_va = reinterpret_cast<PVOID>(virt);

		return reinterpret_cast<PVOID>(virt);
	}
}

NTAPI extern "C" void MmUnmapLockedPages(PVOID base_addr, MDL* mdl) {
	if (base_addr == mdl->start_va) {
		auto* thread = get_current_thread();
		auto* process = thread->process;
		for (usize i = 0; i < mdl->byte_count; i += PAGE_SIZE) {
			process->page_map.unmap(reinterpret_cast<u64>(base_addr) + i);
		}
		process->free(reinterpret_cast<u64>(base_addr), mdl->byte_count);
		mdl->start_va = nullptr;
	}
	else {
		assert(mdl->mdl_flags & MDL_MAPPED_TO_SYSTEM_VA);
		assert(base_addr == mdl->mapped_system_va);

		for (usize i = 0; i < mdl->byte_count; i += PAGE_SIZE) {
			KERNEL_PROCESS->page_map.unmap(reinterpret_cast<u64>(base_addr) + i);
		}

		KERNEL_VSPACE.free(base_addr, mdl->byte_count);

		mdl->mapped_system_va = nullptr;
		mdl->mdl_flags &= ~MDL_MAPPED_TO_SYSTEM_VA;
	}
}

NTAPI MDL* MmAllocatePagesForMdl(
	PHYSICAL_ADDRESS low_addr,
	PHYSICAL_ADDRESS high_addr,
	PHYSICAL_ADDRESS skip_bytes,
	SIZE_T total_bytes) {
	return MmAllocatePagesForMdlEx(
		low_addr,
		high_addr,
		skip_bytes,
		total_bytes,
		MmNotMapped,
		0);
}

NTAPI extern "C" MDL* MmAllocatePagesForMdlEx(
	PHYSICAL_ADDRESS low_addr,
	PHYSICAL_ADDRESS high_addr,
	PHYSICAL_ADDRESS skip_bytes,
	SIZE_T total_bytes,
	MEMORY_CACHING_TYPE cache_type,
	ULONG flags) {
	assert(total_bytes < UINT32_MAX - PAGE_SIZE);
	ULONG pages = ALIGNUP(total_bytes, PAGE_SIZE) / PAGE_SIZE;

	MDL* mdl = static_cast<MDL*>(ExAllocatePool2(0, sizeof(MDL) + pages * sizeof(PFN_NUMBER), 0));
	if (!mdl) {
		return nullptr;
	}

	*mdl = {};
	mdl->byte_count = total_bytes;

	auto* pfn = reinterpret_cast<PFN_NUMBER*>(&mdl[1]);

	CacheMode cache_mode;
	switch (cache_type) {
		case MmCached:
		case MmHardwareCoherentCached:
			cache_mode = CacheMode::WriteBack;
			break;
		case MmWriteCombined:
			cache_mode = CacheMode::WriteCombine;
			break;
		case MmNotMapped:
			cache_mode = CacheMode::None;
			break;
		default:
			cache_mode = CacheMode::Uncached;
			break;
	}

	assert(!(flags & MM_ALLOCATE_REQUIRE_CONTIGUOUS));

	if (low_addr.QuadPart == 0 && high_addr.QuadPart == -1) {
		for (u32 i = 0; i < pages; ++i) {
			auto page = pmalloc();
			if (!page) {
				if (flags & MM_ALLOCATE_FULLY_REQUIRED) {
					for (u32 j = 0; j < i; ++j) {
						pfree(static_cast<u64>(pfn[j]) << 12);
					}

					ExFreePool(mdl);
					return nullptr;
				}

				if (i == 0) {
					ExFreePool(mdl);
					return nullptr;
				}
				else {
					mdl->byte_count = i * PAGE_SIZE;
					break;
				}
			}

			if (!(flags & MM_DONT_ZERO_ALLOCATION)) {
				memset(to_virt<void>(page), 0, PAGE_SIZE);
			}

			Page::from_phys(page)->allocated.cache_mode = cache_mode;
			pfn[i] = page >> 12;
		}
	}
	else {
		for (u32 i = 0; i < pages; ++i) {
			again:
			auto page = pmalloc_in_range(static_cast<usize>(low_addr.QuadPart), static_cast<usize>(high_addr.QuadPart));
			if (!page) {
				if (skip_bytes.QuadPart != 0) {
					if (static_cast<u64>(high_addr.QuadPart) < MAX_USABLE_PHYS_ADDR) {
						low_addr.QuadPart += skip_bytes.QuadPart;
						high_addr.QuadPart += skip_bytes.QuadPart;
						goto again;
					}
				}

				if (flags & MM_ALLOCATE_FULLY_REQUIRED) {
					for (u32 j = 0; j < i; ++j) {
						pfree(static_cast<u64>(pfn[j]) << 12);
					}

					ExFreePool(mdl);
					return nullptr;
				}

				if (i == 0) {
					ExFreePool(mdl);
					return nullptr;
				}
				else {
					mdl->byte_count = i * PAGE_SIZE;
					break;
				}
			}

			if (!(flags & MM_DONT_ZERO_ALLOCATION)) {
				memset(to_virt<void>(page), 0, PAGE_SIZE);
			}

			Page::from_phys(page)->allocated.cache_mode = cache_mode;
			pfn[i] = page >> 12;
		}
	}

	return mdl;
}

NTAPI void MmFreePagesFromMdl(MDL* mdl) {
	auto* pfn = reinterpret_cast<PFN_NUMBER*>(&mdl[1]);
	for (u32 i = 0; i < ALIGNUP(mdl->byte_count, PAGE_SIZE) / PAGE_SIZE; ++i) {
		assert(pfn[i]);
		pfree(static_cast<u64>(pfn[i]) << 12);
		pfn[i] = 0;
	}
}

NTAPI NTSTATUS MmAllocateMdlForIoSpace(
	MM_PHYSICAL_ADDRESS_LIST* phys_addr_list,
	SIZE_T num_of_entries,
	MDL** new_mdl) {
	ULONG pages = 0;
	for (usize i = 0; i < num_of_entries; ++i) {
		auto& entry = phys_addr_list[i];
		if (entry.phys_addr.QuadPart % PAGE_SIZE != 0 ||
			(entry.num_of_bytes % PAGE_SIZE != 0)) {
			return STATUS_INVALID_PARAMETER_1;
		}

		pages += entry.num_of_bytes / PAGE_SIZE;
	}

	MDL* mdl = static_cast<MDL*>(ExAllocatePool2(0, sizeof(MDL) + pages * sizeof(PFN_NUMBER), 0));
	if (!mdl) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	*mdl = {};
	mdl->byte_count = pages * PAGE_SIZE;

	auto* pfn = reinterpret_cast<PFN_NUMBER*>(&mdl[1]);

	ULONG page_index = 0;
	for (usize i = 0; i < num_of_entries; ++i) {
		auto& entry = phys_addr_list[i];

		for (usize j = 0; j < entry.num_of_bytes; j += PAGE_SIZE) {
			u64 phys = static_cast<u64>(entry.phys_addr.QuadPart) + j;
			pfn[page_index++] = phys >> 12;
		}
	}

	*new_mdl = mdl;
	return STATUS_SUCCESS;
}

namespace {
	struct ContiguousAlloc {
		hz::list_hook hook {};
		PVOID base {};
		usize phys {};
		usize size {};
	};

	KSPIN_LOCK CONTIGUOUS_ALLOC_LOCK {};
	hz::list<ContiguousAlloc, &ContiguousAlloc::hook> CONTIGUOUS_ALLOCS {};
}

NTAPI extern "C" PVOID MmAllocateContiguousMemorySpecifyCache(
	SIZE_T num_of_bytes,
	PHYSICAL_ADDRESS lowest_addr,
	PHYSICAL_ADDRESS highest_addr,
	PHYSICAL_ADDRESS boundary_addr_multiple,
	MEMORY_CACHING_TYPE cache_type) {
	auto* entry = static_cast<ContiguousAlloc*>(kcalloc(sizeof(ContiguousAlloc)));
	if (!entry) {
		return nullptr;
	}

	usize pages = ALIGNUP(num_of_bytes, PAGE_SIZE) / PAGE_SIZE;
	usize phys = pmalloc_contiguous(
		static_cast<usize>(lowest_addr.QuadPart),
		static_cast<usize>(highest_addr.QuadPart),
		pages,
		static_cast<usize>(boundary_addr_multiple.QuadPart));
	if (!phys) {
		kfree(entry, sizeof(ContiguousAlloc));
		return nullptr;
	}

	auto* virt = KERNEL_VSPACE.alloc(0, num_of_bytes);
	if (!virt) {
		pfree_contiguous(phys, pages);
		kfree(entry, sizeof(ContiguousAlloc));
		return nullptr;
	}

	CacheMode cache_mode;
	switch (cache_type) {
		case MmCached:
		case MmHardwareCoherentCached:
			cache_mode = CacheMode::WriteBack;
			break;
		case MmWriteCombined:
			cache_mode = CacheMode::WriteCombine;
			break;
		default:
			cache_mode = CacheMode::Uncached;
			break;
	}

	for (usize i = 0; i < num_of_bytes; i += PAGE_SIZE) {
		auto status = KERNEL_PROCESS->page_map.map(
			reinterpret_cast<u64>(virt) + i,
			phys + i,
			PageFlags::Read | PageFlags::Write,
			cache_mode);
		if (!status) {
			for (usize j = 0; j < i; j += PAGE_SIZE) {
				KERNEL_PROCESS->page_map.unmap(reinterpret_cast<u64>(virt) + j);
			}

			KERNEL_VSPACE.free(virt, num_of_bytes);
			pfree_contiguous(phys, pages);
			kfree(entry, sizeof(ContiguousAlloc));
			return nullptr;
		}
	}

	entry->base = virt;
	entry->phys = phys;
	entry->size = num_of_bytes;

	auto old = KeAcquireSpinLockRaiseToDpc(&CONTIGUOUS_ALLOC_LOCK);
	CONTIGUOUS_ALLOCS.push(entry);
	KeReleaseSpinLock(&CONTIGUOUS_ALLOC_LOCK, old);

	return virt;
}

NTAPI void MmFreeContiguousMemory(PVOID base_addr) {
	auto old = KeAcquireSpinLockRaiseToDpc(&CONTIGUOUS_ALLOC_LOCK);

	for (auto& alloc : CONTIGUOUS_ALLOCS) {
		if (alloc.base == base_addr) {
			CONTIGUOUS_ALLOCS.remove(&alloc);
			pfree_contiguous(alloc.phys, ALIGNUP(alloc.size, PAGE_SIZE) / PAGE_SIZE);
			KERNEL_VSPACE.free(base_addr, alloc.size);
			kfree(&alloc, sizeof(ContiguousAlloc));
			break;
		}
	}

	KeReleaseSpinLock(&CONTIGUOUS_ALLOC_LOCK, old);
}

NTAPI PHYSICAL_ADDRESS MmGetPhysicalAddress(PVOID base_addr) {
	auto addr = get_current_thread()->process->page_map.get_phys(reinterpret_cast<u64>(base_addr));
	return {.QuadPart = static_cast<LONGLONG>(addr)};
}
