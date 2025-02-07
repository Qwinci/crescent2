#pragma once
#include <hz/unordered_map.hpp>
#include <hz/cstddef.hpp>
#include "allocator.hpp"

namespace kstd {
	template<typename K, typename T>
	class unordered_map : public hz::unordered_map<K, T, Allocator> {
		using super = hz::unordered_map<K, T, Allocator>;

	public:
		unordered_map() : super {{}} {}
	};
}
