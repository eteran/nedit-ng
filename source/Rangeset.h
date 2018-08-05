
#ifndef RANGESET_H_
#define RANGESET_H_

#include "TextBufferFwd.h"
#include "TextCursor.h"
#include <QColor>
#include <vector>
#include <boost/optional.hpp>

constexpr int N_RANGESETS = 63;

class Rangeset;

struct Range {
	TextCursor start;
	TextCursor end; /* range from [start-]end */
};

struct RangesetInfo {
	bool        defined = false;
	int         label   = 0;
	int64_t     count   = 0;
	QString     color;
	QString     name;
	QString     mode;
};

using RangesetUpdateFn = Rangeset *(Rangeset *rangeset, TextCursor pos, int64_t ins, int64_t del);

class Rangeset {
public:
	explicit Rangeset(TextBuffer *buffer, uint8_t label);
	Rangeset()                            = delete;
	Rangeset(const Rangeset &)            = default;
	Rangeset& operator=(const Rangeset &) = default;
	Rangeset(Rangeset &&)                 = default;
	Rangeset& operator=(Rangeset &&)      = default;
	~Rangeset() noexcept;

public:
	bool setColor(TextBuffer *buffer, const QString &color_name);
	bool setName(const QString &name);
	bool setMode(const QString &mode);

public:
	RangesetInfo RangesetGetInfo() const;

public:
	boost::optional<Range> RangesetFindRangeNo(int index) const;

public:
	int64_t RangesetCheckRangeOfPos(TextCursor pos);
	int64_t RangesetFindRangeOfPos(TextCursor pos, bool incl_end) const;

public:
	int64_t RangesetAddBetween(TextCursor start, TextCursor end);
	int64_t RangesetAdd(Rangeset *other);
	int64_t RangesetInverse();
	int64_t RangesetRemoveBetween(TextCursor start, TextCursor end);
	int64_t RangesetRemove(Rangeset *other);

public:
	QString name() const;
	int64_t size() const;

public:
	TextBuffer *buffer_;
	RangesetUpdateFn *update_;  // modification update function
	std::vector<Range> ranges_; // the ranges table (sorted array of ranges)

	QColor color_;              // the value of a particular color
	QString color_name_;        // the name of an assigned color
	QString name_;              // name of rangeset
	QString update_name_;       // update function name

	int64_t last_index_ = 0;    // a place to start looking
	int8_t color_set_   = 0;    // 0: unset; 1: set; -1: invalid
	uint8_t label_;             // a number 1-63
};

#endif
