
#ifndef STRING_VIEW_H_
#define STRING_VIEW_H_

#include "Raise.h"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <functional>
#include <iosfwd>
#include <iterator>
#include <stdexcept>
#include <string>

namespace view {

template <class Ch, class Tr = std::char_traits<Ch>>
class basic_string_view {
public:
	using traits_type            = Tr;
	using value_type             = Ch;
	using pointer                = Ch *;
	using const_pointer          = const Ch *;
	using reference              = Ch &;
	using const_reference        = const Ch &;
	using const_iterator         = const_pointer;
	using iterator               = const_iterator;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;
	using reverse_iterator       = const_reverse_iterator;
	using size_type              = std::size_t;
	using difference_type        = std::ptrdiff_t;

public:
	static constexpr auto npos = size_type(-1);

public:
	constexpr basic_string_view() noexcept
		: data_(nullptr) {
	}

	constexpr basic_string_view(const basic_string_view &other) noexcept = default;

	template <class A>
	basic_string_view(const std::basic_string<Ch, Tr, A> &str) noexcept
		: data_(str.data()), size_(str.size()) {
	}

	constexpr basic_string_view(const Ch *str, size_type len)
		: data_(str), size_(len) {
	}

	constexpr basic_string_view(const Ch *str)
		: data_(str), size_(Tr::length(str)) {
	}

	basic_string_view &operator=(const basic_string_view &rhs) noexcept = default;

public:
	constexpr const_iterator begin() const noexcept { return data_; }
	constexpr const_iterator cbegin() const noexcept { return data_; }
	constexpr const_iterator end() const noexcept { return data_ + size_; }
	constexpr const_iterator cend() const noexcept { return data_ + size_; }

	const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
	const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }
	const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
	const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

public:
	constexpr const_reference operator[](size_type pos) const noexcept {
		return data_[pos];
	}

	constexpr const_reference at(size_t pos) const {
		return pos >= size_ ? Raise<std::out_of_range>("string_view::at"), data_[0] : data_[pos];
	}

	constexpr const_reference front() const {
		return data_[0];
	}

	constexpr const_reference back() const {
		return data_[size_ - 1];
	}

	constexpr const_pointer data() const noexcept {
		return data_;
	}

public:
	constexpr size_type size() const noexcept {
		return size_;
	}

	constexpr size_type length() const noexcept {
		return size_;
	}

	constexpr size_type max_size() const noexcept {
		return size_type(-1) / sizeof(Ch);
	}

	constexpr bool empty() const noexcept {
		return size_ == 0;
	}

public:
	constexpr void remove_prefix(size_type n) {
		if (n > size_) {
			n = size_;
		}

		data_ += n;
		size_ -= n;
	}

	constexpr void remove_suffix(size_type n) {
		if (n > size_) {
			n = size_;
		}

		size_ -= n;
	}

	constexpr void swap(basic_string_view &s) noexcept {
		using std::swap;
		swap(data_, s.data_);
		swap(size_, s.size_);
	}

public:
	template <class A>
	explicit operator std::basic_string<Ch, Tr, A>() const {
		return std::basic_string<Ch, Tr, A>(begin(), end());
	}

	template <class A = std::allocator<Ch>>
	std::basic_string<Ch, Tr, A> to_string(const A &a = A()) const {
		return std::basic_string<Ch, Tr, A>(begin(), end(), a);
	}

public:
	size_type copy(Ch *dest, size_type count, size_type pos = 0) const {

		using std::min;

		if (pos > size()) {
			Raise<std::out_of_range>("string_view::copy");
		}

		size_type rlen = min(count, size_ - pos);
		Tr::copy(dest, data() + pos, rlen);
		return rlen;
	}

	constexpr basic_string_view substr(size_type pos, size_type n = npos) const {

		using std::min;

		if (pos > size()) {
			Raise<std::out_of_range>("string_view::substr");
		}

		return basic_string_view(data() + pos, min(size() - pos, n));
	}

public:
	constexpr int compare(basic_string_view x) const noexcept {
		const int cmp = Tr::compare(data_, x.data_, (std::min)(size_, x.size_));
		return cmp != 0 ? cmp : (size_ == x.size_ ? 0 : size_ < x.size_ ? -1 : 1);
	}

