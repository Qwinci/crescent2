#pragma once
#include "string_view.hpp"
#include "pnp.hpp"
#include <hz/span.hpp>

struct Driver {
	kstd::wstring_view name {};
	hz::span<const kstd::wstring_view> compatibles {};
	PDRIVER_INITIALIZE init {};
	PDRIVER_ADD_DEVICE add_device {};
	DRIVER_OBJECT* object {};

	void do_init() {
		auto obj = new DRIVER_OBJECT {};
		auto ext = new DRIVER_EXTENSION {};
		ext->driver = obj;
		ext->add_device = add_device;
		obj->driver_name.Buffer = const_cast<PWSTR>(name.data());
		obj->driver_name.Length = name.size() * 2;
		obj->driver_name.MaximumLength = (name.size() + 1) * 2;
		obj->driver_init = init;
		obj->driver_extension = ext;

		init(obj, nullptr);

		object = obj;
	}
};

#define DRIVER_DECL [[gnu::section(".drivers"), gnu::used]] constinit
