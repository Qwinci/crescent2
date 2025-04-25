#pragma once

#include "ntdef.h"
#include "sched/thread.hpp"

struct alignas(16) M128A {
	ULONGLONG low;
	LONGLONG high;
};

struct alignas(16) XMM_SAVE_AREA32 {
	USHORT control_word;
	USHORT status_word;
	UCHAR tag_word;
	UCHAR reserved1;
	USHORT error_opcode;
	ULONG error_offset;
	USHORT error_selector;
	USHORT reserve2;
	ULONG data_offset;
	USHORT data_selector;
	USHORT reserved3;
	ULONG mx_csr;
	ULONG mx_csr_mask;
	M128A float_registers[8];
	M128A xmm_registers[16];
	UCHAR reserved4[96];
};

// CONTEXT_CONTROL = seg_ss, rsp, seg_cs, rip, eflags
// CONTEXT_INTEGER = rax, rcx, rdx, rbx, rbp, rsi, rdi, r8-r15
// CONTEXT_SEGMENTS = seg_ds, seg_es, seg_fs, seg_gs
// CONTEXT_FLOATING_POINT = xmm0-xmm15
// CONTEXT_DEBUG_REGISTERS = dr0-dr3, dr6-dr7

#define CONTEXT_AMD64 0x100000
#define CONTEXT_CONTROL (CONTEXT_AMD64 | 1)
#define CONTEXT_INTEGER (CONTEXT_AMD64 | 2)
#define CONTEXT_SEGMENTS (CONTEXT_AMD64 | 4)
#define CONTEXT_FLOATING_POINT (CONTEXT_AMD64 | 8)
#define CONTEXT_DEBUG_REGISTERS (CONTEXT_AMD64 | 0x10)
#define CONTEXT_FULL (CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_FLOATING_POINT)
#define CONTEXT_ALL (CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS | \
	CONTEXT_FLOATING_POINT | CONTEXT_DEBUG_REGISTERS)
#define CONTEXT_XSTATE (CONTEXT_AMD64 | 0x40)
#define CONTEXT_KERNEL_CET (CONTEXT_AMD64 | 0x80)

struct alignas(16) CONTEXT {
	ULONG64 p1_home;
	ULONG64 p2_home;
	ULONG64 p3_home;
	ULONG64 p4_home;
	ULONG64 p5_home;
	ULONG64 p6_home;
	ULONG context_flags;
	ULONG mx_csr;
	USHORT seg_cs;
	USHORT seg_ds;
	USHORT seg_es;
	USHORT seg_fs;
	USHORT seg_gs;
	USHORT seg_ss;
	ULONG eflags;
	ULONG64 dr0;
	ULONG64 dr1;
	ULONG64 dr2;
	ULONG64 dr3;
	ULONG64 dr6;
	ULONG64 dr7;
	ULONG64 rax;
	ULONG64 rcx;
	ULONG64 rdx;
	ULONG64 rbx;
	ULONG64 rsp;
	ULONG64 rbp;
	ULONG64 rsi;
	ULONG64 rdi;
	ULONG64 r8;
	ULONG64 r9;
	ULONG64 r10;
	ULONG64 r11;
	ULONG64 r12;
	ULONG64 r13;
	ULONG64 r14;
	ULONG64 r15;
	ULONG64 rip;
	union {
		XMM_SAVE_AREA32 flt_save;
		struct {
			M128A header[2];
			M128A legacy[8];
			M128A xmm[16];
		};
	};
	M128A vector_register[26];
	ULONG64 vector_control;
	ULONG64 debug_control;
	ULONG64 last_branch_to_rip;
	ULONG64 last_branch_from_rip;
	ULONG64 last_exception_to_rip;
	ULONG64 last_exception_from_rip;
};

struct KEXCEPTION_FRAME {
	ULONG64 p1_home;
	ULONG64 p2_home;
	ULONG64 p3_home;
	ULONG64 p4_home;
	ULONG64 p5;
	ULONG64 spare1;

	// nonvolatile float registers
	M128A xmm6;
	M128A xmm7;
	M128A xmm8;
	M128A xmm9;
	M128A xmm10;
	M128A xmm11;
	M128A xmm12;
	M128A xmm13;
	M128A xmm14;
	M128A xmm15;

	ULONG64 trap_frame;
	ULONG64 output_buffer;
	ULONG64 output_length;
	ULONG64 spare2;

	ULONG64 mx_csr;

	ULONG64 rbp;
	ULONG64 rbx;
	ULONG64 rdi;
	ULONG64 rsi;
	ULONG64 r12;
	ULONG64 r13;
	ULONG64 r14;
	ULONG64 r15;
	ULONG64 return_addr;
};

struct KTRAP_FRAME {
	ULONG64 p1_home;
	ULONG64 p2_home;
	ULONG64 p3_home;
	ULONG64 p4_home;
	ULONG64 p5;

	KPROCESSOR_MODE previous_mode;

	// only for interrupts
	KIRQL previous_irql;

	// page fault load/store indicator
	UCHAR fault_indicator;

	// 0 == interrupt frame, 1 == exception frame, 2 == syscall frame
	UCHAR exception_active;

	ULONG mx_csr;

	// volatile registers (only saved for exceptions/interrupts)
	ULONG64 rax;
	ULONG64 rcx;
	ULONG64 rdx;
	ULONG64 r8;
	ULONG64 r9;
	ULONG64 r10;
	ULONG64 r11;

	union {
		ULONG64 gs_base;
		ULONG64 gs_swap;
	};

	// volatile float registers (saved on exceptions/interrupts, we also save them for syscalls)
	M128A xmm0;
	M128A xmm1;
	M128A xmm2;
	M128A xmm3;
	M128A xmm4;
	M128A xmm5;

	union {
		ULONG64 fault_address;
		ULONG64 context_record;
	};

	union {
		struct {
			ULONG64 dr0;
			ULONG64 dr1;
			ULONG64 dr2;
			ULONG64 dr3;
			ULONG64 dr6;
			ULONG64 dr7;
		};
		struct {
			ULONG64 shadow_stack_frame;
			ULONG64 spare[5];
		};
	};

	struct {
		ULONG64 debug_control;
		ULONG64 last_branch_to_rip;
		ULONG64 last_branch_from_rip;
		ULONG64 last_exception_to_rip;
		ULONG64 last_exception_from_rip;
	};

	USHORT seg_ds;
	USHORT seg_es;
	USHORT seg_fs;
	USHORT seg_gs;

	// previous trap frame address
	ULONG64 trap_frame;

	// nonvolatile registers (only saved for syscalls)
	ULONG64 rbx;
	ULONG64 rdi;
	ULONG64 rsi;

	ULONG64 rbp;

	union {
		ULONG64 error_code;
		ULONG64 exception_frame;
	};
	ULONG64 rip;
	USHORT seg_cs;
	UCHAR fill0;
	UCHAR logging;
	USHORT fill1[2];
	ULONG eflags;
	ULONG fill2;
	ULONG64 rsp;
	USHORT seg_ss;
	USHORT fill3;
	ULONG fill4;
};

struct KNONVOLATILE_CONTEXT_POINTERS {
	M128A* float_context[16];
	PDWORD64 integer_context[16];
};
