#include "arch/arch_thread.hpp"
#include "sched/thread.hpp"
#include "arch/cpu.hpp"
#include "cpu.hpp"
#include "mem/vspace.hpp"
#include "simd_state.hpp"

asm(R"(
.pushsection .text
.globl sched_switch_thread
.hidden sched_switch_thread
.type sched_switch_thread, @function

// void sched_switch_thread(ArchThread* prev, ArchThread* next, KIRQL new_irql)
sched_switch_thread:
	push %rbx
	push %rcx
	push %rbp
	push %rdi
	push %rsi
	push %r12
	push %r13
	push %r14
	push %r15
	sub $(6 * 16), %rsp
	movaps %xmm0, 0(%rsp)
	movaps %xmm1, 16(%rsp)
	movaps %xmm2, 32(%rsp)
	movaps %xmm3, 48(%rsp)
	movaps %xmm4, 64(%rsp)
	movaps %xmm5, 80(%rsp)

	mov %rsp, 8(%rcx)

	// current->lock = false
	movq $0, 184(%rcx)

	mov 8(%rdx), %rsp

	movaps 0(%rsp), %xmm0
	movaps 16(%rsp), %xmm1
	movaps 32(%rsp), %xmm2
	movaps 48(%rsp), %xmm3
	movaps 64(%rsp), %xmm4
	movaps 80(%rsp), %xmm5
	add $(6 * 16), %rsp

	pop %r15
	pop %r14
	pop %r13
	pop %r12
	pop %rsi
	pop %rdi
	pop %rbp
	pop %rcx
	pop %rbx

	// lower irql
	mov %r8, %cr8

	ret
.popsection
)");

void sched_before_switch(Thread* prev, Thread* thread) {
	if (prev->process->user) {
		if (CPU_FEATURES.xsave) {
			xsave(prev->simd, ~0);
		}
		else {
			asm volatile("fxsaveq %0" : : "m"(*prev->simd) : "memory");
		}
	}

	if (thread->process->user) {
		if (CPU_FEATURES.xsave) {
			xrstor(thread->simd, ~0);
		}
		else {
			asm volatile("fxrstorq %0" : : "m"(*thread->simd) : "memory");
		}

		msrs::IA32_FSBASE.write(thread->fs_base);
		msrs::IA32_KERNEL_GSBASE.write(thread->gs_base);
	}

	auto& tss = thread->cpu->tss;
	auto kernel_sp = reinterpret_cast<usize>(thread->syscall_sp);
	tss.rsp0_low = kernel_sp;
	tss.rsp0_high = kernel_sp >> 32;
}

struct InitFrame {
	u8 xmm[6][16];
	u64 r15;
	u64 r14;
	u64 r13;
	u64 r12;
	u64 rsi;
	u64 rdi;
	u64 rbp;
	u64 rcx;
	u64 rbx;
	u64 on_first_switch;
	u64 rip;
	u64 pad;
};

struct UserInitFrame {
	InitFrame kernel;
	u64 rflags;
	u64 rsp;
};

namespace {
	constexpr usize KERNEL_STACK_SIZE = 1024 * 64;
	constexpr usize USER_STACK_SIZE = 1024 * 1024;
}

asm(R"(
.pushsection .text
.globl arch_on_first_switch
.hidden arch_on_first_switch
arch_on_first_switch:
	ret

.globl arch_on_first_switch_user
.hidden arch_on_first_switch_user
arch_on_first_switch_user:
	xor %eax, %eax
	xor %ebx, %ebx
	xor %edx, %edx
	xor %r8d, %r8d
	xor %r9d, %r9d
	xor %r10d, %r10d
	xor %r11d, %r11d
	xor %r12d, %r12d
	xor %r13d, %r13d
	xor %r14d, %r14d
	xor %r15d, %r15d

	pop %rcx
	pop %r11
	pop %rsp
	swapgs
	sysretq
.popsection
)");

extern "C" void arch_on_first_switch();
extern "C" void arch_on_first_switch_user();

ArchThread::ArchThread(void (*fn)(void*), void* arg, Process* process) {
	kernel_stack_base = new u8[KERNEL_STACK_SIZE + PAGE_SIZE] {};
	syscall_sp = kernel_stack_base + KERNEL_STACK_SIZE + PAGE_SIZE;
	sp = kernel_stack_base + KERNEL_STACK_SIZE + PAGE_SIZE -
		(process->user ? sizeof(UserInitFrame) : sizeof(InitFrame));
	auto* frame = reinterpret_cast<InitFrame*>(sp);
	frame->rcx = reinterpret_cast<u64>(arg);

	KERNEL_PROCESS->page_map.protect(reinterpret_cast<u64>(kernel_stack_base), PageFlags::Read, CacheMode::WriteBack);

	frame->rip = reinterpret_cast<u64>(fn);

	if (process->user) {
		user_stack_base = process->allocate(
			nullptr,
			USER_STACK_SIZE + PAGE_SIZE,
			PageFlags::Read | PageFlags::Write,
			nullptr);
		assert(user_stack_base);

		process->page_map.protect(user_stack_base, PageFlags::User | PageFlags::Read, CacheMode::WriteBack);

		auto simd_size = CPU_FEATURES.xsave ? CPU_FEATURES.xsave_area_size : sizeof(FxState);
		simd = static_cast<u8*>(KERNEL_VSPACE.alloc_backed(0, simd_size, PageFlags::Read | PageFlags::Write));
		assert(simd);

		auto* fx = reinterpret_cast<FxState*>(simd);
		// IM | DM | ZM | OM | UM | PM | PC
		fx->fcw = 1 << 0 | 1 << 1 | 1 << 2 | 1 << 3 | 1 << 4 | 1 << 5 | 0b11 << 8;
		fx->mxcsr = 0b1111110000000;

		frame->on_first_switch = reinterpret_cast<u64>(arch_on_first_switch_user);
		auto* user_frame = reinterpret_cast<UserInitFrame*>(frame);
		user_frame->rflags = 0x202;
		user_frame->rsp = user_stack_base + USER_STACK_SIZE + PAGE_SIZE;
	}
	else {
		frame->on_first_switch = reinterpret_cast<u64>(arch_on_first_switch);
	}
}

ArchThread::~ArchThread() {
	kfree(kernel_stack_base, KERNEL_STACK_SIZE + PAGE_SIZE);
	if (user_stack_base) {
		static_cast<Thread*>(this)->process->free(user_stack_base, USER_STACK_SIZE + PAGE_SIZE);
	}
	if (simd) {
		auto simd_size = CPU_FEATURES.xsave ? CPU_FEATURES.xsave_area_size : sizeof(FxState);
		KERNEL_VSPACE.free_backed(simd, simd_size);
	}
}
