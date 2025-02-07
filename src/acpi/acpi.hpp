#pragma once
#include "types.hpp"

namespace acpi {
	struct [[gnu::packed]] SdtHeader {
		char signature[4];
		u32 length;
		u8 revision;
		u8 checksum;
		char oem_id[6];
		char oem_table_id[8];
		u32 oem_revision;
		char creator_id[4];
		u32 creator_revision;
	};

	enum class RegionSpace : u8 {
		SystemMemory = 0x0
	};

	struct [[gnu::packed]] Address {
		RegionSpace space_id;
		u8 reg_bit_width;
		u8 reg_bit_offset;
		u8 access_size;
		u64 address;
	};

	void init(void* rsdp_ptr);
	void* get_table(const char (&signature)[5], u32 index = 0);
}
