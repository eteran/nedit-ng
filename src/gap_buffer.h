
#ifndef GAP_BUFFER_H_
#define GAP_BUFFER_H_

#include "Util/Raise.h"
#include "gap_buffer_fwd.h"
#include "gap_buffer_iterator.h"

#include <algorithm>
#include <cassert>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

template <class Ch, class Tr>
class gap_buffer {
public:
	static constexpr int PreferredGapSize = 80;
	using string_type                     = std::basic_string<Ch, Tr>;
	using view_type                       = std::basic_string_view<Ch, Tr>;

public:
	using value_type             = typename std::allocator<Ch>::value_type;
	using allocator_type         = std::allocator<Ch>;
	using size_type              = int64_t; // NOTE(eteran): typically unsigned...
	using difference_type        = typename std::allocator<Ch>::difference_type;
	using reference              = Ch &;
	using const_reference        = const Ch &;
	using pointer                = Ch *;
	using const_pointer          = const Ch *;
	using iterator               = gap_buffer_iterator<Ch, Tr, false>;
	using const_iterator         = gap_buffer_iterator<Ch, Tr, true>;
	using reverse_iterator       = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

public:
	gap_buffer();
	explicit gap_buffer(size_type reserve_size);
	gap_buffer(const gap_buffer &)            = delete;
	gap_buffer &operator=(const gap_buffer &) = delete;
	gap_buffer(gap_buffer &&)                 = delete;
	gap_buffer &operator=(gap_buffer &&)      = delete;
	~gap_buffer()                             = default;

public:
	iterator begin() noexcept { return iterator(this, 0); }
	iterator end() noexcept { return iterator(this, size()); }
	const_iterator begin() const noexcept { return const_iterator(this, 0); }
	const_iterator end() const noexcept { return const_iterator(this, size()); }
	const_iterator cbegin() const noexcept { return const_iterator(this, 0); }
	const_iterator cend() const noexcept { return const_iterator(this, size()); }

	reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
	reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
	const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
	const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
	const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }
	const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

public:
	size_type gap_size() const noexcept { return gap_end_ - gap_start_; }
	size_type size() const noexcept { return size_; }
	size_type capacity() const noexcept { return size_ + gap_end_ - gap_start_; }
	bool empty() const noexcept { return size() == 0; }
	void swap(gap_buffer &other) noexcept;

public:
	Ch operator[](size_type n) const noexcept;
	Ch &operator[](size_type n) noexcept;
	Ch at(size_type n) const;
	Ch &at(size_type n);

public:
	int compare(size_type pos, view_type str) const noexcept;
	int compare(size_type pos, Ch ch) const noexcept;

public:
	string_type to_string() const;
	string_type to_string(size_type start, size_type end) const;
	view_type to_view() noexcept;
	view_type to_view(size_type start, size_type end) noexcept;

public:
	void append(view_type str);
	void append(Ch ch);
	void insert(size_type pos, view_type str);
	void insert(size_type pos, Ch ch);
	size_type erase(size_type start, size_type end) noexcept;
	void replace(size_type start, size_type end, view_type str);
	void replace(size_type start, size_type end, Ch ch);
	void assign(view_type str);
	void clear() noexcept;

private:
	void move_gap(size_type pos) noexcept;
	void reallocate_buffer(size_type new_gap_start, size_type new_gap_size);
	void delete_range(size_type start, size_type end) noexcept;

private:
	std::unique_ptr<Ch[]> buf_; // points to the internal buffer
	size_type gap_start_;       // points to the first character of the gap
	size_type gap_end_;         // points to the first char after the gap
	size_type size_;            // length of the text in the buffer (the length of the buffer itself must be calculated: gapEnd - gapStart + length)
};

/**
 *
 */
template <class Ch, class Tr>
gap_buffer<Ch, Tr>::gap_buffer()
	: gap_buffer(0) {
}

/**
 *
 */
