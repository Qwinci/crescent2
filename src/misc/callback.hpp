#pragma once

#include "fs/object.hpp"

struct CALLBACK_OBJECT;

using PCALLBACK_FUNCTION = void (*)(PVOID callback_ctx, PVOID arg1, PVOID arg2);

void callback_init();

NTAPI extern "C" NTSTATUS ExCreateCallback(
	CALLBACK_OBJECT** callback,
	OBJECT_ATTRIBUTES* object_attribs,
	BOOLEAN create,
	BOOLEAN allow_multiple_callbacks);
NTAPI extern "C" void ExNotifyCallback(
	PVOID callback_object,
	PVOID arg1,
	PVOID arg2);

NTAPI extern "C" PVOID ExRegisterCallback(
	CALLBACK_OBJECT* callback_object,
	PCALLBACK_FUNCTION callback_fn,
	PVOID callback_ctx);
NTAPI extern "C" void ExUnregisterCallback(PVOID callback_registration);
