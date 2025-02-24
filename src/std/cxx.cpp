#include "cxx.hpp"
#include "types.hpp"

using Fn = void (*)();

#pragma section(".CRT$XCT", read)
#pragma section(".CRT$XCV", read)
__declspec(allocate(".CRT$XCT")) Fn CTORS_START {};
__declspec(allocate(".CRT$XCV")) Fn CTORS_END {};

void call_constructors() {
	for (auto fn = &CTORS_START + 1; fn != &CTORS_END; ++fn) {
		(*fn)();
	}
}

extern "C" void atexit(void (*)()) {}
