#include "utils.hpp"
#include "windef.h"

EXPORT HANDLE WINAPI GetStdHandle(_In_ DWORD nStdHandle) {
	return nullptr;
}

EXPORT BOOL WINAPI WriteConsoleA(
	HANDLE hConsoleOutput,
	_In_reads_(nNumberOfCharsToWrite) CONST VOID* lpBuffer,
	_In_ DWORD nNumberOfCharsToWrite,
	_Out_opt_ LPDWORD lpNumberOfCharsWritten,
	_Reserved_ LPVOID lpReserved) {
	return true;
}
