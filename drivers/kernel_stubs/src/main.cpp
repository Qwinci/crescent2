#define NTKERNELAPI __declspec(dllexport)
#include "wdm.h"

NTKERNELAPI ULONG DbgPrintEx(ULONG ComponentId, ULONG Level, PCSTR Format, ...) {
	return 0;
}

NTKERNELAPI PVOID ExAllocatePool2(POOL_FLAGS Flags, SIZE_T NumberOfBytes, ULONG Tag) {
	return nullptr;
}

NTKERNELAPI void ExFreePool(PVOID P) {}

NTKERNELAPI PVOID MmMapIoSpace(
	PHYSICAL_ADDRESS PhysicalAddress,
	SIZE_T NumberOfBytes,
	MEMORY_CACHING_TYPE CacheType) {
	return nullptr;
}

NTKERNELAPI NTSTATUS IoCreateDevice(
	PDRIVER_OBJECT DriverObject,
	ULONG DeviceExtensionSize,
	PUNICODE_STRING DeviceName,
	DEVICE_TYPE DeviceType,
	ULONG DeviceCharacteristics,
	BOOLEAN Exclusive,
	PDEVICE_OBJECT* DeviceObject) {
	return 0;
}

NTKERNELAPI void IofCompleteRequest(PIRP Irp, CCHAR PriorityBoost) {}

NTKERNELAPI NTSTATUS ZwQuerySystemInformation(
	SYSTEM_INFORMATION_CLASS SystemInformationClass,
	PVOID SystemInformation,
	ULONG SystemInformationLength,
	PULONG ReturnLength) {
	return 0;
}

#undef memcpy
#undef memset

extern "C" NTKERNELAPI void* memcpy(void* __restrict dest, const void* __restrict src, size_t size) {
	return nullptr;
}

extern "C" NTKERNELAPI void* memset(void* __restrict dest, int ch, size_t size) {
	return nullptr;
}
