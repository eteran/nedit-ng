
#ifndef RAISE_H_
#define RAISE_H_

#include <utility>

template <class E, class ... Args>
constexpr void
#ifdef __GNUC__
__attribute__(( noinline, cold, noreturn ))
#endif
raise(Args && ...args) {
	throw E(std::forward<Args>(args)...);
}

#endif
