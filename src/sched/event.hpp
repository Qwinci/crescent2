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

NTAPI extern "C" void KeInitializeEvent(KEVENT* event, EVENT_TYPE type, BOOLEAN state);
NTAPI extern "C" LONG KeSetEvent(KEVENT* event, KPRIORITY increment, BOOLEAN wait);
NTAPI extern "C" void KeClearEvent(KEVENT* event);
NTAPI extern "C" LONG KeResetEvent(KEVENT* event);
NTAPI extern "C" LONG KeReadStateEvent(KEVENT* event);

struct OBJECT_TYPE;

NTAPI extern "C" OBJECT_TYPE* ExEventObjectType;
