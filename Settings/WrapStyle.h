
#ifndef WRAP_STYLE_H_
#define WRAP_STYLE_H_

#include "FromInteger.h"

#include <QLatin1String>
#include <QtDebug>

enum class WrapStyle : int {
	Default    = -1,
	None       = 0,
	Newline    = 1,
	Continuous = 2
};

template <>
inline WrapStyle FromInteger(int value) {
	switch (value) {
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

inline QLatin1String ToString(WrapStyle style) {

	switch (style) {
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
