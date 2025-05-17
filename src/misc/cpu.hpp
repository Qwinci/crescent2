#pragma once

#include "ntdef.h"

NTAPI extern "C" ULONG KeQueryMaximumProcessorCount();

NTAPI extern "C" volatile CCHAR KeNumberProcessors;
