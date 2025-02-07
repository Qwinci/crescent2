#include "fb_log.hpp"
#include "stdio.hpp"
#include "font.hpp"

namespace {
	constexpr u32 FONT_WIDTH = 8;
	constexpr u32 FONT_HEIGHT = 8;
	constexpr u32 LINE_HEIGHT = 12;
}

#define memset __builtin_memset

FbLog::FbLog(usize phys, void* mapping, u32 width, u32 height, usize pitch, u32 bpp)
	: phys {phys}, mapping {mapping}, width {width}, height {height}, pitch {pitch},
	bpp {bpp} {
	IrqGuard irq_guard;
	LOG.lock()->register_sink(this);
}

FbLog::~FbLog() {
	IrqGuard irq_guard;
	LOG.lock()->unregister_sink(this);
}

void FbLog::write(hz::string_view str) {
	for (auto c : str) {
		if (c == '\n') {
			column = 0;
			++row;
			continue;
		}
		else if (c == '\t') {
			column += 4 - (column % 4);
			continue;
		}

		if ((column + 1) * FONT_WIDTH * scale >= width) {
			column = 0;
			++row;
		}
		if (row * LINE_HEIGHT * scale >= height) {
			column = 0;
			row = 0;
			clear();
		}

		auto* ptr = FONT[static_cast<unsigned char>(c)];
		auto start_x = column * FONT_WIDTH * scale;
		auto start_y = row * LINE_HEIGHT * scale;
		for (u32 y = 0; y < FONT_HEIGHT * scale; ++y) {
			auto font_row = ptr[y / scale];
			for (u32 x = 0; x < FONT_WIDTH * scale; ++x) {
				auto* pixel_ptr = offset(mapping, u32*, (start_y + y) * pitch + (start_x + x) * 4);
				if (font_row >> (x / scale) & 1) {
					*pixel_ptr = fg_color;
				}
			}
		}

		++column;
	}
}

void FbLog::write(hz::u16string_view str) {
	for (auto c : str) {
		if (c == '\n') {
			column = 0;
			++row;
			continue;
		}
		else if (c == '\t') {
			column += 4 - (column % 4);
			continue;
		}

		if ((column + 1) * FONT_WIDTH * scale >= width) {
			column = 0;
			++row;
		}
		if (row * LINE_HEIGHT * scale >= height) {
			column = 0;
			row = 0;
			clear();
		}

		auto* ptr = FONT[static_cast<unsigned char>(c)];
		auto start_x = column * FONT_WIDTH * scale;
		auto start_y = row * LINE_HEIGHT * scale;
		for (u32 y = 0; y < FONT_HEIGHT * scale; ++y) {
			auto font_row = ptr[y / scale];
			for (u32 x = 0; x < FONT_WIDTH * scale; ++x) {
				auto* pixel_ptr = offset(mapping, u32*, (start_y + y) * pitch + (start_x + x) * 4);
				if (font_row >> (x / scale) & 1) {
					*pixel_ptr = fg_color;
				}
			}
		}

		++column;
	}
}

void FbLog::clear() {
	memset(mapping, 0, pitch * height);
}
