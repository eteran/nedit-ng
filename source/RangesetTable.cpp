

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
    if ((nInserted != nDeleted) || table->buf_->BufCmpEx(pos, deletedText) != 0) {
        RangesetTable::RangesetTableUpdatePos(table, pos, nInserted, nDeleted);
	}
}

/*
** clone a ranges set.
*/
static void rangesetClone(Rangeset *destRangeset, const Rangeset *srcRangeset) {
	
	destRangeset->update_fn_   = srcRangeset->update_fn_;
	destRangeset->update_name_ = srcRangeset->update_name_;
	destRangeset->maxpos_      = srcRangeset->maxpos_;
	destRangeset->last_index_  = srcRangeset->last_index_;
	destRangeset->n_ranges_    = srcRangeset->n_ranges_;
	destRangeset->color_set_   = srcRangeset->color_set_;
    destRangeset->color_       = srcRangeset->color_;
    destRangeset->color_name_  = srcRangeset->color_name_;
    destRangeset->name_        = srcRangeset->name_;

	if (srcRangeset->ranges_) {
		destRangeset->ranges_ = RangesetTable::RangesNew(srcRangeset->n_ranges_);
		memcpy(destRangeset->ranges_, srcRangeset->ranges_, srcRangeset->n_ranges_ * sizeof(Range));
	}
}

/*
** Assign the range set table list.
*/

static void RangesetTableListSet(RangesetTable *table) {

    for (int i = 0; i < table->n_set_; i++) {
        table->list_[i] = rangeset_labels[static_cast<int>(table->order_[i])];
	}
	
    table->list_[table->n_set_] = '\0';
}


/*
** Helper routines for managing the order and depth tables.
*/

static int activateRangeset(RangesetTable *table, int active) {

    if (table->active_[active]) {
		return 0; // already active 
	}

    int depth = table->depth_[active];

	/* we want to make the "active" set the most recent (lowest depth value):
	   shuffle table->order[0..depth-1] to table->order[1..depth]
	   readjust the entries in table->depth[] accordingly */
	for (int i = depth; i > 0; i--) {
        int j = table->order_[i] = table->order_[i - 1];
        table->depth_[j] = i;
	}
	
	// insert the new one: first in order, of depth 0 
    table->order_[0] = active;
    table->depth_[active] = 0;

	// and finally... 
    table->active_[active] = 1;
    table->n_set_++;

	RangesetTableListSet(table);

	return 1;
}

static int deactivateRangeset(RangesetTable *table, int active) {
	int depth;

    if (!table->active_[active])
		return 0; // already inactive 

	/* we want to start by swapping the deepest entry in order with active
	   shuffle table->order[depth+1..n_set-1] to table->order[depth..n_set-2]
	   readjust the entries in table->depth[] accordingly */
    depth = table->depth_[active];
    int n = table->n_set_ - 1;

	for (int i = depth; i < n; i++) {
        int j = table->order_[i] = table->order_[i + 1];
        table->depth_[j] = i;
	}
	
	// reinsert the old one: at max (active) depth 
    table->order_[n] = active;
    table->depth_[active] = n;

	// and finally... 
    table->active_[active] = 0;
    table->n_set_--;

	RangesetTableListSet(table);

	return 1;
}




}

RangesetTable::RangesetTable(TextBuffer *buffer) {
	int i;

    buf_ = buffer;

	for (i = 0; i < N_RANGESETS; i++) {
        set_[i].RangesetInit(rangeset_labels[i], buffer);
        order_[i] = static_cast<uint8_t>(i);
        active_[i] = 0;
        depth_[i] = static_cast<uint8_t>(i);
	}

    n_set_ = 0;
    list_[0] = '\0';
	/* Range sets must be updated before the text display callbacks are
	   called to avoid highlighted ranges getting out of sync. */
	buffer->BufAddHighPriorityModifyCB(RangesetBufModifiedCB, this);
}

/*
** Create a new rangeset table in the destination buffer
** by cloning it from the source table.
*/
RangesetTable::RangesetTable(TextBuffer *buffer, const RangesetTable &other) : RangesetTable(buffer) {

    n_set_ = other.n_set_;
    memcpy(order_,  other.order_,  sizeof(uint8_t) * N_RANGESETS);
    memcpy(active_, other.active_, sizeof(uint8_t) * N_RANGESETS);
    memcpy(depth_,  other.depth_,  sizeof(uint8_t) * N_RANGESETS);
    memcpy(list_,   other.list_,   sizeof(uint8_t) * (N_RANGESETS + 1));

	for (int i = 0; i < N_RANGESETS; i++) {
        rangesetClone(&set_[i], &other.set_[i]);
	}
}

