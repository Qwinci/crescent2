.globl x86_syscall_stub
.def x86_syscall_stub
.type 32
.endef

// num == rax
// a0 == r10
// a1 == rdx
// a2 == r8
// a3 == r9

x86_syscall_stub:
.seh_proc x86_syscall_stub

	swapgs

	mov gs:16, rsp
	mov rsp, gs:44

	cld
	sti

	sub rsp, (80 + 6 * 16)
	.seh_stackalloc 80 + 6 * 16
	.seh_endprologue

	mov [rsp + 0], rax
	mov [rsp + 8], rcx
	mov [rsp + 16], rdx
	mov [rsp + 24], r8
	mov [rsp + 32], r9
	mov [rsp + 40], r10
	mov [rsp + 48], r11
	mov rax, gs:16
	mov [rsp + 56], rax
	stmxcsr [rsp + 72]
	movaps [rsp + 80], xmm0
	movaps [rsp + 96], xmm1
	movaps [rsp + 112], xmm2
	movaps [rsp + 128], xmm3
	movaps [rsp + 144], xmm4
	movaps [rsp + 160], xmm5

	mov rcx, rsp

	// todo fix seh here
	sub rsp, 32
	call syscall_handler
	add rsp, 32

	mov rax, [rsp + 0]
	mov rcx, [rsp + 8]
	mov rdx, [rsp + 16]
	mov r8, [rsp + 24]
	mov r9, [rsp + 32]
	mov r10, [rsp + 40]
	mov r11, [rsp + 48]
	ldmxcsr [rsp + 72]
	movaps xmm0, [rsp + 80]
	movaps xmm1, [rsp + 96]
	movaps xmm2, [rsp + 112]
	movaps xmm3, [rsp + 128]
	movaps xmm4, [rsp + 144]
	movaps xmm5, [rsp + 160]

	add rsp, (80 + 6 * 16)

	cli

	mov rsp, gs:16

	swapgs
	sysretq
.seh_endproc

.section ".note.GNU-stack"
