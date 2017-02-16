
#ifndef CALL_TIP_H_
#define CALL_TIP_H_

struct CallTip {
	int ID;           //  ID of displayed calltip.  Equals zero if none is displayed.
	bool anchored; //  Is it anchored to a position
	int pos;          //  Position tip is anchored to
	int hAlign;       //  horizontal alignment
	int vAlign;       //  vertical alignment
	int alignMode;    //  Strict or sloppy alignment
};

#endif
