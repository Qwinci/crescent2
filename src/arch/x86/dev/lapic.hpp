#pragma once
#include "dev/clock.hpp"

struct KINTERRUPT;

struct LapicTickSource : TickSource {
	constexpr LapicTickSource(u64 ticks_in_us, u64 max_us)
		: TickSource {"lapic", max_us}, ticks_in_us {ticks_in_us} {}

	void oneshot(u64 us) override;

	static bool on_irq(KINTERRUPT*, void*);

	u64 ticks_in_us;
};

struct Cpu;

void lapic_first_init();
void lapic_init(Cpu* cpu, bool initial);
void lapic_ipi_self(u8 vec);
void lapic_eoi();
