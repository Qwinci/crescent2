#include "pe.hpp"
#include "sched/process.hpp"
#include "assert.hpp"
#include "cstring.hpp"
#include "mem/vspace.hpp"

#define IMAGE_NT_SIGNATURE 0x4550
#define DOS_MAGIC 0x5A4D

struct DosHeader {
	u16 e_magic;
	u16 e_cblp;
	u16 e_cp;
	u16 e_crlc;
	u16 e_cparhdr;
	u16 e_minalloc;
	u16 e_maxalloc;
	u16 e_ss;
	u16 e_sp;
	u16 e_csum;
	u16 e_ip;
	u16 e_cs;
	u16 e_lfarlc;
	u16 e_ovno;
	u16 e_res[4];
	u16 e_oemid;
	u16 e_oeminfo;
	u16 e_res2[10];
	u32 e_lfanew;
};

#define IMAGE_FILE_MACHINE_AMD64 0x8664
#define IMAGE_FILE_MACHINE_I386 0x14C

#define IMAGE_FILE_RELOCS_STRIPPED 0x1
#define IMAGE_FILE_DLL 0x2000

struct CoffHeader {
	u16 machine;
	u16 number_of_sections;
	u32 time_date_stamp;
	u32 ptr_to_symtab;
	u32 num_of_syms;
	u16 size_of_opt_hdr;
	u16 characteristics;
};

#define IMAGE_OPTIONAL_PE32_MAGIC 0x10B
#define IMAGE_OPTIONAL_PE64_MAGIC 0x20B

#define IMAGE_DIRECTORY_ENTRY_EXPORT 0
#define IMAGE_DIRECTORY_ENTRY_IMPORT 1
#define IMAGE_DIRECTORY_ENTRY_BASERELOC 5

struct DataDirectory {
	u32 virt_addr;
	u32 size;
};

struct PeStdOptionalHeader {
	u16 magic;
	u8 major_linker_version;
	u8 minor_linker_version;
	u32 size_of_code;
	u32 size_of_init_data;
	u32 size_of_uninit_data;
	u32 addr_of_entry;
	u32 base_of_code;
};

struct PeOptionalHeader32 {
	PeStdOptionalHeader hdr;
	u32 base_of_data;
	u32 image_base;
	u32 section_align;
	u32 file_align;
	u16 major_os_version;
	u16 minor_os_version;
	u16 major_image_version;
	u16 minor_image_version;
	u16 major_subsystem_version;
	u16 minor_subsystem_version;
	u32 win32_version_value;
	u32 size_of_image;
	u32 size_of_headers;
	u32 checksum;
	u16 subsystem;
	u16 dll_characteristics;
	u32 size_of_stack_reserve;
	u32 size_of_stack_commit;
	u32 size_of_heap_reserve;
	u32 size_of_heap_commit;
	u32 loader_flags;
	u32 num_of_rva_and_sizes;
	DataDirectory data_dirs[16];
};

struct PeOptionalHeader64 {
	PeStdOptionalHeader hdr;
	u64 image_base;
	u32 section_align;
	u32 file_align;
	u16 major_os_version;
	u16 minor_os_version;
	u16 major_image_version;
	u16 minor_image_version;
	u16 major_subsystem_version;
	u16 minor_subsystem_version;
	u32 win32_version_value;
	u32 size_of_image;
	u32 size_of_headers;
	u32 checksum;
	u16 subsystem;
	u16 dll_characteristics;
	u64 size_of_stack_reserve;
	u64 size_of_stack_commit;
	u64 size_of_heap_reserve;
	u64 size_of_heap_commit;
	u32 loader_flags;
	u32 num_of_rva_and_sizes;
	DataDirectory data_dirs[16];
};

struct PeHeader {
	u32 signature;
	CoffHeader coff;
	PeStdOptionalHeader opt;
};

struct PeHeader32 {
	u32 signature;
	CoffHeader coff;
	PeOptionalHeader32 opt;
};

struct PeHeader64 {
	u32 signature;
	CoffHeader coff;
	PeOptionalHeader64 opt;
};

#define IMAGE_SCN_MEM_DISCARDABLE 0x2000000
#define IMAGE_SCN_EXECUTE 0x20000000
#define IMAGE_SCN_READ 0x40000000
#define IMAGE_SCN_WRITE 0x80000000

struct PeSectionHeader {
	char name[8];
	u32 virt_size;
	u32 virt_addr;
	u32 size_of_raw_data;
	u32 ptr_to_raw_data;
	u32 ptr_to_relocs;
	u32 ptr_to_line_nums;
	u16 num_of_relocs;
	u16 num_of_line_nums;
	u32 characteristics;
};

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

extern usize _DYNAMIC[];
extern char KERNEL_START[];

#define DT_HASH 4
#define DT_STRTAB 5
#define DT_SYMTAB 6

struct Elf64Sym {
	u16 st_name;
	u8 st_info;
	u8 st_other;
	u16 st_shndx;
	u64 st_value;
	u64 st_size;
};

static void* resolve_name(hz::string_view name) {
	auto base = reinterpret_cast<usize>(KERNEL_START);

	u32* hash = nullptr;
	const char* strtab = nullptr;
	Elf64Sym* symtab = nullptr;

	for (usize* dyn = _DYNAMIC; *dyn; dyn += 2) {
		usize tag = *dyn;
		switch (tag) {
			case DT_HASH:
				hash = reinterpret_cast<u32*>(dyn[1] - 0xFFFFFFFF80000000 + base);
				break;
			case DT_STRTAB:
				strtab = reinterpret_cast<const char*>(dyn[1] - 0xFFFFFFFF80000000 + base);
				break;
			case DT_SYMTAB:
				symtab = reinterpret_cast<Elf64Sym*>(dyn[1] - 0xFFFFFFFF80000000 + base);
				break;
			default:
				break;
		}
	}

	assert(hash);
	assert(strtab);
	assert(symtab);
	u32 num_syms = hash[1];

	for (usize i = 0; i < num_syms; ++i) {
		auto& sym = symtab[i];
		auto* sym_name = strtab + sym.st_name;
		if (name == sym_name) {
			return reinterpret_cast<void*>(sym.st_value - 0xFFFFFFFF80000000 + base);
		}
	}

	return nullptr;
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
	assert(sect_offset + common_hdr.coff.number_of_sections * sizeof(PeSectionHeader) <= size);

	for (int i = 0; i < common_hdr.coff.number_of_sections; ++i) {
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

	if (import_dir.size) {
		auto* imports = offset(mapping.data(), ImportEntry*, import_dir.virt_addr);
		for (; imports->import_lookup_table_rva; ++imports) {
			auto* lib_name = static_cast<const char*>(mapping.data()) + imports->name_rva;
			println(lib_name);

			u64* entry = offset(mapping.data(), u64*, imports->iat_rva);
			for (; *entry; ++entry) {
				if (*entry & IMAGE_ORDINAL_FLAG_64) {
					u16 ordinal = *entry;
					panic("import by ordinal ", ordinal);
				}
				else {
					auto* name = static_cast<const char*>(mapping.data()) + (*entry & 0xFFFFFFFF) + 2;
					println(name);
					*entry = reinterpret_cast<usize>(resolve_name(name));
				}
			}
		}
	}

	for (int i = 0; i < common_hdr.coff.number_of_sections; ++i) {
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
		.entry = load_base + common_hdr.opt.addr_of_entry
	});
}
