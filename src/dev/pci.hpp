#pragma once
#include "types.hpp"

namespace pci {
	struct PciAddress {
		u16 seg;
		u8 bus;
		u8 dev;
		u8 func;
	};

	u32 read(PciAddress addr, u32 offset, u8 size);
	void write(PciAddress addr, u32 offset, u8 size, u32 value);

	extern bool PCIE_SUPPORTED;
}
