#ifndef _CONSOLEAPI_H
#define _CONSOLEAPI_H

#include <windef.h>
#include <winnt.h>

__begin_decls

__declspec(dllimport) BOOL WINAPI WriteConsoleA(
	HANDLE hConsoleOutput,
	_In_reads_(nNumberOfCharsToWrite) CONST VOID* lpBuffer,
	_In_ DWORD nNumberOfCharsToWrite,
	_Out_opt_ LPDWORD lpNumberOfCharsWritten,
	_Reserved_ LPVOID lpReserved);

__end_decls

#endif
