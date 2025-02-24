.lcomm STACK, 0x8000, 16

.section .text
.globl _start
.def _start
.type 32
.endef
_start:
	lea rsp, [rip + STACK + 0x8000 - 40]

	mov rax, cr0
	// clear EM
	and rax, ~(1 << 2)
	// set MP
	or rax, 1 << 1
	mov cr0, rax

	// set OSXMMEXCPT and OSFXSR
	mov rax, cr4
	or rax, (1 << 10 | 1 << 9)
	mov cr4, rax

	jmp early_start
