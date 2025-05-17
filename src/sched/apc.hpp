#pragma once
#include "thread.hpp"
#include "wait.hpp"
#include "event.hpp"

enum class KAPC_ENVIRONMENT {
	Original,
	Attached,
	Current
};

struct KAPC;

using PKNORMAL_ROUTINE = void (*)(
	PVOID normal_ctx,
	PVOID system_arg1,
	PVOID system_arg2);
using PKKERNEL_ROUTINE = void (*)(
	KAPC* apc,
	PKNORMAL_ROUTINE* normal_routine,
	PVOID* normal_ctx,
	PVOID* system_arg1,
	PVOID* system_arg2);
using PKRUNDOWN_ROUTINE = void (*)(KAPC* apc);

struct KAPC {
	UCHAR type;
	UCHAR all_flags;
	UCHAR size;
	UCHAR spare_byte1;
	ULONG spare_long;
	Thread* thread;
	LIST_ENTRY apc_list_entry;
	PKKERNEL_ROUTINE kernel_routine;
	PKRUNDOWN_ROUTINE rundown_routine;
	PKNORMAL_ROUTINE normal_routine;
	PVOID normal_ctx;
	PVOID system_arg1;
	PVOID system_arg2;
	CCHAR apc_state_index;
	KPROCESSOR_MODE apc_mode;
	BOOLEAN inserted;
};

NTAPI extern "C" void KeInitializeApc(
	KAPC* apc,
	Thread* thread,
	KAPC_ENVIRONMENT environment,
	PKKERNEL_ROUTINE kernel_routine,
	PKRUNDOWN_ROUTINE rundown_routine,
	PKNORMAL_ROUTINE normal_routine,
	KPROCESSOR_MODE apc_mode,
	PVOID normal_ctx);

NTAPI extern "C" BOOLEAN KeInsertQueueApc(
	KAPC* apc,
	PVOID system_arg1,
	PVOID system_arg2,
	KPRIORITY increment);

NTAPI extern "C" BOOLEAN KeAreAllApcsDisabled();

struct KTRAP_FRAME;
struct KEXCEPTION_FRAME;

void deliver_apc(
	KPROCESSOR_MODE previous_mode,
	KTRAP_FRAME* trap_frame,
	KEXCEPTION_FRAME* exception_frame);
void deliver_user_start_apc(KTRAP_FRAME* trap_frame);
