#include <wdm.h>
#include <wchar.h>
#include <uacpi/uacpi.h>
#include <uacpi/utilities.h>
#include <uacpi/context.h>
#include <uacpi/event.h>

void glue_set_rsdp(ULONG_PTR rsdp);
ULONG glue_get_irq();
void glue_init(PDEVICE_OBJECT device, ULONG irq_vec, KIRQL irql, KINTERRUPT_MODE mode);

#define IOCTL_PROVIDE_RSDP CTL_CODE(0x8000, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

NTSTATUS handle_device_control(PDEVICE_OBJECT, PIRP irp) {
	auto slot = IoGetCurrentIrpStackLocation(irp);

	if (slot->Parameters.DeviceIoControl.IoControlCode == IOCTL_PROVIDE_RSDP) {
		glue_set_rsdp(*static_cast<ULONG_PTR*>(irp->AssociatedIrp.SystemBuffer));
		irp->IoStatus.Status = STATUS_SUCCESS;
		IofCompleteRequest(irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;
	}
	else {
		irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
		IofCompleteRequest(irp, IO_NO_INCREMENT);
		return STATUS_NOT_IMPLEMENTED;
	}
}

NTSTATUS handle_pnp(PDEVICE_OBJECT device, PIRP irp) {
	auto slot = IoGetCurrentIrpStackLocation(irp);

	if (slot->MinorFunction == IRP_MN_START_DEVICE) {
		auto& params = slot->Parameters.StartDevice;

		assert(params.AllocatedResourcesTranslated->Count == 1);

		auto& part = params.AllocatedResourcesTranslated->List[0].PartialResourceList;
		assert(part.Count == 1);

		auto& irq_desc = part.PartialDescriptors[0];
		assert(irq_desc.Type == CmResourceTypeInterrupt);

		uacpi_context_set_log_level(UACPI_LOG_TRACE);

		auto status = uacpi_namespace_load();
		assert(status == UACPI_STATUS_OK);

		glue_init(
			device,
			irq_desc.u.Interrupt.Vector,
			irq_desc.u.Interrupt.Level,
			static_cast<KINTERRUPT_MODE>(irq_desc.Flags & CM_RESOURCE_INTERRUPT_LEVEL_LATCHED_BITS));

		status = uacpi_set_interrupt_model(UACPI_INTERRUPT_MODEL_IOAPIC);
		assert(status == UACPI_STATUS_OK);
		status = uacpi_namespace_initialize();
		assert(status == UACPI_STATUS_OK);
		status = uacpi_finalize_gpe_initialization();
		assert(status == UACPI_STATUS_OK);
		status = uacpi_install_fixed_event_handler(
			UACPI_FIXED_EVENT_POWER_BUTTON,
			[](uacpi_handle) -> uacpi_interrupt_ret {
				DbgPrintEx(DPFLTR_ACPI_ID, DPFLTR_TRACE_LEVEL, "power button\n");
				return UACPI_INTERRUPT_HANDLED;
			},
			nullptr);
		assert(status == UACPI_STATUS_OK);

		uacpi_namespace_for_each_child_simple(
			uacpi_namespace_root(),
			[](void* user, uacpi_namespace_node* node, uacpi_u32) {
				uacpi_id_string* hid;
				uacpi_pnp_id_list* cid;
				auto hid_status = uacpi_eval_hid(node, &hid);
				auto cid_status = uacpi_eval_cid(node, &cid);

				if (hid_status == UACPI_STATUS_OK) {
					uacpi_free_id_string(hid);
				}
				if (cid_status == UACPI_STATUS_OK) {
					uacpi_free_pnp_id_list(cid);
				}

				if (hid_status != UACPI_STATUS_OK && cid_status != UACPI_STATUS_OK) {
					return UACPI_ITERATION_DECISION_CONTINUE;
				}

				auto acpi_bus_dev = static_cast<PDEVICE_OBJECT>(user);

				PDEVICE_OBJECT dev;
				IoCreateDevice(
					acpi_bus_dev->DriverObject,
					sizeof(uacpi_namespace_node*),
					nullptr,
					0,
					0,
					false,
					&dev);
				*static_cast<uacpi_namespace_node**>(dev->DeviceExtension) = node;

				return UACPI_ITERATION_DECISION_CONTINUE;
			},
			device);
	}
	else if (slot->MinorFunction == IRP_MN_QUERY_RESOURCES) {
		auto status = uacpi_initialize(0);
		assert(status == UACPI_STATUS_OK);

		auto list = static_cast<CM_RESOURCE_LIST*>(ExAllocatePool2(
			0,
			sizeof(CM_RESOURCE_LIST) +
			sizeof(CM_FULL_RESOURCE_DESCRIPTOR) +
			sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR),
			0
			));
		if (!list) {
			irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
			IofCompleteRequest(irp, IO_NO_INCREMENT);
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		list->Count = 1;
		auto& full_desc = list->List[0];

		auto& partial_list = full_desc.PartialResourceList;
		partial_list.Version = 1;
		partial_list.Revision = 1;
		partial_list.Count = 1;

		auto& desc = partial_list.PartialDescriptors[0];
		desc = {};
		desc.Type = CmResourceTypeInterrupt;
		desc.u.Interrupt.Vector = glue_get_irq();

		irp->IoStatus.Information = reinterpret_cast<ULONG_PTR>(list);
	}
	else if (slot->MinorFunction == IRP_MN_QUERY_DEVICE_RELATIONS) {
		if (slot->Parameters.QueryDeviceRelations.Type != BusRelations) {
			irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
			IofCompleteRequest(irp, IO_NO_INCREMENT);
			return STATUS_NOT_IMPLEMENTED;
		}

		size_t count = 0;
		// skip the acpi bus device
		for (auto* dev = device->DriverObject->DeviceObject->NextDevice; dev; dev = dev->NextDevice) {
			++count;
		}

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

		size_t i = 0;
		for (auto* dev = device->DriverObject->DeviceObject->NextDevice; dev; dev = dev->NextDevice) {
			relations->Objects[i++] = dev;
		}

		relations->Count = count;
		irp->IoStatus.Information = reinterpret_cast<ULONG_PTR>(relations);
	}
	else if (slot->MinorFunction == IRP_MN_QUERY_ID) {
		auto type = slot->Parameters.QueryId.IdType;

		if (!device->DeviceExtension) {
			irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
			IofCompleteRequest(irp, IO_NO_INCREMENT);
			return STATUS_NOT_IMPLEMENTED;
		}

		auto node = *static_cast<uacpi_namespace_node**>(device->DeviceExtension);

		auto* str = static_cast<wchar_t*>(ExAllocatePool2(0, MAX_DEVICE_ID_LEN * sizeof(WCHAR), 0));
		if (!str) {
			irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
			IofCompleteRequest(irp, IO_NO_INCREMENT);
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		uacpi_id_string* id;
		uacpi_pnp_id_list* cid = nullptr;
		auto hid_status = uacpi_eval_hid(node, &id);
		if (hid_status != UACPI_STATUS_OK) {
			auto status = uacpi_eval_cid(node, &cid);
			assert(status == UACPI_STATUS_OK);
			assert(cid->num_ids);
			id = &cid->ids[0];
		}

		uacpi_object* sub;
		auto sub_status = uacpi_eval_simple_string(
			node,
			"_SUB",
			&sub);
		uacpi_data_view sub_str {};
		if (sub_status == UACPI_STATUS_OK) {
			uacpi_object_get_string(sub, &sub_str);
		}

		uacpi_u64 hrv;
		auto hrv_status = uacpi_eval_simple_integer(node, "_HRV", &hrv);

		const char* ven_str;
		const char* dev_str;
		int ven_str_len;
		if (id->size - 1 == 8) {
			ven_str = id->value;
			ven_str_len = 4;
			dev_str = id->value + 4;
		}
		else {
			assert(id->size - 1 == 7);
			ven_str = id->value;
			ven_str_len = 3;
			dev_str = id->value + 3;
		}

		if (type == BusQueryDeviceID) {
			if (sub_status == UACPI_STATUS_OK) {
				if (hrv_status == UACPI_STATUS_OK) {
					swprintf(
						str,
						MAX_DEVICE_ID_LEN,
						L"ACPI\\VEN_%.*s&DEV_%.4s&SUBSYS_%s&REV_%04X",
						ven_str,
						ven_str_len,
						dev_str,
						sub_str.text,
						static_cast<unsigned int>(hrv));
				}
				else {
					swprintf(
						str,
						MAX_DEVICE_ID_LEN,
						L"ACPI\\VEN_%.*s&DEV_%.4s&SUBSYS_%s",
						ven_str,
						ven_str_len,
						dev_str,
						sub_str.text);
				}
			}
			else {
				if (hrv_status == UACPI_STATUS_OK) {
					swprintf(
						str,
						MAX_DEVICE_ID_LEN,
						L"ACPI\\VEN_%.*s&DEV_%.4s&REV_%04X",
						ven_str,
						ven_str_len,
						dev_str,
						static_cast<unsigned int>(hrv));
				}
				else {
					swprintf(
						str,
						MAX_DEVICE_ID_LEN,
						L"ACPI\\VEN_%.*s&DEV_%.4s",
						ven_str,
						ven_str_len,
						dev_str,
						sub_str.text);
				}
			}
		}
		else if (type == BusQueryHardwareIDs) {
			size_t remaining = REGSTR_VAL_MAX_HCID_LEN - 2;

			auto str_ptr = str;
			int written;
			if (sub_status == UACPI_STATUS_OK) {
				if (hrv_status == UACPI_STATUS_OK) {
					written = swprintf(
						str_ptr,
						remaining,
						L"ACPI\\VEN_%.*s&DEV_%.4s&SUBSYS_%s&REV_%04X",
						ven_str,
						ven_str_len,
						dev_str,
						sub_str.text,
						static_cast<unsigned int>(hrv));
					str_ptr += written + 1;
					remaining -= written + 1;
				}

				written = swprintf(
					str_ptr,
					remaining,
					L"ACPI\\VEN_%.*s&DEV_%.4s&SUBSYS_%s",
					ven_str,
					ven_str_len,
					dev_str,
					sub_str.text);
				str_ptr += written + 1;
				remaining -= written + 1;
			}

			if (hrv_status == UACPI_STATUS_OK) {
				written = swprintf(
					str_ptr,
					remaining,
					L"ACPI\\VEN_%.*s&DEV_%.4s&REV_%04X",
					ven_str,
					ven_str_len,
					dev_str,
					static_cast<unsigned int>(hrv));
				str_ptr += written + 1;
				remaining -= written + 1;
			}

			written = swprintf(
				str_ptr,
				remaining,
				L"ACPI\\VEN_%.*s&DEV_%.4s",
				ven_str,
				ven_str_len,
				dev_str,
				sub_str.text);
			str_ptr += written + 1;

			*str_ptr = 0;
			str_ptr[1] = 0;
		}
		else if (type == BusQueryCompatibleIDs) {
			size_t remaining = REGSTR_VAL_MAX_HCID_LEN - 2;
			auto str_ptr = str;
			int written;

			bool do_cid = true;
			if (hid_status == UACPI_STATUS_OK) {
				do_cid = uacpi_eval_cid(node, &cid) == UACPI_STATUS_OK;
			}

			if (do_cid) {
				for (size_t i = 0; i < cid->num_ids; ++i) {
					auto* cid_id = &cid->ids[i];
					if (cid_id->size - 1 == 8) {
						ven_str = cid_id->value;
						ven_str_len = 4;
						dev_str = cid_id->value + 4;
					}
					else {
						assert(cid_id->size - 1 == 7);
						ven_str = cid_id->value;
						ven_str_len = 3;
						dev_str = cid_id->value + 3;
					}

					if (sub_status == UACPI_STATUS_OK) {
						if (hrv_status == UACPI_STATUS_OK) {
							written = swprintf(
								str_ptr,
								remaining,
								L"ACPI\\VEN_%.*s&DEV_%.4s&SUBSYS_%s&REV_%04X",
								ven_str,
								ven_str_len,
								dev_str,
								sub_str.text,
								static_cast<unsigned int>(hrv));
							str_ptr += written + 1;
							remaining -= written + 1;
						}

						written = swprintf(
							str_ptr,
							remaining,
							L"ACPI\\VEN_%.*s&DEV_%.4s&SUBSYS_%s",
							ven_str,
							ven_str_len,
							dev_str,
							sub_str.text);
						str_ptr += written + 1;
						remaining -= written + 1;
					}

					if (hrv_status == UACPI_STATUS_OK) {
						written = swprintf(
							str_ptr,
							remaining,
							L"ACPI\\VEN_%.*s&DEV_%.4s&REV_%04X",
							ven_str,
							ven_str_len,
							dev_str,
							static_cast<unsigned int>(hrv));
						str_ptr += written + 1;
						remaining -= written + 1;
					}

					written = swprintf(
						str_ptr,
						remaining,
						L"ACPI\\VEN_%.*s&DEV_%.4s",
						ven_str,
						ven_str_len,
						dev_str,
						sub_str.text);
					str_ptr += written + 1;
					remaining -= written + 1;
				}
			}

			uacpi_id_string* cls;
			auto cls_status = uacpi_eval_cls(node, &cls);
			if (cls_status == UACPI_STATUS_OK) {
				written = swprintf(
					str_ptr,
					remaining,
					L"ACPI\\CLS_00%.2s",
					cls->value);
				str_ptr += written + 1;
				remaining -= written + 1;

				written = swprintf(
					str_ptr,
					remaining,
					L"ACPI\\CLS_00%.2s&SUBCLS_00%.2s",
					cls->value + 2);
				str_ptr += written + 1;

				uacpi_free_id_string(cls);
			}

			if (hid_status == UACPI_STATUS_OK && do_cid) {
				uacpi_free_pnp_id_list(cid);
			}

			*str_ptr = 0;
			str_ptr[1] = 0;
		}
		else {
			if (hid_status == UACPI_STATUS_OK) {
				uacpi_free_id_string(id);
			}
			else {
				uacpi_free_pnp_id_list(cid);
			}

			if (sub_status == UACPI_STATUS_OK) {
				uacpi_object_unref(sub);
			}

			ExFreePool(str);

			irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
			IofCompleteRequest(irp, IO_NO_INCREMENT);
			return STATUS_NOT_IMPLEMENTED;
		}

		if (hid_status == UACPI_STATUS_OK) {
			uacpi_free_id_string(id);
		}
		else {
			uacpi_free_pnp_id_list(cid);
		}

		if (sub_status == UACPI_STATUS_OK) {
			uacpi_object_unref(sub);
		}

		irp->IoStatus.Information = reinterpret_cast<ULONG_PTR>(str);
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
	DbgPrintEx(DPFLTR_ACPI_ID, DPFLTR_INFO_LEVEL, "uacpi.sys: initializing\n");

	driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = handle_device_control;
	driver->MajorFunction[IRP_MJ_PNP] = handle_pnp;

	return STATUS_SUCCESS;
}
