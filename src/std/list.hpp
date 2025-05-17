#pragma once

#include "ntdef.h"
#include "utils/spinlock.hpp"

NTAPI extern "C" PLIST_ENTRY ExInterlockedInsertHeadList(
	PLIST_ENTRY list_head,
	PLIST_ENTRY list_entry,
	KSPIN_LOCK* lock);
NTAPI extern "C" PLIST_ENTRY ExInterlockedInsertTailList(
	PLIST_ENTRY list_head,
	PLIST_ENTRY list_entry,
	KSPIN_LOCK* lock);
NTAPI extern "C" PLIST_ENTRY ExInterlockedRemoveHeadList(
	PLIST_ENTRY list_head,
	KSPIN_LOCK* lock);

struct [[gnu::aligned(16)]] SLIST_ENTRY {
	SLIST_ENTRY* next;
};

struct [[gnu::aligned(16)]] SLIST_HEADER {
	struct Header {
		union {
			struct {
				ULONGLONG depth : 16;
				ULONGLONG sequence : 48;
				SLIST_ENTRY* next;
			};
		};
	} header;
};

NTAPI extern "C" SLIST_ENTRY* ExpInterlockedPushEntrySList(SLIST_HEADER* list_head, SLIST_ENTRY* list_entry);
NTAPI extern "C" SLIST_ENTRY* ExpInterlockedPopEntrySList(SLIST_HEADER* list_head);
NTAPI extern "C" USHORT ExQueryDepthSList(SLIST_HEADER* list_head);
NTAPI extern "C" SLIST_ENTRY* ExpInterlockedFlushSList(SLIST_HEADER* list_head);
