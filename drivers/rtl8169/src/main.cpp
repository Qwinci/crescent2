#include <wdm.h>

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry_path) {
	DbgPrintEx(DPFLTR_NDIS_ID, DPFLTR_INFO_LEVEL, "rtl8169.sys: initializing\n");
	return STATUS_SUCCESS;
}
