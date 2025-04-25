#pragma once

#include "object.hpp"
#include "utils/spinlock.hpp"
#include "utils/push_lock.hpp"

#include <hz/atomic.hpp>
#include <hz/array.hpp>

struct OBJECT_TYPE {
	LIST_ENTRY type_list;
	UNICODE_STRING name;
	PVOID default_object;
	UCHAR index;
	ULONG total_number_of_objects;
	ULONG total_number_of_handles;
	ULONG high_number_of_objects;
	ULONG high_number_of_handles;
	OBJECT_TYPE_INITIALIZER type_info;
};

struct OBJECT_CREATE_INFORMATION {
	ULONG attribs;
	HANDLE root_directory;
	KPROCESSOR_MODE probe_mode;
	ULONG paged_pool_charge;
	ULONG non_paged_pool_charge;
	ULONG security_descriptor_charge;
	SECURITY_DESCRIPTOR* security_descriptor;
	PVOID parse_ctx;
};

struct OBJECT_HEADER {
	hz::atomic<LONGLONG> pointer_count;
	union {
		hz::atomic<LONGLONG> handle_count;
		PVOID next_to_free;
	};
	EX_PUSH_LOCK lock;
	UCHAR type_index;
	union {
		UCHAR trace_flags;
		struct {
			UCHAR dbg_ref_trace : 1;
			UCHAR dbg_trace_permanent : 1;
		};
	};
	UCHAR info_mask;
	union {
		UCHAR flags;
		struct {
			UCHAR new_object : 1;
			UCHAR kernel_object : 1;
			UCHAR kernel_only_access : 1;
			UCHAR exclusive_object : 1;
			UCHAR permanent_object : 1;
			UCHAR default_security_quota : 1;
			UCHAR single_handle_entry : 1;
			UCHAR deleted_inline : 1;
		};
	};
	// used as a size
	ULONG reserved;
	OBJECT_CREATE_INFORMATION* object_create_info;
	SECURITY_DESCRIPTOR* security_descriptor;
};
static_assert(sizeof(OBJECT_HEADER) % 16 == 0);

struct ObjectDirectory;

struct OBJECT_HEADER_CREATOR_INFO {
	LIST_ENTRY type_list;
	PVOID creator_unique_process;
	USHORT creator_back_trace_index;
	USHORT reserved1;
	ULONG reserved2;
};

struct OBJECT_HEADER_NAME_INFO {
	ObjectDirectory* dir;
	UNICODE_STRING name;
	LONG reference_count;
	ULONG reserved;
};

struct OBJECT_HANDLE_COUNT_ENTRY {
	Process* process;
	ULONG handle_count : 24;
	ULONG lock_count : 8;
};

struct OBJECT_HANDLE_COUNT_DATABASE {
	ULONG count_entries;
	OBJECT_HANDLE_COUNT_ENTRY handle_count_entries[];
};

struct OBJECT_HEADER_HANDLE_INFO {
	union {
		OBJECT_HANDLE_COUNT_DATABASE* handle_count_database;
		OBJECT_HANDLE_COUNT_ENTRY single_entry;
	};
};

struct OBJECT_HEADER_QUOTA_INFO {
	ULONG paged_pool_charge;
	ULONG non_paged_pool_charge;
	ULONG security_descriptor_charge;
	ULONG reserved1;
	PVOID security_descriptor_quota_block;
	ULONGLONG reserved2;
};

struct OBJECT_HEADER_PROCESS_INFO {
	Process* exclusive_process;
	ULONGLONG reserved;
};

struct OBJECT_HEADER_AUDIT_INFO {
	PVOID security_descriptor;
	ULONGLONG reserved;
};

struct OBJECT_HEADER_EXTENDED_INFO {
	struct OBJECT_FOOTER* footer;
	ULONGLONG reserved;
};

struct OBJECT_HEADER_PADDING_INFO {
	ULONG padding_amount;
};

