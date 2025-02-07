#pragma once
#include "arch/arch_irq.hpp"
#include <hz/list.hpp>

struct KINTERRUPT;

using PKSERVICE_ROUTINE = bool (*)(KINTERRUPT* irq, void* service_ctx);

struct IrqHandler {
	hz::list_hook hook {};
	PKSERVICE_ROUTINE fn {};
	void* service_ctx {};
	bool can_be_shared {};
};

void register_irq_handler(u32 num, IrqHandler* handler);
void deregister_irq_handler(u32 num, IrqHandler* handler);
