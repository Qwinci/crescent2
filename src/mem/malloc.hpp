#pragma once
#include "types.hpp"
#include "ntdef.h"

void* kmalloc(usize size);
void kfree(void* ptr, usize size);

using POOL_FLAGS = ULONG;

NTAPI extern "C" void* ExAllocatePool2(POOL_FLAGS flags, size_t num_of_bytes, ULONG tag);
NTAPI extern "C" void ExFreePool(void* ptr);
