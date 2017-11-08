
#ifndef INDENT_STYLE_H_
#define INDENT_STYLE_H_

#include <QString>

enum class IndentStyle {
    Default = -1,
    None    = 0,
    Auto    = 1,
    Smart   = 2
};

inline QLatin1String to_string(IndentStyle style) {

    switch(style) {
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
