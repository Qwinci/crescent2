#pragma once

#include "ntdef.h"

NTAPI extern "C" void ProbeForRead(const volatile void* addr, SIZE_T length, ULONG alignment);
NTAPI extern "C" void ProbeForWrite(volatile void* addr, SIZE_T length, ULONG alignment);
