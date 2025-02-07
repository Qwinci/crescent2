#pragma once
#include "string_view.hpp"
#include "types.hpp"

template<typename T, typename F>
bool path_for_each_component(hz::basic_string_view<T> path, F fn) {
	usize offset = 0;
	while (true) {
		usize end;
		if constexpr (hz::is_same_v<T, char>) {
			end = path.find_first_of("/\\", offset);
		}
		else {
			end = path.find_first_of(u"/\\", offset);
		}
		auto slice = path.substr_abs(offset, end);
		if (slice.size() == 0) {
			if (end == kstd::wstring_view::npos) {
				break;
			}
			offset = end + 1;
			continue;
		}

		if (!fn(slice)) {
			return false;
		}

		if (end == kstd::wstring_view::npos) {
			break;
		}

		offset = end + 1;
	}

	return true;
}
