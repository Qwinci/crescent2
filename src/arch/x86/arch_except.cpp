#include "arch/arch_except.hpp"
#include "arch/arch_irq.hpp"
#include "types.hpp"
#include "stdio.hpp"
#include "assert.hpp"
#include "cstring.hpp"

union UNWIND_CODE {
	struct {
		BYTE offset_in_prolog;
		BYTE unwind_opcode : 4;
		BYTE op_info : 4;
	};
	USHORT whole;
};

struct UNWIND_INFO {
	BYTE version : 3;
	BYTE flags : 5;
	BYTE size_of_prolog;
	BYTE num_unwind_codes;
	BYTE frame_register : 4;
	BYTE frame_register_offset : 4;
	UNWIND_CODE unwind_codes[];
	// if exception handler:
	//  ULONG addr_of_exception_handler;
	//  char language_specific_data[];
	// if chained unwind info:
	//  ULONG func_start_addr;
	//  ULONG func_end_addr;
	//  ULONG unwind_info_addr;
};

#define UWOP_PUSH_NONVOL 0
#define UWOP_ALLOC_LARGE 1
#define UWOP_ALLOC_SMALL 2
#define UWOP_SET_FPREG 3
#define UWOP_SAVE_NONVOL 4
#define UWOP_SAVE_NONVOL_FAR 5
#define UWOP_EPILOG 6
#define UWOP_SAVE_XMM128 8
#define UWOP_SAVE_XMM128_FAR 9
#define UWOP_PUSH_MACHFRAME 10

static u32 get_unwind_slots(UNWIND_CODE code) {
	switch (code.unwind_opcode) {
		case UWOP_PUSH_NONVOL:
		case UWOP_ALLOC_SMALL:
		case UWOP_SET_FPREG:
		case UWOP_EPILOG:
		case UWOP_PUSH_MACHFRAME:
			return 1;
		case UWOP_ALLOC_LARGE:
			return code.op_info == 0 ? 2 : 3;
		case UWOP_SAVE_NONVOL:
		case UWOP_SAVE_XMM128:
			return 2;
		case UWOP_SAVE_NONVOL_FAR:
		case UWOP_SAVE_XMM128_FAR:
			return 3;
		default:
			panic("[kernel][except]: invalid unwind opcode");
	}
}

