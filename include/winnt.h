#ifndef _WINNT_H
#define _WINNT_H

#include <intsafe.h>
#include <basetsd.h>

#define VOID void

typedef void* PVOID;
typedef PVOID HANDLE;
typedef char CHAR;
#ifdef __cplusplus
typedef char16_t WCHAR;
#else
typedef short WCHAR;
#endif

typedef CHAR* LPSTR;
typedef WCHAR* LPWSTR;
typedef WCHAR* PWSTR;
typedef const WCHAR* PWCSTR;

typedef PVOID HANDLE;
typedef HANDLE* PHANDLE;

typedef DWORD ACCESS_MASK;

typedef int LONG;
typedef long long LONGLONG;

#ifndef _NTDEF_H
typedef union _LARGE_INTEGER {
	struct {
		DWORD LowPart;
		LONG HighPart;
	};
	struct {
		DWORD LowPart;
		LONG HighPart;
	} u;
	LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;
#endif

#endif
