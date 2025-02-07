#pragma once
#include <hz/rb_tree.hpp>
#include "compare.hpp"
#include "string.hpp"
#include "string_view.hpp"
#include "mem/vmem.hpp"
#include "arch/paging.hpp"

enum class ProcessPriority {
	Idle = 1,
	Normal,
	High,
	RealTime,
	BelowNormal,
	AboveNormal
};

struct UniqueKernelMapping {
	constexpr UniqueKernelMapping() = default;
	constexpr UniqueKernelMapping(UniqueKernelMapping&& other)
		: ptr {other.ptr}, size {other.size} {
		other.ptr = nullptr;
		other.size = 0;
	}
	constexpr UniqueKernelMapping(const UniqueKernelMapping&) = delete;
	constexpr UniqueKernelMapping& operator=(UniqueKernelMapping&& other) {
		this->~UniqueKernelMapping();
		ptr = other.ptr;
		size = other.size;
		other.ptr = nullptr;
		other.size = 0;
		return *this;
	}
	constexpr UniqueKernelMapping& operator=(const UniqueKernelMapping&) = delete;

	constexpr void* data() {
		return ptr;
	}

	[[nodiscard]] constexpr const void* data() const {
		return ptr;
	}

	~UniqueKernelMapping();

	constexpr UniqueKernelMapping(void* ptr, usize size) : ptr {ptr}, size {size} {}

	void* ptr {};
	usize size {};
};

struct Process {
	Process(kstd::wstring_view name, bool user);
	~Process();

	usize allocate(void* base, usize size, PageFlags flags, UniqueKernelMapping* mapping);
	void free(usize ptr, usize size);

	kstd::wstring name;
	PageMap page_map;
	ProcessPriority priority {ProcessPriority::Normal};
	bool user;

	struct Mapping {
		hz::rb_tree_hook hook {};
		usize base {};
		usize size {};
		PageFlags flags {};

		constexpr bool operator==(const Mapping& other) const {
			return base == other.base;
		}

		constexpr int operator<=>(const Mapping& other) const {
			return kstd::threeway(base, other.base);
		}
	};

	KSPIN_LOCK mapping_lock {};
	hz::rb_tree<Mapping, &Mapping::hook> mappings {};

private:
	VMem vmem {};
};

extern hz::manually_init<Process> KERNEL_PROCESS;
