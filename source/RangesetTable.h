
#ifndef RANGESET_TABLE_H_
#define RANGESET_TABLE_H_

#include "Rangeset.h"
#include "TextCursor.h"
#include <vector>

class RangesetTable {
public:
	explicit RangesetTable(TextBuffer *buffer);
	RangesetTable(const RangesetTable &)            = delete;
	RangesetTable& operator=(const RangesetTable &) = delete;
	~RangesetTable();

public:
	QString RangesetTableGetColorName(int index) const;
	Rangeset *RangesetFetch(int label);
	int RangesetCreate();
	int RangesetIndex1ofPos(TextCursor pos, bool needs_color);
	int RangesetTableGetColorValid(int index, QColor *color) const;
	int nRangesetsAvailable() const;
	std::vector<uint8_t> RangesetGetList() const;
	void RangesetForget(int label);
	void RangesetTableAssignColor(int index, const QColor &color);
	void RangesetTableUpdatePos(TextCursor pos, int64_t ins, int64_t del);

public:
	static bool RangesetLabelOK(int label);

public:
	TextBuffer *buffer_;
	std::vector<Rangeset> sets_;
};

#endif
