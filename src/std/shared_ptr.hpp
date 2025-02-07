#pragma once

#include <hz/cstddef.hpp>
#include <hz/utility.hpp>
#include <hz/new.hpp>
#include "mem/malloc.hpp"

namespace std {
	template<typename T>
	class shared_ptr {
	public:
		constexpr shared_ptr() = default;
		constexpr shared_ptr(std::nullptr_t) : ptr {}, refs {} {}

		constexpr shared_ptr(const shared_ptr& other) : ptr {other.ptr}, refs {other.refs} {
			__atomic_add_fetch(refs, 1, __ATOMIC_RELAXED);
		}

		template<typename Y>
		constexpr shared_ptr(const shared_ptr<Y>& other) requires (__is_convertible(Y*, T*))
			: ptr {other.ptr}, refs {other.refs} {
			__atomic_add_fetch(refs, 1, __ATOMIC_RELAXED);
		}

		constexpr shared_ptr(shared_ptr&& other) : ptr {other.ptr}, refs {other.refs} {
			other.ptr = nullptr;
			other.refs = nullptr;
		}

		template<typename Y>
		constexpr shared_ptr(shared_ptr<Y>&& other) requires (__is_convertible(Y*, T*))
			: ptr {other.ptr}, refs {other.refs} {
			other.ptr = nullptr;
			other.refs = nullptr;
		}

		constexpr shared_ptr& operator=(const shared_ptr& other) {
			if (&other == this) {
				return *this;
			}

			decrement_refs();
			ptr = other.ptr;
			refs = other.refs;
			__atomic_add_fetch(refs, 1, __ATOMIC_RELAXED);
			return *this;
		}

		template<typename Y>
		constexpr shared_ptr& operator=(const shared_ptr<Y>& other) requires (__is_convertible(Y*, T*)) {
			decrement_refs();
			ptr = other.ptr;
			refs = other.refs;
			__atomic_add_fetch(refs, 1, __ATOMIC_RELAXED);
			return *this;
		}

		constexpr shared_ptr& operator=(shared_ptr&& other) {
			decrement_refs();
			ptr = other.ptr;
			refs = other.refs;
			other.ptr = nullptr;
			other.refs = nullptr;
			return *this;
		}

		template<typename Y>
		constexpr shared_ptr& operator=(shared_ptr<Y>&& other) requires (__is_convertible(Y*, T*)) {
			decrement_refs();
			ptr = other.ptr;
			refs = other.refs;
			other.ptr = nullptr;
			other.refs = nullptr;
			return *this;
		}

		constexpr ~shared_ptr() {
			if (ptr) {
				decrement_refs();
			}
		}

		constexpr T* operator->() {
			return ptr;
		}

		constexpr const T* operator->() const {
			return ptr;
		}

		constexpr T& operator*() {
			return *ptr;
		}

		constexpr const T& operator*() const {
			return *ptr;
		}

		constexpr T* get() {
			return ptr;
		}

		constexpr const T* get() const {
			return ptr;
		}

		constexpr explicit operator bool() const {
			return ptr;
		}

	private:
		template<typename>
		friend class shared_ptr;

		template<typename Y, typename... Args>
		friend shared_ptr<Y> make_shared(Args&&... args);

		constexpr shared_ptr(T* ptr, size_t* refs) : ptr {ptr}, refs {refs} {}

		void decrement_refs() {
			if (!ptr) {
				return;
			}
			if (__atomic_add_fetch(refs, -1, __ATOMIC_ACQ_REL) == 0) {
				ptr->~T();
				kfree(ptr, ((sizeof(T) + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1)) + sizeof(size_t));
				ptr = nullptr;
				refs = nullptr;
			}
		}

		T* ptr {};
		size_t* refs {};
	};

	template<typename T, typename... Args>
	shared_ptr<T> make_shared(Args&&... args) {
		static constexpr size_t t_size = (sizeof(T) + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1);

		char* storage = static_cast<char*>(kmalloc(t_size + sizeof(size_t)));
		auto* ptr = new (storage) T {forward<Args>(args)...};
		auto* refs = new (storage + t_size) size_t {1};
		return {ptr, refs};
	}
}
