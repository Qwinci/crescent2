#pragma once

#include "ntdef.h"

struct RTL_BITMAP {
	ULONG size_of_bit_map;
	PULONG buffer;
};

NTAPI extern "C" void RtlInitAnsiString(PANSI_STRING dest, PCSTR src);
NTAPI extern "C" void RtlInitUnicodeString(PUNICODE_STRING dest, PCWSTR src);
NTAPI extern "C" void RtlInitString(PSTRING dest, PCSTR src);

NTAPI extern "C" void RtlCopyUnicodeString(
	PUNICODE_STRING dest,
	PCUNICODE_STRING src);
NTAPI extern "C" NTSTATUS RtlAppendUnicodeStringToString(
	PUNICODE_STRING dest,
	PCUNICODE_STRING src);
NTAPI extern "C" NTSTATUS RtlAppendUnicodeToString(
	PUNICODE_STRING dest,
	PCWSTR src);
NTAPI extern "C" NTSTATUS RtlAnsiStringToUnicodeString(
	PUNICODE_STRING dest,
	PCANSI_STRING src,
	BOOLEAN allocate_dest);
NTAPI extern "C" NTSTATUS RtlUnicodeStringToAnsiString(
	PANSI_STRING dest,
	PCUNICODE_STRING src,
	BOOLEAN allocate_dest);
NTAPI extern "C" void RtlFreeUnicodeString(PUNICODE_STRING str);
NTAPI extern "C" void RtlFreeAnsiString(PANSI_STRING str);
NTAPI extern "C" NTSTATUS RtlIntegerToUnicodeString(
	ULONG value,
	ULONG base,
	PUNICODE_STRING string);
NTAPI extern "C" NTSTATUS RtlInt64ToUnicodeString(
	ULONGLONG value,
	ULONG base,
	PUNICODE_STRING string);
NTAPI extern "C" NTSTATUS RtlUnicodeStringToInteger(
	PCUNICODE_STRING str,
	ULONG base,
	PULONG value);

NTAPI extern "C" ULONG RtlxUnicodeStringToAnsiSize(PCUNICODE_STRING unicode_str);
NTAPI extern "C" BOOLEAN RtlEqualUnicodeString(
	PCUNICODE_STRING a,
	PCUNICODE_STRING b,
	BOOLEAN case_insensitive);
NTAPI extern "C" LONG RtlCompareUnicodeString(
	PCUNICODE_STRING a,
	PCUNICODE_STRING b,
	BOOLEAN case_insensitive);
NTAPI extern "C" BOOLEAN RtlEqualString(
	const STRING* a,
	const STRING* b,
	BOOLEAN case_insensitive);
NTAPI extern "C" LONG RtlCompareString(
	const STRING* a,
	const STRING* b,
	BOOLEAN case_insensitive);
NTAPI extern "C" BOOLEAN RtlPrefixUnicodeString(
	PCUNICODE_STRING a,
	PCUNICODE_STRING b,
	BOOLEAN case_insensitive);
NTAPI extern "C" WCHAR RtlUpcaseUnicodeChar(WCHAR ch);
NTAPI extern "C" WCHAR RtlDowncaseUnicodeChar(WCHAR ch);

NTAPI extern "C" void RtlInitializeBitMap(
	RTL_BITMAP* bitmap,
	PULONG bitmap_buffer,
	ULONG size_of_bitmap);
NTAPI extern "C" void RtlClearAllBits(RTL_BITMAP* bitmap);
NTAPI extern "C" void RtlClearBits(RTL_BITMAP* bitmap, ULONG start_index, ULONG number_to_clear);
NTAPI extern "C" void RtlClearBit(RTL_BITMAP* bitmap, ULONG bit_number);
NTAPI extern "C" void RtlSetBits(RTL_BITMAP* bitmap, ULONG start_index, ULONG number_to_set);
NTAPI extern "C" void RtlSetBit(RTL_BITMAP* bitmap, ULONG bit_number);
NTAPI extern "C" ULONG RtlFindClearBitsAndSet(RTL_BITMAP* bitmap, ULONG number_to_find, ULONG hint_index);
NTAPI extern "C" ULONG RtlFindNextForwardRunClear(RTL_BITMAP* bitmap, ULONG from_index, PULONG starting_run_index);
NTAPI extern "C" BOOLEAN RtlAreBitsSet(RTL_BITMAP* bitmap, ULONG start_index, ULONG length);
NTAPI extern "C" BOOLEAN RtlAreBitsClear(RTL_BITMAP* bitmap, ULONG start_index, ULONG length);
NTAPI extern "C" ULONG RtlNumberOfClearBits(RTL_BITMAP* bitmap);
NTAPI extern "C" ULONG RtlNumberOfSetBits(RTL_BITMAP* bitmap);

