#pragma once
#include <hz/string.hpp>
#include <hz/cstddef.hpp>
#include "allocator.hpp"

namespace kstd {
	template<typename T>
	class basic_string : public hz::basic_string<T, Allocator> {
		using super = hz::basic_string<T, Allocator>;

	public:
		basic_string() : super {{}} {}
		explicit basic_string(std::nullptr_t) : super {nullptr, {}} {}
		explicit basic_string(hz::basic_string_view<T> str) : super {str, {}} {}

		using super::operator=;
		using super::operator+=;
		using super::operator+;
		//using super::operator==;
	};

	using string = basic_string<char>;
	using wstring = basic_string<char16_t>;
}

namespace hz {
	template<typename T, hz::Hasher Hasher>
	struct hash_impl<kstd::basic_string<T>, Hasher> {
		static void hash(Hasher& hasher, const kstd::basic_string<T>& self) {
			for (auto c : self) {
				hasher.hash(c);
			}
		}
	};
}
