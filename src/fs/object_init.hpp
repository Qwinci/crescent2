#pragma once

#include "mem/pool.hpp"

// standard access types

// allows deleting the object
#define DELETE 0x10000
// allows reading the info from the object's security descriptor (not system access control list (SACL))
#define READ_CONTROL 0x20000
// allows modifying the discretionary access control list (DACL) of the object's security descriptor
#define WRITE_DAC 0x40000
// allows changing the owner of the object's security descriptor
#define WRITE_OWNER 0x80000
// allows waiting for the object to became signaled
#define SYNCHRONIZE 0x100000

// DELETE | READ_CONTROL | WRITE_DAC | WRITE_OWNER
#define STANDARD_RIGHTS_REQUIRED 0xF0000

#define STANDARD_RIGHTS_READ (READ_CONTROL)
#define STANDARD_RIGHTS_WRITE (READ_CONTROL)
#define STANDARD_RIGHTS_EXECUTE (READ_CONTROL)

#define STANDARD_RIGHTS_ALL 0x1F0000

// AccessSystemAcl
#define ACCESS_SYSTEM_SECURITY 0x1000000

// MaximumAllowed access type
#define MAXIMUM_ALLOWED 0x2000000

// object directory access rights
#define DIRECTORY_QUERY 1
#define DIRECTORY_TRAVERSE 2
#define DIRECTORY_CREATE_OBJECT 4
#define DIRECTORY_CREATE_SUBDIRECTORY 8
#define DIRECTORY_ALL_ACCESS (STANDARD_RIGHTS_REQUIRED | 0xF)

#define GENERIC_ALL 0x10000000
#define GENERIC_EXECUTE 0x20000000
#define GENERIC_WRITE 0x40000000
#define GENERIC_READ 0x80000000

struct ACCESS_STATE;
struct OBJECT_TYPE;
using PACCESS_STATE = ACCESS_STATE*;
using POBJECT_TYPE = OBJECT_TYPE*;

using ACCESS_MASK = ULONG;

struct GENERIC_MAPPING {
	ACCESS_MASK generic_read;
	ACCESS_MASK generic_write;
	ACCESS_MASK generic_execute;
	ACCESS_MASK generic_all;
};

struct OBJECT_DUMP_CONTROL {
	PVOID stream;
	ULONG detail;
};

enum OB_OPEN_REASON {
	ObCreateHandle,
	ObOpenHandle,
	ObDuplicateHandle,
	ObInheritHandle
};

struct Process;

struct SECURITY_QUALITY_OF_SERVICE;

struct OBJECT_NAME_INFORMATION {
	UNICODE_STRING name;
	WCHAR buffer[];
};

struct OBJECT_TYPE_INITIALIZER {
	USHORT length;
	union {
		UCHAR object_type_flags;
		struct {
			UCHAR case_insensitive : 1;
			UCHAR unnamed_objects_only : 1;
			UCHAR use_default_object : 1;
			UCHAR security_required : 1;
			UCHAR maintain_handle_count : 1;
			UCHAR maintain_type_list : 1;
			UCHAR supports_object_callbacks : 1;
			UCHAR cache_aligned : 1;
		};
	};
	ULONG object_type_code;
	ULONG invalid_attribs;
	GENERIC_MAPPING generic_mapping;
	ULONG valid_access_mask;
	ULONG retain_access;
	POOL_TYPE pool_type;
	ULONG default_paged_pool_charge;
	ULONG default_non_paged_pool_charge;
	void (*dump_proc)(PVOID object, OBJECT_DUMP_CONTROL* control);
	NTSTATUS (*open_proc)(
		OB_OPEN_REASON open_reason,
		KPROCESSOR_MODE access_mode,
		Process* process,
		PVOID object,
		ACCESS_MASK* granted_access,
		ULONG handle_count);
	void (*close_proc)(
		Process* process,
		PVOID object,
		ULONG_PTR process_handle_count,
		ULONG_PTR system_handle_count);
	void (*delete_proc)(PVOID object);
	NTSTATUS (*parse_proc)(
		PVOID parse_object,
		PVOID object_type,
		PACCESS_STATE access_state,
		KPROCESSOR_MODE access_mode,
		ULONG attribs,
		PUNICODE_STRING complete_name,
		PUNICODE_STRING remaining_name,
		PVOID context,
		SECURITY_QUALITY_OF_SERVICE* security_qos,
		PVOID* object);
	PVOID security_proc;
	NTSTATUS (*query_name_proc)(
		PVOID object,
		BOOLEAN has_object_name,
		OBJECT_NAME_INFORMATION* object_name_info,
		ULONG length,
		PULONG return_length,
		KPROCESSOR_MODE mode);
	BOOLEAN (*okay_to_close_proc)(
		Process* process,
		PVOID object,
		HANDLE handle,
		KPROCESSOR_MODE previous_mode);
	ULONG wait_object_flag_mask;
	USHORT wait_object_flag_offset;
	USHORT wait_object_pointer_offset;
};
