#include "ctx_offsets.h"

.section .text
.globl RtlCaptureContext
// void RtlCaptureContext(PCONTEXT ctx);
RtlCaptureContext:
.seh_proc RtlCaptureContext
	pushfq
	.seh_stackalloc 8
	.seh_endprologue
	mov [rcx + CONTEXT_RAX], rax
	mov rax, [rsp]
	mov [rcx + CONTEXT_EFLAGS], eax
	stmxcsr [rcx + CONTEXT_MXCSR]
	mov [rcx + CONTEXT_SEG_CS], cs
	mov [rcx + CONTEXT_SEG_DS], ds
	mov [rcx + CONTEXT_SEG_ES], es
	mov [rcx + CONTEXT_SEG_FS], fs
	mov [rcx + CONTEXT_SEG_GS], gs
	mov [rcx + CONTEXT_SEG_SS], ss
	mov [rcx + CONTEXT_RCX], rcx
	mov [rcx + CONTEXT_RDX], rdx
	mov [rcx + CONTEXT_RBX], rbx
	lea rax, [rsp + 16]
	mov [rcx + CONTEXT_RSP], rax
	mov [rcx + CONTEXT_RBP], rbp
	mov [rcx + CONTEXT_RSI], rsi
	mov [rcx + CONTEXT_RDI], rdi
	mov [rcx + CONTEXT_R8], r8
	mov [rcx + CONTEXT_R9], r9
	mov [rcx + CONTEXT_R10], r10
	mov [rcx + CONTEXT_R11], r11
	mov [rcx + CONTEXT_R12], r12
	mov [rcx + CONTEXT_R13], r13
	mov [rcx + CONTEXT_R14], r14
	mov [rcx + CONTEXT_R15], r15
	mov rax, [rsp + 8]
	mov [rcx + CONTEXT_RIP], rax
	fxsave64 [rcx + CONTEXT_FLOAT]
	add rsp, 8
	ret
.seh_endproc

.globl RtlRestoreContext
// void RtlRestoreContext(PCONTEXT ctx, EXCEPTION_RECORD* exception_record);
RtlRestoreContext:
.seh_proc RtlRestoreContext
	sub rsp, 40
	.seh_stackalloc 40
	.seh_endprologue

	cli

	// create irq frame
	movzx eax, word ptr [rcx + CONTEXT_SEG_SS]
	mov [rsp + 32], rax
	mov rax, [rcx + CONTEXT_RSP]
	mov [rsp + 24], rax
	mov eax, [rcx + CONTEXT_EFLAGS]
	mov [rsp + 16], rax
	movzx eax, word ptr [rcx + CONTEXT_SEG_CS]
	mov [rsp + 8], rax
	test eax, 3
	jz skip_gs
	swapgs

skip_gs:
	mov rax, [rcx + CONTEXT_RIP]
	mov [rsp], rax

	ldmxcsr [rcx + CONTEXT_MXCSR]
	/*
	mov [rcx + CONTEXT_SEG_DS], ds
	mov [rcx + CONTEXT_SEG_ES], es
	mov [rcx + CONTEXT_SEG_FS], fs
	mov [rcx + CONTEXT_SEG_GS], gs*/
	mov rax, [rcx + CONTEXT_RAX]
	mov rdx, [rcx + CONTEXT_RDX]
	mov rbx, [rcx + CONTEXT_RBX]
	mov rbp, [rcx + CONTEXT_RBP]
	mov rsi, [rcx + CONTEXT_RSI]
	mov rdi, [rcx + CONTEXT_RDI]
	mov r8, [rcx + CONTEXT_R8]
	mov r9, [rcx + CONTEXT_R9]
	mov r10, [rcx + CONTEXT_R10]
	mov r11, [rcx + CONTEXT_R11]
	mov r12, [rcx + CONTEXT_R12]
	mov r13, [rcx + CONTEXT_R13]
	mov r14, [rcx + CONTEXT_R14]
	mov r15, [rcx + CONTEXT_R15]
	fxrstor64 [rcx + CONTEXT_FLOAT]

	mov rcx, [rcx + CONTEXT_RCX]

	iretq
.seh_endproc

.section .drectve, "yni"
.ascii " /EXPORT:RtlCaptureContext"
