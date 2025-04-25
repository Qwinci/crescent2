#include "wchar.hpp"
#include "string_view.hpp"
#include "cstring.hpp"
#include "types.hpp"
#include "assert.hpp"
#include <hz/algorithm.hpp>

namespace {
	constexpr WCHAR to_lower(WCHAR c) {
		if (c >= u'A' && c <= u'Z') {
			return c | 1 << 5;
		}
		else {
			return c;
		}
	}
}

NTAPI int wcscmp(const WCHAR* a, const WCHAR* b) {
	for (;; ++a, ++b) {
		int res = *a - *b;
		if (res != 0 || !*a) {
			return res;
		}
	}
}

NTAPI int wcsncmp(const WCHAR* a, const WCHAR* b, size_t count) {
	for (; count; --count, ++a, ++b) {
		int res = *a - *b;
		if (res != 0 || !*a) {
			return res;
		}
	}

	return 0;
}

NTAPI size_t wcslen(const WCHAR* str) {
	size_t len = 0;
	for (; *str; ++str) ++len;
	return len;
}

NTAPI size_t wcsnlen(const WCHAR* str, size_t max) {
	size_t len = 0;
	for (; max && *str; --max, ++str) ++len;
	return len;
}

NTAPI WCHAR* wcschr(const WCHAR* str, WCHAR ch) {
	for (; *str; ++str) {
		if (*str == ch) {
			return const_cast<WCHAR*>(str);
		}
	}

	return nullptr;
}

NTAPI WCHAR* wcsrchr(const WCHAR* str, WCHAR ch) {
	usize len = wcslen(str);
	for (; len; --len) {
		if (str[len - 1] == ch) {
			return const_cast<WCHAR*>(str + len - 1);
		}
	}

	return nullptr;
}

NTAPI WCHAR* wcsstr(const WCHAR* str, const WCHAR* substr) {
	if (*substr == 0) {
		return const_cast<WCHAR*>(str);
	}

	kstd::wstring_view view {str};
	kstd::wstring_view substr_view {substr};
	auto pos = view.find(substr_view);
	if (pos == kstd::wstring_view::npos) {
		return nullptr;
	}
	return const_cast<WCHAR*>(str + pos);
}

NTAPI int _wcsicmp(const WCHAR* a, const WCHAR* b) {
	for (;; ++a, ++b) {
		int res = to_lower(*a) - to_lower(*b);
		if (res != 0 || !*a) {
			return res;
		}
	}
}

NTAPI int _wcsnicmp(const WCHAR* a, const WCHAR* b, size_t count) {
	for (; count; --count, ++a, ++b) {
		int res = to_lower(*a) - to_lower(*b);
		if (res != 0 || !*a) {
			return res;
		}
	}

	return 0;
}

NTAPI size_t wcstombs(char* dest, const WCHAR* src, size_t len) {
	if (!src || len > INT32_MAX) {
		return -1;
	}

	if (!dest) {
		len = SIZE_MAX;
	}

	size_t written = 0;
	for (;; ++src) {
		if (!len) {
			return written;
		}

		// todo unicode
		auto c = *src;
		assert(c <= 0x7F);
		if (dest) {
			*dest++ = static_cast<char>(c);
			--len;
		}

		++written;

		if (!c) {
			return written - 1;
		}
	}
}

NTAPI WCHAR* _wcslwr(WCHAR* str) {
	if (!str) {
		return nullptr;
	}

	auto* start = str;
	for (; *str; ++str) {
		*str = to_lower(*str);
	}

	return start;
}

NTAPI int wcscpy_s(WCHAR* dest, size_t dest_size, const WCHAR* src) {
	if (!dest) {
		return EINVAL;
	}

	if (!src) {
		dest[0] = 0;
		return EINVAL;
	}

	usize len = wcslen(src);
	if (len + 1 <= dest_size) {
		memcpy(dest, src, (len + 1) * 2);
		return 0;
	}
	else {
		dest[0] = 0;
		return EINVAL;
	}
}

NTAPI int wcsncpy_s(WCHAR* dest, size_t dest_size, const WCHAR* src, size_t count) {
	if (!dest) {
		return EINVAL;
	}

	if (!dest_size) {
		return EINVAL;
	}

	if (!src) {
		dest[0] = 0;
		return EINVAL;
	}

	if (count == static_cast<size_t>(-1)) {
		usize len = wcslen(src);
		usize to_copy = hz::min(dest_size, len - 1);
		memcpy(dest, src, to_copy * 2);
		dest[to_copy] = 0;
		return STRUNCATE;
	}

	usize len = wcsnlen(src, count);
	if (len + 1 <= dest_size) {
		memcpy(dest, src, len * 2);
		dest[len] = 0;
		return 0;
	}
	else {
		dest[0] = 0;
		return ERANGE;
	}
}

NTAPI int wcscat_s(WCHAR* dest, size_t dest_size, const WCHAR* src) {
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

	usize len = wcslen(src);

	if (dest_len + len + 1 <= dest_size) {
		memcpy(dest + dest_len, src, (len + 1) * 2);
		return 0;
	}
	else {
		dest[0] = 0;
		return EINVAL;
	}
}

NTAPI int wcsncat_s(WCHAR* dest, size_t dest_size, const WCHAR* src, size_t count) {
	if (!dest) {
		return EINVAL;
	}

	if (!src) {
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

	if (count == static_cast<size_t>(-1)) {
		usize len = wcslen(src);
		usize to_copy = hz::min(dest_size - dest_len - 1, len);
		memcpy(dest + dest_len, src, to_copy * 2);
		dest[dest_len + to_copy] = 0;
		return 0;
	}

	usize len = wcsnlen(src, count);

	if (dest_len + len + 1 <= dest_size) {
		memcpy(dest + dest_len, src, len * 2);
		dest[dest_len + len] = 0;
		return 0;
	}
	else {
		dest[0] = 0;
		return ERANGE;
	}
}

NTAPI int _wcsnset_s(WCHAR* dest, size_t dest_size, WCHAR ch, size_t count) {
	if (!dest) {
		return EINVAL;
	}

	// todo verify size
	usize len = wcsnlen(dest, dest_size);
	if (len < count) {
		len = count;
	}

	for (usize i = 0; i < len; ++i) {
		dest[i] = ch;
	}

	return 0;
}