RangesetTable::~RangesetTable() {

    buf_->BufRemoveModifyCB(RangesetBufModifiedCB, this);
	for (int i = 0; i < N_RANGESETS; i++) {
        set_[i].RangesetEmpty();
	}
}
	

/*
** Fetch the rangeset identified by label - initialise it if not active and
** make_active is true, and make it the most visible.
*/

Rangeset *RangesetTable::RangesetFetch(int label) {
    int rangesetIndex = RangesetFindIndex(this, label, 0);

	if (rangesetIndex < 0)
		return nullptr;

    if (active_[rangesetIndex])
        return &set_[rangesetIndex];
	else
		return nullptr;
}

/*
** Forget the rangeset identified by label - clears it, renders it inactive.
*/

Rangeset *RangesetTable::RangesetForget(int label) {
    int set_ind = RangesetFindIndex(this, label, 1);

	if (set_ind < 0)
		return nullptr;

	if (deactivateRangeset(this, set_ind))
        set_[set_ind].RangesetEmpty();

    return &set_[set_ind];
}

/*
** Return the color name, if any.
*/

QString RangesetTable::RangesetTableGetColorName(int index) {
    Rangeset *rangeset = &set_[index];
	return rangeset->color_name_;
}

/*
** Find a range set given its label in the table.
*/
int RangesetTable::RangesetFindIndex(RangesetTable *table, int label, int must_be_active)  {

	if (label == 0) {
		return -1;
	}

    if (table) {
        auto p_label = reinterpret_cast<uint8_t *>(strchr(reinterpret_cast<char *>(rangeset_labels), label));
		if (p_label) {
			int i = p_label - rangeset_labels;
            if (!must_be_active || table->active_[i])
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

int RangesetTable::RangesetIndex1ofPos(RangesetTable *table, int pos, int needs_color) {
	int i;

    if (!table)
		return 0;

    for (i = 0; i < table->n_set_; i++) {
        Rangeset *rangeset = &table->set_[static_cast<int>(table->order_[i])];
		if (rangeset->RangesetCheckRangeOfPos(pos) >= 0) {
            if (needs_color && rangeset->color_set_ >= 0 && !rangeset->color_name_.isNull())
                return table->order_[i] + 1;
		}
	}
	return 0;
}

/*
** Assign a color pixel value to a rangeset via the rangeset table. If ok is
** false, the color_set flag is set to an invalid (negative) value.
*/

int RangesetTable::RangesetTableAssignColorPixel(int index, const QColor &color, bool ok) {
    Rangeset *rangeset = &set_[index];
	rangeset->color_set_ = ok ? 1 : -1;
	rangeset->color_ = color;
	return 1;
}

/*
** Return the color color validity, if any, and the value in *color.
*/

int RangesetTable::RangesetTableGetColorValid(int index, QColor *color) {
    Rangeset *rangeset = &set_[index];
	*color = rangeset->color_;
	return rangeset->color_set_;
}

/*
** Return the number of rangesets that are inactive
*/

int RangesetTable::nRangesetsAvailable() const {
    return (N_RANGESETS - n_set_);
}


uint8_t *RangesetTable::RangesetGetList(RangesetTable *table) {
    return table ? table->list_ : const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(""));
}

void RangesetTable::RangesetTableUpdatePos(RangesetTable *table, int pos, int ins, int del) {

	if (!table || (ins == 0 && del == 0))
		return;

    for (int i = 0; i < table->n_set_; i++) {
        Rangeset *p = &table->set_[static_cast<int>(table->order_[i])];
		p->update_fn_(p, pos, ins, del);
	}
}

/*
** Create a new empty rangeset
*/

int RangesetTable::RangesetCreate() {

    size_t firstAvailableIndex = strspn(reinterpret_cast<char *>(rangeset_labels), reinterpret_cast<char *>(list_));

	if (firstAvailableIndex >= sizeof(rangeset_labels))
		return 0;

	int label = rangeset_labels[firstAvailableIndex];

    int setIndex = RangesetFindIndex(this, label, 0);

	if (setIndex < 0)
		return 0;

    if (active_[setIndex])
		return label;

	if (activateRangeset(this, setIndex)) {
        set_[setIndex].RangesetInit(rangeset_labels[setIndex], buf_);
	}

	return label;
}

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
}

/*
** Return true if label is a valid identifier for a range set.
*/

int RangesetTable::RangesetLabelOK(int label) {
    return strchr(reinterpret_cast<char *>(rangeset_labels), label) != nullptr;
}
