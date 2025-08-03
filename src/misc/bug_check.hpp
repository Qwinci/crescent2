#pragma once

#include "ntdef.h"

enum KBUGCHECK_CALLBACK_REASON {
	KbCallbackInvalid,
	KbCallbackReserved1,
	KbCallbackSecondaryDumpData,
	KbCallbackDumpIo,
	KbCallbackAddPages,
	KbCallbackSecondaryMultiPartDumpData,
	KbCallbackRemovePages,
	KbCallbackTriageDumpData,
	KbCallbackReserved2
};

struct KBUGCHECK_REASON_CALLBACK_RECORD;

using PKBUGCHECK_REASON_CALLBACK_ROUTINE = void (*)(
	KBUGCHECK_CALLBACK_REASON reason,
	KBUGCHECK_REASON_CALLBACK_RECORD* record,
	PVOID reason_specific_data,
	ULONG reason_specific_data_length);

struct KBUGCHECK_REASON_CALLBACK_RECORD {
	LIST_ENTRY entry;
	PKBUGCHECK_REASON_CALLBACK_ROUTINE callback_routine;
	PUCHAR component;
	ULONG_PTR checksum;
	KBUGCHECK_CALLBACK_REASON reason;
	UCHAR state;
};

enum KBUGCHECK_BUFFER_DUMP_STATE {
	BufferEmpty,
	BufferInserted,
	BufferStarted,
	BufferFinished,
	BufferIncomplete
};

#define IRQL_NOT_LESS_OR_EQUAL 0xA

NTAPI extern "C" BOOLEAN KeRegisterBugCheckReasonCallback(
	KBUGCHECK_REASON_CALLBACK_RECORD* callback_record,
	PKBUGCHECK_REASON_CALLBACK_ROUTINE callback_routine,
	KBUGCHECK_CALLBACK_REASON reason,
	PUCHAR component);
NTAPI extern "C" BOOLEAN KeDeregisterBugCheckReasonCallback(
	KBUGCHECK_REASON_CALLBACK_RECORD* callback_record);

NTAPI extern "C" [[noreturn]] void KeBugCheckEx(
	ULONG bug_check_code,
	ULONG_PTR bug_check_param1,
	ULONG_PTR bug_check_param2,
	ULONG_PTR bug_check_param3,
	ULONG_PTR bug_check_param4);
