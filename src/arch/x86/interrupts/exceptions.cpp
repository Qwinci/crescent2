#include "arch/irq.hpp"
#include "stdio.hpp"

struct Frame {
	Frame* rbp;
	u64 rip;
};

static void backtrace_display() {
	Frame frame {};
	Frame* frame_ptr;
	asm volatile("mov %0, rbp" : "=rm"(frame_ptr));
	int limit = 20;
	while (limit--) {
		if (reinterpret_cast<usize>(frame_ptr) < 0xFFFFFFFF80000000) {
			return;
		}
		frame = *frame_ptr;

		if (!frame.rbp) {
			break;
		}

		println(Fmt::Hex, frame.rip, Fmt::Reset);
		frame_ptr = frame.rbp;
	}
}

#define GENERATE_HANDLER(handler_name, desc) \
extern "C" [[gnu::used]] bool x86_ ##handler_name ## _handler(KEXCEPTION_FRAME* ex_frame) { \
	auto* frame = reinterpret_cast<KTRAP_FRAME*>(ex_frame->trap_frame); \
	panic("[kernel][x86]: EXCEPTION: " desc, " at ", Fmt::Hex, frame->rip, Fmt::Reset); \
	backtrace_display(); \
}

GENERATE_HANDLER(div, "division by zero")
GENERATE_HANDLER(debug, "debug exception")
GENERATE_HANDLER(nmi, "non maskable interrupt")
GENERATE_HANDLER(breakpoint, "breakpoint")
GENERATE_HANDLER(overflow, "overflow")
GENERATE_HANDLER(bound_range_exceeded, "bound range exceeded")
GENERATE_HANDLER(invalid_op, "invalid opcode")
GENERATE_HANDLER(device_not_available, "device not available")
GENERATE_HANDLER(double_fault, "double fault")
GENERATE_HANDLER(invalid_tss, "invalid tss")
GENERATE_HANDLER(seg_not_present, "segment not present")
GENERATE_HANDLER(stack_seg_fault, "stack segment fault")
GENERATE_HANDLER(x87_floating_point, "x87 floating point exception")
GENERATE_HANDLER(alignment_check, "alignment check")
GENERATE_HANDLER(machine_check, "machine check")
GENERATE_HANDLER(simd_exception, "simd exception")
GENERATE_HANDLER(virt_exception, "virtualization exception")
GENERATE_HANDLER(control_protection_exception, "control protection exception")
GENERATE_HANDLER(hypervisor_injection_exception, "hypervisor injection exception")
GENERATE_HANDLER(vmm_comm_exception, "vmm communication exception")
GENERATE_HANDLER(security_exception, "security exception")

extern "C" [[gnu::used]] bool x86_gp_fault_handler(KEXCEPTION_FRAME* ex_frame) {
	auto* frame = reinterpret_cast<KTRAP_FRAME*>(ex_frame->trap_frame);
	println("[kernel][x86]: EXCEPTION: general protection fault at ", Fmt::Hex, frame->rip, Fmt::Reset);
	backtrace_display();

	while (true) {
		asm volatile("hlt");
	}
}

extern "C" [[gnu::used]] bool x86_pagefault_handler(KEXCEPTION_FRAME* ex_frame) {
	auto* frame = reinterpret_cast<KTRAP_FRAME*>(ex_frame->trap_frame);

	u64 cr2;
	asm volatile("mov %0, cr2" : "=r"(cr2));

	auto error = frame->error_code;

	const char* who;
	if (error & 1 << 2) {
		who = "userspace";
	}
	else {
		who = "kernel";
	}

	const char* action;
	if (error & 1 << 1) {
		action = " write to ";
	}
	else {
		action = " read from ";
	}

	const char* desc;
	if (error & 1 << 6) {
		desc = "shadow stack access";
	}
	else if (error & 1 << 5) {
		desc = "protection key violation";
	}
	else if (error & 1 << 4) {
		desc = "instruction fetch";
	}
	else if (error & 1 << 3) {
		desc = "page table entry contains reserved bits";
	}
	else if (error & 1) {
		desc = "page protection violation";
	}
	else {
		desc = "non present page";
	}

	println("[kernel][x86]: pagefault caused by ", who, action, Fmt::Hex, cr2, " (", desc, ")");
	println("\tat ", frame->rip, Fmt::Reset);

	backtrace_display();

	while (true) {
		asm volatile("hlt");
	}
}