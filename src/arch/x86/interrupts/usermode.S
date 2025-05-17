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

	sub rsp, 400
	.seh_stackalloc 400
	.seh_endprologue

	sti
	mov [rsp + 48], rax
	mov [rsp + 56], rcx
	mov [rsp + 64], rdx
	mov [rsp + 72], r8
	mov [rsp + 80], r9
	mov [rsp + 88], r10
	mov [rsp + 96], r11
	mov rax, gs:16
	mov [rsp + 384], rax
	stmxcsr [rsp + 44]
	movaps [rsp + 112], xmm0
	movaps [rsp + 128], xmm1
	movaps [rsp + 144], xmm2
	movaps [rsp + 160], xmm3
	movaps [rsp + 176], xmm4
	movaps [rsp + 192], xmm5

	mov rcx, rsp

	call syscall_handler

	mov rax, [rsp + 48]
	mov rcx, [rsp + 56]
	mov rdx, [rsp + 64]
	mov r8, [rsp + 72]
	mov r9, [rsp + 80]
	mov r10, [rsp + 88]
	mov r11, [rsp + 96]
	ldmxcsr [rsp + 44]
	movaps xmm0, [rsp + 112]
	movaps xmm1, [rsp + 128]
	movaps xmm2, [rsp + 144]
	movaps xmm3, [rsp + 160]
	movaps xmm4, [rsp + 176]
	movaps xmm5, [rsp + 192]

	cli

	mov rsp, gs:16
	swapgs
	sysretq
.seh_endproc

.globl x86_syscall_stub
.def x86_syscall_stub
.type 32
.endef

.section ".note.GNU-stack"
