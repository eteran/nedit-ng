
#include "util/MotifHelper.h"
#include "RangesetTable.h"
#include "TextBuffer.h"
#include <string>

namespace {

// -------------------------------------------------------------------------- 

static uint8_t rangeset_labels[N_RANGESETS + 1] = {58, 10, 15, 1,  27, 52, 14, 3,  61, 13, 31, 30, 45, 28, 41, 55, 33, 20, 62, 34, 42, 18, 57, 47, 24, 49, 19, 50, 25, 38, 40, 2,
                                                   21, 39, 59, 22, 60, 4,  6,  16, 29, 37, 48, 46, 54, 43, 32, 56, 51, 7,  9,  63, 5,  8,  36, 44, 26, 11, 23, 17, 53, 35, 12, 0};
														 
														 
// -------------------------------------------------------------------------- 

static void RangesetBufModifiedCB(int pos, int nInserted, int nDeleted, int nRestyled, view::string_view deletedText, void *cbArg) {
	(void)nRestyled;

	auto table = static_cast<RangesetTable *>(cbArg);
	if ((nInserted != nDeleted) || table->buf->BufCmpEx(pos, nInserted, deletedText) != 0) {
		table->RangesetTableUpdatePos(pos, nInserted, nDeleted);
	}
}

/*
** clone a ranges set.
*/
static void rangesetClone(Rangeset *destRangeset, const Rangeset *srcRangeset) {
	
	destRangeset->update_fn   = srcRangeset->update_fn;
	destRangeset->update_name = srcRangeset->update_name;
	destRangeset->maxpos      = srcRangeset->maxpos;
	destRangeset->last_index  = srcRangeset->last_index;
	destRangeset->n_ranges    = srcRangeset->n_ranges;
	destRangeset->color_set   = srcRangeset->color_set;
	destRangeset->color       = srcRangeset->color;

	if (srcRangeset->color_name) {
		destRangeset->color_name = XtStringDup(srcRangeset->color_name);
	}

	if (srcRangeset->name) {
		destRangeset->name = XtStringDup(srcRangeset->name);
	}

	if (srcRangeset->ranges) {
		destRangeset->ranges = RangesetTable::RangesNew(srcRangeset->n_ranges);
		memcpy(destRangeset->ranges, srcRangeset->ranges, srcRangeset->n_ranges * sizeof(Range));
	}
}

/*
** Assign the range set table list.
*/

static void RangesetTableListSet(RangesetTable *table) {

	for (int i = 0; i < table->n_set; i++) {
		table->list[i] = rangeset_labels[(int)table->order[i]];
	}
	
	table->list[table->n_set] = '\0';
}


/*
** Helper routines for managing the order and depth tables.
*/

static int activateRangeset(RangesetTable *table, int active) {

	if (table->active[active]) {
		return 0; // already active 
	}

	int depth = table->depth[active];

	/* we want to make the "active" set the most recent (lowest depth value):
	   shuffle table->order[0..depth-1] to table->order[1..depth]
	   readjust the entries in table->depth[] accordingly */
	for (int i = depth; i > 0; i--) {
		int j = table->order[i] = table->order[i - 1];
		table->depth[j] = i;
	}
	
	// insert the new one: first in order, of depth 0 
	table->order[0] = active;
	table->depth[active] = 0;

	// and finally... 
	table->active[active] = 1;
	table->n_set++;

	RangesetTableListSet(table);

	return 1;
}

static int deactivateRangeset(RangesetTable *table, int active) {
	int depth;

	if (!table->active[active])
		return 0; // already inactive 

	/* we want to start by swapping the deepest entry in order with active
	   shuffle table->order[depth+1..n_set-1] to table->order[depth..n_set-2]
	   readjust the entries in table->depth[] accordingly */
	depth = table->depth[active];
	int n = table->n_set - 1;

	for (int i = depth; i < n; i++) {
		int j = table->order[i] = table->order[i + 1];
		table->depth[j] = i;
	}
	
	// reinsert the old one: at max (active) depth 
	table->order[n] = active;
	table->depth[active] = n;

	// and finally... 
	table->active[active] = 0;
	table->n_set--;

	RangesetTableListSet(table);

	return 1;
}




}

