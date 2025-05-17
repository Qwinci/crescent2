#include <uacpi/kernel_api.h>
#include <wdm.h>

namespace {
	struct Work {
		LIST_ENTRY hook {};
		uacpi_work_handler handler {};
		uacpi_handle ctx {};
	};

	HANDLE ACPI_WORKER;
	LIST_ENTRY WORK_LIST {};
	KSPIN_LOCK WORK_LOCK {};
	KEVENT WORK_EVENT {};
	ULONG ACPI_IRQ;

	ULONG_PTR GLOBAL_RSDP;

	struct UacpiIrqHandler {
		uacpi_interrupt_handler handler;
		uacpi_handle ctx;
	};

	UacpiIrqHandler UACPI_IRQ_HANDLER;
}

[[noreturn]] static void acpi_worker_fn(PVOID) {
	while (true) {
		auto old = KeAcquireSpinLockRaiseToDpc(&WORK_LOCK);
		auto work = CONTAINING_RECORD(RemoveHeadList(&WORK_LIST), Work, hook);
		KeReleaseSpinLock(&WORK_LOCK, old);

		if (!work) {
			KeWaitForSingleObject(&WORK_EVENT, Executive, KernelMode, false, nullptr);
			continue;
		}

		work->handler(work->ctx);
		ExFreePool(work);
	}
}

void glue_set_rsdp(ULONG_PTR rsdp) {
	GLOBAL_RSDP = rsdp;
}

ULONG glue_get_irq() {
	return ACPI_IRQ;
}

