#pragma once
#include "types.hpp"

struct SyscallFrame {
	u64 rax;
	u64 rcx;
	u64 rdx;
	u64 r8;
	u64 r9;
	u64 r10;
	u64 r11;
	u64 rdi;
	u64 rsi;

	[[nodiscard]] constexpr u64 num() const {
		return rax;
	}

	constexpr u64* ret() {
		return &rax;
	}

	constexpr u64* arg0() {
		return &rdx;
	}

	[[nodiscard]] constexpr u64 arg1() const {
		return r8;
	}

	[[nodiscard]] constexpr u64 arg2() const {
		return r9;
	}

	[[nodiscard]] constexpr u64 arg3() const {
		return r10;
	}

	[[nodiscard]] constexpr u64 arg4() const {
		return rdi;
	}

	[[nodiscard]] constexpr u64 arg5() const {
		return rsi;
	}
};
