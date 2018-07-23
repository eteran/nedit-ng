
#ifndef CALL_TIP_H_
#define CALL_TIP_H_

#include "TextCursor.h"
#include <boost/variant.hpp>

enum class TipHAlignMode { Left, Center, Right };
enum class TipVAlignMode { Above, Below };
enum class TipAlignMode  { Sloppy, Strict };

// a cursor if it's anchored, otherwise it's a relative x position
using CallTipPosition = boost::variant<int, TextCursor>;

struct CallTip {
    int ID;                 //  ID of displayed calltip.  Equals zero if none is displayed.
    CallTipPosition pos;    //  Position tip is anchored to
    bool anchored;          //  Is it anchored to a position
    TipHAlignMode hAlign;   //  horizontal alignment
    TipVAlignMode vAlign;   //  vertical alignment
    TipAlignMode alignMode; //  Strict or sloppy alignment
};

#endif
