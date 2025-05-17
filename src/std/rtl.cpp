#include "rtl.hpp"
#include "cstring.hpp"
#include "mem/malloc.hpp"
#include "dev/clock.hpp"
#include "wchar.hpp"
#include "string_view.hpp"
#include "fs/registry.hpp"
#include "fs/file.hpp"
#include <hz/algorithm.hpp>

NTAPI void RtlInitAnsiString(PANSI_STRING dest, PCSTR src) {
	if (!src) {
		dest->Length = 0;
		dest->MaximumLength = 0;
		return;
	}

	auto len = strlen(src);
	dest->Length = len;
	dest->MaximumLength = len + 1;
	dest->Buffer = const_cast<PSTR>(src);
}

NTAPI void RtlInitUnicodeString(PUNICODE_STRING dest, PCWSTR src) {
	if (!src) {
		dest->Length = 0;
		dest->MaximumLength = 0;
		return;
	}

	auto len = wcslen(src);
	dest->Length = len * 2;
	dest->MaximumLength = (len + 1) * 2;
	dest->Buffer = const_cast<PWSTR>(src);
}

NTAPI void RtlInitString(PSTRING dest, PCSTR src) {
	if (!src) {
		dest->Length = 0;
		dest->MaximumLength = 0;
		return;
	}

	auto len = strlen(src);
	dest->Length = len;
	dest->MaximumLength = len + 1;
	dest->Buffer = const_cast<PSTR>(src);
}

NTAPI void RtlCopyUnicodeString(
	PUNICODE_STRING dest,
	PCUNICODE_STRING src) {
	if (src) {
		auto to_copy = hz::min(src->Length, dest->MaximumLength);
		memcpy(dest->Buffer, src->Buffer, to_copy);
		if (src->Length + 2 <= dest->MaximumLength) {
			dest->Buffer[src->Length / 2] = 0;
		}

		dest->Length = to_copy;
	}
	else{
		dest->Length = 0;
	}
}

NTAPI NTSTATUS RtlAppendUnicodeStringToString(
	PUNICODE_STRING dest,
	PCUNICODE_STRING src) {
	if (dest->MaximumLength < dest->Length + src->Length) {
		return STATUS_BUFFER_TOO_SMALL;
	}

	memcpy(dest->Buffer + dest->Length, src->Buffer, src->Length);
	dest->Length += src->Length;
	if (dest->Length + 2 <= dest->MaximumLength) {
		dest->Buffer[dest->Length / 2] = 0;
	}
	return STATUS_SUCCESS;
}

NTAPI extern "C" NTSTATUS RtlAppendUnicodeToString(
	PUNICODE_STRING dest,
	PCWSTR src) {
	auto len = wcslen(src);
	if (dest->MaximumLength < dest->Length + len * 2) {
		return STATUS_BUFFER_TOO_SMALL;
	}

	memcpy(dest->Buffer + dest->Length, src, len * 2);
	if (dest->Length + 2 <= dest->MaximumLength) {
		dest->Buffer[dest->Length / 2] = 0;
	}
	return STATUS_SUCCESS;
}

NTAPI NTSTATUS RtlAnsiStringToUnicodeString(
	PUNICODE_STRING dest,
	PCANSI_STRING src,
	BOOLEAN allocate_dest) {
	auto len = src->Length;

	if (!allocate_dest) {
		if (dest->MaximumLength < (len + 1) * 2) {
			return STATUS_BUFFER_OVERFLOW;
		}
	}
	else {
		dest->Buffer = static_cast<PWSTR>(kmalloc((len + 1) * 2));
		if (!dest->Buffer) {
			return STATUS_NO_MEMORY;
		}
		dest->MaximumLength = (len + 1) * 2;
	}

	for (usize i = 0; i < len; ++i) {
		dest->Buffer[i] = src->Buffer[i];
	}

	dest->Buffer[len] = 0;
	dest->Length = len * 2;

	return STATUS_SUCCESS;
}

