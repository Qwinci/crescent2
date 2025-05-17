#pragma once
#include "types.hpp"

// this is marked as always inline because it's a single instruction and
// if it is a function with only one instruction it's not properly unwound by gdb
[[gnu::always_inline]] static inline void arch_hlt() {
	asm volatile("hlt");
}

static inline bool arch_enable_irqs(bool enable) {
	u64 old;
	asm("pushfq; pop %0" : "=r"(old));
	if (enable) {
		asm volatile("sti");
	}
	else {
		asm volatile("cli");
	}
	return old & 1 << 9;
}
