
#ifndef XSTRING_H_
#define XSTRING_H_

#include <memory>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <Xm/Xm.h>
#include "MotifHelper.h"

// NOTE(eteran): this does more copies than necessary, we could do reference
//               counting, but we would rather this be "obviously" correct

class XString {
public:
	typedef char*       iterator;
	typedef const char* const_iterator;
	
public:
	XString() : ptr_(nullptr), size_(0) {
	}
	
	explicit XString(nullptr_t) : ptr_(nullptr), size_(0) {
	}
	
	explicit XString(const std::string &text) : ptr_(nullptr), size_(0) {
		size_ = text.size();
		ptr_  = XtMalloc(size_ + 1);
		assert(ptr_);
		std::copy(text.begin(), text.end(), ptr_);
		ptr_[size_] = '\0';
	}
	
	explicit XString(const char *text) : ptr_(nullptr), size_(0) {
		if(text) {
			size_ = strlen(text);
			ptr_  = XtMalloc(size_ + 1);
			assert(ptr_);
			strcpy(ptr_, text);
		}
	}	
	
	XString(const char *text, size_t length) : ptr_(nullptr), size_(length) {
		if(text) {	
			size_ = length;	
			ptr_  = XtMalloc(size_ + 1);
			assert(ptr_);
			std::copy_n(text, length, ptr_);
			ptr_[size_] = '\0';
		}
	}
	
	XString(const XString &other) : ptr_(nullptr), size_(0) {
		if(other.ptr_) {	
			size_ = other.size_;
			ptr_  = XtMalloc(size_ + 1);
			assert(ptr_);
			std::copy(other.begin(), other.end(), ptr_);
			ptr_[size_] = '\0';
		}	
	}
	
	XString& operator=(const XString &rhs) {
		if(this != &rhs) {
			XString(rhs).swap(*this);
		}
		return *this;
	}
	
	XString& operator=(const std::string &rhs) {
		XString(rhs).swap(*this);
		return *this;
	}	
	
	XString(XString &&other) : ptr_(other.ptr_), size_(other.size_) {
		other.ptr_  = nullptr;
		other.size_ = 0;
	}
	
	XString& operator=(XString &&rhs) {
		if(this != &rhs) {
			XString(std::move(rhs)).swap(*this);
		}
		return *this;
	}

	~XString() {
		XtFree(ptr_);
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
	iterator begin()             { return ptr_;          }
	iterator end()               { return ptr_ + size_; }
	const_iterator begin() const { return ptr_;          }
	const_iterator end() const   { return ptr_ + size_; }
	
public:
	char operator[](size_t index) const {
		return ptr_[index];
	}
	
	char& operator[](size_t index) {
		return ptr_[index];
	}
	
	char *str() {
		return ptr_;
	}
	
	const char *str() const {
		return ptr_;
	}
	
	char *data() {
		return ptr_;
	}
	
	const char *data() const {
		return ptr_;
	}	

public:
	int compare(const char *s) const {
	
		if(!ptr_ && !s) {
			return 0;
		}
		
		if(!ptr_ && s) {
			return -1;
		}
		
		if(ptr_ && !s) {
			return 1;
		}
	
		return strcmp(ptr_, s);
	}
	
	int compare(const std::string &s) const {
		return strcmp(ptr_, s.c_str());
	}	
	
	int compare(const XString &s) const {
	
		if(!ptr_ && !s.ptr_) {
			return 0;
		}
		
		if(!ptr_ && s.ptr_) {
			return -1;
		}
		
		if(ptr_ && !s.ptr_) {
			return 1;
		}	
	
		return strcmp(ptr_, s.ptr_);
	}	
	
public:
	XmString toXmString() const {
		return XmStringCreateSimple(ptr_);
	}
	
public:
	// NOTE(eteran): takes ownership of the string instead of copying it
	// and thus will free it later
	static XString takeString(char *s) {
		XString str;
		str.ptr_  = s;
		str.size_ = strlen(s);
		return str;
	}
	
	template <class... Args>
	static XString format(const char *format, Args... args) {
	
		int length = snprintf(nullptr, 0, format, args...);	
		char *s    = XtMalloc(length + 1);
		assert(s);
		snprintf(s, length + 1, format, args...);	
		
		XString str;
		str.ptr_  = s;
		str.size_ = length;
		return str;
	}
	
public:
	void swap(XString &other) {
		using std::swap;
		swap(ptr_,  other.ptr_);
		swap(size_, other.size_);
	}
	
private:
	char  *ptr_;
	size_t size_;
};

inline bool operator==(const XString &lhs, const XString &rhs) {
	return lhs.compare(rhs) == 0;
}

inline bool operator!=(const XString &lhs, const XString &rhs) {
	return lhs.compare(rhs) != 0;
}

inline bool operator==(const char *lhs, const XString &rhs) {
	return rhs.compare(lhs) == 0;
}

inline bool operator!=(const char *lhs, const XString &rhs) {
	return rhs.compare(lhs) != 0;
}

inline bool operator==(const XString &lhs, const char *rhs) {
	return lhs.compare(rhs) == 0;
}

inline bool operator!=(const XString &lhs, const char *rhs) {
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
