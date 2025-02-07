#pragma once
#include "types.hpp"
#include <hz/rb_tree.hpp>
#include "vmem.hpp"
#include "arch/paging.hpp"
#include "compare.hpp"

class VirtualSpace {
public:
	void init(usize base, usize size);
	void destroy();

	void* alloc(usize hint, usize size);
	void free(void* ptr, usize size);

	void* alloc_backed(usize hint, usize size, PageFlags flags, CacheMode cache_mode = CacheMode::WriteBack);
	void free_backed(void* ptr, usize size);

private:
	VMem vmem;
};

extern VirtualSpace KERNEL_VSPACE;
