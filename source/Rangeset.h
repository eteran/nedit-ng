
#ifndef RANGESET_H_
#define RANGESET_H_

#include "TextBufferFwd.h"
#include <QColor>

constexpr int N_RANGESETS = 63;

class Rangeset;
struct Range;

struct RangesetInfo {
    bool        defined = false;
    int         label   = 0;
    int64_t     count   = 0;
	QString     color;
	QString     name;
    QString     mode;
};

using RangesetUpdateFn = Rangeset *(Rangeset *rangeset, int64_t pos, int64_t ins, int64_t del);

class Rangeset {
public:
    Rangeset()                            = default;
    Rangeset(const Rangeset &)            = delete;
    Rangeset& operator=(const Rangeset &) = delete;
    Rangeset(Rangeset &&)                 = delete;
    Rangeset& operator=(Rangeset &&)      = delete;
    ~Rangeset()                           = default;

public:
    static void RangesFree(Range *ranges);
    static Range *RangesNew(int64_t n);
    static Range *RangesRealloc(Range *ranges, int64_t n);

public:
    static void RangesetRefreshRange(TextBuffer *buffer, int64_t start, int64_t end);

public:
    bool RangesetAssignColorName(TextBuffer *buffer, const QString &color_name);
    bool RangesetAssignName(const QString &name);
    bool RangesetChangeModifyResponse(QString name);
    bool RangesetFindRangeNo(int64_t index, int64_t *start, int64_t *end) const;
    int64_t RangesetAddBetween(TextBuffer *buffer, int64_t start, int64_t end);
    int64_t RangesetAdd(TextBuffer *buffer, Rangeset *plusSet);
    int64_t RangesetCheckRangeOfPos(int64_t pos);
    int64_t RangesetFindRangeOfPos(int64_t pos, int incl_end) const;
    int RangesetGetColorValid(QColor *color) const;
    int64_t RangesetGetNRanges() const;
    int64_t RangesetInverse(TextBuffer *buffer);
    int64_t RangesetRemoveBetween(TextBuffer *buffer, int64_t start, int64_t end);
    int64_t RangesetRemove(TextBuffer *buffer, Rangeset *minusSet);
    QString RangesetGetName() const;
    RangesetInfo RangesetGetInfo() const;
    void RangesetEmpty(TextBuffer *buffer);
    void RangesetInit(int label);

public:
    RangesetUpdateFn *update_fn_; // modification update function
    QString update_name_;         // update function name

    int64_t last_index_;          // a place to start looking
    int64_t n_ranges_;            // how many ranges in ranges
    Range *ranges_;               // the ranges table
    uint8_t label_;               // a number 1-63

    int8_t color_set_;            // 0: unset; 1: set; -1: invalid
    QString color_name_;          // the name of an assigned color
    QColor color_;                // the value of a particular color
    QString name_;                // name of rangeset
};

#endif
