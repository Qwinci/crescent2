#include "cstring.hpp"

#undef memcmp

int memcmp(const void* __restrict a, const void* __restrict b, size_t size) {
	auto* a_ptr = static_cast<const unsigned char*>(a);
	auto* b_ptr = static_cast<const unsigned char*>(b);
	for (; size; --size) {
		auto res = *a_ptr++ - *b_ptr++;
		if (res != 0) {
			return res;
		}
	}
	return 0;
}
