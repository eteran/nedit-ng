
#ifndef STRING_VIEW_H_
#define STRING_VIEW_H_

#include <cassert>
#include <iterator>
#include <stdexcept>
#include <string>

namespace view {

template <class Ch, class Tr = std::char_traits<Ch>>
class basic_string_view {
public:
	typedef Tr                                    traits_type;
	typedef Ch                                    value_type;
	typedef Ch*                                   pointer;
	typedef const Ch*                             const_pointer;
	typedef Ch&                                   reference;
	typedef const Ch&                             const_reference;
	typedef const Ch*                             const_iterator;
	typedef const_iterator                        iterator;
	typedef std::reverse_iterator<iterator>       reverse_iterator;
	typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
	typedef std::size_t                           size_type;
	typedef std::ptrdiff_t                        difference_type;

public:
	static constexpr size_type npos = size_type(-1);

public:
	constexpr basic_string_view() noexcept : data_(nullptr), size_(0) {
	}
	
	constexpr basic_string_view(const basic_string_view &other) noexcept = default;
	
	template <class A>
	basic_string_view(const std::basic_string<Ch, Tr, A> &str) noexcept : data_(str.data()), size_(str.size()) {
	}
	
	constexpr basic_string_view(const Ch *s, size_type count) : data_(s), size_(count) {
	}
	
	constexpr basic_string_view(const Ch *s) : data_(s), size_(Tr::length(s)) {
	}
	
	basic_string_view &operator=(const basic_string_view &rhs) noexcept = default;
	
#if 0
	template <class A>
	basic_string_view(const std::basic_string<Ch, Tr, A> &str, size_type pos, size_type count = npos)  : data_(nullptr), size_(0) {
		basic_string_view(str).substr(pos, count).swap(*this);
	}
#endif
	
public:
	constexpr const_iterator begin() const noexcept  { return data_; }
	constexpr const_iterator cbegin() const noexcept { return data_; }
	constexpr const_iterator end() const noexcept    { return data_ + size_; }
	constexpr const_iterator cend() const noexcept   { return data_ + size_; }	

	const_reverse_iterator rbegin() const noexcept  { return reverse_iterator(end()); }
	const_reverse_iterator crbegin() const noexcept { return reverse_iterator(end()); }
	const_reverse_iterator rend() const noexcept	{ return reverse_iterator(begin()); }
	const_reverse_iterator rcend() const noexcept	{ return reverse_iterator(begin()); }

public:
	constexpr const_reference operator[](size_type pos) const {
		return data_[pos];
	}
	
	/*constexpr */const_reference at(size_type pos) const {
		if(pos >= size_) {
			throw std::out_of_range("out_of_range");
		}
		return data_[pos];
	}
	
	constexpr const_reference front() const {
		return data_[0];
	}

	constexpr const_reference back() const {
		return data_[size_ - 1];
	}
	
	constexpr const_pointer data() const noexcept {
		return data_;
	}

public:
	constexpr size_type size() const noexcept {
		return size_;
	}
	
	constexpr size_type length() const noexcept {
		return size_;
	}
	
	constexpr size_type max_size() const noexcept {
		return size_type(-1) / sizeof(Ch);
	}	
	
	constexpr bool empty() const noexcept {
		return size() == 0;
	}
	
public:

	/* constexpr */void remove_prefix(size_type n) {
		data_ += n;
		size_ -= n;
	}
	
	/* constexpr */ void remove_suffix(size_type n) {
		size_ -= n;
	}
	
	/* constexpr */void swap(basic_string_view &other) noexcept {
		using std::swap;
		swap(data_, other.data_);
		swap(size_, other.size_);
	}
	
public:
	template <class A = std::allocator<Ch>>
	std::basic_string<Ch, Tr, A> to_string(const A &a = A()) const {
		if(data_ && size_ != 0) {
			return std::basic_string<Ch, Tr, A>(data_, size_, a);
		} else {
			return std::basic_string<Ch, Tr, A>(a);
		}
	}
	
