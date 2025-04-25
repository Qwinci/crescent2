#pragma once
#include "arch/arch_irq.hpp"
#include "arch/arch_irql.hpp"
#include <hz/list.hpp>

struct KINTERRUPT;

using PKSERVICE_ROUTINE = bool (*)(KINTERRUPT* irq, void* service_ctx);

void register_irq_handler(KINTERRUPT* irq);
void deregister_irq_handler(KINTERRUPT* irq);

void arch_request_software_irq(KIRQL level);
