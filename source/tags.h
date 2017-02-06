/*******************************************************************************
*                                                                              *
* tags.h -- Nirvana Editor Tags Header File                                    *
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

#ifndef TAGS_H_
#define TAGS_H_

#include <ctime>
#include <string>

class DocumentWidget;
class TextArea;

struct tagFile {
	tagFile     *next;
	std::string filename;
	time_t      date;
	bool        loaded;
	short       index;
	short       refcount; // Only tips files are refcounted, not tags files
};

extern tagFile *TagsFileList; // list of loaded tags files
extern tagFile *TipsFileList; // list of loaded calltips tag files
extern int searchMode;
extern const char *tagName;
extern bool globAnchored;
extern int globPos;
extern int globHAlign;
extern int globVAlign;
extern int globAlignMode;

// file_type and search_type arguments are to select between tips and tags,
// and should be one of TAG or TIP.  TIP_FROM_TAG is for ShowTipString.
enum Mode {
	TAG, 
	TIP_FROM_TAG, 
	TIP
};

bool AddRelTagsFileEx(const QString &tagSpec, const QString &windowPath, int file_type);

// tagSpec is a colon-delimited list of filenames 
int AddTagsFile(const char *tagSpec, int file_type);
int DeleteTagsFile(const char *tagSpec, int file_type, bool force_unload);

int AddTagsFileEx(const QString &tagSpec, int file_type);
int DeleteTagsFileEx(const QString &tagSpec, int file_type, bool force_unload);
int tagsShowCalltipEx(TextArea *area, const char *text);

// Routines for handling tags or tips from the current selection

//  Display (possibly finding first) a calltip.  Search type can only be TIP or TIP_FROM_TAG here.
int ShowTipStringEx(DocumentWidget *window, char *text, bool anchored, int pos, bool lookup, int search_type, int hAlign, int vAlign, int alignMode);

void editTaggedLocationEx(TextArea *area, int i);
void showMatchingCalltipEx(TextArea *area, int i);

int findAllMatchesEx(DocumentWidget *document, TextArea *area, const char *string);

#endif
