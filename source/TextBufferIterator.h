
#ifndef TEXT_BUFFER_ITERATOR_H_
#define TEXT_BUFFER_ITERATOR_H_

template <class Ch, class Tr>
class BasicTextBuffer;

template <class Ch, class Tr>
class BasicTextBufferIterator {
public:
    BasicTextBufferIterator() = default;
    BasicTextBufferIterator(const BasicTextBuffer<Ch, Tr> *buffer, int index) : buffer_(buffer), index_(index) {}
    BasicTextBufferIterator(const BasicTextBufferIterator &) = default;
    BasicTextBufferIterator& operator=(const BasicTextBufferIterator &) = default;

public:
    Ch operator*() const       { return buffer_->BufGetCharacter(index_); }
    Ch operator[](int n) const { return buffer_->BufGetCharacter(index_ + n); }

public:
    BasicTextBufferIterator& operator++() { ++index_; }
    BasicTextBufferIterator& operator--() { --index_; }
    BasicTextBufferIterator operator++(int) { BasicTextBufferIterator temp{*this}; ++index_; return temp; }
    BasicTextBufferIterator operator--(int) { BasicTextBufferIterator temp{*this}; --index_; return temp; }

public:
    BasicTextBufferIterator& operator+=(int n) { index_ += n; return *this; }
    BasicTextBufferIterator& operator-=(int n) { index_ -= n; return *this; }
    BasicTextBufferIterator operator+(int n) const { BasicTextBufferIterator temp{*this}; temp += n; return temp; }
    BasicTextBufferIterator operator-(int n) const { BasicTextBufferIterator temp{*this}; temp -= n; return temp; }

public:
    int operator-(const BasicTextBufferIterator &rhs) const { assert(buffer_ == rhs.buffer_); return index_ - rhs.index_; }

public:
    bool operator==(const BasicTextBufferIterator &rhs) const { return buffer_ == rhs.buffer_ && index_ == rhs.index_; }
    bool operator!=(const BasicTextBufferIterator &rhs) const { return buffer_ != rhs.buffer_ || index_ != rhs.index_; }

private:
    const BasicTextBuffer<Ch, Tr> *buffer_ = nullptr;
    int index_ = 0;
};

#endif
