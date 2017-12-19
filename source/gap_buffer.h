
#ifndef GAP_BUFFER_H_
#define GAP_BUFFER_H_

#include <string>
#include <algorithm>
#include <cassert>
#include "util/string_view.h"

template <class Ch = char, class Tr = std::char_traits<Ch>>
class gap_buffer {
public:
    static constexpr int PreferredGapSize = 80;
    using string_type = std::basic_string<Ch, Tr>;
    using view_type   = view::basic_string_view<Ch, Tr>;

public:
    gap_buffer();
    gap_buffer(int64_t size);
    gap_buffer(const gap_buffer&)            = delete;
    gap_buffer& operator=(const gap_buffer&) = delete;
    ~gap_buffer() noexcept;

public:
    int64_t gap_size() const { return gapEnd_ - gapStart_; }
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
    Ch *buf_;           // points to the internal buffer
    int64_t gapStart_;  // points to the first character of the gap
    int64_t gapEnd_;    // points to the first char after the gap
    int64_t size_;      // length of the text in the buffer (the length of the buffer itself must be calculated: gapEnd - gapStart + length)
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

    buf_      = new Ch[size + PreferredGapSize];
    gapStart_ = 0;
    gapEnd_   = PreferredGapSize;
    size_     = 0;

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

    if (n < gapStart_) {
        return buf_[n];
    }

    return buf_[n + gap_size()];
}

/**
 *
 */
template <class Ch, class Tr>
Ch& gap_buffer<Ch, Tr>::operator[](int64_t n) {

    if (n < gapStart_) {
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

    if (n < gapStart_) {
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

    if (n < gapStart_) {
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

    if (posEnd <= gapStart_) {
        return Tr::compare(&buf_[pos], str.data(), str.size());
    } else if (pos >= gapStart_) {
        return Tr::compare(&buf_[pos + gap_size()], str.data(), str.size());
    } else {
        const int64_t part1Length = gapStart_ - pos;
        const int result = Tr::compare(&buf_[pos], str.data(), part1Length);
        if (result) {
            return result;
        }

        return Tr::compare(&buf_[gapEnd_], &str[part1Length], str.size() - part1Length);
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

    std::copy_n(buf_,           gapStart_,          std::back_inserter(text));
    std::copy_n(&buf_[gapEnd_], size() - gapStart_, std::back_inserter(text));

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
    if (end <= gapStart_) {
        std::copy_n(&buf_[start], length, std::back_inserter(text));
    } else if (start >= gapStart_) {
        std::copy_n(&buf_[start + gap_size()], length, std::back_inserter(text));
    } else {
        const int64_t part1Length = gapStart_ - start;

        std::copy_n(&buf_[start],   part1Length,          std::back_inserter(text));
        std::copy_n(&buf_[gapEnd_], length - part1Length, std::back_inserter(text));
    }

    return text;
}

/**
 *
 */
template <class Ch, class Tr>
auto gap_buffer<Ch, Tr>::to_view() -> view_type {

    const int64_t bufLen   = size();
    int64_t leftLen        = gapStart_;
    const int64_t rightLen = bufLen - leftLen;

    // find where best to put the gap to minimise memory movement
    if (leftLen != 0 && rightLen != 0) {
        leftLen = (leftLen < rightLen) ? 0 : bufLen;
        move_gap(leftLen);
    }

    // get the start position of the actual data
    Ch *const text = &buf_[(leftLen == 0) ? gapEnd_ : 0];

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
    int64_t leftLen        = gapStart_;
    const int64_t rightLen = bufLen - leftLen;

    // find where best to put the gap to minimise memory movement
    if (leftLen != 0 && rightLen != 0) {
        leftLen = (leftLen < rightLen) ? 0 : bufLen;
        move_gap(leftLen);
    }

    // get the start position of the actual data
    Ch *const text = &buf_[(leftLen == 0) ? gapEnd_ : 0];

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
    } else if (pos != gapStart_) {
        move_gap(pos);
    }

    // Insert the new text (pos now corresponds to the start of the gap)
    std::copy(str.begin(), str.end(), &buf_[pos]);

    gapStart_ += length;
    size_     += length;
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
    } else if (pos != gapStart_) {
        move_gap(pos);
    }

    // Insert the new text (pos now corresponds to the start of the gap)
    buf_[pos] = ch;

    gapStart_ += length;
    size_     += length;
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
    buf_      = new Ch[length + PreferredGapSize + 1];
    size_     = length;
    gapStart_ = length / 2;
    gapEnd_   = gapStart_ + PreferredGapSize;

    Tr::copy(&buf_[0],       &str[0],         gapStart_);
    Tr::copy(&buf_[gapEnd_], &str[gapStart_], length - gapStart_);

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

    const int64_t gapLen = gapEnd_ - gapStart_;

    if (pos > gapStart_) {
        Tr::move(&buf_[gapStart_], &buf_[gapEnd_], pos - gapStart_);
    } else {
        Tr::move(&buf_[pos + gapLen], &buf_[pos], gapStart_ - pos);
    }

    gapEnd_   += (pos - gapStart_);
    gapStart_ += (pos - gapStart_);
}

/*
** Reallocate the text storage in "buf_" to have a gap starting at "newGapStart"
** and a gap size of "newGapLen", preserving the buffer's current contents.
*/
template <class Ch, class Tr>
void gap_buffer<Ch, Tr>::reallocate_buffer(int64_t new_gap_start, int64_t new_gap_size) {

    auto newBuf      = new Ch[size() + new_gap_size];
    int64_t newGapEnd = new_gap_start + new_gap_size;

    if (new_gap_start <= gapStart_) {
        Tr::copy(newBuf,                                       &buf_[0],           new_gap_start);
        Tr::copy(&newBuf[newGapEnd],                           &buf_[new_gap_start], gapStart_ - new_gap_start);
        Tr::copy(&newBuf[newGapEnd + gapStart_ - new_gap_start], &buf_[gapEnd_],     size() - gapStart_);
    } else { // newGapStart > gapStart_
        Tr::copy(newBuf,             &buf_[0],                                 gapStart_);
        Tr::copy(&newBuf[gapStart_], &buf_[gapEnd_],                           new_gap_start - gapStart_);
        Tr::copy(&newBuf[newGapEnd], &buf_[gapEnd_ + new_gap_start - gapStart_], size() - new_gap_start);
    }

    delete [] buf_;

    buf_      = newBuf;
    gapStart_ = new_gap_start;
    gapEnd_   = newGapEnd;

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
    if (start > gapStart_) {
        move_gap(start);
    } else if (end < gapStart_) {
        move_gap(end);
    }

    // expand the gap to encompass the deleted characters
    gapEnd_   += (end - gapStart_);
    gapStart_ -= (gapStart_ - start);

    // update the length
    size_ -= (end - start);
}

#endif
