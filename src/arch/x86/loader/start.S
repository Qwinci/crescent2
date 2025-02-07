.local STACK
.comm STACK, 0x8000, 16

.cfi_sections .debug_frame

.section .text
.globl _start
.hidden _start
.type _start, @function
_start:
	lea STACK + 0x8000 - 40(%rip), %rsp

	// clear EM
	mov %cr0, %rax
	and $~(1 << 2), %rax
	// set MP
	or $(1 << 1), %rax
	mov %rax, %cr0

	// set OSXMMEXCPT and OSFXSR
	mov %cr4, %rax
	or $(1 << 10 | 1 << 9), %rax
	mov %rax, %cr4

	jmp early_start
