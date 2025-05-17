#ifndef _ASSERT_H
#define _ASSERT_H

#ifdef NDEBUG

#define assert(expr) ((void) 0)

#else

#ifdef __cplusplus
extern "C" {
#endif

__declspec(dllimport)
void _assert(const char* _Message, const char* _File, unsigned int _Line);

#ifdef __cplusplus
}
#endif

#define assert(expr) (void) ((expr) ? (void) 0 : _assert(#expr, __FILE_NAME__, __LINE__))

#endif

#endif
