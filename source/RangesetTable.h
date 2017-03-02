
#ifndef RANGESET_TABLE_H_
#define RANGESET_TABLE_H_

#include "Rangeset.h"

class RangesetTable {
public:
	explicit RangesetTable(TextBuffer *buffer);	
	RangesetTable(TextBuffer *buffer, const RangesetTable &other); // for "cloning"
	~RangesetTable();
	
public:
    QString RangesetTableGetColorName(int index);
	int nRangesetsAvailable() const;
	int RangesetCreate();	
    int RangesetTableAssignColorPixel(int index, const QColor &color, bool ok);
    int RangesetTableGetColorValid(int index, QColor *color);
	Rangeset *RangesetFetch(int label);
	Rangeset *RangesetForget(int label);

public:
    static void RangesetTableUpdatePos(RangesetTable *table, int pos, int n_ins, int n_del);
    static uint8_t *RangesetGetList(RangesetTable *table);
    static int RangesetFindIndex(RangesetTable *table, int label, int must_be_active);
    static int RangesetIndex1ofPos(RangesetTable *table, int pos, int needs_color);
	static Range *RangesFree(Range *ranges);
	static Range *RangesNew(int n);
	static Range *RangesRealloc(Range *ranges, int n);
	static int RangesetLabelOK(int label);

public:
    int n_set_;                     /* how many sets are active */
    TextBuffer *buf_;               /* the text buffer of the rangeset */
    Rangeset set_[N_RANGESETS];     /* the rangeset table */
    uint8_t order_[N_RANGESETS];    /* inds of set[]s ordered by depth */
    uint8_t active_[N_RANGESETS];   /* entry true if corresp. set active */
    uint8_t depth_[N_RANGESETS];    /* depth[i]: pos of set[i] in order[] */
    uint8_t list_[N_RANGESETS + 1]; /* string of labels in depth order */
};

#endif
