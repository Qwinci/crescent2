#include "user_access.hpp"
#include "types.hpp"
#include "utils/except.hpp"

NTAPI extern "C" usize MmUserProbeAddress = 0x7FFFFFFFE000;

NTAPI void ProbeForRead(const volatile void* addr, SIZE_T length, ULONG alignment) {
	if (length) {
		auto ptr = reinterpret_cast<usize>(addr);
		if (ptr % alignment) {
			ExRaiseDatatypeMisalignment();
		}
		else if (ptr >= MmUserProbeAddress || length > MmUserProbeAddress - ptr) {
			ExRaiseAccessViolation();
		}
	}
}

NTAPI void ProbeForWrite(volatile void* addr, SIZE_T length, ULONG alignment) {
	if (length) {
		auto ptr = reinterpret_cast<usize>(addr);
		if (ptr % alignment) {
			ExRaiseDatatypeMisalignment();
		}
		else if (ptr >= MmUserProbeAddress || length > MmUserProbeAddress - ptr) {
			ExRaiseAccessViolation();
		}

		for (usize i = 0; i < length; i += 0x1000) {
			static_cast<volatile char*>(addr)[i] = 0;
		}

		static_cast<volatile char*>(addr)[length - 1] = 0;
	}
}
