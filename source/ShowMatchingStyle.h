
#ifndef SHOW_MATCHING_STYLE_H_
#define SHOW_MATCHING_STYLE_H_

#include <QtDebug>

enum class ShowMatchingStyle {
    None,
    Delimeter,
    Range
};

template <class T>
inline T from_integer(int value);

template <>
inline ShowMatchingStyle from_integer(int value) {
    switch(value) {
    case static_cast<int>(ShowMatchingStyle::None):
    case static_cast<int>(ShowMatchingStyle::Delimeter):
    case static_cast<int>(ShowMatchingStyle::Range):
        return static_cast<ShowMatchingStyle>(value);
    default:
        qWarning("NEdit: Invalid value for ShowMatchingStyle");
        return ShowMatchingStyle::None;
    }
}

#endif
