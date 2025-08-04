#include "paged_list.hpp"
#include "mem/malloc.hpp"
#include "assert.hpp"

NTAPI void ExInitializePagedLookasideList(
	PAGED_LOOKASIDE_LIST* list,
	PALLOCATE_FUNCTION allocate,
	PFREE_FUNCTION free,
	ULONG flags,
	SIZE_T size,
	ULONG tag,
	USHORT depth) {
	if (!allocate) {
		allocate = ExAllocatePoolWithTag;
	}
	if (!free) {
		free = ExFreePool;
	}

	list->l = {};
	list->l.maximum_depth = 64;
	list->l.allocate = allocate;
	list->l.free = free;
	// todo paged pool
	list->l.type = NonPagedPoolNx;
	list->l.tag = tag;
	assert(size >= sizeof(SLIST_ENTRY));
	list->l.size = size;
}

NTAPI void ExDeletePagedLookasideList(PAGED_LOOKASIDE_LIST* list) {
	auto* entry = ExpInterlockedFlushSList(&list->l.list_head);
	SLIST_ENTRY* next;
	for (; entry; entry = next) {
		list->l.free(entry);
		next = entry->next;
	}
}

NTAPI void ExInitializeNPagedLookasideList(
	NPAGED_LOOKASIDE_LIST* list,
	PALLOCATE_FUNCTION allocate,
	PFREE_FUNCTION free,
	ULONG flags,
	SIZE_T size,
	ULONG tag,
	USHORT depth) {
	if (!allocate) {
		allocate = ExAllocatePoolWithTag;
	}
	if (!free) {
		free = ExFreePool;
	}

	list->l = {};
	list->l.maximum_depth = 64;
	list->l.allocate = allocate;
	list->l.free = free;
	if (flags & POOL_NX_ALLOCATION) {
		list->l.type = NonPagedPoolNx;
	}
	else {
		list->l.type = NonPagedPool;
	}
	list->l.tag = tag;
	assert(size >= sizeof(SLIST_ENTRY));
	list->l.size = size;
}

NTAPI void ExDeleteNPagedLookasideList(NPAGED_LOOKASIDE_LIST* list) {
	auto* entry = ExpInterlockedFlushSList(&list->l.list_head);
	SLIST_ENTRY* next;
	for (; entry; entry = next) {
		list->l.free(entry);
		next = entry->next;
	}
}
