#include "ntmmapi.h"
#include "syscall.hpp"
#include "syscalls.hpp"

NTAPI NTSTATUS NtAllocateVirtualMemory(
	HANDLE process_handle,
	PVOID* base_address,
	ULONG_PTR zero_bits,
	PSIZE_T region_size,
	ULONG allocation_type,
	ULONG page_protection) {
	DO_SYSCALL(SYS_ALLOCATE_VIRTUAL_MEMORY);
}

NTAPI NTSTATUS NtFreeVirtualMemory(
	HANDLE process_handle,
	PVOID* base_address,
	PSIZE_T region_size,
	ULONG free_type) {
	DO_SYSCALL(SYS_FREE_VIRTUAL_MEMORY);
}

NTAPI NTSTATUS NtProtectVirtualMemory(
	HANDLE process_handle,
	PVOID* base_address,
	PSIZE_T region_size,
	ULONG new_protection,
	PULONG old_protection) {
	DO_SYSCALL(SYS_PROTECT_VIRTUAL_MEMORY);
}
