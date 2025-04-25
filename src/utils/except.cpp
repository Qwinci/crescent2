#include "except.hpp"
#include "except_internals.hpp"
#include "arch/arch_except.hpp"
#include "exe/driver_loader.hpp"
#include "stdio.hpp"

static RUNTIME_FUNCTION* RtlLookupTable(
	DWORD64 control_pc,
	PDWORD64 image_base,
	PULONG length) {
	for (auto& image : LOADED_IMAGE_LIST) {
		if (control_pc >= image.image_base && control_pc < image.image_base + image.image_size) {
			*image_base = image.image_base;
			*length = image.function_table_len;
			return image.function_table;
		}
	}

	return nullptr;
}

NTAPI RUNTIME_FUNCTION* RtlLookupFunctionEntry(
	DWORD64 control_pc,
	PDWORD64 image_base,
	UNWIND_HISTORY_TABLE* history_table) {
	ULONG length;
	auto* table = RtlLookupTable(control_pc, image_base, &length);
	if (!table) {
		return nullptr;
	}

	control_pc -= *image_base;

	u32 entries = length / sizeof(RUNTIME_FUNCTION);
	for (u32 i = 0; i < entries; ++i) {
		auto& entry = table[i];
		if (control_pc >= entry.begin_addr && control_pc < entry.end_addr) {
			return &entry;
		}
	}

	return nullptr;
}

using PEXCEPTION_FILTER = LONG (*)(EXCEPTION_POINTERS* exception_pointers, PVOID establisher_frame);
using PTERMINATION_HANDLER = void (*)(BOOLEAN abnormal_termination, PVOID establisher_frame);

extern "C" EXCEPTION_DISPOSITION __C_specific_handler(
	EXCEPTION_RECORD* exception_record,
	void* establisher_frame,
	CONTEXT* ctx,
	DISPATCHER_CONTEXT* dispatcher_ctx) {
	auto* scope_table = static_cast<SCOPE_TABLE*>(dispatcher_ctx->handler_data);

	EXCEPTION_POINTERS pointers {
		.exception_record = exception_record,
		.ctx = ctx
	};

	auto image_base = dispatcher_ctx->image_base;
	auto relative_ip = dispatcher_ctx->control_pc - image_base;
	auto relative_target_ip = dispatcher_ctx->target_ip - image_base;

	while (dispatcher_ctx->scope_index < scope_table->count) {
		auto& scope = scope_table->scope_record[dispatcher_ctx->scope_index++];

		if (relative_ip < scope.begin_addr || relative_ip >= scope.end_addr) {
			continue;
		}

		if (exception_record->exception_flags & EXCEPTION_UNWIND) {
			if (exception_record->exception_flags & EXCEPTION_TARGET_UNWIND) {
				if (relative_target_ip >= scope.begin_addr && relative_target_ip < scope.end_addr) {
					return ExceptionContinueSearch;
				}
			}

			// __finally has no jump target
			if (!scope.jump_target) {
				auto handler = reinterpret_cast<PTERMINATION_HANDLER>(image_base + scope.handler_addr);
				handler(true, establisher_frame);
			}
			else if (scope.jump_target == relative_target_ip) {
				return ExceptionContinueSearch;
			}
		}
		else {
			if (!scope.jump_target) {
				continue;
			}

			LONG filter_result;
			if (scope.handler_addr == EXCEPTION_EXECUTE_HANDLER) {
				filter_result = EXCEPTION_EXECUTE_HANDLER;
			}
			else {
				auto filter = reinterpret_cast<PEXCEPTION_FILTER>(image_base + scope.handler_addr);
				filter_result = filter(&pointers, establisher_frame);
			}

			if (filter_result < 0) {
				return ExceptionContinueExecution;
			}
			else if (filter_result > 0) {
				auto jump_target = reinterpret_cast<PVOID>(image_base + scope.jump_target);
				RtlUnwindEx(
					establisher_frame,
					jump_target,
					exception_record,
					reinterpret_cast<PVOID>(exception_record->exception_code),
					dispatcher_ctx->ctx,
					dispatcher_ctx->history_table);
			}
		}
	}

	return ExceptionContinueSearch;
}

NTAPI void ExRaiseDatatypeMisalignment() {
	RtlRaiseStatus(STATUS_DATATYPE_MISALIGNMENT);
}

NTAPI void ExRaiseAccessViolation() {
	RtlRaiseStatus(STATUS_ACCESS_VIOLATION);
}

NTAPI void ExRaiseStatus(NTSTATUS status) {
	RtlRaiseStatus(status);
}

extern "C" EXCEPTION_DISPOSITION __CxxFrameHandler3(
	EXCEPTION_RECORD* exception_record,
	void* establisher_frame,
	CONTEXT* ctx,
	DISPATCHER_CONTEXT* dispatcher_ctx) {
	return ExceptionContinueSearch;
}
