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

struct DateTime {
	u16 year;
	u8 month;
	u8 day;
	u8 hour;
	u8 minute;
	u8 second;
};

struct DateTimeProvider {
	virtual ~DateTimeProvider() = default;

	virtual NTSTATUS get_date_time(DateTime& time) = 0;
};

static constexpr u64 NS_IN_US = 1000;
static constexpr u64 NS_IN_MS = NS_IN_US * 1000;
static constexpr u64 NS_IN_S = NS_IN_MS * 1000;

void register_clock_source(ClockSource* source);

extern ClockSource* CLOCK_SOURCE;
extern DateTimeProvider* DATE_TIME_PROVIDER;

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
NTAPI extern "C" ULONG KeQueryTimeIncrement();

NTAPI extern "C" void ExSystemTimeToLocalTime(PLARGE_INTEGER system_time, PLARGE_INTEGER local_time);

void system_time_init();