NTAPI extern "C" PEXCEPTION_ROUTINE RtlVirtualUnwind(
	DWORD handler_type,
	DWORD64 image_base,
	DWORD64 control_pc,
	RUNTIME_FUNCTION* function_entry,
	CONTEXT* ctx,
	PVOID* handler_data,
	PDWORD64 establisher_frame,
	KNONVOLATILE_CONTEXT_POINTERS* ctx_pointers) {
	auto* info = offset(image_base, const UNWIND_INFO*, function_entry->unwind_data);
	assert(!(info->flags & UNW_FLAG_CHAININFO));

	control_pc -= image_base;
	control_pc -= function_entry->begin_addr;

	if (info->frame_register == 0) {
		*establisher_frame = ctx->rsp;
	}
	else if (control_pc >= info->size_of_prolog ||
	         (info->flags & UNW_FLAG_CHAININFO)) {
		*establisher_frame = (&ctx->rax)[info->frame_register] - info->frame_register_offset * 16;
	}
	else {
		bool found = false;
		for (u32 i = 0; i < info->num_unwind_codes; i += get_unwind_slots(info->unwind_codes[i])) {
			auto& code = info->unwind_codes[i];
			if (code.offset_in_prolog <= control_pc &&
			    code.unwind_opcode == UWOP_SET_FPREG) {
				*establisher_frame = (&ctx->rax)[info->frame_register] - info->frame_register_offset * 16;
				found = true;
				break;
			}
		}

		if (!found) {
			*establisher_frame = ctx->rsp;
		}
	}

	if (info->num_unwind_codes && info->unwind_codes[0].unwind_opcode == UWOP_EPILOG) {
		auto* code = &info->unwind_codes[0];
		auto epilog_size = code->offset_in_prolog;
		assert(epilog_size);

		bool in_epilog = false;

		// if bit 0 is set then only one epilogue, else multiple codes
		if (code->op_info & 1) {
			auto relative_epilog_start = function_entry->end_addr - epilog_size - function_entry->begin_addr;
			if (control_pc >= relative_epilog_start) {
				in_epilog = true;
			}
		}

		if (!in_epilog) {
			for (u32 i = 1; i < info->num_unwind_codes; ++i) {
				code = &info->unwind_codes[i];
				if (code->unwind_opcode != UWOP_EPILOG) {
					break;
				}

				u16 offset = code->offset_in_prolog | static_cast<u16>(code->op_info) << 8;
				if (!offset) {
					break;
				}

				auto relative_epilog_start = function_entry->end_addr - offset - function_entry->begin_addr;
				if (control_pc >= relative_epilog_start) {
					in_epilog = true;
					break;
				}
			}
		}

		assert(!in_epilog);
	}

	// the unwind op array is in descending order

	u32 i = 0;
	if (control_pc <= info->size_of_prolog) {
		for (; i < info->num_unwind_codes; i += get_unwind_slots(info->unwind_codes[i])) {
			if (info->unwind_codes[i].offset_in_prolog <= control_pc) {
				break;
			}
		}
	}

	// undo the effects of the prolog
	for (; i < info->num_unwind_codes; i += get_unwind_slots(info->unwind_codes[i])) {
		auto& code = info->unwind_codes[i];

		switch (code.unwind_opcode) {
			case UWOP_PUSH_NONVOL:
				(&ctx->rax)[code.op_info] = *reinterpret_cast<u64*>(ctx->rsp);
				ctx->rsp += 8;
				break;
			case UWOP_ALLOC_LARGE:
				if (code.op_info == 0) {
					ctx->rsp += info->unwind_codes[i + 1].whole * 8;
				}
				else {
					ctx->rsp += info->unwind_codes[i + 1].whole |
					            info->unwind_codes[i + 2].whole << 16;
				}
				break;
			case UWOP_ALLOC_SMALL:
				ctx->rsp += 8 + code.op_info * 8;
				break;
			case UWOP_SET_FPREG:
				ctx->rsp = (&ctx->rax)[info->frame_register] - info->frame_register_offset * 16;
				break;
			case UWOP_SAVE_NONVOL:
				(&ctx->rax)[code.op_info] = *reinterpret_cast<u64*>(ctx->rsp + info->unwind_codes[i + 1].whole * 8);
				break;
			case UWOP_SAVE_NONVOL_FAR:
			{
				u32 offset;
				memcpy(&offset, &info->unwind_codes[i + 1], 4);
				(&ctx->rax)[code.op_info] = *reinterpret_cast<u64*>(ctx->rsp + offset);
				break;
			}
			case UWOP_EPILOG:
				break;
			case UWOP_SAVE_XMM128:
				ctx->xmm[code.op_info] = *reinterpret_cast<M128A*>(ctx->rsp + info->unwind_codes[i + 1].whole * 16);
				break;
			case UWOP_SAVE_XMM128_FAR:
			{
				u32 offset;
				memcpy(&offset, &info->unwind_codes[i + 1], 4);
				ctx->xmm[code.op_info] = *reinterpret_cast<M128A*>(ctx->rsp + offset);
				break;
			}
			case UWOP_PUSH_MACHFRAME:
				if (code.op_info == 0) {
					ctx->rip = *reinterpret_cast<u64*>(ctx->rsp);
					ctx->rsp = *reinterpret_cast<u64*>(ctx->rsp + 24);
				}
				else {
					ctx->rip = *reinterpret_cast<u64*>(ctx->rsp + 8);
					ctx->rsp = *reinterpret_cast<u64*>(ctx->rsp + 32);
				}
				assert(i + 1 == info->num_unwind_codes);
				goto exit;
			default:
				panic("[kernel][except]: invalid unwind opcode");
		}
	}

	if (ctx->rsp != 0) {
		ctx->rip = *reinterpret_cast<u64*>(ctx->rsp);
		ctx->rsp += 8;
	}

	exit:
	if (info->flags & handler_type) {
		auto ptr = offset(&info->unwind_codes, u32*, ALIGNUP(info->num_unwind_codes, 2) * 2);
		auto addr_of_exception_handler = *ptr;
		*handler_data = &ptr[1];
		return offset(image_base, PEXCEPTION_ROUTINE, addr_of_exception_handler);
	}

	return nullptr;
}

