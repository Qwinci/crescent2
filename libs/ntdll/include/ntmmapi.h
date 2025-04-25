#ifndef _NTMEM_H
#define _NTMEM_H

#include "ntapi.h"
#include "ntdef.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PAGE_NOACCESS 1
#define PAGE_READONLY 2
#define PAGE_READWRITE 4
#define PAGE_WRITECOPY 8
#define PAGE_EXECUTE 0x10
#define PAGE_EXECUTE_READ 0x20
#define PAGE_EXECUTE_READWRITE 0x40
#define PAGE_EXECUTE_WRITECOPY 0x80

#define MEM_COMMIT 0x1000
#define MEM_RESERVE 0x2000

#define MEM_DECOMMIT 0x4000
#define MEM_RELEASE 0x8000

NTAPI NTSTATUS NtAllocateVirtualMemory(
	HANDLE ProcessHandle,
	PVOID* BaseAddress,
	ULONG_PTR ZeroBits,
	PSIZE_T RegionSize,
	ULONG AllocationType,
	ULONG PageProtection);
NTAPI NTSTATUS NtFreeVirtualMemory(
	HANDLE ProcessHandle,
	PVOID* BaseAddress,
	PSIZE_T RegionSize,
	ULONG FreeType);
NTAPI NTSTATUS NtProtectVirtualMemory(
	HANDLE ProcessHandle,
	PVOID* BaseAddress,
	PSIZE_T RegionSize,
	ULONG NewProtection,
	PULONG OldProtection);

#ifdef __cplusplus
}
#endif

#endif
