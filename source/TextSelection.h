
#ifndef TEXT_SELECTION_H_
#define TEXT_SELECTION_H_

class TextSelection {
public:
    int getSelectionPos(int *start, int *end, bool *isRect, int *rectStart, int *rectEnd) const;
    bool inSelection(int pos, int lineStartPos, int dispIndex) const;
    bool rangeTouchesRectSel(int rangeStart, int rangeEnd) const;
	void setRectSelect(int newStart, int newEnd, int newRectStart, int newRectEnd);
	void setSelection(int newStart, int newEnd);
	void updateSelection(int pos, int nDeleted, int nInserted);

public:
	explicit operator bool() const { return selected; }

public:
    bool selected    = false; // True if the selection is active
    bool rectangular = false; // True if the selection is rectangular
    bool zeroWidth   = false; // Width 0 selections aren't "real" selections, but they can be useful when creating rectangular selections from the keyboard.
    int start        = 0;     // Pos. of start of selection, or if rectangular start of line containing it.
    int end          = 0;     // Pos. of end of selection, or if rectangular end of line containing it.
    int rectStart    = 0;     // Indent of left edge of rect. selection
    int rectEnd      = 0;     // Indent of right edge of rect. selection
};

#endif