template <class Ch, class Tr>
gap_buffer<Ch, Tr>::gap_buffer(size_type reserve_size)
	: gap_start_(0), gap_end_(PreferredGapSize), size_(0) {

	buf_ = std::make_unique<Ch[]>(reserve_size + PreferredGapSize);

#ifdef PURIFY
	std::fill(&buf_[gap_start_], &buf_[gap_end_], Ch('.'));
#endif
}

/**
 *
 */
template <class Ch, class Tr>
Ch gap_buffer<Ch, Tr>::operator[](size_type n) const noexcept {

	if (n < gap_start_) {
		return buf_[n];
	}

	return buf_[n + gap_size()];
}

/**
 *
 */
template <class Ch, class Tr>
Ch &gap_buffer<Ch, Tr>::operator[](size_type n) noexcept {

	if (n < gap_start_) {
		return buf_[n];
	}

	return buf_[n + gap_size()];
}

/**
 *
 */
template <class Ch, class Tr>
Ch gap_buffer<Ch, Tr>::at(size_type n) const {

	if (n >= size() || n < 0) {
		Raise<std::out_of_range>("gap_buffer::at");
	}

	if (n < gap_start_) {
		return buf_[n];
	}

	return buf_[n + gap_size()];
}

/**
 *
 */
template <class Ch, class Tr>
Ch &gap_buffer<Ch, Tr>::at(size_type n) {

	if (n >= size() || n < 0) {
		Raise<std::out_of_range>("gap_buffer::at");
	}

	if (n < gap_start_) {
		return buf_[n];
	}

	return buf_[n + gap_size()];
}

/**
 *
 */
template <class Ch, class Tr>
int gap_buffer<Ch, Tr>::compare(size_type pos, view_type str) const noexcept {

	// NOTE(eteran): could just be:
	// return to_view(pos, pos + str.size()).compare(str);
	// but that would also be slightly less efficient since "to_view" *may* do
	// a gap move. Worth it for the simplicity?

	auto posEnd = pos + static_cast<size_type>(str.size());
	if (posEnd > size()) {
		return 1;
	}

	if (pos < 0) {
		return -1;
	}

	if (posEnd <= gap_start_) {
		return Tr::compare(&buf_[pos], str.data(), str.size());
	}

	if (pos >= gap_start_) {
		return Tr::compare(&buf_[pos + gap_size()], str.data(), str.size());
	}

	const auto part1Length = static_cast<size_t>(gap_start_ - pos);
	const int result       = Tr::compare(&buf_[pos], str.data(), part1Length);
	if (result != 0) {
		return result;
	}

	return Tr::compare(&buf_[gap_end_], &str[part1Length], static_cast<size_t>(str.size() - part1Length));
}

/**
 *
 */
template <class Ch, class Tr>
int gap_buffer<Ch, Tr>::compare(size_type pos, Ch ch) const noexcept {
	if (pos >= size()) {
		return 1;
	}

	if (pos < 0) {
		return -1;
	}

	const Ch buffer_char = (*this)[pos];
	return Tr::compare(&buffer_char, &ch, 1);
}

/**
 *
 */
template <class Ch, class Tr>
auto gap_buffer<Ch, Tr>::to_string() const -> string_type {
	string_type text;
	text.reserve(static_cast<size_t>(size()));
	text.append(&buf_[0], &buf_[gap_start_]);
	text.append(&buf_[gap_end_], &buf_[gap_size() + size()]);
	return text;
}

/**
 *
 */
template <class Ch, class Tr>
auto gap_buffer<Ch, Tr>::to_string(size_type start, size_type end) const -> string_type {
	string_type text;

	assert(start <= size() && start >= 0);
	assert(end <= size() && end >= 0);
	assert(start <= end);

	const difference_type length = end - start;
	text.reserve(static_cast<size_t>(length));

	// Copy the text from the buffer to the returned string
	if (end <= gap_start_) {
		text.append(&buf_[start], &buf_[end]);
	} else if (start >= gap_start_) {
		text.append(&buf_[start + gap_size()], length);
	} else {
		const difference_type part1Length = gap_start_ - start;

		text.append(&buf_[start], part1Length);
		text.append(&buf_[gap_end_], length - part1Length);
	}

	return text;
}

