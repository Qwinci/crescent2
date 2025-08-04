#include "object_internals.hpp"
#include "mem/malloc.hpp"
#include "cstring.hpp"
#include "rtl.hpp"
#include "wchar.hpp"
#include "sched/handle_table.hpp"
#include "sched/process.hpp"
#include "arch/arch_sched.hpp"
#include "sched/thread.hpp"
#include "sys/misc.hpp"
#include <hz/new.hpp>
#include <hz/rb_tree.hpp>

struct ObjectDirectory {
	struct Object {
		hz::rb_tree_hook hook {};
		OBJECT_HEADER* hdr {};

		[[nodiscard]] constexpr int operator<=>(const Object& other) const {
			auto* name = object_header_name_info(hdr);
			auto* other_name = object_header_name_info(other.hdr);

			if (name->name.Length < other_name->name.Length) {
				return -1;
			}
			else if (name->name.Length > other_name->name.Length) {
				return 1;
			}
			else {
				return wcscmp(name->name.Buffer, other_name->name.Buffer);
			}
		}

		constexpr bool operator==(const Object&) const {
			// not used
			assert(false);
			return true;
		}
	};

	hz::rb_tree<Object, &Object::hook> children {};
	EX_SPIN_LOCK lock {};
};

namespace {
	OBJECT_TYPE OBJECT_DIR_TYPE {
		.type_list {},
		.name = RTL_CONSTANT_STRING(u"Object Directory"),
		.default_object = nullptr,
		.index = 0,
		.total_number_of_objects = 0,
		.total_number_of_handles = 0,
		.high_number_of_objects = 0,
		.high_number_of_handles = 0,
		.type_info {
			.length = sizeof(OBJECT_TYPE_INITIALIZER),
			.case_insensitive = true,
			.object_type_code = 0,
			.invalid_attribs = 0,
			.generic_mapping {},
			.valid_access_mask = 0,
			.retain_access = 0,
			.pool_type = NonPagedPoolNx,
			.default_paged_pool_charge = 0,
			.default_non_paged_pool_charge = 0,
			.dump_proc = nullptr,
			.open_proc = nullptr,
			.close_proc = nullptr,
			.delete_proc = nullptr,
			.parse_proc = nullptr,
			.security_proc = nullptr,
			.query_name_proc = nullptr,
			.okay_to_close_proc = nullptr,
			.wait_object_flag_mask = 0,
			.wait_object_flag_offset = 0,
			.wait_object_pointer_offset = 0
		}
	};

	struct RootObject {
		OBJECT_HEADER_NAME_INFO name_info;
		OBJECT_HEADER hdr;
		ObjectDirectory dir {};
	};

	RootObject ROOT_OBJECT {
		.name_info {
			.dir = nullptr,
			.name = RTL_CONSTANT_STRING(u"\\"),
			.reference_count = 0,
			.reserved = 0
		},
		.hdr {
			.pointer_count {1},
			.handle_count {0},
			.lock {},
			.type_index = 0,
			.trace_flags = 0,
			.info_mask = 1 << 1,
			.permanent_object = true,
			.reserved = 0,
			.object_create_info = nullptr,
			.security_descriptor = nullptr
		}
	};

	OBJECT_TYPE* OBJECT_TYPE_TABLE[0x100] {&OBJECT_DIR_TYPE};
	KSPIN_LOCK OBJECT_TYPE_TABLE_LOCK {};
	UCHAR OBJECT_TYPE_TABLE_INDEX = 1;
}

OBJECT_TYPE_INITIALIZER* object_get_type(PVOID object) {
	auto* hdr = reinterpret_cast<OBJECT_HEADER*>(object) - 1;
	auto* type = OBJECT_TYPE_TABLE[hdr->type_index];
	return &type->type_info;
}

OBJECT_TYPE* object_get_full_type(PVOID object) {
	auto* hdr = reinterpret_cast<OBJECT_HEADER*>(object) - 1;
	return OBJECT_TYPE_TABLE[hdr->type_index];
}

