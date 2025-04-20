
#ifndef ALGORITHM_H_
#define ALGORITHM_H_

#include <QtGlobal>

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <type_traits>

// container algorithms

// this warning is pretty dumb in this context, the compiler inlines this
// function and notices that MoveItem(c, row, row + 1) means that from will
// ALWAYS be less than to, which is good... but it's dumb to warn about
// because it's only true in that particular expansion, but not all
QT_WARNING_PUSH
QT_WARNING_DISABLE_GCC("-Wstrict-overflow")

/**
 * @brief Moves an item in a container from one position to another.
 *
 * @param cont The container to operate on, which must support random access iterators.
 * @param from The index of the item to move.
 * @param to The index to which the item should be moved.
 */
template <class Cont>
void MoveItem(Cont &cont, int from, int to) {

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

/**
 * @brief Inserts an item into a container if it does not already exist, or updates it if it does.
 *
 * @param cont The container to operate on.
 * @param item The item to insert or update in the container.
 * @param pred A predicate function that determines if an item in the container matches the item to be inserted.
 */
template <class Cont, class T, class Pred>
void Upsert(Cont &cont, T &&item, Pred &&pred) {

	auto it = std::find_if(cont.begin(), cont.end(), pred);
	if (it != cont.end()) {
		*it = item;
	} else {
		cont.emplace_back(item);
	}
}

/**
 * @brief Returns the size of a container as a signed integer type.
 *
 * @param c The container whose size is to be determined.
 * @return The size of the container as a signed integer type.
 */
template <class C>
constexpr auto ssize(const C &c) -> std::common_type_t<ptrdiff_t, std::make_signed_t<decltype(c.size())>> {
	using R = std::common_type_t<ptrdiff_t, std::make_signed_t<decltype(c.size())>>;
	return static_cast<R>(c.size());
}

/**
 * @brief Returns the size of a fixed-size array as a signed integer type.
 *
 * @param arr The fixed-size array whose size is to be determined.
 * @return The size of the array as a signed integer type.
 */
template <class T, ptrdiff_t N>
constexpr ptrdiff_t ssize(const T (&)[N]) noexcept {
	return N;
}

#endif
