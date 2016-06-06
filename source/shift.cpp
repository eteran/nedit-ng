/*******************************************************************************
*                                                                              *
* shift.c -- Nirvana Editor built-in filter commands                           *
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
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* June 18, 1991                                                                *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/

#include <QApplication>

#include "shift.h"
#include "TextHelper.h"
#include "TextBuffer.h"
#include "text.h"
#include "nedit.h"
#include "Document.h"
#include "window.h"
#include "memory.h"
#include "TextDisplay.h"
#include "textP.h"

#include <cstring>
#include <climits>
#include <cctype>
#include <algorithm>

static char *makeIndentString(int indent, int tabDist, int allowTabs, int *nChars);
static char *shiftLineLeft(const char *line, int lineLen, int tabDist, int nChars);
static char *shiftLineRight(const char *line, int lineLen, int tabsAllowed, int tabDist, int nChars);

static std::string shiftLineLeftEx(view::string_view line, int lineLen, int tabDist, int nChars);
static std::string shiftLineRightEx(view::string_view line, int lineLen, int tabsAllowed, int tabDist, int nChars);

static int atTabStop(int pos, int tabDist);
static int countLines(const char *text);
static int countLinesEx(view::string_view text);

template <class In>
static int findLeftMarginEx(In first, In last, int length, int tabDist);

static int findParagraphEnd(TextBuffer *buf, int startPos);
static int findParagraphStart(TextBuffer *buf, int startPos);
static int nextTab(int pos, int tabDist);
static std::string fillParagraphEx(view::string_view text, int leftMargin, int firstLineIndent, int rightMargin, int tabDist, int allowTabs, char nullSubsChar, int *filledLen);
static std::string fillParagraphsEx(view::string_view text, int rightMargin, int tabDist, int useTabs, char nullSubsChar, int *filledLen, int alignWithFirst);
static void changeCase(Document *window, int makeUpper);
static void shiftRect(Document *window, int direction, int byTab, int selStart, int selEnd, int rectStart, int rectEnd);

/*
** Shift the selection left or right by a single character, or by one tab stop
** if "byTab" is true.  (The length of a tab stop is the size of an emulated
** tab if emulated tabs are turned on, or a hardware tab if not).
*/
void ShiftSelection(Document *window, ShiftDirection direction, int byTab) {
	int selStart;
	int selEnd;
	bool isRect;
	int rectStart;
	int rectEnd;
	int newEndPos;
	int cursorPos;
	int origLength;
	int emTabDist;
	int shiftDist;
	TextBuffer *buf = window->buffer_;
	std::string text;

	auto textD = textD_of(window->lastFocus_);

	// get selection, if no text selected, use current insert position 
	if (!buf->BufGetSelectionPos(&selStart, &selEnd, &isRect, &rectStart, &rectEnd)) {
		cursorPos = textD->TextGetCursorPos();
		selStart = buf->BufStartOfLine(cursorPos);
		selEnd = buf->BufEndOfLine(cursorPos);
		if (selEnd < buf->BufGetLength())
			selEnd++;
		buf->BufSelect(selStart, selEnd);
		isRect = False;
		text = buf->BufGetRangeEx(selStart, selEnd);
	} else if (isRect) {
		cursorPos = textD->TextGetCursorPos();
		origLength = buf->BufGetLength();
		shiftRect(window, direction, byTab, selStart, selEnd, rectStart, rectEnd);
		
		auto textD = textD_of(window->lastFocus_);
		textD->TextSetCursorPos((cursorPos < (selEnd + selStart) / 2) ? selStart : cursorPos + (buf->BufGetLength() - origLength));
		return;
	} else {
		selStart = buf->BufStartOfLine(selStart);
		if (selEnd != 0 && buf->BufGetCharacter(selEnd - 1) != '\n') {
			selEnd = buf->BufEndOfLine(selEnd);
			if (selEnd < buf->BufGetLength())
				selEnd++;
		}
		buf->BufSelect(selStart, selEnd);
		text = buf->BufGetRangeEx(selStart, selEnd);
	}

	// shift the text by the appropriate distance 
	if (byTab) {
		XtVaGetValues(window->textArea_, textNemulateTabs, &emTabDist, nullptr);
		shiftDist = emTabDist == 0 ? buf->tabDist_ : emTabDist;
	} else
		shiftDist = 1;

	std::string shiftedText = ShiftTextEx(text, direction, buf->useTabs_, buf->tabDist_, shiftDist);

	buf->BufReplaceSelectedEx(shiftedText);

	newEndPos = selStart + shiftedText.size();
	buf->BufSelect(selStart, newEndPos);
}

