#pragma once
#include "types.hpp"
#include "arch/arch_irq.hpp"

struct SyscallFrame : KTRAP_FRAME {
	[[nodiscard]] constexpr u64 num() const {
		return rax;
	}

	constexpr u64* ret() {
		return &rax;
	}

	constexpr u64* arg0() {
		return &r10;
	}

	[[nodiscard]] constexpr u64 arg1() const {
		return rdx;
	}

	[[nodiscard]] constexpr u64 arg2() const {
		return r8;
	}

	[[nodiscard]] constexpr u64 arg3() const {
		return r9;
	}

	[[nodiscard]] inline bool arg4(u64& value) const {
		__try {
			value = *reinterpret_cast<usize*>(rsp + 40);
			return true;
		}
		__except (1) {
			return false;
		}
	}

	[[nodiscard]] inline bool arg5(u64& value) const {
		__try {
			value = *reinterpret_cast<usize*>(rsp + 48);
			return true;
		}
		__except (1) {
			return false;
		}
	}

	[[nodiscard]] inline bool arg6(u64& value) const {
		__try {
			value = *reinterpret_cast<usize*>(rsp + 56);
			return true;
		}
		__except (1) {
			return false;
		}
	}

	[[nodiscard]] inline bool arg7(u64& value) const {
		__try {
			value = *reinterpret_cast<usize*>(rsp + 64);
			return true;
		}
		__except (1) {
			return false;
		}
	}

	[[nodiscard]] inline bool arg8(u64& value) const {
		__try {
			value = *reinterpret_cast<usize*>(rsp + 72);
			return true;
		}
		__except (1) {
			return false;
		}
	}
};
