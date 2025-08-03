#include "ps.hpp"
#include "thread.hpp"
#include "arch/cpu.hpp"
#include "rtl.hpp"
#include "sys/misc.hpp"

NTAPI extern "C" OBJECT_TYPE* PsProcessType = nullptr;
NTAPI extern "C" OBJECT_TYPE* PsThreadType = nullptr;

void ps_init() {
	UNICODE_STRING name = RTL_CONSTANT_STRING(u"Process");
	OBJECT_TYPE_INITIALIZER init {};
	init.delete_proc = [](PVOID object) {
		static_cast<Process*>(object)->~Process();
	};
	auto status = ObCreateObjectType(
		&name,
		&init,
		nullptr,
		&PsProcessType);
	assert(NT_SUCCESS(status));

	name = RTL_CONSTANT_STRING(u"Thread");
	init.delete_proc = [](PVOID object) {
		static_cast<Thread*>(object)->~Thread();
	};
	status = ObCreateObjectType(
		&name,
		&init,
		nullptr,
		&PsThreadType);
	assert(NT_SUCCESS(status));

	void* ptr;
	status = ObCreateObject(
		KernelMode,
		PsProcessType,
		nullptr,
		KernelMode,
		nullptr,
		sizeof(Process),
		0,
		sizeof(Process),
		&ptr);
	assert(NT_SUCCESS(status));

	KERNEL_PROCESS = new (ptr) Process {*KERNEL_MAP};

	auto handle = SCHED_HANDLE_TABLE.insert(KERNEL_PROCESS);
	assert(handle != INVALID_HANDLE_VALUE);
	KERNEL_PROCESS->handle = handle;
}

Process* create_process(kstd::wstring_view name) {
	auto mode = ExGetPreviousMode();
	void* ptr;
	auto status = ObCreateObject(
		KernelMode,
		PsProcessType,
		nullptr,
		mode,
		nullptr,
		sizeof(Process),
		0,
		sizeof(Process),
		&ptr);
	if (!NT_SUCCESS(status)) {
		return nullptr;
	}

	auto* proc = new (ptr) Process {name};

	auto handle = SCHED_HANDLE_TABLE.insert(proc);
	if (handle == INVALID_HANDLE_VALUE) {
		ObfDereferenceObject(proc);
		return nullptr;
	}
	proc->handle = handle;

	return proc;
}

Thread* create_initial_thread(Cpu* cpu) {
	void* ptr;
	auto status = ObCreateObject(
		KernelMode,
		PsThreadType,
		nullptr,
		KernelMode,
		nullptr,
		sizeof(Thread),
		0,
		sizeof(Thread),
		&ptr);
	if (!NT_SUCCESS(status)) {
		return nullptr;
	}

	auto* thread = new (ptr) Thread {u"kernel main", cpu, &*KERNEL_PROCESS};

	auto handle = SCHED_HANDLE_TABLE.insert(thread);
	assert(handle != INVALID_HANDLE_VALUE);
	thread->handle = handle;

	return thread;
}

Thread* create_thread(kstd::wstring_view name, Cpu* cpu, Process* process, bool user, void (*fn)(void*), void* arg) {
	auto mode = ExGetPreviousMode();
	void* ptr;
	auto status = ObCreateObject(
		KernelMode,
		PsThreadType,
		nullptr,
		mode,
		nullptr,
		sizeof(Thread),
		0,
		sizeof(Thread),
		&ptr);
	if (!NT_SUCCESS(status)) {
		return nullptr;
	}

	auto* thread = new (ptr) Thread {name, cpu, process, user, fn, arg};

	auto handle = SCHED_HANDLE_TABLE.insert(thread);
	if (handle == INVALID_HANDLE_VALUE) {
		ObfDereferenceObject(thread);
		return nullptr;
	}
	thread->handle = handle;

	return thread;
}

NTAPI NTSTATUS PsCreateSystemThread(
	PHANDLE thread_handle,
	ULONG desired_access,
	OBJECT_ATTRIBUTES* object_attribs,
	HANDLE process_handle,
	CLIENT_ID* client_id,
	PKSTART_ROUTINE start_routine,
	PVOID start_ctx) {
	/*auto* cpu = get_current_cpu();

	Thread* thread;

	hz::optional<Handle> generic_proc_handle {};
	if (process_handle != nullptr) {
		generic_proc_handle = KERNEL_PROCESS->handle_table.get(process_handle);
		ObjectWrapper<ProcessDescriptor>* desc_ptr;
		if (!generic_proc_handle || !(desc_ptr = generic_proc_handle->get<ObjectWrapper<ProcessDescriptor>>())) {
			return STATUS_INVALID_PARAMETER;
		}

		auto& desc = *desc_ptr;

		auto old = KeAcquireSpinLockRaiseToDpc(&desc->lock);

		// todo add thread to process
		thread = new Thread {u"system thread", cpu, desc->process, false, start_routine, start_ctx};

		KeReleaseSpinLock(&desc->lock, old);
	}
	else {
		thread = new Thread {u"system thread", cpu, &*KERNEL_PROCESS, false, start_routine, start_ctx};
	}

	thread->kernel_apc_disable = true;

	auto desc = thread->create_descriptor();

	auto result_handle = KERNEL_HANDLE_TABLE.insert(Handle {std::move(desc)});
	if (result_handle == INVALID_HANDLE_VALUE) {
		delete thread;
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	*thread_handle = result_handle;

	assert(client_id == nullptr);

	cpu->scheduler.queue(cpu, thread);
	return STATUS_SUCCESS;*/
	return STATUS_SUCCESS;
}

NTAPI HANDLE PsGetCurrentThreadId() {
	return get_current_thread()->handle;
}

NTAPI HANDLE PsGetCurrentProcessId() {
	return get_current_thread()->process->handle;
}
