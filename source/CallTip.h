
#ifndef CALL_TIP_H_
#define CALL_TIP_H_

enum class TipHAlignMode { Left, Center, Right };
enum class TipVAlignMode { Above, Below };
enum class TipAlignMode  { Sloppy, Strict };

struct CallTip {
    int ID;                 //  ID of displayed calltip.  Equals zero if none is displayed.
    bool anchored;          //  Is it anchored to a position
    int pos;                //  Position tip is anchored to
    TipHAlignMode hAlign;   //  horizontal alignment
    TipVAlignMode vAlign;   //  vertical alignment
    TipAlignMode alignMode; //  Strict or sloppy alignment
};

#endif
