#include "arch/arch_irq.hpp"

void trap_frame_to_context(
	KTRAP_FRAME* trap_frame,
	KEXCEPTION_FRAME* exception_frame,
	CONTEXT* ctx) {
	auto flags = ctx->context_flags;

	if (flags & CONTEXT_CONTROL) {
		ctx->seg_cs = trap_frame->seg_cs;
		ctx->seg_ss = trap_frame->seg_ss;
		ctx->rip = trap_frame->rip;
		ctx->rsp = trap_frame->rsp;
		ctx->eflags = trap_frame->eflags;
	}

	if (flags & CONTEXT_INTEGER) {
		ctx->rax = trap_frame->rax;
		ctx->rcx = trap_frame->rcx;
		ctx->rdx = trap_frame->rdx;
		ctx->rbp = trap_frame->rbp;
		ctx->r8 = trap_frame->r8;
		ctx->r9 = trap_frame->r9;
		ctx->r10 = trap_frame->r10;
		ctx->r11 = trap_frame->r11;

		if (exception_frame) {
			ctx->rbx = exception_frame->rbx;
			ctx->rsi = exception_frame->rsi;
			ctx->rdi = exception_frame->rdi;
			ctx->r12 = exception_frame->r12;
			ctx->r13 = exception_frame->r13;
			ctx->r14 = exception_frame->r14;
			ctx->r15 = exception_frame->r15;
		}
	}

	if (flags & CONTEXT_SEGMENTS) {
		ctx->seg_ds = trap_frame->seg_ds;
		ctx->seg_es = trap_frame->seg_es;
		ctx->seg_fs = trap_frame->seg_fs;
		ctx->seg_gs = trap_frame->seg_gs;
	}

	if (flags & CONTEXT_FLOATING_POINT) {
		ctx->mx_csr = trap_frame->mx_csr;
		ctx->xmm[0] = trap_frame->xmm0;
		ctx->xmm[1] = trap_frame->xmm1;
		ctx->xmm[2] = trap_frame->xmm2;
		ctx->xmm[3] = trap_frame->xmm3;
		ctx->xmm[4] = trap_frame->xmm4;
		ctx->xmm[5] = trap_frame->xmm5;

		if (exception_frame) {
			ctx->xmm[6] = exception_frame->xmm6;
			ctx->xmm[7] = exception_frame->xmm7;
			ctx->xmm[8] = exception_frame->xmm8;
			ctx->xmm[9] = exception_frame->xmm9;
			ctx->xmm[10] = exception_frame->xmm10;
			ctx->xmm[11] = exception_frame->xmm11;
			ctx->xmm[12] = exception_frame->xmm12;
			ctx->xmm[13] = exception_frame->xmm13;
			ctx->xmm[14] = exception_frame->xmm14;
			ctx->xmm[15] = exception_frame->xmm15;
		}
	}
}
