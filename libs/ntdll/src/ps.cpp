#include "ntpsapi.h"
#include "winnt.h"
#include "syscall.hpp"
#include "syscalls.hpp"

NTAPI NTSTATUS NtContinue(PCONTEXT ThreadContext, BOOLEAN TestAlert) {
	DO_SYSCALL(SYS_CONTINUE);
}
