#pragma once
#include <stddef.h>

extern "C" void* memcpy(void* __restrict dest, const void* __restrict src, size_t size);
extern "C" void* memset(void* __restrict dest, int ch, size_t size);
extern "C" int memcmp(const void* __restrict a, const void* __restrict b, size_t size);

#define memcpy __builtin_memcpy
#define memset __builtin_memset
#define memcmp __builtin_memcmp
