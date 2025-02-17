#include "utils/export.hpp"
#include "ntdef.h"
#include "cstring.hpp"
#include "stdio.hpp"
#include "string_view.hpp"
#include <hz/algorithm.hpp>
#include <stdarg.h>
#include <stdint.h>

static bool is_digit(WCHAR c) {
	return c >= u'0' && c <= u'9';
}

extern "C" EXPORT int vswprintf(WCHAR* buffer, size_t buffer_size, const WCHAR* fmt, va_list ap) {
	size_t written = 0;

	auto write = [&](const WCHAR* ptr, size_t size) {
		auto to_write = hz::min(buffer_size - written, size);
		memcpy(buffer + written, ptr, to_write * sizeof(WCHAR));
		written += size;
		return written != buffer_size;
	};

	while (true) {
		auto start = fmt;
		for (; *fmt && *fmt != u'%'; ++fmt);
		auto size = fmt - start;
		for (; fmt[0] == u'%' && fmt[1] == u'%'; fmt += 2) ++size;

		if (size) {
			if (!write(start, size)) {
				return -1;
			}
			continue;
		}

		if (!*fmt) {
			if (written == buffer_size) {
				return -1;
			}
			else {
				buffer[written] = 0;
				return static_cast<int>(written);
			}
		}

		++fmt;

		bool zero_pad = false;
		while (true) {
			bool do_break = false;
			switch (*fmt) {
				case u'0':
					++fmt;
					zero_pad = true;
					break;
				default:
					do_break = true;
					break;
			}

			if (do_break) {
				break;
			}
		}

		int min_width = -1;
		if (is_digit(*fmt)) {
			min_width = 0;
			while (is_digit(*fmt)) {
				min_width *= 10;
				min_width += *fmt - u'0';
				++fmt;
			}
		}

		enum class Width {
			None,
			l,
			ll
		};
		Width width {};
		if (*fmt == 'l') {
			++fmt;
			if (*fmt == 'l') {
				++fmt;
				width = Width::ll;
			}
			else {
				width = Width::l;
			}
		}

		auto pop_int = [&]() {
			switch (width) {
				// NOLINTNEXTLINE
				case Width::None:
					return static_cast<uintmax_t>(va_arg(ap, unsigned int));
				case Width::l:
					return static_cast<uintmax_t>(va_arg(ap, unsigned long));
				case Width::ll:
					return static_cast<uintmax_t>(va_arg(ap, unsigned long long));
			}
			__builtin_unreachable();
		};

		auto pad = [&](int written_size) {
			if (written_size < min_width) {
				for (int i = 0; i < min_width - written_size; ++i) {
					if (!write(zero_pad ? u"0" : u" ", 1)) {
						return false;
					}
				}
			}

			return true;
		};

		switch (*fmt) {
			case 'X':
			{
				++fmt;

				WCHAR buf[16];
				WCHAR* ptr = buf + 16;

				auto value = pop_int();
				do {
					*--ptr = u"0123456789ABCDEF"[value % 16];
					value /= 16;
				} while (value);

				int written_size = static_cast<int>(buf + 16 - ptr);
				if (!pad(written_size)) {
					return -1;
				}

				if (!write(ptr, static_cast<size_t>(written_size))) {
					return -1;
				}

				break;
			}
			default:
				panic("[kernel][stdio]: unsupported snprintf modifier '", kstd::wstring_view {fmt, 1});
		}
	}
}

extern "C" EXPORT int swprintf(WCHAR* buffer, size_t buffer_size, const WCHAR* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int ret = vswprintf(buffer, buffer_size, fmt, ap);
	va_end(ap);
	return ret;
}

extern "C" EXPORT ULONG DbgPrintEx(ULONG component_id, ULONG level, PCSTR format, ...) {
	print(format);
	return 0;
}
