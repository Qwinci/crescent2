#include "cstring.hpp"
#include "utils/export.hpp"

#undef memcpy
#undef memset

EXPORT void* memcpy(void* __restrict dest, const void* __restrict src, size_t size) {
	void* dest_copy = dest;
	asm volatile("rep movsb" : "+D"(dest_copy), "+S"(src), "+c"(size) : : "flags", "memory");
	return dest;
}

EXPORT void* memset(void* __restrict dest, int ch, size_t size) {
	void* dest_copy = dest;
	asm volatile("rep stosb" : "+D"(dest_copy), "+c"(size) : "a"(ch) : "flags", "memory");
	return dest;
}
