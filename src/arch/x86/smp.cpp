#include "smp.hpp"
#include "arch/cpu.hpp"
#include "stdio.hpp"
#include "arch/x86/interrupts/gdt.hpp"
#include "arch/x86/interrupts/idt.hpp"
#include "cpu.hpp"
#include "cpuid.hpp"
#include "utils/shared_data.hpp"
#include "sched/sched.hpp"
#include "loader/limine.h"

namespace {
	hz::atomic<u32> NUM_CPUS {1};
	hz::spinlock<void> SMP_LOCK;

	[[gnu::section(".limine_requests")]] volatile limine_smp_request SMP_REQUEST {
		.id = LIMINE_SMP_REQUEST,
		.revision = 0,
		.response = nullptr,
		.flags = 0
	};
}

CpuFeatures CPU_FEATURES;

extern "C" void x86_syscall_stub();

static constexpr u64 USER_GDT_BASE = 0x10;
static constexpr u64 KERNEL_CS = 0x8;

static void init_usermode() {
	// enable syscall
	auto efer = msrs::IA32_EFER.read();
	efer |= 1;
	msrs::IA32_EFER.write(efer);

	msrs::IA32_LSTAR.write(reinterpret_cast<u64>(x86_syscall_stub));
	// disable interrupts on syscall entry
	msrs::IA32_SFMASK.write(0x200);
	u64 star = USER_GDT_BASE << 48 | KERNEL_CS << 32;
	msrs::IA32_STAR.write(star);
}

static void x86_detect_cpu_features() {
	auto data = SharedUserData;
	data->processor_features[PF_COMPARE_EXCHANGE_DOUBLE] = true;
	data->processor_features[PF_PAE_ENABLED] = true;
	data->processor_features[PF_NX_ENABLED] = true;
	data->processor_features[PF_FASTFAIL_AVAILABLE] = true;

	Cpuid info = cpuid(1, 0);
	if (info.edx & 1 << 23) {
		data->processor_features[PF_MMX_INSTRUCTIONS_AVAILABLE] = true;
	}
	if (info.ecx & 1 << 0) {
		data->processor_features[PF_SSE3_INSTRUCTIONS_AVAILABLE] = true;
	}
	if (info.ecx & 1 << 13) {
		data->processor_features[PF_COMPARE_EXCHANGE128] = true;
	}
	if (info.ecx & 1 << 26) {
		data->processor_features[PF_XSAVE_ENABLED] = true;
		CPU_FEATURES.xsave = true;

		if (info.ecx & 1 << 28) {
			CPU_FEATURES.avx = true;
		}

		CPU_FEATURES.xsave_area_size = cpuid(0xD, 0).ecx;

		data->xstate.all_feature_size = CPU_FEATURES.xsave_area_size;
	}
	if (info.ecx & 1 << 30) {
		data->processor_features[PF_RDRAND_INSTRUCTION_AVAILABLE] = true;
		CPU_FEATURES.rdrnd = true;
	}
	if (info.ecx & 1 << 9) {
		data->processor_features[PF_SSSE3_INSTRUCTIONS_AVAILABLE] = true;
	}
	if (info.ecx & 1 << 19) {
		data->processor_features[PF_SSE4_1_INSTRUCTIONS_AVAILABLE] = true;
	}
	if (info.ecx & 1 << 20) {
		data->processor_features[PF_SSE4_2_INSTRUCTIONS_AVAILABLE] = true;
	}
	if (info.ecx & 1 << 28) {
		data->processor_features[PF_AVX_INSTRUCTIONS_AVAILABLE] = true;
	}

	info = cpuid(7, 0);
	if (info.ecx & 1 << 2) {
		data->processor_features[PF_CHANNELS_ENABLED] = true;
		CPU_FEATURES.umip = true;
	}
	if (info.ebx & 1 << 7) {
		CPU_FEATURES.smep = true;
	}
	if (info.ebx & 1 << 20) {
		CPU_FEATURES.smap = true;
	}
	if (info.ebx & 1 << 18) {
		CPU_FEATURES.rdseed = true;
	}
	if (info.ebx & 1 << 0) {
		data->processor_features[PF_RDWRFSGSBASE_AVAILABLE] = true;
	}
	if (info.ebx & 1 << 16) {
		CPU_FEATURES.avx512 = true;
	}
	if (info.ecx & 1 << 22) {
		data->processor_features[PF_RDPID_INSTRUCTION_AVAILABLE] = true;
	}
	if (info.ebx & 1 << 5) {
		data->processor_features[PF_AVX2_INSTRUCTIONS_AVAILABLE] = true;
	}
	if (info.ebx & 1 << 9) {
		data->processor_features[PF_ERMS_AVAILABLE] = true;
	}

	if (CPU_FEATURES.avx) {
		data->xstate.enabled_features |= 1 << 2;
	}
	if (CPU_FEATURES.avx512) {
		data->xstate.enabled_features |= 1U << 5 | 1U << 6 | 1U << 7;
	}
}

