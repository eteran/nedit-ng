
#ifndef EXT_VARIANT_H_
#define EXT_VARIANT_H_

#if __cplusplus >= 201703L
#include <variant>

namespace ext {
template <class... T>
using variant = std::variant<T...>;

using monostate = std::monostate;

template <class... T>
constexpr std::size_t index(const variant<T...> &v) {
	return v.index();
}

using std::get;
using std::get_if;

}

#else
#include <boost/variant.hpp>
namespace ext {
template <class... T>
using variant = boost::variant<T...>;

using monostate = boost::blank;

template <class... T>
constexpr size_t index(const variant<T...> &v) {
	return v.which();
}

using boost::get;

template <class T, class Variant>
auto get_if(const Variant *v) {
	return boost::get<T>(v);
}

template <class T, class Variant>
auto get_if(Variant *v) {
	return boost::get<T>(v);
}

}
#endif

#endif
