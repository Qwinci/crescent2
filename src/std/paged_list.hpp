#pragma once

#include "list.hpp"
#include "mem/pool.hpp"

struct LOOKASIDE_LIST_EX;

using PALLOCATE_FUNCTION = PVOID (*)(
	POOL_TYPE pool_type,
	SIZE_T num_of_bytes,
	ULONG tag);
using PALLOCATE_FUNCTION_EX = PVOID (*)(
	POOL_TYPE pool_type,
	SIZE_T num_of_bytes,
	ULONG tag,
	LOOKASIDE_LIST_EX* lookaside);
using PFREE_FUNCTION = void (*)(PVOID buffer);
using PFREE_FUNCTION_EX = void (*)(
	PVOID buffer,
	LOOKASIDE_LIST_EX* lookaside);

#define GENERAL_LOOKASIDE_FIELDS \
	union { \
		SLIST_HEADER list_head; \
		SINGLE_LIST_ENTRY single_list_head; \
	}; \
	USHORT depth; \
	USHORT maximum_depth; \
	ULONG total_allocates; \
	union { \
		ULONG allocate_misses; \
		ULONG allocate_hits; \
	}; \
	ULONG total_frees; \
	union { \
		ULONG free_misses; \
		ULONG free_hits; \
	}; \
	POOL_TYPE type; \
	ULONG tag; \
	ULONG size; \
	union { \
		PALLOCATE_FUNCTION_EX allocate_ex; \
		PALLOCATE_FUNCTION allocate; \
	}; \
	union { \
		PFREE_FUNCTION_EX free_ex; \
		PFREE_FUNCTION free; \
	}; \
	LIST_ENTRY list_entry; \
	ULONG last_total_allocates; \
	union { \
		ULONG last_allocate_misses; \
		ULONG last_allocate_hits; \
	}; \
	ULONG future[2]

struct [[gnu::aligned(64)]] GENERAL_LOOKASIDE {
	GENERAL_LOOKASIDE_FIELDS;
};

struct GENERAL_LOOKASIDE_POOL {
	GENERAL_LOOKASIDE_FIELDS;
};

struct LOOKASIDE_LIST_EX {
	GENERAL_LOOKASIDE_POOL l;
};

struct [[gnu::aligned(64)]] PAGED_LOOKASIDE_LIST {
	GENERAL_LOOKASIDE l;
};

struct [[gnu::aligned(64)]] NPAGED_LOOKASIDE_LIST {
	GENERAL_LOOKASIDE l;
};

#define POOL_QUOTA_FAIL_INSTEAD_OF_RAISE 8
#define POOL_RAISE_IF_ALLOCATION_FAILURE 16

#define POOL_NX_ALLOCATION 512

NTAPI extern "C" void ExInitializePagedLookasideList(
	PAGED_LOOKASIDE_LIST* list,
	PALLOCATE_FUNCTION allocate,
	PFREE_FUNCTION free,
	ULONG flags,
	SIZE_T size,
	ULONG tag,
	USHORT depth);
NTAPI extern "C" void ExDeletePagedLookasideList(PAGED_LOOKASIDE_LIST* list);

NTAPI extern "C" void ExInitializeNPagedLookasideList(
	NPAGED_LOOKASIDE_LIST* list,
	PALLOCATE_FUNCTION allocate,
	PFREE_FUNCTION free,
	ULONG flags,
	SIZE_T size,
	ULONG tag,
	USHORT depth);
NTAPI extern "C" void ExDeleteNPagedLookasideList(NPAGED_LOOKASIDE_LIST* list);
