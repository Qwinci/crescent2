#pragma once

#include <hz/variant.hpp>
#include <hz/optional.hpp>
#include "fs/file.hpp"
#include "utils/spinlock.hpp"
#include "sched/descriptors.hpp"

struct Handle {
	PVOID object;
	bool free;
	bool inherit;
};

#define INVALID_HANDLE_VALUE (HANDLE) (-1)

class HandleTable {
public:
	~HandleTable();

	hz::optional<PVOID> get(HANDLE handle);

	HANDLE insert(PVOID object, bool inherit = false);
	bool remove(HANDLE handle);

private:
	Handle* table {};
	usize size {};
	usize used {};
	HANDLE free_handle_index = INVALID_HANDLE_VALUE;
	KSPIN_LOCK lock {};
};
