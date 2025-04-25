#include "registry.hpp"
#include "types.hpp"
#include "wchar.hpp"
#include "utils/spinlock.hpp"
#include "assert.hpp"
#include "string_view.hpp"
#include "object.hpp"
#include "mem/malloc.hpp"
#include "cstring.hpp"
#include "rtl.hpp"
#include "arch/arch_sched.hpp"
#include "sched/thread.hpp"
#include "sys/misc.hpp"
#include "utils/except.hpp"
#include "sys/user_access.hpp"
#include "arch/arch_syscall.hpp"

#include <hz/rb_tree.hpp>

struct RegistryValue {
	constexpr int operator<=>(const RegistryValue& other) const {
		if (name.Length < other.name.Length) {
			return -1;
		}
		else if (name.Length > other.name.Length) {
			return 1;
		}

		return _wcsnicmp(name.Buffer, other.name.Buffer, name.Length / 2);
	}

	constexpr bool operator==(const RegistryValue& other) const {
		if (name.Length != other.name.Length) {
			return false;
		}

		return _wcsnicmp(name.Buffer, other.name.Buffer, name.Length / 2) == 0;
	}

	hz::rb_tree_hook hook {};
	UNICODE_STRING name {};
	ULONG type {};
	ULONG data_size {};
	PVOID data {};
};

struct RegistryObject {
	constexpr int operator<=>(const RegistryObject& other) const {
		if (name.Length < other.name.Length) {
			return -1;
		}
		else if (name.Length > other.name.Length) {
			return 1;
		}

		return _wcsnicmp(name.Buffer, other.name.Buffer, name.Length / 2);
	}

	constexpr bool operator==(const RegistryObject& other) const {
		if (name.Length != other.name.Length) {
			return false;
		}

		return _wcsnicmp(name.Buffer, other.name.Buffer, name.Length / 2) == 0;
	}

	RegistryObject* lookup_child(kstd::wstring_view component) {
		auto* node = children.get_root();
		while (node) {
			if (component.size() < node->name.Length / 2) {
				// NOLINTNEXTLINE
				node = children.get_left(node);
				continue;
			}
			else if (component.size() > node->name.Length / 2) {
				// NOLINTNEXTLINE
				node = children.get_right(node);
				continue;
			}

			int res = _wcsnicmp(component.data(), node->name.Buffer, component.size());
			if (res < 0) {
				// NOLINTNEXTLINE
				node = children.get_left(node);
			}
			else if (res > 0) {
				// NOLINTNEXTLINE
				node = children.get_right(node);
			}
			else {
				return node;
			}
		}

		return nullptr;
	}

	RegistryValue* lookup_value(kstd::wstring_view component) {
		auto* node = values.get_root();
		while (node) {
			if (component.size() < node->name.Length / 2) {
				// NOLINTNEXTLINE
				node = values.get_left(node);
				continue;
			}
			else if (component.size() > node->name.Length / 2) {
				// NOLINTNEXTLINE
				node = values.get_right(node);
				continue;
			}

			int res = _wcsnicmp(component.data(), node->name.Buffer, component.size());
			if (res < 0) {
				// NOLINTNEXTLINE
				node = values.get_left(node);
			}
			else if (res > 0) {
				// NOLINTNEXTLINE
				node = values.get_right(node);
			}
			else {
				return node;
			}
		}

		return nullptr;
	}

	UNICODE_STRING name {};
	hz::rb_tree_hook hook {};
	hz::rb_tree<RegistryObject, &RegistryObject::hook> children {};
	hz::rb_tree<RegistryValue, &RegistryValue::hook> values {};
	EX_SPIN_LOCK lock {};
	bool is_volatile {};
};

struct RegistryParseCtx {
	ULONG create_options;
	ULONG disposition;
	bool create;
};

namespace {
	POBJECT_TYPE KEY_TYPE;
}

