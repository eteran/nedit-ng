
#ifndef COMPILER_H_
#define COMPILER_H_

#if defined(__GNUC__)
#define COLD_CODE __attribute__((noinline, cold))
#define FORCE_INLINE __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
#define COLD_CODE
#define FORCE_INLINE __forceinline
#endif

#endif
