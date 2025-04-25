#include "rand.hpp"

namespace {
	int VALUE = 1;
}

NTAPI void srand(unsigned int seed) {
	VALUE = static_cast<int>(seed);
}

NTAPI int rand() {
	VALUE *= 0x343FD;
	VALUE += 0x269EC3;
	return VALUE >> 16 & 0x7FFF;
}