/**
 *
 */
template <class Ch, class Tr>
auto gap_buffer<Ch, Tr>::to_view() noexcept -> view_type {

	const size_type bufLen   = size();
	size_type leftLen        = gap_start_;
	const size_type rightLen = bufLen - leftLen;

	// find where best to put the gap to minimize memory movement
	if (leftLen != 0 && rightLen != 0) {
		leftLen = (leftLen < rightLen) ? 0 : bufLen;
		move_gap(leftLen);
	}

	// get the start position of the actual data
	Ch *const text = &buf_[(leftLen == 0) ? gap_end_ : 0];

	return view_type(text, static_cast<size_t>(bufLen));
}

/**
 *
 */
template <class Ch, class Tr>
auto gap_buffer<Ch, Tr>::to_view(size_type start, size_type end) noexcept -> view_type {

	assert(start <= size() && start >= 0);
	assert(end <= size() && end >= 0);
	assert(start <= end);

	const size_type bufLen   = size();
	size_type leftLen        = gap_start_;
	const size_type rightLen = bufLen - leftLen;

	// find where best to put the gap to minimize memory movement
	if (leftLen != 0 && rightLen != 0) {
		leftLen = (leftLen < rightLen) ? 0 : bufLen;
		move_gap(leftLen);
	}

	// get the start position of the actual data
	Ch *const text = &buf_[(leftLen == 0) ? gap_end_ : 0];

	return view_type(text + start, static_cast<size_t>(end - start));
}

/**
 *
 */
template <class Ch, class Tr>
void gap_buffer<Ch, Tr>::append(view_type str) {
	insert(size(), str);
}

/**
 *
 */
template <class Ch, class Tr>
void gap_buffer<Ch, Tr>::append(Ch ch) {
	insert(size(), ch);
}

/**
 *
 */
template <class Ch, class Tr>
void gap_buffer<Ch, Tr>::insert(size_type pos, view_type str) {

	assert(pos <= size() && pos >= 0);

	const auto length = static_cast<size_type>(str.size());

	/* Prepare the buffer to receive the new text.  If the new text fits in
	   the current buffer, just move the gap (if necessary) to where
	   the text should be inserted.  If the new text is too large, reallocate
	   the buffer with a gap large enough to accommodate the new text and a
	   gap of PreferredGapSize */
	if (length > gap_size()) {
		reallocate_buffer(pos, length + PreferredGapSize);
	} else if (pos != gap_start_) {
		move_gap(pos);
	}

	// Insert the new text (pos now corresponds to the start of the gap)
	std::copy(str.begin(), str.end(), &buf_[pos]);

	gap_start_ += length;
	size_ += length;
}

/**
 *
 */
template <class Ch, class Tr>
void gap_buffer<Ch, Tr>::insert(size_type pos, Ch ch) {

	assert(pos <= size() && pos >= 0);

	const size_type length = 1;

	/* Prepare the buffer to receive the new text.  If the new text fits in
	   the current buffer, just move the gap (if necessary) to where
	   the text should be inserted.  If the new text is too large, reallocate
	   the buffer with a gap large enough to accommodate the new text and a
	   gap of PreferredGapSize */
	if (length > gap_size()) {
		reallocate_buffer(pos, length + PreferredGapSize);
	} else if (pos != gap_start_) {
		move_gap(pos);
	}

	// Insert the new text (pos now corresponds to the start of the gap)
	buf_[pos] = ch;

	gap_start_ += length;
	size_ += length;
}

/**
 *
 */
template <class Ch, class Tr>
auto gap_buffer<Ch, Tr>::erase(size_type start, size_type end) noexcept -> size_type {

	assert(start <= size() && start >= 0);
	assert(end <= size() && end >= 0);
	assert(start <= end);

	delete_range(start, end);
	return start;
}

