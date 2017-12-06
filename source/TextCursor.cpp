
#include "TextCursor.h"

/**
 * @brief TextCursor::TextCursor
 */
TextCursor::TextCursor() : pos_(Invalid) {
}

/**
 * @brief TextCursor::TextCursor
 * @param pos
 */
TextCursor::TextCursor(int pos) : pos_(pos) {
}

/**
 * @brief operator ++
 * @return
 */
TextCursor TextCursor::operator++(int) {
    TextCursor temp{*this};
    ++pos_;
    return temp;
}

/**
 * @brief TextCursor::operator ++
 * @return
 */
TextCursor& TextCursor::operator++() {
    ++pos_;
    return *this;
}

/**
 * @brief TextCursor::operator --
 * @return
 */
TextCursor TextCursor::operator--(int) {
    TextCursor temp{*this};
    ++pos_;
    return temp;
}

/**
 * @brief operator --
 * @return
 */
TextCursor& TextCursor::operator--() {
    --pos_;
    return *this;
}

/**
 * @brief TextCursor::operator ==
 * @param rhs
 * @return
 */
bool TextCursor::operator==(const TextCursor &rhs) const {
    return pos_ == rhs.pos_;
}

/**
 * @brief TextCursor::operator !=
 * @param rhs
 * @return
 */
bool TextCursor::operator!=(const TextCursor &rhs) const {
    return pos_ != rhs.pos_;
}

/**
 * @brief TextCursor::operator +=
 * @param n
 * @return
 */
TextCursor& TextCursor::operator+=(int n) {
    pos_ += n;
    return *this;
}

/**
 * @brief TextCursor::operator -=
 * @param n
 * @return
 */
TextCursor& TextCursor::operator-=(int n) {
    pos_ -= n;
    return *this;
}

/**
 * @brief TextCursor::value
 * @return
 */
int TextCursor::value() const {
    return pos_;
}
