#include "malloc.hpp"
#include "vspace.hpp"
#include "mem/mem.hpp"
#include "assert.hpp"
#include "ntdef.h"
#include <hz/array.hpp>
#include <hz/spinlock.hpp>

template<usize N>
struct SlabAllocator {
	constexpr explicit SlabAllocator(const hz::array<usize, N>& sizes) {
		for (usize i = 0; i < N; ++i) {
			freelists[i].size = sizes[i];
		}
	}

	void* alloc(usize size) {
		auto old = KfRaiseIrql(DISPATCH_LEVEL);

		for (auto& list : freelists) {
			if (size <= list.size) {
				KeAcquireSpinLockAtDpcLevel(&list.lock);
				if (!list.pages.is_empty()) {
					auto page = list.pages.front();

					auto node = page->slab.freelist.next;
					assert(node);
					assert(page->slab.count != PAGE_SIZE / list.size);

					page->slab.freelist.next = static_cast<Node*>(node)->next;
					++page->slab.count;
					if (page->slab.count == PAGE_SIZE / list.size) {
						list.pages.pop_front();
					}

					KeReleaseSpinLock(&list.lock, old);
					return node;
				}
				else {
					auto phys = pmalloc();
					if (!phys) {
						KeReleaseSpinLock(&list.lock, old);
						return nullptr;
					}

					auto page = Page::from_phys(phys);

					usize count = PAGE_SIZE / list.size;

					Node* root = nullptr;
					for (usize i = 0; i < count; ++i) {
						auto node = new (to_virt<void>(phys + i * list.size)) Node {};

						node->next = root;
						root = node;
					}

					auto node = root;
					root = root->next;

					page->slab.freelist.next = root;
					page->slab.count = 1;

					list.pages.push_front(page);

					KeReleaseSpinLock(&list.lock, old);
					return node;
				}
			}
		}

		KeLowerIrql(old);
		return nullptr;
	}

	void dealloc(void* ptr, usize size) {
		auto old = KfRaiseIrql(DISPATCH_LEVEL);

		for (auto& list : freelists) {
			if (size <= list.size) {
				auto* node = new (ptr) Node {};

				auto phys = to_phys(reinterpret_cast<void*>(ALIGNDOWN(reinterpret_cast<usize>(ptr), PAGE_SIZE)));
				auto page = Page::from_phys(phys);
				assert(page);

				KeAcquireSpinLockAtDpcLevel(&list.lock);

				--page->slab.count;

				if (page->slab.count == 0) {
					list.pages.remove(page);
					pfree(phys);
					KeReleaseSpinLock(&list.lock, old);
					return;
				}
				else if (page->slab.count == PAGE_SIZE / list.size - 1) {
					list.pages.push_front(page);
				}

				node->next = static_cast<Node*>(page->slab.freelist.next);
				page->slab.freelist.next = node;

				KeReleaseSpinLock(&list.lock, old);
				return;
			}
		}

		KeLowerIrql(old);
	}

private:
	struct Node {
		Node* next;
	};

	struct Freelist {
		hz::list<Page, &Page::hook> pages {};
		usize size {};
		KSPIN_LOCK lock {};
	};

	hz::array<Freelist, N> freelists {};
};

namespace {
	constexpr hz::array<usize, 7> SIZES {
		16, 32, 64, 128, 256, 512, 1024
	};
	constinit SlabAllocator ALLOCATOR {SIZES};
}

void* operator new(size_t size) {
	return kmalloc(size);
}

void operator delete(void* ptr, size_t size) {
	kfree(ptr, size);
}

void* operator new[](size_t size) {
	return kmalloc(size);
}

void operator delete[](void* ptr, size_t size) {
	kfree(ptr, size);
}

void* kmalloc(usize size) {
	if (!size) {
		return nullptr;
	}

	if (size <= SIZES.back()) {
		return ALLOCATOR.alloc(size);
	}
	else if (size <= PAGE_SIZE) {
		auto page = pmalloc();
		if (!page) {
			return nullptr;
		}
		return to_virt<void>(page);
	}
	return KERNEL_VSPACE.alloc_backed(0, size, PageFlags::Read | PageFlags::Write);
}

void kfree(void* ptr, usize size) {
	if (!ptr || !size) {
		return;
	}

	if (size <= SIZES.back()) {
		ALLOCATOR.dealloc(ptr, size);
		return;
	}
	else if (size <= PAGE_SIZE) {
		pfree(to_phys(ptr));
		return;
	}
	KERNEL_VSPACE.free_backed(ptr, size);
}

NTAPI void* ExAllocatePool2(POOL_FLAGS flags, size_t num_of_bytes, ULONG tag) {
	auto* ptr = static_cast<usize*>(kmalloc(num_of_bytes + sizeof(usize) * 2));
	assert(reinterpret_cast<usize>(ptr) % 16 == 0);
	if (!ptr) {
		return nullptr;
	}
	*ptr = num_of_bytes;
	return &ptr[2];
}

NTAPI void ExFreePool(void* ptr) {
	auto* p = static_cast<usize*>(ptr);
	auto size = *(p - 2);
	kfree(p - 2, size);
}
