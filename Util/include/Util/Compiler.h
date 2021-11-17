
#ifndef COMPILER_H_
#define COMPILER_H_

#include <QtGlobal>

#if __cplusplus > 202002L
#define HAS_CXX20
#endif

#if __cplusplus >= 201703L
#define HAS_CXX17
#endif

#if __cplusplus >= 201402L
#define HAS_CXX14
#endif

#if __cplusplus >= 201103L
#define HAS_CXX11
#endif

#if __cplusplus >= 199711L
#define HAS_CXX03
#define HAS_CXX89
#endif

#if defined(__GNUC__)
#define COLD_CODE __attribute__((noinline, cold))
#define FORCE_INLINE __attribute__((always_inline)) inline
#define NO_DISCARD __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
#define COLD_CODE
#define FORCE_INLINE __forceinline
#define NO_DISCARD
#endif

#ifdef HAS_CXX17
#define NEDIT_FALLTHROUGH() [[fallthrough]]
#else
#ifdef Q_FALLTHROUGH
#define NEDIT_FALLTHROUGH() Q_FALLTHROUGH()
#else
#define NEDIT_FALLTHROUGH() (void)0
#endif
#endif

#endif
