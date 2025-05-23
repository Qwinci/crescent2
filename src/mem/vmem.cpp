#include "vmem.hpp"
#include <hz/bit.hpp>
#include <hz/new.hpp>

#ifdef TESTING
#include <cassert>
#include <cstdlib>

#define pmalloc(count) malloc(count * 0x1000)
#define pfree(ptr, count) free(reinterpret_cast<void*>(ptr))
template<typename T>
T* to_virt(void* ptr) {
	return static_cast<T*>(ptr);
}

template<typename T>
usize to_phys(T* ptr) {
	return reinterpret_cast<usize>(ptr);
}

#define ALIGNUP(value, align) (((value) + ((align) - 1)) & ~((align) - 1))

#else
#include "mem/mem.hpp"
#include "pmalloc.hpp"
#include "arch/paging.hpp"
#include "assert.hpp"
#endif

#include <hz/algorithm.hpp>

#ifdef TESTING
#define PAGE_SIZE 0x1000
#endif

static void* alloc_page() {
	auto phys = pmalloc();
	if (!phys) {
		return nullptr;
	}
	return to_virt<void>(phys);
}

static void free_page(void* addr) {
	pfree(to_phys(addr));
}

static u64 murmur64(u64 key) {
	key ^= key >> 33;
	key *= 0xFF51AFD7ED558CCDULL;
	key ^= key >> 33;
	key *= 0xC4CEB9FE1A85EC53ULL;
	key ^= key >> 33;
	return key;
}

constexpr unsigned int VMem::size_to_index(usize size) {
	return FREELIST_COUNT - hz::countl_zero(size) - 1;
}

void VMem::init(usize base, usize size, usize quantum) {
	_base = base;
	_size = size;
	_quantum = quantum;

	auto* span = new (seg_alloc()) Segment {
		.base = base,
		.size = size,
		.type = Segment::Type::Span
	};

	auto* init = new (seg_alloc()) Segment {
		.base = base,
		.size = size,
		.type = Segment::Type::Free
	};

	seg_list.push(span);
	seg_list.push(init);

	freelists[size_to_index(size)].push(init);
}

void VMem::destroy(bool assert_allocations) {
	KIRQL old = KeAcquireSpinLockRaiseToDpc(&lock);

	while (seg_page_list) {
		auto* seg = seg_page_list;
		seg_page_list = static_cast<Segment*>(seg->list_hook.next);
		free_page(seg);
	}
	free_segs = nullptr;
	for (const auto& i : hash_tab) {
		if (assert_allocations) {
			assert(i.is_empty() && "tried to destroy vmem with allocations");
		}
	}
	seg_list.clear();
	for (auto& list : freelists) {
		list.clear();
	}

	KeReleaseSpinLock(&lock, old);
}

VMem::Segment* VMem::seg_alloc() {
	if (free_segs) {
		auto seg = free_segs;
		free_segs = static_cast<Segment*>(seg->list_hook.next);
		return seg;
	}
	else {
		auto* page = alloc_page();
		if (!page) {
			return nullptr;
		}

		auto* page_seg = new (page) Segment {
			.list_hook {
				.next = seg_page_list
			}
		};
		seg_page_list = page_seg;

		auto* seg = page_seg + 1;
		for (usize i = 1; i < PAGE_SIZE / sizeof(Segment); ++i) {
			seg->list_hook.next = free_segs;
			free_segs = seg;
			seg += 1;
		}

		seg = free_segs;
		free_segs = static_cast<Segment*>(seg->list_hook.next);
		return seg;
	}
}

void VMem::seg_free(VMem::Segment* seg) {
	seg->list_hook.next = free_segs;
	free_segs = seg;
}