static void shiftRect(Document *window, int direction, int byTab, int selStart, int selEnd, int rectStart, int rectEnd) {
	int offset, emTabDist;
	TextBuffer *buf = window->buffer_;

	// Make sure selStart and SelEnd refer to whole lines 
	selStart = buf->BufStartOfLine(selStart);
	selEnd   = buf->BufEndOfLine(selEnd);

	// Calculate the the left/right offset for the new rectangle 
	if (byTab) {
		XtVaGetValues(window->textArea_, textNemulateTabs, &emTabDist, nullptr);
		offset = emTabDist == 0 ? buf->tabDist_ : emTabDist;
	} else
		offset = 1;
	offset *= direction == SHIFT_LEFT ? -1 : 1;
	if (rectStart + offset < 0)
		offset = -rectStart;

	/* Create a temporary buffer for the lines containing the selection, to
	   hide the intermediate steps from the display update routines */
	auto tempBuf = new TextBuffer;
	tempBuf->tabDist_ = buf->tabDist_;
	tempBuf->useTabs_ = buf->useTabs_;
	std::string text = buf->BufGetRangeEx(selStart, selEnd);
	tempBuf->BufSetAllEx(text);

	// Do the shift in the temporary buffer 
	text = buf->BufGetTextInRectEx(selStart, selEnd, rectStart, rectEnd);
	tempBuf->BufRemoveRect(0, selEnd - selStart, rectStart, rectEnd);
	tempBuf->BufInsertColEx(rectStart + offset, 0, text, nullptr, nullptr);

	// Make the change in the real buffer 
	buf->BufReplaceEx(selStart, selEnd, tempBuf->BufAsStringEx());
	buf->BufRectSelect(selStart, selStart + tempBuf->BufGetLength(), rectStart + offset, rectEnd + offset);
	delete tempBuf;
}

void UpcaseSelection(Document *window) {
	changeCase(window, True);
}

void DowncaseSelection(Document *window) {
	changeCase(window, False);
}

/*
** Capitalize or lowercase the contents of the selection (or of the character
** before the cursor if there is no selection).  If "makeUpper" is true,
** change to upper case, otherwise, change to lower case.
*/
static void changeCase(Document *window, int makeUpper) {
	TextBuffer *buf = window->buffer_;
	int start;
	int end;
	int rectStart;
	int rectEnd;
	bool isRect;

	auto textD = textD_of(window->lastFocus_);

	// Get the selection.  Use character before cursor if no selection 
	if (!buf->BufGetSelectionPos(&start, &end, &isRect, &rectStart, &rectEnd)) {
		char bufChar[2] = " ";
		int cursorPos = textD->TextGetCursorPos();
		if (cursorPos == 0) {
			QApplication::beep();
			return;
		}
		*bufChar = buf->BufGetCharacter(cursorPos - 1);
		*bufChar = makeUpper ? toupper((uint8_t)*bufChar) : tolower((uint8_t)*bufChar);
		buf->BufReplaceEx(cursorPos - 1, cursorPos, bufChar);
	} else {
		bool modified = false;

		std::string text = buf->BufGetSelectionTextEx();
		
		for(char &ch: text) {
			char oldChar = ch;
			ch = makeUpper ? toupper((uint8_t)ch) : tolower((uint8_t)ch);
			if (ch != oldChar) {
				modified = true;
			}
		}

		if (modified) {
			buf->BufReplaceSelectedEx(text);
		}

		if (isRect)
			buf->BufRectSelect(start, end, rectStart, rectEnd);
		else
			buf->BufSelect(start, end);
	}
}

