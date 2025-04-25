#include <stddef.h>
#include "utils.hpp"
#include "windef.h"
#include "processenv.h"
#include "handleapi.h"
#include "priv/peb.h"

EXPORT WINAPI HANDLE GetStdHandle(_In_ DWORD std_handle) {
	auto params = NtCurrentPeb()->ProcessParameters;

	switch (std_handle) {
		case STD_INPUT_HANDLE:
			return params->StandardInput;
		case STD_OUTPUT_HANDLE:
			return params->StandardOutput;
		case STD_ERROR_HANDLE:
			return params->StandardError;
		default:
			return INVALID_HANDLE_VALUE;
	}

	return nullptr;
}

EXPORT WINAPI BOOL WriteConsoleA(
	HANDLE hConsoleOutput,
	_In_reads_(nNumberOfCharsToWrite) CONST VOID* lpBuffer,
	_In_ DWORD nNumberOfCharsToWrite,
	_Out_opt_ LPDWORD lpNumberOfCharsWritten,
	_Reserved_ LPVOID lpReserved) {
	return true;
}
