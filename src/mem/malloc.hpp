#pragma once
#include "types.hpp"
#include "ntdef.h"
#include "pool.hpp"

void* kmalloc(usize size);
void kfree(void* ptr, usize size);

inline void* kcalloc(usize size) {
	auto* ptr = kmalloc(size);
	if (!ptr) {
		return nullptr;
	}
	__builtin_memset(ptr, 0, size);
	return ptr;
}

using POOL_FLAGS = ULONG;

NTAPI extern "C" void* ExAllocatePool2(POOL_FLAGS flags, size_t num_of_bytes, ULONG tag);
NTAPI extern "C" void* ExAllocatePoolWithTag(POOL_TYPE pool_type, size_t num_of_bytes, ULONG tag);
NTAPI extern "C" void* ExAllocatePoolWithQuotaTag(POOL_TYPE pool_type, size_t num_of_bytes, ULONG tag);
NTAPI extern "C" void ExFreePool(void* ptr);
NTAPI extern "C" void ExFreePoolWithTag(void* ptr, ULONG tag);