usize VMem::xalloc(usize size, usize min, usize max) {
	size = ALIGNUP(size, _quantum);

	if (!max) {
		max = UINTPTR_MAX;
	}
	else if (max == min) {
		max = min + size;
	}

	auto index = size_to_index(size);

	bool has_min_max = min || max;
	if (!has_min_max) {
		if (!hz::has_single_bit(size)) {
			index += 1;
		}
	}

	auto seg_fit = [&](Segment* seg, hz::list<Segment, &Segment::list_hook>& list, bool popped) {
		usize start = hz::max(seg->base, min);
		usize end = hz::min(seg->base + seg->size, max);
		if (start > end || end - start < size) {
			if (popped) {
				list.push_front(seg);
			}
			return usize {0};
		}

		if (seg->size == size) {
			if (!popped) {
				list.remove(seg);
			}
			hashtab_insert(seg);
			return seg->base;
		}

		auto* alloc_seg = seg_alloc();
		if (!alloc_seg) {
			if (popped) {
				list.push_front(seg);
			}
			return usize {0};
		}

		if (start != seg->base) {
			auto* new_seg = seg_alloc();
			if (!new_seg) {
				if (popped) {
					list.push_front(seg);
				}
				seg_free(alloc_seg);
				return usize {0};
			}

			new (new_seg) Segment {
				.base = seg->base,
				.size = start - seg->base,
				.type = Segment::Type::Free
			};

			seg_list.insert_before(seg, new_seg);
			seg->base = start;
			seg->size -= new_seg->size;

			freelist_insert_no_merge(new_seg);
		}

		if (!popped) {
			list.remove(seg);
		}

		if (seg->size == size) {
			seg_free(alloc_seg);
			hashtab_insert(seg);
			return seg->base;
		}

		new (alloc_seg) Segment {
			.seg_list_hook {
				.prev = seg->seg_list_hook.prev,
				.next = seg
			},
			.base = seg->base,
			.size = size,
			.type = Segment::Type::Used
		};
		static_cast<Segment*>(seg->seg_list_hook.prev)->seg_list_hook.next = alloc_seg;
		seg->seg_list_hook.prev = alloc_seg;
		seg->base += size;
		seg->size -= size;

		freelist_insert(seg);
		hashtab_insert(alloc_seg);

		return alloc_seg->base;
	};

	KIRQL old = KeAcquireSpinLockRaiseToDpc(&lock);

	for (usize i = index; i < FREELIST_COUNT; ++i) {
		auto& list = freelists[i];
		auto* seg = list.pop_front();
		if (!seg) {
			continue;
		}

		if (seg->size >= size) {
			if (auto addr = seg_fit(seg, list, true)) {
				KeReleaseSpinLock(&lock, old);
				return addr;
			}
		}
		else if (has_min_max) {
			list.push_front(seg);
			for (auto& other_seg : list) {
				if (other_seg.size >= size) {
					if (auto addr = seg_fit(&other_seg, list, false)) {
						KeReleaseSpinLock(&lock, old);
						return addr;
					}
				}
			}

			continue;
		}
	}

	KeReleaseSpinLock(&lock, old);

	return 0;
}

void VMem::freelist_remove(VMem::Segment* seg) {
	usize index = size_to_index(seg->size);
	freelists[index].remove(seg);
}

void VMem::freelist_insert_no_merge(VMem::Segment* seg) {
	usize index = size_to_index(seg->size);
	auto& list = freelists[index];

	assert(seg->size);
	seg->type = Segment::Type::Free;
	list.push_front(seg);
}

void VMem::freelist_insert(VMem::Segment* seg) {
	assert(seg->size);

	while (true) {
		if (auto prev = static_cast<Segment*>(seg->seg_list_hook.prev);
			prev && prev->type == Segment::Type::Free &&
			prev->base + prev->size == seg->base) {
			if (auto next = static_cast<Segment*>(seg->seg_list_hook.next)) {
				next->seg_list_hook.prev = prev;
			}
			prev->seg_list_hook.next = seg->seg_list_hook.next;

			freelist_remove(prev);

			prev->size += seg->size;

			seg_free(seg);
			seg = prev;
			continue;
		}
		if (auto next = static_cast<Segment*>(seg->seg_list_hook.next);
			next && next->type == Segment::Type::Free &&
			seg->base + seg->size == next->base) {
			if (auto next_next = static_cast<Segment*>(next->seg_list_hook.next)) {
				next_next->seg_list_hook.prev = seg;
			}
			seg->seg_list_hook.next = next->seg_list_hook.next;

			freelist_remove(next);

			seg->size += next->size;

			seg_free(next);
			continue;
		}

		freelist_insert_no_merge(seg);
		break;
	}
}

void VMem::hashtab_insert(VMem::Segment* seg) {
	seg->type = Segment::Type::Used;

	u64 hash = murmur64(seg->base);
	hash_tab[hash % HASHTAB_COUNT].push_front(seg);
}

VMem::Segment* VMem::hashtab_remove(usize key) {
	u64 hash = murmur64(key);
	auto& list = hash_tab[hash % HASHTAB_COUNT];
	for (auto& entry : list) {
		if (entry.base == key) {
			list.remove(&entry);
			return &entry;
		}
	}
	return nullptr;
}

void VMem::xfree(usize ptr, usize size) {
	KIRQL old = KeAcquireSpinLockRaiseToDpc(&lock);

	auto* seg = hashtab_remove(ptr);
	if (!seg) {
		// todo
		KeReleaseSpinLock(&lock, old);
		return;
	}
	else if (seg->size != ALIGNUP(size, _quantum)) {
		assert(false && "tried to free with different size than allocated");
	}
	freelist_insert(seg);
	KeReleaseSpinLock(&lock, old);
}
