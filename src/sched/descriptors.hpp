#pragma once

#include <hz/list.hpp>
#include "utils/spinlock.hpp"

struct Thread;
struct Process;

struct ThreadDescriptor {
	~ThreadDescriptor();

	hz::list_hook hook {};
	Thread* thread {};
	KSPIN_LOCK lock {};
	int status {};
};

struct ProcessDescriptor {
	~ProcessDescriptor();

	hz::list_hook hook {};
	Process* process {};
	KSPIN_LOCK lock {};
	int status {};
};
