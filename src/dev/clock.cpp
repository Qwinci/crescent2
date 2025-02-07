#include "clock.hpp"
#include "stdio.hpp"

ClockSource* CLOCK_SOURCE;

void register_clock_source(ClockSource* source) {
	println("[kernel]: registering clock source ", source->name);

	if (!CLOCK_SOURCE || source->frequency > CLOCK_SOURCE->frequency) {
		println("[kernel]: switching to clock source ", source->name);
		CLOCK_SOURCE = source;
	}
}
