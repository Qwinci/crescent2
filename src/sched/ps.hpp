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

struct Thread;
struct Cpu;

Process* create_process(kstd::wstring_view name);
Thread* create_initial_thread(Cpu* cpu);
Thread* create_thread(kstd::wstring_view name, Cpu* cpu, Process* process, bool user, void (*fn)(void*), void* arg);

NTAPI extern "C" NTSTATUS PsCreateSystemThread(
	PHANDLE thread_handle,
	ULONG desired_access,
	OBJECT_ATTRIBUTES* object_attribs,
	HANDLE process_handle,
	CLIENT_ID* client_id,
	PKSTART_ROUTINE start_routine,
	PVOID start_ctx);
NTAPI extern "C" NTSTATUS PsTerminateSystemThread(NTSTATUS exit_status);

NTAPI extern "C" HANDLE PsGetCurrentThreadId();
NTAPI extern "C" HANDLE PsGetCurrentProcessId();

NTAPI extern "C" NTSTATUS PsLookupProcessByProcessId(
	HANDLE process_id,
	Process** process);
NTAPI extern "C" HANDLE PsGetProcessId(Process* process);
