#pragma once
#include "arch/x86/interrupts/tss.hpp"
#include "arch/x86/dev/lapic.hpp"
#include <hz/manually_init.hpp>

struct Thread;

struct ArchCpu {
	constexpr explicit ArchCpu(u32 number) : legacy_number {static_cast<u8>(number)}, number {number} {}

	ArchCpu* self {this};
	u32 irql {};
	u32 lapic_id {};
	u64 tmp_syscall_reg {};
	u8 hw_irql {};
	u8 pad0[7] {};
	void* extension {&kernel_mxcsr};
	Tss tss {};
	hz::manually_init<LapicTickSource> tick_source {};
	char pad1[160] {};

	char pad2[8] {};

	// extension begins here
	u32 kernel_mxcsr {};
	u8 legacy_number {};
	u8 pad3[3] {};
	Thread* current_thread {};
	Thread* next_thread {};
	u8 pad4[12] {};
	u32 number {};
};

static_assert(offsetof(ArchCpu, irql) == 8);
// user in usermode.S
static_assert(offsetof(ArchCpu, tmp_syscall_reg) == 16);
static_assert(offsetof(ArchCpu, tss) == 40);
static_assert(offsetof(Tss, rsp0_low) == 4);

static_assert(offsetof(ArchCpu, hw_irql) == 24);

static_assert(offsetof(ArchCpu, pad2) == 0x178);

static_assert(offsetof(ArchCpu, extension) == 0x20);
static_assert(offsetof(ArchCpu, legacy_number) == 0x184);
// used in usermode.S too
static_assert(offsetof(ArchCpu, current_thread) == 0x188);
static_assert(offsetof(ArchCpu, number) == 0x1A4);

inline Cpu* get_current_cpu() {
	Cpu* cpu;
	asm volatile("mov %0, gs:0" : "=r"(cpu));
	return cpu;
}

inline u64 get_cycle_count() {
	u32 low;
	u32 high;
	asm volatile("lfence; rdtsc" : "=a"(low), "=d"(high));
	return static_cast<u64>(high) << 32 | low;
}
