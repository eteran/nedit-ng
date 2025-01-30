
#ifndef RANGESET_TABLE_H_
#define RANGESET_TABLE_H_

#include "Rangeset.h"
#include "TextCursor.h"

#include <cstddef>
#include <cstdint>
#include <vector>

class RangesetTable {
public:
	explicit RangesetTable(TextBuffer *buffer);
	RangesetTable(const RangesetTable &)            = delete;
	RangesetTable &operator=(const RangesetTable &) = delete;
	~RangesetTable();

public:
	QString getColorName(size_t index) const;
	Rangeset *RangesetFetch(int label);
	int RangesetCreate();
	int getColorValid(size_t index, QColor *color) const;
	int rangesetsAvailable() const;
	size_t index1ofPos(TextCursor pos, bool needs_color);
	std::vector<uint8_t> labels() const;
	void forgetLabel(int label);
	void assignColor(size_t index, const QColor &color);
	void updatePos(TextCursor pos, int64_t ins, int64_t del);

public:
	static bool LabelOK(int label);

public:
	TextBuffer *buffer_;
	std::vector<Rangeset> sets_;
};

#endif
