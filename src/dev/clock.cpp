#include "clock.hpp"
#include "stdio.hpp"
#include "utils/shared_data.hpp"
#include "atomic.hpp"
#include "rtl.hpp"
#include "assert.hpp"
#include "sched/sched.hpp"

ClockSource* CLOCK_SOURCE;
DateTimeProvider* DATE_TIME_PROVIDER;

void register_clock_source(ClockSource* source) {
	println("[kernel]: registering clock source ", source->name);

	if (!CLOCK_SOURCE || source->frequency > CLOCK_SOURCE->frequency) {
		println("[kernel]: switching to clock source ", source->name);
		CLOCK_SOURCE = source;
	}
}

NTAPI LARGE_INTEGER KeQueryPerformanceCounter(PLARGE_INTEGER freq) {
	if (freq) {
		freq->QuadPart = static_cast<LONGLONG>(NS_IN_S);
	}

	return {.QuadPart = static_cast<LONGLONG>(CLOCK_SOURCE->get_ns())};
}

NTAPI void KeStallExecutionProcessor(ULONG us) {
	auto start = CLOCK_SOURCE->get_ns();
	auto end = start + us * NS_IN_US;
	while (CLOCK_SOURCE->get_ns() < end);
}

NTAPI ULONG KeQueryTimeIncrement() {
	return Scheduler::CLOCK_INTERVAL_MS * (NS_IN_MS / 100);
}

NTAPI void ExSystemTimeToLocalTime(PLARGE_INTEGER system_time, PLARGE_INTEGER local_time) {
	auto bias = static_cast<i64>(atomic_load(&SharedUserData->time_zone_bias.u64, memory_order::relaxed));
	local_time->QuadPart = system_time->QuadPart + bias;
}

void system_time_init() {
	assert(DATE_TIME_PROVIDER);

	DateTime date_time {};
	auto status = DATE_TIME_PROVIDER->get_date_time(date_time);
	assert(status == 0);

	TIME_FIELDS fields {
		.year = static_cast<CSHORT>(date_time.year),
		.month = date_time.month,
		.day = date_time.day,
		.hour = date_time.hour,
		.minute = date_time.minute,
		.second = date_time.second,
		.milliseconds = 0,
		.weekday = 0
	};

	LARGE_INTEGER time {};
	auto rtl_status = RtlTimeFieldsToTime(&fields, &time);
	assert(rtl_status);

	atomic_store(&SharedUserData->system_time.u64, static_cast<u64>(time.QuadPart), memory_order::relaxed);
}