	constexpr int compare(size_type pos1, size_type n1, basic_string_view x) const noexcept {
		return substr(pos1, n1).compare(x);
	}

	constexpr int compare(size_type pos1, size_type n1, basic_string_view x, size_type pos2, size_type n2) const {
		return substr(pos1, n1).compare(x.substr(pos2, n2));
	}

	constexpr int compare(const Ch *x) const {
		return compare(basic_string_view(x));
	}

	constexpr int compare(size_type pos1, size_type n1, const Ch *x) const {
		return substr(pos1, n1).compare(basic_string_view(x));
	}

	constexpr int compare(size_type pos1, size_type n1, const Ch *x, size_type n2) const {
		return substr(pos1, n1).compare(basic_string_view(x, n2));
	}

public:
	constexpr size_type find(basic_string_view s, size_type pos = 0) const noexcept {
		if (pos > size()) {
			return npos;
		}

		if (s.empty()) {
			return pos;
		}

		auto it = std::search(cbegin() + pos, cend(), s.cbegin(), s.cend(), Tr::eq);
		return it == cend() ? npos : std::distance(cbegin(), it);
	}

	constexpr size_type find(Ch c, size_type pos = 0) const noexcept {
		return find(basic_string_view(&c, 1), pos);
	}

	constexpr size_type find(const Ch *s, size_type pos, size_type n) const noexcept {
		return find(basic_string_view(s, n), pos);
	}

	constexpr size_type find(const Ch *s, size_type pos = 0) const noexcept {
		return find(basic_string_view(s), pos);
	}

public:
	constexpr size_type rfind(basic_string_view s, size_type pos = npos) const noexcept {
		if (size_ < s.size_) {
			return npos;
		}

		if (pos > size_ - s.size_) {
			pos = size_ - s.size_;
		}

		if (s.size_ == 0u) {
			return pos;
		}

		for (const Ch *cur = data_ + pos;; --cur) {
			if (Tr::compare(cur, s.data_, s.size_) == 0) {
				return cur - data_;
			}

			if (cur == data_) {
				return npos;
			}
		};
	}

	constexpr size_type rfind(Ch c, size_type pos = npos) const noexcept {
		return rfind(basic_string_view(&c, 1), pos);
	}

	constexpr size_type rfind(const Ch *s, size_type pos, size_type n) const noexcept {
		return rfind(basic_string_view(s, n), pos);
	}

	constexpr size_type rfind(const Ch *s, size_type pos = npos) const noexcept {
		return rfind(basic_string_view(s), pos);
	}

public:
	constexpr size_type find_first_of(basic_string_view s, size_type pos = 0) const noexcept {
		if (pos >= size_ || s.size_ == 0) {
			return npos;
		}

		auto it = std::find_first_of(cbegin() + pos, cend(), s.cbegin(), s.cend(), Tr::eq);
		return it == cend() ? npos : std::distance(cbegin(), it);
	}

	constexpr size_type find_first_of(Ch c, size_type pos = 0) const noexcept {
		return find_first_of(basic_string_view(&c, 1), pos);
	}

	constexpr size_type find_first_of(const Ch *s, size_type pos, size_type n) const noexcept {
		return find_first_of(basic_string_view(s, n), pos);
	}

	constexpr size_type find_first_of(const Ch *s, size_type pos = 0) const noexcept {
		return find_first_of(basic_string_view(s), pos);
	}

public:
	constexpr size_type find_last_of(basic_string_view s, size_type pos = npos) const noexcept {
		if (s.size_ == 0u) {
			return npos;
		}

		if (pos >= size_) {
			pos = 0;
		} else {
			pos = size_ - (pos + 1);
		}

		auto it = std::find_first_of(crbegin() + pos, crend(), s.cbegin(), s.cend(), Tr::eq);
		return it == crend() ? npos : reverse_distance(crbegin(), it);
	}

