#pragma once
#include "wait.hpp"

struct KSEMAPHORE {
	DISPATCHER_HEADER header;
	i32 limit;
};

using KPRIORITY = i32;

extern "C" void KeInitializeSemaphore(KSEMAPHORE* semaphore, i32 count, i32 limit);
extern "C" i32 KeReleaseSemaphore(KSEMAPHORE* semaphore, KPRIORITY increment, i32 adjustment, bool wait);