NTAPI NTSTATUS RtlUnicodeStringToAnsiString(
	PANSI_STRING dest,
	PCUNICODE_STRING src,
	BOOLEAN allocate_dest) {
	usize len = src->Length / 2;

	if (!allocate_dest) {
		if (dest->MaximumLength < len + 1) {
			return STATUS_BUFFER_OVERFLOW;
		}
	}
	else {
		dest->Buffer = static_cast<PSTR>(kmalloc(len + 1));
		if (!dest->Buffer) {
			return STATUS_NO_MEMORY;
		}
		dest->MaximumLength = len + 1;
	}

	for (usize i = 0; i < len; ++i) {
		// todo
		dest->Buffer[i] = static_cast<char>(src->Buffer[i]);
	}

	dest->Buffer[len] = 0;
	dest->Length = len;

	return STATUS_SUCCESS;
}

NTAPI void RtlFreeUnicodeString(PUNICODE_STRING str) {
	kfree(str->Buffer, str->MaximumLength);
}

NTAPI void RtlFreeAnsiString(PANSI_STRING str) {
	kfree(str->Buffer, str->MaximumLength);
}

static constexpr char CHARS[] = "0123456789ABCDEF";

NTAPI NTSTATUS RtlIntegerToUnicodeString(
	ULONG value,
	ULONG base,
	PUNICODE_STRING string) {
	if (base != 0 && base != 2 && base != 8 && base != 10 && base != 16) {
		return STATUS_INVALID_PARAMETER;
	}

	WCHAR buffer[64];
	WCHAR* ptr = buffer + 64;
	do {
		*--ptr = CHARS[value % base];
		value /= base;
	} while (value);

	usize count = buffer + 64 - ptr;
	if (string->MaximumLength < (count + 1) * 2) {
		return STATUS_BUFFER_OVERFLOW;
	}

	memcpy(string->Buffer, ptr, count * 2);
	string->Buffer[count] = 0;
	string->Length = count * 2;
	return STATUS_SUCCESS;
}

NTAPI NTSTATUS RtlInt64ToUnicodeString(
	ULONGLONG value,
	ULONG base,
	PUNICODE_STRING string) {
	if (base != 0 && base != 2 && base != 8 && base != 10 && base != 16) {
		return STATUS_INVALID_PARAMETER;
	}

	WCHAR buffer[64];
	WCHAR* ptr = buffer + 64;
	do {
		*--ptr = CHARS[value % base];
		value /= base;
	} while (value);

	usize count = buffer + 64 - ptr;
	if (string->MaximumLength < count * 2) {
		return STATUS_BUFFER_OVERFLOW;
	}

	memcpy(string->Buffer, ptr, count * 2);

	string->Length = count * 2;

	if (count * 2 + 2 <= string->MaximumLength) {
		string->Buffer[count] = 0;
	}

	return STATUS_SUCCESS;
}

