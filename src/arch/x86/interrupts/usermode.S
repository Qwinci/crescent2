.globl x86_syscall_stub
.hidden x86_syscall_stub
.type x86_syscall_stub, @function

// num == rax
// a0 == rdx
// a1 == r8
// a2 == r9
// a3 == r10
// a3 == rdi
// a4 == rsi

x86_syscall_stub:
	swapgs

	mov %rax, %gs:16
	mov %gs:0x188, %rax

	mov %rsp, 24(%rax)
	mov 16(%rax), %rsp

	mov %gs:16, %rax

	cld
	sti

	sub $(80 + 6 * 16), %rsp
	mov %rax, 0(%rsp)
	mov %rcx, 8(%rsp)
	mov %rdx, 16(%rsp)
	mov %r8, 24(%rsp)
	mov %r9, 32(%rsp)
	mov %r10, 40(%rsp)
	mov %r11, 48(%rsp)
	mov %rdi, 56(%rsp)
	mov %rsi, 64(%rsp)
	stmxcsr 72(%rsp)
	movaps %xmm0, 80(%rsp)
	movaps %xmm1, 96(%rsp)
	movaps %xmm2, 112(%rsp)
	movaps %xmm3, 128(%rsp)
	movaps %xmm4, 144(%rsp)
	movaps %xmm5, 160(%rsp)

	mov %rsp, %rcx
	call syscall_handler

	mov 0(%rsp), %rax
	mov 8(%rsp), %rcx
	mov 16(%rsp), %rdx
	mov 24(%rsp), %r8
	mov 32(%rsp), %r9
	mov 40(%rsp), %r10
	mov 48(%rsp), %r11
	mov 56(%rsp), %rdi
	mov 64(%rsp), %rsi
	ldmxcsr 72(%rsp)
	movaps 80(%rsp), %xmm0
	movaps 96(%rsp), %xmm1
	movaps 112(%rsp), %xmm2
	movaps 128(%rsp), %xmm3
	movaps 144(%rsp), %xmm4
	movaps 160(%rsp), %xmm5

	add $(80 + 6 * 16), %rsp

	cli

	mov %rax, %gs:16
	mov %gs:0x188, %rax
	mov 24(%rax), %rsp
	mov %gs:16, %rax

	swapgs
	sysretq

.section .note.GNU-stack
