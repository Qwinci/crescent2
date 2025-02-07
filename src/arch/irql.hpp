#pragma once
#include "arch/arch_irql.hpp"

extern "C" inline KIRQL KeGetCurrentIrql() {
	return arch_get_current_irql();
}

extern "C" inline KIRQL KfRaiseIrql(KIRQL new_irql) {
	return arch_raise_irql(new_irql);
}

extern "C" inline void KeLowerIrql(KIRQL new_irql) {
	arch_lower_irql(new_irql);
}

#define KeRaiseIrql(old_irql, new_irql) *(old_irql) = KfRaiseIrql(new_irql)
