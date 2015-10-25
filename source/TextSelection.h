
#ifndef TEXT_SELECTION_H_
#define TEXT_SELECTION_H_

class TextSelection {
public:
	TextSelection();
	TextSelection(const TextSelection &) = default;
	TextSelection &operator=(const TextSelection &) = default;
	~TextSelection() = default;
	
public:
	char selected;    /* True if the selection is active */
	char rectangular; /* True if the selection is rectangular */
	char zeroWidth;   /* Width 0 selections aren't "real" selections, but they can be useful when creating rectangular selections from the keyboard. */
	int start;        /* Pos. of start of selection, or if rectangular start of line containing it. */
	int end;          /* Pos. of end of selection, or if rectangular end of line containing it. */
	int rectStart;    /* Indent of left edge of rect. selection */
	int rectEnd;      /* Indent of right edge of rect. selection */
};

#endif
