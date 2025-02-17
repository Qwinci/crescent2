#include "wdm.h"
#include "aux_klib.h"
#include <stdint.h>
#include <wchar.h>

struct [[gnu::packed]] AcpiSdtHeader {
	char signature[4];
	uint32_t length;
	uint8_t revision;
	uint8_t checksum;
	char oem_id[6];
	char oem_table_id[8];
	uint32_t oem_revision;
	char creator_id[4];
	uint32_t creator_revision;
};

struct [[gnu::packed]] Mcfg {
	struct Entry {
		uint64_t base;
		uint16_t seg;
		uint8_t start;
		uint8_t end;
		uint32_t reserved;
	};

	AcpiSdtHeader hdr;
	uint32_t reserved[2];
	Entry entries[];
};

struct PciAddress {
	uint16_t seg;
	uint8_t bus;
	uint8_t dev;
	uint8_t func;

	constexpr bool operator==(const PciAddress& other) const = default;
};

namespace {
	Mcfg* GLOBAL_MCFG;
}

static char* mcfg_get_space(const PciAddress& addr) {
	uint32_t entries = (GLOBAL_MCFG->hdr.length - sizeof(Mcfg)) / sizeof(Mcfg::Entry);
	for (uint32_t i = 0; i < entries; ++i) {
		auto& entry = GLOBAL_MCFG->entries[i];

		if (entry.seg == addr.seg) {
			char* res = reinterpret_cast<char*>(entry.base);
			res += static_cast<size_t>(addr.bus) << 20;
			res += static_cast<size_t>(addr.dev) << 15;
			res += static_cast<size_t>(addr.func) << 12;
			return res;
		}
	}

	return nullptr;
}

#ifdef __x86_64__
namespace x86 {
	static inline uint8_t in1(uint16_t port) {
		uint8_t value;
		asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
		return value;
	}

	static inline uint16_t in2(uint16_t port) {
		uint16_t value;
		asm volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
		return value;
	}

	static inline uint32_t in4(uint16_t port) {
		uint32_t value;
		asm volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
		return value;
	}

	static inline void out1(uint16_t port, uint8_t value) {
		asm volatile("outb %1, %0" : : "Nd"(port), "a"(value));
	}

	static inline void out2(uint16_t port, uint16_t value) {
		asm volatile("outw %1, %0" : : "Nd"(port), "a"(value));
	}

	static inline void out4(uint16_t port, uint32_t value) {
		asm volatile("outl %1, %0" : : "Nd"(port), "a"(value));
	}
}

static constexpr uint16_t CFG_BASE = 0xCF8;
static constexpr uint16_t CFG_ADDR_OFFSET = 0;
static constexpr uint16_t CFG_DATA_OFFSET = 4;

#endif

static uint32_t pci_read(PciAddress addr, uint32_t offset, uint8_t size) {
	uint32_t align = offset & 0b11;
	offset &= ~0b11;

	uint32_t value;
	if (GLOBAL_MCFG) {
		char* ptr = mcfg_get_space(addr);
#ifdef __x86_64__
		asm volatile("mov %1, %0" : "=a"(value) : "m"(*(ptr + offset)));
#else
#error pci mcfg read unsupported on architecture
#endif
	}
	else {
#ifdef __x86_64__
		uint32_t io_addr = 1 << 31;
		io_addr |= addr.bus << 16;
		io_addr |= addr.dev << 11;
		io_addr |= addr.func << 8;
		io_addr |= offset;

		x86::out4(CFG_BASE + CFG_ADDR_OFFSET, io_addr);
		value = x86::in4(CFG_BASE + CFG_DATA_OFFSET);
#else
		panic("[kernel][pci]: pci read unsupported on architecture");
#endif
	}

	uint32_t shift = 32 - (size * 8);
	return (value >> (align * 8)) << shift >> shift;
}

static void pci_write(PciAddress addr, uint32_t offset, uint8_t size, uint32_t value) {
	uint32_t align = offset & 0b11;
	offset &= ~0b11;

	uint32_t res = pci_read(addr, offset, size);
	res &= ~(((uint64_t {1} << (size * 8)) - 1) << (align * 8));
	res |= value << (align * 8);

	if (GLOBAL_MCFG) {
		char* ptr = mcfg_get_space(addr);
#ifdef __x86_64__
		asm volatile("mov %0, %1" : : "a"(res), "m"(*(ptr + offset)));
#else
#error pci mcfg write unsupported on architecture
#endif
	}
	else {
#ifdef __x86_64__
		uint32_t io_addr = 1 << 31;
		io_addr |= addr.bus << 16;
		io_addr |= addr.dev << 11;
		io_addr |= addr.func << 8;
		io_addr |= offset;

		x86::out4(CFG_BASE + CFG_ADDR_OFFSET, io_addr);
		x86::out4(CFG_BASE + CFG_DATA_OFFSET, res);
#else
		panic("[kernel][pci]: pci write unsupported on architecture");
#endif
	}
}

