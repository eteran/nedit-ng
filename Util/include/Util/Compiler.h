
#ifndef COMPILER_H_
#define COMPILER_H_

#if defined(__GNUC__)
#define COLD_CODE __attribute__((noinline, cold))
#define NO_RETURN __attribute__((noreturn))
#define FORCE_INLINE __attribute__((always_inline))
#elif defined(_MSC_VER)
#define COLD_CODE
#define NO_RETURN __declspec(noreturn)
#define FORCE_INLINE __forceinline
#endif

#endif
