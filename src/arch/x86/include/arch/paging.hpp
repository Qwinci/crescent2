#pragma once
#include "types.hpp"
#include "mem/pmalloc.hpp"
#include "flags_enum.hpp"
#include "utils/spinlock.hpp"
#include "caching.hpp"
#include <hz/manually_init.hpp>
#include <hz/list.hpp>

enum class PageFlags {
	Read = 1 << 0,
	Write = 1 << 1,
	Execute = 1 << 2,
	User = 1 << 3
};
FLAGS_ENUM(PageFlags);

class PageMap {
public:
	explicit PageMap(PageMap* kernel_map);
	~PageMap();

	[[nodiscard]] bool map_2mb(u64 virt, u64 phys, PageFlags flags, CacheMode cache_mode);
	[[nodiscard]] bool map(u64 virt, u64 phys, PageFlags flags, CacheMode cache_mode);
	void protect(u64 virt, PageFlags flags, CacheMode cache_mode);

	[[nodiscard]] u64 get_phys(u64 virt);

	void unmap(u64 virt);
	void use();

	constexpr bool operator==(const PageMap& other) const {
		return level0 == other.level0;
	}

	void fill_high_half();

	[[nodiscard]] u64 get_top_level_phys() const;

private:
	u64* level0;
	hz::list<Page, &Page::hook> used_pages {};
	KSPIN_LOCK lock {};
};
