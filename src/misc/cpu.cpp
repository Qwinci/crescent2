#include "cpu.hpp"

NTAPI volatile CCHAR KeNumberProcessors = 1;

NTAPI ULONG KeQueryMaximumProcessorCount() {
	return 64;
}
