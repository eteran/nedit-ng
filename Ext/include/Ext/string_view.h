
#ifndef EXT_STRING_VIEW_H_
#define EXT_STRING_VIEW_H_

#if __cplusplus >= 201703L
#include <string_view>

namespace ext {

template <class Ch, class Tr = std::char_traits<Ch>>
using basic_string_view = std::basic_string_view<Ch, Tr>;

using string_view    = std::string_view;
using wstring_view   = std::wstring_view;
using u16string_view = std::u16string_view;
using u32string_view = std::u32string_view;

}
#else

#include <boost/utility/string_view.hpp>

namespace ext {

template <class Ch, class Tr = std::char_traits<Ch>>
using basic_string_view = boost::basic_string_view<Ch, Tr>;

using string_view    = boost::string_view;
using wstring_view   = boost::wstring_view;
using u16string_view = boost::u16string_view;
using u32string_view = boost::u32string_view;

}

#endif

#endif
