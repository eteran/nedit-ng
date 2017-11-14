
#include "HighlightPattern.h"

bool HighlightPattern::operator==(const HighlightPattern &rhs) const {

    if (flags != rhs.flags) {
        return false;
    }

    if (name != rhs.name) {
        return false;
    }

    if (startRE != rhs.startRE) {
        return false;
    }

    if (endRE != rhs.endRE) {
        return false;
    }

    if (errorRE != rhs.errorRE) {
        return false;
    }

    if(style != rhs.style) {
        return false;
    }

    if (subPatternOf != rhs.subPatternOf) {
        return false;
    }

    return true;

}

bool HighlightPattern::operator!=(const HighlightPattern &rhs) const {
    return !(*this == rhs);
}