NTAPI NTSTATUS RtlUnicodeStringToInteger(
	PCUNICODE_STRING str,
	ULONG base,
	PULONG value) {
	if (!str || str->Length <= 1 || !str->Buffer || !*str->Buffer) {
		return STATUS_INVALID_PARAMETER;
	}

	usize len = str->Length / 2;
	kstd::wstring_view view {str->Buffer, len};

	auto to_upper = [](WCHAR c) {
		if (c >= u'a' && c <= 'z') {
			return static_cast<WCHAR>(c & ~(1 << 5));
		}
		else {
			return c;
		}
	};

	if (base == 0) {
		if (view.starts_with(u"0x") || view.starts_with(u"0X")) {
			view = view.substr(2);
			base = 16;
		}
		else if (view.starts_with(u"0o") || view.starts_with(u"0O")) {
			view = view.substr(2);
			base = 8;
		}
		else if (view.starts_with(u"0b") || view.starts_with(u"0B")) {
			view = view.substr(2);
			base = 2;
		}
		else {
			base = 10;
		}
	}

	if (view.size() == 0) {
		*value = 0;
		return STATUS_SUCCESS;
	}

	usize start = 0;
	for (; start < view.size(); ++start) {
		if (view[start] != ' ' && view[start] != '\r' &&
			view[start] != '\n' && view[start] != '\t' &&
			view[start] != '\f') {
			break;
		}
	}

	if (start == view.size()) {
		*value = 0;
		return STATUS_SUCCESS;
	}

	bool negative = false;
	if (view[start] == u'+') {
		++start;
	}
	else if (view[start] == u'-') {
		++start;
		negative = true;
	}

	ULONG res = 0;

	if (base <= 10) {
		for (; start < view.size(); ++start) {
			auto c = view[start];
			if (c < u'0' || c > CHARS[base - 1]) {
				break;
			}
			res *= base;
			res += c - u'0';
		}
	}
	else {
		for (; start < view.size(); ++start) {
			auto c = view[start];
			if (c >= u'0' && c <= u'9') {
				res *= base;
				res += c - u'0';
			}
			else {
				c = to_upper(c);
				if (c >= u'A' && c <= u'F') {
					res *= base;
					res += c - u'A' + 10;
				}
				else {
					break;
				}
			}
		}
	}

	if (negative) {
		res = -res;
	}

	*value = res;
	return STATUS_SUCCESS;
}

NTAPI ULONG RtlxUnicodeStringToAnsiSize(PCUNICODE_STRING unicode_str) {
	// todo properly support unicode
	return unicode_str->Length / 2 + 1;
}

NTAPI BOOLEAN RtlEqualUnicodeString(
	PCUNICODE_STRING a,
	PCUNICODE_STRING b,
	BOOLEAN case_insensitive) {
	auto to_upper = [](WCHAR c) {
		if (c >= u'a' && c <= u'z') {
			return static_cast<WCHAR>(c & ~(1 << 5));
		}
		else {
			return c;
		}
	};

	if (a->Length != b->Length) {
		return false;
	}

	if (case_insensitive) {
		for (usize i = 0; i < a->Length / 2; ++i) {
			if (to_upper(a->Buffer[i]) != to_upper(b->Buffer[i])) {
				return false;
			}
		}
	}
	else {
		for (usize i = 0; i < a->Length / 2; ++i) {
			if (a->Buffer[i] != b->Buffer[i]) {
				return false;
			}
		}
	}

	return true;
}

NTAPI extern "C" LONG RtlCompareUnicodeString(
	PCUNICODE_STRING a,
	PCUNICODE_STRING b,
	BOOLEAN case_insensitive) {
	auto to_upper = [](WCHAR c) {
		if (c >= u'a' && c <= u'z') {
			return static_cast<WCHAR>(c & ~(1 << 5));
		}
		else {
			return c;
		}
	};

	if (a->Length < b->Length) {
		return -1;
	}
	else if (a->Length > b->Length) {
		return 1;
	}

	if (case_insensitive) {
		for (usize i = 0; i < a->Length / 2; ++i) {
			int res = to_upper(a->Buffer[i]) - to_upper(b->Buffer[i]);
			if (res != 0) {
				return res;
			}
		}
	}
	else {
		for (usize i = 0; i < a->Length / 2; ++i) {
			int res = a->Buffer[i] - b->Buffer[i];
			if (res != 0) {
				return res;
			}
		}
	}

	return 0;
}

NTAPI BOOLEAN RtlEqualString(
	const STRING* a,
	const STRING* b,
	BOOLEAN case_insensitive) {
	auto to_upper = [](char c) {
		if (c >= 'a' && c <= 'z') {
			return static_cast<char>(c & ~(1 << 5));
		}
		else {
			return c;
		}
	};

	if (a->Length != b->Length) {
		return false;
	}

	if (case_insensitive) {
		for (usize i = 0; i < a->Length; ++i) {
			if (to_upper(a->Buffer[i]) != to_upper(b->Buffer[i])) {
				return false;
			}
		}
	}
	else {
		for (usize i = 0; i < a->Length; ++i) {
			if (a->Buffer[i] != b->Buffer[i]) {
				return false;
			}
		}
	}

	return true;
}