	constexpr size_type find_last_of(Ch c, size_type pos = npos) const noexcept {
		return find_last_of(basic_string_view(&c, 1), pos);
	}

	constexpr size_type find_last_of(const Ch *s, size_type pos, size_type n) const noexcept {
		return find_last_of(basic_string_view(s, n), pos);
	}

	constexpr size_type find_last_of(const Ch *s, size_type pos = npos) const noexcept {
		return find_last_of(basic_string_view(s), pos);
	}

public:
	constexpr size_type find_first_not_of(basic_string_view s, size_type pos = 0) const noexcept {
		if (pos >= size_) {
			return npos;
		}

		if (s.size_ == 0) {
			return pos;
		}

		auto it = find_not_of(cbegin() + pos, cend(), s);
		return it == cend() ? npos : std::distance(cbegin(), it);
	}

	constexpr size_type find_first_not_of(Ch c, size_type pos = 0) const noexcept {
		return find_first_not_of(basic_string_view(&c, 1), pos);
	}

	constexpr size_type find_first_not_of(const Ch *s, size_type pos, size_type n) const noexcept {
		return find_first_not_of(basic_string_view(s, n), pos);
	}

	constexpr size_type find_first_not_of(const Ch *s, size_type pos = 0) const noexcept {
		return find_first_not_of(basic_string_view(s), pos);
	}

public:
	constexpr size_type find_last_not_of(basic_string_view s, size_type pos = npos) const noexcept {
		if (pos >= size_) {
			pos = size_ - 1;
		}

		if (s.size_ == 0u) {
			return pos;
		}

		pos     = size_ - (pos + 1);
		auto it = find_not_of(crbegin() + pos, crend(), s);
		return it == crend() ? npos : reverse_distance(crbegin(), it);
	}

	constexpr size_type find_last_not_of(Ch c, size_type pos = npos) const noexcept {
		return find_last_not_of(basic_string_view(&c, 1), pos);
	}

	constexpr size_type find_last_not_of(const Ch *s, size_type pos, size_type n) const noexcept {
		return find_last_not_of(basic_string_view(s, n), pos);
	}

	constexpr size_type find_last_not_of(const Ch *s, size_type pos = npos) const noexcept {
		return find_last_not_of(basic_string_view(s), pos);
	}

private:
	template <typename It>
	size_type reverse_distance(It first, It last) const noexcept {
		return size_ - 1 - std::distance(first, last);
	}

	template <typename It>
	It find_not_of(It first, It last, basic_string_view s) const noexcept {
		for (; first != last; ++first) {
			if (0 == Tr::find(s.data_, s.size_, *first)) {
				return first;
			}
		}
		return last;
	}

private:
	const Ch *data_;
	std::size_t size_ = 0;
};

template <class Ch, class Tr>
constexpr bool operator==(basic_string_view<Ch, Tr> lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	if (lhs.size() != rhs.size())
		return false;
	return lhs.compare(rhs) == 0;
}

template <class Ch, class Tr>
constexpr bool operator!=(basic_string_view<Ch, Tr> lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	if (lhs.size() != rhs.size())
		return true;
	return lhs.compare(rhs) != 0;
}

//  Less than
template <class Ch, class Tr>
constexpr bool operator<(basic_string_view<Ch, Tr> lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return lhs.compare(rhs) < 0;
}

//  Greater than
template <class Ch, class Tr>
constexpr bool operator>(basic_string_view<Ch, Tr> lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return lhs.compare(rhs) > 0;
}

//  Less than or equal to
template <class Ch, class Tr>
constexpr bool operator<=(basic_string_view<Ch, Tr> lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return lhs.compare(rhs) <= 0;
}

//  Greater than or equal to
template <class Ch, class Tr>
constexpr bool operator>=(basic_string_view<Ch, Tr> lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return lhs.compare(rhs) >= 0;
}

