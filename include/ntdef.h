#ifndef _NTDEF_H
#define _NTDEF_H

#include "basetsd.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef char CHAR;
typedef short SHORT;
typedef int INT;
typedef int LONG;
typedef long long LONGLONG;

typedef unsigned char UCHAR;
typedef unsigned short USHORT;
typedef unsigned int UINT;
typedef unsigned int ULONG, *PULONG;
typedef unsigned long long ULONGLONG;

typedef char CCHAR;
typedef short CSHORT;
typedef ULONG CLONG;

typedef UCHAR BOOLEAN;

typedef void* PVOID;
typedef CHAR* PCHAR;

#ifdef __cplusplus
typedef char16_t WCHAR;
#else
typedef unsigned short WCHAR;
#endif

typedef CHAR* PSTR;
typedef const CHAR* PCSTR;
typedef WCHAR* PWSTR;
typedef const WCHAR* PCWSTR;

typedef union _LARGE_INTEGER {
	struct {
		ULONG LowPart;
		LONG HighPart;
	};
	struct {
		ULONG LowPart;
		LONG HighPart;
	} u;
	LONGLONG QuadPart;
} LARGE_INTEGER;

typedef LARGE_INTEGER PHYSICAL_ADDRESS, *PPHYSICAL_ADDRESS;

typedef struct _LIST_ENTRY {
	struct _LIST_ENTRY* Flink;
	struct _LIST_ENTRY* Blink;
} LIST_ENTRY, *PLIST_ENTRY;

typedef struct _SINGLE_LIST_ENTRY {
	struct _SINGLE_LIST_ENTRY* Next;
} SINGLE_LIST_ENTRY, *PSINGLE_LIST_ENTRY;

#define FORCEINLINE __forceinline

FORCEINLINE void InitializeListHead(LIST_ENTRY* ListHead) {
	ListHead->Flink = ListHead;
	ListHead->Blink = ListHead;
}

FORCEINLINE BOOLEAN IsListEmpty(const LIST_ENTRY* ListHead) {
	return ListHead->Flink == ListHead;
}

FORCEINLINE PLIST_ENTRY RemoveHeadList(PLIST_ENTRY ListHead) {
	PLIST_ENTRY entry = ListHead->Flink;
	ListHead->Flink = entry->Flink;
	entry->Flink->Blink = ListHead;
	return entry;
}

FORCEINLINE BOOLEAN RemoveEntryList(PLIST_ENTRY Entry) {
	PLIST_ENTRY prev = Entry->Blink;
	prev->Flink = Entry->Flink;
	Entry->Flink->Blink = prev;
	return Entry->Flink == prev;
}

FORCEINLINE void InsertTailList(PLIST_ENTRY ListHead, PLIST_ENTRY Entry) {
	PLIST_ENTRY old = ListHead->Blink;
	ListHead->Blink = Entry;
	Entry->Blink = old;
	Entry->Flink = ListHead;
	old->Flink = Entry;
}

typedef LONG NTSTATUS;

#define NT_SUCCESS(Status) (((NTSTATUS) (Status)) >= 0)
#define NT_INFORMATION(Status) (((ULONG) (Status) >> 30) == 1)
#define NT_WARNING(Status) (((ULONG) (Status) >> 30) == 2)
#define NT_ERROR(Status) (((ULONG) (Status) >> 30) == 3)

#define STATUS_SUCCESS (NTSTATUS) 0
#define STATUS_TIMEOUT (NTSTATUS) 0x102
#define STATUS_NOT_IMPLEMENTED (NTSTATUS) 0xC0000002
#define STATUS_INVALID_PARAMETER (NTSTATUS) 0xC000000D
#define STATUS_NO_MEMORY (NTSTATUS) 0xC0000017
#define STATUS_BUFFER_TOO_SMALL (NTSTATUS) 0xC0000023
#define STATUS_NOT_FOUND (NTSTATUS) 0xC0000225

typedef struct _UNICODE_STRING {
	USHORT Length;
	USHORT MaximumLength;
	PWSTR Buffer;
} UNICODE_STRING, *PUNICODE_STRING, *const PCUNICODE_STRING;

#ifdef __cplusplus
}
#endif

#endif
