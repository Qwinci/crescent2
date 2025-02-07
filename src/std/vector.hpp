#pragma once
#include <hz/vector.hpp>
#include "allocator.hpp"

namespace kstd {
	template<typename T>
	class vector : public hz::vector<T, Allocator> {
	public:
		vector() : hz::vector<T, Allocator> {{}} {}
	};
}
