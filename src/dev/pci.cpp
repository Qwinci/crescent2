#include "pci.hpp"
#include "acpi/acpi.hpp"
#include "mem/mem.hpp"
#include "assert.hpp"
#include "stdio.hpp"

#ifdef __x86_64__
#include "arch/x86/port_space.hpp"
#include "mem/register.hpp"
#endif

namespace pci {
	struct [[gnu::packed]] Mcfg {
		struct Entry {
			u64 base;
			u16 seg;
			u8 start;
			u8 end;
			u32 reserved;
		};

		acpi::SdtHeader hdr;
		u32 reserved[2];
		Entry entries[];
	};

	struct MsiXEntry {
		u32 msg_addr_low;
		u32 msg_addr_high;
		u32 msg_data;
		u32 vector_ctrl;
	};

	namespace {
		Mcfg* GLOBAL_MCFG = nullptr;

		void* mcfg_get_space(const PciAddress& addr) {
			assert(GLOBAL_MCFG);

			u16 entries = (GLOBAL_MCFG->hdr.length - sizeof(Mcfg)) / sizeof(Mcfg::Entry);
			for (u32 i = 0; i < entries; ++i) {
				auto& entry = GLOBAL_MCFG->entries[i];

				if (entry.seg == addr.seg) {
					void* res = to_virt<void>(entry.base);
					res = offset(res, void*, static_cast<usize>(addr.bus) << 20);
					res = offset(res, void*, static_cast<usize>(addr.dev) << 15);
					res = offset(res, void*, static_cast<usize>(addr.func) << 12);
					return res;
				}
			}

			return nullptr;
		}
	}

	void acpi_init() {
		GLOBAL_MCFG = static_cast<Mcfg*>(acpi::get_table("MCFG"));
		if (GLOBAL_MCFG) {
			PCIE_SUPPORTED = true;
		}
	}

#ifdef __x86_64__
	static constinit PortSpace IO_SPACE {0xCF8};
	static constexpr BasicRegister<u32> CFG_ADDR {0};
	static constexpr BasicRegister<u32> CFG_DATA {4};
#endif

	u32 read(PciAddress addr, u32 offset, u8 size) {
		u32 align = offset & 0b11;
		offset &= ~0b11;

		u32 value;
		if (GLOBAL_MCFG) {
			void* ptr = mcfg_get_space(addr);
			value = *offset(ptr, volatile u32*, offset);
		}
		else {
#ifdef __x86_64__
			assert(offset <= 0xFF);

			u32 io_addr = 1 << 31;
			io_addr |= addr.bus << 16;
			io_addr |= addr.dev << 11;
			io_addr |= addr.func << 8;
			io_addr |= offset;

			IO_SPACE.store(CFG_ADDR, io_addr);
			value = IO_SPACE.load(CFG_DATA);
#else
			panic("[kernel][pci]: pci read unsupported on architecture");
#endif
		}

		u32 shift = 32 - (size * 8);
		return (value >> (align * 8)) << shift >> shift;
	}

	void write(PciAddress addr, u32 offset, u8 size, u32 value) {
		u32 align = offset & 0b11;
		offset &= ~0b11;

		u32 res = read(addr, offset, size);
		res &= ~(((u64 {1} << (size * 8)) - 1) << (align * 8));
		res |= value << (align * 8);

		if (GLOBAL_MCFG) {
			void* ptr = mcfg_get_space(addr);
			*offset(ptr, volatile u32*, offset) = res;
		}
		else {
#ifdef __x86_64__
			assert(offset <= 0xFF);

			u32 io_addr = 1 << 31;
			io_addr |= addr.bus << 16;
			io_addr |= addr.dev << 11;
			io_addr |= addr.func << 8;
			io_addr |= offset;

			IO_SPACE.store(CFG_ADDR, io_addr);
			IO_SPACE.store(CFG_DATA, res);
#else
			panic("[kernel][pci]: pci write unsupported on architecture");
#endif
		}
	}

	bool PCIE_SUPPORTED = false;
}
