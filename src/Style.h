
#ifndef STYLE_H_
#define STYLE_H_

class Style {
public:
	Style()                         = default;
	Style(const Style &)            = default;
	Style &operator=(const Style &) = default;

	explicit Style(void *v)
		: value_(v) {
	}

public:
	bool operator==(const Style &rhs) const {
		return value_ == rhs.value_;
	}

private:
	void *value_ = nullptr;
};

#endif