void FillSelection(Document *window) {
	TextBuffer *buf = window->buffer_;
	int left;
	int right;
	int nCols;
	int len;
	int rectStart;
	int rectEnd;
	bool isRect;
	int rightMargin;
	int wrapMargin;
	
	auto textD = textD_of(window->lastFocus_);
	
	int insertPos = textD->TextGetCursorPos();
	int hasSelection = window->buffer_->primary_.selected;
	std::string text;

	/* Find the range of characters and get the text to fill.  If there is a
	   selection, use it but extend non-rectangular selections to encompass
	   whole lines.  If there is no selection, find the paragraph containing
	   the insertion cursor */
	if (!buf->BufGetSelectionPos(&left, &right, &isRect, &rectStart, &rectEnd)) {
		left = findParagraphStart(buf, insertPos);
		right = findParagraphEnd(buf, insertPos);
		if (left == right) {
			QApplication::beep();
			return;
		}
		text = buf->BufGetRangeEx(left, right);
	} else if (isRect) {
		left  = buf->BufStartOfLine(left);
		right = buf->BufEndOfLine(right);
		text  = buf->BufGetTextInRectEx(left, right, rectStart, INT_MAX);
	} else {
		left = buf->BufStartOfLine(left);
		if (right != 0 && buf->BufGetCharacter(right - 1) != '\n') {
			right = buf->BufEndOfLine(right);
			if (right < buf->BufGetLength())
				right++;
		}
		buf->BufSelect(left, right);
		text = buf->BufGetRangeEx(left, right);
	}

	/* Find right margin either as specified in the rectangular selection, or
	   by measuring the text and querying the window's wrap margin (or width) */
	if (hasSelection && isRect) {
		rightMargin = rectEnd - rectStart;
	} else {
		XtVaGetValues(window->textArea_, textNcolumns, &nCols, textNwrapMargin, &wrapMargin, nullptr);
		rightMargin = (wrapMargin == 0 ? nCols : wrapMargin);
	}

	// Fill the text 
	std::string filledText = fillParagraphsEx(text, rightMargin, buf->tabDist_, buf->useTabs_, buf->nullSubsChar_, &len, False);

	// Replace the text in the window 
	if (hasSelection && isRect) {
		buf->BufReplaceRectEx(left, right, rectStart, INT_MAX, filledText);
		buf->BufRectSelect(left, buf->BufEndOfLine(buf->BufCountForwardNLines(left, countLinesEx(filledText) - 1)), rectStart, rectEnd);
	} else {
		buf->BufReplaceEx(left, right, filledText);
		if (hasSelection)
			buf->BufSelect(left, left + len);
	}

	/* Find a reasonable cursor position.  Usually insertPos is best, but
	   if the text was indented, positions can shift */
	if (hasSelection && isRect) {
		textD->TextSetCursorPos(buf->cursorPosHint_);
	} else {
		textD->TextSetCursorPos(insertPos < left ? left : (insertPos > left + len ? left + len : insertPos));
	}
}

/*
** shift lines left and right in a multi-line text string.  Returns the
** shifted text in memory that must be freed by the caller with XtFree.
*/
char *ShiftText(const char *text, ShiftDirection direction, int tabsAllowed, int tabDist, int nChars, int *newLen) {
	char *shiftedText, *shiftedLine;
	char *shiftedPtr;
	const char *textPtr;
	int bufLen;

	/*
	** Allocate memory for shifted string.  Shift left adds a maximum of
	** tabDist-2 characters per line (remove one tab, add tabDist-1 spaces).
	** Shift right adds a maximum of nChars character per line.
	*/
	if (direction == SHIFT_RIGHT) {
		bufLen = strlen(text) + countLines(text) * nChars;
	} else {
		bufLen = strlen(text) + countLines(text) * tabDist;
	}
	
	shiftedText = XtMalloc(bufLen + 1);

	/*
	** break into lines and call shiftLine(Left/Right) on each
	*/
	const char *lineStartPtr = text;
	textPtr = text;
	shiftedPtr = shiftedText;
	while (true) {
		if (*textPtr == '\n' || *textPtr == '\0') {
			shiftedLine = (direction == SHIFT_RIGHT) ? shiftLineRight(lineStartPtr, textPtr - lineStartPtr, tabsAllowed, tabDist, nChars) 
			                                         : shiftLineLeft (lineStartPtr, textPtr - lineStartPtr,              tabDist, nChars);
			strcpy(shiftedPtr, shiftedLine);
			shiftedPtr += strlen(shiftedLine);
			XtFree(shiftedLine);
			if (*textPtr == '\0') {
				// terminate string & exit loop at end of text 
				*shiftedPtr = '\0';
				break;
			} else {
				// move the newline from text to shifted text 
				*shiftedPtr++ = *textPtr++;
			}
			// start line over 
			lineStartPtr = textPtr;
		} else
			textPtr++;
	}
	*newLen = shiftedPtr - shiftedText;
	return shiftedText;
}