NTAPI LONG RtlCompareString(
	const STRING* a,
	const STRING* b,
	BOOLEAN case_insensitive) {
	auto to_upper = [](char c) {
		if (c >= 'a' && c <= 'z') {
			return static_cast<char>(c & ~(1 << 5));
		}
		else {
			return c;
		}
	};

	if (a->Length < b->Length) {
		return -1;
	}
	else if (a->Length > b->Length) {
		return 1;
	}

	if (case_insensitive) {
		for (usize i = 0; i < a->Length; ++i) {
			int res = to_upper(a->Buffer[i]) - to_upper(b->Buffer[i]);
			if (res != 0) {
				return res;
			}
		}
	}
	else {
		for (usize i = 0; i < a->Length; ++i) {
			int res = a->Buffer[i] - b->Buffer[i];
			if (res != 0) {
				return res;
			}
		}
	}

	return 0;
}

NTAPI extern "C" BOOLEAN RtlPrefixUnicodeString(
	PCUNICODE_STRING a,
	PCUNICODE_STRING b,
	BOOLEAN case_insensitive) {
	auto to_upper = [](WCHAR c) {
		if (c >= u'a' && c <= u'z') {
			return static_cast<WCHAR>(c & ~(1 << 5));
		}
		else {
			return c;
		}
	};

	if (b->Length < a->Length) {
		return false;
	}

	if (case_insensitive) {
		for (usize i = 0; i < a->Length / 2; ++i) {
			if (to_upper(a->Buffer[i]) != to_upper(b->Buffer[i])) {
				return false;
			}
		}
	}
	else {
		for (usize i = 0; i < a->Length / 2; ++i) {
			if (a->Buffer[i] != b->Buffer[i]) {
				return false;
			}
		}
	}

	return true;
}

NTAPI WCHAR RtlUpcaseUnicodeChar(WCHAR ch) {
	if (ch >= u'a' && ch <= u'z') {
		return ch & ~(1 << 5);
	}
	else {
		return ch;
	}
}

NTAPI WCHAR RtlDowncaseUnicodeChar(WCHAR ch) {
	if (ch >= u'A' && ch <= u'Z') {
		return ch | 1 << 5;
	}
	else {
		return ch;
	}
}

NTAPI void RtlInitializeBitMap(
	RTL_BITMAP* bitmap,
	PULONG bitmap_buffer,
	ULONG size_of_bitmap) {
	bitmap->size_of_bit_map = size_of_bitmap;
	bitmap->buffer = bitmap_buffer;
}

NTAPI void RtlClearAllBits(RTL_BITMAP* bitmap) {
	memset(bitmap->buffer, 0, (bitmap->size_of_bit_map + 7) / 8);
}

NTAPI void RtlClearBit(RTL_BITMAP* bitmap, ULONG bit_number) {
	bitmap->buffer[bit_number / 32] &= ~(1U << (bit_number % 32));
}

NTAPI void RtlClearBits(RTL_BITMAP* bitmap, ULONG start_index, ULONG number_to_clear) {
	auto* ptr = bitmap->buffer;

	auto start_bit_offset = start_index % 32;
	ULONG index = start_index / 32;
	if (start_bit_offset != 0) {
		if (number_to_clear <= 32 - start_bit_offset) {
			ptr[index] &= ~(((1U << number_to_clear) - 1) << start_bit_offset);
			return;
		}

		ptr[index++] &= ~(UINT32_MAX << start_bit_offset);
		number_to_clear -= 32 - start_bit_offset;
	}

	for (; number_to_clear >= 32; number_to_clear -= 32) {
		ptr[index++] = 0;
	}

	if (number_to_clear != 0) {
		ptr[index] &= ~(((1U << number_to_clear) - 1));
	}
}

