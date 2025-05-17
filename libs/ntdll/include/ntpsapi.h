#ifndef _NTPSAPI_H
#define _NTPSAPI_H

#include "ntapi.h"
#include "winnt.h"

#define NtCurrentProcess() ((HANDLE) (LONG_PTR) -1)

NTAPI NTSTATUS NtContinue(PCONTEXT ThreadContext, BOOLEAN TestAlert);

#endif
