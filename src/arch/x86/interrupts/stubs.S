.cfi_sections .debug_frame

.macro stub i handler

irq\i:
.cfi_startproc
.cfi_signal_frame
.cfi_def_cfa %rsp, 0
.cfi_offset %rip, 0
.cfi_offset %rsp, 24

.if \i != 8 && \i != 10 && \i != 11 && \i != 12 && \i != 13 && \i != 14 && \i != 17 && \i != 21 && \i != 29 && \i != 30
	push $0
.endif
.cfi_adjust_cfa_offset 8

cld

push %rax
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %rax, 0
push %rbx
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %rbx, 0
push %rcx
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %rcx, 0
push %rdx
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %rdx, 0
push %rdi
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %rdi, 0
push %rsi
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %rsi, 0
push %rbp
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %rbp, 0
push %r8
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %r8, 0
push %r9
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %r9, 0
push %r10
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %r10, 0
push %r11
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %r11, 0
push %r12
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %r12, 0
push %r13
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %r13, 0
push %r14
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %r14, 0
push %r15
.cfi_adjust_cfa_offset 8
.cfi_rel_offset %r15, 0

mov %rsp, %rcx

pushq 16 * 8(%rsp)
.cfi_adjust_cfa_offset 8
push %rbp
.cfi_adjust_cfa_offset 8
mov %rsp, %rbp

mov $\i, %edx

.cfi_def_cfa_register %rbp

and $~15, %rsp

sub $(7 * 16), %rsp
movaps %xmm0, (%rsp)
movaps %xmm1, 16(%rsp)
movaps %xmm2, 32(%rsp)
movaps %xmm3, 48(%rsp)
movaps %xmm4, 64(%rsp)
movaps %xmm5, 80(%rsp)
stmxcsr 96(%rsp)

sub $32, %rsp

call \handler

add $32, %rsp
movaps (%rsp), %xmm0
movaps 16(%rsp), %xmm1
movaps 32(%rsp), %xmm2
movaps 48(%rsp), %xmm3
movaps 64(%rsp), %xmm4
movaps 80(%rsp), %xmm5
ldmxcsr 96(%rsp)

mov %rbp, %rsp
.cfi_def_cfa_register %rsp

add $16, %rsp
.cfi_adjust_cfa_offset -16

pop %r15
.cfi_adjust_cfa_offset -8
.cfi_restore %r15
pop %r14
.cfi_adjust_cfa_offset -8
.cfi_restore %r14
pop %r13
.cfi_adjust_cfa_offset -8
.cfi_restore %r13
pop %r12
.cfi_adjust_cfa_offset -8
.cfi_restore %r12
pop %r11
.cfi_adjust_cfa_offset -8
.cfi_restore %r11
pop %r10
.cfi_adjust_cfa_offset -8
.cfi_restore %r10
pop %r9
.cfi_adjust_cfa_offset -8
.cfi_restore %r9
pop %r8
.cfi_adjust_cfa_offset -8
.cfi_restore %r8
pop %rbp
.cfi_adjust_cfa_offset -8
.cfi_restore %rbp
pop %rsi
.cfi_adjust_cfa_offset -8
.cfi_restore %rsi
pop %rdi
.cfi_adjust_cfa_offset -8
.cfi_restore %rdi
pop %rdx
.cfi_adjust_cfa_offset -8
.cfi_restore %rdx
pop %rcx
.cfi_adjust_cfa_offset -8
.cfi_restore %rcx
pop %rbx
.cfi_adjust_cfa_offset -8
.cfi_restore %rbx
pop %rax
.cfi_adjust_cfa_offset -8
.cfi_restore %rax

add $8, %rsp
.cfi_adjust_cfa_offset -8
iretq

.cfi_endproc
.endm

.altmacro
stub 0, x86_div_handler
stub 1, x86_debug_handler
stub 2, x86_nmi_handler
stub 3, x86_breakpoint_handler
stub 4, x86_overflow_handler
stub 5, x86_bound_range_exceeded_handler
stub 6, x86_invalid_op_handler
stub 7, x86_device_not_available_handler
stub 8, x86_double_fault_handler
stub 10, x86_invalid_tss_handler
stub 11, x86_seg_not_present_handler
stub 12, x86_stack_seg_fault_handler
stub 13, x86_gp_fault_handler
stub 14, x86_pagefault_handler
stub 16, x86_x87_floating_point_handler
stub 17, x86_alignment_check_handler
stub 18, x86_machine_check_handler
stub 19, x86_simd_exception_handler
stub 20, x86_virt_exception_handler
stub 21, x86_control_protection_exception_handler
stub 28, x86_hypervisor_injection_exception_handler
stub 29, x86_vmm_comm_exception_handler
stub 30, x86_security_exception_handler

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
stub %i arch_irq_handler
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