NTAPI NTSTATUS ObCreateObjectType(
	PUNICODE_STRING type_name,
	OBJECT_TYPE_INITIALIZER* object_type_initializer,
	SECURITY_DESCRIPTOR* security_descriptor,
	POBJECT_TYPE* object_type) {
	auto* ptr = static_cast<OBJECT_TYPE*>(kmalloc(sizeof(OBJECT_TYPE)));
	if (!ptr) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	UNICODE_STRING name {
		.Length = type_name->Length,
		.MaximumLength = static_cast<USHORT>(type_name->Length + 2),
		.Buffer = static_cast<PWSTR>(kmalloc(type_name->Length + 2))
	};
	if (!name.Buffer) {
		kfree(ptr, sizeof(OBJECT_TYPE));
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	memcpy(name.Buffer, type_name->Buffer, type_name->Length);
	name.Buffer[type_name->Length / 2] = 0;

	auto old = KeAcquireSpinLockRaiseToDpc(&OBJECT_TYPE_TABLE_LOCK);
	UCHAR index = OBJECT_TYPE_TABLE_INDEX++;
	KeReleaseSpinLock(&OBJECT_TYPE_TABLE_LOCK, old);

	OBJECT_TYPE_TABLE[index] = new (ptr) OBJECT_TYPE {
		.type_list {.Flink = &ptr->type_list, .Blink = &ptr->type_list},
		.name = name,
		.default_object = nullptr,
		.index = index,
		.total_number_of_objects = 0,
		.total_number_of_handles = 0,
		.high_number_of_objects = 0,
		.high_number_of_handles = 0,
		.type_info = *object_type_initializer
	};

	*object_type = ptr;
	return STATUS_SUCCESS;
}

static OBJECT_ATTRIBUTES DEFAULT_ATTRIBS {};

NTAPI NTSTATUS ObCreateObject(
	KPROCESSOR_MODE probe_mode,
	POBJECT_TYPE object_type,
	OBJECT_ATTRIBUTES* object_attribs,
	KPROCESSOR_MODE ownership_mode,
	PVOID parse_context,
	ULONG object_body_size,
	ULONG paged_pool_charge,
	ULONG non_paged_pool_charge,
	PVOID* object) {
	auto* info = static_cast<OBJECT_CREATE_INFORMATION*>(kmalloc(sizeof(OBJECT_CREATE_INFORMATION)));
	if (!info) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	if (!object_attribs) {
		object_attribs = &DEFAULT_ATTRIBS;
	}

	UCHAR info_mask = 0;
	ULONG headers_size = sizeof(OBJECT_HEADER);
	if (object_type->type_info.maintain_type_list) {
		info_mask |= 1 << 0;
		headers_size += sizeof(OBJECT_HEADER_CREATOR_INFO);
	}
	if (object_attribs->object_name && object_attribs->object_name->Length) {
		info_mask |= 1 << 1;
		headers_size += sizeof(OBJECT_HEADER_NAME_INFO);
	}
	if (object_type->type_info.maintain_handle_count) {
		info_mask |= 1 << 2;
		headers_size += sizeof(OBJECT_HEADER_HANDLE_INFO);
	}

	// todo quota if object not created by the initial or idle system process

	bool exclusive = object_attribs->attributes & OBJ_EXCLUSIVE;
	if (exclusive) {
		info_mask |= 1 << 4;
		headers_size += sizeof(OBJECT_HEADER_PROCESS_INFO);
	}

	// todo audit if file system object + auditing enabled
	// 1 << 5

	// todo extended info if footer needed (handle revocation info, used by file and key objects)
	// or extended user context info (silo context object)
	// 1 << 6

	ULONG padding = 0;

	if (object_type->type_info.cache_aligned) {
		info_mask |= 1 << 7;
		headers_size += sizeof(OBJECT_HEADER_PADDING_INFO);
		ULONG misalign = headers_size & 63;
		if (misalign) {
			padding = 64 - misalign;
			headers_size += padding;
		}
	}

	ULONG total_size = headers_size + object_body_size;

	auto* start = static_cast<char*>(kcalloc(total_size));
	if (!start) {
		kfree(info, sizeof(OBJECT_CREATE_INFORMATION));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	auto* hdr = reinterpret_cast<OBJECT_HEADER*>(start + headers_size - sizeof(OBJECT_HEADER));

	//bool kernel_object = object_attribs->attributes & OBJ_KERNEL_HANDLE;
	bool kernel_only = object_attribs->attributes & OBJ_KERNEL_EXCLUSIVE;
	bool permanent = object_attribs->attributes & OBJ_PERMANENT;
	// default security quota = object's security descriptor uses default 2kb quota
	// single handle entry = handle info header only contains single entry
	// deleted inline = object not being deleted through deferred deletion worker thread
	// but rather through an inline call to ObfDereferenceObject(Ex)

	// don't allow creating kernel handles from usermode
	bool kernel_object;
	if (ownership_mode == UserMode) {
		kernel_object = false;
	}
	else {
		kernel_object = true;
	}

	new (hdr) OBJECT_HEADER {
		.pointer_count {1},
		.handle_count {},
		.lock {},
		.type_index = object_type->index,
		.trace_flags = 0,
		.info_mask = info_mask,
		.new_object = true,
		.kernel_object = kernel_object,
		.kernel_only_access = kernel_only,
		.exclusive_object = exclusive,
		.permanent_object = permanent,
		.reserved = total_size,
		.object_create_info = info,
		.security_descriptor = object_attribs->security_descriptor
	};

	if (auto* pad_header = object_header_padding_info(hdr)) {
		pad_header->padding_amount = padding;
	}

	new (info) OBJECT_CREATE_INFORMATION {
		.attribs = object_attribs->attributes,
		.root_directory = object_attribs->root_directory,
		.probe_mode = probe_mode,
		.paged_pool_charge = paged_pool_charge,
		.non_paged_pool_charge = non_paged_pool_charge,
		.security_descriptor_charge = 0,
		.security_descriptor = object_attribs->security_descriptor,
		.parse_ctx = parse_context
	};

	if (auto* object_name = object_attribs->object_name) {
		auto* name = object_header_name_info(hdr);
		name->name.Buffer = static_cast<WCHAR*>(kmalloc(object_name->Length + 2));
		if (!name->name.Buffer) {
			kfree(start, total_size);
			kfree(info, sizeof(OBJECT_CREATE_INFORMATION));
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		memcpy(name->name.Buffer, object_name->Buffer, object_name->Length);
		name->name.Buffer[object_name->Length / 2] = 0;
		name->name.Length = object_name->Length;
		name->name.MaximumLength = object_name->Length + 2;
	}

	*object = &hdr[1];

	return STATUS_SUCCESS;
}

static bool is_kernel_handle(HANDLE handle) {
	return reinterpret_cast<usize>(handle) & 1ULL << 63;
}

static HANDLE to_kernel_handle(HANDLE handle) {
	return reinterpret_cast<HANDLE>(reinterpret_cast<usize>(handle) | 1 << 31);
}

static HANDLE from_kernel_handle(HANDLE handle) {
	assert(is_kernel_handle(handle));
	return reinterpret_cast<HANDLE>(reinterpret_cast<usize>(handle) & ~(1 << 31));
}

static UNICODE_STRING str_to_rtl(kstd::wstring_view str) {
	return {
		.Length = static_cast<USHORT>(str.size() * 2),
		.MaximumLength = static_cast<USHORT>(str.size() * 2),
		.Buffer = const_cast<PWSTR>(str.data())
	};
}

// OBJ_INHERIT saved in handle table entry
// determines if the handle is inherited in child processes and whether it can be copied using DuplicateHandle

// OBJ_CASE_INSENSITIVE used when matching against existing objects
// OBJ_OPENIF used when matching against existing objects, if exists then open existing instead of failing
// OBJ_OPENLINK open handle to link object instead of its target
// OBJ_FORCE_ACCESS_CHECK always perform full access checks even if opened in kernel mode
// OBJ_KERNEL_EXCLUSIVE disable usermode from opening handles, used for \Device\PhysicalMemory + \Win32kSessionGlobals
// section objects
// OBJ_IGNORE_IMPERSONATED_DEVICEMAP when token is being impersonated don't use the dos device map
// of the source user but instead the impersonating process's
// OBJ_DONT_REPARSE disable any reparsing (symbolic links, ntfs reparse points, registry key redirection)
// and return STATUS_REPARSE_POINT_ENCOUNTERED

static ObjectDirectory::Object* find_inside_object_dir(
	ObjectDirectory* dir,
	kstd::wstring_view component,
	bool case_insensitive) {

	auto* node = dir->children.get_root();
	while (node) {
		auto* hdr = node->hdr;
		auto* node_name_info = object_header_name_info(hdr);
		auto node_name = node_name_info->name;

		if (component.size() < node_name.Length / 2) {
			// NOLINTNEXTLINE
			node = dir->children.get_left(node);
			continue;
		}
		else if (component.size() > node_name.Length / 2) {
			// NOLINTNEXTLINE
			node = dir->children.get_right(node);
			continue;
		}

		int res;
		if (case_insensitive) {
			res = _wcsnicmp(component.data(), node_name.Buffer, component.size());
		}
		else {
			res = wcsncmp(component.data(), node_name.Buffer, component.size());
		}

		if (res < 0) {
			// NOLINTNEXTLINE
			node = dir->children.get_left(node);
			continue;
		}
		else if (res > 0) {
			// NOLINTNEXTLINE
			node = dir->children.get_right(node);
			continue;
		}

		return node;
	}

	return nullptr;
}

NO_THREAD_SAFETY_ANALYSIS
static NTSTATUS lookup_object(
	HANDLE root_directory_handle,
	PUNICODE_STRING object_name,
	ULONG attribs,
	PVOID insert_object,
	ACCESS_MASK desired_access,
	KPROCESSOR_MODE access_mode,
	POBJECT_TYPE object_type,
	PVOID parse_ctx,
	PVOID* got_object) {
	bool case_insensitive = !object_type ||
		object_type->type_info.case_insensitive ||
		(attribs & OBJ_CASE_INSENSITIVE);

	auto status = STATUS_SUCCESS;
	ObjectDirectory* root_directory = nullptr;

	auto complete_name = *object_name;

	kstd::wstring_view name {object_name->Buffer, static_cast<size_t>(object_name->Length / 2)};

	bool root_referenced = false;
	if (root_directory_handle) {
		status = ObReferenceObjectByHandle(
			root_directory_handle,
			0,
			nullptr,
			access_mode,
			reinterpret_cast<PVOID*>(&root_directory),
			nullptr);
		if (!NT_SUCCESS(status)) {
			return status;
		}

		auto* root_hdr = get_header(root_directory);

		// todo reparsing
		assert(root_hdr->type_index == OBJECT_DIR_TYPE.index);
		root_referenced = true;
	}
	else {
		if (!name.size() || !name.data() || name[0] != '\\') {
			return STATUS_OBJECT_PATH_SYNTAX_BAD;
		}

		name = name.substr(1);

		root_directory = &ROOT_OBJECT.dir;
	}

	if (!name.size()) {
		if (attribs & OBJ_OPENIF) {
			*got_object = root_directory;
			return STATUS_OBJECT_NAME_EXISTS;
		}

		if (root_referenced) {
			ObfDereferenceObject(root_directory);
		}

		return STATUS_OBJECT_NAME_COLLISION;
	}

	usize offset = 0;
	PVOID object = nullptr;
	ObjectDirectory* dir = root_directory;
	while (true) {
		auto end = name.find_first_of(u"/\\", offset);
		auto slice = name.substr_abs(offset, end);

		bool is_last = offset == kstd::wstring_view::npos ||
			name.find_first_not_of(u"/\\", end) == kstd::wstring_view::npos;
		KIRQL old;
		if (is_last && insert_object) {
			old = ExAcquireSpinLockExclusive(&dir->lock);
		}
		else {
			old = ExAcquireSpinLockShared(&dir->lock);
		}

		auto* next = find_inside_object_dir(dir, slice, case_insensitive);
		if (next) {
			if (is_last && !(attribs & OBJ_OPENIF)) {
				status = STATUS_OBJECT_NAME_COLLISION;

				if (insert_object) {
					ExReleaseSpinLockExclusive(&dir->lock, old);
				} else {
					ExReleaseSpinLockShared(&dir->lock, old);
				}

				break;
			}

			if (next->hdr->type_index != OBJECT_DIR_TYPE.index) {
				auto* next_type = OBJECT_TYPE_TABLE[next->hdr->type_index];

				if (!is_last) {
					if (!next_type->type_info.parse_proc) {
						status = STATUS_OBJECT_PATH_INVALID;
						ExReleaseSpinLockShared(&dir->lock, old);
						break;
					}
				}

				if (attribs & OBJ_DONT_REPARSE) {
					status = STATUS_REPARSE_POINT_ENCOUNTERED;

					if (insert_object) {
						ExReleaseSpinLockExclusive(&dir->lock, old);
					}
					else {
						ExReleaseSpinLockShared(&dir->lock, old);
					}

					break;
				}

				if (next_type->type_info.parse_proc) {
					UNICODE_STRING remaining_name = str_to_rtl(name.substr(end));

					while (true) {
						status = next_type->type_info.parse_proc(
							&next->hdr[1],
							object_type,
							nullptr,
							access_mode,
							attribs,
							&complete_name,
							&remaining_name,
							parse_ctx,
							nullptr,
							&object);
						if (status == STATUS_REPARSE || status == STATUS_REPARSE_OBJECT) {
							continue;
						}
						else if (!NT_SUCCESS(status)) {
							object = nullptr;
						}
						break;
					}

					if (insert_object) {
						ExReleaseSpinLockExclusive(&dir->lock, old);
					}
					else {
						ExReleaseSpinLockShared(&dir->lock, old);
					}

					break;
				}
			}

			if (is_last) {
				object = &next->hdr[1];
				status = ObReferenceObjectByPointer(
					object,
					desired_access,
					object_type,
					access_mode);
				if (NT_SUCCESS(status)) {
					status = STATUS_OBJECT_NAME_EXISTS;
				}

				if (insert_object) {
					ExReleaseSpinLockExclusive(&dir->lock, old);
				}
				else {
					ExReleaseSpinLockShared(&dir->lock, old);
				}

				break;
			}

			ExReleaseSpinLockShared(&dir->lock, old);
			auto* hdr = next->hdr;
			dir = reinterpret_cast<ObjectDirectory*>(&hdr[1]);
		}
		else {
			if (!is_last) {
				ExReleaseSpinLockShared(&dir->lock, old);
				status = STATUS_OBJECT_PATH_NOT_FOUND;
				break;
			}

			if (insert_object) {
				// insert the new object
				auto* new_object = static_cast<ObjectDirectory::Object*>(kmalloc(sizeof(ObjectDirectory::Object)));
				if (!new_object) {
					ExReleaseSpinLockExclusive(&dir->lock, old);
					status = STATUS_INSUFFICIENT_RESOURCES;
					break;
				}

				// modify the name to only the last component
				auto* new_name = static_cast<WCHAR*>(kmalloc(slice.size() * 2 + 2));
				if (!new_name) {
					ExReleaseSpinLockExclusive(&dir->lock, old);
					status = STATUS_INSUFFICIENT_RESOURCES;
					break;
				}
				memcpy(new_name, slice.data(), slice.size() * 2);
				new_name[slice.size()] = 0;

				auto* insert_hdr = get_header(insert_object);

				auto* name_info = object_header_name_info(insert_hdr);

				kfree(name_info->name.Buffer, name_info->name.MaximumLength);
				name_info->name.Buffer = new_name;
				name_info->name.Length = slice.size() * 2;
				name_info->name.MaximumLength = slice.size() * 2 + 2;

				new_object->hdr = insert_hdr;
				new_object->hook = {};

				dir->children.insert(new_object);

				ExReleaseSpinLockExclusive(&dir->lock, old);

				object = insert_object;
				break;
			}
			else {
				ExReleaseSpinLockShared(&dir->lock, old);
				status = STATUS_OBJECT_NAME_NOT_FOUND;
				break;
			}
		}

		offset = end + 1;
	}

	if (root_referenced) {
		ObfDereferenceObject(root_directory);
	}

	*got_object = object;
	return status;
}

NTAPI NTSTATUS ObInsertObject(
	PVOID object,
	PACCESS_STATE passed_access_state,
	ACCESS_MASK desired_access,
	ULONG object_pointer_bias,
	PVOID* new_object,
	PHANDLE handle) {
	auto* hdr = get_header(object);

	if (!hdr->new_object) {
		return STATUS_INVALID_PARAMETER;
	}

	auto* type = OBJECT_TYPE_TABLE[hdr->type_index];
	auto* info = hdr->object_create_info;

	auto* name_info = object_header_name_info(hdr);

	if (name_info) {
		auto* name = &name_info->name;

		PVOID got_object;
		auto status = lookup_object(
			info->root_directory,
			name,
			info->attribs,
			object,
			desired_access,
			hdr->kernel_object ? KernelMode : UserMode,
			type,
			info->parse_ctx,
			&got_object);
		if (!NT_SUCCESS(status)) {
			kfree(hdr->object_create_info, sizeof(OBJECT_CREATE_INFORMATION));
			kfree(name->Buffer, name->MaximumLength);
			ObfDereferenceObject(object);
			return status;
		}
	}

	if (handle) {
		if (hdr->kernel_object) {
			auto h = KERNEL_PROCESS->handle_table.insert(object, info->attribs & OBJ_INHERIT);

			if (h == INVALID_HANDLE_VALUE) {
				kfree(hdr->object_create_info, sizeof(OBJECT_CREATE_INFORMATION));
				hdr->object_create_info = nullptr;
				ObfDereferenceObject(object);
				return STATUS_INSUFFICIENT_RESOURCES;
			}

			*handle = to_kernel_handle(h);
		}
		else {
			auto h = get_current_thread()->process->handle_table.insert(object, info->attribs & OBJ_INHERIT);

			if (h == INVALID_HANDLE_VALUE) {
				kfree(hdr->object_create_info, sizeof(OBJECT_CREATE_INFORMATION));
				hdr->object_create_info = nullptr;
				ObfDereferenceObject(object);
				return STATUS_INSUFFICIENT_RESOURCES;
			}

			*handle = h;
		}
	}

	kfree(hdr->object_create_info, sizeof(OBJECT_CREATE_INFORMATION));
	hdr->new_object = false;
	hdr->object_create_info = nullptr;

	return STATUS_SUCCESS;
}

NTAPI NTSTATUS NtCreateDirectoryObject(
	PHANDLE directory_handle,
	ACCESS_MASK desired_access,
	OBJECT_ATTRIBUTES* object_attribs) {
	PVOID object;
	auto mode = ExGetPreviousMode();
	auto status = ObCreateObject(
		ExGetPreviousMode(),
		&OBJECT_DIR_TYPE,
		object_attribs,
		mode,
		nullptr,
		sizeof(ObjectDirectory),
		0,
		sizeof(ObjectDirectory),
		&object);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	return ObInsertObject(
		object,
		nullptr,
		desired_access,
		0,
		nullptr,
		directory_handle);
}

NTAPI NTSTATUS ZwCreateDirectoryObject(
	PHANDLE directory_handle,
	ACCESS_MASK desired_access,
	OBJECT_ATTRIBUTES* object_attribs) {
	auto* thread = get_current_thread();
	auto prev = thread->previous_mode;
	thread->previous_mode = KernelMode;
	auto status = NtCreateDirectoryObject(directory_handle, desired_access, object_attribs);
	thread->previous_mode = prev;
	return status;
}

NTAPI void ObfReferenceObject(PVOID object) {
	auto* hdr = get_header(object);
	hdr->pointer_count.fetch_add(1, hz::memory_order::relaxed);
}

NTAPI void ObfDereferenceObject(PVOID object) {
	if (!object) {
		return;
	}

	auto* hdr = get_header(object);

	if (hdr->permanent_object) {
		auto old = hdr->pointer_count.load(hz::memory_order::relaxed);
		if (old == 1) {
			return;
		}
	}

	if (hdr->pointer_count.sub_fetch(1, hz::memory_order::acq_rel) == 0) {
		// todo maybe defer
		auto* type = OBJECT_TYPE_TABLE[hdr->type_index];
		if (type->type_info.delete_proc) {
			type->type_info.delete_proc(object);
		}

		ULONG headers_size = OBJECT_INFO_MASK_TO_OFFSET[hdr->info_mask];

		// todo extended info if footer needed (handle revocation info, used by file and key objects)
		// or extended user context info (silo context object)
		// 1 << 6

		if (auto* padding_hdr = object_header_padding_info(hdr)) {
			headers_size += padding_hdr->padding_amount;
		}

		auto* start = reinterpret_cast<char*>(hdr) - headers_size;
		kfree(start, hdr->reserved);
	}
}

namespace {
	KSPIN_LOCK DELETE_LIST_LOCK {};
	PVOID DEFERRED_DELETE_LIST GUARDED_BY(DELETE_LIST_LOCK) = nullptr;
}

void object_do_deferred_deletes() {
	auto old = KeAcquireSpinLockRaiseToDpc(&DELETE_LIST_LOCK);
	PVOID object = DEFERRED_DELETE_LIST;
	DEFERRED_DELETE_LIST = nullptr;
	KeReleaseSpinLock(&DELETE_LIST_LOCK, old);

	while (object) {
		auto* hdr = get_header(object);
		auto* next = hdr->next_to_free;

		ObfDereferenceObject(object);

		object = next;
	}
}

NTAPI void ObDereferenceObjectDeferDelete(PVOID object) {
	auto old = KeAcquireSpinLockRaiseToDpc(&DELETE_LIST_LOCK);

	auto* hdr = get_header(object);
	hdr->next_to_free = DEFERRED_DELETE_LIST;
	DEFERRED_DELETE_LIST = object;

	KeReleaseSpinLock(&DELETE_LIST_LOCK, old);
}

NTAPI NTSTATUS ob_close_handle(HANDLE handle, KPROCESSOR_MODE previous_mode) {
	auto is_kernel = is_kernel_handle(handle);
	if (is_kernel) {
		if (previous_mode != KernelMode) {
			return STATUS_INVALID_HANDLE;
		}

		handle = from_kernel_handle(handle);

		if (!KERNEL_PROCESS->handle_table.remove(handle)) {
			return STATUS_INVALID_HANDLE;
		}
	}
	else {
		auto* process = get_current_thread()->process;
		if (!process->handle_table.remove(handle)) {
			return STATUS_INVALID_HANDLE;
		}
	}

	return STATUS_SUCCESS;
}

NTAPI NTSTATUS ObReferenceObjectByPointer(
	PVOID object,
	ACCESS_MASK desired_access,
	POBJECT_TYPE object_type,
	KPROCESSOR_MODE access_mode) {
	auto* hdr = get_header(object);

	if (access_mode == UserMode) {
		if (object_type->index != hdr->type_index) {
			return STATUS_OBJECT_TYPE_MISMATCH;
		}
	}

	ObfReferenceObject(object);
	return STATUS_SUCCESS;
}

NTAPI NTSTATUS ObReferenceObjectByHandle(
	HANDLE handle,
	ACCESS_MASK desired_access,
	POBJECT_TYPE object_type,
	KPROCESSOR_MODE access_mode,
	PVOID* object,
	OBJECT_HANDLE_INFORMATION* handle_info) {
	assert(!handle_info);

	auto is_kernel = is_kernel_handle(handle);
	if (is_kernel) {
		handle = from_kernel_handle(handle);

		if (auto res_object_opt = KERNEL_PROCESS->handle_table.get(handle)) {
			PVOID res_object = res_object_opt.value();

			auto* hdr = get_header(res_object);
			if (object_type && hdr->type_index != object_type->index) {
				ObfDereferenceObject(res_object);
				return STATUS_OBJECT_TYPE_MISMATCH;
			}

			*object = res_object;
			return STATUS_SUCCESS;
		}
	}
	else {
		auto* process = get_current_thread()->process;
		if (auto res_object_opt = process->handle_table.get(handle)) {
			PVOID res_object = res_object_opt.value();

			auto* hdr = get_header(res_object);
			if (object_type && hdr->type_index != object_type->index) {
				ObfDereferenceObject(res_object);
				return STATUS_OBJECT_TYPE_MISMATCH;
			}

			*object = res_object;
			return STATUS_SUCCESS;
		}
	}

	return STATUS_INVALID_HANDLE;
}

NTAPI NTSTATUS ObReferenceObjectByName(
	PUNICODE_STRING object_name,
	ULONG attribs,
	PACCESS_STATE access_state,
	ACCESS_MASK desired_access,
	POBJECT_TYPE object_type,
	KPROCESSOR_MODE access_mode,
	PVOID parse_ctx,
	PVOID* object) {
	auto status = lookup_object(
		nullptr,
		object_name,
		attribs | OBJ_OPENIF,
		nullptr,
		desired_access,
		access_mode,
		object_type,
		parse_ctx,
		object);
	if (status == STATUS_OBJECT_NAME_EXISTS) {
		status = STATUS_SUCCESS;
	}
	return status;
}

NTAPI extern "C" NTSTATUS ObOpenObjectByName(
	OBJECT_ATTRIBUTES* object_attribs,
	POBJECT_TYPE object_type,
	KPROCESSOR_MODE access_mode,
	PACCESS_STATE access_state,
	ACCESS_MASK desired_access,
	PVOID parse_ctx,
	PHANDLE handle) {
	PVOID object;
	auto status = lookup_object(
		object_attribs->root_directory,
		object_attribs->object_name,
		object_attribs->attributes,
		nullptr,
		desired_access,
		access_mode,
		object_type,
		parse_ctx,
		&object);
	if (status == STATUS_OBJECT_NAME_EXISTS) {
		status = STATUS_SUCCESS;
	}
	if (!NT_SUCCESS(status)) {
		return status;
	}

	auto* hdr = get_header(object);

	if (hdr->kernel_object) {
		auto h = KERNEL_PROCESS->handle_table.insert(object, object_attribs->attributes & OBJ_INHERIT);

		if (h == INVALID_HANDLE_VALUE) {
			ObfDereferenceObject(object);
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		*handle = to_kernel_handle(h);
	}
	else {
		auto h = get_current_thread()->process->handle_table.insert(object, object_attribs->attributes & OBJ_INHERIT);

		if (h == INVALID_HANDLE_VALUE) {
			ObfDereferenceObject(object);
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		*handle = h;
	}

	return STATUS_SUCCESS;
}

NTAPI extern "C" NTSTATUS ObOpenObjectByPointer(
	PVOID object,
	ULONG handle_attribs,
	PACCESS_STATE passed_access_state,
	ACCESS_MASK desired_access,
	OBJECT_TYPE* object_type,
	KPROCESSOR_MODE access_mode,
	PHANDLE handle) {
	auto* hdr = get_header(object);

	if ((handle_attribs & OBJ_FORCE_ACCESS_CHECK) || access_mode == UserMode) {
		if (hdr->type_index != object_type->index) {
			return STATUS_OBJECT_TYPE_MISMATCH;
		}
	}

	// todo
	assert(!(handle_attribs & OBJ_EXCLUSIVE));

	if (hdr->kernel_object || (handle_attribs & OBJ_KERNEL_HANDLE)) {
		auto h = KERNEL_PROCESS->handle_table.insert(object, handle_attribs & OBJ_INHERIT);

		if (h == INVALID_HANDLE_VALUE) {
			ObfDereferenceObject(object);
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		*handle = to_kernel_handle(h);
	}
	else {
		auto h = get_current_thread()->process->handle_table.insert(object, handle_attribs & OBJ_INHERIT);

		if (h == INVALID_HANDLE_VALUE) {
			ObfDereferenceObject(object);
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		*handle = h;
	}

	return STATUS_SUCCESS;
}
