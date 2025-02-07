#include "ntdef.h"
#include "utils/export.hpp"
#include "stdio.hpp"
#include "acpi.hpp"
#include "cstring.hpp"
#include <hz/algorithm.hpp>

enum class SYSTEM_INFORMATION_CLASS {
	FirmwareTableInformation = 76
};

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

extern "C" EXPORT NTSTATUS ZwQuerySystemInformation(
	SYSTEM_INFORMATION_CLASS clazz,
	PVOID info,
	ULONG info_len,
	PULONG ret_len) {
	if (clazz == SYSTEM_INFORMATION_CLASS::FirmwareTableInformation) {
		auto* ptr = static_cast<SYSTEM_FIRMWARE_TABLE_INFORMATION*>(info);

		if (ptr->ProviderSignature == 'ACPI') {
			char sig[5] {};
			memcpy(sig, &ptr->TableID, 4);
			auto table = static_cast<acpi::SdtHeader*>(acpi::get_table(sig));
			if (!table) {
				return STATUS_INVALID_PARAMETER;
			}

			usize to_copy = hz::min<u32>(info_len - sizeof(SYSTEM_FIRMWARE_TABLE_INFORMATION), table->length);
			memcpy(ptr->TableBuffer, table, to_copy);
			ptr->TableBufferLength = table->length;

			if (ret_len) {
				*ret_len = sizeof(SYSTEM_FIRMWARE_TABLE_INFORMATION) + to_copy;
			}

			if (to_copy == table->length) {
				return STATUS_SUCCESS;
			}
			else {
				return STATUS_BUFFER_TOO_SMALL;
			}
		}
		else {
			panic("ZwQuerySysteminformation: unsupported provider signature ", Fmt::Hex, ptr->ProviderSignature, Fmt::Reset);
		}
	}
	else {
		return STATUS_INVALID_PARAMETER;
	}
}
