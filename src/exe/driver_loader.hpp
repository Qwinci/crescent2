#pragma once

#include "types.hpp"
#include "ntdef.h"
#include "string_view.hpp"
#include "pe.hpp"
#include <hz/list.hpp>
#include <hz/pair.hpp>

struct RUNTIME_FUNCTION;

struct LoadedImage {
	hz::list_hook hook {};
	usize image_base {};
	u32 image_size {};
	RUNTIME_FUNCTION* function_table {};
	ULONG function_table_len {};
};

struct DEVICE_OBJECT;

void init_driver_loader();
hz::pair<LoadedPe, DEVICE_OBJECT*> load_predefined_bus_driver(kstd::wstring_view name, kstd::wstring_view path);
DEVICE_OBJECT* load_predefined_acpi_driver(kstd::wstring_view name, kstd::wstring_view path);

extern hz::list<LoadedImage, &LoadedImage::hook> LOADED_IMAGE_LIST;
