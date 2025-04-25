#pragma once

#include <hz/atomic.hpp>
#include "ntdef.h"

struct EX_PUSH_LOCK {
	hz::atomic<ULONGLONG> value;
};
