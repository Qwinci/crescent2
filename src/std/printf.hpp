#pragma once

#include "ntdef.h"
#include <stdarg.h>

NTAPI extern "C" [[gnu::format(printf, 2, 3)]]
int sprintf(char* buffer, const char* fmt, ...);
NTAPI extern "C" [[gnu::format(printf, 3, 4)]]
int sprintf_s(char* buffer, size_t buffer_size, const char* fmt, ...);
NTAPI extern "C" [[gnu::format(printf, 3, 0)]]
int vsprintf_s(char* buffer, size_t buffer_size, const char* fmt, va_list ap);

NTAPI extern "C" [[gnu::format(printf, 3, 4)]]
int _snprintf(char* buffer, size_t buffer_size, const char* fmt, ...);
NTAPI extern "C" [[gnu::format(printf, 3, 0)]]
int _vsnprintf(char* buffer, size_t buffer_size, const char* fmt, va_list ap);
NTAPI extern "C" [[gnu::format(printf, 4, 0)]]
int _vsnprintf_s(char* buffer, size_t buffer_size, size_t count, const char* fmt, va_list ap);
NTAPI extern "C" [[gnu::format(printf, 4, 5)]]
int _snprintf_s(char* buffer, size_t buffer_size, size_t count, const char* fmt, ...);

NTAPI extern "C" int swprintf(WCHAR* buffer, size_t buffer_size, const WCHAR* fmt, ...);
NTAPI extern "C" int vswprintf(WCHAR* buffer, size_t buffer_size, const WCHAR* fmt, va_list ap);

NTAPI extern "C" int _vsnwprintf(WCHAR* buffer, size_t buffer_size, const WCHAR* fmt, va_list ap);

NTAPI extern "C" [[gnu::format(printf, 3, 4)]]
NTSTATUS DbgPrintEx(ULONG component_id, ULONG level, PCSTR format, ...);
