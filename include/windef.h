#ifndef _WINDEF_H
#define _WINDEF_H

#include <intsafe.h>
#include <winnt.h>

#define _In_
#define _In_reads_(count)
#define _Out_
#define _Out_opt_
#define _Reserved_

#ifdef __cplusplus
#define __begin_decls extern "C" {
#define __end_decls }
#else
#define __begin_decls
#define __end_decls
#endif

#define WINAPI __stdcall
#define CONST const

typedef short SHORT;
typedef unsigned short USHORT;
typedef int BOOL;

typedef DWORD* LPDWORD;
typedef void* LPVOID;
typedef HANDLE HINSTANCE;

#endif