[[clang::no_thread_safety_analysis]]
NTSTATUS registry_parse_proc(
	PVOID parse_object,
	PVOID object_type,
	PACCESS_STATE access_state,
	KPROCESSOR_MODE access_mode,
	ULONG attribs,
	PUNICODE_STRING complete_name,
	PUNICODE_STRING remaining_name,
	PVOID context,
	SECURITY_QUALITY_OF_SERVICE* security_qos,
	PVOID* got_object) {
	auto* start = static_cast<RegistryObject*>(parse_object);

	auto* start_buffer = remaining_name->Buffer;
	usize len = remaining_name->Length / 2;
	while (len && *start_buffer == u'\\') {
		++start_buffer;
		--len;
	}

	while (len && start_buffer[len - 1] == u'\\') {
		--len;
	}

	if (!len) {
		auto status = ObReferenceObjectByPointer(
			start,
			0,
			static_cast<OBJECT_TYPE*>(object_type),
			access_mode);
		if (!NT_SUCCESS(status)) {
			return status;
		}
		*got_object = start;
		return STATUS_SUCCESS;
	}

	kstd::wstring_view remaining {start_buffer, len};

	auto* ctx = static_cast<RegistryParseCtx*>(context);
	bool create = ctx && ctx->create;

	usize offset = 0;
	auto* dir = start;
	PVOID object = nullptr;
	auto status = STATUS_SUCCESS;

	while (true) {
		auto end = remaining.find('\\', offset);
		bool is_last = end == kstd::wstring_view::npos;

		auto component = remaining.substr_abs(offset, end);

		KIRQL old;
		if (!is_last || !create) {
			old = ExAcquireSpinLockShared(&dir->lock);
		}
		else {
			old = ExAcquireSpinLockExclusive(&dir->lock);
		}

		auto* next = dir->lookup_child(component);
		if (next) {
			if (is_last) {
				if (attribs & OBJ_OPENIF) {
					status = ObReferenceObjectByPointer(
						next,
						0,
						static_cast<POBJECT_TYPE>(object_type),
						access_mode);

					if (NT_SUCCESS(status)) {
						object = next;
						status = STATUS_OBJECT_NAME_EXISTS;

						ctx->disposition = REG_OPENED_EXISTING_KEY;
					}
				}
				else {
					status = STATUS_OBJECT_NAME_COLLISION;
				}

				if (!create) {
					ExReleaseSpinLockShared(&dir->lock, old);
				}
				else {
					ExReleaseSpinLockExclusive(&dir->lock, old);
				}

				break;
			}

			ExReleaseSpinLockShared(&dir->lock, old);
			dir = next;
		}
		else {
			if (!is_last) {
				ExReleaseSpinLockShared(&dir->lock, old);
				status = STATUS_OBJECT_PATH_NOT_FOUND;
				break;
			}

			if (create) {
				auto* name = static_cast<WCHAR*>(kmalloc(component.size() * 2 + 2));
				if (!name) {
					ExReleaseSpinLockExclusive(&dir->lock, old);
					status = STATUS_INSUFFICIENT_RESOURCES;
					break;
				}
				memcpy(name, component.data(), component.size() * 2);
				name[component.size()] = 0;

				// create the new object
				RegistryObject* new_object;
				OBJECT_ATTRIBUTES object_attribs {};
				InitializeObjectAttributes(&object_attribs, nullptr, 0, nullptr, nullptr);
				status = ObCreateObject(
					KernelMode,
					KEY_TYPE,
					&object_attribs,
					access_mode,
					nullptr,
					sizeof(RegistryObject),
					0,
					sizeof(RegistryObject),
					reinterpret_cast<PVOID*>(&new_object));
				if (!NT_SUCCESS(status)) {
					ExReleaseSpinLockExclusive(&dir->lock, old);
					kfree(name, component.size() * 2 + 2);
					break;
				}

				status = ObInsertObject(
					new_object,
					nullptr,
					0,
					0,
					nullptr,
					nullptr);
				assert(NT_SUCCESS(status));

				new_object->name.Buffer = name;
				new_object->name.Length = component.size() * 2;
				new_object->name.MaximumLength = component.size() * 2 + 2;
				new_object->is_volatile = ctx->create_options & REG_OPTION_VOLATILE;

				dir->children.insert(new_object);

				ExReleaseSpinLockExclusive(&dir->lock, old);

				ctx->disposition = REG_CREATED_NEW_KEY;

				object = new_object;
				break;
			}
			else {
				ExReleaseSpinLockShared(&dir->lock, old);
				status = STATUS_OBJECT_NAME_NOT_FOUND;
				break;
			}
		}

		offset = end + 1;

		if (end == kstd::wstring_view::npos) {
			break;
		}
	}

	*got_object = object;
	return status;
}

