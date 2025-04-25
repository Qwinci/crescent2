#include "printf.hpp"
#include "cstring.hpp"
#include "stdio.hpp"
#include "string_view.hpp"
#include "assert.hpp"
#include "wchar.hpp"
#include <hz/algorithm.hpp>
#include <hz/optional.hpp>
#include <stdarg.h>
#include <stdint.h>

static bool is_digit(WCHAR c) {
	return c >= u'0' && c <= u'9';
}

template<typename T, typename C>
concept PrintfTarget = requires(T target, const C* str, usize len) {
	target.write(str, len);
};

enum class Size {
	Char,
	Short,
	Int,
	Long,
	Longlong,
	Intmax,
	Size,
	Ptrdiff,
	LongDouble
};

enum class FormatResult {
	Success,
	InvalidFormat
};

template<typename C, PrintfTarget<C> Target>
static FormatResult generic_printf(Target& target, const C* fmt, va_list ap) {
	while (true) {
		auto* start = fmt;
		for (; *fmt && *fmt != '%'; ++fmt);
		auto len = fmt - start;
		for (; fmt[0] == '%' && fmt[1] == '%'; fmt += 2) ++len;

		if (len) {
			target.write(start, len);
			continue;
		}

		if (!*fmt) {
			return FormatResult::Success;
		}

		++fmt;

		bool left_justify = false;
		bool always_sign = false;
		bool space_sign = false;
		bool alt_form = false;
		bool zero_pad = false;
		while (true) {
			switch (*fmt) {
				case '-':
					left_justify = true;
					++fmt;
					continue;
				case '+':
					always_sign = true;
					++fmt;
					continue;
				case ' ':
					space_sign = true;
					++fmt;
					continue;
				case '#':
					alt_form = true;
					++fmt;
					continue;
				case '0':
					zero_pad = true;
					++fmt;
					continue;
				default:
					break;
			}

			break;
		}

		if (space_sign && always_sign) {
			space_sign = false;
		}
		if (left_justify && zero_pad) {
			zero_pad = false;
		}

		int min_width;
		if (*fmt == '*') {
			++fmt;
			int value = va_arg(ap, int);
			if (value < 0) {
				left_justify = true;
				zero_pad = false;
				value *= -1;
			}
			min_width = value;
		}
		else {
			min_width = 0;
			while (*fmt >= '0' && *fmt <= '9') {
				min_width *= 10;
				min_width += *fmt - '0';
				++fmt;
			}
		}

		hz::optional<int> precision;
		if (*fmt == '.') {
			++fmt;
			if (*fmt == '*') {
				precision = va_arg(ap, int);
			}
			else {
				int value = 0;
				while (*fmt >= '0' && *fmt <= '9') {
					value *= 10;
					value += *fmt - '0';
					++fmt;
				}
				precision = value;
			}
		}

		Size size = Size::Int;
		switch (*fmt) {
			case 'h':
				++fmt;
				if (*fmt == 'h') {
					++fmt;
					size = Size::Char;
				}
				else {
					size = Size::Short;
				}
				break;
			case 'l':
				++fmt;
				if (*fmt == 'l') {
					++fmt;
					size = Size::Longlong;
				}
				else {
					size = Size::Long;
				}
				break;
			case 'j':
				++fmt;
				size = Size::Intmax;
				break;
			case 'z':
				++fmt;
				size = Size::Size;
				break;
			case 't':
				++fmt;
				size = Size::Ptrdiff;
				break;
			case 'L':
				++fmt;
				size = Size::LongDouble;
				break;
			default:
				break;
		}

		switch (*fmt) {
			case 'c':
			{
				++fmt;

				if (size == Size::Int) {
					auto c = static_cast<unsigned char>(va_arg(ap, int));
					C ch = static_cast<C>(c);
					target.write(&ch, 1);
				}
				else if (size == Size::Long) {
					auto c = va_arg(ap, int);
					// todo
					C ch = static_cast<C>(c);
					target.write(&ch, 1);
				}
				else {
					return FormatResult::InvalidFormat;
				}

				break;
			}
			case 's':
			{
				++fmt;

				if (size == Size::Int) {
					auto* str = va_arg(ap, const char*);
					if (!str) {
						str = "(null)";
					}

					int str_len;
					if (precision) {
						str_len = static_cast<int>(strnlen(str, *precision));
					}
					else {
						str_len = static_cast<int>(strlen(str));
					}

					if (left_justify) {
						if constexpr (hz::is_same_v<C, char>) {
							target.write(str, str_len);
						}
						else {
							for (int i = 0; i < str_len; ++i) {
								C ch = static_cast<C>(str[i]);
								target.write(&ch, 1);
							}
						}

						for (int i = str_len; i < min_width; ++i) {
							C space = static_cast<C>(' ');
							target.write(&space, 1);
						}
					}
					else {
						for (int i = str_len; i < min_width; ++i) {
							C space = static_cast<C>(' ');
							target.write(&space, 1);
						}

						if constexpr (hz::is_same_v<C, char>) {
							target.write(str, str_len);
						}
						else {
							for (int i = 0; i < str_len; ++i) {
								C ch = static_cast<C>(str[i]);
								target.write(&ch, 1);
							}
						}
					}
				}
				else if (size == Size::Long) {
					auto* str = va_arg(ap, const WCHAR*);
					if (!str) {
						str = u"(null)";
					}

					int str_len;
					if (precision) {
						str_len = static_cast<int>(wcsnlen(str, *precision));
					}
					else {
						str_len = static_cast<int>(wcslen(str));
					}

					if (left_justify) {
						if constexpr (hz::is_same_v<C, WCHAR>) {
							target.write(str, str_len);
						}
						else {
							for (int i = 0; i < str_len; ++i) {
								C ch = static_cast<C>(str[i]);
								target.write(&ch, 1);
							}
						}

						for (int i = str_len; i < min_width; ++i) {
							C space = static_cast<C>(' ');
							target.write(&space, 1);
						}
					}
					else {
						for (int i = str_len; i < min_width; ++i) {
							C space = static_cast<C>(' ');
							target.write(&space, 1);
						}

						if constexpr (hz::is_same_v<C, WCHAR>) {
							target.write(str, str_len);
						}
						else {
							for (int i = 0; i < str_len; ++i) {
								C ch = static_cast<C>(str[i]);
								target.write(&ch, 1);
							}
						}
					}
				}
				else {
					return FormatResult::InvalidFormat;
				}

				break;
			}
			case 'd':
			case 'i':
			{
				++fmt;

				intmax_t value;
				switch (size) {
					case Size::Char:
					case Size::Short:
					case Size::Int:
						value = va_arg(ap, int);
						break;
					case Size::Long:
						value = va_arg(ap, long);
						break;
					case Size::Longlong:
						value = va_arg(ap, long long);
						break;
					case Size::Intmax:
						value = va_arg(ap, intmax_t);
						break;
					case Size::Size:
						value = va_arg(ap, isize);
						break;
					case Size::Ptrdiff:
						value = va_arg(ap, ptrdiff_t);
						break;
					case Size::LongDouble:
						return FormatResult::InvalidFormat;
				}

				int min_chars = 1;
				if (*precision) {
					min_chars = *precision;
				}

				C buf[20];
				C* ptr = buf + 20;

				bool negative = false;
				if (value < 0) {
					value *= 1;
					negative = true;
				}

				if (value != 0 || min_chars != 0) {
					do {
						*--ptr = static_cast<C>('0' + value % 10);
						value /= 10;
					} while (value);

					if (negative) {
						*--ptr = '-';
					}
					else if (always_sign) {
						*--ptr = '+';
					}
					else if (space_sign) {
						*--ptr = ' ';
					}
				}

				int num_len = static_cast<int>((buf + 20) - ptr);

				C pad = static_cast<C>(zero_pad ? '0' : ' ');

				if (left_justify) {
					target.write(ptr, num_len);
					if (num_len < min_chars) {
						C zero = static_cast<C>('0');
						for (int i = num_len; i < min_chars; ++i) {
							target.write(&zero, 1);
						}
						num_len = min_chars;
					}
					for (int i = num_len; i < min_width; ++i) {
						target.write(&pad, 1);
					}
				}
				else {
					int real_chars = hz::max(num_len, min_chars);
					for (int i = real_chars; i < min_width; ++i) {
						target.write(&pad, 1);
					}
					C zero = static_cast<C>('0');
					for (int i = num_len; i < real_chars; ++i) {
						target.write(&zero, 1);
					}
					target.write(ptr, num_len);
				}

				break;
			}
			case 'b':
			case 'B':
			case 'o':
			case 'x':
			case 'X':
			case 'u':
			case 'p':
			{
				auto specific = *fmt;
				++fmt;

				uintmax_t value;
				if (specific == 'p') {
					value = reinterpret_cast<uintptr_t>(va_arg(ap, void*));
				}
				else {
					switch (size) {
						case Size::Char:
						case Size::Short:
						case Size::Int:
							value = va_arg(ap, unsigned int);
							break;
						case Size::Long:
							value = va_arg(ap, unsigned long);
							break;
						case Size::Longlong:
							value = va_arg(ap, unsigned long long);
							break;
						case Size::Intmax:
							value = va_arg(ap, uintmax_t);
							break;
						case Size::Size:
							value = va_arg(ap, size_t);
							break;
						case Size::Ptrdiff:
							value = va_arg(ap, uintptr_t);
							break;
						case Size::LongDouble:
							return FormatResult::InvalidFormat;
					}
				}

				int min_chars = 1;
				if (*precision) {
					min_chars = *precision;
				}

				int base;
				const char* prefix = "";
				switch (specific) {
					case 'b':
						base = 2;
						prefix = "0B";
						break;
					case 'B':
						base = 2;
						prefix = "0b";
						break;
					case 'o':
						base = 8;
						prefix = "0";
						break;
					case 'x':
						base = 16;
						prefix = "0x";
						break;
					case 'X':
						base = 16;
						prefix = "0X";
						break;
					case 'u':
						base = 10;
						break;
					case 'p':
						base = 16;
						prefix = "0x";
						break;
					default:
						panic("unimplemented int format");
				}

				C buf[64 + 3];
				C* ptr = buf + 64 + 3;
				const C* chars;
				if constexpr (hz::is_same_v<C, char>) {
					if (specific == 'b' || specific == 'x') {
						chars = "0123456789abcdef";
					}
					else {
						chars = "0123456789ABCDEF";
					}
				}
				else {
					if (specific == 'b' || specific == 'x') {
						chars = u"0123456789abcdef";
					}
					else {
						chars = u"0123456789ABCDEF";
					}
				}

				C sign_char = 0;
				int prefix_len = 0;
				if (value != 0 || min_chars != 0) {
					do {
						*--ptr = chars[value % base];
						value /= base;
					} while (value);

					if (alt_form) {
						prefix_len = static_cast<int>(strlen(prefix));
					}

					if (always_sign) {
						sign_char = '+';
					}
					else if (space_sign) {
						sign_char = ' ';
					}
				}

				int num_len = static_cast<int>((buf + 64 + 3) - ptr);

				C pad = static_cast<C>(zero_pad ? '0' : ' ');

				if (left_justify) {
					if (prefix_len) {
						ptr -= prefix_len;
						for (int i = 0; i < prefix_len; ++i) {
							ptr[i] = prefix[i];
						}
						num_len += prefix_len;
					}
					if (sign_char) {
						*--ptr = sign_char;
						++num_len;
					}

					target.write(ptr, num_len);
					if (num_len < min_chars) {
						C zero = static_cast<C>('0');
						for (int i = num_len; i < min_chars; ++i) {
							target.write(&zero, 1);
						}
						num_len = min_chars;
					}
					for (int i = num_len; i < min_width; ++i) {
						target.write(&pad, 1);
					}
				}
				else {
					int real_chars = hz::max(num_len, min_chars);
					int sign_len = sign_char ? 1 : 0;

					if (!zero_pad) {
						for (int i = real_chars + prefix_len + sign_len; i < min_width; ++i) {
							target.write(&pad, 1);
						}
					}

					if (sign_char) {
						target.write(&sign_char, 1);
					}
					if (prefix_len) {
						for (int i = 0; i < prefix_len; ++i) {
							C ch = static_cast<C>(prefix[i]);
							target.write(&ch, 1);
						}
					}

					if (zero_pad) {
						for (int i = real_chars + prefix_len + sign_len; i < min_width; ++i) {
							target.write(&pad, 1);
						}
					}

					C zero = static_cast<C>('0');
					for (int i = num_len; i < real_chars; ++i) {
						target.write(&zero, 1);
					}

					target.write(ptr, num_len);
				}

				break;
			}
			default:
				panic("[kernel]: invalid printf format ", hz::basic_string_view<C> {fmt, 1});
		}
	}
}

