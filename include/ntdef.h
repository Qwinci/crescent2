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
typedef unsigned long ULONG, *PULONG;
typedef unsigned long long ULONGLONG, *PULONGLONG;

typedef char CCHAR;
typedef short CSHORT;
typedef ULONG CLONG;

typedef UCHAR BOOLEAN;

typedef void* PVOID;
typedef CHAR* PCHAR;
typedef UCHAR* PUCHAR;

#ifdef __cplusplus
typedef char16_t WCHAR;
#else
typedef unsigned short WCHAR;
#endif

typedef CHAR* PSTR;
typedef const CHAR* PCSTR;
typedef WCHAR* PWSTR;
typedef const WCHAR* PCWSTR;

typedef PVOID HANDLE;
typedef HANDLE* PHANDLE;

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
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef LARGE_INTEGER PHYSICAL_ADDRESS, *PPHYSICAL_ADDRESS;

typedef struct _LIST_ENTRY {
	struct _LIST_ENTRY* Flink;
	struct _LIST_ENTRY* Blink;
} LIST_ENTRY, *PLIST_ENTRY;

typedef struct _SINGLE_LIST_ENTRY {
	struct _SINGLE_LIST_ENTRY* Next;
} SINGLE_LIST_ENTRY, *PSINGLE_LIST_ENTRY;

typedef enum _WAIT_TYPE {
	WaitAll,
	WaitAny,
	WaitNotification,
	WaitDequeue
} WAIT_TYPE;

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

FORCEINLINE void InsertHeadList(PLIST_ENTRY ListHead, PLIST_ENTRY Entry) {
	PLIST_ENTRY old = ListHead->Flink;
	ListHead->Flink = Entry;
	Entry->Flink = old;
	Entry->Blink = ListHead;
	old->Blink = Entry;
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
#define STATUS_USER_APC (NTSTATUS) 0xC0
#define STATUS_TIMEOUT (NTSTATUS) 0x102
#define STATUS_PENDING (NTSTATUS) 0x103
#define STATUS_REPARSE (NTSTATUS) 0x104
#define STATUS_REPARSE_OBJECT (NTSTATUS) 0x118
#define STATUS_OBJECT_NAME_EXISTS (NTSTATUS) 0x40000000
#define STATUS_DATATYPE_MISALIGNMENT (NTSTATUS) 0x80000002
#define STATUS_BUFFER_OVERFLOW (NTSTATUS) 0x80000005
#define STATUS_NO_MORE_ENTRIES (NTSTATUS) 0x8000001A
#define STATUS_UNWIND_CONSOLIDATE (NTSTATUS) 0x80000029
#define STATUS_UNSUCCESSFUL (NTSTATUS) 0xC0000001
#define STATUS_NOT_IMPLEMENTED (NTSTATUS) 0xC0000002
#define STATUS_ACCESS_VIOLATION (NTSTATUS) 0xC0000005
#define STATUS_INVALID_HANDLE (NTSTATUS) 0xC0000008
#define STATUS_INVALID_PARAMETER (NTSTATUS) 0xC000000D
#define STATUS_INVALID_DEVICE_REQUEST (NTSTATUS) 0xC0000010
#define STATUS_MORE_PROCESSING_REQUIRED (NTSTATUS) 0xC0000016
#define STATUS_NO_MEMORY (NTSTATUS) 0xC0000017
#define STATUS_ACCESS_DENIED (NTSTATUS) 0xC0000022
#define STATUS_BUFFER_TOO_SMALL (NTSTATUS) 0xC0000023
#define STATUS_OBJECT_TYPE_MISMATCH (NTSTATUS) 0xC0000024
#define STATUS_NONCONTINUABLE_EXCEPTION (NTSTATUS) 0xC0000025
#define STATUS_INVALID_DISPOSITION (NTSTATUS) 0xC0000026
#define STATUS_UNWIND (NTSTATUS) 0xC0000027
#define STATUS_OBJECT_NAME_INVALID (NTSTATUS) 0xC0000033
#define STATUS_OBJECT_NAME_NOT_FOUND (NTSTATUS) 0xC0000034
#define STATUS_OBJECT_NAME_COLLISION (NTSTATUS) 0xC0000035
#define STATUS_OBJECT_PATH_INVALID (NTSTATUS) 0xC0000039
#define STATUS_OBJECT_PATH_NOT_FOUND (NTSTATUS) 0xC000003A
#define STATUS_OBJECT_PATH_SYNTAX_BAD (NTSTATUS) 0xC000003B
#define STATUS_INSUFFICIENT_RESOURCES (NTSTATUS) 0xC000009A
#define STATUS_INVALID_PARAMETER_1 (NTSTATUS) 0xC00000EF
#define STATUS_NOT_FOUND (NTSTATUS) 0xC0000225
#define STATUS_REPARSE_POINT_ENCOUNTERED (NTSTATUS) 0xC000050B
#define STATUS_ALREADY_REGISTERED (NTSTATUS) 0xC0000718

typedef struct _UNICODE_STRING {
	USHORT Length;
	USHORT MaximumLength;
	PWSTR Buffer;
} UNICODE_STRING, *PUNICODE_STRING;
typedef const UNICODE_STRING* PCUNICODE_STRING;

typedef struct _ANSI_STRING {
	USHORT Length;
	USHORT MaximumLength;
	PSTR Buffer;
} ANSI_STRING, *PANSI_STRING;
typedef const ANSI_STRING* PCANSI_STRING;

typedef struct _ANSI_STRING STRING, *PSTRING;

#ifdef __cplusplus
}
#endif

#endif