extern "C" [[gnu::naked, gnu::used]] void unwind_exception_handler() {
	asm volatile(R"(
	// get the dispatcher context ptr from the establisher frame
	// (stored before calling language handler in execute_unwind_handler)
	mov rax, [rdx + 32]

	// copy the dispatcher context (excluding target_ip)
	// control_pc
	mov r10, [rax + 0]
	mov [r9 + 0], r10
	// image_base
	mov r10, [rax + 8]
	mov [r9 + 8], r10
	// function_entry
	mov r10, [rax + 16]
	mov [r9 + 16], r10
	// establisher_frame
	mov r10, [rax + 24]
	mov [r9 + 24], r10
	// ctx
	mov r10, [rax + 40]
	mov [r9 + 40], r10
	// language_handler
	mov r10, [rax + 48]
	mov [r9 + 48], r10
	// handler_data
	mov r10, [rax + 56]
	mov [r9 + 56], r10
	// history_table
	mov r10, [rax + 64]
	mov [r9 + 64], r10
	// scope_index
	mov r10d, [rax + 72]
	mov [r9 + 72], r10d

	// ExceptionCollidedUnwind
	mov eax, 3
	ret
)");
}

extern "C" [[gnu::naked, gnu::used]] void exception_exception_handler() {
	asm volatile(R"(
	// ExceptionContinueSearch
	mov eax, 1

	// if unwinding then return ExceptionContinueSearch
	test dword ptr [rcx + 4], 0x66
	jnz 1f
	// get the dispatcher context ptr from the establisher frame
	// (stored before calling language handler in execute_exception_handler)
	mov rax, [rdx + 32]
	// get establisher frame out of the original dispatcher context
	mov rax, [rax + 24]
	// store it in current dispatch ctx
	mov [r9 + 24], rax
	// ExceptionNestedException
	mov eax, 2
1:
	ret
)");
}

extern "C" [[gnu::naked]] EXCEPTION_DISPOSITION execute_unwind_handler(
	EXCEPTION_RECORD* exception_record,
	DWORD64 establisher_frame,
	CONTEXT* ctx,
	DISPATCHER_CONTEXT* dispatcher_ctx) {
	asm volatile(R"(
.seh_proc execute_unwind_handler
.seh_handler unwind_exception_handler, @unwind, @except
	sub rsp, (32 + 8)
	.seh_stackalloc (32 + 8)
	.seh_endprologue

	// store dispatcher_ctx to be used by unwind_exception_handler
	mov [rsp + 32], r9

	// dispatcher_ctx->language_handler
	call qword ptr [r9 + 48]
	// this nop is here because the return address of the previous call would point to the epilogue otherwise
	// and the handler wouldn't get called
	nop
	add rsp, (32 + 8)
	ret
.seh_endproc
)");
}