NTAPI int vswprintf(WCHAR* buffer, size_t buffer_size, const WCHAR* fmt, va_list ap) {
	struct Target {
		WCHAR* buffer;
		size_t written;
		size_t buffer_size;

		void write(const WCHAR* str, usize len) {
			if (buffer) {
				auto to_write = hz::min(buffer_size - written, len);
				memcpy(buffer + written, str, to_write * sizeof(WCHAR));
				written += to_write;
			}
			else {
				written += len;
			}
		}
	};

	Target target {
		.buffer = buffer,
		.written = 0,
		.buffer_size = buffer_size
	};
	auto res = generic_printf(target, fmt, ap);
	assert(res == FormatResult::Success);
	if (buffer) {
		if (target.written == buffer_size) {
			return -1;
		}
		buffer[target.written] = 0;
	}

	return static_cast<int>(target.written);
}

NTAPI int vsprintf_s(char* buffer, size_t buffer_size, const char* fmt, va_list ap) {
	if (!buffer || !fmt || !buffer_size) {
		return -1;
	}

	struct Target {
		char* buffer;
		size_t written;
		size_t buffer_size;

		void write(const char* str, usize len) {
			auto to_write = hz::min(buffer_size - written, len);
			memcpy(buffer + written, str, to_write);
			written += to_write;
		}
	};

	Target target {
		.buffer = buffer,
		.written = 0,
		.buffer_size = buffer_size
	};
	auto res = generic_printf(target, fmt, ap);
	if (res != FormatResult::Success || target.written == buffer_size) {
		return -1;
	}
	buffer[target.written] = 0;
	return static_cast<int>(target.written);
}