struct PciExtension {
	PciAddress addr;
	uint16_t vendor;
	uint16_t device;
	uint8_t clazz;
	uint8_t subclass;
	uint8_t prog_if;
	uint8_t revision;
	uint8_t type;
	uint16_t subsys_vendor;
	uint16_t subsys_id;
};

static void enumerate(PDRIVER_OBJECT driver, PDEVICE_OBJECT* device_objects, size_t* count) {
	size_t actual = 0;

	PciAddress addr {};

	auto enumerate_bus = [&](uint32_t bus_num) {
		addr.bus = bus_num;

		for (addr.dev = 0; addr.dev < 32; ++addr.dev) {
			addr.func = 0;

			uint16_t vendor_id = pci_read(addr, 0, 2);
			if (vendor_id == 0xFFFF) {
				continue;
			}

			uint8_t type = pci_read(addr, 14, 1);
			uint32_t max_func;
			if (type & 1 << 7) {
				max_func = 8;
			}
			else {
				max_func = 1;
			}

			for (uint32_t func = 0; func < max_func; ++func) {
				addr.func = func;

				vendor_id = pci_read(addr, 0, 2);
				if (vendor_id == 0xFFFF) {
					continue;
				}

				++actual;
				if (device_objects) {
					PDEVICE_OBJECT existing;
					for (existing = driver->DeviceObject; existing; existing = existing->NextDevice) {
						auto* ext = static_cast<PciExtension*>(existing->DeviceExtension);
						if (ext->addr == addr) {
							break;
						}
					}

					if (existing) {
						device_objects[actual - 1] = existing;
						continue;
					}

					PDEVICE_OBJECT new_device;
					auto status = IoCreateDevice(
						driver,
						sizeof(PciExtension),
						nullptr,
						0,
						FILE_AUTOGENERATED_DEVICE_NAME,
						false,
						&new_device);
					if (NT_SUCCESS(status)) {
						auto* ext = static_cast<PciExtension*>(new_device->DeviceExtension);

						uint16_t device_id = pci_read(addr, 2, 2);
						uint8_t rev = pci_read(addr, 8, 1);
						uint8_t prog_if = pci_read(addr, 9, 1);
						uint8_t subclass = pci_read(addr, 10, 1);
						uint8_t clazz = pci_read(addr, 11, 1);

						ext->addr = addr;
						ext->vendor = vendor_id;
						ext->device = device_id;
						ext->clazz = clazz;
						ext->subclass = subclass;
						ext->prog_if = prog_if;
						ext->revision = rev;
						ext->type = type & ~(1 << 7);

						if ((type & ~(1 << 7)) == 0) {
							ext->subsys_vendor = pci_read(addr, 44, 2);
							ext->subsys_id = pci_read(addr, 46, 2);
						}

						device_objects[actual - 1] = new_device;
					}
				}
			}
		}
	};

	if (GLOBAL_MCFG) {
		uint32_t entries = (GLOBAL_MCFG->hdr.length - sizeof(Mcfg)) / sizeof(Mcfg::Entry);
		for (uint32_t i = 0; i < entries; ++i) {
			auto& entry = GLOBAL_MCFG->entries[i];

			addr.seg = entry.seg;

			for (uint32_t bus = entry.start; bus < entry.end; ++bus) {
				enumerate_bus(bus);
			}
		}
	}
	else {
		for (uint32_t i = 0; i < 256; ++i) {
			enumerate_bus(i);
		}
	}

	*count = actual;
}

