#pragma once

#include "pnp.hpp"

struct PnpIrpBuilder {
	static hz::optional<PnpIrpBuilder> create(DEVICE_OBJECT* dev, ULONG minor) {
		IRP* irp = IoAllocateIrp(dev->stack_size, false);
		if (!irp) {
			return hz::nullopt;
		}

		auto slot = IoGetNextIrpStackLocation(irp);
		slot->major_function = IRP_MJ_PNP;
		slot->minor_function = minor;
		slot->device = dev;
		return PnpIrpBuilder {irp};
	}

	template<typename F> requires requires (F fn, IO_STACK_LOCATION* slot) {
		fn(slot);
	}
	inline PnpIrpBuilder& params(F fn) {
		fn(IoGetNextIrpStackLocation(irp));
		return *this;
	}

	template<typename F> requires requires (F fn, IRP* irp) {
		{ fn(irp) } -> hz::same_as<NTSTATUS>;
	}
	inline NTSTATUS send(F on_complete) {
		IoSetCompletionRoutine(irp, [](DEVICE_OBJECT* device, IRP* irp, void* ctx) {
			return (*static_cast<F*>(ctx))(irp);
		}, &on_complete, true, false, false);
		return IofCallDriver(IoGetNextIrpStackLocation(irp)->device, irp);
	}

private:
	constexpr explicit PnpIrpBuilder(IRP* irp) : irp {irp} {}

	IRP* irp;
};
