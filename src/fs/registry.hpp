#pragma once

#include "object.hpp"

#define REG_OPTION_NON_VOLATILE 0
#define REG_OPTION_VOLATILE 1
#define REG_OPTION_CREATE_LINK 2
#define REG_OPTION_BACKUP_RESTORE 4
#define REG_OPTION_OPEN_LINK 8
#define REG_OPTION_DONT_VIRTUALIZE 0x10

#define REG_CREATED_NEW_KEY 1
#define REG_OPENED_EXISTING_KEY 2

// registry access rights
#define KEY_QUERY_VALUE 1
#define KEY_SET_VALUE 2
#define KEY_CREATE_SUB_KEY 4
#define KEY_ENUMERATE_SUB_KEYS 8
#define KEY_NOTIFY 0x10
#define KEY_CREATE_LINK 0x20
#define KEY_WOW64_64KEY 0x100
#define KEY_WOW64_32KEY 0x200
#define KEY_WOW64_RES 0x300

// no value type
#define REG_NONE 0
// null terminated unicode string
#define REG_SZ 1
// null terminated unicode string with environment variable references
#define REG_EXPAND_SZ 2
// binary data
#define REG_BINARY 3
// 32-bit number
#define REG_DWORD 4
// symbolic link (unicode)
#define REG_LINK 6
// multiple null terminated unicode strings, ends with two nulls
#define REG_MULTI_SZ 7
// resource list in resource map
#define REG_RESOURCE_LIST 8
// resource list in hardware description
#define REG_FULL_RESOURCE_LIST 9
#define REG_RESOURCE_REQUIREMENTS_LIST 10
// 64-bit number
#define REG_QWORD 11

struct KEY_BASIC_INFORMATION {
	LARGE_INTEGER last_write_time;
	ULONG title_index;
	ULONG name_length;
	WCHAR name[];
};

struct KEY_NODE_INFORMATION {
	LARGE_INTEGER last_write_time;
	ULONG title_index;
	ULONG class_offset;
	ULONG class_length;
	ULONG name_length;
	WCHAR name[];
};

struct KEY_FULL_INFORMATION {
	LARGE_INTEGER last_write_time;
	ULONG title_index;
	ULONG class_offset;
	ULONG class_length;
	ULONG subkeys;
	ULONG max_name_len;
	ULONG max_class_len;
	ULONG values;
	ULONG max_value_name_len;
	ULONG max_value_data_len;
	WCHAR clazz[];
};

struct KEY_NAME_INFORMATION {
	ULONG name_length;
	WCHAR name[];
};

struct KEY_CACHED_INFORMATION {
	LARGE_INTEGER last_write_time;
	ULONG title_index;
	ULONG subkeys;
	ULONG max_name_len;
	ULONG values;
	ULONG max_value_name_len;
	ULONG max_value_data_len;
	ULONG name_length;
};

struct KEY_VIRTUALIZATION_INFORMATION {
	ULONG virtualization_candidate : 1;
	ULONG virtualization_enabled : 1;
	ULONG virtual_target : 1;
	ULONG virtual_store : 1;
	ULONG virtual_source : 1;
	ULONG reserved : 27;
};

enum KEY_INFORMATION_CLASS {
	KeyBasicInformation,
	KeyNodeInformation,
	KeyFullInformation,
	KeyNameInformation,
	KeyCachedInformation,
	KeyFlagsInformation,
	KeyVirtualizationInformation,
	KeyHandleTagsInformation,
	KeyTrustInformation,
	KeyLayerInformation
};

struct KEY_VALUE_BASIC_INFORMATION {
	ULONG title_index;
	ULONG type;
	ULONG name_length;
	WCHAR name[];
};

struct KEY_VALUE_FULL_INFORMATION {
	ULONG title_index;
	ULONG type;
	ULONG data_offset;
	ULONG data_length;
	ULONG name_length;
	WCHAR name[];
};

struct KEY_VALUE_PARTIAL_INFORMATION {
	ULONG title_index;
	ULONG type;
	ULONG data_length;
	UCHAR data[];
};

struct KEY_VALUE_PARTIAL_INFORMATION_ALIGN64 {
	ULONG type;
	ULONG data_length;
	UCHAR data[];
};

struct KEY_VALUE_LAYER_INFORMATION {
	ULONG is_tombstone : 1;
	ULONG reserved : 31;
};

enum KEY_VALUE_INFORMATION_CLASS {
	KeyValueBasicInformation,
	KeyValueFullInformation,
	KeyValuePartialInformation,
	KeyValueFullInformationAlign64,
	KeyValuePartialInformationAlign64,
	KeyValueLayerInformation
};

NTAPI extern "C" NTSTATUS NtCreateKey(
	PHANDLE key_handle,
	ACCESS_MASK desired_access,
	OBJECT_ATTRIBUTES* object_attribs,
	ULONG title_index,
	PUNICODE_STRING clazz,
	ULONG create_options,
	PULONG disposition);
NTAPI extern "C" NTSTATUS ZwCreateKey(
	PHANDLE key_handle,
	ACCESS_MASK desired_access,
	OBJECT_ATTRIBUTES* object_attribs,
	ULONG title_index,
	PUNICODE_STRING clazz,
	ULONG create_options,
	PULONG disposition);

NTAPI extern "C" NTSTATUS NtOpenKey(
	PHANDLE key_handle,
	ACCESS_MASK desired_access,
	OBJECT_ATTRIBUTES* object_attribs);
NTAPI extern "C" NTSTATUS ZwOpenKey(
	PHANDLE key_handle,
	ACCESS_MASK desired_access,
	OBJECT_ATTRIBUTES* object_attribs);

NTAPI extern "C" NTSTATUS NtEnumerateKey(
	HANDLE key_handle,
	ULONG index,
	KEY_INFORMATION_CLASS key_info_class,
	PVOID key_info,
	ULONG length,
	PULONG result_length);
NTAPI extern "C" NTSTATUS ZwEnumerateKey(
	HANDLE key_handle,
	ULONG index,
	KEY_INFORMATION_CLASS key_info_class,
	PVOID key_info,
	ULONG length,
	PULONG result_length);

NTAPI extern "C" NTSTATUS NtSetValueKey(
	HANDLE key_handle,
	PUNICODE_STRING value_name,
	ULONG title_index,
	ULONG type,
	PVOID data,
	ULONG data_size);
NTAPI extern "C" NTSTATUS ZwSetValueKey(
	HANDLE key_handle,
	PUNICODE_STRING value_name,
	ULONG title_index,
	ULONG type,
	PVOID data,
	ULONG data_size);

NTAPI extern "C" NTSTATUS NtDeleteValueKey(
	HANDLE key_handle,
	PUNICODE_STRING value_name);
NTAPI extern "C" NTSTATUS ZwDeleteValueKey(
	HANDLE key_handle,
	PUNICODE_STRING value_name);

NTAPI extern "C" NTSTATUS NtQueryValueKey(
	HANDLE key_handle,
	PUNICODE_STRING value_name,
	KEY_VALUE_INFORMATION_CLASS key_value_info_class,
	PVOID key_value_info,
	ULONG length,
	PULONG result_length);
NTAPI extern "C" NTSTATUS ZwQueryValueKey(
	HANDLE key_handle,
	PUNICODE_STRING value_name,
	KEY_VALUE_INFORMATION_CLASS key_value_info_class,
	PVOID key_value_info,
	ULONG length,
	PULONG result_length);

void registry_init();
