
#ifndef SHOW_MATCHING_STYLE_H_
#define SHOW_MATCHING_STYLE_H_

#include <QLatin1String>
#include <QtDebug>

enum class ShowMatchingStyle {
	None,
	Delimiter,
	Range
};

template <class T>
inline T from_integer(int value);

template <>
inline ShowMatchingStyle from_integer(int value) {
	switch(value) {
	case static_cast<int>(ShowMatchingStyle::None):
	case static_cast<int>(ShowMatchingStyle::Delimiter):
	case static_cast<int>(ShowMatchingStyle::Range):
		return static_cast<ShowMatchingStyle>(value);
	default:
		qWarning("NEdit: Invalid value for ShowMatchingStyle");
		return ShowMatchingStyle::None;
	}
}

inline QLatin1String to_string(ShowMatchingStyle style) {

	switch(style) {
	case ShowMatchingStyle::None:
		return QLatin1String("none");
	case ShowMatchingStyle::Delimiter:
		return QLatin1String("delimeter");
	case ShowMatchingStyle::Range:
		return QLatin1String("range");
	}

	Q_UNREACHABLE();
}

#endif
