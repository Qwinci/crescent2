#include "process.hpp"
#include "mem/vspace.hpp"
#include "assert.hpp"
#include "priv/peb.h"

struct ProcessPeb {
	PEB peb;
	RTL_USER_PROCESS_PARAMETERS params;
	PEB_LDR_DATA ldr_data;
};

Process::Process(kstd::wstring_view name, bool user)
	// NOLINTNEXTLINE
	: name {nullptr}, page_map {user ? &KERNEL_PROCESS->page_map : nullptr}, user {user} {
	if (user) {
		this->name = name;
	}
	usize start = 0x200000;
	vmem.init(start, 0x7FFFFFFFE000 - start, PAGE_SIZE);

	if (user) {
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
}

ProcessDescriptor::~ProcessDescriptor() {
	auto old = KeAcquireSpinLockRaiseToDpc(&lock);

	if (process) {
		KeAcquireSpinLockAtDpcLevel(&process->desc_lock);
		process->descriptors.remove(this);
		KeReleaseSpinLockFromDpcLevel(&process->desc_lock);
		process = nullptr;
	}

	KeReleaseSpinLock(&lock, old);
}

void Process::mark_as_exiting(int exit_status, ProcessDescriptor* skip_lock) {
	assert(KeGetCurrentIrql() >= DISPATCH_LEVEL);
	KeAcquireSpinLockAtDpcLevel(&desc_lock);

	for (auto& desc : descriptors) {
		if (&desc != skip_lock) {
			KeAcquireSpinLockAtDpcLevel(&desc.lock);
			assert(desc.process == this);
			desc.process = nullptr;
			desc.status = exit_status;
			KeReleaseSpinLockFromDpcLevel(&desc.lock);
		}
		else {
			assert(desc.process == this);
			desc.process = nullptr;
			desc.status = exit_status;
		}
	}

	descriptors.clear();

	exiting = true;

	KeReleaseSpinLockFromDpcLevel(&desc_lock);
}

ProcessDescriptor* Process::create_descriptor() {
	/*ObjectWrapper<ProcessDescriptor> desc {};
	desc->process = this;

	auto old = KeAcquireSpinLockRaiseToDpc(&desc_lock);

	descriptors.push(desc.get());

	KeReleaseSpinLock(&desc_lock, old);

	return desc;*/
	// todo
	assert(false);
}

Process::~Process() {
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

		auto& kernel_map = KERNEL_PROCESS->page_map;

		for (usize i = 0; i < size; i += PAGE_SIZE) {
			auto phys = pmalloc();
			if (!phys || !page_map.map(virt + i, phys, page_flags, cache_mode) ||
			    (kernel_mapping && !kernel_map.map(kernel_virt + i, phys, PageFlags::Read | PageFlags::Write, CacheMode::WriteBack))) {
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
		auto& KERNEL_MAP = KERNEL_PROCESS->page_map;
		for (usize i = 0; i < size; ++i) {
			KERNEL_MAP.unmap(reinterpret_cast<u64>(ptr) + i);
		}
		KERNEL_VSPACE.free(ptr, size);
	}
}

hz::manually_init<Process> KERNEL_PROCESS;
