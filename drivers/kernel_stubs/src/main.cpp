#define NTKERNELAPI __declspec(dllexport)
#include "wdm.h"
#include <stdarg.h>

NTKERNELAPI ULONG DbgPrintEx(ULONG ComponentId, ULONG Level, PCSTR Format, ...) {
	return 0;
}

NTKERNELAPI PVOID ExAllocatePool2(POOL_FLAGS Flags, SIZE_T NumberOfBytes, ULONG Tag) {
	return nullptr;
}

NTKERNELAPI void ExFreePool(PVOID P) {}

NTKERNELAPI void ObfReferenceObject(PVOID Object) {}

NTKERNELAPI void ObfDereferenceObject(PVOID Object) {}

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

NTKERNELAPI PIRP IoAllocateIrp(CCHAR StackSize, BOOLEAN ChargeQuota) {
	return nullptr;
}

NTKERNELAPI void IoFreeIrp(PIRP Irp) {}

NTKERNELAPI void IoInitializeIrp(PIRP Irp, USHORT PacketSize, CCHAR StackSize) {}

NTKERNELAPI NTSTATUS IofCallDriver(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
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

extern "C" NTKERNELAPI int snprintf(char* buffer, size_t buffer_size, const char* fmt, ...) {
	return 0;
}

extern "C" NTKERNELAPI int swprintf(wchar_t* __buffer, size_t __buffer_size, const wchar_t* __fmt, ...) {
	return 0;
}

extern "C" NTKERNELAPI int vswprintf(wchar_t* __buffer, size_t __buffer_size, const wchar_t* __fmt, va_list __ap) {
	return 0;
}

NTKERNELAPI void KeInitializeEvent(PKEVENT Event, EVENT_TYPE Type, BOOLEAN State) {}

NTKERNELAPI LONG KeSetEvent(PKEVENT Event, KPRIORITY Increment, BOOLEAN Wait) {
	return 0;
}

NTKERNELAPI void KeClearEvent(PKEVENT Event) {}

NTKERNELAPI LONG KeResetEvent(PKEVENT Event) {
	return 0;
}
