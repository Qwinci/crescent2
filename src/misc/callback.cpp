#include "callback.hpp"
#include "utils/spinlock.hpp"
#include "rtl.hpp"
#include "mem/malloc.hpp"
#include <hz/container_of.hpp>

struct Callback {
	LIST_ENTRY entry;
	CALLBACK_OBJECT* obj;
	PCALLBACK_FUNCTION fn;
	PVOID ctx;
	volatile ULONG refs;
};

struct CALLBACK_OBJECT {
	LIST_ENTRY callbacks;
	KSPIN_LOCK lock;
	BOOLEAN multiple_callbacks;
};

static OBJECT_TYPE* CALLBACK_TYPE;

void callback_init() {
	UNICODE_STRING name = RTL_CONSTANT_STRING(u"Callback");
	OBJECT_TYPE_INITIALIZER init {};
	init.delete_proc = [](PVOID ptr) {
		auto* callback = static_cast<CALLBACK_OBJECT*>(ptr);
		assert(IsListEmpty(&callback->callbacks));
	};
	auto status = ObCreateObjectType(&name, &init, nullptr, &CALLBACK_TYPE);
	assert(NT_SUCCESS(status));

	HANDLE dir_handle;
	OBJECT_ATTRIBUTES attribs {};
	name = RTL_CONSTANT_STRING(u"\\Callbacks");
	InitializeObjectAttributes(&attribs, &name, 0, nullptr, nullptr);
	status = ZwCreateDirectoryObject(
		&dir_handle,
		0,
		&attribs);
	assert(NT_SUCCESS(status));
}

NTAPI NTSTATUS ExCreateCallback(
	CALLBACK_OBJECT** callback,
	OBJECT_ATTRIBUTES* object_attribs,
	BOOLEAN create,
	BOOLEAN allow_multiple_callbacks) {
	if (!object_attribs->object_name) {
		return STATUS_UNSUCCESSFUL;
	}

	auto status = ObReferenceObjectByName(
		object_attribs->object_name,
		object_attribs->attributes,
		nullptr,
		0,
		CALLBACK_TYPE,
		KernelMode,
		nullptr,
		reinterpret_cast<PVOID*>(callback));
	if (status == STATUS_OBJECT_NAME_NOT_FOUND) {
		if (!create) {
			return status;
		}

		CALLBACK_OBJECT* object;
		status = ObCreateObject(
			KernelMode,
			CALLBACK_TYPE,
			object_attribs,
			KernelMode,
			nullptr,
			sizeof(CALLBACK_OBJECT),
			0,
			sizeof(CALLBACK_OBJECT),
			reinterpret_cast<PVOID*>(&object));
		if (!NT_SUCCESS(status)) {
			return status;
		}

		InitializeListHead(&object->callbacks);
		object->multiple_callbacks = allow_multiple_callbacks;

		*callback = object;

		return ObInsertObject(
			object,
			nullptr,
			0,
			0,
			nullptr,
			nullptr);
	}
	else {
		return status;
	}
}

NTAPI void ExNotifyCallback(
	PVOID callback_object,
	PVOID arg1,
	PVOID arg2) {
	auto* obj = static_cast<CALLBACK_OBJECT*>(callback_object);

	auto old = KeAcquireSpinLockRaiseToDpc(&obj->lock);

	if (!IsListEmpty(&obj->callbacks)) {
		for (auto* entry = obj->callbacks.Flink; entry != &obj->callbacks; entry = entry->Flink) {
			auto* callback = hz::container_of(entry, &Callback::entry);
			callback->refs += 1;

			KeReleaseSpinLock(&obj->lock, old);
			callback->fn(callback->ctx, arg1, arg2);
			old = KeAcquireSpinLockRaiseToDpc(&obj->lock);

			callback->refs -= 1;
		}
	}

	KeReleaseSpinLock(&obj->lock, old);
}

NTAPI PVOID ExRegisterCallback(
	CALLBACK_OBJECT* callback_object,
	PCALLBACK_FUNCTION callback_fn,
	PVOID callback_ctx) {
	auto old = KeAcquireSpinLockRaiseToDpc(&callback_object->lock);

	if (!callback_object->multiple_callbacks && !IsListEmpty(&callback_object->callbacks)) {
		KeReleaseSpinLock(&callback_object->lock, old);
		return nullptr;
	}

	auto* obj = static_cast<Callback*>(kcalloc(sizeof(Callback)));
	if (!obj) {
		KeReleaseSpinLock(&callback_object->lock, old);
		return nullptr;
	}

	obj->obj = callback_object;
	obj->fn = callback_fn;
	obj->ctx = callback_ctx;
	InsertHeadList(&callback_object->callbacks, &obj->entry);

	KeReleaseSpinLock(&callback_object->lock, old);
	return obj;
}

NTAPI void ExUnregisterCallback(PVOID callback_registration) {
	if (!callback_registration) {
		return;
	}

	auto* callback = static_cast<Callback*>(callback_registration);
	assert(callback->obj);

	auto* obj = callback->obj;

	auto old = KeAcquireSpinLockRaiseToDpc(&obj->lock);

	if (callback->refs != 0) {
		while (true) {
			if (callback->refs == 0) {
				break;
			}

			KeReleaseSpinLock(&obj->lock, old);

			old = KeAcquireSpinLockRaiseToDpc(&obj->lock);
		}
	}

	RemoveEntryList(&callback->entry);

	KeReleaseSpinLock(&obj->lock, old);
}