	template <class A>
	explicit operator std::basic_string<Ch, Tr, A>() const {
		if(data_ && size_ != 0) {
			return std::basic_string<Ch, Tr, A>(data_, size_);
		} else {
			return std::basic_string<Ch, Tr, A>();
		}
	}
	
public:
	size_type copy(Ch *dest, size_type count, size_type pos = 0) const {
		if(pos >= size()) {
			throw std::out_of_range("out_of_range");
		}
		
		if(count >= size_) {
			count = size_ - pos;
		}		
		
		Tr::copy(dest, data_ + pos, count);
		return count;
	}
	
	//--------------------------------------------------------------------------
	
	/* constexpr*/ basic_string_view substr(size_type pos = 0, size_type count = npos) const {
		
		if(pos >= size()) {
			throw std::out_of_range("out_of_range");
		}
		
		if(count >= size_) {
			count = size_ - pos;
		}	
		
		return basic_string_view(data_ + pos, count);
	}
	
	//--------------------------------------------------------------------------
	
	/* constexpr */ int compare(basic_string_view v) const noexcept {
		size_type rlen = std::min(v.size(), size());
		int n = Tr::compare(data(), v.data(), rlen);
		
		if(n == 0) {
			return size() - v.size();
		} else {
			return n;
		}
	}
	
	constexpr int compare(size_type pos1, size_type count1, basic_string_view v) const {
		return substr(pos1, count1).compare(v);
	}
	
	constexpr int compare(size_type pos1, size_type count1, basic_string_view v, size_type pos2, size_type count2) const {
		return substr(pos1, count1).compare(v.substr(pos2, count2));
	}
	
	constexpr int compare(const Ch *s) const {
		return compare(basic_string_view(s));
	}
	
	constexpr int compare(size_type pos1, size_type count1, const Ch *s) const {
		return substr(pos1, count1).compare(s);
	}
	
	constexpr int compare(size_type pos1, size_type count1, const Ch *s, size_type count2) const {
		return substr(pos1, count1).compare(basic_string_view(s, count2));
	}
	
	//--------------------------------------------------------------------------
	
	/* constexpr */size_type find(basic_string_view v, size_type pos = 0) const noexcept {
		try {
			if(v.empty() || empty()) {
				return npos;
			}
			
			if(pos == npos) {
				pos = size_;
			}
			
			for(size_type i = pos; i < size_; ++i) {
				if(compare(i, v.size(), v) == 0) {
					return i;
				}
			}
	
			return npos;
		} catch(const std::exception &) {
			return npos;
		}
	}
	
	constexpr size_type find(Ch ch, size_type pos = 0) const noexcept {
		return find(basic_string_view(&ch, 1), pos);
	}
	
	constexpr size_type find(const Ch *s, size_type pos, size_type count) const {
		return find(basic_string_view(s, count), pos);
	}
	
	constexpr size_type find(const Ch *s, size_type pos = 0) const {
		return find(basic_string_view(s), pos);
	}
	
	//--------------------------------------------------------------------------
	
	/* constexpr */size_type rfind(basic_string_view v, size_type pos = npos) const noexcept {
	
		try {
			if(v.empty() || empty()) {
				return npos;
			}

			if(pos == npos) {
				pos = size_;
			}

			for(int i = pos - 1; i >= 0; --i) {
				if(compare(i, v.size(), v) == 0) {
					return i;
				}
			}

			return npos;
		} catch(const std::exception &) {
			return npos;
		}	
	}
	
	constexpr size_type rfind(Ch ch, size_type pos = npos) const noexcept {
		return rfind(basic_string_view(&ch, 1), pos);
	}
	
	constexpr size_type rfind(const Ch *s, size_type pos, size_type count) const {
		return rfind(basic_string_view(s, count), pos);
	}
	
	constexpr size_type rfind(const Ch *s, size_type pos = npos) const {
		return rfind(basic_string_view(s), pos);
	}
	
	//--------------------------------------------------------------------------
	
	/*constexpr */ size_type find_first_of(basic_string_view v, size_type pos = 0) const noexcept {
		if(v.empty() || empty()) {
			return npos;
		}
		
		for(size_type i = pos; i < size_; ++i) {
		
			for(char ch : v) {
				if(data_[i] == ch) {
					return i;
				}
			}
		}

		return npos;
	}
	
