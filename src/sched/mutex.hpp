#pragma once

#include "event.hpp"

struct FAST_MUTEX {
	LONG count;
	PVOID owner;
	ULONG contention;
	KEVENT event;
	ULONG old_irql;
};
using KGUARDED_MUTEX = FAST_MUTEX;

NTAPI extern "C" void KeInitializeGuardedMutex(KGUARDED_MUTEX* mutex);
NTAPI extern "C" void KeAcquireGuardedMutex(KGUARDED_MUTEX* mutex);
NTAPI extern "C" void KeReleaseGuardedMutex(KGUARDED_MUTEX* mutex);
