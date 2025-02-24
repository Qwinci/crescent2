#pragma once

#include "types.hpp"

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
	u16 num_of_sections;
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
#define IMAGE_DIRECTORY_ENTRY_EXCEPTION 3
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