	constexpr size_type find_first_of(Ch ch, size_type pos = 0) const noexcept {
		return find_first_of(basic_string_view(&ch, 1), pos);
	}
	
	constexpr size_type find_first_of(const Ch *s, size_type pos, size_type count) const noexcept {
		return find_first_of(basic_string_view(s, count), pos);
	}
	
	constexpr size_type find_first_of(const Ch *s, size_type pos = 0) const noexcept {
		return find_first_of(basic_string_view(s), pos);
	}
	
	//--------------------------------------------------------------------------
	
	/*constexpr */ size_type find_last_of(basic_string_view v, size_type pos = npos) const noexcept {
		if(v.empty() || empty()) {
			return npos;
		}
		
		if(pos == npos) {
			pos = size_;
		}
		
		for(int i = pos - 1; i >= 0; --i) {
			for(char ch : v) {
				if(data_[i] == ch) {
					return i;
				}
			}
		}

		return npos;
	}
	
	constexpr size_type find_last_of(Ch ch, size_type pos = npos) const noexcept {
		return find_last_of(basic_string_view(&ch, 1), pos);
	}
	
	constexpr size_type find_last_of(const Ch *s, size_type pos, size_type count) const noexcept {
		return find_last_of(basic_string_view(s, count), pos);
	}
	
	constexpr size_type find_last_of(const Ch *s, size_type pos = npos) const noexcept {
		return find_last_of(basic_string_view(s), pos);
	}
	
	//--------------------------------------------------------------------------
	
	/*constexpr */ size_type find_first_not_of(basic_string_view v, size_type pos = 0) const noexcept {
		if(v.empty() || empty()) {
			return npos;
		}
		
		for(size_type i = pos; i < size_; ++i) {
		
			bool found = false;
			for(char ch : v) {
				if(data_[i] == ch) {
					found = true;
					break;
				}
			}
			
			if(!found) {
				return i;
			}
		}

		return npos;
	}
	
	constexpr size_type find_first_not_of(Ch ch, size_type pos = 0) const noexcept {
		return find_first_not_of(basic_string_view(&ch, 1), pos);
	}
	
	constexpr size_type find_first_not_of(const Ch *s, size_type pos, size_type count) const noexcept {
		return find_first_not_of(basic_string_view(s, count), pos);
	}
	
	constexpr size_type find_first_not_of(const Ch *s, size_type pos = 0) const noexcept {
		return find_first_not_of(basic_string_view(s), pos);
	}
	
	//--------------------------------------------------------------------------
	
	/*constexpr */ size_type find_last_not_of(basic_string_view v, size_type pos = npos) const noexcept {
		if(v.empty() || empty()) {
			return npos;
		}
		
		if(pos == npos) {
			pos = size_;
		}
		
		for(int i = pos - 1; i >= 0; --i) {
			bool found = false;
			for(char ch : v) {
				if(data_[i] == ch) {
					found = true;
					break;
				}
			}
			
			if(!found) {
				return i;
			}
		}

		return npos;
	}
	
	constexpr size_type find_last_not_of(Ch ch, size_type pos = npos) const noexcept {
		return find_last_not_of(basic_string_view(&ch, 1), pos);
	}
	
	constexpr size_type find_last_not_of(const Ch *s, size_type pos, size_type count) const noexcept {
		return find_last_not_of(basic_string_view(s, count), pos);
	}
	
