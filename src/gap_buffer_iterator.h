
#ifndef GAP_BUFFER_ITERATOR_H_
#define GAP_BUFFER_ITERATOR_H_

#include "gap_buffer_fwd.h"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <type_traits>

template <class Ch, class Tr, bool IsConst>
class gap_buffer_iterator {
	using buffer_type = typename std::conditional<IsConst, const gap_buffer<Ch, Tr>, gap_buffer<Ch, Tr>>::type;
	using size_type   = typename buffer_type::size_type;

	template <class CharT, class Traits, bool Const>
	friend class gap_buffer_iterator;

public:
	using difference_type   = std::ptrdiff_t;
	using iterator_category = std::random_access_iterator_tag;
	using pointer           = Ch *;
	using reference         = Ch &;
	using value_type        = Ch;

public:
	gap_buffer_iterator() = default;
	gap_buffer_iterator(buffer_type *buf, size_type pos)
		: buf_(buf), pos_(pos) {}

public:
	// for construction of a const-iterator from a non-const iterator
	// These only exist for the const version
	template <bool Const = IsConst, class = typename std::enable_if<Const>::type>
	gap_buffer_iterator(const gap_buffer_iterator<Ch, Tr, false> &other)
		: buf_(other.buf_), pos_(other.pos_) {}

	template <bool Const = IsConst, class = typename std::enable_if<Const>::type>
	gap_buffer_iterator &operator=(const gap_buffer_iterator<Ch, Tr, false> &rhs) {
		gap_buffer_iterator(rhs).swap(*this);
		return *this;
	}

public:
	gap_buffer_iterator(const gap_buffer_iterator &rhs)         = default;
	gap_buffer_iterator &operator=(const gap_buffer_iterator &) = default;

public:
	gap_buffer_iterator &operator+=(difference_type rhs) {
		pos_ += rhs;
		return *this;
	}
	gap_buffer_iterator &operator-=(difference_type rhs) {
		pos_ -= rhs;
		return *this;
	}

public:
	gap_buffer_iterator &operator++() {
		++pos_;
		return *this;
	}
	gap_buffer_iterator &operator--() {
		--pos_;
		return *this;
	}
	gap_buffer_iterator operator++(int) {
		gap_buffer_iterator tmp(*this);
		++pos_;
		return tmp;
	}
	gap_buffer_iterator operator--(int) {
		gap_buffer_iterator tmp(*this);
		--pos_;
		return tmp;
	}

public:
	gap_buffer_iterator operator+(difference_type rhs) const { return gap_buffer_iterator(pos_ + rhs); }
	gap_buffer_iterator operator-(difference_type rhs) const { return gap_buffer_iterator(pos_ - rhs); }

public:
	difference_type operator-(const gap_buffer_iterator &rhs) const {
		assert(buf_ == rhs.buf_);
		return pos_ - rhs.pos_;
	}
	friend gap_buffer_iterator operator+(difference_type lhs, const gap_buffer_iterator &rhs) { return gap_buffer_iterator(lhs + rhs.pos_); }
	friend gap_buffer_iterator operator-(difference_type lhs, const gap_buffer_iterator &rhs) { return gap_buffer_iterator(lhs - rhs.pos_); }

public:
	reference operator*() const { return buf_[pos_]; }
	reference operator[](difference_type offset) const { return buf_[pos_ + offset]; }
	pointer operator->() const { return &buf_[pos_]; }

public:
	void swap(gap_buffer_iterator &other) {
		using std::swap;
		swap(pos_, other.pos_);
		swap(buf_, other.buf_);
	}

public:
	// templated to allow comparison between const/non-const iterators
	template <class CharT, class Traits, bool Const>
	bool operator==(const gap_buffer_iterator<CharT, Traits, Const> &rhs) const {
		assert(buf_ == rhs.buf_);
		return pos_ == rhs.pos_;
	}

	template <class CharT, class Traits, bool Const>
	bool operator!=(const gap_buffer_iterator<CharT, Traits, Const> &rhs) const {
		assert(buf_ == rhs.buf_);
		return pos_ != rhs.pos_;
	}

	template <class CharT, class Traits, bool Const>
	bool operator>(const gap_buffer_iterator<CharT, Traits, Const> &rhs) const {
		assert(buf_ == rhs.buf_);
		return pos_ > rhs.pos_;
	}

	template <class CharT, class Traits, bool Const>
	bool operator<(const gap_buffer_iterator<CharT, Traits, Const> &rhs) const {
		assert(buf_ == rhs.buf_);
		return pos_ < rhs.pos_;
	}

	template <class CharT, class Traits, bool Const>
	bool operator>=(const gap_buffer_iterator<CharT, Traits, Const> &rhs) const {
		assert(buf_ == rhs.buf_);
		return pos_ >= rhs.pos_;
	}

	template <class CharT, class Traits, bool Const>
	bool operator<=(const gap_buffer_iterator<CharT, Traits, Const> &rhs) const {
		assert(buf_ == rhs.buf_);
		return pos_ <= rhs.pos_;
	}

private:
	buffer_type *buf_ = nullptr;
	size_type pos_    = 0;
};

#endif
