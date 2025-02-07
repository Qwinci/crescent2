#include "profiler.hpp"
#include "types.hpp"

struct CallFrame {
	void* fn;
	void* site;
	u64 start;
	u64 ptime;
};

#define MAX_FRAMES 64

namespace {
	CallFrame FRAMES[MAX_FRAMES];
	usize CUR_FRAME = 0;
}

struct Record {
	void* fn;
	u64 total;
	usize calls;
};

#define MAX_RECORDS 0x10000 // should be greater than the total number of functions across the kernel, 64K is completely arbitrary
static Record RECORDS[MAX_RECORDS];
static usize NUM_RECORDS = 0;

static int PROFILER_LOCK = 0;
static int PROFILER_ACTIVE = 0;

[[gnu::no_instrument_function]] static u64 sdi() {
	u64 value;
	asm volatile("pushfq; popq %0; cli" : "=rm"(value));
	while (__atomic_exchange_n(&PROFILER_LOCK, 1, __ATOMIC_ACQUIRE)) {
		__builtin_ia32_pause();
	}
	return value;
}

[[gnu::no_instrument_function]] static void ri(u64 value) {
	__atomic_store_n(&PROFILER_LOCK, 0, __ATOMIC_RELEASE);
	if (value & 0x200) {
		asm volatile("sti");
	}
}

[[gnu::no_instrument_function]] void profiler_start() {
	__atomic_store_n(&PROFILER_ACTIVE, 1, __ATOMIC_RELEASE);
}

[[gnu::no_instrument_function]] void profiler_stop() {
	__atomic_store_n(&PROFILER_ACTIVE, 0, __ATOMIC_RELEASE);
}

[[gnu::no_instrument_function]] void profiler_reset() {
	u64 state = sdi();
	NUM_RECORDS = 0;
	ri(state);
}

[[gnu::no_instrument_function]] static void printc(char c) {
	asm volatile("out %0, $0xE9" : : "a"(c));
}

[[gnu::no_instrument_function]] static void prints(const char* s) {
	for (char c = *s; c != 0; c = *++s) {
		printc(c);
	}
}

[[gnu::no_instrument_function]] static void printu(u64 value) {
	char buffer[sizeof(value) * 3 + 1];
	size_t index = sizeof(buffer);
	buffer[--index] = 0;

	do {
		buffer[--index] = static_cast<char>('0' + (value % 10));
		value /= 10;
	} while (value > 0);

	prints(&buffer[index]);
}

[[gnu::no_instrument_function]] static void printx(usize value) {
	char buffer[sizeof(value) * 2 + 1];
	size_t index = sizeof(buffer);
	buffer[--index] = 0;

	do {
		buffer[--index] = "0123456789abcdef"[value & 15];
		value >>= 4;
	} while (value > 0);

	prints(&buffer[index]);
}

[[gnu::no_instrument_function]] static bool pred(Record* a, Record* b) {
	size_t ta = (a->total + (a->calls / 2)) / a->calls;
	size_t tb = (b->total + (b->calls / 2)) / b->calls;
	return ta < tb;
}

[[gnu::no_instrument_function]] void profiler_show(const char* name) {
	u64 state = sdi();

	// sort it
	for (size_t i = 1; i < NUM_RECORDS; i++) {
		for (size_t j = i; j > 0 && pred(&RECORDS[j - 1], &RECORDS[j]); j--) {
			Record* a = &RECORDS[j - 1];
			Record* b = &RECORDS[j];

			Record temp = *a;
			*a = *b;
			*b = temp;
		}
	}

	// print it
	prints("profiler results for '");
	prints(name);
	prints("' (");
	printu(NUM_RECORDS);
	prints(" records):\n");

	for (size_t i = 0; i < NUM_RECORDS; i++) {
		printu(i + 1);
		prints(". 0x");
		printx((uintptr_t)RECORDS[i].fn);
		prints(": ");
		printu(RECORDS[i].total);
		prints(" (");
		printu(RECORDS[i].calls);
		prints(" calls, avg ");
		printu((RECORDS[i].total + (RECORDS[i].calls / 2)) / RECORDS[i].calls);
		prints(" per call)\n");
	}

	ri(state);
}

extern "C" [[gnu::no_instrument_function]] void __cyg_profile_func_enter(void* fn, void* call_site) {
	uint64_t start = __builtin_ia32_rdtsc();
	if (!__atomic_load_n(&PROFILER_ACTIVE, __ATOMIC_ACQUIRE)) return;

	size_t idx = CUR_FRAME++;
	if (idx >= MAX_FRAMES) {
		asm volatile("cli");
		prints("\ntoo many frames\n");
		for (;;) asm volatile("hlt");
	}

	CallFrame* frame = &FRAMES[idx];
	frame->fn = fn;
	frame->site = call_site;
	frame->ptime = 0;

	uint64_t end = __builtin_ia32_rdtsc();
	uint64_t time = end - start;

	for (size_t i = 0; i < idx; i++) {
		FRAMES[i].ptime += time;
	}

	frame->start = __builtin_ia32_rdtsc();
}

extern "C" [[gnu::no_instrument_function]] void __cyg_profile_func_exit(void *fn, void *call_site) {
	uint64_t start = __builtin_ia32_rdtsc();
	if (!__atomic_load_n(&PROFILER_ACTIVE, __ATOMIC_ACQUIRE)) return;

	u64 state = sdi();

	size_t idx = --CUR_FRAME;
	CallFrame* frame = &FRAMES[idx];
	uint64_t time = start - frame->start - frame->ptime;

	if (frame->fn != fn && frame->site != call_site) {
		asm volatile("cli");
		prints("\nframe mismatch\n");
		for (;;) asm volatile("hlt");
	}

	for (size_t i = 0; i < NUM_RECORDS; i++) {
		Record* record = &RECORDS[i];

		if (record->fn == fn) {
			record->total += time;
			record->calls += 1;
			ri(state);
			return;
		}
	}

	if (NUM_RECORDS == MAX_RECORDS) {
		asm volatile("cli");
		prints("\nmax records\n");
		for (;;) asm volatile("hlt");
	}

	Record* record = &RECORDS[NUM_RECORDS++];
	record->fn = fn;
	record->total = time;
	record->calls = 1;

	ri(state);

	uint64_t end = __builtin_ia32_rdtsc();
	time = end - start;

	for (size_t i = 0; i < idx; i++) {
		FRAMES[i].ptime += time;
	}
}
