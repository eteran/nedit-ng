
#ifndef UTIL_RAISE_H_
#define UTIL_RAISE_H_

#include "Compiler.h"
#include <utility>

template <class E, class... Args>
[[noreturn]]
COLD_CODE constexpr void Raise(Args &&... args) {
	throw E{std::forward<Args>(args)...};
}

#endif
