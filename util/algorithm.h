
#ifndef ALGORITHM_H_
#define ALGORITHM_H_

#include <algorithm>
#include "string_view.h"

// container algorithms

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


// string_view algorithms

template <class Ch, class Tr>
constexpr view::basic_string_view<Ch, Tr> substr(const view::basic_string_view<Ch, Tr> &str, typename view::basic_string_view<Ch, Tr>::size_type pos, typename view::basic_string_view<Ch, Tr>::size_type count = view::basic_string_view<Ch, Tr>::npos) {
    return str.substr(pos, count);
}

template <class Ch, class Tr, class A>
constexpr view::basic_string_view<Ch, Tr> substr(typename std::basic_string<Ch, Tr, A>::const_iterator first, typename std::basic_string<Ch, Tr, A>::const_iterator last) {

    Ch *data = &*first;
    typename view::basic_string_view<Ch, Tr>::size_type size = std::distance(first, last);

    return view::basic_string_view<Ch, Tr>(data, size);
}

template <class Ch, class Tr = std::char_traits<Ch>>
constexpr view::basic_string_view<Ch, Tr> substr(const Ch *first, const Ch *last) {

    const Ch *data = first;
    typename view::basic_string_view<Ch, Tr>::size_type size = std::distance(first, last);

    return view::basic_string_view<Ch, Tr>(data, size);
}

template <class Ch, class Tr = std::char_traits<Ch>>
constexpr bool starts_with(view::basic_string_view<Ch, Tr> str, Ch ch) noexcept {
    return !str.empty() && Tr::eq(ch, str.front());
}

template <class Ch, class Tr = std::char_traits<Ch>>
constexpr bool starts_with(view::basic_string_view<Ch, Tr> str, view::basic_string_view<Ch, Tr> other) noexcept {
    return str.size_ >= other.size_ && Tr::compare(str.data_, other.data_, other.size_) == 0;
}

template <class Ch, class Tr = std::char_traits<Ch>>
constexpr bool ends_with(view::basic_string_view<Ch, Tr> str, Ch ch) noexcept {
    return !str.empty() && Tr::eq(ch, str.back());
}

template <class Ch, class Tr = std::char_traits<Ch>>
constexpr bool ends_with(view::basic_string_view<Ch, Tr> str, view::basic_string_view<Ch, Tr> other) noexcept {
    return str.size_ >= other.size_ && Tr::compare(str.data_ + str.size_ - other.size_, other.data_, other.size_) == 0;
}

#endif
