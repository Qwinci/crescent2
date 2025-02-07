#pragma once
#include "stdio.hpp"

struct FbLog : LogSink {
	FbLog(usize phys, void* mapping, u32 width, u32 height, usize pitch, u32 bpp);
	~FbLog() override;

	void write(hz::string_view str) override;
	void write(hz::u16string_view str) override;

	void clear();

	u32 column = 0;
	u32 row = 0;
	u32 scale = 1;

	usize phys;
	void* mapping;
	u32 width;
	u32 height;
	usize pitch;
	u32 bpp;
	u32 fg_color {0x00FF00};
};
