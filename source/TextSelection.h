
#ifndef TEXT_SELECTION_H_
#define TEXT_SELECTION_H_

#include <cstdint>

class TextSelection {
public:
    bool getSelectionPos(int64_t *start, int64_t *end, bool *isRect, int64_t *rectStart, int64_t *rectEnd) const;
    bool inSelection(int64_t pos, int64_t lineStartPos, int64_t dispIndex) const;
    bool rangeTouchesRectSel(int64_t rangeStart, int64_t rangeEnd) const;
    void setRectSelect(int64_t newStart, int64_t newEnd, int64_t newRectStart, int64_t newRectEnd);
    void setSelection(int64_t newStart, int64_t newEnd);
    void updateSelection(int64_t pos, int64_t nDeleted, int64_t nInserted);

public:
	explicit operator bool() const { return selected; }

public:
    bool selected    = false; // true if the selection is active
    bool rectangular = false; // true if the selection is rectangular
    bool zeroWidth   = false; // Width 0 selections aren't "real" selections, but they can be useful when creating rectangular selections from the keyboard.
    int64_t start        = 0; // Pos. of start of selection, or if rectangular start of line containing it.
    int64_t end          = 0; // Pos. of end of selection, or if rectangular end of line containing it.
    int64_t rectStart    = 0; // Indent of left edge of rect. selection
    int64_t rectEnd      = 0; // Indent of right edge of rect. selection
};

#endif