namespace {
	constexpr auto OBJECT_INFO_MASK_TO_OFFSET = [] {
		hz::array<u8, 0x100> arr {};

		u8 sizes[] {
			sizeof(OBJECT_HEADER_CREATOR_INFO),
			sizeof(OBJECT_HEADER_NAME_INFO),
			sizeof(OBJECT_HEADER_HANDLE_INFO),
			sizeof(OBJECT_HEADER_QUOTA_INFO),
			sizeof(OBJECT_HEADER_PROCESS_INFO),
			sizeof(OBJECT_HEADER_AUDIT_INFO),
			sizeof(OBJECT_HEADER_EXTENDED_INFO),
			sizeof(OBJECT_HEADER_PADDING_INFO)
		};

		u32 implemented = 7;
		for (u32 i = 0; i < 1U << (implemented + 1); ++i) {
			for (u32 j = 0; j <= implemented; ++j) {
				if (i & 1U << j) {
					arr[i] += sizes[j];
				}
			}
		}

		return arr;
	}();

	OBJECT_HEADER_CREATOR_INFO* object_header_creator_info(OBJECT_HEADER* hdr) {
		if (hdr->info_mask & 1 << 0) {
			return offset(hdr, OBJECT_HEADER_CREATOR_INFO*, -OBJECT_INFO_MASK_TO_OFFSET[hdr->info_mask & 0b1]);
		}
		else {
			return nullptr;
		}
	}

	OBJECT_HEADER_NAME_INFO* object_header_name_info(OBJECT_HEADER* hdr) {
		if (hdr->info_mask & 1 << 1) {
			return offset(hdr, OBJECT_HEADER_NAME_INFO*, -OBJECT_INFO_MASK_TO_OFFSET[hdr->info_mask & 0b11]);
		}
		else {
			return nullptr;
		}
	}

	OBJECT_HEADER_HANDLE_INFO* object_header_handle_info(OBJECT_HEADER* hdr) {
		if (hdr->info_mask & 1 << 2) {
			return offset(hdr, OBJECT_HEADER_HANDLE_INFO*, -OBJECT_INFO_MASK_TO_OFFSET[hdr->info_mask & 0b111]);
		}
		else {
			return nullptr;
		}
	}

	OBJECT_HEADER_QUOTA_INFO* object_header_quota_info(OBJECT_HEADER* hdr) {
		if (hdr->info_mask & 1 << 3) {
			return offset(hdr, OBJECT_HEADER_QUOTA_INFO*, -OBJECT_INFO_MASK_TO_OFFSET[hdr->info_mask & 0b1111]);
		}
		else {
			return nullptr;
		}
	}

	OBJECT_HEADER_PROCESS_INFO* object_header_process_info(OBJECT_HEADER* hdr) {
		if (hdr->info_mask & 1 << 4) {
			return offset(hdr, OBJECT_HEADER_PROCESS_INFO*, -OBJECT_INFO_MASK_TO_OFFSET[hdr->info_mask & 0b11111]);
		}
		else {
			return nullptr;
		}
	}

	OBJECT_HEADER_AUDIT_INFO* object_header_audit_info(OBJECT_HEADER* hdr) {
		if (hdr->info_mask & 1 << 5) {
			return offset(hdr, OBJECT_HEADER_AUDIT_INFO*, -OBJECT_INFO_MASK_TO_OFFSET[hdr->info_mask & 0b111111]);
		}
		else {
			return nullptr;
		}
	}

	OBJECT_HEADER_EXTENDED_INFO* object_header_extended_info(OBJECT_HEADER* hdr) {
		if (hdr->info_mask & 1 << 6) {
			return offset(hdr, OBJECT_HEADER_EXTENDED_INFO*, -OBJECT_INFO_MASK_TO_OFFSET[hdr->info_mask & 0b1111111]);
		}
		else {
			return nullptr;
		}
	}

	OBJECT_HEADER_PADDING_INFO* object_header_padding_info(OBJECT_HEADER* hdr) {
		if (hdr->info_mask & 1 << 7) {
			return offset(hdr, OBJECT_HEADER_PADDING_INFO*, -OBJECT_INFO_MASK_TO_OFFSET[hdr->info_mask & 0b11111111]);
		}
		else {
			return nullptr;
		}
	}

	OBJECT_HEADER* get_header(PVOID object) {
		return static_cast<OBJECT_HEADER*>(object) - 1;
	}
}

