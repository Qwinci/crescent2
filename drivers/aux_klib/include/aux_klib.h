#ifndef _AUX_KLIB_H
#define _AUX_KLIB_H

#include "ntdef.h"

NTSTATUS AuxKlibInitialize(void);
NTSTATUS AuxKlibGetSystemFirmwareTable(
	ULONG FirmwareTableProviderSignature,
	ULONG FirmwareTableID,
	PVOID FirmwareTableBuffer,
	ULONG BufferLength,
	PULONG ReturnLength);

#endif
