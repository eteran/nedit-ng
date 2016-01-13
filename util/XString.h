
#ifndef XSTRING_H_
#define XSTRING_H_

#include <memory>
#include <algorithm>
#include <cassert>
#include <Xm/Xm.h>

class XString {
public:
	typedef char*       iterator;
	typedef const char* const_iterator;
public:
	XString() : size_(0) {
	}
	
	XString(nullptr_t) : size_(0) {
	}
	
	XString(const std::string &s) : size_(s.size()) {
		char *str = XtMalloc(s.size() + 1);
		strncpy(str, s.c_str(), s.size());
		str[s.size()] = '\0';

		ptr_  = std::shared_ptr<char>(str, XtFree);
		size_ = s.size();		
	}
	
	XString(const char *s) : size_(0) {
		if(s) {
			int length = strlen(s);
			char *str = XtMalloc(length + 1);
			strncpy(str, s, length);
			str[length] = '\0';

			ptr_  = std::shared_ptr<char>(str, XtFree);
			size_ = length;	
		}
	}	
	
	XString(const char *s, int length) : size_(0) {
		if(s) {	
			char *str = XtMalloc(length + 1);
			strncpy(str, s, length);
			str[length] = '\0';
	
			ptr_  = std::shared_ptr<char>(str, XtFree);
			size_ = length;	
		}
	}
	
	XString(const XString &other) : ptr_(other.ptr_), size_(other.size_) {
	}
	
	XString& operator=(const XString &rhs) {
		if(this != &rhs) {
			XString(rhs).swap(*this);
		}
		return *this;
	}
	
	XString(XString &&other) : ptr_(std::move(other.ptr_)), size_(other.size_) {
	}
	
	XString& operator=(XString &&rhs) {
		if(this != &rhs) {
			XString(std::move(rhs)).swap(*this);
		}
		return *this;
	}
	
	~XString() {
	}

public:
	size_t size() const {
		return size_;
	}
	
	bool empty() const {
		return size_ == 0;
	}
	
	explicit operator bool() const {
		return ptr_ != nullptr;
	}
	
public:
	iterator begin()             { return ptr_.get(); }
	iterator end()               { return ptr_.get() + size(); }
	const_iterator begin() const { return ptr_.get(); }
	const_iterator end() const   { return ptr_.get() + size(); }
	
public:
	char operator[](size_t index) const {
		return ptr_.get()[index];
	}
	
	char& operator[](size_t index) {
		return ptr_.get()[index];
	}
	
	char *c_str() {
		return ptr_.get();
	}
	
	const char *c_str() const {
		return ptr_.get();
	}
	
	char *data() {
		return ptr_.get();
	}
	
	const char *data() const {
		return ptr_.get();
	}	

public:
	int compare(const char *s) const {
		return strcmp(ptr_.get(), s);
	}
	
	int compare(const std::string &s) const {
		return strcmp(ptr_.get(), s.c_str());
	}	
	
	int compare(const XString &s) const {
		return strcmp(ptr_.get(), s.ptr_.get());
	}	
	
public:
	XmString toXmString() const {
		return XmStringCreateSimple(ptr_.get());
	}
	
public:
	// NOTE(eteran): takes ownership of the string instead of copying it
	// and thus will free it later
	static XString takeString(char *s) {
		XString str;
		str.ptr_  = std::shared_ptr<char>(s, XtFree);
		str.size_ = strlen(s);
		return str;
	}	
	
public:
	void swap(XString &other) {
		using std::swap;
		swap(ptr_, other.ptr_);
		swap(size_, other.size_);
	}
	
private:
	std::shared_ptr<char> ptr_;
	int                   size_;
};

inline bool operator==(const XString &lhs, const XString &rhs) {
	return lhs.compare(rhs) == 0;
}

inline bool operator!=(const XString &lhs, const XString &rhs) {
	return lhs.compare(rhs) != 0;
}

inline bool operator==(const char *lhs, const XString &rhs) {
	assert(lhs);
	return rhs.compare(lhs) == 0;
}

inline bool operator!=(const char *lhs, const XString &rhs) {
	assert(lhs);
	return rhs.compare(lhs) != 0;
}

inline bool operator==(const XString &lhs, const char *rhs) {
	assert(rhs);
	return lhs.compare(rhs) == 0;
}

inline bool operator!=(const XString &lhs, const char *rhs) {
	assert(rhs);
	return lhs.compare(rhs) != 0;
}

inline bool operator==(const std::string &lhs, const XString &rhs) {
	return rhs.compare(lhs) == 0;
}

inline bool operator!=(const std::string &lhs, const XString &rhs) {
	return rhs.compare(lhs) != 0;
}

inline bool operator==(const XString &lhs, const std::string &rhs) {
	return lhs.compare(rhs) == 0;
}

inline bool operator!=(const XString &lhs, const std::string &rhs) {
	return lhs.compare(rhs) != 0;
}

#endif
