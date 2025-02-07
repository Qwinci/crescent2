#ifndef _WINBASE_H
#define _WINBASE_H

#include <processenv.h>
#include <consoleapi.h>

__begin_decls

WINAPI int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd);
WINAPI int wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd);

__end_decls

#endif
