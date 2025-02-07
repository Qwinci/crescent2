#pragma once
#include "types.hpp"
#include "stdio.hpp"
#include "config.hpp"

using KIRQL = u8;

#define PASSIVE_LEVEL 0
#define LOW_LEVEL 0
#define APC_LEVEL 1
#define DISPATCH_LEVEL 2
#define CLOCK_LEVEL 13
#define IPI_LEVEL 14
#define POWER_LEVEL 14
#define PROFILE_LEVEL 15
#define HIGH_LEVEL 15

inline KIRQL arch_get_current_irql() {
	u32 value;
	asm("mov %%gs:8, %0" : "=r"(value));
	return value;
}

inline KIRQL arch_raise_irql(KIRQL new_irql) {
	auto current = arch_get_current_irql();
	if (new_irql < current) {
		panic("[kernel][x86]: specified irql less than current in arch_raise_irql");
	}
	else if (new_irql == current) {
		return current;
	}

	asm volatile("mov %0, %%gs:8" : : "r"(static_cast<u32>(new_irql)));
#if !CONFIG_LAZY_IRQL
	asm volatile("mov %0, %%cr8" : : "r"(static_cast<u64>(new_irql)));
#endif

	return current;
}

inline void arch_lower_irql(KIRQL new_irql) {
	void dispatcher_process();

	auto current = arch_get_current_irql();
	if (new_irql > current) {
		panic("[kernel][x86]: specified irql greater than current in arch_lower_irql");
	}
	else if (new_irql == current) {
		return;
	}

	if (current >= DISPATCH_LEVEL && new_irql < DISPATCH_LEVEL) {
		asm volatile("mov %0, %%gs:8" : : "r"(static_cast<u32>(DISPATCH_LEVEL)));
		asm volatile("mov %0, %%cr8" : : "r"(static_cast<u64>(DISPATCH_LEVEL)));
		dispatcher_process();
	}

	asm volatile("mov %0, %%gs:8" : : "r"(static_cast<u32>(new_irql)));
	asm volatile("mov %0, %%cr8" : : "r"(static_cast<u64>(new_irql)));
}
