/*******************************************************************************
*                                                                              *
* TextBuffer.h -- Nirvana Editor Text Buffer Header File                       *
*                                                                              *
* Copyright 2003 The NEdit Developers                                          *
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

#ifndef TEXT_BUFFER_H_
#define TEXT_BUFFER_H_

#include "TextSelection.h"
#include "string_view.h"
#include <string>
#include <deque>

/* Maximum length in characters of a tab or control character expansion
   of a single buffer character */
#define MAX_EXP_CHAR_LEN 20

//#define PURIFY

struct RangesetTable;

typedef void (*bufModifyCallbackProc)(int pos, int nInserted, int nDeleted, int nRestyled, view::string_view deletedText, void *cbArg);
typedef void (*bufPreDeleteCallbackProc)(int pos, int nDeleted, void *cbArg);

struct TextBuffer {
public:
	TextBuffer();
	explicit TextBuffer(int requestedSize);
	TextBuffer(const TextBuffer &) = delete;
	TextBuffer &operator=(const TextBuffer &) = delete;
	~TextBuffer();

public:
	static int BufCharWidth(char c, int indent, int tabDist, char nullSubsChar);
	static int BufExpandCharacter(char c, int indent, char *outStr, int tabDist, char nullSubsChar);

private:
	
public:
	char *BufGetAll();
	char *BufGetRange(int start, int end);
	char *BufGetSecSelectText();
	char *BufGetSelectionText();
	char *BufGetTextInRect(int start, int end, int rectStart, int rectEnd);
	char BufGetCharacter(int pos) const;
	const char *BufAsString();
	view::string_view BufAsStringEx();
	int BufCmp(int pos, int len, const char *cmpText);
	int BufCmpEx(int pos, int len, view::string_view cmpText);
	int BufCountBackwardNLines(int startPos, int nLines) const;
	int BufCountDispChars(int lineStartPos, int targetPos) const;
	int BufCountForwardDispChars(int lineStartPos, int nChars) const;
	int BufCountForwardNLines(int startPos, unsigned nLines) const;
	int BufCountLines(int startPos, int endPos) const;
	int BufEndOfLine(int pos) const;
	int BufGetEmptySelectionPos(int *start, int *end, int *isRect, int *rectStart, int *rectEnd);
	int BufGetExpandedChar(int pos, int indent, char *outStr) const;
	int BufGetHighlightPos(int *start, int *end, int *isRect, int *rectStart, int *rectEnd);
	int BufGetSecSelectPos(int *start, int *end, int *isRect, int *rectStart, int *rectEnd);
	int BufGetSelectionPos(int *start, int *end, int *isRect, int *rectStart, int *rectEnd);
	int BufGetTabDistance() const;
	int BufSearchBackward(int startPos, const char *searchChars, int *foundPos) const;
	int BufSearchBackwardEx(int startPos, view::string_view searchChars, int *foundPos) const;
	int BufSearchForward(int startPos, const char *searchChars, int *foundPos) const;
	int BufSearchForwardEx(int startPos, view::string_view searchChars, int *foundPos) const;
	int BufStartOfLine(int pos) const;
	int BufSubstituteNullChars(char *string, int length);
	int BufSubstituteNullCharsEx(std::string &string);
	std::string BufGetAllEx();
	std::string BufGetRangeEx(int start, int end);
	std::string BufGetSecSelectTextEx();
	std::string BufGetSelectionTextEx();
	std::string BufGetTextInRectEx(int start, int end, int rectStart, int rectEnd);
	void BufAddHighPriorityModifyCB(bufModifyCallbackProc bufModifiedCB, void *cbArg);
	void BufAddModifyCB(bufModifyCallbackProc bufModifiedCB, void *cbArg);
	void BufAddPreDeleteCB(bufPreDeleteCallbackProc bufPreDeleteCB, void *cbArg);
	void BufCheckDisplay(int start, int end);
	void BufClearRect(int start, int end, int rectStart, int rectEnd);
	void BufCopyFromBuf(TextBuffer *fromBuf, int fromStart, int fromEnd, int toPos);
	void BufHighlight(int start, int end);
	void BufInsert(int pos, const char *text);
	void BufInsertCol(int column, int startPos, const char *text, int *charsInserted, int *charsDeleted);
	void BufInsertColEx(int column, int startPos, view::string_view text, int *charsInserted, int *charsDeleted);
	void BufInsertEx(int pos, view::string_view text);
	void BufOverlayRect(int startPos, int rectStart, int rectEnd, const char *text, int *charsInserted, int *charsDeleted);
	void BufOverlayRectEx(int startPos, int rectStart, int rectEnd, view::string_view text, int *charsInserted, int *charsDeleted);
	void BufRectHighlight(int start, int end, int rectStart, int rectEnd);
	void BufRectSelect(int start, int end, int rectStart, int rectEnd);
	void BufRemove(int start, int end);
	void BufRemoveModifyCB(bufModifyCallbackProc bufModifiedCB, void *cbArg);
	void BufRemovePreDeleteCB(bufPreDeleteCallbackProc bufPreDeleteCB, void *cbArg);
	void BufRemoveRect(int start, int end, int rectStart, int rectEnd);
	void BufRemoveSecSelect();
	void BufRemoveSelected();
	void BufSecRectSelect(int start, int end, int rectStart, int rectEnd);
	void BufReplace(int start, int end, const char *text);
	void BufReplaceEx(int start, int end, view::string_view text);
	void BufReplaceRect(int start, int end, int rectStart, int rectEnd, const char *text);
	void BufReplaceRectEx(int start, int end, int rectStart, int rectEnd, view::string_view text);
	void BufReplaceSecSelect(const char *text);
	void BufReplaceSecSelectEx(view::string_view text);
	void BufReplaceSelected(const char *text);
	void BufReplaceSelectedEx(view::string_view text);
	void BufSecondarySelect(int start, int end);
	void BufSecondaryUnselect();
	void BufSelect(int start, int end);
	void BufSetAll(const char *text);
	void BufSetAllEx(view::string_view text);
	void BufSetTabDistance(int tabDist);
	void BufUnhighlight();
	void BufUnselect();
	void BufUnsubstituteNullChars(char *string) const;
	void BufUnsubstituteNullCharsEx(std::string &string) const;
	int BufGetLength() const;

private:
	char *getSelectionText(TextSelection *sel);  
	int insert(int pos, const char *text);
	int insertEx(int pos, view::string_view text);
	int searchBackward(int startPos, char searchChar, int *foundPos) const;
	int searchForward(int startPos, char searchChar, int *foundPos) const;
	std::string getSelectionTextEx(TextSelection *sel);
	void callModifyCBs(int pos, int nDeleted, int nInserted, int nRestyled, view::string_view deletedText);
	void callPreDeleteCBs(int pos, int nDeleted);
	void deleteRange(int start, int end);
	void deleteRect(int start, int end, int rectStart, int rectEnd, int *replaceLen, int *endPos);
	void findRectSelBoundariesForCopy(int lineStartPos, int rectStart, int rectEnd, int *selStart, int *selEnd);
	void insertCol(int column, int startPos, const char *insText, int *nDeleted, int *nInserted, int *endPos);
	void insertColEx(int column, int startPos, view::string_view insText, int *nDeleted, int *nInserted, int *endPos);
	void moveGap(int pos);
	void overlayRect(int startPos, int rectStart, int rectEnd, const char *insText, int *nDeleted, int *nInserted, int *endPos);
	void overlayRectEx(int startPos, int rectStart, int rectEnd, view::string_view insText, int *nDeleted, int *nInserted, int *endPos);
	void reallocateBuf(int newGapStart, int newGapLen);
	void redisplaySelection(TextSelection *oldSelection, TextSelection *newSelection);
	void removeSelected(TextSelection *sel);
	void replaceSelected(TextSelection *sel, const char *text);
	void replaceSelectedEx(TextSelection *sel, view::string_view text); 
	void updateSelections(int pos, int nDeleted, int nInserted);
		 	
private:
	char *buf_;         /* allocated memory where the text is stored */
	int gapStart_;      /* points to the first character of the gap */
	int gapEnd_;        /* points to the first char after the gap */
	int length_;        /* length of the text in the buffer (the length of the buffer itself must be calculated: gapEnd gapStart + length) */
	
public:
	// TODO(eteran): accessors
	TextSelection primary_; /* highlighted areas */
	TextSelection secondary_;
	TextSelection highlight_;	
	int tabDist_; /* equiv. number of characters in a tab */
	int useTabs_; /* True if buffer routines are allowed to use tabs for padding in rectangular operations */
	std::deque<std::pair<bufModifyCallbackProc, void *>> modifyProcs_;       /* procedures to call when buffer is modified to redisplay contents */
	std::deque<std::pair<bufPreDeleteCallbackProc, void *>> preDeleteProcs_; /* procedure to call before text is deleted from the buffer; at most one is supported. */
	int cursorPosHint_;            /* hint for reasonable cursor position after a buffer modification operation */
	char nullSubsChar_;            /* NEdit is based on C null-terminated strings, so ascii-nul characters must be substituted with something else.  This is the else, but of course, things get quite messy when you use it */
	RangesetTable *rangesetTable_; /* current range sets */
};

#endif
