#include <uacpi/kernel_api.h>
#include "mem/mem.hpp"
#include "mem/malloc.hpp"
#include "x86/io.hpp"
#include "stdio.hpp"
#include "arch/misc.hpp"
#include "dev/clock.hpp"
#include "dev/hal.hpp"
#include "arch/arch_sched.hpp"
#include "arch/irql.hpp"
#include "sched/thread.hpp"
#include "arch/cpu.hpp"
#include "sched/semaphore.hpp"
#include <hz/spinlock.hpp>

extern void* GLOBAL_RSDP;

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr* out_rsdp_address) {
	*out_rsdp_address = to_phys(GLOBAL_RSDP);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_device_open(
	uacpi_pci_address address,
	uacpi_handle* out_handle) {
	static_assert(sizeof(address) <= sizeof(*out_handle));
	memcpy(*out_handle, &address, sizeof(address));
	return UACPI_STATUS_OK;
}

void uacpi_kernel_pci_device_close(uacpi_handle) {}

struct PciInfo {
	u8 bus;
	u32 slot;
};

static PciInfo uacpi_handle_to_pci(uacpi_handle handle) {
	uacpi_pci_address addr {};
	memcpy(&addr, &handle, sizeof(addr));
	PCI_SLOT_NUMBER slot {
		.u {
			.bits {
				.device_number = addr.device,
				.function_number = addr.function,
				.reserved = 0
			}
		}
	};
	return {addr.bus, slot.u.as_u32};
}

uacpi_status uacpi_kernel_pci_read8(uacpi_handle device, uacpi_size offset, uacpi_u8* value) {
	auto [bus, slot] = uacpi_handle_to_pci(device);
	HalGetBusDataByOffset(BUS_DATA_TYPE::PciConfiguration, bus, slot, value, offset, 1);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read16(uacpi_handle device, uacpi_size offset, uacpi_u16* value) {
	auto [bus, slot] = uacpi_handle_to_pci(device);
	HalGetBusDataByOffset(BUS_DATA_TYPE::PciConfiguration, bus, slot, value, offset, 2);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read32(uacpi_handle device, uacpi_size offset, uacpi_u32* value) {
	auto [bus, slot] = uacpi_handle_to_pci(device);
	HalGetBusDataByOffset(BUS_DATA_TYPE::PciConfiguration, bus, slot, value, offset, 4);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write8(uacpi_handle device, uacpi_size offset, uacpi_u8 value) {
	auto [bus, slot] = uacpi_handle_to_pci(device);
	HalSetBusDataByOffset(BUS_DATA_TYPE::PciConfiguration, bus, slot, &value, offset, 1);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write16(uacpi_handle device, uacpi_size offset, uacpi_u16 value) {
	auto [bus, slot] = uacpi_handle_to_pci(device);
	HalSetBusDataByOffset(BUS_DATA_TYPE::PciConfiguration, bus, slot, &value, offset, 2);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write32(uacpi_handle device, uacpi_size offset, uacpi_u32 value) {
	auto [bus, slot] = uacpi_handle_to_pci(device);
	HalSetBusDataByOffset(BUS_DATA_TYPE::PciConfiguration, bus, slot, &value, offset, 4);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_map(
	uacpi_io_addr base,
	uacpi_size len,
	uacpi_handle* out_handle) {
	*out_handle = reinterpret_cast<uacpi_handle>(base);
	return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle handle) {}

uacpi_status uacpi_kernel_io_read8(uacpi_handle handle, uacpi_size offset, uacpi_u8* out_value) {
	auto port = reinterpret_cast<usize>(handle) + offset;
	*out_value = x86::in1(port);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read16(uacpi_handle handle, uacpi_size offset, uacpi_u16* out_value) {
	auto port = reinterpret_cast<usize>(handle) + offset;
	*out_value = x86::in2(port);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read32(uacpi_handle handle, uacpi_size offset, uacpi_u32* out_value) {
	auto port = reinterpret_cast<usize>(handle) + offset;
	*out_value = x86::in4(port);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write8(uacpi_handle handle, uacpi_size offset, uacpi_u8 in_value) {
	auto port = reinterpret_cast<usize>(handle) + offset;
	x86::out1(port, in_value);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write16(uacpi_handle handle, uacpi_size offset, uacpi_u16 in_value) {
	auto port = reinterpret_cast<usize>(handle) + offset;
	x86::out2(port, in_value);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write32(uacpi_handle handle, uacpi_size offset, uacpi_u32 in_value) {
	auto port = reinterpret_cast<usize>(handle) + offset;
	x86::out4(port, in_value);
	return UACPI_STATUS_OK;
}

void* uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len) {
	return to_virt<void>(addr);
}

void uacpi_kernel_unmap(void* addr, uacpi_size len) {}

void* uacpi_kernel_alloc(uacpi_size size) {
	return kmalloc(size);
}

void uacpi_kernel_free(void* mem, uacpi_size size_hint) {
	kfree(mem, size_hint);
}

void uacpi_kernel_log(uacpi_log_level, const uacpi_char* msg) {
	println("uACPI: ", msg);
}

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot() {
	return CLOCK_SOURCE->get_ns();
}

void uacpi_kernel_stall(uacpi_u8 usec) {
	udelay(usec);
}

void uacpi_kernel_sleep(uacpi_u64 msec) {
	auto thread = get_current_thread();
	auto old = KfRaiseIrql(DISPATCH_LEVEL);
	thread->cpu->scheduler.sleep(msec * 1000);
	KeLowerIrql(old);
}

uacpi_handle uacpi_kernel_create_mutex() {
	auto ptr = new KSEMAPHORE {};
	KeInitializeSemaphore(ptr, 1, 1);
	return ptr;
}

void uacpi_kernel_free_mutex(uacpi_handle mutex) {
	delete static_cast<KSEMAPHORE*>(mutex);
}

uacpi_handle uacpi_kernel_create_event() {
	auto ptr = new KSEMAPHORE {};
	KeInitializeSemaphore(ptr, 0, INT32_MAX);
	return ptr;
}

void uacpi_kernel_free_event(uacpi_handle event) {
	delete static_cast<KSEMAPHORE*>(event);
}

uacpi_thread_id uacpi_kernel_get_thread_id() {
	return get_current_thread();
}

uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle mutex, uacpi_u16 timeout) {
	i64 timeout_value = static_cast<i64>(timeout * (NS_IN_MS / 100));
	auto status = KeWaitForSingleObject(
		mutex,
		KWAIT_REASON::Executive,
		KPROCESSOR_MODE::Kernel,
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
	uacpi_interrupt_handler,
	uacpi_handle ctx,
	uacpi_handle* out_irq_handle) {
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(
	uacpi_interrupt_handler,
	uacpi_handle irq_handle) {
	return UACPI_STATUS_OK;
}

uacpi_handle uacpi_kernel_create_spinlock() {
	return new hz::spinlock<void> {};
}

void uacpi_kernel_free_spinlock(uacpi_handle handle) {
	delete reinterpret_cast<hz::spinlock<void>*>(handle);
}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle handle) {
	auto old = arch_enable_irqs(false);
	auto* lock = reinterpret_cast<hz::spinlock<void>*>(handle);
	lock->manual_lock();
	return old;
}

void uacpi_kernel_unlock_spinlock(uacpi_handle handle, uacpi_cpu_flags old) {
	auto* lock = reinterpret_cast<hz::spinlock<void>*>(handle);
	lock->manual_unlock();
	arch_enable_irqs(old);
}

uacpi_status uacpi_kernel_schedule_work(
	uacpi_work_type,
	uacpi_work_handler,
	uacpi_handle ctx) {
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_wait_for_work_completion() {
	return UACPI_STATUS_OK;
}
