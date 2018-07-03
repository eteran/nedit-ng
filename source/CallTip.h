
#ifndef CALL_TIP_H_
#define CALL_TIP_H_

#include "TextCursor.h"
#include <cstdint>
#include <boost/variant.hpp>

enum class TipHAlignMode { Left, Center, Right };
enum class TipVAlignMode { Above, Below };
enum class TipAlignMode  { Sloppy, Strict };

struct CallTip {
    int ID;                              //  ID of displayed calltip.  Equals zero if none is displayed.
    boost::variant<int, TextCursor> pos; //  Position tip is anchored to (It's a cursor if it's anchored, otherwise it's a relative x position)
    bool anchored;                       //  Is it anchored to a position
    TipHAlignMode hAlign;                //  horizontal alignment
    TipVAlignMode vAlign;                //  vertical alignment
    TipAlignMode alignMode;              //  Strict or sloppy alignment
};

#endif
