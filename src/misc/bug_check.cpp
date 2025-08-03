#include "bug_check.hpp"
#include "utils/spinlock.hpp"
#include "dev/fb.hpp"
#include "hz/array.hpp"

namespace {
	KSPIN_LOCK LOCK {};
	LIST_ENTRY LIST {.Flink = &LIST, .Blink = &LIST};
}

NTAPI BOOLEAN KeRegisterBugCheckReasonCallback(
	KBUGCHECK_REASON_CALLBACK_RECORD* callback_record,
	PKBUGCHECK_REASON_CALLBACK_ROUTINE callback_routine,
	KBUGCHECK_CALLBACK_REASON reason,
	PUCHAR component) {
	if (callback_record->state != BufferEmpty) {
		return false;
	}

	callback_record->state = BufferInserted;
	callback_record->callback_routine = callback_routine;
	callback_record->component = component;
	callback_record->reason = reason;

	auto old = KeAcquireSpinLockRaiseToDpc(&LOCK);
	InsertTailList(&LIST, &callback_record->entry);
	KeReleaseSpinLock(&LOCK, old);

	return true;
}

NTAPI BOOLEAN KeDeregisterBugCheckReasonCallback(
	KBUGCHECK_REASON_CALLBACK_RECORD* callback_record) {
	if (callback_record->state == BufferEmpty) {
		return false;
	}

	auto old = KeAcquireSpinLockRaiseToDpc(&LOCK);
	RemoveEntryList(&callback_record->entry);
	KeReleaseSpinLock(&LOCK, old);

	callback_record->state = BufferEmpty;

	return true;
}

static constexpr hz::string_view IRQL_STRS[] {
	"PASSIVE_LEVEL",
	"APC_LEVEL",
	"DISPATCH_LEVEL",
	"DEVICE_LEVEL",
	"DEVICE_LEVEL",
	"DEVICE_LEVEL",
	"DEVICE_LEVEL",
	"DEVICE_LEVEL",
	"DEVICE_LEVEL",
	"DEVICE_LEVEL",
	"DEVICE_LEVEL",
	"DEVICE_LEVEL",
	"DEVICE_LEVEL",
	"CLOCK_LEVEL",
	"IPI_LEVEL",
	"HIGH_LEVEL"
};

[[noreturn]] NTAPI void KeBugCheckEx(
	ULONG bug_check_code,
	ULONG_PTR bug_check_param1,
	ULONG_PTR bug_check_param2,
	ULONG_PTR bug_check_param3,
	ULONG_PTR bug_check_param4) {
	print("KERNEL PANIC: bug check with code ");

	switch (bug_check_code) {
		case IRQL_NOT_LESS_OR_EQUAL:
			println("IRQL_NOT_LESS_OR_EQUAL [0xa]");
			println(
				"irql at the time of fault: ",
				IRQL_STRS[bug_check_param2],
				" [0x", Fmt::Hex, bug_check_param2, Fmt::Reset);
			break;
		default:
			println("UNKNOWN_BUG_CHECK_CODE [0x", Fmt::Hex, bug_check_code, Fmt::Reset, "]");
	}

	panic("system halted");
}