// "sufficient additional overloads of comparison functions"
template <class Ch, class Tr, class A>
constexpr bool operator==(basic_string_view<Ch, Tr> lhs, const std::basic_string<Ch, Tr, A> &rhs) noexcept {
	return lhs == basic_string_view<Ch, Tr>(rhs);
}

template <class Ch, class Tr, class A>
constexpr bool operator==(const std::basic_string<Ch, Tr, A> &lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return basic_string_view<Ch, Tr>(lhs) == rhs;
}

template <class Ch, class Tr>
constexpr bool operator==(basic_string_view<Ch, Tr> lhs, const Ch *rhs) noexcept {
	return lhs == basic_string_view<Ch, Tr>(rhs);
}

template <class Ch, class Tr>
constexpr bool operator==(const Ch *lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return basic_string_view<Ch, Tr>(lhs) == rhs;
}

template <class Ch, class Tr, class A>
constexpr bool operator!=(basic_string_view<Ch, Tr> lhs, const std::basic_string<Ch, Tr, A> &rhs) noexcept {
	return lhs != basic_string_view<Ch, Tr>(rhs);
}

template <class Ch, class Tr, class A>
constexpr bool operator!=(const std::basic_string<Ch, Tr, A> &lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return basic_string_view<Ch, Tr>(lhs) != rhs;
}

template <class Ch, class Tr>
constexpr bool operator!=(basic_string_view<Ch, Tr> lhs, const Ch *rhs) noexcept {
	return lhs != basic_string_view<Ch, Tr>(rhs);
}

template <class Ch, class Tr>
constexpr bool operator!=(const Ch *lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return basic_string_view<Ch, Tr>(lhs) != rhs;
}

template <class Ch, class Tr, class A>
constexpr bool operator<(basic_string_view<Ch, Tr> lhs, const std::basic_string<Ch, Tr, A> &rhs) noexcept {
	return lhs < basic_string_view<Ch, Tr>(rhs);
}

template <class Ch, class Tr, class A>
constexpr bool operator<(const std::basic_string<Ch, Tr, A> &lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return basic_string_view<Ch, Tr>(lhs) < rhs;
}

template <class Ch, class Tr>
constexpr bool operator<(basic_string_view<Ch, Tr> lhs, const Ch *rhs) noexcept {
	return lhs < basic_string_view<Ch, Tr>(rhs);
}

template <class Ch, class Tr>
constexpr bool operator<(const Ch *lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return basic_string_view<Ch, Tr>(lhs) < rhs;
}

template <class Ch, class Tr, class A>
constexpr bool operator>(basic_string_view<Ch, Tr> lhs, const std::basic_string<Ch, Tr, A> &rhs) noexcept {
	return lhs > basic_string_view<Ch, Tr>(rhs);
}

template <class Ch, class Tr, class A>
constexpr bool operator>(const std::basic_string<Ch, Tr, A> &lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return basic_string_view<Ch, Tr>(lhs) > rhs;
}

template <class Ch, class Tr>
constexpr bool operator>(basic_string_view<Ch, Tr> lhs, const Ch *rhs) noexcept {
	return lhs > basic_string_view<Ch, Tr>(rhs);
}

template <class Ch, class Tr>
constexpr bool operator>(const Ch *lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return basic_string_view<Ch, Tr>(lhs) > rhs;
}

template <class Ch, class Tr, class A>
constexpr bool operator<=(basic_string_view<Ch, Tr> lhs, const std::basic_string<Ch, Tr, A> &rhs) noexcept {
	return lhs <= basic_string_view<Ch, Tr>(rhs);
}

template <class Ch, class Tr, class A>
constexpr bool operator<=(const std::basic_string<Ch, Tr, A> &lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return basic_string_view<Ch, Tr>(lhs) <= rhs;
}

template <class Ch, class Tr>
constexpr bool operator<=(basic_string_view<Ch, Tr> lhs, const Ch *rhs) noexcept {
	return lhs <= basic_string_view<Ch, Tr>(rhs);
}