/*
** shift lines left and right in a multi-line text string.  Returns the
** shifted text in memory that must be freed by the caller with XtFree.
*/
std::string ShiftTextEx(view::string_view text, ShiftDirection direction, int tabsAllowed, int tabDist, int nChars) {
	int bufLen;

	/*
	** Allocate memory for shifted string.  Shift left adds a maximum of
	** tabDist-2 characters per line (remove one tab, add tabDist-1 spaces).
	** Shift right adds a maximum of nChars character per line.
	*/
	if (direction == SHIFT_RIGHT) {
		bufLen = text.size() + countLinesEx(text) * nChars;
	} else {
		bufLen = text.size() + countLinesEx(text) * tabDist;
	}
		

	std::string shiftedText;
	shiftedText.reserve(bufLen);
	
	auto shiftedPtr = std::back_inserter(shiftedText);

	/*
	** break into lines and call shiftLine(Left/Right) on each
	*/
	auto lineStartPtr = text.begin();
	auto textPtr      = text.begin();
	
	while (true) {
		if (textPtr == text.end() || *textPtr == '\n') {
		
			// TODO(eteran): avoid the string copy... wish we had string_view!
			auto segment = view::substr(lineStartPtr, text.end());
			
			std::string shiftedLineString = (direction == SHIFT_RIGHT) ? 
				shiftLineRightEx(segment, textPtr - lineStartPtr, tabsAllowed, tabDist, nChars): 
				shiftLineLeftEx (segment, textPtr - lineStartPtr, 			   tabDist, nChars);
							
			std::copy(shiftedLineString.begin(), shiftedLineString.end(), shiftedPtr);

			if (textPtr == text.end()) {
				// terminate string & exit loop at end of text 
				break;
			} else {
				// move the newline from text to shifted text 
				*shiftedPtr++ = *textPtr++;
			}
			// start line over 
			lineStartPtr = textPtr;
		} else
			textPtr++;
	}

	return shiftedText;
}

static char *shiftLineRight(const char *line, int lineLen, int tabsAllowed, int tabDist, int nChars) {
	char *lineOut;
	const char *lineInPtr;
	char *lineOutPtr;
	int whiteWidth, i;

	lineInPtr = line;
	lineOut = XtMalloc(lineLen + nChars + 1);
	lineOutPtr = lineOut;
	whiteWidth = 0;
	while (true) {
		if (*lineInPtr == '\0' || (lineInPtr - line) >= lineLen) {
			// nothing on line, wipe it out 
			*lineOut = '\0';
			return lineOut;
		} else if (*lineInPtr == ' ') {
			// white space continues with tab, advance to next tab stop 
			whiteWidth++;
			*lineOutPtr++ = *lineInPtr++;
		} else if (*lineInPtr == '\t') {
			// white space continues with tab, advance to next tab stop 
			whiteWidth = nextTab(whiteWidth, tabDist);
			*lineOutPtr++ = *lineInPtr++;
		} else {
			// end of white space, add nChars of space 
			for (i = 0; i < nChars; i++) {
				*lineOutPtr++ = ' ';
				whiteWidth++;
				// if we're now at a tab stop, change last 8 spaces to a tab 
				if (tabsAllowed && atTabStop(whiteWidth, tabDist)) {
					lineOutPtr -= tabDist;
					*lineOutPtr++ = '\t';
				}
			}
			// move remainder of line 
			while (*lineInPtr != '\0' && (lineInPtr - line) < lineLen)
				*lineOutPtr++ = *lineInPtr++;
			*lineOutPtr = '\0';
			return lineOut;
		}
	}
}


