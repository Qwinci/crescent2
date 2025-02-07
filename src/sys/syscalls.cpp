#include "arch/arch_syscall.hpp"
#include "stdio.hpp"

extern "C" void syscall_handler(SyscallFrame* frame) {
	panic("syscall ", frame->num());
}