/**
 *
 */
template <class Ch, class Tr>
void gap_buffer<Ch, Tr>::replace(size_type start, size_type end, view_type str) {
	insert(erase(start, end), str);
}

/**
 *
 */
template <class Ch, class Tr>
void gap_buffer<Ch, Tr>::replace(size_type start, size_type end, Ch ch) {
	insert(erase(start, end), ch);
}

/**
 *
 */
template <class Ch, class Tr>
void gap_buffer<Ch, Tr>::assign(view_type str) {
	replace(0, size(), str);
}

/**
 *
 */
template <class Ch, class Tr>
void gap_buffer<Ch, Tr>::clear() noexcept {
	erase(0, size());
}

/**
 *
 */
template <class Ch, class Tr>
void gap_buffer<Ch, Tr>::move_gap(size_type pos) noexcept {

	const size_type gap_length = gap_size();

	if (pos > gap_start_) {
		Tr::move(&buf_[gap_start_], &buf_[gap_end_], static_cast<size_t>(pos - gap_start_));
	} else {
		Tr::move(&buf_[pos + gap_length], &buf_[pos], static_cast<size_t>(gap_start_ - pos));
	}

	gap_end_ += (pos - gap_start_);
	gap_start_ += (pos - gap_start_);
}

/*
** Reallocate the text storage in "buf_" to have a gap starting at "new_gap_start"
** and a gap size of "new_gap_size", preserving the buffer's current contents.
*/
template <class Ch, class Tr>
void gap_buffer<Ch, Tr>::reallocate_buffer(size_type new_gap_start, size_type new_gap_size) {

	auto new_buffer = std::make_unique<Ch[]>(size() + new_gap_size);

	const size_type new_gap_end = new_gap_start + new_gap_size;

	if (new_gap_start <= gap_start_) {
		Tr::copy(&new_buffer[0], &buf_[0], static_cast<size_t>(new_gap_start));
		Tr::copy(&new_buffer[new_gap_end], &buf_[new_gap_start], static_cast<size_t>(gap_start_ - new_gap_start));
		Tr::copy(&new_buffer[new_gap_end + gap_start_ - new_gap_start], &buf_[gap_end_], static_cast<size_t>(size() - gap_start_));
	} else { // newGapStart > gap_start_
		Tr::copy(&new_buffer[0], &buf_[0], static_cast<size_t>(gap_start_));
		Tr::copy(&new_buffer[gap_start_], &buf_[gap_end_], static_cast<size_t>(new_gap_start - gap_start_));
		Tr::copy(&new_buffer[new_gap_end], &buf_[gap_end_ + new_gap_start - gap_start_], static_cast<size_t>(size() - new_gap_start));
	}

	buf_       = std::move(new_buffer);
	gap_start_ = new_gap_start;
	gap_end_   = new_gap_end;

#ifdef PURIFY
	std::fill(&buf_[gap_start_], &buf_[gap_end_], Ch('.'));
#endif
}

/*
** Removes the contents of the buffer between start and end (and moves the gap
** to the site of the delete).
*/
template <class Ch, class Tr>
void gap_buffer<Ch, Tr>::delete_range(size_type start, size_type end) noexcept {

	// if the gap is not contiguous to the area to remove, move it there
	if (start > gap_start_) {
		move_gap(start);
	} else if (end < gap_start_) {
		move_gap(end);
	}

	// expand the gap to encompass the deleted characters
	gap_end_ += (end - gap_start_);
	gap_start_ -= (gap_start_ - start);

	// update the length
	size_ -= (end - start);
}

template <class Ch, class Tr>
void gap_buffer<Ch, Tr>::swap(gap_buffer &other) noexcept {
	using std::swap;

	swap(buf_, other.buf_);
	swap(gap_start_, other.gap_start_);
	swap(gap_end_, other.gap_end_);
	swap(size_, other.size_);
}

#endif
