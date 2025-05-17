#include "clock.hpp"
#include "stdio.hpp"

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
