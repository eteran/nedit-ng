
#ifndef RANGESET_TABLE_H_
#define RANGESET_TABLE_H_

#include "Rangeset.h"
#include "TextCursor.h"
#include <memory>
#include <vector>

class RangesetTable {
public:
	explicit RangesetTable(TextBuffer *buffer);
	RangesetTable(const RangesetTable &)            = delete;
	RangesetTable& operator=(const RangesetTable &) = delete;
	~RangesetTable();

public:
	int nRangesetsAvailable() const;
	int RangesetCreate();
	int RangesetFindIndex(int label, bool must_be_active) const;
	int RangesetIndex1ofPos(TextCursor pos, bool needs_color);
	int RangesetTableGetColorValid(int index, QColor *color) const;
	QString RangesetTableGetColorName(int index) const;
	Rangeset *RangesetFetch(int label);
	Rangeset *RangesetForget(int label);
	std::vector<uint8_t> RangesetGetList() const;
	void RangesetTableAssignColorPixel(int index, const QColor &color);
	void RangesetTableUpdatePos(TextCursor pos, int64_t ins, int64_t del);

public:
	static bool RangesetLabelOK(int label);

public:
	int n_set_;                   /* how many sets are active */
	TextBuffer *buf_;             /* the text buffer of the rangeset */
	Rangeset set_[N_RANGESETS];   /* the rangeset table */
	uint8_t order_[N_RANGESETS];  /* inds of set[]s ordered by depth */
	uint8_t depth_[N_RANGESETS];  /* depth[i]: pos of set[i] in order[] */
	bool active_[N_RANGESETS];    /* entry true if corresp. set active */
};

#endif
