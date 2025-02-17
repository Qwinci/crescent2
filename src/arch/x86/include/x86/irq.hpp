#pragma once
#include "types.hpp"
#include "arch/arch_irql.hpp"

u32 x86_alloc_irq(u32 count, KIRQL irql, bool shared);
void x86_dealloc_irq(u32 irq, u32 count, bool shared);