extern "C" [[gnu::naked]] EXCEPTION_DISPOSITION execute_exception_handler(
	EXCEPTION_RECORD* exception_record,
	DWORD64 establisher_frame,
	CONTEXT* ctx,
	DISPATCHER_CONTEXT* dispatcher_ctx) {
	asm volatile(R"(
.seh_proc execute_exception_handler
.seh_handler exception_exception_handler, @unwind, @except
	sub rsp, (32 + 8)
	.seh_stackalloc (32 + 8)
	.seh_endprologue

	// store dispatcher_ctx to be used by exception_exception_handler
	mov [rsp + 32], r9

	// dispatcher_ctx->language_handler
	call qword ptr [r9 + 48]
	// this nop is here because the return address of the previous call would point to the epilogue otherwise
	// and the handler wouldn't get called
	nop
	add rsp, (32 + 8)
	ret
.seh_endproc
)");
}

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
	ULONG handler_type) {
	CONTEXT unwind_ctx = *ctx;
	DWORD64 next_establisher_frame = 0;

	while (true) {
		auto control_pc = unwind_ctx.rip;

		DWORD64 image_base;
		auto entry = RtlLookupFunctionEntry(
			control_pc,
			&image_base,
			history_table);

		DWORD64 establisher_frame = 0;
		if (entry) {
			PVOID handler_data;
			auto language_handler = RtlVirtualUnwind(
				handler_type,
				image_base,
				control_pc,
				entry,
				&unwind_ctx,
				&handler_data,
				&establisher_frame,
				nullptr);

			// todo raise STATUS_BAD_STACK if the establisher frame is not 8 byte aligned
			// or is not within the limits of the current thread or dpc stack
			// or target_frame is not null and is less than establisher frame

			DISPATCHER_CONTEXT dispatch_ctx {};
			// call language handler
			if (language_handler) {
				dispatch_ctx.target_ip = reinterpret_cast<ULONG64>(target_ip);

				ULONG scope_index = 0;

				while (true) {
					if (reinterpret_cast<PVOID>(establisher_frame) == target_frame) {
						exception_record->exception_flags |= EXCEPTION_TARGET_UNWIND;
					}

					unwind_ctx.rax = reinterpret_cast<ULONG64>(return_value);

					dispatch_ctx.control_pc = control_pc;
					dispatch_ctx.image_base = image_base;
					dispatch_ctx.function_entry = entry;
					dispatch_ctx.establisher_frame = establisher_frame;
					dispatch_ctx.ctx = &unwind_ctx;
					dispatch_ctx.language_handler = language_handler;
					dispatch_ctx.handler_data = handler_data;
					dispatch_ctx.history_table = history_table;
					dispatch_ctx.scope_index = scope_index;

					EXCEPTION_DISPOSITION disposition;
					if (handler_type == UNW_FLAG_UHANDLER) {
						disposition = execute_unwind_handler(
							exception_record,
							establisher_frame,
							ctx,
							&dispatch_ctx);
					}
					else {
						disposition = execute_exception_handler(
							exception_record,
							establisher_frame,
							ctx,
							&dispatch_ctx);
					}

					exception_record->exception_flags &= ~(EXCEPTION_TARGET_UNWIND | EXCEPTION_COLLIDED_UNWIND);

					if (next_establisher_frame == establisher_frame) {
						exception_record->exception_flags &= ~EXCEPTION_NESTED_CALL;
						next_establisher_frame = 0;
					}

					bool collided = false;

					switch (disposition) {
						case ExceptionContinueExecution:
							if (exception_record->exception_flags & EXCEPTION_NONCONTINUABLE) {
								RtlRaiseStatus(STATUS_NONCONTINUABLE_EXCEPTION);
							}
							return;
						case ExceptionContinueSearch:
							break;
						case ExceptionNestedException:
							exception_record->exception_flags |= EXCEPTION_NESTED_CALL;
							if (dispatch_ctx.establisher_frame > next_establisher_frame) {
								next_establisher_frame = dispatch_ctx.establisher_frame;
							}
							break;
						case ExceptionCollidedUnwind:
							image_base = dispatch_ctx.image_base;
							control_pc = dispatch_ctx.control_pc;
							entry = dispatch_ctx.function_entry;

							unwind_ctx = *dispatch_ctx.ctx;
							*ctx = unwind_ctx;

							if (handler_type == UNW_FLAG_UHANDLER) {
								RtlVirtualUnwind(
									UNW_FLAG_NHANDLER,
									image_base,
									control_pc,
									entry,
									&unwind_ctx,
									&handler_data,
									&establisher_frame,
									nullptr);
							}
							else {
								unwind_ctx.rip = control_pc;
							}

							establisher_frame = dispatch_ctx.establisher_frame;
							language_handler = dispatch_ctx.language_handler;
							handler_data = dispatch_ctx.handler_data;
							history_table = dispatch_ctx.history_table;
							scope_index = dispatch_ctx.scope_index;
							exception_record->exception_flags |= EXCEPTION_COLLIDED_UNWIND;
							collided = true;
							break;
						default:
							RtlRaiseStatus(STATUS_INVALID_DISPOSITION);
					}

					if (!collided) {
						break;
					}
				}
			}

			if (reinterpret_cast<PVOID>(establisher_frame) != target_frame &&
			    handler_type == UNW_FLAG_UHANDLER) {
				*ctx = unwind_ctx;
			}
		}
		// leaf function
		else {
			unwind_ctx.rip = *reinterpret_cast<u64*>(unwind_ctx.rsp);
			unwind_ctx.rsp += 8;

			if (handler_type == UNW_FLAG_UHANDLER) {
				*ctx = unwind_ctx;
			}
		}

		if (establisher_frame % 8 == 0) {
			// todo check if stack limit out of bounds,
			// if it is check if it is within dpc stack bounds and if yes then do the following check
			// there (or if it is within stack bounds then do it too)

			if (reinterpret_cast<PVOID>(establisher_frame) != target_frame) {
				continue;
			}
			break;
		}

		if (reinterpret_cast<PVOID>(establisher_frame) != target_frame) {
			// todo raise the exception record as a non first chance exception with active context
			// if control pc != active_ctx->rip, else raise status STATUS_BAD_FUNCTION_TABLE
			panic("[kernel][except]: unaligned frame");
		}

		break;
	}

	ctx->rax = reinterpret_cast<u64>(return_value);
	if (exception_record->exception_code != STATUS_UNWIND_CONSOLIDATE) {
		ctx->rip = reinterpret_cast<ULONG64>(target_ip);
	}

	RtlRestoreContext(ctx, exception_record);
}

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
	UNWIND_HISTORY_TABLE* history_table) {

	RtlCaptureContext(ctx);

	EXCEPTION_RECORD unwind_record {};
	if (!exception_record) {
		unwind_record.exception_code = STATUS_UNWIND;
		unwind_record.exception_flags = EXCEPTION_UNWINDING;
		unwind_record.exception_addr = reinterpret_cast<PVOID>(ctx->rip);
		exception_record = &unwind_record;
	}

	exception_record->exception_flags = EXCEPTION_UNWINDING;

	if (!target_frame) {
		exception_record->exception_flags |= EXCEPTION_EXIT_UNWIND;
	}

	RtlUnwindExInternal(
		target_frame,
		target_ip,
		exception_record,
		return_value,
		ctx,
		history_table,
		UNW_FLAG_UHANDLER);
}