static std::string shiftLineRightEx(view::string_view line, int lineLen, int tabsAllowed, int tabDist, int nChars) {
	int whiteWidth, i;

	auto lineInPtr = line.begin();
	std::string lineOut;
	lineOut.reserve(lineLen + nChars);

	auto lineOutPtr = std::back_inserter(lineOut);
	whiteWidth = 0;
	while (true) {
		if (lineInPtr == line.end() || (lineInPtr - line.begin()) >= lineLen) {
			// nothing on line, wipe it out 
			return lineOut;
		} else if (*lineInPtr == ' ') {
			// white space continues with tab, advance to next tab stop 
			whiteWidth++;
			*lineOutPtr++ = *lineInPtr++;
		} else if (*lineInPtr == '\t') {
			// white space continues with tab, advance to next tab stop 
			whiteWidth = nextTab(whiteWidth, tabDist);
			*lineOutPtr++ = *lineInPtr++;
		} else {
			// end of white space, add nChars of space 
			for (i = 0; i < nChars; i++) {
				*lineOutPtr++ = ' ';
				whiteWidth++;
				// if we're now at a tab stop, change last 8 spaces to a tab 
				if (tabsAllowed && atTabStop(whiteWidth, tabDist)) {
				
					lineOut.resize(lineOut.size() - tabDist);
				
					//lineOutPtr -= tabDist;
					*lineOutPtr++ = '\t';
				}
			}
			
			// move remainder of line 
			while (lineInPtr != line.end() && (lineInPtr - line.begin()) < lineLen) {
				*lineOutPtr++ = *lineInPtr++;
			}

			return lineOut;
		}
	}
}

static char *shiftLineLeft(const char *line, int lineLen, int tabDist, int nChars) {
	char *lineOut;
	int i, whiteWidth, lastWhiteWidth, whiteGoal;
	const char *lineInPtr;
	char *lineOutPtr;

	lineInPtr = line;
	lineOut = XtMalloc(lineLen + tabDist + 1);
	lineOutPtr = lineOut;
	whiteWidth = 0;
	lastWhiteWidth = 0;
	while (true) {
		if (*lineInPtr == '\0' || (lineInPtr - line) >= lineLen) {
			// nothing on line, wipe it out 
			*lineOut = '\0';
			return lineOut;
		} else if (*lineInPtr == ' ') {
			// white space continues with space, advance one character 
			whiteWidth++;
			*lineOutPtr++ = *lineInPtr++;
		} else if (*lineInPtr == '\t') {
			// white space continues with tab, advance to next tab stop	    
			// save the position, though, in case we need to remove the tab 
			lastWhiteWidth = whiteWidth;
			whiteWidth = nextTab(whiteWidth, tabDist);
			*lineOutPtr++ = *lineInPtr++;
		} else {
			// end of white space, remove nChars characters 
			for (i = 1; i <= nChars; i++) {
				if (lineOutPtr > lineOut) {
					if (*(lineOutPtr - 1) == ' ') {
						// end of white space is a space, just remove it 
						lineOutPtr--;
					} else {
						/* end of white space is a tab, remove it and add
						   back spaces */
						lineOutPtr--;
						whiteGoal = whiteWidth - i;
						whiteWidth = lastWhiteWidth;
						while (whiteWidth < whiteGoal) {
							*lineOutPtr++ = ' ';
							whiteWidth++;
						}
					}
				}
			}
			// move remainder of line 
			while (*lineInPtr != '\0' && (lineInPtr - line) < lineLen)
				*lineOutPtr++ = *lineInPtr++;
			// add a null 
			*lineOutPtr = '\0';
			return lineOut;
		}
	}
}

static std::string shiftLineLeftEx(view::string_view line, int lineLen, int tabDist, int nChars) {


	auto buffer = new char[line.size() + 1];
	std::copy(line.begin(), line.end(), buffer);
	buffer[line.size()] = '\0';
	std::string r = shiftLineLeft(buffer, lineLen, tabDist, nChars);
	delete [] buffer;
	return r;
	
}

static int atTabStop(int pos, int tabDist) {
	return (pos % tabDist == 0);
}

static int nextTab(int pos, int tabDist) {
	return (pos / tabDist) * tabDist + tabDist;
}

static int countLines(const char *text) {
	int count = 1;

	while (*text != '\0') {
		if (*text++ == '\n') {
			count++;
		}
	}
	return count;
}

static int countLinesEx(view::string_view text) {
	int count = 1;

	for(char ch: text) {
		if (ch == '\n') {
			count++;
		}
	}
	return count;
}

/*
** Find the implied left margin of a text string (the number of columns to the
** first non-whitespace character on any line) up to either the terminating
** null character at the end of the string, or "length" characters, whever
** comes first.
*/
template <class In>
static int findLeftMarginEx(In first, In last, int length, int tabDist) {

	int col        = 0;
	int leftMargin = INT_MAX;
	bool inMargin  = true;

	for (auto it = first; it != last && length--; ++it) {
	
		switch(*it) {
		case '\t':
			col += TextBuffer::BufCharWidth('\t', col, tabDist, '\0');
			break;
		case ' ':
			col++;
			break;
		case '\n':
			col = 0;
			inMargin = true;
			break;
		default:
			// non-whitespace 
			if (col < leftMargin && inMargin) {
				leftMargin = col;
			}
			inMargin = false;
			break;						
		}
	}

	// if no non-white text is found, the leftMargin will never be set 
	if (leftMargin == INT_MAX) {
		return 0;
	}

	return leftMargin;
}

