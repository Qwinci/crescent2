#include "dpc.hpp"
#include "arch/cpu.hpp"
#include "assert.hpp"
#include "arch/irq.hpp"
#include "dev/irq.hpp"

NTAPI void KeInitializeDpc(KDPC* dpc, PKDEFERRED_ROUTINE deferred_routine, void* deferred_ctx) {
	dpc->target_info = 0;
	dpc->importance = static_cast<u8>(DpcImportance::Medium);
	dpc->number = get_current_cpu()->number;
	dpc->dpc_list_entry.Next = nullptr;
	dpc->deferred_routine = deferred_routine;
	dpc->deferred_ctx = deferred_ctx;
	dpc->dpc_data = nullptr;
}

NTAPI bool KeInsertQueueDpc(KDPC* dpc, void* system_arg1, void* system_arg2) {
	if (__atomic_load_n(&dpc->dpc_data, __ATOMIC_RELAXED)) {
		return false;
	}

	auto cpu = CPUS[dpc->number];

	// high level is needed to prevent getting interrupted by any code at all
	KIRQL old = KfRaiseIrql(HIGH_LEVEL);

	KeAcquireSpinLockAtDpcLevel(&cpu->dpc_list_lock);

	dpc->system_arg1 = system_arg1;
	dpc->system_arg2 = system_arg2;
	dpc->dpc_data = cpu;

	if (dpc->importance == static_cast<u8>(DpcImportance::High) ||
		!cpu->dpc_list_head.Next) {
		dpc->dpc_list_entry.Next = cpu->dpc_list_head.Next;
		cpu->dpc_list_head.Next = &dpc->dpc_list_entry;
		cpu->dpc_list_tail = &dpc->dpc_list_entry;
	}
	else {
		cpu->dpc_list_tail->Next = &dpc->dpc_list_entry;
		cpu->dpc_list_tail = &dpc->dpc_list_entry;
	}

	if (cpu == get_current_cpu()) {
		arch_request_software_irq(DISPATCH_LEVEL);
	}

	KeReleaseSpinLock(&cpu->dpc_list_lock, old);
	return true;
}

NTAPI BOOLEAN KeRemoveQueueDpc(KDPC* dpc) {
	if (!__atomic_load_n(&dpc->dpc_data, __ATOMIC_RELAXED)) {
		return false;
	}

	auto cpu = CPUS[dpc->number];

	// high level is needed to prevent getting interrupted by any code at all
	KIRQL old = KfRaiseIrql(HIGH_LEVEL);

	KeAcquireSpinLockAtDpcLevel(&cpu->dpc_list_lock);

	assert(dpc->dpc_data);
	SINGLE_LIST_ENTRY* entry = &cpu->dpc_list_head;
	while (entry->Next != &dpc->dpc_list_entry) {
		assert(entry->Next);
		entry = entry->Next;
	}

	entry->Next = dpc->dpc_list_entry.Next;
	if (!entry->Next) {
		assert(&dpc->dpc_list_entry == cpu->dpc_list_tail);
		cpu->dpc_list_tail = entry;
	}

	__atomic_store_n(&dpc->dpc_data, nullptr, __ATOMIC_RELAXED);

	KeReleaseSpinLock(&cpu->dpc_list_lock, old);
	return true;
}

NTAPI void KeSetImportanceDpc(KDPC* dpc, DpcImportance importance) {
	dpc->importance = static_cast<u8>(importance);
}

void process_dpcs() {
	assert(KeGetCurrentIrql() == DISPATCH_LEVEL);

	auto* cpu = get_current_cpu();

	while (true) {
		KfRaiseIrql(HIGH_LEVEL);
		KeAcquireSpinLockAtDpcLevel(&cpu->dpc_list_lock);

		auto entry = cpu->dpc_list_head.Next;
		KDPC* dpc;
		void* ctx;
		void* arg0;
		void* arg1;
		if (entry) {
			cpu->dpc_list_head.Next = entry->Next;
			dpc = hz::container_of(entry, &KDPC::dpc_list_entry);
			ctx = dpc->deferred_ctx;
			arg0 = dpc->system_arg1;
			arg1 = dpc->system_arg2;
			__atomic_store_n(&dpc->dpc_data, nullptr, __ATOMIC_RELAXED);
		}

		KeReleaseSpinLockFromDpcLevel(&cpu->dpc_list_lock);
		KeLowerIrql(DISPATCH_LEVEL);

		if (entry) {
			dpc->deferred_routine(
				dpc,
				ctx,
				arg0,
				arg1);
		}
		else {
			break;
		}
	}
}

void dispatcher_process() {
	process_dpcs();

	auto* cpu = get_current_cpu();
	if (cpu->quantum_end) {
		cpu->scheduler.handle_quantum_end(cpu);
	}
}

bool dispatch_interrupt(KINTERRUPT*, void*) {
	dispatcher_process();
	return true;
}

KINTERRUPT DISPATCH_IRQ_HANDLER {
	.fn = dispatch_interrupt
};
