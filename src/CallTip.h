
#ifndef CALL_TIP_H_
#define CALL_TIP_H_

#include "TextCursor.h"

#include <variant>

enum class TipHAlignMode {
	Left,
	Center,
	Right,
};

enum class TipVAlignMode {
	Above,
	Below,
};

enum class TipAlignMode {
	Sloppy,
	Strict,
};

// a cursor if it's anchored, otherwise it's a relative x position
using CallTipPosition = std::variant<int, TextCursor>;

struct CallTip {
	int ID                 = 0;                    // ID of displayed calltip.  Equals zero if none is displayed.
	CallTipPosition pos    = {0};                  // Position tip is anchored to
	bool anchored          = false;                // Is it anchored to a position
	TipHAlignMode hAlign   = TipHAlignMode::Left;  // horizontal alignment
	TipVAlignMode vAlign   = TipVAlignMode::Above; // vertical alignment
	TipAlignMode alignMode = TipAlignMode::Strict; // Strict or sloppy alignment
};

#endif
