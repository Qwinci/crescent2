#include "stdio.hpp"

NTAPI extern "C" void _assert(const char* expr, const char* file, unsigned int line) {
	panic(file, ":", line, ": assertion '", expr, "' failed\n");
}