/*
 * HKEY_LOCAL_MACHINE = \Registry\Machine
 * HKEY_USERS = \Registry\User
 * HKEY_CURRENT_USER = \Registry\User\<S-ID user guid>
 * HKEY_CLASSES_ROOT = merge of \Registry\Machine\Software\Classes
 * and \Registry\User\<S-ID>\Software\Classes (done in usermode)
 */

NTAPI NTSTATUS NtCreateKey(
	PHANDLE key_handle,
	ACCESS_MASK desired_access,
	OBJECT_ATTRIBUTES* object_attribs,
	ULONG title_index,
	PUNICODE_STRING clazz,
	ULONG create_options,
	PULONG disposition) {
	RegistryParseCtx ctx {
		.create_options = create_options,
		.disposition = 0,
		.create = true
	};
	OBJECT_ATTRIBUTES copy = *object_attribs;
	copy.attributes |= OBJ_CASE_INSENSITIVE | OBJ_OPENIF;
	auto status = ObOpenObjectByName(
		object_attribs,
		nullptr,
		ExGetPreviousMode(),
		nullptr,
		desired_access,
		&ctx,
		key_handle);

	if (disposition) {
		*disposition = ctx.disposition;
	}

	return status;
}

NTAPI NTSTATUS ZwCreateKey(
	PHANDLE key_handle,
	ACCESS_MASK desired_access,
	OBJECT_ATTRIBUTES* object_attribs,
	ULONG title_index,
	PUNICODE_STRING clazz,
	ULONG create_options,
	PULONG disposition) {
	auto* thread = get_current_thread();
	auto old = thread->previous_mode;
	thread->previous_mode = KernelMode;
	auto status = NtCreateKey(
		key_handle,
		desired_access,
		object_attribs,
		title_index,
		clazz,
		create_options,
		disposition);
	thread->previous_mode = old;
	return status;
}

NTAPI NTSTATUS NtOpenKey(
	PHANDLE key_handle,
	ACCESS_MASK desired_access,
	OBJECT_ATTRIBUTES* object_attribs) {
	return ObOpenObjectByName(
		object_attribs,
		nullptr,
		ExGetPreviousMode(),
		nullptr,
		desired_access,
		nullptr,
		key_handle);
}

NTAPI NTSTATUS ZwOpenKey(
	PHANDLE key_handle,
	ACCESS_MASK desired_access,
	OBJECT_ATTRIBUTES* object_attribs) {
	auto* thread = get_current_thread();
	auto old = thread->previous_mode;
	thread->previous_mode = KernelMode;
	auto status = NtOpenKey(
		key_handle,
		desired_access,
		object_attribs);
	thread->previous_mode = old;
	return status;
}