static NTSTATUS handle_pnp(DEVICE_OBJECT* device_object, IRP* irp) {
	auto slot = IoGetCurrentIrpStackLocation(irp);

	if (slot->MinorFunction == IRP_MN_QUERY_DEVICE_RELATIONS) {
		if (slot->Parameters.QueryDeviceRelations.Type != BusRelations) {
			irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
			IofCompleteRequest(irp, IO_NO_INCREMENT);
			return STATUS_NOT_IMPLEMENTED;
		}

		size_t count = 0;
		enumerate(device_object->DriverObject, nullptr, &count);

		auto* relations = static_cast<DEVICE_RELATIONS*>(ExAllocatePool2(
			0,
			sizeof(DEVICE_RELATIONS) +
			sizeof(PDEVICE_OBJECT) * count,
			0));
		if (!relations) {
			irp->IoStatus.Status = STATUS_NO_MEMORY;
			IofCompleteRequest(irp, IO_NO_INCREMENT);
			return STATUS_NO_MEMORY;
		}

		enumerate(device_object->DriverObject, relations->Objects, &count);

		relations->Count = count;
		irp->IoStatus.Information = reinterpret_cast<ULONG_PTR>(relations);
	}
	else if (slot->MinorFunction == IRP_MN_QUERY_ID) {
		auto type = slot->Parameters.QueryId.IdType;

		if (type == BusQueryDeviceID) {
			auto* str = static_cast<wchar_t*>(ExAllocatePool2(0, MAX_DEVICE_ID_LEN * sizeof(WCHAR), 0));
			if (!str) {
				irp->IoStatus.Status = STATUS_NO_MEMORY;
				IofCompleteRequest(irp, IO_NO_INCREMENT);
				return STATUS_NO_MEMORY;
			}

			irp->IoStatus.Information = reinterpret_cast<ULONG_PTR>(str);

			auto* ext = static_cast<PciExtension*>(device_object->DeviceExtension);

			if (ext->type == 0) {
				swprintf(
					str,
					MAX_DEVICE_ID_LEN,
					L"PCI\\VEN_%04X&DEV_%04X&SUBSYS_%04X%04X&REV_%02X",
					ext->vendor,
					ext->device,
					ext->subsys_id,
					ext->subsys_vendor,
					ext->revision);
			}
			else {
				swprintf(
					str,
					MAX_DEVICE_ID_LEN,
					L"PCI\\VEN_%04X&DEV_%04X&REV_%02X",
					ext->vendor,
					ext->device,
					ext->revision);
			}
		}
		else if (type == BusQueryHardwareIDs) {
			auto* str = static_cast<wchar_t*>(ExAllocatePool2(0, REGSTR_VAL_MAX_HCID_LEN * sizeof(WCHAR), 0));
			if (!str) {
				irp->IoStatus.Status = STATUS_NO_MEMORY;
				IofCompleteRequest(irp, IO_NO_INCREMENT);
				return STATUS_NO_MEMORY;
			}

			irp->IoStatus.Information = reinterpret_cast<ULONG_PTR>(str);

			auto* ext = static_cast<PciExtension*>(device_object->DeviceExtension);

			size_t remaining = REGSTR_VAL_MAX_HCID_LEN - 2;
			if (ext->type == 0) {
				int written = swprintf(
					str,
					remaining,
					L"PCI\\VEN_%04X&DEV_%04X&SUBSYS_%04X%04X&REV_%02X",
					ext->vendor,
					ext->device,
					ext->subsys_id,
					ext->subsys_vendor,
					ext->revision);
				str += written + 1;
				remaining -= written + 1;
				written = swprintf(
					str,
					remaining,
					L"PCI\\VEN_%04X&DEV_%04X&SUBSYS_%04X%04X",
					ext->vendor,
					ext->device,
					ext->subsys_id,
					ext->subsys_vendor);
				str += written + 1;
				remaining -= written + 1;
			}

			int written = swprintf(
				str,
				remaining,
				L"PCI\\VEN_%04X&DEV_%04X&REV_%02X",
				ext->vendor,
				ext->device,
				ext->revision);
			str += written + 1;
			remaining -= written + 1;
			written = swprintf(
				str,
				remaining,
				L"PCI\\VEN_%04X&DEV_%04X",
				ext->vendor,
				ext->device);
			str += written + 1;
			remaining -= written + 1;
			written = swprintf(
				str,
				remaining,
				L"PCI\\VEN_%04X&DEV_%04X&CC_%02X%02X%02X",
				ext->vendor,
				ext->device,
				ext->clazz,
				ext->subclass,
				ext->prog_if);
			str += written + 1;
			remaining -= written + 1;
			written = swprintf(
				str,
				remaining,
				L"PCI\\VEN_%04X&DEV_%04X&CC_%02X%02X",
				ext->vendor,
				ext->device,
				ext->clazz,
				ext->subclass);
			str += written + 1;
			remaining -= written + 1;

			*str = 0;
			str[1] = 0;
		}
		else if (type == BusQueryCompatibleIDs) {
			auto* str = static_cast<wchar_t*>(ExAllocatePool2(0, REGSTR_VAL_MAX_HCID_LEN * sizeof(WCHAR), 0));
			if (!str) {
				irp->IoStatus.Status = STATUS_NO_MEMORY;
				IofCompleteRequest(irp, IO_NO_INCREMENT);
				return STATUS_NO_MEMORY;
			}

			irp->IoStatus.Information = reinterpret_cast<ULONG_PTR>(str);

			auto* ext = static_cast<PciExtension*>(device_object->DeviceExtension);

			size_t remaining = REGSTR_VAL_MAX_HCID_LEN - 2;

			int written = swprintf(
				str,
				remaining,
				L"PCI\\VEN_%04X&DEV_%04X&REV_%02X",
				ext->vendor,
				ext->device,
				ext->revision);
			str += written + 1;
			remaining -= written + 1;
			written = swprintf(
				str,
				remaining,
				L"PCI\\VEN_%04X&DEV_%04X",
				ext->vendor,
				ext->device);
			str += written + 1;
			remaining -= written + 1;
			written = swprintf(
				str,
				remaining,
				L"PCI\\VEN_%04X&CC_%02X%02X%02X",
				ext->vendor,
				ext->clazz,
				ext->subclass,
				ext->prog_if);
			str += written + 1;
			remaining -= written + 1;
			written = swprintf(
				str,
				remaining,
				L"PCI\\VEN_%04X&CC_%02X%02X",
				ext->vendor,
				ext->clazz,
				ext->subclass);
			str += written + 1;
			remaining -= written + 1;
			written = swprintf(
				str,
				remaining,
				L"PCI\\VEN_%04X",
				ext->vendor);
			str += written + 1;
			remaining -= written + 1;
			written = swprintf(
				str,
				remaining,
				L"PCI\\CC_%02X%02X%02X",
				ext->clazz,
				ext->subclass,
				ext->prog_if);
			str += written + 1;
			remaining -= written + 1;
			written = swprintf(
				str,
				remaining,
				L"PCI\\CC_%02X%02X",
				ext->clazz,
				ext->subclass);
			str += written + 1;
			remaining -= written + 1;

			*str = 0;
			str[1] = 0;
		}
		else {
			irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
			IofCompleteRequest(irp, IO_NO_INCREMENT);
			return STATUS_NOT_IMPLEMENTED;
		}
	}
	else {
		irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
		IofCompleteRequest(irp, IO_NO_INCREMENT);
		return STATUS_NOT_IMPLEMENTED;
	}

	irp->IoStatus.Status = STATUS_SUCCESS;
	IofCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry_path) {
	DbgPrintEx(DPFLTR_PCI_ID, DPFLTR_INFO_LEVEL, "pci.sys: initializing\n");

	auto status = AuxKlibInitialize();
	if (!NT_SUCCESS(status)) {
		return status;
	}

	ULONG mcfg_len;
	status = AuxKlibGetSystemFirmwareTable('ACPI', 0x4746434D, nullptr, 0, &mcfg_len);
	if (status != STATUS_BUFFER_TOO_SMALL && status != STATUS_INVALID_PARAMETER) {
		return status;
	}

	if (status != STATUS_INVALID_PARAMETER) {
		GLOBAL_MCFG = static_cast<Mcfg*>(ExAllocatePool2(0, mcfg_len, 0));
		if (!GLOBAL_MCFG) {
			return STATUS_NO_MEMORY;
		}

		status = AuxKlibGetSystemFirmwareTable('ACPI', 0x4746434D, GLOBAL_MCFG, mcfg_len, &mcfg_len);
		if (!NT_SUCCESS(status)) {
			ExFreePool(GLOBAL_MCFG);
			return status;
		}

		uint32_t entries = (GLOBAL_MCFG->hdr.length - sizeof(Mcfg)) / sizeof(Mcfg::Entry);
		for (uint32_t i = 0; i < entries; ++i) {
			auto& entry = GLOBAL_MCFG->entries[i];

			PHYSICAL_ADDRESS addr {
				.QuadPart = static_cast<LONGLONG>(entry.base)
			};
			size_t size = entry.end << 20;
			PVOID mapping = MmMapIoSpace(
				addr,
				size,
				MmNonCached);
			if (!mapping) {
				entry.base = 0;
				continue;
			}

			entry.base = reinterpret_cast<uint64_t>(mapping);
		}
	}

	driver->MajorFunction[IRP_MJ_PNP] = handle_pnp;

	return STATUS_SUCCESS;
}
