#include "stdio.hpp"

constexpr const char CHARS[] = "0123456789ABCDEF";

#define memcpy __builtin_memcpy

Log& Log::operator<<(usize value) {
	u8 base;
	switch (fmt) {
		case Fmt::Dec:
			base = 10;
			break;
		case Fmt::Hex:
			base = 16;
			break;
		case Fmt::Bin:
			base = 2;
			break;
		default:
			base = 10;
			break;
	}

	char local_buf[64];
	char* ptr = local_buf + 64;
	do {
		*--ptr = CHARS[value % base];
		value /= base;
	} while (value);
	auto size = static_cast<usize>(local_buf + 64 - ptr);
	for (u32 i = size; i < pad.amount; ++i) {
		hz::string_view c {&pad.c, 1};
		operator<<(c);
	}
	operator<<(hz::string_view {ptr, size});
	return *this;
}

Log& Log::operator<<(isize value) {
	if (value < 0) {
		operator<<(hz::string_view {"-", 1});
		value *= -1;
	}
	return operator<<(static_cast<usize>(value));
}

Log& Log::operator<<(hz::string_view str) {
	if (str.size() + 1 > LOG_SIZE) {
		for (auto& sink : sinks) {
			sink.write(str);
		}
		return *this;
	}

	if (buf_ptr + str.size() + 1 > LOG_SIZE) {
		buf_ptr = 0;
	}

	memcpy(buf + buf_ptr, str.data(), str.size());
	buf[buf_ptr + str.size()] = 0;
	buf_ptr += str.size() + 1;

	for (auto& sink : sinks) {
		sink.write(str);
	}
	return *this;
}

Log& Log::operator<<(hz::u16string_view str) {
	if (str.size() + 1 > LOG_SIZE) {
		for (auto& sink : sinks) {
			sink.write(str);
		}
		return *this;
	}

	if (buf_ptr + str.size() + 1 > LOG_SIZE) {
		buf_ptr = 0;
	}

	for (auto c : str) {
		buf[buf_ptr++] = static_cast<char>(c);
	}

	buf[buf_ptr++] = 0;

	for (auto& sink : sinks) {
		sink.write(str);
	}
	return *this;
}

Log& Log::operator<<(Fmt new_fmt) {
	if (new_fmt == Fmt::Reset) {
		fmt = Fmt::Dec;
		pad.amount = 0;
		return *this;
	}

	fmt = new_fmt;
	return *this;
}

Log& Log::operator<<(Pad new_pad) {
	pad = new_pad;
	return *this;
}

void Log::register_sink(LogSink* sink) {
	sink->registered = true;
	sinks.push(sink);

	for (usize i = 0; i < buf_ptr;) {
		hz::string_view str {buf + i};
		sink->write(str);
		i += str.size() + 1;
	}
}

void Log::unregister_sink(LogSink* sink) {
	if (!sink->registered) {
		return;
	}
	sink->registered = false;
	sinks.remove(sink);
}

hz::spinlock<Log> LOG {};