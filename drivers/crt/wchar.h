#ifndef _WCHAR_H
#define _WCHAR_H

#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _CRTAPI
#define _CRTAPI __declspec(dllimport)
#endif

_CRTAPI int swprintf(wchar_t* __buffer, size_t __buffer_size, const wchar_t* __fmt, ...);
_CRTAPI int vswprintf(wchar_t* __buffer, size_t __buffer_size, const wchar_t* __fmt, va_list __ap);

#ifdef __cplusplus
}
#endif

#endif
