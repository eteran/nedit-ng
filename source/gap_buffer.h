
#ifndef GAP_BUFFER_H_
#define GAP_BUFFER_H_

#include "gap_buffer_fwd.h"
#include "gap_buffer_iterator.h"

#include <string>
#include <algorithm>
#include <cassert>
#include "Util/string_view.h"

template <class Ch, class Tr>
class gap_buffer {
public:
    static constexpr int PreferredGapSize = 80;
    using string_type = std::basic_string<Ch, Tr>;
    using view_type   = view::basic_string_view<Ch, Tr>;

public:
    using value_type             = std::allocator<char>::value_type;
    using allocator_type         = std::allocator<char>;
    using size_type              = int64_t; // NOTE(eteran): typically unsigned...
    using difference_type        = std::allocator<char>::difference_type;
    using reference              = std::allocator<char>::reference;
    using const_reference        = std::allocator<char>::const_reference;
    using pointer                = std::allocator<char>::pointer;
    using const_pointer          = std::allocator<char>::const_pointer;
    using iterator               = gap_buffer_iterator<Ch, Tr>;
    using const_iterator         = gap_buffer_const_iterator<Ch, Tr>;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

public:
    gap_buffer();
    explicit gap_buffer(int64_t size);
    gap_buffer(const gap_buffer&)            = delete;
    gap_buffer& operator=(const gap_buffer&) = delete;
    ~gap_buffer() noexcept;

public:
    iterator begin()              { return iterator(this, 0); }
    iterator end()                { return iterator(this, size()); }
    const_iterator begin() const  { return const_iterator(this, 0); }
    const_iterator end() const    { return const_iterator(this, size()); }
    const_iterator cbegin() const { return const_iterator(this, 0); }
    const_iterator cend() const   { return const_iterator(this, size()); }

