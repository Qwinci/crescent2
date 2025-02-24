#pragma once
#include "types.hpp"

enum class BUS_DATA_TYPE {
	PciConfiguration = 4
};

struct PCI_SLOT_NUMBER {
	union {
		struct {
			u32 device_number : 5;
			u32 function_number : 3;
			u32 reserved : 24;
		} bits;
		u32 as_u32;
	} u;
};

NTAPI extern "C" u32 HalGetBusDataByOffset(
	BUS_DATA_TYPE type,
	u32 bus_number,
	u32 slot_number,
	void* buffer,
	u32 offset,
	u32 length);
NTAPI extern "C" u32 HalSetBusDataByOffset(
	BUS_DATA_TYPE type,
	u32 bus_number,
	u32 slot_number,
	void* buffer,
	u32 offset,
	u32 length);
