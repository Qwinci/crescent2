#ifndef _PROCESSENV_H
#define _PROCESSENV_H

#include <windef.h>
#include <winnt.h>

__begin_decls

#define STD_INPUT_HANDLE ((DWORD)-10)
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define STD_ERROR_HANDLE ((DWORD)-12)

__declspec(dllimport) HANDLE WINAPI GetStdHandle(_In_ DWORD nStdHandle);

__end_decls

#endif
