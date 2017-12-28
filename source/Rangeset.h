
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
    int         count   = 0;
	QString     color;
	QString     name;
    QString     mode;
};

using RangesetUpdateFn = Rangeset *(Rangeset *rangeset, int pos, int ins, int del);

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
    static Range *RangesNew(int n);
    static Range *RangesRealloc(Range *ranges, int n);

public:
    static void RangesetRefreshRange(TextBuffer *buffer, int start, int end);

public:
    bool RangesetAssignColorName(TextBuffer *buffer, const QString &color_name);
    bool RangesetAssignName(const QString &name);
    bool RangesetChangeModifyResponse(QString name);
    bool RangesetFindRangeNo(int index, int *start, int *end) const;
    int RangesetAddBetween(TextBuffer *buffer, int start, int end);
    int RangesetAdd(TextBuffer *buffer, Rangeset *plusSet);
    int RangesetCheckRangeOfPos(int pos);
    int RangesetFindRangeOfPos(int pos, int incl_end) const;
    int RangesetGetColorValid(QColor *color) const;
    int RangesetGetNRanges() const;
    int RangesetInverse(TextBuffer *buffer);
    int RangesetRemoveBetween(TextBuffer *buffer, int start, int end);
    int RangesetRemove(TextBuffer *buffer, Rangeset *minusSet);
    QString RangesetGetName() const;
    RangesetInfo RangesetGetInfo() const;
    void RangesetEmpty(TextBuffer *buffer);
    void RangesetInit(int label);

public:
    RangesetUpdateFn *update_fn_; // modification update function
    QString update_name_;         // update function name

    int last_index_;              // a place to start looking
    int n_ranges_;                // how many ranges in ranges
    Range *ranges_;               // the ranges table
    uint8_t label_;               // a number 1-63

    int8_t color_set_;            // 0: unset; 1: set; -1: invalid
    QString color_name_;          // the name of an assigned color
    QColor color_;                // the value of a particular color
    QString name_;                // name of rangeset
};

#endif