/*
** Fill multiple paragraphs between rightMargin and an implied left margin
** and first line indent determined by analyzing the text.  alignWithFirst
** aligns subsequent paragraphs with the margins of the first paragraph (a
** capability not currently used in NEdit, but carried over from code for
** previous versions which did all paragraphs together).
*/
static std::string fillParagraphsEx(view::string_view text, int rightMargin, int tabDist, int useTabs, char nullSubsChar, int *filledLen, int alignWithFirst) {
	char ch;
	int len;

	// Create a buffer to accumulate the filled paragraphs 
	auto buf = new TextBuffer;
	buf->BufSetAllEx(text);

	/*
	** Loop over paragraphs, filling each one, and accumulating the results
	** in buf
	*/
	int paraStart = 0;
	for (;;) {

		// Skip over white space 
		while (paraStart < buf->BufGetLength()) {
			ch = buf->BufGetCharacter(paraStart);
			if (ch != ' ' && ch != '\t' && ch != '\n')
				break;
			paraStart++;
		}
		if (paraStart >= buf->BufGetLength()) {
			break;
		}
		
		paraStart = buf->BufStartOfLine(paraStart);

		// Find the end of the paragraph 
		int paraEnd = findParagraphEnd(buf, paraStart);

		/* Operate on either the one paragraph, or to make them all identical,
		   do all of them together (fill paragraph can format all the paragraphs
		   it finds with identical specs if it gets passed more than one) */
		int fillEnd = alignWithFirst ? buf->BufGetLength() : paraEnd;

		/* Get the paragraph in a text string (or all of the paragraphs if
		   we're making them all the same) */
		std::string paraText = buf->BufGetRangeEx(paraStart, fillEnd);

		/* Find separate left margins for the first and for the first line of
		   the paragraph, and for rest of the remainder of the paragraph */
		auto it = std::find(paraText.begin(), paraText.end(), '\n');
		
		int firstLineLen     = std::distance(paraText.begin(), it);
		auto secondLineStart = (it == paraText.end()) ? paraText.begin() : it + 1;
		int firstLineIndent  = findLeftMarginEx(paraText.begin(), paraText.end(), firstLineLen, tabDist);
		int leftMargin       = findLeftMarginEx(secondLineStart,  paraText.end(), paraEnd - paraStart - (secondLineStart - paraText.begin()), tabDist);

		// Fill the paragraph 
		std::string filledText = fillParagraphEx(paraText, leftMargin, firstLineIndent, rightMargin, tabDist, useTabs, nullSubsChar, &len);

		// Replace it in the buffer 
		buf->BufReplaceEx(paraStart, fillEnd, filledText);

		// move on to the next paragraph 
		paraStart += len;
	}

	// Free the buffer and return its contents 
	std::string filledText = buf->BufGetAllEx();
	*filledLen = buf->BufGetLength();
	delete buf;
	return filledText;
}