[[gnu::noinline, noreturn]] void RtlRaiseStatus(NTSTATUS status) {
	CONTEXT ctx {};
	RtlCaptureContext(&ctx);
	EXCEPTION_RECORD record {};
	record.exception_code = status;
	record.exception_flags = EXCEPTION_NONCONTINUABLE;
	record.exception_addr = reinterpret_cast<PVOID>(ctx.rip);
	RtlUnwindExInternal(nullptr, nullptr, &record, nullptr, &ctx, nullptr, UNW_FLAG_EHANDLER);
	panic("RtlUnwindExInternal returned");
}

[[gnu::noinline, noreturn]] void RtlRaiseException(EXCEPTION_RECORD* exception_record) {
	CONTEXT ctx {};
	RtlCaptureContext(&ctx);

	// skip the current frame to get the rip of the caller
	DWORD64 image_base;
	auto entry = RtlLookupFunctionEntry(
		ctx.rip,
		&image_base,
		nullptr);
	assert(entry);

	PVOID handler_data;
	DWORD64 establisher_frame;
	RtlVirtualUnwind(
		UNW_FLAG_NHANDLER,
		image_base,
		ctx.rip,
		entry,
		&ctx,
		&handler_data,
		&establisher_frame,
		nullptr);
	exception_record->exception_addr = reinterpret_cast<PVOID>(ctx.rip);
	RtlUnwindExInternal(nullptr, nullptr, exception_record, nullptr, &ctx, nullptr, UNW_FLAG_EHANDLER);
	panic("RtlUnwindExInternal returned");
}
