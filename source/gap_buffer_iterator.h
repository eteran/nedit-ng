
#ifndef GAP_BUFFER_ITERATOR_H_
#define GAP_BUFFER_ITERATOR_H_

#include "gap_buffer_fwd.h"
#include <cassert>
#include <iterator>

//  non-const version
template <class Ch, class Tr>
class gap_buffer_iterator {
    using traits_type = typename std::iterator<std::random_access_iterator_tag, Ch>;

    template <class U, class Y>
    friend class gap_buffer_const_iterator;

public:
    using difference_type   = typename traits_type::difference_type;
    using iterator_category = typename traits_type::iterator_category;
    using pointer           = typename traits_type::pointer;
    using reference         = typename traits_type::reference;
    using value_type        = typename traits_type::value_type;

public:
    gap_buffer_iterator()                                     : gap_buffer_iterator(nullptr, 0) {}
    gap_buffer_iterator(gap_buffer<Ch, Tr> *buf, int64_t pos) : buf_(buf), pos_(pos) {}
    gap_buffer_iterator(const gap_buffer_iterator &rhs)         = default;
    gap_buffer_iterator& operator=(const gap_buffer_iterator &) = default;

public:
    gap_buffer_iterator& operator+=(difference_type rhs) { pos_ += rhs; return *this; }
    gap_buffer_iterator& operator-=(difference_type rhs) { pos_ -= rhs; return *this; }

public:
    gap_buffer_iterator& operator++()    { ++pos_; return *this; }
    gap_buffer_iterator& operator--()    { --pos_; return *this; }
    gap_buffer_iterator operator++(int)  { gap_buffer_iterator tmp(*this); ++pos_; return tmp; }
    gap_buffer_iterator operator--(int)  { gap_buffer_iterator tmp(*this); --pos_; return tmp; }

public:
    gap_buffer_iterator operator+(difference_type rhs) const { return gap_buffer_iterator(pos_ + rhs); }
    gap_buffer_iterator operator-(difference_type rhs) const { return gap_buffer_iterator(pos_ - rhs); }

public:
    difference_type operator-(const gap_buffer_iterator& rhs) const                           { return gap_buffer_iterator(pos_ - rhs.ptr); }
    friend gap_buffer_iterator operator+(difference_type lhs, const gap_buffer_iterator& rhs) { return gap_buffer_iterator(lhs + rhs.pos_); }
    friend gap_buffer_iterator operator-(difference_type lhs, const gap_buffer_iterator& rhs) { return gap_buffer_iterator(lhs - rhs.pos_); }

public:
    reference operator*() const                        { return buf_[pos_];          }
    reference operator[](difference_type offset) const { return buf_[pos_ + offset]; }
    pointer operator->() const                         { return &buf_[pos_];         }

public:
    // NOTE(eteran): it is UB to compare iterators from difference containers
    // so we just assert
    bool operator==(const gap_buffer_iterator& rhs) const { assert(buf_ == rhs.buf_); return pos_ == rhs.pos_; }
    bool operator!=(const gap_buffer_iterator& rhs) const { assert(buf_ == rhs.buf_); return pos_ != rhs.pos_; }
    bool operator>(const gap_buffer_iterator& rhs) const  { assert(buf_ == rhs.buf_); return pos_ > rhs.pos_;  }
    bool operator<(const gap_buffer_iterator& rhs) const  { assert(buf_ == rhs.buf_); return pos_ < rhs.pos_;  }
    bool operator>=(const gap_buffer_iterator& rhs) const { assert(buf_ == rhs.buf_); return pos_ >= rhs.pos_; }
    bool operator<=(const gap_buffer_iterator& rhs) const { assert(buf_ == rhs.buf_); return pos_ <= rhs.pos_; }

private:
    gap_buffer<Ch, Tr> *buf_;
    int64_t             pos_;
};

// const version
template <class Ch, class Tr>
class gap_buffer_const_iterator {
    using traits_type = typename std::iterator<std::random_access_iterator_tag, const Ch>;
public:
    using difference_type   = typename traits_type::difference_type;
    using iterator_category = typename traits_type::iterator_category;
    using pointer           = typename traits_type::pointer;
    using reference         = typename traits_type::reference;
    using value_type        = typename traits_type::value_type;

public:
    gap_buffer_const_iterator()                                             : gap_buffer_const_iterator(nullptr, 0) {}
    gap_buffer_const_iterator(const gap_buffer<Ch, Tr> *buf, int64_t pos)   : buf_(buf), pos_(pos) {}
    gap_buffer_const_iterator(const gap_buffer_const_iterator &rhs)         = default;
    gap_buffer_const_iterator& operator=(const gap_buffer_const_iterator &) = default;

