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
	static inline uint32_t in4(uint16_t port) {
		uint32_t value;
		asm volatile("in %0, %1" : "=a"(value) : "Nd"(port));
		return value;
	}

	static inline void out4(uint16_t port, uint32_t value) {
		asm volatile("out %0, %1" : : "Nd"(port), "a"(value));
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
		asm volatile("mov %0, %1" : "=a"(value) : "m"(*(ptr + offset)));
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
		asm volatile("mov %0, %1" : "=m"(*(ptr + offset)) : "a"(res));
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
	uint8_t msi_cap_offset;
	uint8_t msix_cap_offset;
};
static_assert(offsetof(PciExtension, addr) == 0);

#define pci_hdr_offsetof(field) (offsetof(PCI_COMMON_HEADER, u) + offsetof(PCI_HEADER_TYPE, field))

static ULONG find_cap_in_hdr0(PciExtension* ext, uint8_t find_id) {
	using PCI_HEADER_TYPE = decltype(_PCI_COMMON_HEADER::u)::_PCI_HEADER_TYPE_0;

	uint16_t status = pci_read(ext->addr, offsetof(PCI_COMMON_HEADER, Status), 2);

	if (status & 1 << 4) {
		uint8_t cap_ptr = pci_read(ext->addr, pci_hdr_offsetof(CapabilitiesPtr), 1) & ~0b11;

		while (true) {
			uint8_t id = pci_read(ext->addr, cap_ptr, 1);
			uint8_t next = pci_read(ext->addr, cap_ptr + 1, 1);

			if (id == find_id) {
				return cap_ptr;
			}

			if (!next) {
				break;
			}

			cap_ptr = next;
		}
	}

	return 0;
}

