#pragma once

#include "utils/except_internals.hpp"

struct RUNTIME_FUNCTION {
	ULONG begin_addr;
	ULONG end_addr;
	ULONG unwind_data;
};

using PEXCEPTION_ROUTINE = EXCEPTION_DISPOSITION (*)(
	EXCEPTION_RECORD* exception_record,
	void* establisher_frame,
	CONTEXT* ctx,
	DISPATCHER_CONTEXT* dispatcher_ctx);

struct DISPATCHER_CONTEXT {
	ULONG64 control_pc;
	ULONG64 image_base;
	RUNTIME_FUNCTION* function_entry;
	ULONG64 establisher_frame;
	ULONG64 target_ip;
	CONTEXT* ctx;
	PEXCEPTION_ROUTINE language_handler;
	PVOID handler_data;
	UNWIND_HISTORY_TABLE* history_table;
	ULONG scope_index;
	ULONG fill0;
};
