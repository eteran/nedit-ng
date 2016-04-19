
#ifndef RANGESET_TABLE_H_
#define RANGESET_TABLE_H_

#include "Rangeset.h"

class RangesetTable {
public:
	explicit RangesetTable(TextBuffer *buffer);	
	RangesetTable(TextBuffer *buffer, const RangesetTable &other); // for "cloning"
	~RangesetTable();
	
public:
	char *RangesetTableGetColorName(int index);
	int nRangesetsAvailable() const;
	int RangesetCreate();
	int RangesetFindIndex(int label, int must_be_active) const;
	int RangesetIndex1ofPos(int pos, int needs_color);
	int RangesetTableAssignColorPixel(int index, Pixel color, int ok);	
	int RangesetTableGetColorValid(int index, Pixel *color);
	Rangeset *RangesetFetch(int label);
	Rangeset *RangesetForget(int label);
	unsigned char *RangesetGetList();
	void RangesetTableUpdatePos(int pos, int n_ins, int n_del);
	
public:
	static Range *RangesFree(Range *ranges);
	static Range *RangesNew(int n);
	static Range *RangesRealloc(Range *ranges, int n);
	static int RangesetLabelOK(int label);

public:
	int n_set;                           /* how many sets are active */
	TextBuffer *buf;                     /* the text buffer of the rangeset */
	Rangeset set[N_RANGESETS];           /* the rangeset table */
	unsigned char order[N_RANGESETS];    /* inds of set[]s ordered by depth */
	unsigned char active[N_RANGESETS];   /* entry true if corresp. set active */
	unsigned char depth[N_RANGESETS];    /* depth[i]: pos of set[i] in order[] */
	unsigned char list[N_RANGESETS + 1]; /* string of labels in depth order */
};

#endif
