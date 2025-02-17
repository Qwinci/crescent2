#ifndef _BASETSD_H
#define _BASETSD_H

#ifdef _WIN64
typedef __int64 LONG_PTR;
typedef unsigned __int64 ULONG_PTR;
#else
typedef int LONG_PTR;
typedef unsigned int ULONG_PTR;
#endif

typedef unsigned __int64 ULONG64, *PULONG64;
typedef __int64 LONG64, *PLONG64;

typedef ULONG_PTR KAFFINITY;

typedef ULONG_PTR SIZE_T, *PSIZE_T;

#endif