NTAPI extern "C" NTSTATUS NtEnumerateKey(
	HANDLE key_handle,
	ULONG index,
	KEY_INFORMATION_CLASS key_info_class,
	PVOID key_info,
	ULONG length,
	PULONG result_length) {
	if (!result_length) {
		return STATUS_INVALID_PARAMETER;
	}

	auto mode = ExGetPreviousMode();
	if (mode == UserMode) {
		__try {
			enable_user_access();

			if (length) {
				ProbeForRead(key_info, length, 1);
			}

			ProbeForRead(result_length, sizeof(ULONG), alignof(ULONG));
			disable_user_access();
		}
		__except (1) {
			disable_user_access();
			return GetExceptionCode();
		}
	}

	RegistryObject* object;
	auto status = ObReferenceObjectByHandle(
		key_handle,
		0,
		KEY_TYPE,
		mode,
		reinterpret_cast<PVOID*>(&object),
		nullptr);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	auto old = ExAcquireSpinLockShared(&object->lock);

	usize subkey_i = 0;
	RegistryObject* subkey;
	for (subkey = object->children.get_first();
		subkey;
		subkey = static_cast<RegistryObject*>(subkey->hook.successor)) {
		if (subkey_i == index) {
			break;
		}

		++subkey_i;
	}

	if (!subkey) {
		ExReleaseSpinLockShared(&object->lock, old);
		ObfDereferenceObject(object);
		return STATUS_NO_MORE_ENTRIES;
	}

	status = STATUS_SUCCESS;

	switch (key_info_class) {
		case KeyBasicInformation:
		{
			if (length < sizeof(KEY_BASIC_INFORMATION)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}

			// todo last write time + title index
			auto* ptr = static_cast<KEY_BASIC_INFORMATION*>(key_info);
			__try {
				enable_user_access();

				ptr->last_write_time = {};
				ptr->title_index = 0;
				ptr->name_length = subkey->name.Length;

				if (length < sizeof(*ptr) + subkey->name.Length) {
					memcpy(ptr->name, subkey->name.Buffer, length - sizeof(*ptr));
					status = STATUS_BUFFER_OVERFLOW;
				}
				else {
					memcpy(ptr->name, subkey->name.Buffer, subkey->name.Length);
				}

				*result_length = sizeof(*ptr) + subkey->name.Length;

				disable_user_access();
			}
			__except (1) {
				disable_user_access();
				status = GetExceptionCode();
			}

			break;
		}
		case KeyNodeInformation:
		{
			if (length < sizeof(KEY_NODE_INFORMATION)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}

			// todo last write time + title index + class
			auto* ptr = static_cast<KEY_NODE_INFORMATION*>(key_info);
			__try {
				enable_user_access();

				ptr->last_write_time = {};
				ptr->title_index = 0;
				ptr->class_offset = 0;
				ptr->class_length = 0;
				ptr->name_length = subkey->name.Length;

				if (length < sizeof(*ptr) + subkey->name.Length) {
					memcpy(ptr->name, subkey->name.Buffer, length - sizeof(*ptr));
					status = STATUS_BUFFER_OVERFLOW;
				}
				else {
					memcpy(ptr->name, subkey->name.Buffer, subkey->name.Length);
				}

				*result_length = sizeof(*ptr) + subkey->name.Length;

				disable_user_access();
			}
			__except (1) {
				disable_user_access();
				status = GetExceptionCode();
			}

			break;
		}
		case KeyFullInformation:
		{
			if (length < sizeof(KEY_FULL_INFORMATION)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}

			auto old2 = ExAcquireSpinLockShared(&subkey->lock);

			// todo cache
			ULONG subkey_count = 0;
			ULONG max_name_len = 0;
			for (auto* ptr = subkey->children.get_first();
				ptr;
				ptr = static_cast<RegistryObject*>(ptr->hook.successor)) {
				++subkey_count;
				max_name_len = hz::max<ULONG>(max_name_len, ptr->name.Length);
			}

			ULONG value_count = 0;
			ULONG max_value_name_len = 0;
			ULONG max_value_data_len = 0;
			for (auto* ptr = subkey->values.get_first();
				ptr;
				ptr = static_cast<RegistryValue*>(ptr->hook.successor)) {
				++value_count;
				max_value_name_len = hz::max<ULONG>(max_value_name_len, ptr->name.Length);
				max_value_data_len = hz::max(max_value_data_len, ptr->data_size);
			}

			ExReleaseSpinLockShared(&subkey->lock, old2);

			// todo last write time + title index + class
			auto* ptr = static_cast<KEY_FULL_INFORMATION*>(key_info);
			__try {
				enable_user_access();

				ptr->last_write_time = {};
				ptr->title_index = 0;
				ptr->class_offset = 0;
				ptr->class_length = 0;
				ptr->subkeys = subkey_count;
				ptr->max_name_len = max_name_len;
				ptr->max_class_len = 0;
				ptr->values = value_count;
				ptr->max_value_name_len = max_value_name_len;
				ptr->max_value_data_len = max_value_data_len;

				*result_length = sizeof(*ptr);

				disable_user_access();
			}
			__except (1) {
				disable_user_access();
				status = GetExceptionCode();
			}

			break;
		}
		default:
			status = STATUS_INVALID_PARAMETER;
			break;
	}

	ExReleaseSpinLockShared(&object->lock, old);
	ObfDereferenceObject(object);

	return status;
}

NTAPI extern "C" NTSTATUS ZwEnumerateKey(
	HANDLE key_handle,
	ULONG index,
	KEY_INFORMATION_CLASS key_info_class,
	PVOID key_info,
	ULONG length,
	PULONG result_length) {
	auto* thread = get_current_thread();
	auto old = thread->previous_mode;
	thread->previous_mode = KernelMode;
	auto status = NtEnumerateKey(
		key_handle,
		index,
		key_info_class,
		key_info,
		length,
		result_length);
	thread->previous_mode = old;
	return status;
}