NTAPI void RtlSetBits(RTL_BITMAP* bitmap, ULONG start_index, ULONG number_to_set) {
	auto* ptr = bitmap->buffer;

	auto start_bit_offset = start_index % 32;
	ULONG index = start_index / 32;
	if (start_bit_offset != 0) {
		if (number_to_set <= 32 - start_bit_offset) {
			ptr[index] |= ((1U << number_to_set) - 1) << start_bit_offset;
			return;
		}

		ptr[index++] |= UINT32_MAX << start_bit_offset;
		number_to_set -= 32 - start_bit_offset;
	}

	for (; number_to_set >= 32; number_to_set -= 32) {
		ptr[index++] = UINT32_MAX;
	}

	if (number_to_set != 0) {
		ptr[index] |= (1U << number_to_set) - 1;
	}
}

NTAPI ULONG RtlFindClearBitsAndSet(RTL_BITMAP* bitmap, ULONG number_to_find, ULONG hint_index) {
	auto* ptr = bitmap->buffer;

	// todo improve
	ULONG start = hint_index;
	while (true) {
		for (ULONG index = start; index < bitmap->size_of_bit_map;) {
			bool found = true;
			for (ULONG i = 0; i < number_to_find; ++i) {
				auto value = ptr[(index + i) / 32];
				auto off = (index + i) % 32;
				if (value & 1U << off) {
					found = false;
					index += i + 1;
					break;
				}
			}

			if (found) {
				for (ULONG i = 0; i < number_to_find; ++i) {
					ptr[(index + i) / 32] |= 1U << ((index + i) % 32);
				}
				return index;
			}
		}

		if (start == 0) {
			break;
		}

		start = 0;
	}

	return UINT32_MAX;
}

NTAPI ULONG RtlFindNextForwardRunClear(RTL_BITMAP* bitmap, ULONG from_index, PULONG starting_run_index) {
	auto* ptr = bitmap->buffer;

	for (ULONG index = from_index; index < bitmap->size_of_bit_map;) {
		ULONG count = 0;
		for (ULONG i = 0; i < bitmap->size_of_bit_map - index; ++i) {
			auto value = ptr[(index + i) / 32];
			auto off = (index + i) % 32;
			if (value & 1U << off) {
				count = i;
				index += i + 1;
				break;
			}
		}

		if (count != 0) {
			*starting_run_index = index;
			return count;
		}
	}

	return 0;
}

NTAPI BOOLEAN RtlAreBitsSet(RTL_BITMAP* bitmap, ULONG start_index, ULONG length) {
	if (!length || start_index >= bitmap->size_of_bit_map || start_index + length > bitmap->size_of_bit_map) {
		return false;
	}

	auto* ptr = bitmap->buffer;
	for (ULONG i = start_index; i < start_index + length; ++i) {
		auto value = ptr[i / 32];
		auto off = i % 32;
		if (!(value & 1U << off)) {
			return false;
		}
	}

	return true;
}

NTAPI BOOLEAN RtlAreBitsClear(RTL_BITMAP* bitmap, ULONG start_index, ULONG length) {
	if (!length || start_index >= bitmap->size_of_bit_map || start_index + length > bitmap->size_of_bit_map) {
		return false;
	}

	auto* ptr = bitmap->buffer;
	for (ULONG i = start_index; i < start_index + length; ++i) {
		auto value = ptr[i / 32];
		auto off = i % 32;
		if (value & 1U << off) {
			return false;
		}
	}

	return true;
}

NTAPI ULONG RtlNumberOfClearBits(RTL_BITMAP* bitmap) {
	ULONG count = 0;

	auto* ptr = bitmap->buffer;
	for (ULONG i = 0; i < bitmap->size_of_bit_map; ++i) {
		auto value = ptr[i / 32];
		auto off = i % 32;
		if (!(value & 1U << off)) {
			++count;
		}
	}

	return count;
}

