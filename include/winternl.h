#ifndef _WINTERNL_H
#define _WINTERNL_H
#include "winnt.h"
#include "windef.h"

typedef struct _UNICODE_STRING {
	USHORT Length;
	USHORT MaximumLength;
	PWSTR Buffer;
} UNICODE_STRING, *PUNICODE_STRING, *const PCUNICODE_STRING;

#endif
