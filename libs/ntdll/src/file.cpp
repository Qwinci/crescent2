#include "winternl.h"
#include "ntioapi.h"
#include "syscall.hpp"
#include "syscalls.hpp"

NTAPI NTSTATUS NtOpenFile(
	PHANDLE file_handle,
	ACCESS_MASK desired_access,
	POBJECT_ATTRIBUTES object_attributes,
	PIO_STATUS_BLOCK io_status_block,
	ULONG share_access,
	ULONG open_options) {
	DO_SYSCALL(SYS_OPEN_FILE);
}

NTAPI NTSTATUS NtClose(HANDLE handle) {
	DO_SYSCALL(SYS_CLOSE);
}

NTAPI NTSTATUS NtReadFile(
	HANDLE file_handle,
	HANDLE event,
	PIO_APC_ROUTINE apc_routine,
	PVOID apc_context,
	PIO_STATUS_BLOCK io_status_block,
	PVOID buffer,
	ULONG length,
	PLARGE_INTEGER byte_offset,
	PULONG key) {
	DO_SYSCALL(SYS_READ_FILE);
}
