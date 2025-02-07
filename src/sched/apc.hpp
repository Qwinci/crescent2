#pragma once
#include "types.hpp"
#include "thread.hpp"
#include "wait.hpp"

struct KAPC {
	u8 type;
	u8 all_flags;
	u8 size;
	u8 spare_byte1;
	u32 spare_long;
	Thread* thread;
	LIST_ENTRY apc_list_entry;
	void* reserved[3];
	void* normal_ctx;
	void* system_arg1;
	void* system_arg2;
	i8 apc_state_index;
	KPROCESSOR_MODE apc_mode;
	bool inserted;
};
