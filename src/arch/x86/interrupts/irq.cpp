#include "x86/irq.hpp"
#include "arch/irq.hpp"
#include "utils/irq_guard.hpp"
#include "stdio.hpp"
#include "arch/arch_irq.hpp"
#include "arch/x86/dev/lapic.hpp"
#include "arch/arch_irql.hpp"
#include "arch/irql.hpp"
#include <hz/spinlock.hpp>

namespace {
	u32 USED_IRQS[256 / 32] {};
	u32 USED_SHAREABLE_IRQS[256 / 32] {};
}

u32 x86_alloc_irq(u32 count, bool shared) {
	if (!count) {
		return 0;
	}

	for (u32 i = 32; i < 256; ++i) {
		bool found = true;

		for (u32 j = i; j < i + count; ++j) {
			if ((USED_IRQS[j / 32] & 1U << (j % 32)) ||
			    (USED_SHAREABLE_IRQS[j / 32] & 1U << (j % 32))) {
				found = false;
				break;
			}
		}

		if (found) {
			if (shared) {
				for (u32 j = i; j < i + count; ++j) {
					USED_SHAREABLE_IRQS[j / 32] |= 1U << (j % 32);
				}
			}
			else {
				for (u32 j = i; j < i + count; ++j) {
					USED_IRQS[j / 32] |= 1U << (j % 32);
				}
			}
			return i;
		}
	}

	if (shared) {
		for (u32 i = 32; i < 256; ++i) {
			bool found = true;

			for (u32 j = i; j < i + count; ++j) {
				if (USED_IRQS[j / 32] & 1U << (j % 32)) {
					found = false;
					break;
				}
			}

			if (found) {
				for (u32 j = i; j < i + count; ++j) {
					USED_SHAREABLE_IRQS[j / 32] |= 1U << (j % 32);
				}
				return i;
			}
		}
	}

	return 0;
}

void x86_dealloc_irq(u32 irq, u32 count, bool shared) {
	if (shared) {
		for (u32 i = irq; i < irq + count; ++i) {
			USED_SHAREABLE_IRQS[i / 32] &= ~(1U << (i % 32));
		}
	}
	else {
		for (u32 i = irq; i < irq + count; ++i) {
			USED_IRQS[i / 32] &= ~(1U << (i % 32));
		}
	}
}

namespace {
	hz::list<IrqHandler, &IrqHandler::hook> IRQ_HANDLERS[256] {};
	hz::spinlock<void> LOCK {};
}

void register_irq_handler(u32 num, IrqHandler* handler) {
	IrqGuard irq_guard {};
	auto guard = LOCK.lock();
	if (!IRQ_HANDLERS[num].is_empty()) {
		if (!IRQ_HANDLERS[num].front()->can_be_shared || !handler->can_be_shared) {
			panic("[kernel][x86]: tried to register an incompatible irq handler");
		}
	}
	IRQ_HANDLERS[num].push(handler);
}

void deregister_irq_handler(u32 num, IrqHandler* handler) {
	IrqGuard irq_guard {};
	auto guard = LOCK.lock();
	if (IRQ_HANDLERS[num].is_empty()) {
		panic("[kernel][x86]: tried to deregister an unregistered irq handler");
	}
	IRQ_HANDLERS[num].remove(handler);
}

extern "C" [[gnu::used]] void arch_irq_handler(IrqFrame* frame, u8 num) {
	if ((frame->cs & 0b11) == 3) {
		asm volatile("swapgs");
	}

	auto vec_irql = num / 16;
	KIRQL old_irql = arch_get_current_irql();
	if (vec_irql >= 2) {
		auto hw_irql = vec_irql - 2;
#if CONFIG_LAZY_IRQL
		if (hw_irql < old_irql) {
			asm volatile("mov %0, %%cr8" : : "r"(static_cast<u64>(vec_irql)));
			// send self-ipi to defer the irq
			lapic_ipi_self(num);
			lapic_eoi();

			if ((frame->cs & 0b11) == 3) {
				asm volatile("swapgs");
			}
			return;
		}
#endif
		arch_raise_irql(hw_irql);
	}

	asm volatile("sti");

	for (auto& handler : IRQ_HANDLERS[num]) {
		if (handler.fn(nullptr, handler.service_ctx)) {
			break;
		}
	}

	lapic_eoi();

	if (vec_irql >= 2) {
		KeLowerIrql(old_irql);
	}

	asm volatile("cli");

	if ((frame->cs & 0b11) == 3) {
		asm volatile("swapgs");
	}
}
