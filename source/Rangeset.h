
#ifndef RANGESET_H_
#define RANGESET_H_

#include <QColor>

constexpr int N_RANGESETS = 63;

class Rangeset;

template <class Ch, class Tr>
class BasicTextBuffer;

using TextBuffer = BasicTextBuffer<char, std::char_traits<char>>;

struct Range {
	int start;
    int end; /* range from [start-]end */
};

struct RangesetInfo {
	bool        defined;
	int         label;
	int         count;
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
    QString RangesetGetName() const;
    int RangesetAdd(TextBuffer *buffer, Rangeset *plusSet);
    int RangesetAddBetween(TextBuffer *buffer, int start, int end);
    bool RangesetAssignColorName(TextBuffer *buffer, const QString &color_name);
    bool RangesetAssignColorPixel(const QColor &color, bool ok);
    bool RangesetAssignName(const QString &name);
    bool RangesetChangeModifyResponse(QString name);
	int RangesetCheckRangeOfPos(int pos);
	int RangesetFindRangeNo(int index, int *start, int *end) const;
	int RangesetFindRangeOfPos(int pos, int incl_end) const;
	int RangesetGetColorValid(QColor *color) const;
	int RangesetGetNRanges() const;
    int RangesetInverse(TextBuffer *buffer);
    int RangesetRemove(TextBuffer *buffer, Rangeset *minusSet);
    int RangesetRemoveBetween(TextBuffer *buffer, int start, int end);
    void RangesetEmpty(TextBuffer *buffer);
    void RangesetGetInfo(bool *defined, int *label, int *count, QString *color, QString *name, QString *mode) const;
	RangesetInfo RangesetGetInfo() const;

    static void RangesetRefreshRange(TextBuffer *buffer, int start, int end);
    void RangesetInit(int label, TextBuffer *buf);

public:
    RangesetUpdateFn *update_fn_; // modification update function
    QString update_name_;         // update function name
    int maxpos_;                  // text buffer maxpos
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