__declspec(dllexport)
extern "C" [[gnu::used]] void pci_enable_msi(PDEVICE_OBJECT dev, IO_INTERRUPT_MESSAGE_INFO_ENTRY* entry, UINT index, bool enable) {
	using PCI_HEADER_TYPE = decltype(PCI_COMMON_HEADER::u.type0);

	auto* ext = static_cast<PciExtension*>(dev->DeviceExtension);
	if (ext->msix_cap_offset) {
		PCI_MSIX_CAPABILITY cap {};
		USHORT control = pci_read(ext->addr, ext->msix_cap_offset + offsetof(PCI_MSIX_CAPABILITY, MessageControl), 2);
		memcpy(&cap.MessageControl, &control, 2);
		cap.MessageTable.TableOffset = pci_read(ext->addr, ext->msix_cap_offset + offsetof(PCI_MSIX_CAPABILITY, MessageTable), 4);
		if (!cap.MessageControl.MSIXEnable) {
			cap.MessageControl.MSIXEnable = true;
			memcpy(&control, &cap.MessageControl, 2);
			pci_write(ext->addr, ext->msix_cap_offset + offsetof(PCI_MSIX_CAPABILITY, MessageControl), 2, control);
		}

		UCHAR bar_index = cap.MessageTable.BaseIndexRegister;
		ULONG offset = cap.MessageTable.TableOffset & MSIX_TABLE_OFFSET_MASK;

		uint64_t bar = pci_read(ext->addr, pci_hdr_offsetof(BaseAddresses) + bar_index * 4, 4);
		if ((bar >> 1 & 0b11) == 2) {
			bar |= static_cast<uint64_t>(
				pci_read(ext->addr, pci_hdr_offsetof(BaseAddresses) + (bar_index + 1) * 4, 4)) << 32;
		}

		bar &= ~0xF;
		char* mapping = static_cast<char*>(MmMapIoSpace(
			{.QuadPart = static_cast<LONGLONG>(bar)},
			(offset + cap.MessageControl.TableSize + 1 + 0xFFF) & ~0xFFF,
			MmCached));
		assert(mapping);
		auto* entries = reinterpret_cast<volatile PCI_MSIX_TABLE_ENTRY*>(mapping + offset);

		auto* msix_entry = &entries[index];
		msix_entry->MessageAddress.QuadPart = entry->MessageAddress.QuadPart;
		msix_entry->MessageData = entry->MessageData;
		msix_entry->VectorControl.Mask = !enable;
	}
	else {
		assert(ext->msi_cap_offset);
		PCI_MSI_CAPABILITY cap {};
		assert(false);
	}
}

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
					PDEVICE_OBJECT existing = nullptr;
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

							uint8_t msi_cap_offset = find_cap_in_hdr0(ext, PCI_CAPABILITY_ID_MSI);
							uint8_t msix_cap_offset = find_cap_in_hdr0(ext, PCI_CAPABILITY_ID_MSIX);

							ext->msi_cap_offset = msi_cap_offset;
							ext->msix_cap_offset = msix_cap_offset;
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
			irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
			IofCompleteRequest(irp, IO_NO_INCREMENT);
			return STATUS_INSUFFICIENT_RESOURCES;
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
				irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
				IofCompleteRequest(irp, IO_NO_INCREMENT);
				return STATUS_INSUFFICIENT_RESOURCES;
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
				irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
				IofCompleteRequest(irp, IO_NO_INCREMENT);
				return STATUS_INSUFFICIENT_RESOURCES;
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
				irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
				IofCompleteRequest(irp, IO_NO_INCREMENT);
				return STATUS_INSUFFICIENT_RESOURCES;
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
	else if (slot->MinorFunction == IRP_MN_QUERY_RESOURCE_REQUIREMENTS) {
		auto* ext = static_cast<PciExtension*>(device_object->DeviceExtension);

		if (ext->type == 0) {
			using PCI_HEADER_TYPE = decltype(_PCI_COMMON_HEADER::u)::_PCI_HEADER_TYPE_0;

			uint16_t cmd = pci_read(ext->addr, offsetof(PCI_COMMON_HEADER, Command), 2);
			cmd &= ~(PCI_ENABLE_IO_SPACE | PCI_ENABLE_MEMORY_SPACE | PCI_ENABLE_BUS_MASTER);
			pci_write(ext->addr, offsetof(PCI_COMMON_HEADER, Command), 2, cmd);

			uint32_t res_count = 0;

			uint64_t bars[6];
			uint64_t bar_sizes[6];
			for (int i = 0; i < 6; ++i) {
				auto offset = pci_hdr_offsetof(BaseAddresses) + i * 4;
				bars[i] = pci_read(ext->addr, offset, 4);
				if (bars[i]) {
					++res_count;

					pci_write(ext->addr, offset, 4, 0xFFFFFFFF);
					uint32_t value = pci_read(ext->addr, offset, 4);

					if (bars[i] & 1) {
						bar_sizes[i] = ~(value & ~0b11) + 1;
						pci_write(ext->addr, offset, 4, bars[i]);
					}
					else {
						if ((bars[i] >> 1 & 0b11) == 2) {
							bars[i] |= static_cast<uint64_t>(pci_read(ext->addr, offset + 4, 4)) << 32;

							uint64_t value64 = value & ~0b1111;

							pci_write(ext->addr, offset + 4, 4, 0xFFFFFFFF);
							value64 |= static_cast<uint64_t>(pci_read(ext->addr, offset + 4, 4)) << 32;
							pci_write(ext->addr, offset, 4, bars[i]);
							pci_write(ext->addr, offset + 4, 4, bars[i] >> 32);

							bar_sizes[i] = ~value64 + 1;

							++i;
						}
						else {
							bar_sizes[i] = ~(value & ~0b1111) + 1;
							pci_write(ext->addr, offset, 4, bars[i]);
						}
					}
				}
			}

			cmd |= PCI_ENABLE_IO_SPACE | PCI_ENABLE_MEMORY_SPACE | PCI_ENABLE_BUS_MASTER;
			pci_write(ext->addr, offsetof(PCI_COMMON_HEADER, Command), 2, cmd);

			uint8_t irq_pin = pci_read(ext->addr, pci_hdr_offsetof(InterruptPin), 1);

			uint32_t msi_count = 0;
			uint32_t msix_count = 0;

			if (ext->msi_cap_offset) {
				uint16_t control_word = pci_read(ext->addr, ext->msi_cap_offset + offsetof(PCI_MSI_CAPABILITY, MessageControl), 2);
				PCI_MSI_CAPABILITY::_PCI_MSI_MESSAGE_CONTROL control {};
				memcpy(&control, &control_word, 2);
				msi_count = 1 << control.MultipleMessageCapable;
			}
			if (ext->msix_cap_offset) {
				uint16_t control_word = pci_read(ext->addr, ext->msix_cap_offset + offsetof(PCI_MSIX_CAPABILITY, MessageControl), 2);
				decltype(PCI_MSIX_CAPABILITY::MessageControl) control {};
				memcpy(&control, &control_word, 2);
				msix_count = control.TableSize + 1;
			}

			if (irq_pin != 0) {
				++res_count;
			}

			if (msix_count) {
				res_count += msix_count;
			}
			else if (msi_count) {
				++res_count;
			}

			size_t list_size = sizeof(IO_RESOURCE_REQUIREMENTS_LIST) +
				sizeof(IO_RESOURCE_LIST) +
				res_count * sizeof(IO_RESOURCE_DESCRIPTOR);
			auto* req_list = static_cast<IO_RESOURCE_REQUIREMENTS_LIST*>(ExAllocatePool2(
				0,
				list_size,
				0));
			if (!req_list) {
				irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
				IofCompleteRequest(irp, IO_NO_INCREMENT);
				return STATUS_INSUFFICIENT_RESOURCES;
			}

			req_list->ListSize = list_size;
			req_list->InterfaceType = PCIBus;
			req_list->BusNumber = 0;
			req_list->SlotNumber = 0;
			req_list->AlternativeLists = 1;

			auto& list = req_list->List[0];
			list.Version = 1;
			list.Revision = 1;
			list.Count = res_count;

			uint32_t res_i = 0;
			for (int i = 0; i < 6; ++i) {
				if (bars[i]) {
					auto& desc = list.Descriptors[res_i++];

					desc.Option = 0;
					desc.ShareDisposition = CmResourceShareShared;
					desc.Flags = 0;

					if (bars[i] & 1) {
						desc.Type = CmResourceTypePort;
						desc.Flags = CM_RESOURCE_PORT_IO | CM_RESOURCE_PORT_BAR;
						desc.u.Port.MinimumAddress.QuadPart = static_cast<LONGLONG>(bars[i] & ~0b11);
						desc.u.Port.MaximumAddress = desc.u.Port.MinimumAddress;
						desc.u.Port.Length = bar_sizes[i];
					}
					else {
						desc.Flags = CM_RESOURCE_MEMORY_BAR;
						if (bars[i] & 1 << 3) {
							desc.Flags |= CM_RESOURCE_MEMORY_PREFETCHABLE;
						}

						desc.u.Memory.MinimumAddress.QuadPart = static_cast<LONGLONG>(bars[i] & ~0b1111);
						desc.u.Memory.MaximumAddress = desc.u.Memory.MinimumAddress;

						auto size = bar_sizes[i];
						if ((bars[i] >> 1 & 0b11) == 2) {
							if (size <= UINT32_MAX) {
								desc.Type = CmResourceTypeMemory;
								desc.u.Memory.Length = size;
							}
							else {
								desc.Type = CmResourceTypeMemoryLarge;
								if (size <= CM_RESOURCE_MEMORY_LARGE_40_MAXLEN) {
									assert((size & 0xFF) == 0);
									desc.Flags |= CM_RESOURCE_MEMORY_LARGE_40;
									desc.u.Memory40.Length40 = size >> 8;
								}
								else if (size <= CM_RESOURCE_MEMORY_LARGE_48_MAXLEN) {
									assert((size & 0xFFFF) == 0);
									desc.Flags |= CM_RESOURCE_MEMORY_LARGE_48;
									desc.u.Memory48.Length48 = size >> 16;
								}
								else {
									assert((size & ~CM_RESOURCE_MEMORY_LARGE_64_MAXLEN) == 0);
									desc.Flags |= CM_RESOURCE_MEMORY_LARGE_64;
									desc.u.Memory64.Length64 = size >> 32;
								}
							}

							++i;
						}
						else {
							desc.Type = CmResourceTypeMemory;
							desc.u.Memory.Length = size;
						}
					}
				}
			}

			if (irq_pin != 0) {
				auto& desc = list.Descriptors[res_i++];
				desc.Option = 0;
				desc.Type = CmResourceTypeInterrupt;
				desc.ShareDisposition = CmResourceShareShared;
				desc.Flags = CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;
				desc.u.Interrupt.MinimumVector = irq_pin;
				desc.u.Interrupt.MaximumVector = irq_pin;
				desc.u.Interrupt.AffinityPolicy = IrqPolicyMachineDefault;
				desc.u.Interrupt.Group = ALL_PROCESSOR_GROUPS;
				desc.u.Interrupt.PriorityPolicy = IrqPriorityNormal;
				desc.u.Interrupt.TargetedProcessors = 0xFFFFFFFF;
			}

			if (msix_count) {
				for (uint32_t i = 0; i < msix_count; ++i) {
					auto& desc = list.Descriptors[res_i++];
					desc.Option = 0;
					desc.Type = CmResourceTypeInterrupt;
					desc.ShareDisposition = CmResourceShareDeviceExclusive;
					desc.Flags = CM_RESOURCE_INTERRUPT_MESSAGE | CM_RESOURCE_INTERRUPT_LATCHED;
					desc.u.Interrupt.MinimumVector = CM_RESOURCE_INTERRUPT_MESSAGE_TOKEN;
					desc.u.Interrupt.MaximumVector = CM_RESOURCE_INTERRUPT_MESSAGE_TOKEN;
					desc.u.Interrupt.AffinityPolicy = IrqPolicyMachineDefault;
					desc.u.Interrupt.Group = ALL_PROCESSOR_GROUPS;
					desc.u.Interrupt.PriorityPolicy = IrqPriorityNormal;
					desc.u.Interrupt.TargetedProcessors = 0xFFFFFFFF;
				}
			}
			else if (msi_count) {
				auto& desc = list.Descriptors[res_i++];
				desc.Type = CmResourceTypeInterrupt;
				desc.ShareDisposition = CmResourceShareDeviceExclusive;
				desc.Flags = CM_RESOURCE_INTERRUPT_MESSAGE | CM_RESOURCE_INTERRUPT_LATCHED;
				desc.u.Interrupt.MinimumVector = CM_RESOURCE_INTERRUPT_MESSAGE_TOKEN - msi_count + 1;
				desc.u.Interrupt.MaximumVector = CM_RESOURCE_INTERRUPT_MESSAGE_TOKEN;
				desc.u.Interrupt.Group = ALL_PROCESSOR_GROUPS;
				desc.u.Interrupt.PriorityPolicy = IrqPriorityNormal;
				desc.u.Interrupt.TargetedProcessors = 0xFFFFFFFF;
			}

			irp->IoStatus.Information = reinterpret_cast<ULONG_PTR>(req_list);
		}
		else {
			irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
			IofCompleteRequest(irp, IO_NO_INCREMENT);
			return STATUS_NOT_IMPLEMENTED;
		}
	}
	else if (slot->MinorFunction == IRP_MN_FILTER_RESOURCE_REQUIREMENTS) {
		IofCompleteRequest(irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;
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
	status = AuxKlibGetSystemFirmwareTable('ACPI', 'GFCM', nullptr, 0, &mcfg_len);
	if (status != STATUS_BUFFER_TOO_SMALL && status != STATUS_INVALID_PARAMETER) {
		return status;
	}

	if (status != STATUS_INVALID_PARAMETER) {
		GLOBAL_MCFG = static_cast<Mcfg*>(ExAllocatePool2(0, mcfg_len, 0));
		if (!GLOBAL_MCFG) {
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		status = AuxKlibGetSystemFirmwareTable('ACPI', 'GFCM', GLOBAL_MCFG, mcfg_len, &mcfg_len);
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
