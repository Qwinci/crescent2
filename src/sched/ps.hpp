#pragma once

#include "ntdef.h"
#include "fs/object.hpp"
#include "string_view.hpp"

struct CLIENT_ID {
	HANDLE UniqueProcess;
	HANDLE UniqueThread;
};

using PKSTART_ROUTINE = void (*)(PVOID start_ctx);

void ps_init();

Process* create_process(kstd::wstring_view name);

NTAPI extern "C" NTSTATUS PsCreateSystemThread(
	PHANDLE thread_handle,
	ULONG desired_access,
	OBJECT_ATTRIBUTES* object_attribs,
	HANDLE process_handle,
	CLIENT_ID* client_id,
	PKSTART_ROUTINE start_routine,
	PVOID start_ctx);

NTAPI extern "C" HANDLE PsGetCurrentProcessId();