NTAPI ULONG RtlNumberOfSetBits(RTL_BITMAP* bitmap) {
	ULONG count = 0;

	auto* ptr = bitmap->buffer;
	for (ULONG i = 0; i < bitmap->size_of_bit_map; ++i) {
		auto value = ptr[i / 32];
		auto off = i % 32;
		if (value & 1U << off) {
			++count;
		}
	}

	return count;
}

NTAPI void RtlSetBit(RTL_BITMAP* bitmap, ULONG bit_number) {
	bitmap->buffer[bit_number / 32] |= 1U << (bit_number % 32);
}

NTAPI SIZE_T RtlCompareMemory(const void* src1, const void* src2, SIZE_T length) {
	auto* ptr1 = static_cast<const unsigned char*>(src1);
	auto* ptr2 = static_cast<const unsigned char*>(src2);
	for (SIZE_T i = 0; i < length; ++i, ++ptr1, ++ptr2) {
		if (*ptr1 != *ptr2) {
			return i;
		}
	}

	return length;
}

namespace {
	constexpr bool is_leap(CSHORT year) {
		return year % 400 == 0 || (year % 4 == 0 && year % 100 != 0);
	}

	constexpr int days_per_year(CSHORT year) {
		return is_leap(year) ? 366 : 365;
	}