RangesetTable::RangesetTable(TextBuffer *buffer) {
	int i;

	this->buf = buffer;

	for (i = 0; i < N_RANGESETS; i++) {
		this->set[i].RangesetInit(rangeset_labels[i], buffer);
		this->order[i] = (uint8_t)i;
		this->active[i] = 0;
		this->depth[i] = (uint8_t)i;
	}

	this->n_set = 0;
	this->list[0] = '\0';
	/* Range sets must be updated before the text display callbacks are
	   called to avoid highlighted ranges getting out of sync. */
	buffer->BufAddHighPriorityModifyCB(RangesetBufModifiedCB, this);
}

/*
** Create a new rangeset table in the destination buffer
** by cloning it from the source table.
*/
RangesetTable::RangesetTable(TextBuffer *buffer, const RangesetTable &other) : RangesetTable(buffer) {

	this->n_set = other.n_set;
	memcpy(this->order,  other.order,  sizeof(uint8_t) * N_RANGESETS);
	memcpy(this->active, other.active, sizeof(uint8_t) * N_RANGESETS);
	memcpy(this->depth,  other.depth,  sizeof(uint8_t) * N_RANGESETS);
	memcpy(this->list,   other.list,   sizeof(uint8_t) * (N_RANGESETS + 1));

	for (int i = 0; i < N_RANGESETS; i++) {
		rangesetClone(&this->set[i], &other.set[i]);
	}
}

RangesetTable::~RangesetTable() {

	this->buf->BufRemoveModifyCB(RangesetBufModifiedCB, this);
	for (int i = 0; i < N_RANGESETS; i++) {
		this->set[i].RangesetEmpty();
	}
}
	

/*
** Fetch the rangeset identified by label - initialise it if not active and
** make_active is true, and make it the most visible.
*/

Rangeset *RangesetTable::RangesetFetch(int label) {
	int rangesetIndex = RangesetFindIndex(label, 0);

	if (rangesetIndex < 0)
		return nullptr;

	if (this->active[rangesetIndex])
		return &this->set[rangesetIndex];
	else
		return nullptr;
}

/*
** Forget the rangeset identified by label - clears it, renders it inactive.
*/

Rangeset *RangesetTable::RangesetForget(int label) {
	int set_ind = RangesetFindIndex(label, 1);

	if (set_ind < 0)
		return nullptr;

	if (deactivateRangeset(this, set_ind))
		this->set[set_ind].RangesetEmpty();

	return &this->set[set_ind];
}

/*
** Return the color name, if any.
*/

char *RangesetTable::RangesetTableGetColorName(int index) {
	Rangeset *rangeset = &this->set[index];
	return rangeset->color_name;
}

/*
** Find a range set given its label in the table.
*/

int RangesetTable::RangesetFindIndex(int label, int must_be_active) const {
	uint8_t *p_label;

	if (label == 0) {
		return -1;
	}

	if (this) {
		p_label = (uint8_t *)strchr((char *)rangeset_labels, label);
		if (p_label) {
			int i = p_label - rangeset_labels;
			if (!must_be_active || this->active[i])
				return i;
		}
	}

	return -1;
}

/*
** Find the index of the first range in depth order which includes the position.
** Returns the index of the rangeset in the rangeset table + 1 if an including
** rangeset was found, 0 otherwise. If needs_color is true, "colorless" ranges
** will be skipped.
*/

int RangesetTable::RangesetIndex1ofPos(int pos, int needs_color) {
	int i;

	if (!this)
		return 0;

	for (i = 0; i < this->n_set; i++) {
		Rangeset *rangeset = &this->set[(int)this->order[i]];
		if (rangeset->RangesetCheckRangeOfPos(pos) >= 0) {
			if (needs_color && rangeset->color_set >= 0 && rangeset->color_name)
				return this->order[i] + 1;
		}
	}
	return 0;
}

