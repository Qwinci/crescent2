#include "hal.hpp"
#include "utils/export.hpp"
#include "pci.hpp"
#include "assert.hpp"
#include "cstring.hpp"
#include <hz/algorithm.hpp>

EXPORT u32 HalGetBusDataByOffset(
	BUS_DATA_TYPE,
	u32 bus_number,
	u32 slot_number,
	void* buffer,
	u32 offset,
	u32 length) {
	length = hz::min(length, pci::PCIE_SUPPORTED ? 0x1000U : 256U);

	PCI_SLOT_NUMBER slot {
		.u {
			.as_u32 = slot_number
		}
	};

	pci::PciAddress addr {
		.seg = 0,
		.bus = static_cast<u8>(bus_number),
		.dev = static_cast<u8>(slot.u.bits.device_number),
		.func = static_cast<u8>(slot.u.bits.function_number)
	};

	if (length >= 4) {
		assert(offset % 4 == 0);
		assert(length % 4 == 0);

		for (u32 i = offset; i < offset + length; i += 4) {
			u32 value = pci::read(addr, i, 4);
			memcpy(offset(buffer, void*, i), &value, 4);
		}

		return length;
	}
	else if (length == 2) {
		u32 value = pci::read(addr, offset, 2);
		memcpy(buffer, &value, 2);
		return 2;
	}
	else if (length == 1) {
		u32 value = pci::read(addr, offset, 1);
		memcpy(buffer, &value, 1);
		return 1;
	}

	return 0;
}

EXPORT u32 HalSetBusDataByOffset(
	BUS_DATA_TYPE,
	u32 bus_number,
	u32 slot_number,
	void* buffer,
	u32 offset,
	u32 length) {
	length = hz::min(length, pci::PCIE_SUPPORTED ? 0x1000U : 256U);

	PCI_SLOT_NUMBER slot {
		.u {
			.as_u32 = slot_number
		}
	};

	pci::PciAddress addr {
		.seg = 0,
		.bus = static_cast<u8>(bus_number),
		.dev = static_cast<u8>(slot.u.bits.device_number),
		.func = static_cast<u8>(slot.u.bits.function_number)
	};

	if (length >= 4) {
		assert(offset % 4 == 0);
		assert(length % 4 == 0);

		for (u32 i = 0; i < length; i += 4) {
			u32 value;
			memcpy(&value, offset(buffer, void*, i), 4);
			pci::write(addr, offset + i, 4, value);
		}

		return length;
	}
	else if (length == 2) {
		u32 value = 0;
		memcpy(&value, buffer, 2);
		pci::write(addr, offset, 2, value);
		return 2;
	}
	else if (length == 1) {
		u32 value = 0;
		memcpy(&value, buffer, 1);
		pci::write(addr, offset, 1, value);
		return 1;
	}

	return 0;
}
