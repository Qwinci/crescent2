#pragma once

#ifdef __clang__
#define __THREAD_ANNOTATION_ATTRIBUTE__(x) __attribute__((x))
#else
#define __THREAD_ANNOTATION_ATTRIBUTE__(x)
#endif

#define CAPABILITY(x) __THREAD_ANNOTATION_ATTRIBUTE__(capability(x))
#define SCOPED_CAPABILITY __THREAD_ANNOTATION_ATTRIBUTE__(scoped_lockable)
#define GUARDED_BY(x) __THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(x))
#define PT_GUARDED_BY(x) __THREAD_ANNOTATION_ATTRIBUTE__(pt_guarded_by(x))
#define ACQUIRED_BEFORE(...) __THREAD_ANNOTATION_ATTRIBUTE__(acquired_before(__VA_ARGS__))
#define ACQUIRED_AFTER(...) __THREAD_ANNOTATION_ATTRIBUTE__(acquired_after(__VA_ARGS__))
#define REQUIRES(...) __THREAD_ANNOTATION_ATTRIBUTE__(requires_capability(__VA_ARGS__))
#define REQUIRES_SHARED(...) __THREAD_ANNOTATION_ATTRIBUTE__(requires_shared_capability(__VA_ARGS__))
#define ACQUIRE(...) __THREAD_ANNOTATION_ATTRIBUTE__(acquire_capability(__VA_ARGS__))
#define ACQUIRE_SHARED(...) __THREAD_ANNOTATION_ATTRIBUTE__(acquire_shared_capability(__VA_ARGS__))
#define RELEASE(...) __THREAD_ANNOTATION_ATTRIBUTE__(release_capability(__VA_ARGS__))
#define RELEASE_SHARED(...) __THREAD_ANNOTATION_ATTRIBUTE__(release_shared_capability(__VA_ARGS__))
#define RELEASE_GENERIC(...) __THREAD_ANNOTATION_ATTRIBUTE__(release_generic_capability(__VA_ARGS__))
#define TRY_ACQUIRE(...) __THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_capability(__VA_ARGS__))
#define TRY_ACQUIRE_SHARED(...) __THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_shared_capability(__VA_ARGS__))
#define EXCLUDES(...) __THREAD_ANNOTATION_ATTRIBUTE__(locks_excluded(__VA_ARGS__))
#define ASSERT_CAPABILITY(x) __THREAD_ANNOTATION_ATTRIBUTE__(assert_capability(x))
#define ASSERT_SHARED_CAPABILITY(x) __THREAD_ANNOTATION_ATTRIBUTE__(assert_shared_capability(x))
#define RETURN_CAPABILITY(x) __THREAD_ANNOTATION_ATTRIBUTE__(lock_returned(x))
#define NO_THREAD_SAFETY_ANALYSIS __THREAD_ANNOTATION_ATTRIBUTE__(no_thread_safety_analysis)
