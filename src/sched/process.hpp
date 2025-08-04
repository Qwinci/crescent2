#pragma once
#include <hz/rb_tree.hpp>
#include "compare.hpp"
#include "string.hpp"
#include "string_view.hpp"
#include "mem/vmem.hpp"
#include "arch/paging.hpp"
#include "fs/object.hpp"
#include "flags_enum.hpp"
#include "descriptors.hpp"
#include "handle_table.hpp"
#include "thread.hpp"

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
	constexpr UniqueKernelMapping(UniqueKernelMapping&& other) noexcept
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

struct _PEB;

enum class MappingFlags {
	None,
	Backed = 1 << 0,
	DisallowUserProtectionChange = 1 << 1
};
FLAGS_ENUM(MappingFlags);

struct Process {
	explicit Process(kstd::wstring_view name);
	explicit Process(const PageMap& map);
	~Process();

	usize allocate(void* base, usize size, PageFlags page_flags, MappingFlags mapping_flags, UniqueKernelMapping* mapping);
	void free(usize ptr, usize size);

	void mark_as_exiting(int exit_status);

	void add_thread(Thread* thread);
	void remove_thread(Thread* thread);

	kstd::wstring name;
	HANDLE handle {INVALID_HANDLE_VALUE};
	PageMap page_map;
	ProcessPriority priority {ProcessPriority::Normal};
	bool user;

	struct Mapping {
		hz::rb_tree_hook hook {};
		usize base {};
		usize size {};
		PageFlags flags {};
		MappingFlags mapping_flags {};

		constexpr bool operator==(const Mapping& other) const {
			return base == other.base;
		}

		constexpr int operator<=>(const Mapping& other) const {
			return kstd::threeway(base, other.base);
		}
	};

	KSPIN_LOCK mapping_lock {};
	hz::rb_tree<Mapping, &Mapping::hook> mappings {};
	hz::list<Thread, &Thread::process_hook> threads {};
	KSPIN_LOCK threads_lock {};
	usize ntdll_base {};
	_PEB* peb {};
	bool exiting {};
	int exit_status {};
	KSPIN_LOCK lock {};
	HandleTable handle_table {};

private:
	VMem vmem {};
};

extern Process* KERNEL_PROCESS;
extern hz::manually_init<PageMap> KERNEL_MAP;

extern HandleTable SCHED_HANDLE_TABLE;