    reverse_iterator rbegin()              { return reverse_iterator(end()); }
    reverse_iterator rend()                { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin() const  { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const    { return const_reverse_iterator(begin()); }
    const_reverse_iterator crbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator crend() const   { return const_reverse_iterator(begin()); }

public:
    int64_t gap_size() const { return gap_end_ - gap_start_; }
    int64_t size() const     { return size_; }
    bool empty() const       { return size() == 0; }

public:
    Ch operator[](int64_t n) const;
    Ch& operator[](int64_t n);
    Ch at(int64_t n) const;
    Ch& at(int64_t n);

public:
    int compare(int64_t pos, view_type str) const;
    int compare(int64_t pos, Ch ch) const;

public:
    string_type to_string() const;
    string_type to_string(int64_t start, int64_t end) const;
    view_type to_view();
    view_type to_view(int64_t start, int64_t end);

public:
    void append(view_type str);
    void append(Ch ch);
    void insert(int64_t pos, view_type str);
    void insert(int64_t pos, Ch ch);
    void erase(int64_t start, int64_t end);
    void replace(int64_t start, int64_t end, view_type str);
    void replace(int64_t start, int64_t end, Ch ch);
    void assign(view_type str);
    void clear();

private:
    void move_gap(int64_t pos);
    void reallocate_buffer(int64_t new_gap_start, int64_t new_gap_size);
    void delete_range(int64_t start, int64_t end);

private:
    Ch *buf_;            // points to the internal buffer
    int64_t gap_start_;  // points to the first character of the gap
    int64_t gap_end_;    // points to the first char after the gap
    int64_t size_;       // length of the text in the buffer (the length of the buffer itself must be calculated: gapEnd - gapStart + length)
};


/**
 *
 */
template <class Ch, class Tr>
gap_buffer<Ch, Tr>::gap_buffer() : gap_buffer(0){
}

/**
 *
 */
template <class Ch, class Tr>
gap_buffer<Ch, Tr>::gap_buffer(int64_t size) {

    buf_       = new Ch[size + PreferredGapSize];
    gap_start_ = 0;
    gap_end_   = PreferredGapSize;
    size_      = 0;

#ifdef PURIFY
    std::fill(&buf_[gapStart_], &buf_[gapEnd_], Ch('.'));
#endif
}

/**
 *
 */
template <class Ch, class Tr>
gap_buffer<Ch, Tr>::~gap_buffer() noexcept {
    delete buf_;
}

/**
 *
 */
template <class Ch, class Tr>
Ch gap_buffer<Ch, Tr>::operator[](int64_t n) const {

    if (n < gap_start_) {
        return buf_[n];
    }

    return buf_[n + gap_size()];
}

/**
 *
 */
template <class Ch, class Tr>
Ch& gap_buffer<Ch, Tr>::operator[](int64_t n) {

    if (n < gap_start_) {
        return buf_[n];
    }

    return buf_[n + gap_size()];
}

/**
 *
 */
template <class Ch, class Tr>
Ch gap_buffer<Ch, Tr>::at(int64_t n) const {

    if (n >= size() || n < 0) {
        raise<std::out_of_range>("gap_buffer::operator[]");
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
Ch& gap_buffer<Ch, Tr>::at(int64_t n) {

    if (n >= size() || n < 0) {
        raise<std::out_of_range>("gap_buffer::at");
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
int gap_buffer<Ch, Tr>::compare(int64_t pos, view_type str) const {

    // NOTE(eteran): could just be:
    // return to_view(pos, pos + str.size()).compare(str);
    // but that would also be slightly less efficient since "to_view" *may* do
    // a gap move. Worth it for the simplicity?

    int64_t posEnd = pos + str.size();
    if (posEnd > size()) {
        return 1;
    }

    if(pos < 0) {
        return -1;
    }

    if (posEnd <= gap_start_) {
        return Tr::compare(&buf_[pos], str.data(), str.size());
    } else if (pos >= gap_start_) {
        return Tr::compare(&buf_[pos + gap_size()], str.data(), str.size());
    } else {
        const int64_t part1Length = gap_start_ - pos;
        const int result = Tr::compare(&buf_[pos], str.data(), part1Length);
        if (result) {
            return result;
        }

        return Tr::compare(&buf_[gap_end_], &str[part1Length], str.size() - part1Length);
    }
}

/**
 *
 */
template <class Ch, class Tr>
int gap_buffer<Ch, Tr>::compare(int64_t pos, Ch ch) const {
    if (pos >= size()) {
        return 1;
    }

    if(pos < 0) {
        return -1;
    }

    const Ch buffer_char = at(pos);
    return Tr::compare(&buffer_char, &ch, 1);
}

/**
 *
 */
template <class Ch, class Tr>
auto gap_buffer<Ch, Tr>::to_string() const -> string_type {
    string_type text;
    text.reserve(size());

    std::copy_n(buf_,            gap_start_,          std::back_inserter(text));
    std::copy_n(&buf_[gap_end_], size() - gap_start_, std::back_inserter(text));

    return text;
}

/**
 *
 */
template <class Ch, class Tr>
auto gap_buffer<Ch, Tr>::to_string(int64_t start, int64_t end) const -> string_type {
    string_type text;

    assert(start <= size() && start >= 0);
    assert(end   <= size() && end   >= 0);
    assert(start <= end);

    const int64_t length = end - start;
    text.reserve(length);

    // Copy the text from the buffer to the returned string
    if (end <= gap_start_) {
        std::copy_n(&buf_[start], length, std::back_inserter(text));
    } else if (start >= gap_start_) {
        std::copy_n(&buf_[start + gap_size()], length, std::back_inserter(text));
    } else {
        const int64_t part1Length = gap_start_ - start;

        std::copy_n(&buf_[start],    part1Length,          std::back_inserter(text));
        std::copy_n(&buf_[gap_end_], length - part1Length, std::back_inserter(text));
    }

    return text;
}

/**
 *
 */
template <class Ch, class Tr>
auto gap_buffer<Ch, Tr>::to_view() -> view_type {

    const int64_t bufLen   = size();
    int64_t leftLen        = gap_start_;
    const int64_t rightLen = bufLen - leftLen;

    // find where best to put the gap to minimise memory movement
    if (leftLen != 0 && rightLen != 0) {
        leftLen = (leftLen < rightLen) ? 0 : bufLen;
        move_gap(leftLen);
    }

    // get the start position of the actual data
    Ch *const text = &buf_[(leftLen == 0) ? gap_end_ : 0];

    return view_type(text, bufLen);
}

/**
 *
 */
template <class Ch, class Tr>
auto gap_buffer<Ch, Tr>::to_view(int64_t start, int64_t end) -> view_type {

    assert(start <= size() && start >= 0);
    assert(end   <= size() && end   >= 0);
    assert(start <= end);

    const int64_t bufLen   = size();
    int64_t leftLen        = gap_start_;
    const int64_t rightLen = bufLen - leftLen;

    // find where best to put the gap to minimise memory movement
    if (leftLen != 0 && rightLen != 0) {
        leftLen = (leftLen < rightLen) ? 0 : bufLen;
        move_gap(leftLen);
    }

    // get the start position of the actual data
    Ch *const text = &buf_[(leftLen == 0) ? gap_end_ : 0];

    return view_type(text + start, end - start);
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
void gap_buffer<Ch, Tr>::insert(int64_t pos, view_type str) {

    assert(pos <= size() && pos >= 0);

    const int64_t length = str.size();

    /* Prepare the buffer to receive the new text.  If the new text fits in
       the current buffer, just move the gap (if necessary) to where
       the text should be inserted.  If the new text is too large, reallocate
       the buffer with a gap large enough to accomodate the new text and a
       gap of PreferredGapSize */
    if (length > gap_size()) {
        reallocate_buffer(pos, length + PreferredGapSize);
    } else if (pos != gap_start_) {
        move_gap(pos);
    }

    // Insert the new text (pos now corresponds to the start of the gap)
    std::copy(str.begin(), str.end(), &buf_[pos]);

    gap_start_ += length;
    size_      += length;
}

/**
 *
 */
template <class Ch, class Tr>
void gap_buffer<Ch, Tr>::insert(int64_t pos, Ch ch) {

    assert(pos <= size() && pos >= 0);

    const int64_t length = 1;

    /* Prepare the buffer to receive the new text.  If the new text fits in
       the current buffer, just move the gap (if necessary) to where
       the text should be inserted.  If the new text is too large, reallocate
       the buffer with a gap large enough to accomodate the new text and a
       gap of PreferredGapSize */
    if (length > gap_size()) {
        reallocate_buffer(pos, length + PreferredGapSize);
    } else if (pos != gap_start_) {
        move_gap(pos);
    }

    // Insert the new text (pos now corresponds to the start of the gap)
    buf_[pos] = ch;

    gap_start_ += length;
    size_      += length;
}

/**
 *
 */
template <class Ch, class Tr>
void gap_buffer<Ch, Tr>::erase(int64_t start, int64_t end) {

    assert(start <= size() && start >= 0);
    assert(end   <= size() && end   >= 0);
    assert(start <= end);

    delete_range(start, end);
}

/**
 *
 */
template <class Ch, class Tr>
void gap_buffer<Ch, Tr>::replace(int64_t start, int64_t end, view_type str) {

    erase(start, end);
    insert(start, str);
}

/**
 *
 */
template <class Ch, class Tr>
void gap_buffer<Ch, Tr>::replace(int64_t start, int64_t end, Ch ch) {

    erase(start, end);
    insert(start, ch);
}

/**
 *
 */
template <class Ch, class Tr>
void gap_buffer<Ch, Tr>::assign(view_type str) {
    const int64_t length = str.size();

    delete [] buf_;

    // Start a new buffer with a gap of GapSize in the center
    buf_       = new Ch[length + PreferredGapSize];
    size_      = length;
    gap_start_ = length / 2;
    gap_end_   = gap_start_ + PreferredGapSize;

    Tr::copy(&buf_[0],        &str[0],          gap_start_);
    Tr::copy(&buf_[gap_end_], &str[gap_start_], length - gap_start_);

#ifdef PURIFY
    std::fill(&buf_[gapStart_], &buf_[gapEnd_], Ch('.'));
#endif
}

/**
 *
 */
template <class Ch, class Tr>
void gap_buffer<Ch, Tr>::clear() {
    erase(0, size());
}

/**
 *
 */
template <class Ch, class Tr>
void gap_buffer<Ch, Tr>::move_gap(int64_t pos) {

    const int64_t gapLen = gap_end_ - gap_start_;

    if (pos > gap_start_) {
        Tr::move(&buf_[gap_start_], &buf_[gap_end_], pos - gap_start_);
    } else {
        Tr::move(&buf_[pos + gapLen], &buf_[pos], gap_start_ - pos);
    }

    gap_end_   += (pos - gap_start_);
    gap_start_ += (pos - gap_start_);
}

/*
** Reallocate the text storage in "buf_" to have a gap starting at "newGapStart"
** and a gap size of "newGapLen", preserving the buffer's current contents.
*/
template <class Ch, class Tr>
void gap_buffer<Ch, Tr>::reallocate_buffer(int64_t new_gap_start, int64_t new_gap_size) {

    auto newBuf         = new Ch[size() + new_gap_size];
    int64_t new_gap_end = new_gap_start + new_gap_size;

    if (new_gap_start <= gap_start_) {
        Tr::copy(newBuf,                                            &buf_[0],             new_gap_start);
        Tr::copy(&newBuf[new_gap_end],                              &buf_[new_gap_start], gap_start_ - new_gap_start);
        Tr::copy(&newBuf[new_gap_end + gap_start_ - new_gap_start], &buf_[gap_end_],      size() - gap_start_);
    } else { // newGapStart > gapStart_
        Tr::copy(newBuf,             &buf_[0],                                        gap_start_);
        Tr::copy(&newBuf[gap_start_], &buf_[gap_end_],                                new_gap_start - gap_start_);
        Tr::copy(&newBuf[new_gap_end],  &buf_[gap_end_ + new_gap_start - gap_start_], size() - new_gap_start);
    }

    delete [] buf_;

    buf_       = newBuf;
    gap_start_ = new_gap_start;
    gap_end_   = new_gap_end;

#ifdef PURIFY
    std::fill(&buf_[gapStart_], &buf_[gapEnd_], Ch('.'));
#endif
}

/*
** Removes the contents of the buffer between start and end (and moves the gap
** to the site of the delete).
*/
template <class Ch, class Tr>
void gap_buffer<Ch, Tr>::delete_range(int64_t start, int64_t end) {

    // if the gap is not contiguous to the area to remove, move it there
    if (start > gap_start_) {
        move_gap(start);
    } else if (end < gap_start_) {
        move_gap(end);
    }

    // expand the gap to encompass the deleted characters
    gap_end_   += (end - gap_start_);
    gap_start_ -= (gap_start_ - start);

    // update the length
    size_ -= (end - start);
}

#endif
