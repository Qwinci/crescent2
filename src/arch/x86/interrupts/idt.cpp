#include "idt.hpp"
#include "types.hpp"

struct IdtEntry {
	u16 offset0;
	u16 selector;
	u16 ist_flags;
	u16 offset1;
	u32 offset2;
	u32 reserved;
};

struct [[gnu::packed]] Idtr {
	u16 limit;
	u64 base;
};

static IdtEntry IDT[256];

static void set_idt_entry(u8 i, void* handler, u8 selector, u8 ist, u8 dpl) {
	auto addr = reinterpret_cast<usize>(handler);
	auto& entry = IDT[i];
	entry.offset0 = addr;
	entry.offset1 = addr >> 16;
	entry.offset2 = addr >> 32;
	entry.selector = selector;
	entry.ist_flags = (ist & 0b111) | (0xE << 8) | (dpl & 0b11) << 13 | 1 << 15;
}

extern "C" void* X86_IRQ_STUBS[];
extern "C" void* X86_EXC_STUBS[];

void x86_init_idt() {
	for (u32 i = 0; i < 32; ++i) {
		set_idt_entry(i, X86_EXC_STUBS[i], 0x8, 0, 0);
	}

	for (u32 i = 32; i < 256; ++i) {
		set_idt_entry(i, X86_IRQ_STUBS[i - 32], 0x8, 0, 0);
	}

	x86_load_idt();
}

void x86_load_idt() {
	Idtr idtr {
		.limit = 0x1000 - 1,
		.base = reinterpret_cast<u64>(IDT)
	};
	asm volatile("cli; lidt %0; sti" : : "m"(idtr));
}
