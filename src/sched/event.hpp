#pragma once
#include "dispatch_header.hpp"

struct KEVENT {
	DISPATCHER_HEADER header;
};

using KPRIORITY = LONG;

enum class EVENT_TYPE {
	Notification,
	Synchronization
};

extern "C" void KeInitializeEvent(KEVENT* event, EVENT_TYPE type, BOOLEAN state);
extern "C" LONG KeSetEvent(KEVENT* event, KPRIORITY increment, BOOLEAN wait);
extern "C" void KeClearEvent(KEVENT* event);
extern "C" LONG KeResetEvent(KEVENT* event);