	constexpr int DAYS_IN_MONTHS_NOLEAP[] {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	constexpr int DAYS_IN_MONTHS_LEAP[] {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

	constexpr const int* get_days_per_month_table(CSHORT year) {
		return is_leap(year) ? DAYS_IN_MONTHS_LEAP : DAYS_IN_MONTHS_NOLEAP;
	}

	constexpr int EPOCH_YEAR = 1601;

	constexpr int TICKS_IN_MS = 10000;
}

NTAPI BOOLEAN RtlTimeFieldsToTime(TIME_FIELDS* time_fields, PLARGE_INTEGER time) {
	i64 value = 0;
	value += time_fields->milliseconds * static_cast<i64>(NS_IN_MS / 100);
	value += time_fields->second * static_cast<i64>(NS_IN_S / 100);
	value += time_fields->minute * static_cast<i64>(NS_IN_S / 100 * 60);
	value += time_fields->hour * static_cast<i64>(NS_IN_S / 100 * 60 * 60);
	value += (time_fields->day - 1) * static_cast<i64>(NS_IN_S / 100 * 60 * 60 * 24);

	auto table = get_days_per_month_table(time_fields->year);
	for (CSHORT i = 1; i < time_fields->month; ++i) {
		auto days = table[i];
		value += days * static_cast<i64>(NS_IN_S / 100 * 60 * 60 * 24);
	}

	assert(time_fields->year >= EPOCH_YEAR);
	for (CSHORT i = EPOCH_YEAR; i < time_fields->year; ++i) {
		auto days = days_per_year(i);
		value += days * static_cast<i64>(NS_IN_S / 100 * 60 * 60 * 24);
	}

	time->QuadPart = value;

	return true;
}

NTAPI void RtlTimeToTimeFields(
	PLARGE_INTEGER time,
	TIME_FIELDS* time_fields) {
	LONGLONG seconds = time->QuadPart / static_cast<LONGLONG>(NS_IN_S / 100);
	LONGLONG ticks_within_sec = time->QuadPart % static_cast<LONGLONG>(NS_IN_S / 100);
	time_fields->milliseconds = static_cast<CSHORT>(ticks_within_sec / TICKS_IN_MS);

	LONGLONG days_since_epoch = seconds / (60 * 60 * 24);

	time_fields->weekday = static_cast<CSHORT>((days_since_epoch + 1) % 7);

	CSHORT year = EPOCH_YEAR;
	while (true) {
		auto days = days_per_year(year);
		if (days_since_epoch >= days) {
			days_since_epoch -= days;
			++year;
		}
		else {
			break;
		}
	}

	CSHORT month = 1;
	auto table = get_days_per_month_table(year);
	for (CSHORT i = 0; i < 12; ++i) {
		auto days = table[i];
		if (days_since_epoch >= days) {
			days_since_epoch -= days;
			++month;
		}
		else {
			break;
		}
	}

	time_fields->year = year;
	time_fields->month = month;
	time_fields->day = static_cast<CSHORT>(days_since_epoch + 1);
	time_fields->hour = static_cast<CSHORT>(seconds / 60 / 60 % 24);
	time_fields->minute = static_cast<CSHORT>(seconds / 60 % 60);
	time_fields->second = static_cast<CSHORT>(seconds % 60);
}

static NTSTATUS open_reg_key(HANDLE* handle, ULONG relative_to, PCWSTR path, bool create) {
	relative_to &= ~RTL_REGISTRY_OPTIONAL;

	if (relative_to == RTL_REGISTRY_HANDLE) {
		*handle = reinterpret_cast<HANDLE>(const_cast<PWSTR>(path));
		return STATUS_SUCCESS;
	}

	UNICODE_STRING name {};
	switch (relative_to) {
		case RTL_REGISTRY_ABSOLUTE:
			break;
		case RTL_REGISTRY_CONTROL:
			name = RTL_CONSTANT_STRING(u"\\Registry\\Machine\\System\\CurrentControlSet\\Control");
			break;
		case RTL_REGISTRY_DEVICEMAP:
			name = RTL_CONSTANT_STRING(u"\\Registry\\Machine\\Hardware\\DeviceMap");
			break;
		case RTL_REGISTRY_SERVICES:
			name = RTL_CONSTANT_STRING(u"\\Registry\\Machine\\System\\CurrentControlSet\\Services");
			break;
		case RTL_REGISTRY_USER:
			// todo non-system process should use the current user
			name = RTL_CONSTANT_STRING(u"\\Registry\\User\\.Default");
			break;
		case RTL_REGISTRY_WINDOWS_NT:
			name = RTL_CONSTANT_STRING(u"\\Registry\\Machine\\Software\\Microsoft\\Windows NT\\CurrentVersion");
			break;
		default:
			break;
	}

	OBJECT_ATTRIBUTES attribs {};
	NTSTATUS status;
	HANDLE root = nullptr;
	if (name.Length) {
		InitializeObjectAttributes(
			&attribs,
			&name,
			OBJ_CASE_INSENSITIVE | OBJ_OPENIF,
			nullptr,
			nullptr);
		status = ZwOpenKey(&root, 0, &attribs);
		assert(NT_SUCCESS(status));
	}

	UNICODE_STRING path_str {};
	RtlInitUnicodeString(&path_str, path);
	InitializeObjectAttributes(
		&attribs,
		&path_str,
		OBJ_CASE_INSENSITIVE | OBJ_OPENIF,
		root,
		nullptr);

	if (create) {
		status = ZwCreateKey(
			handle,
			0,
			&attribs,
			0,
			nullptr,
			REG_OPTION_NON_VOLATILE,
			nullptr);
	}
	else {
		status = ZwOpenKey(handle, 0, &attribs);
	}

	ZwClose(root);
	return status;
}

NTAPI NTSTATUS RtlCheckRegistryKey(
	ULONG relative_to,
	PWSTR path) {
	HANDLE handle;
	auto status = open_reg_key(&handle, relative_to, path, false);
	if (NT_SUCCESS(status)) {
		ZwClose(handle);
	}
	return status;
}

NTAPI NTSTATUS RtlCreateRegistryKey(
	ULONG relative_to,
	PWSTR path) {
	HANDLE handle;
	auto status = open_reg_key(&handle, relative_to, path, true);
	if (NT_SUCCESS(status)) {
		if (relative_to != RTL_REGISTRY_HANDLE) {
			ZwClose(handle);
		}
	}
	return status;
}

NTAPI NTSTATUS RtlWriteRegistryValue(
	ULONG relative_to,
	PCWSTR path,
	PCWSTR value_name,
	ULONG value_type,
	PVOID value_data,
	ULONG value_length) {
	HANDLE handle;
	auto status = open_reg_key(&handle, relative_to, path, true);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	UNICODE_STRING name {};
	RtlInitUnicodeString(&name, value_name);

	status = ZwSetValueKey(handle, &name, 0, value_type, value_data, value_length);

	if (relative_to != RTL_REGISTRY_HANDLE) {
		ZwClose(handle);
	}
	return status;
}

NTAPI NTSTATUS RtlQueryRegistryValues(
	ULONG relative_to,
	PCWSTR path,
	RTL_QUERY_TABLE* query_table,
	PVOID ctx,
	PVOID environment) {
	HANDLE top_handle;
	auto status = open_reg_key(&top_handle, relative_to, path, false);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	status = STATUS_SUCCESS;

	OBJECT_ATTRIBUTES attribs {};
	UNICODE_STRING name {};
	HANDLE handle = top_handle;
	for (auto* entry = query_table; entry->query_routine || entry->name; ++entry) {
		if (entry->flags & RTL_QUERY_REGISTRY_TOPKEY) {
			if (handle != top_handle) {
				ZwClose(handle);
				handle = top_handle;
			}
		}

		if (entry->flags & RTL_QUERY_REGISTRY_SUBKEY) {
			if (!entry->name) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			if (handle != top_handle) {
				ZwClose(handle);
				handle = top_handle;
			}

			RtlInitUnicodeString(&name, entry->name);
			InitializeObjectAttributes(&attribs, &name, 0, top_handle, nullptr);
			status = ZwOpenKey(
				&handle,
				0,
				&attribs);
			if (!NT_SUCCESS(status)) {
				break;
			}
			else if (!entry->query_routine) {
				continue;
			}
		}
		else if (entry->name) {
			usize size = sizeof(KEY_VALUE_FULL_INFORMATION) + 256 * sizeof(WCHAR);
			auto* info = static_cast<KEY_VALUE_FULL_INFORMATION*>(kmalloc(size));
			if (!info) {
				status = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}

			RtlInitUnicodeString(&name, entry->name);
			ULONG result_len;

			for (int i = 0; i < 2; ++i) {
				status = ZwQueryValueKey(
					handle,
					&name,
					KeyValueFullInformation,
					info,
					size,
					&result_len);
				if (status == STATUS_BUFFER_OVERFLOW) {
					kfree(info, size);
					size = result_len;
					info = static_cast<KEY_VALUE_FULL_INFORMATION*>(kmalloc(size));
					if (!info) {
						status = STATUS_INSUFFICIENT_RESOURCES;
						break;
					}
				}
				else {
					break;
				}
			}

			if (status == STATUS_OBJECT_NAME_NOT_FOUND) {
				if ((entry->flags & RTL_QUERY_REGISTRY_REQUIRED) && entry->default_type == REG_NONE) {
					kfree(info, size);
					break;
				}

				status = entry->query_routine(
					entry->name,
					entry->default_type,
					entry->default_data,
					entry->default_length,
					ctx,
					entry->entry_ctx);
			}
			else if (NT_SUCCESS(status)) {
				status = entry->query_routine(
					entry->name,
					info->type,
					reinterpret_cast<char*>(info) + info->data_offset,
					info->data_length,
					ctx,
					entry->entry_ctx);
			}

			kfree(info, size);

			if (!NT_SUCCESS(status)) {
				break;
			}

			if (entry->flags & RTL_QUERY_REGISTRY_DELETE) {
				ZwDeleteValueKey(handle, &name);
			}

			continue;
		}
		else if (entry->flags & RTL_QUERY_REGISTRY_NOVALUE) {
			status = entry->query_routine(
				nullptr,
				REG_NONE,
				nullptr,
				0,
				ctx,
				entry->entry_ctx);
			if (!NT_SUCCESS(status)) {
				break;
			}
			continue;
		}

		// todo
		assert(false);
	}

	if (handle != top_handle) {
		ZwClose(handle);
	}

	if (relative_to != RTL_REGISTRY_HANDLE) {
		ZwClose(top_handle);
	}

	return status;
}
