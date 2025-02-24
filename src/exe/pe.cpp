#include "pe.hpp"
#include "pe_headers.hpp"
#include "sched/process.hpp"
#include "assert.hpp"
#include "cstring.hpp"
#include "mem/vspace.hpp"

struct BaseRelocEntry {
	u32 page_rva;
	u32 block_size;
};

#define IMAGE_REL_BASED_HIGH 1
#define IMAGE_REL_BASED_LOW 2
#define IMAGE_REL_BASED_HIGHLOW 3
#define IMAGE_REL_BASED_DIR64 10

struct ImportEntry {
	u32 import_lookup_table_rva;
	u32 time_date_stamp;
	u32 forwarder_chain;
	u32 name_rva;
	u32 iat_rva;
};

#define IMAGE_ORDINAL_FLAG_64 (1ULL << 63)
#define IMAGE_ORDINAL_FLAG_32 (1ULL << 31)

struct ExportDirectory {
	u32 export_flags;
	u32 time_date_stamp;
	u16 major_version;
	u16 minor_version;
	u32 name_rva;
	u32 ordinal_base;
	u32 num_of_addr_table_entries;
	u32 num_of_name_ptrs;
	u32 export_addr_table_rva;
	u32 name_ptr_rva;
	u32 ordinal_table_rva;
};

extern "C" DosHeader __ImageBase;

static void* resolve_name(hz::string_view name) {
	auto* hdr = offset(&__ImageBase, const PeHeader64*, __ImageBase.e_lfanew);
	auto data_dir = hdr->opt.data_dirs[IMAGE_DIRECTORY_ENTRY_EXPORT];
	if (!data_dir.size) {
		return nullptr;
	}

	auto* dir = offset(&__ImageBase, const ExportDirectory*, data_dir.virt_addr);

	auto* addr_table = offset(&__ImageBase, const u32*, dir->export_addr_table_rva);

	// by ordinal
	u32 rva;
	if (false) {
		u16 ordinal = 0;
		if (ordinal < dir->ordinal_base) {
			return nullptr;
		}
		u16 unbiased_ordinal = ordinal - dir->ordinal_base;
		if (unbiased_ordinal >= dir->num_of_addr_table_entries) {
			return nullptr;
		}
		rva = addr_table[unbiased_ordinal];
	}
	else {
		auto* name_ptrs = offset(&__ImageBase, const u32*, dir->name_ptr_rva);
		auto* ordinals = offset(&__ImageBase, const u16*, dir->ordinal_table_rva);

		bool found = false;
		for (u32 i = 0; i < dir->num_of_name_ptrs; ++i) {
			auto* cur_name = offset(&__ImageBase, const char*, name_ptrs[i]);
			if (cur_name == name) {
				u16 offset = ordinals[i];
				rva = addr_table[offset];
				found = true;
				break;
			}
		}

		if (!found) {
			return nullptr;
		}
	}

	return offset(&__ImageBase, void*, rva);
}

