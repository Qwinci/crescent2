#include "x86/irq.hpp"
#include "arch/irq.hpp"
#include "arch/arch_irq.hpp"
#include "arch/x86/dev/lapic.hpp"
#include "arch/arch_irql.hpp"
#include "arch/irql.hpp"
#include "assert.hpp"
#include <hz/spinlock.hpp>

namespace {
	u32 USED_IRQS[256 / 32] {};
	u32 USED_SHAREABLE_IRQS[256 / 32] {};
	u8 DISPATCH_VEC = 0;
}

u32 x86_alloc_irq(u32 count, KIRQL irql, bool shared) {
	assert(irql >= DISPATCH_LEVEL);
	irql -= DISPATCH_LEVEL;

	if (!count) {
		return 0;
	}

	for (u32 i = 32 + irql * 16; i < 256; ++i) {
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
		for (u32 i = 32 + irql * 16; i < 256; ++i) {
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

extern "C" [[gnu::used]] void arch_irq_handler(KTRAP_FRAME* frame) {
	if (frame->previous_mode == UserMode) {
		asm volatile("swapgs");
	}

	auto num = frame->error_code;

	auto vec_irql = num / 16;
	KIRQL old_irql = arch_get_current_irql();
#if CONFIG_LAZY_IRQL
	if (vec_irql < old_irql) {
		asm volatile("mov cr8, %0" : : "r"(static_cast<u64>(old_irql)));
		// send self-ipi to defer the irq
		lapic_ipi_self(num);
		lapic_eoi();

		__atomic_store_n(reinterpret_cast<__seg_gs KIRQL*>(24), vec_irql, __ATOMIC_RELAXED);

		if (frame->previous_mode == UserMode) {
			asm volatile("swapgs");
		}
		return;
	}
#endif
	arch_raise_irql(vec_irql);

	asm volatile("sti");

	for (auto& handler : IRQ_HANDLERS[num]) {
		if (handler.fn(nullptr, handler.service_ctx)) {
			break;
		}
	}

	lapic_eoi();

	asm volatile("cli");

	KeLowerIrql(old_irql);

	if (frame->previous_mode == UserMode) {
		asm volatile("swapgs");
	}
}

extern IrqHandler DISPATCH_IRQ_HANDLER;

void x86_irq_init() {
	DISPATCH_VEC = x86_alloc_irq(1, DISPATCH_LEVEL, false);
	assert(DISPATCH_VEC);
	register_irq_handler(DISPATCH_VEC, &DISPATCH_IRQ_HANDLER);
}

void arch_request_software_irq(KIRQL level) {
	assert(level == DISPATCH_LEVEL);
	lapic_ipi_self(DISPATCH_VEC);
}