void glue_init(PDEVICE_OBJECT device, ULONG irq_vec, KIRQL irql, KINTERRUPT_MODE mode) {
	InitializeListHead(&WORK_LIST);

	KeInitializeEvent(&WORK_EVENT, SynchronizationEvent, false);

	auto status = PsCreateSystemThread(
		&ACPI_WORKER,
		0,
		nullptr,
		nullptr,
		nullptr,
		acpi_worker_fn,
		nullptr);
	assert(status == STATUS_SUCCESS);

	PKINTERRUPT irq;
	IO_CONNECT_INTERRUPT_PARAMETERS params {
		.Version = CONNECT_LINE_BASED,
		.LineBased = (IO_CONNECT_INTERRUPT_LINE_BASED_PARAMETERS) {
			.PhysicalDeviceObject = device,
			.InterruptObject = &irq,
			.ServiceRoutine = [](PKINTERRUPT, PVOID) -> BOOLEAN {
				assert(UACPI_IRQ_HANDLER.handler);
				return UACPI_IRQ_HANDLER.handler(UACPI_IRQ_HANDLER.ctx) == UACPI_INTERRUPT_HANDLED;
			},
			.ServiceContext = nullptr,
			.SpinLock = nullptr,
			.SynchronizeIrql = irql,
			.FloatingSave = false
		}
	};
	status = IoConnectInterruptEx(&params);
	assert(status == STATUS_SUCCESS);
}

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr* out_rsdp_address) {
	*out_rsdp_address = GLOBAL_RSDP;
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_device_open(
	uacpi_pci_address address,
	uacpi_handle* out_handle) {
	static_assert(sizeof(address) <= sizeof(*out_handle));
	memcpy(out_handle, &address, sizeof(address));
	return UACPI_STATUS_OK;
}

void uacpi_kernel_pci_device_close(uacpi_handle) {}

struct PciInfo {
	uint8_t bus;
	uint32_t slot;
};

static PciInfo uacpi_handle_to_pci(uacpi_handle handle) {
	uacpi_pci_address addr {};
	memcpy(&addr, &handle, sizeof(addr));
	PCI_SLOT_NUMBER slot {
		.u {
			.bits {
				.DeviceNumber = addr.device,
				.FunctionNumber = addr.function,
				.Reserved = 0
			}
		}
	};
	return {addr.bus, slot.u.AsULONG};
}

uacpi_status uacpi_kernel_pci_read8(uacpi_handle device, uacpi_size offset, uacpi_u8* value) {
	auto [bus, slot] = uacpi_handle_to_pci(device);
	HalGetBusDataByOffset(PCIConfiguration, bus, slot, value, offset, 1);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read16(uacpi_handle device, uacpi_size offset, uacpi_u16* value) {
	auto [bus, slot] = uacpi_handle_to_pci(device);
	HalGetBusDataByOffset(PCIConfiguration, bus, slot, value, offset, 2);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read32(uacpi_handle device, uacpi_size offset, uacpi_u32* value) {
	auto [bus, slot] = uacpi_handle_to_pci(device);
	HalGetBusDataByOffset(PCIConfiguration, bus, slot, value, offset, 4);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write8(uacpi_handle device, uacpi_size offset, uacpi_u8 value) {
	auto [bus, slot] = uacpi_handle_to_pci(device);
	HalSetBusDataByOffset(PCIConfiguration, bus, slot, &value, offset, 1);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write16(uacpi_handle device, uacpi_size offset, uacpi_u16 value) {
	auto [bus, slot] = uacpi_handle_to_pci(device);
	HalSetBusDataByOffset(PCIConfiguration, bus, slot, &value, offset, 2);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write32(uacpi_handle device, uacpi_size offset, uacpi_u32 value) {
	auto [bus, slot] = uacpi_handle_to_pci(device);
	HalSetBusDataByOffset(PCIConfiguration, bus, slot, &value, offset, 4);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_map(
	uacpi_io_addr base,
	uacpi_size,
	uacpi_handle* out_handle) {
	*out_handle = reinterpret_cast<uacpi_handle>(base);
	return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle) {}

uacpi_status uacpi_kernel_io_read8(uacpi_handle handle, uacpi_size offset, uacpi_u8* out_value) {
	uint16_t port = reinterpret_cast<uintptr_t>(handle) + offset;
	uint8_t value;
	asm volatile("in %0, %1" : "=a"(value) : "Nd"(port));
	*out_value = value;
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read16(uacpi_handle handle, uacpi_size offset, uacpi_u16* out_value) {
	uint16_t port = reinterpret_cast<uintptr_t>(handle) + offset;
	uint16_t value;
	asm volatile("in %0, %1" : "=a"(value) : "Nd"(port));
	*out_value = value;
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read32(uacpi_handle handle, uacpi_size offset, uacpi_u32* out_value) {
	uint16_t port = reinterpret_cast<uintptr_t>(handle) + offset;
	uint32_t value;
	asm volatile("in %0, %1" : "=a"(value) : "Nd"(port));
	*out_value = value;
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write8(uacpi_handle handle, uacpi_size offset, uacpi_u8 in_value) {
	uint16_t port = reinterpret_cast<uintptr_t>(handle) + offset;
	asm volatile("out %0, %1" : : "Nd"(port), "a"(in_value));
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write16(uacpi_handle handle, uacpi_size offset, uacpi_u16 in_value) {
	uint16_t port = reinterpret_cast<uintptr_t>(handle) + offset;
	asm volatile("out %0, %1" : : "Nd"(port), "a"(in_value));
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write32(uacpi_handle handle, uacpi_size offset, uacpi_u32 in_value) {
	uint16_t port = reinterpret_cast<uintptr_t>(handle) + offset;
	asm volatile("out %0, %1" : : "Nd"(port), "a"(in_value));
	return UACPI_STATUS_OK;
}

void* uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len) {
	return MmMapIoSpace({.QuadPart = static_cast<LONGLONG>(addr)}, len, MmCached);
}

void uacpi_kernel_unmap(void* addr, uacpi_size len) {
	MmUnmapIoSpace(addr, len);
}

void* uacpi_kernel_alloc(uacpi_size size) {
	return ExAllocatePool2(0, size, 0);
}

void uacpi_kernel_free(void* mem) {
	ExFreePool(mem);
}

void uacpi_kernel_log(uacpi_log_level, const uacpi_char* msg) {
	// todo use %s
	DbgPrintEx(DPFLTR_ACPI_ID, DPFLTR_TRACE_LEVEL, msg);
}

static constexpr auto NS_IN_MS = 1000 * 1000;
static constexpr auto NS_IN_SECOND = 1000ULL * 1000ULL * 1000ULL;

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot() {
	LARGE_INTEGER freq;
	auto value = KeQueryPerformanceCounter(&freq);
	value.QuadPart *= NS_IN_SECOND;
	return value.QuadPart / freq.QuadPart;
}

void uacpi_kernel_stall(uacpi_u8 usec) {
	LARGE_INTEGER freq;
	auto start = KeQueryPerformanceCounter(&freq);
	auto end = start;
	end.QuadPart += usec * (freq.QuadPart / 1000 / 1000);

	while (KeQueryPerformanceCounter(nullptr).QuadPart < end.QuadPart);
}

void uacpi_kernel_sleep(uacpi_u64 msec) {
	LARGE_INTEGER interval {};
	interval.QuadPart = -static_cast<LONGLONG>(msec * 1000 * 10);
	KeDelayExecutionThread(KernelMode, false, &interval);
}

uacpi_handle uacpi_kernel_create_mutex() {
	auto ptr = static_cast<KSEMAPHORE*>(ExAllocatePool2(0, sizeof(KSEMAPHORE), 0));
	if (!ptr) {
		return nullptr;
	}
	KeInitializeSemaphore(ptr, 1, 1);
	return ptr;
}

void uacpi_kernel_free_mutex(uacpi_handle mutex) {
	ExFreePool(mutex);
}

uacpi_handle uacpi_kernel_create_event() {
	auto ptr = static_cast<KSEMAPHORE*>(ExAllocatePool2(0, sizeof(KSEMAPHORE), 0));
	if (!ptr) {
		return nullptr;
	}
	KeInitializeSemaphore(ptr, 0, INT32_MAX);
	return ptr;
}

void uacpi_kernel_free_event(uacpi_handle event) {
	ExFreePool(event);
}

uacpi_thread_id uacpi_kernel_get_thread_id() {
	return KeGetCurrentThread();
}

uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle mutex, uacpi_u16 timeout) {
	LARGE_INTEGER timeout_value {
		.QuadPart = static_cast<LONGLONG>(timeout) * (NS_IN_MS / 100)
	};
	auto status = KeWaitForSingleObject(
		mutex,
		Executive,
		KernelMode,
		false,
		timeout != 0xFFFF ? &timeout_value : nullptr);
	return status == STATUS_TIMEOUT ? UACPI_STATUS_TIMEOUT : UACPI_STATUS_OK;
}

void uacpi_kernel_release_mutex(uacpi_handle mutex) {
	auto semaphore = static_cast<KSEMAPHORE*>(mutex);
	KeReleaseSemaphore(semaphore, 0, 1, false);
}

uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle event, uacpi_u16 timeout) {
	return uacpi_kernel_acquire_mutex(event, timeout) == UACPI_STATUS_OK;
}

void uacpi_kernel_signal_event(uacpi_handle event) {
	auto semaphore = static_cast<KSEMAPHORE*>(event);
	KeReleaseSemaphore(semaphore, 0, 1, false);
}

void uacpi_kernel_reset_event(uacpi_handle event) {
	auto semaphore = static_cast<KSEMAPHORE*>(event);
	KeInitializeSemaphore(semaphore, 0, INT32_MAX);
}

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request*) {
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_install_interrupt_handler(
	uacpi_u32 irq,
	uacpi_interrupt_handler handler,
	uacpi_handle ctx,
	uacpi_handle*) {
	DbgPrintEx(0, 0, "set handler\n");
	assert(handler);
	UACPI_IRQ_HANDLER.handler = handler;
	UACPI_IRQ_HANDLER.ctx = ctx;
	ACPI_IRQ = irq;
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(
	uacpi_interrupt_handler,
	uacpi_handle) {
	return UACPI_STATUS_OK;
}

uacpi_handle uacpi_kernel_create_spinlock() {
	auto ptr = ExAllocatePool2(0, sizeof(KSPIN_LOCK), 0);
	if (!ptr) {
		return nullptr;
	}
	KeInitializeSpinLock(static_cast<KSPIN_LOCK*>(ptr));
	return ptr;
}

void uacpi_kernel_free_spinlock(uacpi_handle handle) {
	ExFreePool(handle);
}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle handle) {
	auto old = KfRaiseIrql(HIGH_LEVEL);
	KeAcquireSpinLockAtDpcLevel(static_cast<PKSPIN_LOCK>(handle));
	return old;
}

void uacpi_kernel_unlock_spinlock(uacpi_handle handle, uacpi_cpu_flags old) {
	KeReleaseSpinLock(static_cast<PKSPIN_LOCK>(handle), old);
}

uacpi_status uacpi_kernel_schedule_work(
	uacpi_work_type,
	uacpi_work_handler handler,
	uacpi_handle ctx) {
	auto work = static_cast<Work*>(ExAllocatePool2(0, sizeof(Work), 0));
	if (!work) {
		return UACPI_STATUS_OUT_OF_MEMORY;
	}
	work->handler = handler;
	work->ctx = ctx;

	auto old = KeAcquireSpinLockRaiseToDpc(&WORK_LOCK);
	InsertTailList(&WORK_LIST, &work->hook);
	KeReleaseSpinLock(&WORK_LOCK, old);
	KeSetEvent(&WORK_EVENT, 0, false);

	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_wait_for_work_completion() {
	while (true) {
		auto old = KeAcquireSpinLockRaiseToDpc(&WORK_LOCK);
		auto empty = IsListEmpty(&WORK_LIST);
		KeReleaseSpinLock(&WORK_LOCK, old);

		if (empty) {
			break;
		}

		KeWaitForSingleObject(&WORK_EVENT, Executive, KernelMode, false, nullptr);
	}

	return UACPI_STATUS_OK;
}
