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

typedef char CHAR;
typedef short SHORT;
typedef int INT;
typedef int LONG;
typedef long long LONGLONG;

typedef unsigned char UCHAR;
typedef unsigned short USHORT;
typedef unsigned int UINT;
typedef unsigned int ULONG, *PULONG;
typedef unsigned long long ULONGLONG;

typedef int BOOL;

typedef DWORD* LPDWORD;
typedef void* LPVOID;
typedef HANDLE HINSTANCE;

#endif
