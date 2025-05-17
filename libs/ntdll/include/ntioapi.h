#ifndef _NTIOAPI_H
#define _NTIOAPI_H

#include "ntapi.h"
#include "ntdef.h"
#include "winternl.h"
#include "winnt.h"

#ifdef __cplusplus
extern "C" {
#endif

NTAPI NTSTATUS NtReadFile(
	HANDLE FileHandle,
	HANDLE Event,
	PIO_APC_ROUTINE ApcRoutine,
	PVOID ApcContext,
	PIO_STATUS_BLOCK IoStatusBlock,
	PVOID Buffer,
	ULONG Length,
	PLARGE_INTEGER ByteOffset,
	PULONG Key);

#ifdef __cplusplus
}
#endif

#endif
