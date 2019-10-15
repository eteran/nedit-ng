
#ifndef COMPILER_H_
#define COMPILER_H_

#include <QtGlobal>

#if defined(__GNUC__)
#define COLD_CODE __attribute__((noinline, cold))
#define NO_RETURN __attribute__((noreturn))
#define FORCE_INLINE __attribute__((always_inline))
#elif defined(_MSC_VER)
#define COLD_CODE
#define NO_RETURN __declspec(noreturn)
#define FORCE_INLINE __forceinline
#endif

#ifdef Q_FALLTHROUGH
#define NEDIT_FALLTHROUGH() Q_FALLTHROUGH()
#else
#define NEDIT_FALLTHROUGH() (void)0
#endif

#endif
