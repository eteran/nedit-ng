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

typedef Rangeset *RangesetUpdateFn(Rangeset *p, int pos, int ins, int del);

class Rangeset {
public:
    QString RangesetGetName() const;
	int RangesetAdd(Rangeset *plusSet);
	int RangesetAddBetween(int start, int end);
	int RangesetAssignColorName(const char *color_name);
    int RangesetAssignColorPixel(const QColor &color, int ok);
	int RangesetAssignName(const char *name);
	int RangesetChangeModifyResponse(const char *name);
	int RangesetCheckRangeOfPos(int pos);
	int RangesetFindRangeNo(int index, int *start, int *end);
	int RangesetFindRangeOfPos(int pos, int incl_end);
    int RangesetGetColorValid(QColor *color);
	int RangesetGetNRanges() const;
	int RangesetInverse();
	int RangesetRemove(Rangeset *minusSet);
	int RangesetRemoveBetween(int start, int end);
	void RangesetEmpty();
    void RangesetGetInfo(bool *defined, int *label, int *count, const char **color, QString *name, const char **mode);
	void RangesetRefreshRange(int start, int end);
	void RangesetInit(int label, TextBuffer *buf);

public:
	RangesetUpdateFn *update_fn; // modification update function
    const char *update_name;     // update function name
	int maxpos;                  // text buffer maxpos
	int last_index;              // a place to start looking
	int n_ranges;                // how many ranges in ranges
	Range *ranges;               // the ranges table
    uint8_t label;               // a number 1-63

	int8_t color_set;            // 0: unset; 1: set; -1: invalid
    char *color_name;            // the name of an assigned color
    QColor color;                 // the value of a particular color
	TextBuffer *buf;             // the text buffer of the rangeset
    QString name;                  // name of rangeset
};

#endif
