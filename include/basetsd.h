#ifndef _BASETSD_H
#define _BASETSD_H

#ifdef _WIN64
typedef unsigned __int64 ULONG_PTR;
#else
typedef unsigned int ULONG_PTR;
#endif

typedef __int64 LONG64, *PLONG64;

typedef ULONG_PTR KAFFINITY;

typedef ULONG_PTR SIZE_T, *PSIZE_T;

#endif
