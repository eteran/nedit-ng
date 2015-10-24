/*******************************************************************************
*                                                                              *
* TextBuffer.h -- Nirvana Editor Text Buffer Header File                          *
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

#ifndef NEDIT_TEXTBUF_H_INCLUDED
#define NEDIT_TEXTBUF_H_INCLUDED

#include <string>
#include <deque>

/* Maximum length in characters of a tab or control character expansion
   of a single buffer character */
#define MAX_EXP_CHAR_LEN 20

struct RangesetTable;

struct Selection {
	char selected;    /* True if the selection is active */
	char rectangular; /* True if the selection is rectangular */
	char zeroWidth;   /* Width 0 selections aren't "real" selections, but
	                      they can be useful when creating rectangular
	                      selections from the keyboard. */
	int start;        /* Pos. of start of selection, or if rectangular
	                       start of line containing it. */
	int end;          /* Pos. of end of selection, or if rectangular
	                       end of line containing it. */
	int rectStart;    /* Indent of left edge of rect. selection */
	int rectEnd;      /* Indent of right edge of rect. selection */
};

typedef void (*bufModifyCallbackProc)(int pos, int nInserted, int nDeleted, int nRestyled, const std::string &deletedText, void *cbArg);
typedef void (*bufPreDeleteCallbackProc)(int pos, int nDeleted, void *cbArg);

template <class T> struct CallbackPair {
	T callback;
	void *argument;
};

struct textBuffer {
public:
	textBuffer();
	explicit textBuffer(int requestedSize);
	~textBuffer();

public:
	int length;        /* length of the text in the buffer (the length of the buffer itself must be calculated: gapEnd -
	                      gapStart + length) */
	char *buf;         /* allocated memory where the text is stored */
	int gapStart;      /* points to the first character of the gap */
	int gapEnd;        /* points to the first char after the gap */
	Selection primary; /* highlighted areas */
	Selection secondary;
	Selection highlight;
	int tabDist; /* equiv. number of characters in a tab */
	int useTabs; /* True if buffer routines are allowed to use tabs for padding in rectangular operations */

	std::deque<CallbackPair<bufModifyCallbackProc>> modifyProcs;       /* procedures to call when buffer is modified to redisplay contents */
	std::deque<CallbackPair<bufPreDeleteCallbackProc>> preDeleteProcs; /* procedure to call before text is deleted from the buffer; at most one is supported. */

	int cursorPosHint;            /* hint for reasonable cursor position after a buffer modification operation */
	char nullSubsChar;            /* NEdit is based on C null-terminated strings, so ascii-nul characters must be substituted with
	                                 something else.  This is the else, but of course, things get quite messy when you use it */
	RangesetTable *rangesetTable; /* current range sets */
};

