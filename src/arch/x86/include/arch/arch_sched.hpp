#pragma once

struct Thread;

inline Thread* get_current_thread() {
	Thread* thread;
	asm("movq %%gs:0x188, %0" : "=r"(thread));
	return thread;
}

inline void set_current_thread(Thread* thread) {
	asm volatile("movq %0, %%gs:0x188" : : "r"(thread));
}
