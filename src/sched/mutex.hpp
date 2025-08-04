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

struct Thread;

struct KMUTANT {
	DISPATCHER_HEADER header;
	LIST_ENTRY mutant_list_entry;
	Thread* owner_thread;
	union {
		UCHAR mutant_flags;
		struct {
			UCHAR abandoned : 1;
			UCHAR spare : 7;
		};
	};
	UCHAR apc_disable;
};

NTAPI extern "C" void KeInitializeGuardedMutex(KGUARDED_MUTEX* mutex);
NTAPI extern "C" void KeAcquireGuardedMutex(KGUARDED_MUTEX* mutex);
NTAPI extern "C" void KeReleaseGuardedMutex(KGUARDED_MUTEX* mutex);

NTAPI extern "C" void KeInitializeMutex(KMUTANT* mutex, ULONG level);
NTAPI extern "C" LONG KeReleaseMutex(KMUTANT* mutex, BOOLEAN wait);

NTAPI extern "C" void ExAcquireFastMutex(FAST_MUTEX* mutex);
NTAPI extern "C" void ExReleaseFastMutex(FAST_MUTEX* mutex);