hz::result<LoadedPe, int> pe_load(Process* process, std::shared_ptr<VNode>& file, bool user) {
	usize size;
	if (auto s = file->stat()) {
		size = s->size;
	}
	else {
		panic("[kernel][pe]: file stat failed");
	}

	assert(size > sizeof(DosHeader) + sizeof(PeHeader));

	DosHeader dos_hdr {};
	auto read = file->read(&dos_hdr, 0, sizeof(dos_hdr));
	assert(read);
	assert(read.value() == sizeof(DosHeader));

	assert(dos_hdr.e_magic == DOS_MAGIC);
	assert(dos_hdr.e_lfanew < size - sizeof(PeHeader));

	PeHeader common_hdr {};
	read = file->read(&common_hdr, dos_hdr.e_lfanew, sizeof(common_hdr));
	assert(read);
	assert(read.value() == sizeof(common_hdr));

	assert(common_hdr.signature == IMAGE_NT_SIGNATURE);

#ifdef __x86_64__
	assert(common_hdr.coff.machine == IMAGE_FILE_MACHINE_AMD64 ||
		common_hdr.coff.machine == IMAGE_FILE_MACHINE_I386);
#else
#error unsupported architecture
#endif

	usize image_base;
	usize image_size;
	usize size_of_headers;
	DataDirectory base_reloc_dir {};
	DataDirectory import_dir {};
	if (common_hdr.opt.magic == IMAGE_OPTIONAL_PE64_MAGIC) {
		PeHeader64 hdr {};
		read = file->read(&hdr, dos_hdr.e_lfanew, sizeof(hdr));
		assert(read);
		assert(read.value() == sizeof(hdr));

		image_base = hdr.opt.image_base;
		image_size = hdr.opt.size_of_image;
		size_of_headers = hdr.opt.size_of_headers;
		base_reloc_dir = hdr.opt.data_dirs[IMAGE_DIRECTORY_ENTRY_BASERELOC];
		import_dir = hdr.opt.data_dirs[IMAGE_DIRECTORY_ENTRY_IMPORT];
	}
	else {
		PeHeader32 hdr {};
		read = file->read(&hdr, dos_hdr.e_lfanew, sizeof(hdr));
		assert(read);
		assert(read.value() == sizeof(hdr));

		image_base = hdr.opt.image_base;
		image_size = hdr.opt.size_of_image;
		size_of_headers = hdr.opt.size_of_headers;
		base_reloc_dir = hdr.opt.data_dirs[IMAGE_DIRECTORY_ENTRY_BASERELOC];
		import_dir = hdr.opt.data_dirs[IMAGE_DIRECTORY_ENTRY_IMPORT];
	}

	assert(size_of_headers);
	assert(size_of_headers < size);

	UniqueKernelMapping mapping;

	usize load_base;
	if (common_hdr.coff.characteristics & IMAGE_FILE_RELOCS_STRIPPED) {
		if (user) {
			load_base = process->allocate(
				reinterpret_cast<void*>(image_base),
				image_size,
				PageFlags::Read | PageFlags::Write,
				&mapping);
		}
		else {
			load_base = reinterpret_cast<usize>(KERNEL_VSPACE.alloc_backed(
				reinterpret_cast<usize>(image_base),
				image_size,
				PageFlags::Read | PageFlags::Write));
			mapping.ptr = reinterpret_cast<void*>(load_base);
		}
	}
	else {
		if (user) {
			load_base = process->allocate(
				nullptr,
				image_size,
				PageFlags::Read | PageFlags::Write,
				&mapping);
		}
		else {
			load_base = reinterpret_cast<usize>(KERNEL_VSPACE.alloc_backed(
				0,
				image_size,
				PageFlags::Read | PageFlags::Write));
			mapping.ptr = reinterpret_cast<void*>(load_base);
		}
	}

	assert(load_base);

	read = file->read(mapping.data(), 0, size_of_headers);
	assert(read);
	assert(read.value() == size_of_headers);

	usize sect_offset = dos_hdr.e_lfanew + offsetof(PeHeader, opt) + common_hdr.coff.size_of_opt_hdr;
	assert(sect_offset + common_hdr.coff.num_of_sections * sizeof(PeSectionHeader) <= size);

	for (int i = 0; i < common_hdr.coff.num_of_sections; ++i) {
		PeSectionHeader sect {};
		memcpy(&sect, offset(mapping.data(), void*, sect_offset + i * sizeof(PeSectionHeader)), sizeof(PeSectionHeader));

		if (sect.characteristics & IMAGE_SCN_MEM_DISCARDABLE) {
			continue;
		}

		assert(sect.ptr_to_raw_data + sect.size_of_raw_data <= image_size);

		auto dest = offset(mapping.data(), void*, sect.virt_addr);

		read = file->read(dest, sect.ptr_to_raw_data, sect.size_of_raw_data);
		assert(read);
		assert(read.value() == sect.size_of_raw_data);

		if (sect.size_of_raw_data < sect.virt_size) {
			memset(offset(dest, void*, sect.size_of_raw_data), 0, sect.virt_size - sect.size_of_raw_data);
		}
	}

	assert(base_reloc_dir.virt_addr + base_reloc_dir.size <= image_size);

	auto* relocs = offset(mapping.data(), BaseRelocEntry*, base_reloc_dir.virt_addr);
	u32 total_size = base_reloc_dir.size;
	usize delta = load_base - image_base;
	if (delta) {
		for (u32 i = 0; i < total_size; relocs = offset(relocs, BaseRelocEntry*, relocs->block_size)) {
			u32 reloc_size = relocs->block_size;
			i += reloc_size;
			usize block_start = relocs->page_rva;

			reloc_size -= sizeof(BaseRelocEntry);
			size /= 2;

			auto* entry = reinterpret_cast<u16*>(&relocs[1]);

			for (u32 j = 0; j < reloc_size; ++j) {
				u8 type = entry[j] >> 12;
				u16 offset = entry[j] & 0xFFF;
				void* reloc_loc = offset(mapping.data(), void*, block_start + offset);

				switch (type) {
					case IMAGE_REL_BASED_LOW:
						*static_cast<volatile u16*>(reloc_loc) += delta;
						break;
					case IMAGE_REL_BASED_HIGH:
						*static_cast<volatile u16*>(reloc_loc) += delta >> 16;
						break;
					case IMAGE_REL_BASED_HIGHLOW:
						*static_cast<volatile u32*>(reloc_loc) += delta;
						break;
					case IMAGE_REL_BASED_DIR64:
						*static_cast<volatile u64*>(reloc_loc) += delta;
						break;
					default:
						panic("[kernel][pe]: unsupported relocation type ", type);
				}
			}
		}
	}

	if (!user && import_dir.size) {
		auto* imports = offset(mapping.data(), ImportEntry*, import_dir.virt_addr);
		for (; imports->import_lookup_table_rva; ++imports) {
			auto* lib_name = static_cast<const char*>(mapping.data()) + imports->name_rva;

			u64* entry = offset(mapping.data(), u64*, imports->iat_rva);
			for (; *entry; ++entry) {
				if (*entry & IMAGE_ORDINAL_FLAG_64) {
					u16 ordinal = *entry;
					panic("[kernel][pe]: unresolved import by ordinal ", ordinal);
				}
				else {
					auto* name = static_cast<const char*>(mapping.data()) + (*entry & 0xFFFFFFFF) + 2;
					auto resolved = resolve_name(name);
					if (!resolved) {
						panic("[kernel][pe]: unresolved import '", name, "'");
					}

					*entry = reinterpret_cast<usize>(resolved);
				}
			}
		}
	}

	for (int i = 0; i < common_hdr.coff.num_of_sections; ++i) {
		PeSectionHeader sect {};
		memcpy(&sect, offset(mapping.data(), void*, sect_offset + i * sizeof(PeSectionHeader)), sizeof(PeSectionHeader));

		if (sect.characteristics & IMAGE_SCN_MEM_DISCARDABLE) {
			continue;
		}

		auto addr = load_base + sect.virt_addr;
		usize misalign = addr & 0xFFF;
		addr &= ~0xFFF;

		PageFlags flags = PageFlags::User;
		if (sect.characteristics & IMAGE_SCN_READ) {
			flags |= PageFlags::Read;
		}
		if (sect.characteristics & IMAGE_SCN_WRITE) {
			flags |= PageFlags::Write;
		}
		if (sect.characteristics & IMAGE_SCN_EXECUTE) {
			flags |= PageFlags::Execute;
		}

		for (usize j = 0; j < sect.virt_size + misalign; j += PAGE_SIZE) {
			process->page_map.protect(addr + j, flags, CacheMode::WriteBack);
		}
	}

	mapping.ptr = nullptr;

	return hz::success(LoadedPe {
		.base = load_base,
		.entry = load_base + common_hdr.opt.addr_of_entry
	});
}
