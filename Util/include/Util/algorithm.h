
#ifndef ALGORITHM_H_
#define ALGORITHM_H_

#include "string_view.h"
#include <algorithm>
#include <QtGlobal>

// container algorithms

// this warning is pretty dumb in this context, the compiler inlines this
// function and notices that moveItem(c, row, row + 1) means that from will
// ALWAYS be less than to, which is good... but it's dumb to warn about
// because it's only true in that particular expansion, but not all
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-overflow"
#endif
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
#ifdef __GNUC__
#pragma GCC diagnostic push
#endif


// string_view algorithms
template <class Ch, class Tr = std::char_traits<Ch>>
constexpr view::basic_string_view<Ch, Tr> substr(const Ch *first, const Ch *last) {

	const Ch *data = first;
	auto size = std::distance(first, last);
	return view::basic_string_view<Ch, Tr>(data, static_cast<size_t>(size));
}

#endif
