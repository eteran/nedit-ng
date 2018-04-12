
#include "RangesetTable.h"
#include "TextBuffer.h"
#include <gsl/gsl_util>
#include <string>

namespace {

// -------------------------------------------------------------------------- 

const uint8_t rangeset_labels[N_RANGESETS + 1] = {
    58, 10, 15,  1, 27, 52, 14,  3, 61, 13, 31, 30, 45, 28, 41, 55,
    33, 20, 62, 34, 42, 18, 57, 47, 24, 49, 19, 50, 25, 38, 40,  2,
    21, 39, 59, 22, 60,  4,  6, 16, 29, 37, 48, 46, 54, 43, 32, 56,
    51,  7,  9, 63,  5,  8, 36, 44, 26, 11, 23, 17, 53, 35, 12, '\0'
};
														 
														 
// -------------------------------------------------------------------------- 

void RangesetBufModifiedCB(int64_t pos, int64_t nInserted, int64_t nDeleted, int64_t nRestyled, view::string_view deletedText, void *user) {
    Q_UNUSED(nRestyled);

    if(auto *table = static_cast<RangesetTable *>(user)) {
        if ((nInserted != nDeleted) || table->buf_->BufCmpEx(pos, deletedText) != 0) {
            table->RangesetTableUpdatePos(pos, nInserted, nDeleted);
        }
    }
}

/*
** Helper routines for managing the order and depth tables.
*/
bool activateRangeset(RangesetTable *table, int active) {

    if (table->active_[active]) {
        return false;			/* already active */
    }

    int depth = table->depth_[active];

    /* we want to make the "active" set the most recent (lowest depth value):
       shuffle table->order[0..depth-1] to table->order[1..depth]
       readjust the entries in table->depth[] accordingly */
    for (int i = depth; i > 0; i--) {
        int j = table->order_[i] = table->order_[i - 1];
        table->depth_[j] = i;
    }

    /* insert the new one: first in order, of depth 0 */
    table->order_[0] = active;
    table->depth_[active] = 0;

    /* and finally... */
    table->active_[active] = true;
    table->n_set_++;

    return true;
}

bool deactivateRangeset(RangesetTable *table, int active) {

    if (!table->active_[active]) {
        return false;			/* already inactive */
    }

    /* we want to start by swapping the deepest entry in order with active
       shuffle table->order[depth+1..n_set-1] to table->order[depth..n_set-2]
       readjust the entries in table->depth[] accordingly */
    int depth = table->depth_[active];
    int n = table->n_set_ - 1;

    for (int i = depth; i < n; i++) {
        int j = table->order_[i] = table->order_[i + 1];
        table->depth_[j] = i;
    }

    /* reinsert the old one: at max (active) depth */
    table->order_[n] = active;
    table->depth_[active] = n;

    /* and finally... */
    table->active_[active] = false;
    table->n_set_--;

    return true;
}

}

RangesetTable::RangesetTable(TextBuffer *buffer) : buf_(buffer) {

    for (int i = 0; i < N_RANGESETS; i++) {
        set_[i].RangesetInit(rangeset_labels[i]);

        order_[i]  = static_cast<uint8_t>(i);
        active_[i] = false;
        depth_[i]  = static_cast<uint8_t>(i);
    }

    n_set_   = 0;

    /* Range sets must be updated before the text display callbacks are
       called to avoid highlighted ranges getting out of sync. */
    buffer->BufAddHighPriorityModifyCB(RangesetBufModifiedCB, this);
}

RangesetTable::~RangesetTable() {

    buf_->BufRemoveModifyCB(RangesetBufModifiedCB, this);
    for (Rangeset &value : set_) {
        value.RangesetEmpty(buf_);
    }
}
	

/*
** Fetch the rangeset identified by label - initialise it if not active and
** make_active is true, and make it the most visible.
*/
Rangeset *RangesetTable::RangesetFetch(int label) {
    int rangesetIndex = RangesetFindIndex(label, false);

    if (rangesetIndex < 0) {
        return nullptr;
    }

    if (!active_[rangesetIndex]) {
        return nullptr;
    }

    return &set_[rangesetIndex];
}

