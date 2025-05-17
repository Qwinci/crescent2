#pragma once

#ifdef __x86_64__

#define __DO_SYSCALL_EXPAND(num) #num
#define DO_SYSCALL0(num) asm volatile("mov $" #num ", %rax; syscall; ret")
#define DO_SYSCALL(num) asm volatile("mov $" __DO_SYSCALL_EXPAND(num) ", %rax; mov %rcx, %r10; syscall; ret")

#else
#error unsupported architecture
#endif
