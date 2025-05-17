#include "irq.hpp"
#include "arch/x86/dev/io_apic.hpp"
#include "pnp.hpp"
#include "exe/pe.hpp"

void (*PCI_SYS_ENABLE_MSI)(DEVICE_OBJECT* dev, IO_INTERRUPT_MESSAGE_INFO_ENTRY* entry, UINT index, bool enable);

void pci_irq_init(LoadedPe* pci_sys_pe) {
	PCI_SYS_ENABLE_MSI = reinterpret_cast<decltype(PCI_SYS_ENABLE_MSI)>(pe_get_symbol_addr(pci_sys_pe, "pci_enable_msi"));
	assert(PCI_SYS_ENABLE_MSI);
}

NTAPI NTSTATUS IoConnectInterruptEx(IO_CONNECT_INTERRUPT_PARAMETERS* generic_params) {
	auto dev = generic_params->fully_specified.physical_device;

	switch (generic_params->version) {
		case CONNECT_FULLY_SPECIFIED:
		{
			auto& params = generic_params->fully_specified;

			params.irq_obj = nullptr;

			auto* irq = static_cast<KINTERRUPT*>(kcalloc(sizeof(KINTERRUPT)));
			if (!irq) {
				return STATUS_INSUFFICIENT_RESOURCES;
			}

			irq->fn = params.service_routine;
			irq->service_ctx = params.service_ctx;
			irq->device = dev;

			if (params.spinlock) {
				irq->lock = params.spinlock;
			}
			else {
				irq->lock = &irq->default_lock;
			}

			irq->synchronize_irql = params.synchronize_irql;

			irq->can_be_shared = params.share_vector;

			*params.irq_obj = irq;

			register_irq_handler(irq);
			break;
		}
		case CONNECT_LINE_BASED:
		{
			if (!dev->device_object_extension->interrupt_count) {
				return STATUS_NOT_FOUND;
			}

			auto& params = generic_params->line_based;

			*params.irq_obj = nullptr;

			auto* irq = static_cast<KINTERRUPT*>(kcalloc(sizeof(KINTERRUPT)));
			if (!irq) {
				return STATUS_INSUFFICIENT_RESOURCES;
			}

			auto& pin_irq = dev->device_object_extension->interrupts[0];
			assert(pin_irq.is_pin);

			irq->fn = params.service_routine;
			irq->service_ctx = params.service_ctx;
			irq->vector = pin_irq.pin.vector;
			irq->device = dev;

			if (params.spinlock) {
				irq->lock = params.spinlock;
			}
			else {
				irq->lock = &irq->default_lock;
			}

			irq->synchronize_irql = params.synchronize_irql;

			irq->can_be_shared = true;

			*params.irq_obj = irq;

			register_irq_handler(irq);
			IO_APIC.register_isa_irq(
				pin_irq.pin.pin,
				pin_irq.pin.vector,
				pin_irq.pin.default_active_low,
				pin_irq.pin.default_level_trigger);
			break;
		}
		case CONNECT_MESSAGE_BASED:
		{
			auto& params = generic_params->message_based;

			*params.connection_ctx.irq_obj = nullptr;

			if (dev->device_object_extension->interrupt_count == 1) {
				auto* irq = static_cast<KINTERRUPT*>(kcalloc(sizeof(KINTERRUPT)));
				if (!irq) {
					return STATUS_INSUFFICIENT_RESOURCES;
				}

				auto& pin_irq = dev->device_object_extension->interrupts[0];
				assert(pin_irq.is_pin);

				irq->fn = params.fallback_service_routine;
				irq->service_ctx = params.service_ctx;
				irq->vector = pin_irq.pin.vector;
				irq->device = dev;

				if (params.spinlock) {
					irq->lock = params.spinlock;
				}
				else {
					irq->lock = &irq->default_lock;
				}

				irq->synchronize_irql = params.synchronize_irql;

				irq->can_be_shared = true;

				*params.connection_ctx.irq_obj = irq;

				register_irq_handler(irq);
				IO_APIC.register_isa_irq(
					pin_irq.pin.pin,
					pin_irq.pin.vector,
					pin_irq.pin.default_active_low,
					pin_irq.pin.default_level_trigger);

				generic_params->version = CONNECT_LINE_BASED;
				break;
			}

			u32 msg_count = dev->device_object_extension->interrupt_count - 1;
			assert(msg_count);
			u32 cpu_id = 0;
			u64 msg_addr = 0xFEE00000 | cpu_id << 12;

			auto* table = static_cast<IO_INTERRUPT_MESSAGE_INFO*>(kcalloc(
				sizeof(IO_INTERRUPT_MESSAGE_INFO) +
				msg_count * sizeof(IO_INTERRUPT_MESSAGE_INFO_ENTRY)));
			if (!table) {
				return STATUS_INSUFFICIENT_RESOURCES;
			}

			KIRQL synchronize_irql = params.synchronize_irql;

			table->message_count = msg_count;
			for (u32 i = 0; i < msg_count; ++i) {
				auto* irq = static_cast<KINTERRUPT*>(kcalloc(sizeof(KINTERRUPT)));
				if (!irq) {
					for (u32 j = 0; j < i; ++j) {
						auto& entry = table->message_info[j];

						deregister_irq_handler(entry.irq_obj);
						kfree(entry.irq_obj, sizeof(KINTERRUPT));
					}

					kfree(
						table,
						sizeof(IO_INTERRUPT_MESSAGE_INFO) +
						msg_count * sizeof(IO_INTERRUPT_MESSAGE_INFO_ENTRY));

					return STATUS_INSUFFICIENT_RESOURCES;
				}

				auto& info = dev->device_object_extension->interrupts[1 + i];
				assert(!info.is_pin);

				irq->msg_fn = params.msg_service_routine;
				irq->service_ctx = params.service_ctx;
				irq->vector = info.msg.vector;
				irq->msg_id = i;
				irq->device = dev;

				if (params.spinlock) {
					irq->lock = params.spinlock;
				}
				else {
					irq->lock = &table->message_info[0].irq_obj->default_lock;
				}

				irq->can_be_shared = false;

				*params.connection_ctx.irq_obj = irq;

				register_irq_handler(irq);

				auto& entry = table->message_info[i];
				entry.message_addr.QuadPart = static_cast<LONGLONG>(msg_addr);
				entry.target_processor_set = UINT32_MAX;
				entry.irq_obj = irq;
				entry.message_data = irq->vector;
				entry.vector = irq->vector;
				entry.irql = irq->vector >> 16;
				entry.mode = Latched;
				entry.polarity = InterruptActiveHigh;

				if (entry.irql > synchronize_irql) {
					synchronize_irql = entry.irql;
				}

				PCI_SYS_ENABLE_MSI(dev, &entry, irq->msg_id, true);
			}

			table->unified_irql = synchronize_irql;

			for (u32 i = 0; i < msg_count; ++i) {
				auto* irq = table->message_info[i].irq_obj;
				irq->synchronize_irql = synchronize_irql;
			}

			*params.connection_ctx.irq_msg_table = table;

			break;
		}
		case CONNECT_FULLY_SPECIFIED_GROUP:
			panic("[kernel]: IoConnectInterruptEx CONNECT_FULLY_SPECIFIED_GROUP is not supported");
		case CONNECT_MESSAGE_BASED_PASSIVE:
			panic("[kernel]: IoConnectInterruptEx CONNECT_MESSAGE_BASED_PASSIVE is not supported");
	}

	return STATUS_SUCCESS;
}

NTAPI void IoDisconnectInterruptEx(IO_DISCONNECT_INTERRUPT_PARAMETERS* params) {
	if (params->version != CONNECT_MESSAGE_BASED && params->version != CONNECT_MESSAGE_BASED_PASSIVE) {
		auto* irq = params->connection_ctx.irq;
		deregister_irq_handler(irq);
		kfree(irq, sizeof(KINTERRUPT));
	}
	else {
		auto* table = params->connection_ctx.irq_msg_table;

		for (u32 i = 0; i < table->message_count; ++i) {
			auto& entry = table->message_info[i];

			assert(entry.vector == entry.irq_obj->vector);
			PCI_SYS_ENABLE_MSI(entry.irq_obj->device, &entry, entry.irq_obj->msg_id, false);
			deregister_irq_handler(entry.irq_obj);
			kfree(entry.irq_obj, sizeof(KINTERRUPT));
		}

		kfree(
			table,
			sizeof(IO_INTERRUPT_MESSAGE_INFO) +
			table->message_count * sizeof(IO_INTERRUPT_MESSAGE_INFO_ENTRY));
	}
}
