
#ifndef SHOW_MATCHING_STYLE_H_
#define SHOW_MATCHING_STYLE_H_

#include <QMetaType>

enum class ShowMatchingStyle {
    None,
    Delimeter,
    Range
};

Q_DECLARE_METATYPE(ShowMatchingStyle)

#endif
