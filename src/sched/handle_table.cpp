#include "handle_table.hpp"
#include "sys/misc.hpp"
#include "fs/file.hpp"
#include "process.hpp"

hz::optional<PVOID> HandleTable::get(HANDLE handle) {
	auto old = KeAcquireSpinLockRaiseToDpc(&lock);

	auto value = reinterpret_cast<usize>(handle);

	hz::optional<PVOID> ret;
	if (value < used) {
		auto loc = table[value];
		if (!loc.free) {
			ObfReferenceObject(loc.object);
			ret = loc.object;
		}
	}

	KeReleaseSpinLock(&lock, old);
	return ret;
}

HANDLE HandleTable::insert(PVOID object, bool inherit) {
	auto old = KeAcquireSpinLockRaiseToDpc(&lock);

	HANDLE ret = nullptr;
	if (free_handle_index != INVALID_HANDLE_VALUE) {
		auto value = free_handle_index;
		auto next = table[reinterpret_cast<usize>(free_handle_index)].object;
		free_handle_index = next;

		ret = value;
	}
	else {
		if (size == used) {
			auto new_size = size < 8 ? 8 : (size + size / 2);
			auto* new_table = static_cast<Handle*>(kmalloc(new_size * sizeof(Handle)));
			if (!new_table) {
				KeReleaseSpinLock(&lock, old);
				return INVALID_HANDLE_VALUE;
			}

			for (usize i = 0; i < size; ++i) {
				new (&new_table[i]) Handle {table[i]};
			}

			for (usize i = size; i < new_size; ++i) {
				new (&new_table[i]) Handle {};
			}

			kfree(table, size * sizeof(Handle));
			table = new_table;
			size = new_size;
		}

		ret = reinterpret_cast<HANDLE>(used++);
	}

	ObfReferenceObject(object);
	auto& entry = table[reinterpret_cast<usize>(ret)];
	entry.object = object;
	entry.free = false;
	entry.inherit = inherit;

	KeReleaseSpinLock(&lock, old);
	return ret;
}

HandleTable::~HandleTable() {
	for (usize i = 0; i < size; ++i) {
		if (!table[i].free) {
			ObfDereferenceObject(table[i].object);
		}
	}

	kfree(table, size * sizeof(Handle));
}

bool HandleTable::remove(HANDLE handle) {
	auto value = reinterpret_cast<usize>(handle);

	auto old = KeAcquireSpinLockRaiseToDpc(&lock);

	bool ret;
	if (value < used) {
		auto& loc = table[value];
		if (!loc.free) {
			ObfDereferenceObject(loc.object);
			loc.free = true;
			loc.object = free_handle_index;
			free_handle_index = handle;
			ret = true;
		}
		else {
			ret = false;
		}
	}
	else {
		ret = false;
	}

	KeReleaseSpinLock(&lock, old);
	return ret;
}
