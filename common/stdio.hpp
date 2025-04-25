#pragma once
#include <hz/list.hpp>
#include <hz/string_view.hpp>
#include <hz/spinlock.hpp>
#include "types.hpp"
#include "utils/irq_guard.hpp"

struct LogSink {
	virtual ~LogSink() = default;

	hz::list_hook hook {};
	bool registered {};

	virtual void write(hz::string_view str) = 0;
	virtual void write(hz::u16string_view str) = 0;
};

enum class Fmt {
	Dec,
	Hex,
	Bin,
	Reset
};

struct Pad {
	constexpr Pad() : c {' '}, amount {0} {}
	constexpr Pad(char c, u32 amount) : c {c}, amount {amount} {}
	char c;
	u32 amount;
};

class Log {
public:
	inline Log& operator<<(u8 value) {
		return operator<<(static_cast<usize>(value));
	}
	inline Log& operator<<(u16 value) {
		return operator<<(static_cast<usize>(value));
	}
	inline Log& operator<<(u32 value) {
		return operator<<(static_cast<usize>(value));
	}
	inline Log& operator<<(unsigned long value) {
		return operator<<(static_cast<usize>(value));
	}
	inline Log& operator<<(i8 value) {
		return operator<<(static_cast<isize>(value));
	}
	inline Log& operator<<(i16 value) {
		return operator<<(static_cast<isize>(value));
	}
	inline Log& operator<<(i32 value) {
		return operator<<(static_cast<isize>(value));
	}

	Log& operator<<(usize value);
	Log& operator<<(isize value);
	Log& operator<<(hz::string_view str);
	Log& operator<<(hz::u16string_view str);
	Log& operator<<(Fmt new_fmt);
	Log& operator<<(Pad new_pad);

	void register_sink(LogSink* sink);
	void unregister_sink(LogSink* sink);

private:
	static constexpr usize LOG_SIZE = 0x2000;

	hz::list<LogSink, &LogSink::hook> sinks {};
	char buf[LOG_SIZE] {};
	usize buf_ptr {};
	Fmt fmt {};
	Pad pad {};
};

extern hz::spinlock<Log> LOG;

template<typename... Args>
inline void print(Args... args) {
	IrqGuard irq_guard;
	auto guard = LOG.lock();
	((*guard << args), ...);
}

template<typename... Args>
inline void println(Args... args) {
	IrqGuard irq_guard;
	auto guard = LOG.lock();
	((*guard << args), ...);
	*guard << hz::string_view {"\n"};
}

template<typename... Args>
[[noreturn]] inline void panic(Args... args) {
	IrqGuard irq_guard;
	println("KERNEL PANIC: ", args...);
	while (true) {
		arch_hlt();
	}
}
