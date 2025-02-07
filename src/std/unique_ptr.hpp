#pragma once

#include <hz/cstddef.hpp>
#include <hz/utility.hpp>
#include <hz/new.hpp>
#include "mem/malloc.hpp"

namespace std {
	template<typename T>
	class unique_ptr {
	public:
		constexpr unique_ptr() = default;
		constexpr unique_ptr(std::nullptr_t) : ptr {} {}

		constexpr unique_ptr(T* ptr) : ptr {ptr} {}

		constexpr unique_ptr(unique_ptr&& other) : ptr {other.ptr} {
			other.ptr = nullptr;
		}

		template<typename U>
		constexpr unique_ptr(unique_ptr<U>&& other) requires (__is_convertible(U*, T*))
			: ptr {other.ptr} {
			other.ptr = nullptr;
		}

		constexpr unique_ptr(const unique_ptr&) = delete;

		constexpr unique_ptr& operator=(unique_ptr&& other) {
			delete ptr;
			ptr = other.ptr;
			other.ptr = nullptr;
			return *this;
		}

		template<typename U>
		constexpr unique_ptr& operator=(unique_ptr<U>&& other) requires(__is_convertible(U*, T*)) {
			delete ptr;
			ptr = other.ptr;
			other.ptr = nullptr;
			return *this;
		}

		constexpr unique_ptr& operator=(std::nullptr_t) {
			delete ptr;
			ptr = nullptr;
			return *this;
		}

		constexpr unique_ptr& operator=(const unique_ptr&) = delete;

		constexpr T* release() {
			auto p = ptr;
			ptr = nullptr;
			return p;
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

		constexpr ~unique_ptr() {
			delete ptr;
		}

	private:
		template<typename>
		friend class unique_ptr;

		T* ptr {};
	};

	template<typename T, typename... Args>
	unique_ptr<T> make_unique(Args&&... args) {
		return unique_ptr<T> {new T {forward<Args>(args)...}};
	}
}
