#pragma once
#include "fs/vfs.hpp"
#include <hz/result.hpp>

struct Process;

struct LoadedPe {
	usize entry;
};

hz::result<LoadedPe, int> pe_load(Process* process, std::shared_ptr<VNode>& file, bool user);
