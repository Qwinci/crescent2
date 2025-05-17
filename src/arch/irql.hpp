#pragma once
#include "arch/arch_irql.hpp"

NTAPI extern "C" [[gnu::used]] inline KIRQL KeGetCurrentIrql() {
	return arch_get_current_irql();
}

NTAPI extern "C" [[gnu::used]] inline KIRQL KfRaiseIrql(KIRQL new_irql) {
	return arch_raise_irql(new_irql);
}

NTAPI extern "C" [[gnu::used]] inline void KeLowerIrql(KIRQL new_irql) {
	arch_lower_irql(new_irql);
}

#define KeRaiseIrql(old_irql, new_irql) *(old_irql) = KfRaiseIrql(new_irql)
