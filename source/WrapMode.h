
#ifndef WRAP_MODE_H_
#define WRAP_MODE_H_

#include <QLatin1String>

enum class WrapMode {
    NoWrap,
	Wrap
};

inline QLatin1String to_string(WrapMode wrap) {

    switch(wrap) {
    case WrapMode::NoWrap:
        return QLatin1String("nowrap");
    case WrapMode::Wrap:
        return QLatin1String("wrap");
    }

    Q_UNREACHABLE();
}

#endif
