#include "cxx.hpp"
#include "types.hpp"

using Fn = void (*)();
[[gnu::visibility("hidden")]] extern Fn INIT_ARRAY_START[];
[[gnu::visibility("hidden")]] extern Fn INIT_ARRAY_END[];

void call_constructors() {
	for (auto fn = INIT_ARRAY_START; fn != INIT_ARRAY_END; ++fn) {
		(*fn)();
	}
}

extern "C" void atexit(void (*)()) {}
