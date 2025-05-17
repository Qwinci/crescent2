#pragma once
#include "types.hpp"
#include "arch/arch_irq.hpp"
#include "sys/user_access.hpp"

struct __CpuFeatures {
	usize xsave_area_size;
	bool rdrnd;
	bool xsave;
	bool avx;
	bool avx512;
	bool umip;
	bool smep;
	bool smap;
	bool rdseed;
};

extern "C" __CpuFeatures CPU_FEATURES;

inline void enable_user_access() {
	[[likely]] if (CPU_FEATURES.smap) {
		asm volatile("stac");
	}
}

inline void disable_user_access() {
	[[likely]] if (CPU_FEATURES.smap) {
		asm volatile("clac");
	}
}

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
		auto* ptr = reinterpret_cast<usize*>(rsp + 40);
		enable_user_access();
		__try {
			ProbeForRead(ptr, 8, 8);
			value = *ptr;
			disable_user_access();
			return true;
		}
		__except (1) {
			disable_user_access();
			return false;
		}
	}

	[[nodiscard]] inline bool arg5(u64& value) const {
		auto* ptr = reinterpret_cast<usize*>(rsp + 48);
		enable_user_access();
		__try {
			ProbeForRead(ptr, 8, 8);
			value = *ptr;
			disable_user_access();
			return true;
		}
		__except (1) {
			disable_user_access();
			return false;
		}
	}

	[[nodiscard]] inline bool arg6(u64& value) const {
		auto* ptr = reinterpret_cast<usize*>(rsp + 56);
		enable_user_access();
		__try {
			ProbeForRead(ptr, 8, 8);
			value = *ptr;
			disable_user_access();
			return true;
		}
		__except (1) {
			disable_user_access();
			return false;
		}
	}

	[[nodiscard]] inline bool arg7(u64& value) const {
		auto* ptr = reinterpret_cast<usize*>(rsp + 64);
		enable_user_access();
		__try {
			ProbeForRead(ptr, 8, 8);
			value = *ptr;
			disable_user_access();
			return true;
		}
		__except (1) {
			disable_user_access();
			return false;
		}
	}

	[[nodiscard]] inline bool arg8(u64& value) const {
		auto* ptr = reinterpret_cast<usize*>(rsp + 72);
		enable_user_access();
		__try {
			ProbeForRead(ptr, 8, 8);
			value = *ptr;
			disable_user_access();
			return true;
		}
		__except (1) {
			disable_user_access();
			return false;
		}
	}
};
