#pragma once

#include "except.hpp"
#include "intsafe.h"

enum EXCEPTION_DISPOSITION {
	ExceptionContinueExecution,
	ExceptionContinueSearch,
	ExceptionNestedException,
	ExceptionCollidedUnwind
};

struct DISPATCHER_CONTEXT;
struct CONTEXT;
struct KNONVOLATILE_CONTEXT_POINTERS;

using PEXCEPTION_ROUTINE = EXCEPTION_DISPOSITION (*)(
	EXCEPTION_RECORD* exception_record,
	void* establisher_frame,
	CONTEXT* ctx,
	DISPATCHER_CONTEXT* dispatcher_ctx);

struct RUNTIME_FUNCTION;

struct UNWIND_HISTORY_TABLE_ENTRY {
	DWORD64 image_base;
	RUNTIME_FUNCTION* function_entry;
};

#define UNWIND_HISTORY_TABLE_SIZE 12

struct UNWIND_HISTORY_TABLE {
	DWORD count;
	BYTE local_hint;
	BYTE global_hint;
	BYTE search;
	BYTE once;
	DWORD64 low_addr;
	DWORD64 high_addr;
	UNWIND_HISTORY_TABLE_ENTRY entry[UNWIND_HISTORY_TABLE_SIZE];
};

struct SCOPE_TABLE {
	DWORD count;
	struct {
		DWORD begin_addr;
		DWORD end_addr;
		DWORD handler_addr;
		DWORD jump_target;
	} scope_record[];
};

// no handler
#define UNW_FLAG_NHANDLER 0
// exception handler (filter handler)
#define UNW_FLAG_EHANDLER 1
// unwind handler (termination handled called when unwinding an exception)
#define UNW_FLAG_UHANDLER 2
// function_entry = previous function table entry
#define UNW_FLAG_CHAININFO 4

void RtlUnwindExInternal(
	// the stack pointer for the target
	// (usually establisher_frame, stack pointer of the function where the exception happened)
	PVOID target_frame,
	// target ip to unwind to
	PVOID target_ip,
	// cause of the unwind
	EXCEPTION_RECORD* exception_record,
	// return value put to the return value register before transferring control
	// to the new context
	PVOID return_value,
	// updated when unwinding (initial values are ignored)
	CONTEXT* ctx,
	UNWIND_HISTORY_TABLE* history_table,
	ULONG handler_type);

NTAPI extern "C" RUNTIME_FUNCTION* RtlLookupFunctionEntry(
	DWORD64 control_pc,
	PDWORD64 image_base,
	UNWIND_HISTORY_TABLE* history_table);

NTAPI extern "C" PEXCEPTION_ROUTINE RtlVirtualUnwind(
	DWORD handler_type,
	DWORD64 image_base,
	DWORD64 control_pc,
	RUNTIME_FUNCTION* function_entry,
	CONTEXT* ctx,
	PVOID* handler_data,
	PDWORD64 establisher_frame,
	KNONVOLATILE_CONTEXT_POINTERS* ctx_pointers);

NTAPI extern "C" void RtlUnwindEx(
	// the stack pointer for the target
	// (usually establisher_frame, stack pointer of the function where the exception happened)
	PVOID target_frame,
	// target ip to unwind to
	PVOID target_ip,
	// cause of the unwind
	EXCEPTION_RECORD* exception_record,
	// return value put to the return value register before transferring control
	// to the new context
	PVOID return_value,
	// updated when unwinding (initial values are ignored)
	CONTEXT* ctx,
	UNWIND_HISTORY_TABLE* history_table);

extern "C" void RtlCaptureContext(CONTEXT* ctx);
extern "C" void RtlRestoreContext(CONTEXT* ctx, EXCEPTION_RECORD* exception_record);