/*
** Assign a color pixel value to a rangeset via the rangeset table. If ok is
** false, the color_set flag is set to an invalid (negative) value.
*/

int RangesetTable::RangesetTableAssignColorPixel(int index, Pixel color, int ok) {
	Rangeset *rangeset = &this->set[index];
	rangeset->color_set = ok ? 1 : -1;
	rangeset->color = color;
	return 1;
}

/*
** Return the color color validity, if any, and the value in *color.
*/

int RangesetTable::RangesetTableGetColorValid(int index, Pixel *color) {
	Rangeset *rangeset = &this->set[index];
	*color = rangeset->color;
	return rangeset->color_set;
}

/*
** Return the number of rangesets that are inactive
*/

int RangesetTable::nRangesetsAvailable() const {
	return (N_RANGESETS - this->n_set);
}


uint8_t *RangesetTable::RangesetGetList() {
	return this ? this->list : (uint8_t *)"";
}

void RangesetTable::RangesetTableUpdatePos(int pos, int ins, int del) {

	if (!this || (ins == 0 && del == 0))
		return;

	for (int i = 0; i < this->n_set; i++) {
		Rangeset *p = &this->set[(int)this->order[i]];
		p->update_fn(p, pos, ins, del);
	}
}

/*
** Create a new empty rangeset
*/

int RangesetTable::RangesetCreate() {

	size_t firstAvailableIndex = strspn((char *)rangeset_labels, (char *)this->list);

	if (firstAvailableIndex >= sizeof(rangeset_labels))
		return 0;

	int label = rangeset_labels[firstAvailableIndex];

	int setIndex = this->RangesetFindIndex(label, 0);

	if (setIndex < 0)
		return 0;

	if (this->active[setIndex])
		return label;

	if (activateRangeset(this, setIndex)) {
		this->set[setIndex].RangesetInit(rangeset_labels[setIndex], this->buf);
	}

	return label;
}


// TODO(eteran): does this really belong here?
Range *RangesetTable::RangesNew(int n) {

	if (n != 0) {
		/* We use a blocked allocation scheme here, with a block size of factor.
		   Only allocations of multiples of factor will be allowed.
		   Be sure to allocate at least one more than we really need, and
		   round up to next higher multiple of factor, ie
		    n = (((n + 1) + factor - 1) / factor) * factor
		   If we choose factor = (1 << factor_bits), we can use shifts
		   instead of multiply/divide, ie
		        n = ((n + (1 << factor_bits)) >> factor_bits) << factor_bits
		   or
		    n = (1 + (n >> factor_bits)) << factor_bits
		   Since the shifts just strip the end 1 bits, we can even get away
		   with
		    n = ((1 << factor_bits) + n) & (~0 << factor_bits);
		   Finally, we decide on factor_bits according to the size of n:
		   if n >= 256, we probably want less reallocation on growth than
		   otherwise; choose some arbitrary values thus:
		    factor_bits = (n >= 256) ? 6 : 4
		   so
		    n = (n >= 256) ? (n + (1<<6)) & (~0<<6) : (n + (1<<4)) & (~0<<4)
		   or
		    n = (n >= 256) ? ((n + 64) & ~63) : ((n + 16) & ~15)
		 */
		n = (n >= 256) ? ((n + 64) & ~63) : ((n + 16) & ~15);
		return static_cast<Range *>(malloc(n * sizeof(Range)));
	}

	return nullptr;
}

Range *RangesetTable::RangesFree(Range *ranges) {
	free(ranges);
	return nullptr;
}

Range *RangesetTable::RangesRealloc(Range *ranges, int n) {

	if (n > 0) {
		// see RangesNew() for comments 
		n = (n >= 256) ? ((n + 64) & ~63) : ((n + 16) & ~15);
		return static_cast<Range *>(realloc(ranges, n * sizeof(Range)));
	} else {
		return RangesFree(ranges);
	}

	return nullptr;
}

/*
** Return true if label is a valid identifier for a range set.
*/

int RangesetTable::RangesetLabelOK(int label) {
	return strchr((char *)rangeset_labels, label) != nullptr;
}
