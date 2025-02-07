#ifndef _WINNT_H
#define _WINNT_H

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

#endif
