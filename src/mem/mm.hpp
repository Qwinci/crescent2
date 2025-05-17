#pragma once

#include "ntdef.h"
#include "sched/misc.hpp"

enum MEMORY_CACHING_TYPE {
	MmNonCached = 0,
	MmCached = 1,
	MmWriteCombined = 2,
	MmHardwareCoherentCached,
	MmNonCachedUnordered,
	MmUSWCCached,
	MmNotMapped = -1
};

// flags for MmAllocatePagesForMdlEx
#define MM_DONT_ZERO_ALLOCATION 1
#define MM_ALLOCATE_FROM_LOCAL_NODE_ONLY 2
#define MM_ALLOCATE_FULLY_REQUIRED 4
#define MM_ALLOCATE_NO_WAIT 8
#define MM_ALLOCATE_PREFER_CONTIGUOUS 0x10
#define MM_ALLOCATE_REQUIRE_CONTIGUOUS 0x20
#define MM_ALLOCATE_FAST_LARGE_PAGES 0x40
#define MM_ALLOCATE_AND_HOT_REMOVE 0x100

using PFN_NUMBER = ULONG;

struct Process;

#define MDL_MAPPED_TO_SYSTEM_VA 1
#define MDL_PAGES_LOCKED 2
#define MDL_SOURCE_IS_NONPAGED_POOL 4
#define MDL_ALLOCATED_FIXED_SIZE 8
#define MDL_PARTIAL 0x10
#define MDL_PARTIAL_HAS_BEEN_MAPPED 0x20
#define MDL_IO_PAGE_READ 0x40
#define MDL_WRITE_OPERATION 0x80
#define MDL_LOCKED_PAGE_TABLES 0x100
#define MDL_PARENT_MAPPED_SYSTEM_VA MDL_LOCKED_PAGE_TABLES
#define MDL_FREE_EXTRA_PTES 0x200
#define MDL_DESCRIBES_AWE 0x400
#define MDL_IO_SPACE 0x800
#define MDL_NETWORK_HEADER 0x1000
#define MDL_MAPPING_CAN_FAIL 0x2000
#define MDL_PAGE_CONTENTS_INVARIANT 0x4000
#define MDL_ALLOCATED_MUST_SUCCEED MDL_PAGE_CONTENTS_INVARIANT
#define MDL_INTERNAL 0x8000

#define MDL_MAPPING_FLAGS (\
	MDL_MAPPED_TO_SYSTEM_VA | \
	MDL_PAGES_LOCKED | \
	MDL_SOURCE_IS_NONPAGED_POOL \
	MDL_PARTIAL_HAS_BEEN_MAPPED \
	MDL_PARENT_MAPPED_SYSTEM_VA \
	MDL_IO_SPACE)

// pages = (PFN_NUMBER*) (mdl + 1)
struct MDL {
	MDL* next;
	CSHORT size;
	CSHORT mdl_flags;
	Process* process;
	PVOID mapped_system_va;
	// in context of subject thread virt addr of buffer = start_va | byte_offset
	PVOID start_va;
	ULONG byte_count;
	ULONG byte_offset;
};

#define MdlMappingWithGuardPtes 0x20000000
#define MdlMappingNoExecute 0x40000000
#define MdlMappingNoWrite 0x80000000

struct MM_PHYSICAL_ADDRESS_LIST {
	PHYSICAL_ADDRESS phys_addr;
	SIZE_T num_of_bytes;
};

NTAPI extern "C" PVOID MmMapIoSpace(
	PHYSICAL_ADDRESS physical_addr,
	SIZE_T num_of_bytes,
	MEMORY_CACHING_TYPE cache_type);

NTAPI extern "C" void MmUnmapIoSpace(PVOID base_addr, SIZE_T num_of_bytes);

NTAPI extern "C" PVOID MmMapLockedPagesSpecifyCache(
	MDL* mdl,
	KPROCESSOR_MODE access_mode,
	MEMORY_CACHING_TYPE cache_type,
	PVOID requested_addr,
	ULONG bug_check_on_failure,
	ULONG priority);
NTAPI extern "C" void MmUnmapLockedPages(PVOID base_addr, MDL* mdl);
NTAPI extern "C" MDL* MmAllocatePagesForMdl(
	PHYSICAL_ADDRESS low_addr,
	PHYSICAL_ADDRESS high_addr,
	PHYSICAL_ADDRESS skip_bytes,
	SIZE_T total_bytes);
NTAPI extern "C" MDL* MmAllocatePagesForMdlEx(
	PHYSICAL_ADDRESS low_addr,
	PHYSICAL_ADDRESS high_addr,
	PHYSICAL_ADDRESS skip_bytes,
	SIZE_T total_bytes,
	MEMORY_CACHING_TYPE cache_type,
	ULONG flags);
NTAPI extern "C" void MmFreePagesFromMdl(MDL* mdl);

NTAPI extern "C" NTSTATUS MmAllocateMdlForIoSpace(
	MM_PHYSICAL_ADDRESS_LIST* phys_addr_list,
	SIZE_T num_of_entries,
	MDL** new_mdl);

NTAPI extern "C" PVOID MmAllocateContiguousMemorySpecifyCache(
	SIZE_T num_of_bytes,
	PHYSICAL_ADDRESS lowest_addr,
	PHYSICAL_ADDRESS highest_addr,
	PHYSICAL_ADDRESS boundary_addr_multiple,
	MEMORY_CACHING_TYPE cache_type);
NTAPI extern "C" void MmFreeContiguousMemory(PVOID base_addr);

NTAPI extern "C" PHYSICAL_ADDRESS MmGetPhysicalAddress(PVOID base_addr);
