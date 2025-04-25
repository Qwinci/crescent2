#include "cstring.hpp"
#include "types.hpp"
#include "string_view.hpp"
#include "assert.hpp"
#include "ctype.hpp"

#undef memcmp

NTAPI extern "C" void* memmove(void* dest, const void* src, size_t size) {
	auto dest_addr = reinterpret_cast<usize>(dest);
	auto src_addr = reinterpret_cast<usize>(src);
	if (dest_addr < src_addr || dest_addr >= src_addr + size) {
		return memcpy(dest, src, size);
	}
	else {
		auto* dest_ptr = static_cast<unsigned char*>(dest);
		auto* src_ptr = static_cast<const unsigned char*>(src);
		for (usize i = size; i > 0; --i) {
			dest_ptr[i - 1] = src_ptr[i - 1];
		}
		return dest;
	}
}

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

#undef strlen

NTAPI size_t strlen(const char* str) {
	auto orig = str;
	for (; *str; ++str);
	return str - orig;
}

NTAPI size_t strnlen(const char* str, size_t max) {
	auto orig = str;
	for (; max && *str; --max, ++str);
	return str - orig;
}

NTAPI int strcmp(const char* a, const char* b) {
	for (;; ++a, ++b) {
		int res = *a - *b;
		if (res != 0 || !*a) {
			return res;
		}
	}
}

NTAPI int strncmp(const char* a, const char* b, size_t size) {
	for (; size; --size, ++a, ++b) {
		int res = *a - *b;
		if (res != 0 || !*a) {
			return res;
		}
	}

	return 0;
}

NTAPI char* strstr(const char* str, const char* substr) {
	if (*substr == 0) {
		return const_cast<char*>(str);
	}

	kstd::string_view view {str};
	kstd::string_view substr_view {substr};
	auto pos = view.find(substr_view);
	if (pos == kstd::string_view::npos) {
		return nullptr;
	}
	return const_cast<char*>(str + pos);
}

NTAPI int _stricmp(const char* a, const char* b) {
	for (;; ++a, ++b) {
		int res = tolower(*a) - tolower(*b);
		if (res != 0 || !*a) {
			return res;
		}
	}
}

NTAPI size_t mbstowcs(char16_t* dest, const char* src, size_t len) {
	if (!dest) {
		len = SIZE_MAX;
	}

	size_t written = 0;
	for (;; ++src) {
		if (!len) {
			return written;
		}

		auto c = *src;
		assert(c >= 0 && c <= 0x7F);
		if (dest) {
			*dest++ = static_cast<char16_t>(c);
			--len;
		}

		++written;

		if (!c) {
			return written - 1;
		}
	}
}

NTAPI char* strncpy(char* dest, const char* src, size_t count) {
	usize len = strlen(dest);
	for (; count && *src; --count, ++src) {
		dest[len++] = *src;
	}

	memset(&dest[len], 0, count);

	return dest;
}

NTAPI int strcpy_s(char* dest, size_t dest_size, const char* src) {
	if (!dest) {
		return EINVAL;
	}

	if (!src) {
		dest[0] = 0;
		return EINVAL;
	}

	usize len = strlen(src);
	if (len + 1 <= dest_size) {
		memcpy(dest, src, len + 1);
		return 0;
	}
	else {
		dest[0] = 0;
		return EINVAL;
	}
}

NTAPI int strcat_s(char* dest, size_t dest_size, const char* src) {
	if (!dest) {
		return EINVAL;
	}

	if (!src) {
		dest[0] = 0;
		return EINVAL;
	}

	usize dest_len = 0;
	while (true) {
		if (dest_len >= dest_size) {
			return EINVAL;
		}

		if (!dest[dest_len]) {
			break;
		}
		++dest_len;
	}

	usize len = strlen(src);

	if (dest_len + len + 1 <= dest_size) {
		memcpy(dest + dest_len, src, len + 1);
		return 0;
	}
	else {
		dest[0] = 0;
		return EINVAL;
	}
}

NTAPI char* strtok_s(char* str, const char* delimiters, char** ctx) {
	if ((!str && (!ctx || !*ctx)) || !delimiters || !ctx) {
		return nullptr;
	}

	if (str) {
		*ctx = str;
	}

	hz::string_view str_view {*ctx};
	hz::string_view delim_str {delimiters};

	auto* start = *ctx;

	auto start_pos = str_view.find_first_not_of(delim_str);
	if (start_pos == hz::string_view::npos) {
		return nullptr;
	}

	auto end_pos = str_view.find_first_of(delim_str, start_pos);
	if (end_pos != hz::string_view::npos) {
		(*ctx)[end_pos] = 0;
		*ctx += end_pos + 1;
	}
	else {
		*ctx += str_view.size();
	}

	return start + start_pos;
}