char *BufGetAll(textBuffer *buf);
char *BufGetRange(const textBuffer *buf, int start, int end);
char *BufGetSecSelectText(textBuffer *buf);
char *BufGetSelectionText(textBuffer *buf);
char *BufGetTextInRect(textBuffer *buf, int start, int end, int rectStart, int rectEnd);
char BufGetCharacter(const textBuffer *buf, const int pos);
const char *BufAsString(textBuffer *buf);
int BufCharWidth(char c, int indent, int tabDist, char nullSubsChar);
int BufCmp(textBuffer *buf, int pos, int len, const char *cmpText);
int BufCmpEx(textBuffer *buf, int pos, int len, const std::string &cmpText);
int BufCountBackwardNLines(textBuffer *buf, int startPos, int nLines);
int BufCountDispChars(const textBuffer *buf, const int lineStartPos, const int targetPos);
int BufCountForwardDispChars(textBuffer *buf, int lineStartPos, int nChars);
int BufCountForwardNLines(const textBuffer *buf, const int startPos, const unsigned nLines);
int BufCountLines(textBuffer *buf, int startPos, int endPos);
int BufEndOfLine(textBuffer *buf, int pos);
int BufExpandCharacter(char c, int indent, char *outStr, int tabDist, char nullSubsChar);
int BufGetEmptySelectionPos(textBuffer *buf, int *start, int *end, int *isRect, int *rectStart, int *rectEnd);
int BufGetExpandedChar(const textBuffer *buf, const int pos, const int indent, char *outStr);
int BufGetHighlightPos(textBuffer *buf, int *start, int *end, int *isRect, int *rectStart, int *rectEnd);
int BufGetSecSelectPos(textBuffer *buf, int *start, int *end, int *isRect, int *rectStart, int *rectEnd);
int BufGetSelectionPos(textBuffer *buf, int *start, int *end, int *isRect, int *rectStart, int *rectEnd);
int BufGetTabDistance(textBuffer *buf);
int BufSearchBackward(textBuffer *buf, int startPos, const char *searchChars, int *foundPos);
int BufSearchBackwardEx(textBuffer *buf, int startPos, const std::string &searchChars, int *foundPos);
int BufSearchForward(textBuffer *buf, int startPos, const char *searchChars, int *foundPos);
int BufSearchForwardEx(textBuffer *buf, int startPos, const std::string &searchChars, int *foundPos);
int BufStartOfLine(textBuffer *buf, int pos);
int BufSubstituteNullChars(char *string, int length, textBuffer *buf);
int BufSubstituteNullCharsEx(std::string &string, int length, textBuffer *buf);
std::string BufGetAllEx(textBuffer *buf);
std::string BufGetRangeEx(const textBuffer *buf, int start, int end);
std::string BufGetSecSelectTextEx(textBuffer *buf);
std::string BufGetSelectionTextEx(textBuffer *buf);
std::string BufGetTextInRectEx(textBuffer *buf, int start, int end, int rectStart, int rectEnd);
void BufAddHighPriorityModifyCB(textBuffer *buf, bufModifyCallbackProc bufModifiedCB, void *cbArg);
void BufAddModifyCB(textBuffer *buf, bufModifyCallbackProc bufModifiedCB, void *cbArg);
void BufAddPreDeleteCB(textBuffer *buf, bufPreDeleteCallbackProc bufPreDeleteCB, void *cbArg);
void BufCheckDisplay(textBuffer *buf, int start, int end);
void BufClearRect(textBuffer *buf, int start, int end, int rectStart, int rectEnd);
void BufCopyFromBuf(textBuffer *fromBuf, textBuffer *toBuf, int fromStart, int fromEnd, int toPos);
void BufHighlight(textBuffer *buf, int start, int end);
void BufInsert(textBuffer *buf, int pos, const char *text);
void BufInsertCol(textBuffer *buf, int column, int startPos, const char *text, int *charsInserted, int *charsDeleted);
void BufInsertColEx(textBuffer *buf, int column, int startPos, const std::string &text, int *charsInserted, int *charsDeleted);
void BufInsertEx(textBuffer *buf, int pos, const std::string &text);
void BufOverlayRect(textBuffer *buf, int startPos, int rectStart, int rectEnd, const char *text, int *charsInserted, int *charsDeleted);
void BufOverlayRectEx(textBuffer *buf, int startPos, int rectStart, int rectEnd, const std::string &text, int *charsInserted, int *charsDeleted);
void BufRectHighlight(textBuffer *buf, int start, int end, int rectStart, int rectEnd);
void BufRectSelect(textBuffer *buf, int start, int end, int rectStart, int rectEnd);
void BufRemove(textBuffer *buf, int start, int end);
void BufRemoveModifyCB(textBuffer *buf, bufModifyCallbackProc bufModifiedCB, void *cbArg);
void BufRemovePreDeleteCB(textBuffer *buf, bufPreDeleteCallbackProc bufPreDeleteCB, void *cbArg);
void BufRemoveRect(textBuffer *buf, int start, int end, int rectStart, int rectEnd);
void BufRemoveSecSelect(textBuffer *buf);
void BufRemoveSelected(textBuffer *buf);
void BufSecRectSelect(textBuffer *buf, int start, int end, int rectStart, int rectEnd);
void BufReplace(textBuffer *buf, int start, int end, const char *text);
void BufReplaceEx(textBuffer *buf, int start, int end, const std::string &text);
void BufReplaceRect(textBuffer *buf, int start, int end, int rectStart, int rectEnd, const char *text);
void BufReplaceRectEx(textBuffer *buf, int start, int end, int rectStart, int rectEnd, const std::string &text);
void BufReplaceSecSelect(textBuffer *buf, const char *text);
void BufReplaceSecSelectEx(textBuffer *buf, const std::string &text);
void BufReplaceSelected(textBuffer *buf, const char *text);
void BufReplaceSelectedEx(textBuffer *buf, const std::string &text);
void BufSecondarySelect(textBuffer *buf, int start, int end);
void BufSecondaryUnselect(textBuffer *buf);
void BufSelect(textBuffer *buf, int start, int end);
void BufSetAll(textBuffer *buf, const char *text);
void BufSetAllEx(textBuffer *buf, const std::string &text);
void BufSetTabDistance(textBuffer *buf, int tabDist);
void BufUnhighlight(textBuffer *buf);
void BufUnselect(textBuffer *buf);
void BufUnsubstituteNullChars(char *string, textBuffer *buf);
void BufUnsubstituteNullCharsEx(std::string &string, textBuffer *buf);

#endif /* NEDIT_TEXTBUF_H_INCLUDED */
