.macro def_common_handler handler is_exception

.seh_pushframe @code

// stack is aligned here, the cpu makes sure that it is aligned before pushing registers.
// the misalign if no error code is pushed is fixed by the stub pushing a dummy code.

cld

.if \is_exception == 0
sub rsp, 352
.set trap_frame_offset, 0
.seh_stackalloc 352
.else
sub rsp, 352 + 320
.set trap_frame_offset, 320
.seh_stackalloc 352 + 320
.endif
.seh_endprologue

stmxcsr [rsp + trap_frame_offset + 44]
mov [rsp + trap_frame_offset + 48], rax
mov [rsp + trap_frame_offset + 56], rcx
mov [rsp + trap_frame_offset + 64], rdx
mov [rsp + trap_frame_offset + 72], r8
mov [rsp + trap_frame_offset + 80], r9
mov [rsp + trap_frame_offset + 88], r10
mov [rsp + trap_frame_offset + 96], r11
mov [rsp + trap_frame_offset + 344], rbp

movaps [rsp + trap_frame_offset + 112], xmm0
movaps [rsp + trap_frame_offset + 128], xmm1
movaps [rsp + trap_frame_offset + 144], xmm2
movaps [rsp + trap_frame_offset + 160], xmm3
movaps [rsp + trap_frame_offset + 176], xmm4
movaps [rsp + trap_frame_offset + 192], xmm5

mov ax, [rsp + trap_frame_offset + 368]
and ax, 1
mov [rsp + trap_frame_offset + 40], al

.if \is_exception == 1
// create KEXCEPTION_FRAME
movaps [rsp + 48], xmm6
movaps [rsp + 64], xmm7
movaps [rsp + 80], xmm8
movaps [rsp + 96], xmm9
movaps [rsp + 112], xmm10
movaps [rsp + 128], xmm11
movaps [rsp + 144], xmm12
movaps [rsp + 160], xmm13
movaps [rsp + 176], xmm14
movaps [rsp + 192], xmm15

mov [rsp + 256], rbx
mov [rsp + 264], rdi
mov [rsp + 272], rsi
mov [rsp + 280], r12
mov [rsp + 288], r13
mov [rsp + 296], r14
mov [rsp + 304], r15

lea rax, [rsp + 320]
mov [rsp + 208], rax
.endif

mov rcx, rsp
call \handler

ldmxcsr [rsp + 44]
mov rax, [rsp + 48]
mov rcx, [rsp + 56]
mov rdx, [rsp + 64]
mov r8, [rsp + 72]
mov r9, [rsp + 80]
mov r10, [rsp + 88]
mov r11, [rsp + 96]
mov rbp, [rsp + 344]

movaps xmm0, [rsp + 112]
movaps xmm1, [rsp + 128]
movaps xmm2, [rsp + 144]
movaps xmm3, [rsp + 160]
movaps xmm4, [rsp + 176]
movaps xmm5, [rsp + 192]

.if \is_exception == 0
add rsp, 360
.else
add rsp, 360 + 320
.endif

iretq

.seh_endproc
.endm

common_irq_dispatch:
.seh_proc common_irq_dispatch
def_common_handler arch_irq_handler 0

.macro stub i handler
irq\i:
// the error code is the number in case of interrupts
push \i
jmp \handler
.endm

.macro exc_stub i handler
irq\i:
.if \i != 8 && \i != 10 && \i != 11 && \i != 12 && \i != 13 && \i != 14 && \i != 17 && \i != 21 && \i != 29 && \i != 30
	// the error code is the number in case of interrupts/exceptions without one
	push \i
.endif
.seh_proc \handler
def_common_handler \handler 1
.endm

.altmacro
exc_stub 0, x86_div_handler
exc_stub 1, x86_debug_handler
exc_stub 2, x86_nmi_handler
exc_stub 3, x86_breakpoint_handler
exc_stub 4, x86_overflow_handler
exc_stub 5, x86_bound_range_exceeded_handler
exc_stub 6, x86_invalid_op_handler
exc_stub 7, x86_device_not_available_handler
exc_stub 8, x86_double_fault_handler
exc_stub 10, x86_invalid_tss_handler
exc_stub 11, x86_seg_not_present_handler
exc_stub 12, x86_stack_seg_fault_handler
exc_stub 13, x86_gp_fault_handler
exc_stub 14, x86_pagefault_handler
exc_stub 16, x86_x87_floating_point_handler
exc_stub 17, x86_alignment_check_handler
exc_stub 18, x86_machine_check_handler
exc_stub 19, x86_simd_exception_handler
exc_stub 20, x86_virt_exception_handler
exc_stub 21, x86_control_protection_exception_handler
exc_stub 28, x86_hypervisor_injection_exception_handler
exc_stub 29, x86_vmm_comm_exception_handler
exc_stub 30, x86_security_exception_handler

irq9:
irq15:
irq22:
irq23:
irq24:
irq25:
irq26:
irq27:
irq31:

.set i, 32
.rept 224
stub %i common_irq_dispatch
.set i, i + 1
.endr

.macro label i
	.quad irq\i
.endm

.section .data
.globl X86_IRQ_STUBS
.globl X86_EXC_STUBS
.align 16
X86_EXC_STUBS:
.set i, 0
.rept 32
label %i
.set i, i + 1
.endr

.align 16
X86_IRQ_STUBS:
.set i, 32
.rept 224
label %i
.set i, i + 1
.endr
