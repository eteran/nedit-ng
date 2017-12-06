
#ifndef TEXT_CURSOR_H_
#define TEXT_CURSOR_H_

class TextCursor {
public:
    static constexpr auto Invalid = static_cast<int>(-1);

public:
    TextCursor() ;
    explicit TextCursor(int pos);
    TextCursor(const TextCursor &)            = default;
    TextCursor& operator=(const TextCursor &) = default;

public:
    TextCursor operator++(int);
    TextCursor& operator++();
    TextCursor operator--(int);
    TextCursor& operator--();

public:
    TextCursor& operator+=(int n);
    TextCursor& operator-=(int n);

public:
    int value() const;

public:
    bool operator==(const TextCursor &rhs) const;
    bool operator!=(const TextCursor &rhs) const;

private:
    int pos_;
};

#endif
