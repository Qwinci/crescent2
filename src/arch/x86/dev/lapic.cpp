#include "lapic.hpp"
#include "mem/iospace.hpp"
#include "arch/irq.hpp"
#include "arch/cpu.hpp"
#include "arch/x86/cpu.hpp"
#include "x86/irq.hpp"
#include "dev/irq.hpp"

namespace regs {
	static constexpr BasicRegister<u32> TPR {0x80};
	static constexpr BasicRegister<u32> EOI {0xB0};
	static constexpr BasicRegister<u32> SVR {0xF0};
	static constexpr BitRegister<u32> ICR0 {0x300};
	static constexpr BitRegister<u32> ICR1 {0x310};
	static constexpr BasicRegister<u32> LVT_TIMER {0x320};
	static constexpr BasicRegister<u32> INIT_COUNT {0x380};
	static constexpr BasicRegister<u32> CURR_COUNT {0x390};
	static constexpr BasicRegister<u32> DIVIDE_CONFIG {0x3E0};
}

namespace icr {
	static constexpr BitField<u32, u8> VECTOR {0, 8};
	static constexpr BitField<u32, u8> DELIVERY_MODE {8, 3};
	static constexpr BitField<u32, bool> PENDING {12, 1};
	static constexpr BitField<u32, bool> LEVEL {14, 1};
	static constexpr BitField<u32, u8> DEST_SHORTHAND {18, 2};
	static constexpr BitField<u32, u8> DEST {24, 8};

	static constexpr u8 DEST_SELF = 1;
	static constexpr u8 DEST_ALL_EXCL_SELF = 0b11;

	static constexpr u8 DELIVERY_INIT = 0b101;
	static constexpr u8 DELIVERY_SIPI = 0b110;
}

namespace divide_config {
	static constexpr u32 DIV_BY_2 = 0b0000;
	static constexpr u32 DIV_BY_4 = 0b0001;
	static constexpr u32 DIV_BY_8 = 0b0010;
	static constexpr u32 DIV_BY_16 = 0b0011;
	static constexpr u32 DIV_BY_32 = 0b1000;
	static constexpr u32 DIV_BY_64 = 0b1001;
	static constexpr u32 DIV_BY_128 = 0b1010;
	static constexpr u32 DIV_BY_1 = 0b1011;
}

namespace lvt_timer {
	static constexpr BitField<u32, u8> MODE {17, 2};

	static constexpr u8 MODE_ONE_SHOT = 0b00;
	static constexpr u8 MODE_PERIODIC = 0b01;
	static constexpr u8 MODE_TSC_DEADLINE = 0b10;
}

namespace lvt {
	static constexpr BitField<u32, u8> VECTOR {0, 8};
	static constexpr BitField<u32, bool> MASK {16, 1};
}

namespace {
	IoSpace SPACE {};
	u32 TMP_IRQ {};
	u32 TIMER_IRQ {};
	KINTERRUPT LAPIC_IRQ_HANDLER {
		.fn = &LapicTickSource::on_irq,
		.can_be_shared = false
	};
}

static void lapic_timer_calibrate(Cpu* cpu, bool initial) {
	SPACE.store(regs::DIVIDE_CONFIG, divide_config::DIV_BY_16);

	KINTERRUPT tmp_handler {
		.fn = [](KINTERRUPT*, void*) {
			return true;
		},
		.can_be_shared = false
	};
	tmp_handler.vector = TMP_IRQ;
	register_irq_handler(&tmp_handler);

	SPACE.store(
		regs::LVT_TIMER,
		lvt::VECTOR(TMP_IRQ) | lvt_timer::MODE(lvt_timer::MODE_ONE_SHOT));
	SPACE.store(regs::INIT_COUNT, 0xFFFFFFFF);

	mdelay(10);

	SPACE.store(regs::LVT_TIMER, lvt::MASK(true));
	u64 ticks_in_10ms = 0xFFFFFFFF - SPACE.load(regs::CURR_COUNT);
	u64 ticks_in_ms = ticks_in_10ms / 10;
	u64 ticks_in_us = ticks_in_ms / 1000;
	if (!ticks_in_us) {
		ticks_in_us = 1;
	}

	if (initial) {
		cpu->tick_source.initialize(ticks_in_us, 0xFFFFFFFF / ticks_in_us);
	}
	else {
		cpu->tick_source->max_us = 0xFFFFFFFF / ticks_in_us;
		cpu->tick_source->ticks_in_us = ticks_in_us;
	}

	SPACE.store(regs::DIVIDE_CONFIG, divide_config::DIV_BY_16);
	deregister_irq_handler(&tmp_handler);
}

void lapic_first_init() {
	SPACE.set_phys(msrs::IA32_APIC_BASE.read() & ~0xFFF, 0x1000);
	auto status = SPACE.map(CacheMode::Uncached);
	assert(status);
	TIMER_IRQ = x86_alloc_irq(1, CLOCK_LEVEL, false);
	assert(TIMER_IRQ);
	TMP_IRQ = x86_alloc_irq(1, CLOCK_LEVEL, false);
	assert(TMP_IRQ);

	LAPIC_IRQ_HANDLER.vector = TIMER_IRQ;
	register_irq_handler(&LAPIC_IRQ_HANDLER);
}

void lapic_init(Cpu* cpu, bool initial) {
	SPACE.store(regs::SVR, 0x1FF);
	SPACE.store(regs::TPR, 0);
	lapic_timer_calibrate(cpu, initial);
}

void lapic_ipi_self(u8 vec) {
	SPACE.store(regs::ICR1, 0);
	SPACE.store(
		regs::ICR0,
		icr::VECTOR(vec) |
		icr::LEVEL(true) |
		icr::DEST_SHORTHAND(icr::DEST_SELF));
}

void lapic_eoi() {
	SPACE.store(regs::EOI, 0);
}

void LapicTickSource::oneshot(u64 us) {
	u64 value = ticks_in_us * us;
	assert(value <= UINT32_MAX);
	SPACE.store(
		regs::LVT_TIMER,
		lvt::VECTOR(TIMER_IRQ) | lvt_timer::MODE(lvt_timer::MODE_ONE_SHOT));
	SPACE.store(regs::INIT_COUNT, value);
}

bool LapicTickSource::on_irq(KINTERRUPT*, void*) {
	get_current_cpu()->tick_source->callback_producer.signal_all();
	return true;
}
