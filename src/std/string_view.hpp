#pragma once

#include <hz/string_view.hpp>

namespace kstd {
	using string_view = hz::string_view;
	using wstring_view = hz::u16string_view;
}

static_assert(sizeof(char16_t) == 2);