NTAPI int _vsnprintf(char* buffer, size_t buffer_size, const char* fmt, va_list ap) {
	if (!buffer || !fmt || !buffer_size) {
		return -1;
	}

	struct Target {
		char* buffer;
		size_t written;
		size_t buffer_size;

		void write(const char* str, usize len) {
			auto to_write = hz::min(buffer_size - written, len);
			memcpy(buffer + written, str, to_write);
			written += to_write;
		}
	};

	Target target {
		.buffer = buffer,
		.written = 0,
		.buffer_size = buffer_size
	};
	auto res = generic_printf(target, fmt, ap);
	if (res != FormatResult::Success || target.written == buffer_size) {
		return -1;
	}
	buffer[target.written] = 0;
	return static_cast<int>(target.written);
}

NTAPI int _snprintf(char* buffer, size_t buffer_size, const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int ret = _vsnprintf(buffer, buffer_size, fmt, ap);
	va_end(ap);
	return ret;
}

NTAPI int _vsnprintf_s(char* buffer, size_t buffer_size, size_t count, const char* fmt, va_list ap) {
	if (!buffer || !fmt || !buffer_size) {
		return -1;
	}

	struct Target {
		char* buffer;
		size_t written;
		size_t buffer_size;

		void write(const char* str, usize len) {
			auto to_write = hz::min(buffer_size - written, len);
			memcpy(buffer + written, str, to_write);
			written += to_write;
		}
	};

	Target target {
		.buffer = buffer,
		.written = 0,
		.buffer_size = buffer_size
	};
	auto res = generic_printf(target, fmt, ap);
	if (res != FormatResult::Success || target.written == buffer_size) {
		if (count != static_cast<size_t>(-1)) {
			buffer[0] = 0;
			return -1;
		}
		else {
			buffer[target.written - 1] = 0;
			return static_cast<int>(target.written);
		}
	}
	buffer[target.written] = 0;
	return static_cast<int>(target.written);
}