    // conversion from non-const to const
    explicit gap_buffer_const_iterator(const gap_buffer_iterator<Ch, Tr> &rhs) : buf_(rhs.buf_), pos_(rhs.pos_) {}

public:
    gap_buffer_const_iterator& operator+=(difference_type rhs) { pos_ += rhs; return *this; }
    gap_buffer_const_iterator& operator-=(difference_type rhs) { pos_ -= rhs; return *this; }

public:
    gap_buffer_const_iterator& operator++()    { ++pos_; return *this; }
    gap_buffer_const_iterator& operator--()    { --pos_; return *this; }
    gap_buffer_const_iterator operator++(int)  { gap_buffer_const_iterator tmp(*this); ++pos_; return tmp; }
    gap_buffer_const_iterator operator--(int)  { gap_buffer_const_iterator tmp(*this); --pos_; return tmp; }

public:
    gap_buffer_const_iterator operator+(difference_type rhs) const { return gap_buffer_const_iterator(pos_ + rhs); }
    gap_buffer_const_iterator operator-(difference_type rhs) const { return gap_buffer_const_iterator(pos_ - rhs); }

public:
    difference_type operator-(const gap_buffer_const_iterator& rhs) const                                 { return gap_buffer_const_iterator(pos_ - rhs.ptr); }
    friend gap_buffer_const_iterator operator+(difference_type lhs, const gap_buffer_const_iterator& rhs) { return gap_buffer_const_iterator(lhs + rhs.pos_); }
    friend gap_buffer_const_iterator operator-(difference_type lhs, const gap_buffer_const_iterator& rhs) { return gap_buffer_const_iterator(lhs - rhs.pos_); }

public:
    reference operator*() const                        { return buf_[pos_];          }
    reference operator[](difference_type offset) const { return buf_[pos_ + offset]; }
    pointer operator->() const                         { return &buf_[pos_];         }

public:
    // NOTE(eteran): it is UB to compare iterators from difference containers
    // so we just assert
    bool operator==(const gap_buffer_const_iterator& rhs) const { assert(buf_ == rhs.buf_); return pos_ == rhs.pos_; }
    bool operator!=(const gap_buffer_const_iterator& rhs) const { assert(buf_ == rhs.buf_); return pos_ != rhs.pos_; }
    bool operator>(const gap_buffer_const_iterator& rhs) const  { assert(buf_ == rhs.buf_); return pos_ > rhs.pos_;  }
    bool operator<(const gap_buffer_const_iterator& rhs) const  { assert(buf_ == rhs.buf_); return pos_ < rhs.pos_;  }
    bool operator>=(const gap_buffer_const_iterator& rhs) const { assert(buf_ == rhs.buf_); return pos_ >= rhs.pos_; }
    bool operator<=(const gap_buffer_const_iterator& rhs) const { assert(buf_ == rhs.buf_); return pos_ <= rhs.pos_; }

    // allow the non-const on either side of the comparison
    friend bool operator==(const gap_buffer_iterator<Ch, Tr>& lhs, const gap_buffer_const_iterator& rhs) { return gap_buffer_const_iterator(lhs) == rhs; }
    friend bool operator!=(const gap_buffer_iterator<Ch, Tr>& lhs, const gap_buffer_const_iterator& rhs) { return gap_buffer_const_iterator(lhs) != rhs; }
    friend bool operator>(const gap_buffer_iterator<Ch, Tr>& lhs, const gap_buffer_const_iterator& rhs)  { return gap_buffer_const_iterator(lhs) > rhs; }
    friend bool operator<(const gap_buffer_iterator<Ch, Tr>& lhs, const gap_buffer_const_iterator& rhs)  { return gap_buffer_const_iterator(lhs) < rhs; }
    friend bool operator>=(const gap_buffer_iterator<Ch, Tr>& lhs, const gap_buffer_const_iterator& rhs) { return gap_buffer_const_iterator(lhs) >= rhs; }
    friend bool operator<=(const gap_buffer_iterator<Ch, Tr>& lhs, const gap_buffer_const_iterator& rhs) { return gap_buffer_const_iterator(lhs) <= rhs; }

private:
    const gap_buffer<Ch, Tr> *buf_;
    int64_t                   pos_;
};

#endif
