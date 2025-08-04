#include "process.hpp"
#include "mem/vspace.hpp"
#include "assert.hpp"
#include "priv/peb.h"

struct ProcessPeb {
	PEB peb;
	RTL_USER_PROCESS_PARAMETERS params;
	PEB_LDR_DATA ldr_data;
};

Process::Process(kstd::wstring_view name)
	// NOLINTNEXTLINE
	: name {name}, page_map {&*KERNEL_MAP}, user {true} {
	usize start = 0x200000;
	vmem.init(start, 0x7FFFFFFFE000 - start, PAGE_SIZE);

	UniqueKernelMapping tmp_peb_mapping;
	peb = reinterpret_cast<PEB*>(allocate(
		nullptr,
		sizeof(ProcessPeb),
		PageFlags::Read | PageFlags::Write,
		MappingFlags::Backed,
		&tmp_peb_mapping));
	assert(peb);
	auto* tmp_peb = new (tmp_peb_mapping.data()) ProcessPeb {};
	tmp_peb->peb.ProcessParameters = offset(peb, PRTL_USER_PROCESS_PARAMETERS, offsetof(ProcessPeb, params));
	tmp_peb->peb.Ldr = offset(peb, PPEB_LDR_DATA, offsetof(ProcessPeb, ldr_data));
}

Process::Process(const PageMap& map)
	: name {u"kernel"}, page_map {map, PageMap::OnlyKernel {}}, user {false} {}

void Process::mark_as_exiting(int new_exit_status) {
	assert(KeGetCurrentIrql() >= DISPATCH_LEVEL);
	KeAcquireSpinLockAtDpcLevel(&lock);

	exiting = true;
	exit_status = new_exit_status;

	KeReleaseSpinLockFromDpcLevel(&lock);
}

void Process::add_thread(Thread* thread) {
	thread->process = this;
	auto old = KeAcquireSpinLockRaiseToDpc(&threads_lock);
	threads.push(thread);
	ObfReferenceObject(this);
	KeReleaseSpinLock(&threads_lock, old);
}

void Process::remove_thread(Thread* thread) {
	auto old = KeAcquireSpinLockRaiseToDpc(&threads_lock);
	threads.remove(thread);
	KeReleaseSpinLock(&threads_lock, old);
	ObfDereferenceObject(this);
}

Process::~Process() {
	// removing the handle will dereference this again making the refcount a very large number,
	// but it doesn't really matter as the object is going to be freed by the previous call anyway
	SCHED_HANDLE_TABLE.remove(handle);

	auto old = KeAcquireSpinLockRaiseToDpc(&mapping_lock);

	Mapping* next;
	for (Mapping* mapping = mappings.get_first(); mapping; mapping = next) {
		next = static_cast<Mapping*>(mapping->hook.successor);

		auto base = mapping->base;
		for (usize i = 0; i < mapping->size; i += PAGE_SIZE) {
			auto phys = page_map.get_phys(base + i);
			assert(phys);
			pfree(phys);
		}

		vmem.xfree(base, mapping->size);
		delete mapping;
	}

	KeReleaseSpinLock(&mapping_lock, old);

	vmem.destroy(true);
}

usize Process::allocate(void* base, usize size, PageFlags flags, MappingFlags mapping_flags, UniqueKernelMapping* kernel_mapping) {
	auto real_base = ALIGNDOWN(reinterpret_cast<usize>(base), PAGE_SIZE);
	size = ALIGNUP(size + reinterpret_cast<usize>(base) % PAGE_SIZE, PAGE_SIZE);

	if (!size) {
		return 0;
	}

	auto virt = vmem.xalloc(size, real_base, real_base);
	if (!virt) {
		return 0;
	}

	UniqueKernelMapping unique_kernel_mapping {};
	usize kernel_virt = 0;
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

	if (mapping_flags & MappingFlags::Backed) {
		CacheMode cache_mode = CacheMode::WriteBack;
		auto page_flags = flags | PageFlags::User;

		for (usize i = 0; i < size; i += PAGE_SIZE) {
			auto phys = pmalloc();
			if (!phys || !page_map.map(virt + i, phys, page_flags, cache_mode) ||
			    (kernel_mapping && !KERNEL_MAP->map(
					kernel_virt + i,
					phys,
					PageFlags::Read | PageFlags::Write,
					CacheMode::WriteBack))) {
				for (usize j = 0; j < i; j += PAGE_SIZE) {
					auto page_phys = page_map.get_phys(virt + j);
					page_map.unmap(virt + j);
					pfree(page_phys);
				}

				vmem.xfree(virt, size);
				return 0;
			}
		}
	}

	auto mapping = new Mapping {
		.base = virt,
		.size = size,
		.flags = flags,
		.mapping_flags = mapping_flags
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
	if (mapping->mapping_flags & MappingFlags::Backed) {
		for (usize i = 0; i < mapping->size; i += PAGE_SIZE) {
			auto page_phys = page_map.get_phys(base + i);
			page_map.unmap(base + i);
			pfree(page_phys);
		}
	}

	vmem.xfree(base, mapping->size);

	mappings.remove(mapping);
	delete mapping;
	KeReleaseSpinLock(&mapping_lock, old);
}

UniqueKernelMapping::~UniqueKernelMapping() {
	if (ptr) {
		for (usize i = 0; i < size; ++i) {
			KERNEL_MAP->unmap(reinterpret_cast<u64>(ptr) + i);
		}
		KERNEL_VSPACE.free(ptr, size);
	}
}

Process* KERNEL_PROCESS;
hz::manually_init<PageMap> KERNEL_MAP;

HandleTable SCHED_HANDLE_TABLE {};
