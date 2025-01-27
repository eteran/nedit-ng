
#ifndef ALGORITHM_H_
#define ALGORITHM_H_

#include <QtGlobal>
#include <algorithm>
#include <cstddef>
#include <string_view>
#include <type_traits>

// container algorithms

// this warning is pretty dumb in this context, the compiler inlines this
// function and notices that moveItem(c, row, row + 1) means that from will
// ALWAYS be less than to, which is good... but it's dumb to warn about
// because it's only true in that particular expansion, but not all
QT_WARNING_PUSH
QT_WARNING_DISABLE_GCC("-Wstrict-overflow")

template <class Cont>
void moveItem(Cont &cont, int from, int to) {

	Q_ASSERT(from >= 0 && from < cont.size());
	Q_ASSERT(to >= 0 && to < cont.size());

	if (from == to) // don't detach when no-op
		return;

	auto b = cont.begin();
	if (from < to) {
		std::rotate(b + from, b + from + 1, b + to + 1);
	} else {
		std::rotate(b + to, b + from, b + from + 1);
	}
}

QT_WARNING_POP

template <class Cont, class T, class Pred>
void insert_or_replace(Cont &cont, T &&item, Pred &&pred) {

	auto it = std::find_if(cont.begin(), cont.end(), pred);
	if (it != cont.end()) {
		*it = item;
	} else {
		cont.emplace_back(item);
	}
}

template <class C>
constexpr auto ssize(const C &c) -> std::common_type_t<ptrdiff_t, std::make_signed_t<decltype(c.size())>> {
	using R = std::common_type_t<ptrdiff_t, std::make_signed_t<decltype(c.size())>>;
	return static_cast<R>(c.size());
}

template <class T, ptrdiff_t N>
constexpr ptrdiff_t ssize(const T (&)[N]) noexcept {
	return N;
}

#endif
