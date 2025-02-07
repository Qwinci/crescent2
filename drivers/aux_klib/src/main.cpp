#include "wdm.h"
#include "aux_klib.h"

NTSTATUS AuxKlibInitialize() {
	return STATUS_SUCCESS;
}

enum SYSTEM_FIRMWARE_TABLE_ACTION {
	SystemFirmwareTable_Enumerate = 0,
	SystemFirmwareTable_Get = 1
};

struct SYSTEM_FIRMWARE_TABLE_INFORMATION {
	ULONG ProviderSignature;
	SYSTEM_FIRMWARE_TABLE_ACTION Action;
	ULONG TableID;
	ULONG TableBufferLength;
	CHAR TableBuffer[];
};

NTSTATUS AuxKlibGetSystemFirmwareTable(
	ULONG FirmwareTableProviderSignature,
	ULONG FirmwareTableID,
	PVOID FirmwareTableBuffer,
	ULONG BufferLength,
	PULONG ReturnLength) {
	auto* info = static_cast<SYSTEM_FIRMWARE_TABLE_INFORMATION*>(ExAllocatePool2(
		0,
		sizeof(SYSTEM_FIRMWARE_TABLE_INFORMATION) + BufferLength,
		0));
	if (!info) {
		return STATUS_NO_MEMORY;
	}
	info->ProviderSignature = FirmwareTableProviderSignature;
	info->Action = SystemFirmwareTable_Enumerate;
	info->TableID = FirmwareTableID;
	info->TableBufferLength = BufferLength;

	ULONG ret_len = 0;
	auto status = ZwQuerySystemInformation(
		SystemFirmwareTableInformation,
		info,
		sizeof(SYSTEM_FIRMWARE_TABLE_INFORMATION) + BufferLength,
		&ret_len);
	if (!NT_SUCCESS(status) && status != STATUS_BUFFER_TOO_SMALL) {
		ExFreePool(info);
		return status;
	}

	// TableBufferLength contains the size of the full table
	// ret_len contains the actual number of bytes returned
	if (ReturnLength) {
		*ReturnLength = info->TableBufferLength;
	}

	if (FirmwareTableBuffer) {
		memcpy(FirmwareTableBuffer, info->TableBuffer, ret_len - sizeof(SYSTEM_FIRMWARE_TABLE_INFORMATION));
	}

	ExFreePool(info);

	return status;
}
