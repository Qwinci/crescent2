#include "device.hpp"
#include "utils/export.hpp"

EXPORT NTSTATUS IoCreateDevice(
	DRIVER_OBJECT* driver,
	ULONG device_ext_size,
	PUNICODE_STRING device_name,
	DEVICE_TYPE type,
	ULONG characteristics,
	BOOLEAN exclusive,
	DEVICE_OBJECT* device) {
	println("IoCreateDevice");
	return STATUS_SUCCESS;
}

EXPORT void IofCompleteRequest(IRP* irp, CCHAR priority_boost) {
	println("IofCompleteRequest");
}