/*
** Forget the rangeset identified by label - clears it, renders it inactive.
*/
Rangeset *RangesetTable::RangesetForget(int label) {
    int set_ind = RangesetFindIndex(label, true);

    if (set_ind < 0) {
        return nullptr;
    }

    if (deactivateRangeset(this, set_ind)) {
        set_[set_ind].RangesetEmpty(buf_);
    }

    return &set_[set_ind];
}

/*
** Return the color name, if any.
*/
QString RangesetTable::RangesetTableGetColorName(int index) const {
    const Rangeset *rangeset = &set_[index];
    return rangeset->color_name_;
}

/*
** Find a range set given its label in the table.
*/
int RangesetTable::RangesetFindIndex(int label, bool must_be_active) const {

    if(label == 0) {
       return -1;
    }

    auto p_label = reinterpret_cast<const uint8_t *>(strchr(reinterpret_cast<const char *>(rangeset_labels), label));
    if (p_label) {
        auto i = static_cast<int>(p_label - rangeset_labels);
        if (!must_be_active || active_[i])
        return i;
    }

    return -1;
}

/*
** Find the index of the first range in depth order which includes the position.
** Returns the index of the rangeset in the rangeset table + 1 if an including
** rangeset was found, 0 otherwise. If needs_color is true, "colorless" ranges
** will be skipped.
*/
int RangesetTable::RangesetIndex1ofPos(int64_t pos, bool needs_color) {

    for (int i = 0; i < n_set_; i++) {
        Rangeset *rangeset = &set_[static_cast<int>(order_[i])];
        if (rangeset->RangesetCheckRangeOfPos(pos) >= 0) {
            if (needs_color && rangeset->color_set_ >= 0 && !rangeset->color_name_.isNull()) {
                return order_[i] + 1;
            }
        }
    }

    return 0;
}

/*
** Assign a color pixel value to a rangeset via the rangeset table. If ok is
** false, the color_set flag is set to an invalid (negative) value.
*/
void RangesetTable::RangesetTableAssignColorPixel(int index, const QColor &color) {
    Rangeset *rangeset = &set_[index];
    rangeset->color_set_ = color.isValid() ? 1 : -1;
    rangeset->color_ = color;
}

/*
** Return the color color validity, if any, and the value in *color.
*/
int RangesetTable::RangesetTableGetColorValid(int index, QColor *color) const {
    const Rangeset *rangeset = &set_[index];
    *color = rangeset->color_;
    return rangeset->color_set_;
}

/*
** Return the number of rangesets that are inactive
*/
int RangesetTable::nRangesetsAvailable() const {
    return N_RANGESETS - n_set_;
}


std::vector<uint8_t> RangesetTable::RangesetGetList() const {

    std::vector<uint8_t> list;

    for (int i = 0; i < n_set_; i++) {
        list.push_back(rangeset_labels[static_cast<int>(order_[i])]);
    }

    return list;
}

void RangesetTable::RangesetTableUpdatePos(int64_t pos, int64_t ins, int64_t del) {

    if (ins == 0 && del == 0) {
        return;
    }

    for (int i = 0; i < n_set_; i++) {
        Rangeset *p = &set_[static_cast<int>(order_[i])];
        p->update_fn_(p, pos, ins, del);
    }
}

/*
** Create a new empty rangeset
*/
int RangesetTable::RangesetCreate() {

    std::vector<uint8_t> list = RangesetGetList();

    // find the first label not used
    auto it = std::find_if(std::begin(rangeset_labels), std::end(rangeset_labels), [&list](char ch) {
        return std::find(list.begin(), list.end(), ch) == list.end();
    });

    auto firstAvailableIndex = static_cast<size_t>(it - std::begin(rangeset_labels));

    if(firstAvailableIndex >= sizeof(rangeset_labels)) {
        return 0;
    }

    int label = rangeset_labels[firstAvailableIndex];

    int setIndex = RangesetFindIndex(label, false);

    if (setIndex < 0) {
        return 0;
    }

    if (active_[setIndex]) {
        return label;
    }

    if (activateRangeset(this, setIndex)) {
        set_[setIndex].RangesetInit(rangeset_labels[setIndex]);
    }

    return label;
}

/*
** Return true if label is a valid identifier for a range set.
*/
int RangesetTable::RangesetLabelOK(int label) {
    return strchr(reinterpret_cast<const char *>(rangeset_labels), label) != nullptr;
}
