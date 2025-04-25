#include "misc.hpp"
#include "sched/thread.hpp"
#include "arch/arch_sched.hpp"

NTAPI KPROCESSOR_MODE ExGetPreviousMode() {
	return get_current_thread()->previous_mode;
}
