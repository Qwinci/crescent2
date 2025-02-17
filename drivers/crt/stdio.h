#ifndef _STDIO_H
#define _STDIO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _CRTAPI
#define _CRTAPI __declspec(dllimport)
#endif

__attribute__((__format__(__printf__, 3, 4)))
_CRTAPI int snprintf(char* __buffer, size_t __buffer_size, const char* __fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
