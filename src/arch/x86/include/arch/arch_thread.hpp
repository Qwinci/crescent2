#pragma once
#include "types.hpp"

struct Process;

struct ArchThread {
	ArchThread() = default;
	ArchThread(void (*fn)(void*), void* arg, Process* process);
	~ArchThread();

	ArchThread* self {this};
	u8* sp {};
	u8* syscall_sp {};
	usize saved_user_sp {};
	usize handler_ip {};
	usize handler_sp {};
	u8* kernel_stack_base {};
	usize user_stack_base {};
	u8* simd {};

	u64 fs_base {};
	u64 gs_base {};
};

static_assert(offsetof(ArchThread, sp) == 8);
static_assert(offsetof(ArchThread, syscall_sp) == 16);
static_assert(offsetof(ArchThread, saved_user_sp) == 24);
static_assert(offsetof(ArchThread, handler_ip) == 32);
static_assert(offsetof(ArchThread, handler_sp) == 40);
