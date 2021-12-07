
#ifndef EXT_OPTIONAL_H_
#define EXT_OPTIONAL_H_

#if __cplusplus >= 201703L
#include <optional>

namespace ext {

template <class T>
using optional = std::optional<T>;

}

#else
#include <boost/optional.hpp>

namespace ext {

template <class T>
using optional = boost::optional<T>;

}

#endif

#endif