NTAPI NTSTATUS NtSetValueKey(
	HANDLE key_handle,
	PUNICODE_STRING value_name,
	ULONG title_index,
	ULONG type,
	PVOID data,
	ULONG data_size) {
	RegistryObject* object;
	auto status = ObReferenceObjectByHandle(
		key_handle,
		KEY_SET_VALUE,
		KEY_TYPE,
		ExGetPreviousMode(),
		reinterpret_cast<PVOID*>(&object),
		nullptr);
	if (status == STATUS_OBJECT_TYPE_MISMATCH) {
		status = STATUS_INVALID_HANDLE;
	}
	if (!NT_SUCCESS(status)) {
		return status;
	}

	auto old = ExAcquireSpinLockExclusive(&object->lock);

	kstd::wstring_view name {value_name->Buffer, static_cast<size_t>(value_name->Length / 2)};

	auto* value = object->lookup_value(name);
	if (value) {
		if (value->data_size == data_size) {
			value->type = type;
			memcpy(value->data, data, data_size);
		}
		else {
			auto* new_data = kcalloc(data_size);
			if (!new_data) {
				ExReleaseSpinLockExclusive(&object->lock, old);
				return STATUS_INSUFFICIENT_RESOURCES;
			}

			kfree(value->data, value->data_size);
			value->data = new_data;
			value->type = type;
			value->data_size = data_size;
			memcpy(value->data, data, data_size);
		}
	}
	else {
		usize size = sizeof(RegistryValue) + value_name->Length + 2;
		auto* new_value = static_cast<RegistryValue*>(kcalloc(size));
		if (!new_value) {
			ExReleaseSpinLockExclusive(&object->lock, old);
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		auto* data_buffer = kcalloc(data_size);
		if (!data_buffer) {
			ExReleaseSpinLockExclusive(&object->lock, old);
			kfree(new_value, size);
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		new_value->name.Buffer = reinterpret_cast<WCHAR*>(&new_value[1]);
		new_value->name.Length = value_name->Length;
		new_value->name.MaximumLength = value_name->Length + 2;
		memcpy(new_value->name.Buffer, value_name->Buffer, value_name->Length);
		new_value->name.Buffer[value_name->Length / 2] = 0;

		new_value->type = type;
		new_value->data_size = data_size;
		new_value->data = data_buffer;

		memcpy(new_value->data, data, data_size);

		object->values.insert(new_value);
	}

	ExReleaseSpinLockExclusive(&object->lock, old);
	return STATUS_SUCCESS;
}

NTAPI NTSTATUS ZwSetValueKey(
	HANDLE key_handle,
	PUNICODE_STRING value_name,
	ULONG title_index,
	ULONG type,
	PVOID data,
	ULONG data_size) {
	auto* thread = get_current_thread();
	auto old = thread->previous_mode;
	thread->previous_mode = KernelMode;
	auto status = NtSetValueKey(
		key_handle,
		value_name,
		title_index,
		type,
		data,
		data_size);
	thread->previous_mode = old;
	return status;
}

NTAPI NTSTATUS NtDeleteValueKey(
	HANDLE key_handle,
	PUNICODE_STRING value_name) {
	auto mode = ExGetPreviousMode();

	if (mode == UserMode) {
		__try {
			ProbeForRead(value_name, sizeof(UNICODE_STRING), alignof(UNICODE_STRING));
		}
		__except (1) {
			return GetExceptionCode();
		}
	}

	RegistryObject* object;
	auto status = ObReferenceObjectByHandle(
		key_handle,
		0,
		KEY_TYPE,
		mode,
		reinterpret_cast<PVOID*>(&object),
		nullptr);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	auto old = ExAcquireSpinLockExclusive(&object->lock);

	kstd::wstring_view name {value_name->Buffer, static_cast<size_t>(value_name->Length / 2)};
	RegistryValue* value;
	__try {
		enable_user_access();
		value = object->lookup_value(name);
		disable_user_access();
	}
	__except (1) {
		ExReleaseSpinLockExclusive(&object->lock, old);
		ObfDereferenceObject(object);
		return GetExceptionCode();
	}

	if (!value) {
		ExReleaseSpinLockExclusive(&object->lock, old);
		ObfDereferenceObject(object);
		return STATUS_OBJECT_NAME_NOT_FOUND;
	}

	object->values.remove(value);

	ExReleaseSpinLockExclusive(&object->lock, old);
	ObfDereferenceObject(object);

	kfree(value->data, value->data_size);
	kfree(value, sizeof(RegistryValue) + value->name.MaximumLength);

	return STATUS_SUCCESS;
}

NTAPI NTSTATUS ZwDeleteValueKey(
	HANDLE key_handle,
	PUNICODE_STRING value_name) {
	auto* thread = get_current_thread();
	auto old = thread->previous_mode;
	thread->previous_mode = KernelMode;
	auto status = NtDeleteValueKey(
		key_handle,
		value_name);
	thread->previous_mode = old;
	return status;
}

NTAPI NTSTATUS NtQueryValueKey(
	HANDLE key_handle,
	PUNICODE_STRING value_name,
	KEY_VALUE_INFORMATION_CLASS key_value_info_class,
	PVOID key_value_info,
	ULONG length,
	PULONG result_length) {
	if (!result_length) {
		return STATUS_INVALID_PARAMETER;
	}

	auto mode = ExGetPreviousMode();
	UNICODE_STRING name {};
	if (mode == UserMode) {
		__try {
			enable_user_access();
			ProbeForRead(value_name, sizeof(UNICODE_STRING), alignof(UNICODE_STRING));
			name = *value_name;
			ProbeForRead(name.Buffer, name.Length, alignof(WCHAR));

			if (length) {
				ProbeForRead(key_value_info, length, 1);
			}

			ProbeForRead(result_length, sizeof(ULONG), alignof(ULONG));
			disable_user_access();
		}
		__except (1) {
			disable_user_access();
			return GetExceptionCode();
		}
	}

	RegistryObject* object;
	auto status = ObReferenceObjectByHandle(
		key_handle,
		0,
		KEY_TYPE,
		mode,
		reinterpret_cast<PVOID*>(&object),
		nullptr);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	auto old = ExAcquireSpinLockShared(&object->lock);
	kstd::wstring_view component {name.Buffer, static_cast<usize>(name.Length / 2)};
	RegistryValue* value;
	__try {
		enable_user_access();
		value = object->lookup_value(component);
		disable_user_access();
	}
	__except (1) {
		disable_user_access();
		ExReleaseSpinLockShared(&object->lock, old);
		ObfDereferenceObject(object);
		return GetExceptionCode();
	}

	if (!value) {
		ExReleaseSpinLockShared(&object->lock, old);
		ObfDereferenceObject(object);
		return STATUS_OBJECT_NAME_NOT_FOUND;
	}

	status = STATUS_SUCCESS;

	switch (key_value_info_class) {
		case KeyValueBasicInformation:
		{
			if (length < sizeof(KEY_VALUE_BASIC_INFORMATION)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}

			// todo title index
			auto* ptr = static_cast<KEY_VALUE_BASIC_INFORMATION*>(key_value_info);
			__try {
				enable_user_access();

				ptr->title_index = 0;
				ptr->type = value->type;
				ptr->name_length = value->name.Length + 2;

				if (length < sizeof(*ptr) + value->name.Length + 2) {
					memcpy(ptr->name, value->name.Buffer, length - sizeof(*ptr));
					status = STATUS_BUFFER_OVERFLOW;
				}
				else {
					memcpy(ptr->name, value->name.Buffer, value->name.Length + 2);
				}

				*result_length = sizeof(*ptr) + value->name.Length + 2;

				disable_user_access();
			}
			__except (1) {
				disable_user_access();
				status = GetExceptionCode();
			}

			break;
		}
		case KeyValueFullInformationAlign64:
			if (length && reinterpret_cast<usize>(key_value_info) % 8 != 0) {
				status = STATUS_DATATYPE_MISALIGNMENT;
				break;
			}
		case KeyValueFullInformation:
		{
			if (length < sizeof(KEY_VALUE_FULL_INFORMATION)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}

			// todo title index
			auto* ptr = static_cast<KEY_VALUE_FULL_INFORMATION*>(key_value_info);
			__try {
				enable_user_access();

				ptr->title_index = 0;
				ptr->type = value->type;
				ptr->data_offset = sizeof(KEY_VALUE_FULL_INFORMATION) + value->name.Length + 2;
				ptr->data_length = value->data_size;
				ptr->name_length = value->name.Length + 2;

				if (length < sizeof(*ptr) + value->name.Length + 2) {
					memcpy(ptr->name, value->name.Buffer, length - sizeof(*ptr));
					status = STATUS_BUFFER_OVERFLOW;
				}
				else {
					memcpy(ptr->name, value->name.Buffer, value->name.Length + 2);

					if (length < sizeof(*ptr) + value->name.Length + 2 + value->data_size) {
						memcpy(
							reinterpret_cast<char*>(&ptr[1]) + value->name.Length + 2,
							value->data,
							length - sizeof(*ptr) - value->name.Length - 2);
						status = STATUS_BUFFER_OVERFLOW;
					}
					else {
						memcpy(
							reinterpret_cast<char*>(&ptr[1]) + value->name.Length + 2,
							value->data,
							value->data_size);
					}
				}

				*result_length = sizeof(*ptr) + value->name.Length + 2 + value->data_size;

				disable_user_access();
			}
			__except (1) {
				disable_user_access();
				status = GetExceptionCode();
			}

			break;
		}
		case KeyValuePartialInformationAlign64:
		{
			if (length && reinterpret_cast<usize>(key_value_info) % 8 != 0) {
				status = STATUS_DATATYPE_MISALIGNMENT;
				break;
			}
			else if (length < sizeof(KEY_VALUE_PARTIAL_INFORMATION_ALIGN64)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}

			auto* ptr = static_cast<KEY_VALUE_PARTIAL_INFORMATION_ALIGN64*>(key_value_info);
			__try {
				enable_user_access();

				ptr->type = value->type;
				ptr->data_length = value->data_size;

				if (length < sizeof(*ptr) + value->data_size) {
					memcpy(ptr->data, value->data, length - sizeof(*ptr));
					status = STATUS_BUFFER_OVERFLOW;
				}
				else {
					memcpy(ptr->data, value->data, value->data_size);
				}

				*result_length = sizeof(*ptr) + value->data_size;

				disable_user_access();
			}
			__except (1) {
				disable_user_access();
				status = GetExceptionCode();
			}

			break;
		}
		case KeyValuePartialInformation:
		{
			if (length < sizeof(KEY_VALUE_PARTIAL_INFORMATION)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}

			auto* ptr = static_cast<KEY_VALUE_PARTIAL_INFORMATION*>(key_value_info);
			__try {
				enable_user_access();

				ptr->title_index = 0;
				ptr->type = value->type;
				ptr->data_length = value->data_size;

				if (length < sizeof(*ptr) + value->data_size) {
					memcpy(ptr->data, value->data, length - sizeof(*ptr));
					status = STATUS_BUFFER_OVERFLOW;
				}
				else {
					memcpy(ptr->data, value->data, value->data_size);
				}

				*result_length = sizeof(*ptr) + value->data_size;

				disable_user_access();
			}
			__except (1) {
				disable_user_access();
				status = GetExceptionCode();
			}

			break;
		}
		case KeyValueLayerInformation:
		{
			if (length < sizeof(KEY_VALUE_LAYER_INFORMATION)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}

			auto* ptr = static_cast<KEY_VALUE_LAYER_INFORMATION*>(key_value_info);
			__try {
				*ptr = {
					.is_tombstone = false,
					.reserved = 0
				};
			}
			__except (1) {
				disable_user_access();
				status = GetExceptionCode();
			}

			break;
		}
		default:
			status = STATUS_INVALID_PARAMETER;
			break;
	}

	ExReleaseSpinLockShared(&object->lock, old);
	ObfDereferenceObject(object);

	return status;
}

NTAPI NTSTATUS ZwQueryValueKey(
	HANDLE key_handle,
	PUNICODE_STRING value_name,
	KEY_VALUE_INFORMATION_CLASS key_value_info_class,
	PVOID key_value_info,
	ULONG length,
	PULONG result_length) {
	auto* thread = get_current_thread();
	auto old = thread->previous_mode;
	thread->previous_mode = KernelMode;
	auto status = NtQueryValueKey(
		key_handle,
		value_name,
		key_value_info_class,
		key_value_info,
		length,
		result_length);
	thread->previous_mode = old;
	return status;
}

void registry_init() {
	UNICODE_STRING key_name = RTL_CONSTANT_STRING(u"Key");
	OBJECT_TYPE_INITIALIZER key_init {};
	key_init.length = sizeof(OBJECT_TYPE_INITIALIZER);
	key_init.case_insensitive = true;
	key_init.parse_proc = registry_parse_proc;
	auto status = ObCreateObjectType(
		&key_name,
		&key_init,
		nullptr,
		&KEY_TYPE);
	assert(status == STATUS_SUCCESS);

	UNICODE_STRING root_path = RTL_CONSTANT_STRING(u"\\Registry");
	OBJECT_ATTRIBUTES attribs {};
	InitializeObjectAttributes(&attribs, &root_path, 0, nullptr, nullptr);
	PVOID root;
	status = ObCreateObject(
		KernelMode,
		KEY_TYPE,
		&attribs,
		KernelMode,
		nullptr,
		sizeof(RegistryObject),
		0,
		sizeof(RegistryObject),
		&root);
	assert(NT_SUCCESS(status));
	status = ObInsertObject(
		root,
		nullptr,
		0,
		0,
		nullptr,
		nullptr);
	assert(NT_SUCCESS(status));

	UNICODE_STRING machine_path = RTL_CONSTANT_STRING(u"\\Registry\\Machine");
	PVOID object;
	RegistryParseCtx ctx {
		.create_options = 0,
		.disposition = 0,
		.create = true
	};
	status = ObReferenceObjectByName(
		&machine_path,
		OBJ_CASE_INSENSITIVE,
		nullptr,
		0,
		nullptr,
		KernelMode,
		&ctx,
		&object);
	assert(NT_SUCCESS(status));

	UNICODE_STRING system_path = RTL_CONSTANT_STRING(u"\\Registry\\Machine\\System");
	status = ObReferenceObjectByName(
		&system_path,
		OBJ_CASE_INSENSITIVE,
		nullptr,
		0,
		nullptr,
		KernelMode,
		&ctx,
		&object);
	assert(NT_SUCCESS(status));

	UNICODE_STRING current_control_set_path = RTL_CONSTANT_STRING(u"\\Registry\\Machine\\System\\CurrentControlSet");
	status = ObReferenceObjectByName(
		&current_control_set_path,
		OBJ_CASE_INSENSITIVE,
		nullptr,
		0,
		nullptr,
		KernelMode,
		&ctx,
		&object);
	assert(NT_SUCCESS(status));

	UNICODE_STRING services_path = RTL_CONSTANT_STRING(u"\\Registry\\Machine\\System\\CurrentControlSet\\Services");
	status = ObReferenceObjectByName(
		&services_path,
		OBJ_CASE_INSENSITIVE,
		nullptr,
		0,
		nullptr,
		KernelMode,
		&ctx,
		&object);
	assert(NT_SUCCESS(status));

	UNICODE_STRING control_path = RTL_CONSTANT_STRING(u"\\Registry\\Machine\\System\\CurrentControlSet\\Control");
	status = ObReferenceObjectByName(
		&control_path,
		OBJ_CASE_INSENSITIVE,
		nullptr,
		0,
		nullptr,
		KernelMode,
		&ctx,
		&object);
	assert(NT_SUCCESS(status));

	UNICODE_STRING users_path = RTL_CONSTANT_STRING(u"\\Registry\\User");
	status = ObReferenceObjectByName(
		&users_path,
		OBJ_CASE_INSENSITIVE,
		nullptr,
		0,
		nullptr,
		KernelMode,
		&ctx,
		&object);
	assert(NT_SUCCESS(status));

	UNICODE_STRING default_user_path = RTL_CONSTANT_STRING(u"\\Registry\\User\\.Default");
	status = ObReferenceObjectByName(
		&default_user_path,
		OBJ_CASE_INSENSITIVE,
		nullptr,
		0,
		nullptr,
		KernelMode,
		&ctx,
		&object);
	assert(NT_SUCCESS(status));
}