NTAPI extern "C" SIZE_T RtlCompareMemory(const void* src1, const void* src2, SIZE_T length);

template<typename T> struct _RTL_CONSTANT_STRING_remove_const;
template<>
struct _RTL_CONSTANT_STRING_remove_const<const char&> {
	using type = char;
};
template<>
struct _RTL_CONSTANT_STRING_remove_const<const WCHAR&> {
	using type = WCHAR;
};

#define RTL_CONSTANT_STRING(s) { \
	sizeof(s) - sizeof(*(s)), \
	sizeof(s), \
	const_cast<_RTL_CONSTANT_STRING_remove_const<decltype(s[0])>::type*>(s) }

struct TIME_FIELDS {
	CSHORT year;
	CSHORT month;
	CSHORT day;
	CSHORT hour;
	CSHORT minute;
	CSHORT second;
	CSHORT milliseconds;
	CSHORT weekday;
};

NTAPI extern "C" void RtlTimeToTimeFields(
	PLARGE_INTEGER time,
	TIME_FIELDS* time_fields);

#define RTL_REGISTRY_ABSOLUTE 0
#define RTL_REGISTRY_SERVICES 1
#define RTL_REGISTRY_CONTROL 2
#define RTL_REGISTRY_WINDOWS_NT 3
#define RTL_REGISTRY_DEVICEMAP 4
#define RTL_REGISTRY_USER 5
#define RTL_REGISTRY_HANDLE 0x40000000
#define RTL_REGISTRY_OPTIONAL 0x80000000

using PRTL_QUERY_REGISTRY_ROUTINE = NTSTATUS (*)(
	PWSTR value_name,
	ULONG value_type,
	PVOID value_data,
	ULONG value_length,
	PVOID ctx,
	PVOID entry_ctx);

struct RTL_QUERY_TABLE {
	PRTL_QUERY_REGISTRY_ROUTINE query_routine;
	ULONG flags;
	PWSTR name;
	PVOID entry_ctx;
	ULONG default_type;
	PVOID default_data;
	ULONG default_length;
};

#define RTL_QUERY_REGISTRY_SUBKEY 1
#define RTL_QUERY_REGISTRY_TOPKEY 2
#define RTL_QUERY_REGISTRY_REQUIRED 4
#define RTL_QUERY_REGISTRY_NOVALUE 8
#define RTL_QUERY_REGISTRY_NOEXPAND 0x10
#define RTL_QUERY_REGISTRY_DIRECT 0x20
#define RTL_QUERY_REGISTRY_DELETE 0x40
#define RTL_QUERY_REGISTRY_NOSTRING 0x80
#define RTL_QUERY_REGISTRY_TYPECHECK 0x100

NTAPI extern "C" NTSTATUS RtlCheckRegistryKey(
	ULONG relative_to,
	PWSTR path);
NTAPI extern "C" NTSTATUS RtlCreateRegistryKey(
	ULONG relative_to,
	PWSTR path);
NTAPI extern "C" NTSTATUS RtlWriteRegistryValue(
	ULONG relative_to,
	PCWSTR path,
	PCWSTR value_name,
	ULONG value_type,
	PVOID value_data,
	ULONG value_length);

NTAPI extern "C" NTSTATUS RtlQueryRegistryValues(
	ULONG relative_to,
	PCWSTR path,
	RTL_QUERY_TABLE* query_table,
	PVOID ctx,
	PVOID environment);
