#pragma once
#include "types.hpp"
#include "mem/malloc.hpp"

namespace kstd {
	struct Allocator {
		static void* allocate(usize size) {
			return kmalloc(size);
		}

		static void deallocate(void* ptr, usize size) {
			kfree(ptr, size);
		}
	};
}
