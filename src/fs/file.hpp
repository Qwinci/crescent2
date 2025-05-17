#pragma once

#include "ntdef.h"
#include "sched/event.hpp"
#include "utils/spinlock.hpp"
#include "object.hpp"

struct DEVICE_OBJECT;
struct VPB;
struct SECTION_OBJECT_POINTERS;
struct IO_COMPLETION_CONTEXT;
struct IO_STATUS_BLOCK;

struct FILE_OBJECT {
	CSHORT type;
	CSHORT size;
	DEVICE_OBJECT* device;
	VPB* vpb;
	PVOID fs_ctx1;
	PVOID fs_ctx2;
	SECTION_OBJECT_POINTERS* section_object_ptr;
	PVOID private_cache_map;
	NTSTATUS final_status;
	FILE_OBJECT* related_object;
	BOOLEAN lock_operation;
	BOOLEAN delete_pending;
	BOOLEAN read_access;
	BOOLEAN write_access;
	BOOLEAN delete_access;
	BOOLEAN shared_read;
	BOOLEAN shared_write;
	BOOLEAN shared_delete;
	ULONG flags;
	UNICODE_STRING file_name;
	i64 current_byte_offset;
	volatile ULONG waiters;
	volatile ULONG busy;
	PVOID last_lock;
	KEVENT lock;
	KEVENT event;
	volatile IO_COMPLETION_CONTEXT* completion_ctx;
	KSPIN_LOCK irp_list_lock;
	LIST_ENTRY irp_list;
	volatile struct _IOP_FILE_OBJECT_EXTENSION* file_obj_extension;
};

// share access
#define FILE_SHARE_READ 1
#define FILE_SHARE_WRITE 2
#define FILE_SHARE_DELETE 4

// create disposition
#define FILE_SUPERSEDE 0
#define FILE_OPEN 1
#define FILE_CREATE 2
#define FILE_OPEN_IF 3
#define FILE_OVERWRITE 4
#define FILE_OVERWRITE_IF 5

// open/create flags
#define FILE_SYNCHRONOUS_IO_ALERT 0x10
#define FILE_SYNCHRONOUS_IO_NONALERT 0x20
#define FILE_NON_DIRECTORY_FILE 0x40

// byte offset parameters
#define FILE_WRITE_TO_END_OF_FILE 0xFFFFFFFF
#define FILE_USE_FILE_POINTER_POSITION 0xFFFFFFFE

struct FileParseCtx {
	IO_STATUS_BLOCK* status_block;
	LARGE_INTEGER allocation_size;
	ULONG file_attribs;
	ULONG share_access;
	ULONG create_disposition;
	ULONG create_options;
	PVOID ea_buffer;
	ULONG ea_length;
};

#define IRP_SYNCHRONOUS_API 4
#define IRP_CREATE_OPERATION 0x80
#define IRP_DEFER_IO_COMPLETION 0x800

using PIO_APC_ROUTINE = void (*)(void* apc_context, IO_STATUS_BLOCK* io_status_block, u32 reserved);

NTAPI extern "C" NTSTATUS NtCreateFile(
	PHANDLE handle,
	ACCESS_MASK desired_access,
	OBJECT_ATTRIBUTES* attribs,
	IO_STATUS_BLOCK* io_status_block,
	PLARGE_INTEGER allocation_size,
	ULONG file_attribs,
	ULONG share_access,
	ULONG create_disposition,
	ULONG create_options,
	PVOID ea_buffer,
	ULONG ea_length);
NTAPI extern "C" NTSTATUS ZwCreateFile(
	PHANDLE handle,
	ACCESS_MASK desired_access,
	OBJECT_ATTRIBUTES* attribs,
	IO_STATUS_BLOCK* io_status_block,
	PLARGE_INTEGER allocation_size,
	ULONG file_attribs,
	ULONG share_access,
	ULONG create_disposition,
	ULONG create_options,
	PVOID ea_buffer,
	ULONG ea_length);
NTAPI extern "C" NTSTATUS NtOpenFile(
	PHANDLE handle,
	ACCESS_MASK desired_access,
	OBJECT_ATTRIBUTES* attribs,
	IO_STATUS_BLOCK* io_status_block,
	ULONG share_access,
	ULONG open_options);
NTAPI extern "C" NTSTATUS ZwOpenFile(
	PHANDLE handle,
	ACCESS_MASK desired_access,
	OBJECT_ATTRIBUTES* attribs,
	IO_STATUS_BLOCK* io_status_block,
	ULONG share_access,
	ULONG open_options);

NTAPI extern "C" NTSTATUS NtReadFile(
	HANDLE file_handle,
	HANDLE event,
	PIO_APC_ROUTINE apc_routine,
	PVOID apc_ctx,
	IO_STATUS_BLOCK* io_status_block,
	PVOID buffer,
	ULONG length,
	PLARGE_INTEGER byte_offset,
	PULONG key);
NTAPI extern "C" NTSTATUS ZwReadFile(
	HANDLE file_handle,
	HANDLE event,
	PIO_APC_ROUTINE apc_routine,
	PVOID apc_ctx,
	IO_STATUS_BLOCK* io_status_block,
	PVOID buffer,
	ULONG length,
	PLARGE_INTEGER byte_offset,
	PULONG key);

NTAPI extern "C" NTSTATUS NtWriteFile(
	HANDLE file_handle,
	HANDLE event,
	PIO_APC_ROUTINE apc_routine,
	PVOID apc_ctx,
	IO_STATUS_BLOCK* io_status_block,
	PVOID buffer,
	ULONG length,
	PLARGE_INTEGER byte_offset,
	PULONG key);
NTAPI extern "C" NTSTATUS ZwWriteFile(
	HANDLE file_handle,
	HANDLE event,
	PIO_APC_ROUTINE apc_routine,
	PVOID apc_ctx,
	IO_STATUS_BLOCK* io_status_block,
	PVOID buffer,
	ULONG length,
	PLARGE_INTEGER byte_offset,
	PULONG key);

NTAPI extern "C" NTSTATUS NtClose(HANDLE handle);
NTAPI extern "C" NTSTATUS ZwClose(HANDLE handle);
