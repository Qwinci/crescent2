#include "hpet.hpp"
#include "acpi/acpi.hpp"
#include "mem/mem.hpp"
#include "mem/iospace.hpp"
#include "assert.hpp"
#include "dev/clock.hpp"
#include <hz/manually_init.hpp>

struct [[gnu::packed]] HpetTable {
	acpi::SdtHeader hdr;
	u8 hw_rev_id;
	u8 flags;
	u16 pci_vendor_id;
	acpi::Address address;
	u8 hpet_num;
	u16 min_tick;
	u8 page_protection;
};

namespace regs {
	static constexpr BitRegister<u64> GCIDR {0};
	static constexpr BitRegister<u64> GCR {0x10};
	static constexpr BasicRegister<u64> MCVR {0xF0};
}

namespace gcidr {
	static constexpr BitField<u64, u32> COUNTER_CLK_PERIOD {32, 32};
}

namespace gcr {
	static constexpr BitField<u64, bool> ENABLE_CNF {0, 1};
}

namespace {
	constinit IoSpace SPACE {};
	u64 NS_IN_TICK = 0;
}

struct HpetClockSource : public ClockSource {
	explicit HpetClockSource(u64 freq) : ClockSource {"hpet", freq} {
		register_clock_source(this);
	}

	u64 get_ns() override {
		return SPACE.load(regs::MCVR) * NS_IN_TICK;
	}
};

static hz::manually_init<HpetClockSource> HPET;

void hpet_init() {
	auto* table = static_cast<const HpetTable*>(acpi::get_table("HPET"));
	if (!table) {
		return;
	}

	assert(table->address.space_id == acpi::RegionSpace::SystemMemory);
	SPACE.set_phys(table->address.address, 0x1000);
	auto status = SPACE.map(CacheMode::Uncached);
	assert(status);

	auto gcidr = SPACE.load(regs::GCIDR);

	auto fs_in_tick = gcidr & gcidr::COUNTER_CLK_PERIOD;
	auto ns_in_tick = fs_in_tick / 1000000;

	// enable main counter
	SPACE.store(regs::GCR, gcr::ENABLE_CNF(true));

	NS_IN_TICK = ns_in_tick;

	HPET.initialize(NS_IN_S / ns_in_tick);
}