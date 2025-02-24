#pragma once

#include "ntdef.h"

#define EXCEPTION_CONTINUE_EXECUTION (-1)
#define EXCEPTION_CONTINUE_SEARCH 0
#define EXCEPTION_EXECUTE_HANDLER 1

#define EXCEPTION_MAXIMUM_PARAMETERS 15

struct EXCEPTION_RECORD {
	NTSTATUS exception_code;
	ULONG exception_flags;
	EXCEPTION_RECORD* exception_record;
	PVOID exception_addr;
	ULONG num_of_params;
	ULONG_PTR exception_info[EXCEPTION_MAXIMUM_PARAMETERS];
};

#define EXCEPTION_NONCONTINUABLE 0x1
#define EXCEPTION_UNWINDING 0x2
#define EXCEPTION_EXIT_UNWIND 0x4
#define EXCEPTION_STACK_INVALID 0x8
#define EXCEPTION_NESTED_CALL 0x10
#define EXCEPTION_TARGET_UNWIND 0x20
#define EXCEPTION_COLLIDED_UNWIND 0x40
#define EXCEPTION_SOFTWARE_ORIGINATE 0x80
#define EXCEPTION_UNWIND \
	(EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND | EXCEPTION_TARGET_UNWIND | EXCEPTION_COLLIDED_UNWIND)

struct CONTEXT;

struct EXCEPTION_POINTERS {
	EXCEPTION_RECORD* exception_record;
	CONTEXT* ctx;
};

extern "C" unsigned long _exception_code();
extern "C" void* _exception_info();
extern "C" int _abnormal_termination();

#define GetExceptionCode _exception_code
#define GetExceptionInformation() ((EXCEPTION_POINTERS*) _exception_info())
#define AbnormalTermination _abnormal_termination

[[gnu::noinline, noreturn]] void RtlRaiseStatus(NTSTATUS status);
[[gnu::noinline, noreturn]] void RtlRaiseException(EXCEPTION_RECORD* exception_record);

extern "C" NTAPI void ExRaiseDatatypeMisalignment();
extern "C" NTAPI void ExRaiseAccessViolation();