/*
** Trim leading space, and arrange text to fill between leftMargin and
** rightMargin (except for the first line which fills from firstLineIndent),
** re-creating whitespace to the left of the text using tabs (if allowTabs is
** True) calculated using tabDist, and spaces.  Returns a newly allocated
** string as the function result, and the length of the new string in filledLen.
*/
static std::string fillParagraphEx(view::string_view text, int leftMargin, int firstLineIndent, int rightMargin, int tabDist, int allowTabs, char nullSubsChar, int *filledLen) {
	char *outText, *indentString, *leadIndentStr, *b;
	int col, cleanedLen, indentLen, leadIndentLen, nLines = 1;
	int inWhitespace;

	// remove leading spaces, convert newlines to spaces 
	char *cleanedText = XtMalloc(text.size() + 1);
	char *outPtr = cleanedText;
	int inMargin = True;
	
	for(char ch : text) {
		if (ch == '\t' || ch == ' ') {
			if (!inMargin)
				*outPtr++ = ch;
		} else if (ch == '\n') {
			if (inMargin) {
				/* a newline before any text separates paragraphs, so leave
				   it in, back up, and convert the previous space back to \n */
				if (outPtr > cleanedText && *(outPtr - 1) == ' ')
					*(outPtr - 1) = '\n';
				*outPtr++ = '\n';
				nLines += 2;
			} else
				*outPtr++ = ' ';
			inMargin = True;
		} else {
			*outPtr++ = ch;
			inMargin = False;
		}
	}
	
	cleanedLen = outPtr - cleanedText;
	*outPtr = '\0';

	/* Put back newlines breaking text at word boundaries within the margins.
	   Algorithm: scan through characters, counting columns, and when the
	   margin width is exceeded, search backward for beginning of the word
	   and convert the last whitespace character into a newline */
	col = firstLineIndent;
	for (char *c = cleanedText; *c != '\0'; c++) {
		if (*c == '\n')
			col = leftMargin;
		else
			col += TextBuffer::BufCharWidth(*c, col, tabDist, nullSubsChar);
		if (col - 1 > rightMargin) {
			inWhitespace = True;
			for (b = c; b >= cleanedText && *b != '\n'; b--) {
				if (*b == '\t' || *b == ' ') {
					if (!inWhitespace) {
						*b = '\n';
						c = b;
						col = leftMargin;
						nLines++;
						break;
					}
				} else
					inWhitespace = False;
			}
		}
	}
	nLines++;

	// produce a string to prepend to lines to indent them to the left margin 
	leadIndentStr = makeIndentString(firstLineIndent, tabDist, allowTabs, &leadIndentLen);
	indentString = makeIndentString(leftMargin, tabDist, allowTabs, &indentLen);

	// allocate memory for the finished string 
	outText = XtMalloc((cleanedLen + leadIndentLen + indentLen * (nLines - 1) + 1));
	outPtr = outText;

	// prepend the indent string to each line of the filled text 
	strncpy(outPtr, leadIndentStr, leadIndentLen);
	outPtr += leadIndentLen;
	for (char *c = cleanedText; *c != '\0'; c++) {
		*outPtr++ = *c;
		if (*c == '\n') {
			strncpy(outPtr, indentString, indentLen);
			outPtr += indentLen;
		}
	}

	// convert any trailing space to newline.  Add terminating null 
	if (*(outPtr - 1) == ' ')
		*(outPtr - 1) = '\n';
	*outPtr = '\0';

	// clean up, return result 
	XtFree(cleanedText);
	XtFree(leadIndentStr);
	XtFree(indentString);
	*filledLen = outPtr - outText;
	
	std::string result = outText;
	XtFree(outText);
	
	return result;
}

static char *makeIndentString(int indent, int tabDist, int allowTabs, int *nChars) {
	char *indentString, *outPtr;
	int i;

	outPtr = indentString = XtMalloc(indent + 1);
	if (allowTabs) {
		for (i = 0; i < indent / tabDist; i++)
			*outPtr++ = '\t';
		for (i = 0; i < indent % tabDist; i++)
			*outPtr++ = ' ';
	} else {
		for (i = 0; i < indent; i++)
			*outPtr++ = ' ';
	}
	*outPtr = '\0';
	*nChars = outPtr - indentString;
	return indentString;
}

/*
** Find the boundaries of the paragraph containing pos
*/
static int findParagraphEnd(TextBuffer *buf, int startPos) {
	char c;
	int pos;
	static char whiteChars[] = " \t";

	pos = buf->BufEndOfLine(startPos) + 1;
	while (pos < buf->BufGetLength()) {
		c = buf->BufGetCharacter(pos);
		if (c == '\n')
			break;
		if (strchr(whiteChars, c) != nullptr)
			pos++;
		else
			pos = buf->BufEndOfLine(pos) + 1;
	}
	return pos < buf->BufGetLength() ? pos : buf->BufGetLength();
}
static int findParagraphStart(TextBuffer *buf, int startPos) {
	char c;
	int pos, parStart;
	static char whiteChars[] = " \t";

	if (startPos == 0)
		return 0;
	parStart = buf->BufStartOfLine(startPos);
	pos = parStart - 2;
	while (pos > 0) {
		c = buf->BufGetCharacter(pos);
		if (c == '\n')
			break;
		if (strchr(whiteChars, c) != nullptr)
			pos--;
		else {
			parStart = buf->BufStartOfLine(pos);
			pos = parStart - 2;
		}
	}
	return parStart > 0 ? parStart : 0;
}
