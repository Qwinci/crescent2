#pragma once
#include "arch/misc.hpp"

struct IrqGuard {
	IrqGuard() {
		old = arch_enable_irqs(false);
	}

	~IrqGuard() {
		arch_enable_irqs(old);
	}

	IrqGuard& operator=(IrqGuard&&) = delete;

private:
	bool old;
};
