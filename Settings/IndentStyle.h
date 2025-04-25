
#ifndef INDENT_STYLE_H_
#define INDENT_STYLE_H_

#include "FromInteger.h"

#include <QLatin1String>
#include <QtDebug>

enum class IndentStyle {
	Default = -1,
	None    = 0,
	Auto    = 1,
	Smart   = 2,
};

template <>
inline IndentStyle FromInteger(int value) {
	switch (value) {
	case static_cast<int>(IndentStyle::Default):
	case static_cast<int>(IndentStyle::None):
	case static_cast<int>(IndentStyle::Auto):
	case static_cast<int>(IndentStyle::Smart):
		return static_cast<IndentStyle>(value);
	default:
		qWarning("NEdit: Invalid value for IndentStyle");
		return IndentStyle::Default;
	}
}

inline QLatin1String ToString(IndentStyle style) {

	switch (style) {
	case IndentStyle::None:
		return QLatin1String("off");
	case IndentStyle::Auto:
		return QLatin1String("on");
	case IndentStyle::Smart:
		return QLatin1String("smart");
	case IndentStyle::Default:
		return QLatin1String("default");
	}

	Q_UNREACHABLE();
}

#endif
