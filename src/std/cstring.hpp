#pragma once
#include <stddef.h>

NTAPI extern "C" void* memcpy(void* __restrict dest, const void* __restrict src, size_t size);
NTAPI extern "C" void* memset(void* __restrict dest, int ch, size_t size);
NTAPI extern "C" void* memmove(void* dest, const void* src, size_t size);
NTAPI extern "C" int memcmp(const void* __restrict a, const void* __restrict b, size_t size);

NTAPI extern "C" size_t strlen(const char* str);
NTAPI extern "C" size_t strnlen(const char* str, size_t max);
NTAPI extern "C" int strcmp(const char* a, const char* b);
NTAPI extern "C" int strncmp(const char* a, const char* b, size_t size);
NTAPI extern "C" char* strstr(const char* str, const char* substr);

NTAPI extern "C" int _stricmp(const char* a, const char* b);

NTAPI extern "C" size_t mbstowcs(char16_t* dest, const char* src, size_t len);

NTAPI extern "C" char* strncpy(char* dest, const char* src, size_t count);
NTAPI extern "C" char* strtok_s(char* str, const char* delimiters, char** ctx);

#define EINVAL 22
#define ERANGE 34
#define EILSEQ 42

NTAPI extern "C" int strcpy_s(char* dest, size_t dest_size, const char* src);
NTAPI extern "C" int strcat_s(char* dest, size_t dest_size, const char* src);

#define memcpy __builtin_memcpy
#define memset __builtin_memset
#define memcmp __builtin_memcmp
#define strlen __builtin_strlen
