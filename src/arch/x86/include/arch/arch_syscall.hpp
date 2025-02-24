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
	}

	[[nodiscard]] constexpr u64 arg4() const {
		return rdi;
	}

	[[nodiscard]] constexpr u64 arg5() const {
		return rsi;
	}
};
