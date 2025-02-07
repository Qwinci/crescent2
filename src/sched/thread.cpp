#include "thread.hpp"

Thread::Thread(kstd::wstring_view name, Cpu* cpu, Process* process)
	: ArchThread {}, name {name}, cpu {cpu}, process {process} {}

Thread::Thread(kstd::wstring_view name, Cpu* cpu, Process* process, void (*fn)(void*), void* arg)
	: ArchThread {fn, arg, process}, name {name}, cpu {cpu}, process {process} {}
