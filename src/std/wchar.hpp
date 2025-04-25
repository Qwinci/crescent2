#pragma once

#include "ntdef.h"

NTAPI extern "C" int wcscmp(const WCHAR* a, const WCHAR* b);
NTAPI extern "C" int wcsncmp(const WCHAR* a, const WCHAR* b, size_t count);
NTAPI extern "C" size_t wcslen(const WCHAR* str);
NTAPI extern "C" size_t wcsnlen(const WCHAR* str, size_t max);

NTAPI extern "C" WCHAR* wcschr(const WCHAR* str, WCHAR ch);
NTAPI extern "C" WCHAR* wcsrchr(const WCHAR* str, WCHAR ch);
NTAPI extern "C" WCHAR* wcsstr(const WCHAR* str, const WCHAR* substr);

NTAPI extern "C" int _wcsicmp(const WCHAR* a, const WCHAR* b);
NTAPI extern "C" int _wcsnicmp(const WCHAR* a, const WCHAR* b, size_t count);

NTAPI extern "C" size_t wcstombs(char* dest, const WCHAR* src, size_t len);

NTAPI extern "C" WCHAR* _wcslwr(WCHAR* str);

NTAPI extern "C" int wcsncat_s(WCHAR* dest, size_t dest_size, const WCHAR* src, size_t count);
NTAPI extern "C" int _wcsnset_s(WCHAR* dest, size_t dest_size, WCHAR ch, size_t count);

#define STRUNCATE 80

NTAPI extern "C" int wcscpy_s(WCHAR* dest, size_t dest_size, const WCHAR* src);
NTAPI extern "C" int wcsncpy_s(WCHAR* dest, size_t dest_size, const WCHAR* src, size_t count);
NTAPI extern "C" int wcscat_s(WCHAR* dest, size_t dest_size, const WCHAR* src);
