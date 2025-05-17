#pragma once
#include "types.hpp"
#include "sched/callback_producer.hpp"
#include <hz/string_view.hpp>

struct ClockSource {
	constexpr explicit ClockSource(hz::string_view name, u64 frequency) : name {name}, frequency {frequency} {}

	virtual ~ClockSource() = default;

	virtual u64 get_ns() = 0;

	hz::string_view name;
	u64 frequency;
};

struct TickSource {
	constexpr TickSource(hz::string_view name, u64 max_us) : name {name}, max_us {max_us} {}

	virtual ~TickSource() = default;

	virtual void oneshot(u64 us) = 0;

	hz::string_view name;
	u64 max_us;

	CallbackProducer callback_producer {};
};

static constexpr u64 NS_IN_US = 1000;
static constexpr u64 NS_IN_MS = NS_IN_US * 1000;
static constexpr u64 NS_IN_S = NS_IN_MS * 1000;

void register_clock_source(ClockSource* source);

extern ClockSource* CLOCK_SOURCE;

inline void udelay(u64 us) {
	auto now = CLOCK_SOURCE->get_ns();
	auto end = now + us * NS_IN_US;
	while (CLOCK_SOURCE->get_ns() < end);
}

inline void mdelay(u64 ms) {
	udelay(ms * 1000);
}

NTAPI extern "C" LARGE_INTEGER KeQueryPerformanceCounter(PLARGE_INTEGER freq);

NTAPI extern "C" void KeStallExecutionProcessor(ULONG us);
