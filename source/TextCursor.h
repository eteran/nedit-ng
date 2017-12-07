
#ifndef TEXT_CURSOR_H_
#define TEXT_CURSOR_H_

#include <cstddef>
#include <cstdint>

class TextCursor {
public:
    using difference_type = std::ptrdiff_t;
    using value_type      = int64_t;

public:
    static TextCursor Invalid;

public:
    TextCursor() : pos_(0) {}
    TextCursor(value_type pos) : pos_(pos) {}
    TextCursor(const TextCursor &other)          = default;
    TextCursor& operator=(const TextCursor &rhs) = default;
    ~TextCursor()                                = default;

public:
    TextCursor& operator+=(difference_type rhs) { pos_ += rhs; return *this; }
    TextCursor& operator-=(difference_type rhs) { pos_ -= rhs; return *this; }

public:
    TextCursor& operator++() { ++pos_; return *this; }
    TextCursor& operator--() { --pos_; return *this; }
    TextCursor operator++(int) { TextCursor tmp(*this); ++pos_; return tmp; }
    TextCursor operator--(int) { TextCursor tmp(*this); --pos_; return tmp; }

public:
    difference_type operator-(const TextCursor& rhs) const { return pos_ - rhs.pos_; }

public:
    TextCursor operator+(difference_type n) const { return TextCursor(pos_ + n); }
    TextCursor operator-(difference_type n) const { return TextCursor(pos_ - n); }

public:
    friend TextCursor operator+(difference_type lhs, const TextCursor& rhs);
    friend TextCursor operator-(difference_type lhs, const TextCursor& rhs);

public:
    bool operator==(const TextCursor& rhs) const { return pos_ == rhs.pos_; }
    bool operator!=(const TextCursor& rhs) const { return pos_ != rhs.pos_; }
    bool operator>(const TextCursor& rhs) const  { return pos_ > rhs.pos_;  }
    bool operator<(const TextCursor& rhs) const  { return pos_ < rhs.pos_;  }
    bool operator>=(const TextCursor& rhs) const { return pos_ >= rhs.pos_; }
    bool operator<=(const TextCursor& rhs) const { return pos_ <= rhs.pos_; }

private:
    value_type pos_;
};

inline TextCursor operator+(TextCursor::difference_type lhs, const TextCursor& rhs) { return TextCursor(lhs + rhs.pos_); }
inline TextCursor operator-(TextCursor::difference_type lhs, const TextCursor& rhs) { return TextCursor(lhs - rhs.pos_); }

#endif
