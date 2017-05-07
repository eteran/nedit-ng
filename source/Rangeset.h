/*******************************************************************************
*                                                                              *
* Rangeset.h         -- Nirvana Editor rangest header                          *
*                                                                              *
* Copyright (C) 1999 Mark Edel                                                 *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute version of this program linked to   *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License        *
* for more details.                                                            *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA        02111-1307 USA                           *
*                                                                              *
* Nirvana Text Editor                                                          *
* Sep 26, 2002                                                                 *
*                                                                              *
* Written by Tony Balinski with contributions from Andrew Hood                 *
*                                                                              *
* Modifications:                                                               *
*                                                                              *
*                                                                              *
*******************************************************************************/
#ifndef RANGESET_H_
#define RANGESET_H_

#include <QColor>

#define N_RANGESETS 63

class Rangeset;
class TextBuffer;

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
	const char *mode;
};



typedef Rangeset *RangesetUpdateFn(Rangeset *rangeset, int pos, int ins, int del);

class Rangeset {
public:
    Rangeset()                            = default;
    Rangeset(const Rangeset &)            = delete;
    Rangeset& operator=(const Rangeset &) = delete;
    Rangeset(Rangeset &&)                 = delete;
    Rangeset& operator=(Rangeset &&)      = delete;
	~Rangeset()                           = default;

public:
    QString RangesetGetName() const;
	int RangesetAdd(Rangeset *plusSet);
	int RangesetAddBetween(int start, int end);
	bool RangesetAssignColorName(const std::string &color_name);
	bool RangesetAssignColorPixel(const QColor &color, int ok);
	bool RangesetAssignName(const std::string &name);
	bool RangesetChangeModifyResponse(const char *name);
	int RangesetCheckRangeOfPos(int pos);
	int RangesetFindRangeNo(int index, int *start, int *end) const;
	int RangesetFindRangeOfPos(int pos, int incl_end) const;
	int RangesetGetColorValid(QColor *color) const;
	int RangesetGetNRanges() const;
	int RangesetInverse();
	int RangesetRemove(Rangeset *minusSet);
	int RangesetRemoveBetween(int start, int end);
	void RangesetEmpty();
	void RangesetGetInfo(bool *defined, int *label, int *count, QString *color, QString *name, const char **mode) const;
	RangesetInfo RangesetGetInfo() const;

	void RangesetRefreshRange(int start, int end) const;
	void RangesetInit(int label, TextBuffer *buf);

public:
    RangesetUpdateFn *update_fn_; // modification update function
    const char *update_name_;     // update function name
    int maxpos_;                  // text buffer maxpos
    int last_index_;              // a place to start looking
    int n_ranges_;                // how many ranges in ranges
    Range *ranges_;               // the ranges table
    uint8_t label_;               // a number 1-63

    int8_t color_set_;            // 0: unset; 1: set; -1: invalid
    QString color_name_;          // the name of an assigned color
    QColor color_;                // the value of a particular color
    TextBuffer *buf_;             // the text buffer of the rangeset
    QString name_;                // name of rangeset
};

#endif
