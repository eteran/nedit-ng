
#ifndef FONT_TYPE_H_
#define FONT_TYPE_H_

enum FontType {
    PLAIN_FONT       = 0,
    ITALIC_FONT      = 1,
    BOLD_FONT        = 2,
    BOLD_ITALIC_FONT = 4,
};

// TODO(eteran): get rid of BOLD_ITALIC_FONT and replace it with the concept of
// simply ORing these values together.

#endif
