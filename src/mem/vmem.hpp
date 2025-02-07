#pragma once
#include "types.hpp"
#include "utils/spinlock.hpp"
#include <hz/list.hpp>

class VMem {
public:
	void init(usize base, usize size, usize quantum);
	void destroy(bool assert_allocations);
	usize xalloc(usize size, usize min, usize max);
	void xfree(usize ptr, usize size);
private:
	static constexpr unsigned int size_to_index(usize size);

	static constexpr usize FREELIST_COUNT = sizeof(void*) * 8;
	static constexpr usize HASHTAB_COUNT = 16;

	struct Segment {
		hz::list_hook list_hook {};
		hz::list_hook seg_list_hook {};
		usize base {};
		usize size {};

		enum class Type {
			Free,
			Used,
			Span
		} type {};
	};
	hz::list<Segment, &Segment::list_hook> freelists[FREELIST_COUNT] {};
	hz::list<Segment, &Segment::seg_list_hook> seg_list {};
	hz::list<Segment, &Segment::list_hook> hash_tab[HASHTAB_COUNT] {};
	Segment* free_segs {};
	Segment* seg_page_list {};
	usize _base {};
	usize _size {};
	usize _quantum {};
	KSPIN_LOCK lock {};

	Segment* seg_alloc();
	void seg_free(Segment* seg);
	void freelist_remove(Segment* seg);
	void freelist_insert_no_merge(Segment* seg);
	void freelist_insert(Segment* seg);
	void hashtab_insert(Segment* seg);
	Segment* hashtab_remove(usize key);
};