	constexpr size_type find_last_not_of(const Ch *s, size_type pos = npos) const noexcept {
		return find_last_not_of(basic_string_view(s), pos);
	}
	
private:
	const Ch *data_;
	size_type size_;
};


//--------------------------------------------------------------------------

template <class Ch, class Tr>
constexpr bool operator==(basic_string_view<Ch, Tr> lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return lhs.compare(rhs) == 0;
}

template <class Ch, class Tr>
constexpr bool operator!=(basic_string_view<Ch, Tr> lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return lhs.compare(rhs) != 0;
}

template <class Ch, class Tr>
constexpr bool operator<(basic_string_view<Ch, Tr> lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return lhs.compare(rhs) < 0;
}

template <class Ch, class Tr>
constexpr bool operator<=(basic_string_view<Ch, Tr> lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return lhs.compare(rhs) <= 0;
}

template <class Ch, class Tr>
constexpr bool operator>(basic_string_view<Ch, Tr> lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return lhs.compare(rhs) > 0;
}

template <class Ch, class Tr>
constexpr bool operator>=(basic_string_view<Ch, Tr> lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return lhs.compare(rhs) >= 0;
}




template <class Ch, class Tr>
constexpr bool operator==(basic_string_view<Ch, Tr> lhs, const std::basic_string<Ch, Tr> &rhs) noexcept {
	return lhs == basic_string_view<Ch, Tr>(rhs);
}

template <class Ch, class Tr>
constexpr bool operator!=(basic_string_view<Ch, Tr> lhs, const std::basic_string<Ch, Tr> &rhs) noexcept {
	return lhs != basic_string_view<Ch, Tr>(rhs);
}

template <class Ch, class Tr>
constexpr bool operator<(basic_string_view<Ch, Tr> lhs, const std::basic_string<Ch, Tr> &rhs) noexcept {
	return lhs < basic_string_view<Ch, Tr>(rhs);
}

template <class Ch, class Tr>
constexpr bool operator<=(basic_string_view<Ch, Tr> lhs, const std::basic_string<Ch, Tr> &rhs) noexcept {
	return lhs <= basic_string_view<Ch, Tr>(rhs);
}

template <class Ch, class Tr>
constexpr bool operator>(basic_string_view<Ch, Tr> lhs, const std::basic_string<Ch, Tr> &rhs) noexcept {
	return lhs > basic_string_view<Ch, Tr>(rhs);
}

template <class Ch, class Tr>
constexpr bool operator>=(basic_string_view<Ch, Tr> lhs, const std::basic_string<Ch, Tr> &rhs) noexcept {
	return lhs >= basic_string_view<Ch, Tr>(rhs);
}



template <class Ch, class Tr>
constexpr bool operator==(const std::basic_string<Ch, Tr> &lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return basic_string_view<Ch, Tr>(lhs) == rhs;
}

template <class Ch, class Tr>
constexpr bool operator!=(const std::basic_string<Ch, Tr> &lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return basic_string_view<Ch, Tr>(lhs) != rhs;
}

template <class Ch, class Tr>
constexpr bool operator<(const std::basic_string<Ch, Tr> &lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return basic_string_view<Ch, Tr>(lhs) < rhs;
}

template <class Ch, class Tr>
constexpr bool operator<=(const std::basic_string<Ch, Tr> &lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return basic_string_view<Ch, Tr>(lhs) <= rhs;
}

template <class Ch, class Tr>
constexpr bool operator>(const std::basic_string<Ch, Tr> &lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return basic_string_view<Ch, Tr>(lhs) > rhs;
}

template <class Ch, class Tr>
constexpr bool operator>=(const std::basic_string<Ch, Tr> &lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return basic_string_view<Ch, Tr>(lhs) >= rhs;
}



template <class Ch, class Tr>
constexpr bool operator==(basic_string_view<Ch, Tr> lhs, const char *rhs) noexcept {
	return lhs == basic_string_view<Ch, Tr>(rhs);
}

template <class Ch, class Tr>
constexpr bool operator!=(basic_string_view<Ch, Tr> lhs, const char *rhs) noexcept {
	return lhs != basic_string_view<Ch, Tr>(rhs);
}

template <class Ch, class Tr>
constexpr bool operator<(basic_string_view<Ch, Tr> lhs, const char *rhs) noexcept {
	return lhs < basic_string_view<Ch, Tr>(rhs);
}

template <class Ch, class Tr>
constexpr bool operator<=(basic_string_view<Ch, Tr> lhs, const char *rhs) noexcept {
	return lhs <= basic_string_view<Ch, Tr>(rhs);
}

template <class Ch, class Tr>
constexpr bool operator>(basic_string_view<Ch, Tr> lhs, const char *rhs) noexcept {
	return lhs > basic_string_view<Ch, Tr>(rhs);
}

template <class Ch, class Tr>
constexpr bool operator>=(basic_string_view<Ch, Tr> lhs, const char *rhs) noexcept {
	return lhs >= basic_string_view<Ch, Tr>(rhs);
}



template <class Ch, class Tr>
constexpr bool operator==(const char *lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return basic_string_view<Ch, Tr>(lhs) == rhs;
}

template <class Ch, class Tr>
constexpr bool operator!=(const char *lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return basic_string_view<Ch, Tr>(lhs) != rhs;
}

template <class Ch, class Tr>
constexpr bool operator<(const char *lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return basic_string_view<Ch, Tr>(lhs) < rhs;
}

template <class Ch, class Tr>
constexpr bool operator<=(const char *lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return basic_string_view<Ch, Tr>(lhs) <= rhs;
}

template <class Ch, class Tr>
constexpr bool operator>(const char *lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return basic_string_view<Ch, Tr>(lhs) > rhs;
}

template <class Ch, class Tr>
constexpr bool operator>=(const char *lhs, basic_string_view<Ch, Tr> rhs) noexcept {
	return basic_string_view<Ch, Tr>(lhs) >= rhs;
}


//--------------------------------------------------------------------------

template <class Ch, class Tr>
std::basic_ostream<Ch, Tr> &operator<<(std::basic_ostream<Ch, Tr> &os, basic_string_view<Ch, Tr> v) {
	os << v.to_string();
	return os;
}

//--------------------------------------------------------------------------

typedef basic_string_view<char>     string_view;
typedef basic_string_view<wchar_t>  wstring_view;
typedef basic_string_view<char16_t> u16string_view;
typedef basic_string_view<char32_t> u32string_view;

//--------------------------------------------------------------------------

// some utility functions for convienience, not part of the standard proposal

template <class Ch, class Tr>
basic_string_view<Ch, Tr> substr(const basic_string_view<Ch, Tr> &str, typename basic_string_view<Ch, Tr>::size_type pos, typename basic_string_view<Ch, Tr>::size_type count = basic_string_view<Ch, Tr>::npos) {
	return str.substr(pos, count);
}

template <class Ch, class Tr, class A>
basic_string_view<Ch, Tr> substr(typename std::basic_string<Ch, Tr, A>::const_iterator first, typename std::basic_string<Ch, Tr, A>::const_iterator last) {

	Ch *data = &*first;
	typename basic_string_view<Ch, Tr>::size_type size = std::distance(first, last);

	return basic_string_view<Ch, Tr>(data, size);
	
}

template <class Ch, class Tr = std::char_traits<Ch>>
basic_string_view<Ch, Tr> substr(const Ch *first, const Ch *last) {

	const Ch *data = first;
	typename basic_string_view<Ch, Tr>::size_type size = std::distance(first, last);

	return basic_string_view<Ch, Tr>(data, size);
	
}

}

//--------------------------------------------------------------------------

namespace std {

template <class Key>
struct hash;

template <>
struct hash<view::string_view> {
	typedef view::string_view argument_type;
	typedef size_t            result_type;
	
	result_type operator()(argument_type key) {
		return hash<string>()(key.to_string());
	}
};

template <>
struct hash<view::wstring_view> {
	typedef view::wstring_view argument_type;
	typedef size_t             result_type;
	
	result_type operator()(argument_type key) {
		return hash<wstring>()(key.to_string());
	}	
};

template <>
struct hash<view::u16string_view> {
	typedef view::u16string_view argument_type;
	typedef size_t               result_type;
	
	result_type operator()(argument_type key) {
		return hash<u16string>()(key.to_string());
	}	
};

template <>
struct hash<view::u32string_view> {
	typedef view::u32string_view argument_type;
	typedef size_t               result_type;
	
	result_type operator()(argument_type key) {
		return hash<u32string>()(key.to_string());
	}	
};

}

#endif
