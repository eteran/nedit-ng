
#ifndef RANGESET_TABLE_H_
#define RANGESET_TABLE_H_

#include "Rangeset.h"
#include <memory>
#include <vector>

class RangesetTable {
public:
    explicit RangesetTable(TextBuffer *buffer);
    RangesetTable(const RangesetTable &)            = delete;
    RangesetTable& operator=(const RangesetTable &) = delete;
	~RangesetTable();

public:
    QString RangesetTableGetColorName(int index);
	int nRangesetsAvailable() const;
	int RangesetCreate();	
    void RangesetTableAssignColorPixel(int index, const QColor &color, bool ok);
    int RangesetTableGetColorValid(int index, QColor *color) const;
	Rangeset *RangesetFetch(int label);
	Rangeset *RangesetForget(int label);
    int RangesetFindIndex(int label, bool must_be_active) const;
    std::vector<uint8_t> RangesetGetList() const;
    int RangesetIndex1ofPos(int pos, bool needs_color);
    void RangesetTableUpdatePos(int pos, int ins, int del);

public:
	static int RangesetLabelOK(int label);

public:
    int n_set_;				        /* how many sets are active */
    TextBuffer *buf_;			    /* the text buffer of the rangeset */
    Rangeset set_[N_RANGESETS];		/* the rangeset table */
    uint8_t order_[N_RANGESETS];	/* inds of set[]s ordered by depth */
    uint8_t active_[N_RANGESETS];	/* entry true if corresp. set active */
    uint8_t depth_[N_RANGESETS];	/* depth[i]: pos of set[i] in order[] */
};

#endif
