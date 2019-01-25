
#ifndef WRAP_STYLE_H_
#define WRAP_STYLE_H_

#include <QLatin1String>
#include <QtDebug>

enum class WrapStyle : int {
	Default    = -1,
	None       = 0,
	Newline    = 1,
	Continuous = 2
};

template <class T>
inline T from_integer(int value);

template <>
inline WrapStyle from_integer(int value) {
	switch(value) {
	case static_cast<int>(WrapStyle::Default):
	case static_cast<int>(WrapStyle::None):
	case static_cast<int>(WrapStyle::Newline):
	case static_cast<int>(WrapStyle::Continuous):
		return static_cast<WrapStyle>(value);
	default:
		qWarning("NEdit: Invalid value for WrapStyle");
		return WrapStyle::Default;
	}
}

inline constexpr QLatin1String to_string(WrapStyle style) {

	switch(style) {
	case WrapStyle::None:
		return QLatin1String("none");
	case WrapStyle::Newline:
		return QLatin1String("auto");
	case WrapStyle::Continuous:
		return QLatin1String("continuous");
	case WrapStyle::Default:
		return QLatin1String("default");
	}

	Q_UNREACHABLE();
}

#endif
