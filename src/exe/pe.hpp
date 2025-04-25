#pragma once
#include "fs/vfs.hpp"
#include <hz/result.hpp>

struct Process;

struct LoadedPe {
	usize base;
	usize entry;
};

struct NtdllOffsets {
	usize user_apc_dispatcher;
	usize ldr_initialize_thunk;
};

hz::result<LoadedPe, int> pe_load(Process* process, std::shared_ptr<VNode>& file, bool user);

void fill_ntdll_offsets(usize base);
void* pe_get_symbol_addr(LoadedPe* pe, kstd::string_view name);

extern NtdllOffsets NTDLL_OFFSETS;