template <class Ch, class Tr>
constexpr bool operator<=(const Ch *lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return basic_string_view<Ch, Tr>(lhs) <= rhs;
}

template <class Ch, class Tr, class A>
constexpr bool operator>=(basic_string_view<Ch, Tr> lhs, const std::basic_string<Ch, Tr, A> &rhs) noexcept {
	return lhs >= basic_string_view<Ch, Tr>(rhs);
}

template <class Ch, class Tr, class A>
constexpr bool operator>=(const std::basic_string<Ch, Tr, A> &lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return basic_string_view<Ch, Tr>(lhs) >= rhs;
}

template <class Ch, class Tr>
constexpr bool operator>=(basic_string_view<Ch, Tr> lhs, const Ch *rhs) noexcept {
	return lhs >= basic_string_view<Ch, Tr>(rhs);
}

template <class Ch, class Tr>
constexpr bool operator>=(const Ch *lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return basic_string_view<Ch, Tr>(lhs) >= rhs;
}

using string_view    = basic_string_view<char>;
using wstring_view   = basic_string_view<wchar_t>;
using u16string_view = basic_string_view<char16_t>;
using u32string_view = basic_string_view<char32_t>;
}

// NOTE(eteran): see https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
// for more details on this algorithm
namespace detail {

template <size_t N>
struct hash_constants_impl;

template <>
struct hash_constants_impl<8> {
	static constexpr uint64_t FNV_offset_basis = 0xcbf29ce484222325ull;
	static constexpr uint64_t FNV_prime        = 1099511628211ull;
};

template <>
struct hash_constants_impl<4> {
	static constexpr uint32_t FNV_offset_basis = 0x811c9dc5;
	static constexpr uint32_t FNV_prime        = 16777619;
};

template <class T>
struct hash_constants : hash_constants_impl<sizeof(T)> {
};

}

namespace std {

template <>
struct hash<view::string_view> {
	using argument_type = view::string_view;
	using result_type   = size_t;

	result_type operator()(argument_type key) const {

		result_type h = detail::hash_constants<result_type>::FNV_offset_basis;
		for (char ch : key) {
			h = (h * detail::hash_constants<result_type>::FNV_prime) ^ static_cast<unsigned char>(ch);
		}
		return h;
	}
};

template <>
struct hash<view::wstring_view> {
	using argument_type = view::wstring_view;
	using result_type   = size_t;

	result_type operator()(argument_type key) const {
		auto p    = reinterpret_cast<const char *>(&*key.begin());
		auto last = reinterpret_cast<const char *>(&*key.end());

		result_type h = detail::hash_constants<result_type>::FNV_offset_basis;
		while (p != last) {
			h = (h * detail::hash_constants<result_type>::FNV_prime) ^ static_cast<unsigned char>(*p++);
		}
		return h;
	}
};

template <>
struct hash<view::u16string_view> {
	using argument_type = view::u16string_view;
	using result_type   = size_t;

	result_type operator()(argument_type key) const {
		auto p    = reinterpret_cast<const char *>(&*key.begin());
		auto last = reinterpret_cast<const char *>(&*key.end());

		result_type h = detail::hash_constants<result_type>::FNV_offset_basis;
		while (p != last) {
			h = (h * detail::hash_constants<result_type>::FNV_prime) ^ static_cast<unsigned char>(*p++);
		}
		return h;
	}
};

template <>
struct hash<view::u32string_view> {
	using argument_type = view::u32string_view;
	using result_type   = size_t;

	result_type operator()(argument_type key) const {
		auto p    = reinterpret_cast<const char *>(&*key.begin());
		auto last = reinterpret_cast<const char *>(&*key.end());

		result_type h = detail::hash_constants<result_type>::FNV_offset_basis;
		while (p != last) {
			h = (h * detail::hash_constants<result_type>::FNV_prime) ^ static_cast<unsigned char>(*p++);
		}
		return h;
	}
};

}

#endif
