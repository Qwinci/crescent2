#include <Windows.h>

int main() {
	asm volatile("syscall" : : "a"(100));
	HANDLE std_out = GetStdHandle(STD_OUTPUT_HANDLE);
	WriteConsoleA(std_out, "Hello world!\n", sizeof("Hello world!\n") - 1, nullptr, nullptr);
}
