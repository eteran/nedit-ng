/*******************************************************************************
*                                                                              *
* selection.h -- Nirvana Editor Selection Header File                          *
*                                                                              *
* Copyright 2002 The NEdit Developers                                          *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute versions of this program linked to  *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for    *
* more details.                                                                *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* July 31, 2001                                                                *
*                                                                              *
*******************************************************************************/

#ifndef SELECTION_H_
#define SELECTION_H_

class QString;
struct Document;

#include <X11/Intrinsic.h>
#include <X11/X.h>

int StringToLineAndCol(const char *text, int *lineNum, int *column);
QString GetAnySelectionEx(Document *window);
void AddMark(Document *window, Widget widget, char label);
void BeginGotoMarkCommand(Document *window, int extend);
void BeginMarkCommand(Document *window);
void GotoLineNumber(Document *window);
void GotoMarkDialog(Document *window, int extend);
void GotoMark(Document *window, Widget w, char label, int extendSel);
void GotoSelectedLineNumber(Document *window, Time time);
void MarkDialog(Document *window);
void OpenSelectedFile(Document *window, Time time);
void SelectNumberedLine(Document *window, int lineNum);
void UpdateMarkTable(Document *window, int pos, int nInserted, int nDeleted);

#endif
