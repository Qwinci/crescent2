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
	return *reinterpret_cast<__seg_gs KIRQL*>(8);
}

inline KIRQL arch_raise_irql(KIRQL new_irql) {
	auto current = arch_get_current_irql();
	if (new_irql < current) {
		panic("[kernel][x86]: specified irql less than current in arch_raise_irql");
	}
	else if (new_irql == current) {
		return current;
	}

	*reinterpret_cast<__seg_gs KIRQL*>(8) = new_irql;
#if !CONFIG_LAZY_IRQL
	if (new_irql != APC_LEVEL) {
		asm volatile("mov %0, %%cr8" : : "r"(static_cast<u64>(new_irql)));
	}
	else {
		asm volatile("mov %0, %%cr8" : : "r"(static_cast<u64>(0)));
	}
#endif

	return current;
}

extern "C" [[gnu::used]] inline void arch_lower_irql(KIRQL new_irql) {
	auto current = arch_get_current_irql();
	if (new_irql > current) {
		panic("[kernel][x86]: specified irql greater than current in arch_lower_irql");
	}
	else if (new_irql == current) {
		return;
	}

	// after this write the hardware irql can be switched to at max new_irql
	__atomic_store_n(reinterpret_cast<__seg_gs KIRQL*>(8), new_irql, __ATOMIC_RELAXED);

#if CONFIG_LAZY_IRQL
	// if the hardware irql was updated prior to the previous statement then change it back
	if (new_irql < __atomic_load_n(reinterpret_cast<__seg_gs KIRQL*>(24), __ATOMIC_RELAXED)) {
		__atomic_store_n(reinterpret_cast<__seg_gs KIRQL*>(24), new_irql, __ATOMIC_RELAXED);
		asm volatile("mov %0, %%cr8" : : "r"(static_cast<u64>(new_irql)));
	}
#else
	if (new_irql != APC_LEVEL) {
		asm volatile("mov %0, %%cr8" : : "r"(static_cast<u64>(new_irql)));
	}
	else {
		asm volatile("mov %0, %%cr8" : : "r"(static_cast<u64>(0)));
	}
#endif
}
