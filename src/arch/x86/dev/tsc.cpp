#include "tsc.hpp"
#include "arch/x86/cpuid.hpp"
#include "dev/clock.hpp"
#include "stdio.hpp"
#include "arch/cpu.hpp"
#include "arch/x86/cpu.hpp"

struct TscClockSource : public ClockSource {
	explicit TscClockSource(u64 freq) : ClockSource {"tsc", freq}, tsc_per_ms {freq / 1000} {}

	u64 get_ns() override {
		return rdtsc() * 1000000 / tsc_per_ms;
	}

	u64 tsc_per_ms;
};

static hz::manually_init<TscClockSource> TSC_CLOCK_SOURCE {};

void tsc_init() {
	u64 start = rdtsc();
	mdelay(10);
	u64 end = rdtsc();
	auto tsc_freq = (end - start) * 100;

	auto info = cpuid(0x80000007, 0);
	if (info.edx & 1 << 8) {
		println("[kernel][x86]: tsc frequency ", tsc_freq);
		TSC_CLOCK_SOURCE.initialize(tsc_freq);
		register_clock_source(&*TSC_CLOCK_SOURCE);
	}
}