static void x86_init_simd() {
	u64 cr0;
	asm volatile("mov %%cr0, %0" : "=r"(cr0));
	// clear EM
	cr0 &= ~(1 << 2);
	// set MP
	cr0 |= 1 << 1;
	asm volatile("mov %0, %%cr0" : : "r"(cr0));

	u64 cr4;
	asm volatile("mov %%cr4, %0" : "=r"(cr4));
	// set OSXMMEXCPT and OSFXSR
	cr4 |= 1 << 10 | 1 << 9;
	if (CPU_FEATURES.xsave) {
		// set OSXSAVE
		cr4 |= 1 << 18;
	}
	asm volatile("mov %0, %%cr4" : : "r"(cr4));

	if (CPU_FEATURES.xsave) {
		// x87 and SSE
		u64 xcr0 = 1 << 0 | 1 << 1;

		if (CPU_FEATURES.avx) {
			xcr0 |= 1 << 2;
		}

		if (CPU_FEATURES.avx512) {
			xcr0 |= 1U << 5;
			xcr0 |= 1U << 6;
			xcr0 |= 1U << 7;
		}

		wrxcr(0, xcr0);
	}
}

static void x86_cpu_resume(Cpu* self, Thread* current_thread, bool initial) {
	lapic_init(self, initial);

	asm volatile("mov $6 * 8, %%eax; ltr %%ax" : : : "eax");

	init_usermode();
	x86_init_simd();

	u64 cr4;
	asm volatile("mov %%cr4, %0" : "=r"(cr4));
	if (CPU_FEATURES.umip) {
		cr4 |= 1U << 11;
	}
	if (CPU_FEATURES.smep) {
		cr4 |= 1U << 20;
	}
	if (CPU_FEATURES.smap) {
		cr4 |= 1U << 21;
	}
	asm volatile("mov %0, %%cr4" : : "r"(cr4));

	msrs::IA32_GSBASE.write(reinterpret_cast<u64>(self));
	self->current_thread = current_thread;
}

static void x86_init_cpu_common(Cpu* self, u32 lapic_id) NO_THREAD_SAFETY_ANALYSIS {
	self->number = NUM_CPUS.fetch_add(1, hz::memory_order::relaxed);
	self->lapic_id = lapic_id;

	x86_load_gdt(&self->tss);
	x86_load_idt();

	auto* thread = new Thread {u"kernel main", self, &*KERNEL_PROCESS};
	thread->status = ThreadStatus::Running;

	self->tss.iopb = sizeof(Tss);
	x86_cpu_resume(self, thread, true);
	sched_init();
}

[[noreturn, gnu::sysv_abi]] static void smp_ap_entry(limine_smp_info* info) {
	KERNEL_PROCESS->page_map.use();
	auto* cpu = reinterpret_cast<Cpu*>(info->extra_argument);

	{
		auto guard = SMP_LOCK.lock();

		x86_init_cpu_common(cpu, info->lapic_id);

		cpu->tick_source->callback_producer.add_callback([cpu]() {
			Scheduler::on_timer(cpu);
		});
	}

	cpu->tick_source->oneshot(Scheduler::CLOCK_INTERVAL_MS * 1000);
	cpu->scheduler.block();
	panic("[kernel][x86]: scheduler block returned in ap entry");
}

void x86_irq_init();

void x86_smp_init() {
	lapic_first_init();
	x86_init_idt();
	x86_irq_init();
	x86_detect_cpu_features();

	auto resp = SMP_REQUEST.response;

	println("[kernel][x86]: smp init");
	CPUS.resize(resp->cpu_count);
	CPUS[0] = new Cpu {0};
	CPUS[0]->tss.iopb = sizeof(Tss);
	x86_init_cpu_common(CPUS[0], resp->bsp_lapic_id);

	u64 index = 1;
	for (u64 i = 0; i < resp->cpu_count; ++i) {
		auto* cpu = resp->cpus[i];
		if (cpu->lapic_id == resp->bsp_lapic_id) {
			continue;
		}

		CPUS[index] = new Cpu {static_cast<u32>(index)};

		u64 prev = NUM_CPUS.load(hz::memory_order::relaxed);

		cpu->extra_argument = reinterpret_cast<u64>(CPUS[i]);
		__atomic_store_n(
			&cpu->goto_address,
			reinterpret_cast<limine_goto_address>(smp_ap_entry),
			__ATOMIC_SEQ_CST);

		while (NUM_CPUS.load(hz::memory_order::relaxed) != prev + 1) {
			__builtin_ia32_pause();
		}

		++index;
	}

	{
		auto guard = SMP_LOCK.lock();

		CPUS[0]->tick_source->callback_producer.add_callback([]() {
			Scheduler::on_timer(CPUS[0]);
		});
	}

	CPUS[0]->tick_source->oneshot(Scheduler::CLOCK_INTERVAL_MS * 1000);
}
