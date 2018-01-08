
#ifndef CALL_TIP_H_
#define CALL_TIP_H_

#include <cstdint>

enum class TipHAlignMode  { Left, Center, Right };
enum class TipVAlignMode  { Above, Below };
enum class TipAlignStrict { Sloppy, Strict };

struct CallTip {
    int ID;                   //  ID of displayed calltip.  Equals zero if none is displayed.
    bool anchored;            //  Is it anchored to a position
    int64_t pos;              //  Position tip is anchored to
    TipHAlignMode hAlign;     //  horizontal alignment
    TipVAlignMode vAlign;     //  vertical alignment
    TipAlignStrict alignMode; //  Strict or sloppy alignment
};

#endif