NTAPI int _snprintf_s(char* buffer, size_t buffer_size, size_t count, const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int ret = _vsnprintf_s(buffer, buffer_size, count, fmt, ap);
	va_end(ap);
	return ret;
}

NTAPI int sprintf(char* buffer, const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int ret = vsprintf_s(buffer, SIZE_MAX, fmt, ap);
	va_end(ap);
	return ret;
}

NTAPI int sprintf_s(char* buffer, size_t buffer_size, const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int ret = vsprintf_s(buffer, buffer_size, fmt, ap);
	va_end(ap);
	return ret;
}

NTAPI int swprintf(WCHAR* buffer, size_t buffer_size, const WCHAR* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int ret = vswprintf(buffer, buffer_size, fmt, ap);
	va_end(ap);
	return ret;
}

NTAPI extern "C" int _vsnwprintf(WCHAR* buffer, size_t buffer_size, const WCHAR* fmt, va_list ap) {
	if (!buffer || !fmt || !buffer_size) {
		return -1;
	}

	struct Target {
		WCHAR* buffer;
		size_t written;
		size_t buffer_size;

		void write(const WCHAR* str, usize len) {
			auto to_write = hz::min(buffer_size - written, len);
			memcpy(buffer + written, str, to_write * sizeof(WCHAR));
			written += to_write;
		}
	};

	Target target {
		.buffer = buffer,
		.written = 0,
		.buffer_size = buffer_size
	};
	auto res = generic_printf(target, fmt, ap);
	if (res != FormatResult::Success || target.written == buffer_size) {
		return -1;
	}
	buffer[target.written] = 0;
	return static_cast<int>(target.written);
}

NTAPI NTSTATUS DbgPrintEx(ULONG component_id, ULONG level, PCSTR format, ...) {
	struct Target {
		static void write(const char* str, usize len) {
			print(kstd::string_view {str, len});
		}
	};

	Target target {};
	va_list ap;
	va_start(ap, format);
	generic_printf(target, format, ap);
	va_end(ap);
	return STATUS_SUCCESS;
}
