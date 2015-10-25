/*******************************************************************************
*                                                                              *
* TextBuffer.c - Manage source text for one or more text areas                 *
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
* June 15, 1995                                                                *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/

#include "TextBuffer.h"
#include "rangeset.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

/* Initial size for the buffer gap (empty space in the buffer where text might
 * be inserted if the user is typing sequential chars)
 */
const int PreferredGapSize = 80;

const char *ControlCodeTable[32] = {"nul", "soh", "stx", "etx", "eot", "enq", "ack", "bel", "bs",  "ht", "nl",  "vt",  "np", "cr", "so", "si",
                                    "dle", "dc1", "dc2", "dc3", "dc4", "nak", "syn", "etb", "can", "em", "sub", "esc", "fs", "gs", "rs", "us"};

/*
** Convert sequences of spaces into tabs.  The threshold for conversion is
** when 3 or more spaces can be converted into a single tab, this avoids
** converting double spaces after a period withing a block of text.
*/
char *unexpandTabs(const char *text, int startIndent, int tabDist, char nullSubsChar, int *newLen) {
	char *outStr, *outPtr, expandedChar[MAX_EXP_CHAR_LEN];
	const char *c;
	int indent, len;

	outStr = XtMalloc(strlen(text) + 1);
	outPtr = outStr;
	indent = startIndent;
	for (c = text; *c != '\0';) {
		if (*c == ' ') {
			len = TextBuffer::BufExpandCharacter('\t', indent, expandedChar, tabDist, nullSubsChar);
			if (len >= 3 && !strncmp(c, expandedChar, len)) {
				c += len;
				*outPtr++ = '\t';
				indent += len;
			} else {
				*outPtr++ = *c++;
				indent++;
			}
		} else if (*c == '\n') {
			indent = startIndent;
			*outPtr++ = *c++;
		} else {
			*outPtr++ = *c++;
			indent++;
		}
	}
	*outPtr = '\0';
	*newLen = outPtr - outStr;
	return outStr;
}

/*
** Convert sequences of spaces into tabs.  The threshold for conversion is
** when 3 or more spaces can be converted into a single tab, this avoids
** converting double spaces after a period withing a block of text.
*/
std::string unexpandTabsEx(const std::string &text, int startIndent, int tabDist, char nullSubsChar, int *newLen) {
	std::string outStr;
	char expandedChar[MAX_EXP_CHAR_LEN];

	outStr.reserve(text.size());

	auto outPtr = std::back_inserter(outStr);
	int indent = startIndent;

	for (auto c = text.begin(); c != text.end();) {
		if (*c == ' ') {
			int len = TextBuffer::BufExpandCharacter('\t', indent, expandedChar, tabDist, nullSubsChar);
			if (len >= 3 && !strncmp(&*c, expandedChar, len)) {
				c += len;
				*outPtr++ = '\t';
				indent += len;
			} else {
				*outPtr++ = *c++;
				indent++;
			}
		} else if (*c == '\n') {
			indent = startIndent;
			*outPtr++ = *c++;
		} else {
			*outPtr++ = *c++;
			indent++;
		}
	}

	*newLen = outStr.size();
	return outStr;
}

/*
** Expand tabs to spaces for a block of text.  The additional parameter
** "startIndent" if nonzero, indicates that the text is a rectangular selection
** beginning at column "startIndent"
*/
char *expandTabs(const char *text, int startIndent, int tabDist, char nullSubsChar, int *newLen) {
	char *outStr, *outPtr;
	const char *c;
	int indent, len, outLen = 0;

	/* rehearse the expansion to figure out length for output string */
	indent = startIndent;
	for (c = text; *c != '\0'; c++) {
		if (*c == '\t') {
			len = TextBuffer::BufCharWidth(*c, indent, tabDist, nullSubsChar);
			outLen += len;
			indent += len;
		} else if (*c == '\n') {
			indent = startIndent;
			outLen++;
		} else {
			indent += TextBuffer::BufCharWidth(*c, indent, tabDist, nullSubsChar);
			outLen++;
		}
	}

	/* do the expansion */
	outStr = XtMalloc(outLen + 1);
	outPtr = outStr;
	indent = startIndent;
	for (c = text; *c != '\0'; c++) {
		if (*c == '\t') {
			len = TextBuffer::BufExpandCharacter(*c, indent, outPtr, tabDist, nullSubsChar);
			outPtr += len;
			indent += len;
		} else if (*c == '\n') {
			indent = startIndent;
			*outPtr++ = *c;
		} else {
			indent += TextBuffer::BufCharWidth(*c, indent, tabDist, nullSubsChar);
			*outPtr++ = *c;
		}
	}
	outStr[outLen] = '\0';
	*newLen = outLen;
	return outStr;
}

/*
** Expand tabs to spaces for a block of text.  The additional parameter
** "startIndent" if nonzero, indicates that the text is a rectangular selection
** beginning at column "startIndent"
*/
std::string expandTabsEx(const std::string &text, int startIndent, int tabDist, char nullSubsChar, int *newLen) {
	int indent;
	int len;
	int outLen = 0;

	/* rehearse the expansion to figure out length for output string */
	indent = startIndent;
	for (char ch : text) {
		if (ch == '\t') {
			len = TextBuffer::BufCharWidth(ch, indent, tabDist, nullSubsChar);
			outLen += len;
			indent += len;
		} else if (ch == '\n') {
			indent = startIndent;
			outLen++;
		} else {
			indent += TextBuffer::BufCharWidth(ch, indent, tabDist, nullSubsChar);
			outLen++;
		}
	}

	/* do the expansion */
	std::string outStr;
	outStr.reserve(outLen);

	auto outPtr = std::back_inserter(outStr);

	indent = startIndent;
	for (char ch : text) {
		if (ch == '\t') {

			char temp[MAX_EXP_CHAR_LEN];
			char *temp_ptr = temp;
			len = TextBuffer::BufExpandCharacter(ch, indent, temp, tabDist, nullSubsChar);

			for (int i = 0; i < len; ++i) {
				*outPtr++ = *temp_ptr++;
			}

			indent += len;
		} else if (ch == '\n') {
			indent = startIndent;
			*outPtr++ = ch;
		} else {
			indent += TextBuffer::BufCharWidth(ch, indent, tabDist, nullSubsChar);
			*outPtr++ = ch;
		}
	}

	*newLen = outLen;
	return outStr;
}

/*
** Adjust the space and tab characters from string "text" so that non-white
** characters remain stationary when the text is shifted from starting at
** "origIndent" to starting at "newIndent".  Returns an allocated string
** which must be freed by the caller with XtFree.
*/
char *realignTabs(const char *text, int origIndent, int newIndent, int tabDist, int useTabs, char nullSubsChar, int *newLength) {
	char *expStr, *outStr;
	int len;

	/* If the tabs settings are the same, retain original tabs */
	if (origIndent % tabDist == newIndent % tabDist) {
		len = strlen(text);
		outStr = XtMalloc(len + 1);
		strcpy(outStr, text);
		*newLength = len;
		return outStr;
	}

	/* If the tab settings are not the same, brutally convert tabs to
	   spaces, then back to tabs in the new position */
	expStr = expandTabs(text, origIndent, tabDist, nullSubsChar, &len);
	if (!useTabs) {
		*newLength = len;
		return expStr;
	}
	outStr = unexpandTabs(expStr, newIndent, tabDist, nullSubsChar, newLength);
	XtFree(expStr);
	return outStr;
}

/*
** Adjust the space and tab characters from string "text" so that non-white
** characters remain stationary when the text is shifted from starting at
** "origIndent" to starting at "newIndent".  Returns an allocated string
** which must be freed by the caller with XtFree.
*/
std::string realignTabsEx(const std::string &text, int origIndent, int newIndent, int tabDist, int useTabs, char nullSubsChar, int *newLength) {
	int len;

	/* If the tabs settings are the same, retain original tabs */
	if (origIndent % tabDist == newIndent % tabDist) {
		std::string outStr(text);
		*newLength = outStr.size();
		return outStr;
	}

	/* If the tab settings are not the same, brutally convert tabs to
	   spaces, then back to tabs in the new position */
	std::string expStr = expandTabsEx(text, origIndent, tabDist, nullSubsChar, &len);
	if (!useTabs) {
		*newLength = len;
		return expStr;
	}

	return unexpandTabsEx(expStr, newIndent, tabDist, nullSubsChar, newLength);
}

/*
** Create a pseudo-histogram of the characters in a string (don't actually
** count, because we don't want overflow, just mark the character's presence
** with a 1).  If init is true, initialize the histogram before acumulating.
** if not, add the new data to an existing histogram.
*/
void histogramCharacters(const char *string, int length, char hist[256], int init) {

	if (init)
		for (int i = 0; i < 256; i++)
			hist[i] = 0;
	for (const char *c = string; c < &string[length]; c++)
		hist[*((unsigned char *)c)] |= 1;
}

/*
** Create a pseudo-histogram of the characters in a string (don't actually
** count, because we don't want overflow, just mark the character's presence
** with a 1).  If init is true, initialize the histogram before acumulating.
** if not, add the new data to an existing histogram.
*/
void histogramCharactersEx(const std::string &string, char hist[256], int init) {

	if (init)
		for (int i = 0; i < 256; i++)
			hist[i] = 0;

	for (char ch : string) {
		hist[(unsigned char)ch] |= 1;
	}
}

/*
** Substitute fromChar with toChar in string.
*/
void subsChars(char *string, int length, char fromChar, char toChar) {
	char *c;

	for (c = string; c < &string[length]; c++)
		if (*c == fromChar)
			*c = toChar;
}

/*
** Substitute fromChar with toChar in string.
*/
void subsCharsEx(std::string &string, char fromChar, char toChar) {

	for (char &ch : string) {
		if (ch == fromChar)
			ch = toChar;
	}
}

/*
** Search through ascii control characters in histogram in order of least
** likelihood of use, find an unused character to use as a stand-in for a
** null.  If the character set is full (no available characters outside of
** the printable set, return the null character.
*/
char chooseNullSubsChar(char hist[256]) {

	static const int N_REPLACEMENTS = 25;
	static char replacements[N_REPLACEMENTS] = {1, 2, 3, 4, 5, 6, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 28, 29, 30, 31, 11, 7};

	for (int i = 0; i < N_REPLACEMENTS; i++) {
		if (hist[(unsigned char)replacements[i]] == 0) {
			return replacements[i];
		}
	}

	return '\0';
}

void addPadding(char *string, int startIndent, int toIndent, int tabDist, int useTabs, char nullSubsChar, int *charsAdded) {
	char *outPtr;
	int len, indent;

	indent = startIndent;
	outPtr = string;
	if (useTabs) {
		while (indent < toIndent) {
			len = TextBuffer::BufCharWidth('\t', indent, tabDist, nullSubsChar);
			if (len > 1 && indent + len <= toIndent) {
				*outPtr++ = '\t';
				indent += len;
			} else {
				*outPtr++ = ' ';
				indent++;
			}
		}
	} else {
		while (indent < toIndent) {
			*outPtr++ = ' ';
			indent++;
		}
	}
	*charsAdded = outPtr - string;
}

/*
** Insert characters from single-line string "insLine" in single-line string
** "line" at "column", leaving "insWidth" space before continuing line.
** "outLen" returns the number of characters written to "outStr", "endOffset"
** returns the number of characters from the beginning of the string to
** the right edge of the inserted text (as a hint for routines which need
** to position the cursor).
*/
void insertColInLine(const char *line, const char *insLine, int column, int insWidth, int tabDist, int useTabs, char nullSubsChar, char *outStr, int *outLen, int *endOffset) {
	char *c, *outPtr, *retabbedStr;
	const char *linePtr;
	int indent, toIndent, len, postColIndent;

	/* copy the line up to "column" */
	outPtr = outStr;
	indent = 0;
	for (linePtr = line; *linePtr != '\0'; linePtr++) {
		len = TextBuffer::BufCharWidth(*linePtr, indent, tabDist, nullSubsChar);
		if (indent + len > column)
			break;
		indent += len;
		*outPtr++ = *linePtr;
	}

	/* If "column" falls in the middle of a character, and the character is a
	   tab, leave it off and leave the indent short and it will get padded
	   later.  If it's a control character, insert it and adjust indent
	   accordingly. */
	if (indent < column && *linePtr != '\0') {
		postColIndent = indent + len;
		if (*linePtr == '\t')
			linePtr++;
		else {
			*outPtr++ = *linePtr++;
			indent += len;
		}
	} else
		postColIndent = indent;

	/* If there's no text after the column and no text to insert, that's all */
	if (*insLine == '\0' && *linePtr == '\0') {
		*outLen = *endOffset = outPtr - outStr;
		return;
	}

	/* pad out to column if text is too short */
	if (indent < column) {
		addPadding(outPtr, indent, column, tabDist, useTabs, nullSubsChar, &len);
		outPtr += len;
		indent = column;
	}

	/* Copy the text from "insLine" (if any), recalculating the tabs as if
	   the inserted string began at column 0 to its new column destination */
	if (*insLine != '\0') {
		retabbedStr = realignTabs(insLine, 0, indent, tabDist, useTabs, nullSubsChar, &len);
		for (c = retabbedStr; *c != '\0'; c++) {
			*outPtr++ = *c;
			len = TextBuffer::BufCharWidth(*c, indent, tabDist, nullSubsChar);
			indent += len;
		}
		XtFree(retabbedStr);
	}

	/* If the original line did not extend past "column", that's all */
	if (*linePtr == '\0') {
		*outLen = *endOffset = outPtr - outStr;
		return;
	}

	/* Pad out to column + width of inserted text + (additional original
	   offset due to non-breaking character at column) */
	toIndent = column + insWidth + postColIndent - column;
	addPadding(outPtr, indent, toIndent, tabDist, useTabs, nullSubsChar, &len);
	outPtr += len;
	indent = toIndent;

	/* realign tabs for text beyond "column" and write it out */
	retabbedStr = realignTabs(linePtr, postColIndent, indent, tabDist, useTabs, nullSubsChar, &len);
	strcpy(outPtr, retabbedStr);
	XtFree(retabbedStr);
	*endOffset = outPtr - outStr;
	*outLen = (outPtr - outStr) + len;
}

/*
** Insert characters from single-line string "insLine" in single-line string
** "line" at "column", leaving "insWidth" space before continuing line.
** "outLen" returns the number of characters written to "outStr", "endOffset"
** returns the number of characters from the beginning of the string to
** the right edge of the inserted text (as a hint for routines which need
** to position the cursor).
*/
void insertColInLineEx(const std::string &line, const std::string &insLine, int column, int insWidth, int tabDist, int useTabs, char nullSubsChar, char *outStr, int *outLen, int *endOffset) {
	char *outPtr;
	int indent, toIndent, len, postColIndent;

	/* copy the line up to "column" */
	outPtr = outStr;
	indent = 0;

	auto linePtr = line.begin();
	for (; linePtr != line.end(); ++linePtr) {
		len = TextBuffer::BufCharWidth(*linePtr, indent, tabDist, nullSubsChar);
		if (indent + len > column)
			break;
		indent += len;
		*outPtr++ = *linePtr;
	}

	/* If "column" falls in the middle of a character, and the character is a
	   tab, leave it off and leave the indent short and it will get padded
	   later.  If it's a control character, insert it and adjust indent
	   accordingly. */
	if (indent < column && linePtr != line.end()) {
		postColIndent = indent + len;
		if (*linePtr == '\t')
			linePtr++;
		else {
			*outPtr++ = *linePtr++;
			indent += len;
		}
	} else
		postColIndent = indent;

	/* If there's no text after the column and no text to insert, that's all */
	if (insLine.empty() && linePtr == line.end()) {
		*outLen = *endOffset = outPtr - outStr;
		return;
	}

	/* pad out to column if text is too short */
	if (indent < column) {
		addPadding(outPtr, indent, column, tabDist, useTabs, nullSubsChar, &len);
		outPtr += len;
		indent = column;
	}

	/* Copy the text from "insLine" (if any), recalculating the tabs as if
	   the inserted string began at column 0 to its new column destination */
	if (!insLine.empty()) {
		std::string retabbedStr = realignTabsEx(insLine, 0, indent, tabDist, useTabs, nullSubsChar, &len);

		for (char ch : retabbedStr) {
			*outPtr++ = ch;
			len = TextBuffer::BufCharWidth(ch, indent, tabDist, nullSubsChar);
			indent += len;
		}
	}

	/* If the original line did not extend past "column", that's all */
	if (linePtr == line.end()) {
		*outLen = *endOffset = outPtr - outStr;
		return;
	}

	/* Pad out to column + width of inserted text + (additional original
	   offset due to non-breaking character at column) */
	toIndent = column + insWidth + postColIndent - column;
	addPadding(outPtr, indent, toIndent, tabDist, useTabs, nullSubsChar, &len);
	outPtr += len;
	indent = toIndent;

	/* realign tabs for text beyond "column" and write it out */
	// TODO(eteran): fix this std::string copy inefficiency!
	std::string retabbedStr = realignTabsEx(std::string(linePtr, line.end()), postColIndent, indent, tabDist, useTabs, nullSubsChar, &len);
	strcpy(outPtr, retabbedStr.c_str());

	*endOffset = outPtr - outStr;
	*outLen = (outPtr - outStr) + len;
}

/*
** Remove characters in single-line string "line" between displayed positions
** "rectStart" and "rectEnd", and write the result to "outStr", which is
** assumed to be large enough to hold the returned string.  Note that in
** certain cases, it is possible for the string to get longer due to
** expansion of tabs.  "endOffset" returns the number of characters from
** the beginning of the string to the point where the characters were
** deleted (as a hint for routines which need to position the cursor).
*/
void deleteRectFromLine(const char *line, int rectStart, int rectEnd, int tabDist, int useTabs, char nullSubsChar, char *outStr, int *outLen, int *endOffset) {
	int indent, preRectIndent, postRectIndent, len;
	const char *c;
	char *outPtr;
	char *retabbedStr;

	/* copy the line up to rectStart */
	outPtr = outStr;
	indent = 0;
	for (c = line; *c != '\0'; c++) {
		if (indent > rectStart)
			break;
		len = TextBuffer::BufCharWidth(*c, indent, tabDist, nullSubsChar);
		if (indent + len > rectStart && (indent == rectStart || *c == '\t'))
			break;
		indent += len;
		*outPtr++ = *c;
	}
	preRectIndent = indent;

	/* skip the characters between rectStart and rectEnd */
	for (; *c != '\0' && indent < rectEnd; c++)
		indent += TextBuffer::BufCharWidth(*c, indent, tabDist, nullSubsChar);
	postRectIndent = indent;

	/* If the line ended before rectEnd, there's nothing more to do */
	if (*c == '\0') {
		*outPtr = '\0';
		*outLen = *endOffset = outPtr - outStr;
		return;
	}

	/* fill in any space left by removed tabs or control characters
	   which straddled the boundaries */
	indent = std::max<int>(rectStart + postRectIndent - rectEnd, preRectIndent);
	addPadding(outPtr, preRectIndent, indent, tabDist, useTabs, nullSubsChar, &len);
	outPtr += len;

	/* Copy the rest of the line.  If the indentation has changed, preserve
	   the position of non-whitespace characters by converting tabs to
	   spaces, then back to tabs with the correct offset */
	retabbedStr = realignTabs(c, postRectIndent, indent, tabDist, useTabs, nullSubsChar, &len);
	strcpy(outPtr, retabbedStr);
	XtFree(retabbedStr);
	*endOffset = outPtr - outStr;
	*outLen = (outPtr - outStr) + len;
}

/*
** Overlay characters from single-line string "insLine" on single-line string
** "line" between displayed character offsets "rectStart" and "rectEnd".
** "outLen" returns the number of characters written to "outStr", "endOffset"
** returns the number of characters from the beginning of the string to
** the right edge of the inserted text (as a hint for routines which need
** to position the cursor).
**
** This code does not handle control characters very well, but oh well.
*/
void overlayRectInLine(const char *line, const char *insLine, int rectStart, int rectEnd, int tabDist, int useTabs, char nullSubsChar, char *outStr, int *outLen, int *endOffset) {
	char *c, *outPtr, *retabbedStr;
	const char *linePtr;
	int inIndent, outIndent, len, postRectIndent;

	/* copy the line up to "rectStart" or just before the char that
	    contains it*/
	outPtr = outStr;
	inIndent = outIndent = 0;
	for (linePtr = line; *linePtr != '\0'; linePtr++) {
		len = TextBuffer::BufCharWidth(*linePtr, inIndent, tabDist, nullSubsChar);
		if (inIndent + len > rectStart)
			break;
		inIndent += len;
		outIndent += len;
		*outPtr++ = *linePtr;
	}

	/* If "rectStart" falls in the middle of a character, and the character
	   is a tab, leave it off and leave the outIndent short and it will get
	   padded later.  If it's a control character, insert it and adjust
	   outIndent accordingly. */
	if (inIndent < rectStart && *linePtr != '\0') {
		if (*linePtr == '\t') {
			/* Skip past the tab */
			linePtr++;
			inIndent += len;
		} else {
			*outPtr++ = *linePtr++;
			outIndent += len;
			inIndent += len;
		}
	}

	/* skip the characters between rectStart and rectEnd */
	for (; *linePtr != '\0' && inIndent < rectEnd; linePtr++)
		inIndent += TextBuffer::BufCharWidth(*linePtr, inIndent, tabDist, nullSubsChar);
	postRectIndent = inIndent;

	/* After this inIndent is dead and linePtr is supposed to point at the
	    character just past the last character that will be altered by
	    the overlay, whether that's a \t or otherwise.  postRectIndent is
	    the position at which that character is supposed to appear */

	/* If there's no text after rectStart and no text to insert, that's all */
	if (*insLine == '\0' && *linePtr == '\0') {
		*outLen = *endOffset = outPtr - outStr;
		return;
	}

	/* pad out to rectStart if text is too short */
	if (outIndent < rectStart) {
		addPadding(outPtr, outIndent, rectStart, tabDist, useTabs, nullSubsChar, &len);
		outPtr += len;
	}
	outIndent = rectStart;

	/* Copy the text from "insLine" (if any), recalculating the tabs as if
	   the inserted string began at column 0 to its new column destination */
	if (*insLine != '\0') {
		retabbedStr = realignTabs(insLine, 0, rectStart, tabDist, useTabs, nullSubsChar, &len);
		for (c = retabbedStr; *c != '\0'; c++) {
			*outPtr++ = *c;
			len = TextBuffer::BufCharWidth(*c, outIndent, tabDist, nullSubsChar);
			outIndent += len;
		}
		XtFree(retabbedStr);
	}

	/* If the original line did not extend past "rectStart", that's all */
	if (*linePtr == '\0') {
		*outLen = *endOffset = outPtr - outStr;
		return;
	}

	/* Pad out to rectEnd + (additional original offset
	   due to non-breaking character at right boundary) */
	addPadding(outPtr, outIndent, postRectIndent, tabDist, useTabs, nullSubsChar, &len);
	outPtr += len;
	outIndent = postRectIndent;

	/* copy the text beyond "rectEnd" */
	strcpy(outPtr, linePtr);
	*endOffset = outPtr - outStr;
	*outLen = (outPtr - outStr) + strlen(linePtr);
}

/*
** Overlay characters from single-line string "insLine" on single-line string
** "line" between displayed character offsets "rectStart" and "rectEnd".
** "outLen" returns the number of characters written to "outStr", "endOffset"
** returns the number of characters from the beginning of the string to
** the right edge of the inserted text (as a hint for routines which need
** to position the cursor).
**
** This code does not handle control characters very well, but oh well.
*/
void overlayRectInLineEx(const std::string &line, const std::string &insLine, int rectStart, int rectEnd, int tabDist, int useTabs, char nullSubsChar, char *outStr, int *outLen, int *endOffset) {
	char *outPtr;
	std::string retabbedStr;
	int inIndent, outIndent, len, postRectIndent;

	/* copy the line up to "rectStart" or just before the char that
	    contains it*/
	outPtr = outStr;
	inIndent = outIndent = 0;

	auto linePtr = line.begin();

	for (; linePtr != line.end(); ++linePtr) {
		len = TextBuffer::BufCharWidth(*linePtr, inIndent, tabDist, nullSubsChar);
		if (inIndent + len > rectStart)
			break;
		inIndent += len;
		outIndent += len;
		*outPtr++ = *linePtr;
	}

	/* If "rectStart" falls in the middle of a character, and the character
	   is a tab, leave it off and leave the outIndent short and it will get
	   padded later.  If it's a control character, insert it and adjust
	   outIndent accordingly. */
	if (inIndent < rectStart && linePtr != line.end()) {
		if (*linePtr == '\t') {
			/* Skip past the tab */
			linePtr++;
			inIndent += len;
		} else {
			*outPtr++ = *linePtr++;
			outIndent += len;
			inIndent += len;
		}
	}

	/* skip the characters between rectStart and rectEnd */
	for (; *linePtr != '\0' && inIndent < rectEnd; linePtr++)
		inIndent += TextBuffer::BufCharWidth(*linePtr, inIndent, tabDist, nullSubsChar);
	postRectIndent = inIndent;

	/* After this inIndent is dead and linePtr is supposed to point at the
	    character just past the last character that will be altered by
	    the overlay, whether that's a \t or otherwise.  postRectIndent is
	    the position at which that character is supposed to appear */

	/* If there's no text after rectStart and no text to insert, that's all */
	if (insLine.empty() && linePtr == line.end()) {
		*outLen = *endOffset = outPtr - outStr;
		return;
	}

	/* pad out to rectStart if text is too short */
	if (outIndent < rectStart) {
		addPadding(outPtr, outIndent, rectStart, tabDist, useTabs, nullSubsChar, &len);
		outPtr += len;
	}
	outIndent = rectStart;

	/* Copy the text from "insLine" (if any), recalculating the tabs as if
	   the inserted string began at column 0 to its new column destination */
	if (insLine.empty()) {
		retabbedStr = realignTabsEx(insLine, 0, rectStart, tabDist, useTabs, nullSubsChar, &len);
		for (char ch : retabbedStr) {
			*outPtr++ = ch;
			len = TextBuffer::BufCharWidth(ch, outIndent, tabDist, nullSubsChar);
			outIndent += len;
		}
	}

	/* If the original line did not extend past "rectStart", that's all */
	if (*linePtr == '\0') {
		*outLen = *endOffset = outPtr - outStr;
		return;
	}

	/* Pad out to rectEnd + (additional original offset
	   due to non-breaking character at right boundary) */
	addPadding(outPtr, outIndent, postRectIndent, tabDist, useTabs, nullSubsChar, &len);
	outPtr += len;
	outIndent = postRectIndent;

	// TODO(eteran): fix this std::string copy inefficiency!
	std::string temp(linePtr, line.end());

	/* copy the text beyond "rectEnd" */
	strcpy(outPtr, temp.c_str());
	*endOffset = outPtr - outStr;
	*outLen = (outPtr - outStr) + temp.size();
}

void setSelection(Selection *sel, int start, int end) {
	sel->selected = start != end;
	sel->zeroWidth = (start == end) ? 1 : 0;
	sel->rectangular = False;
	sel->start = std::min<int>(start, end);
	sel->end = std::max<int>(start, end);
}

void setRectSelect(Selection *sel, int start, int end, int rectStart, int rectEnd) {
	sel->selected = rectStart < rectEnd;
	sel->zeroWidth = (rectStart == rectEnd) ? 1 : 0;
	sel->rectangular = True;
	sel->start = start;
	sel->end = end;
	sel->rectStart = rectStart;
	sel->rectEnd = rectEnd;
}

int getSelectionPos(Selection *sel, int *start, int *end, int *isRect, int *rectStart, int *rectEnd) {
	/* Always fill in the parameters (zero-width can be requested too). */
	*isRect = sel->rectangular;
	*start = sel->start;
	*end = sel->end;
	if (sel->rectangular) {
		*rectStart = sel->rectStart;
		*rectEnd = sel->rectEnd;
	}
	return sel->selected;
}

/*
** Update an individual selection for changes in the corresponding text
*/
void updateSelection(Selection *sel, int pos, int nDeleted, int nInserted) {
	if ((!sel->selected && !sel->zeroWidth) || pos > sel->end)
		return;
	if (pos + nDeleted <= sel->start) {
		sel->start += nInserted - nDeleted;
		sel->end += nInserted - nDeleted;
	} else if (pos <= sel->start && pos + nDeleted >= sel->end) {
		sel->start = pos;
		sel->end = pos;
		sel->selected = False;
		sel->zeroWidth = False;
	} else if (pos <= sel->start && pos + nDeleted < sel->end) {
		sel->start = pos;
		sel->end = nInserted + sel->end - nDeleted;
	} else if (pos < sel->end) {
		sel->end += nInserted - nDeleted;
		if (sel->end <= sel->start)
			sel->selected = False;
	}
}

/*
** Copy from "text" to end up to but not including newline (or end of "text")
** and return the copy as the function value, and the length of the line in
** "lineLen"
*/
char *copyLine(const char *text, int *lineLen) {
	int len = 0;
	const char *c;
	char *outStr;

	for (c = text; *c != '\0' && *c != '\n'; c++)
		len++;
	outStr = XtMalloc(len + 1);
	strncpy(outStr, text, len);
	outStr[len] = '\0';
	*lineLen = len;
	return outStr;
}

/*
** Copy from "text" to end up to but not including newline (or end of "text")
** and return the copy as the function value, and the length of the line in
** "lineLen"
*/
std::string copyLineEx(std::string::const_iterator first, std::string::const_iterator last, int *lineLen) {
	int len = 0;

	for (auto it = first; it != last && *it != '\n'; ++it) {
		len++;
	}

	std::string outStr(first, first + len);

	*lineLen = outStr.size();
	return outStr;
}

/*
** Count the number of newlines in a null-terminated text string;
*/
int countLines(const char *string) {
	const char *c;
	int lineCount = 0;

	for (c = string; *c != '\0'; c++)
		if (*c == '\n')
			lineCount++;
	return lineCount;
}

/*
** Count the number of newlines in a null-terminated text string;
*/
int countLinesEx(const std::string &string) {
	return std::count(string.begin(), string.end(), '\n');
}

/*
** Measure the width in displayed characters of string "text"
*/
int textWidth(const char *text, int tabDist, char nullSubsChar) {
	int width = 0, maxWidth = 0;
	const char *c;

	for (c = text; *c != '\0'; c++) {
		if (*c == '\n') {
			if (width > maxWidth)
				maxWidth = width;
			width = 0;
		} else
			width += TextBuffer::BufCharWidth(*c, width, tabDist, nullSubsChar);
	}
	if (width > maxWidth)
		return width;
	return maxWidth;
}

/*
** Measure the width in displayed characters of string "text"
*/
int textWidthEx(const std::string &text, int tabDist, char nullSubsChar) {
	int width = 0, maxWidth = 0;

	for (char ch : text) {
		if (ch == '\n') {
			if (width > maxWidth)
				maxWidth = width;
			width = 0;
		} else
			width += TextBuffer::BufCharWidth(ch, width, tabDist, nullSubsChar);
	}
	if (width > maxWidth)
		return width;
	return maxWidth;
}
}

/*
** Create an empty text buffer
*/
TextBuffer::TextBuffer() : TextBuffer(0) {
}

/*
** Create an empty text buffer of a pre-determined size (use this to
** avoid unnecessary re-allocation if you know exactly how much the buffer
** will need to hold
*/
TextBuffer::TextBuffer(int requestedSize) : gapStart_(0), gapEnd_(PreferredGapSize), length_(0), tabDist_(8), useTabs_(True), nullSubsChar_('\0'), rangesetTable_(nullptr) {

	buf_ = XtMalloc(requestedSize + PreferredGapSize + 1);
	buf_[requestedSize + PreferredGapSize] = '\0';

	primary_.selected = False;
	primary_.zeroWidth = False;
	primary_.rectangular = False;
	primary_.start = 0;
	primary_.end = 0;
	secondary_.selected = False;
	secondary_.zeroWidth = False;
	secondary_.start = 0;
	secondary_.end = 0;
	secondary_.rectangular = False;
	highlight_.selected = False;
	highlight_.zeroWidth = False;
	highlight_.start = 0;
	highlight_.end = 0;
	highlight_.rectangular = False;

#ifdef PURIFY
	std::fill(&buf_[gapStart_], &buf_[gapEnd_], '.');
#endif
}

/*
** Free a text buffer
*/
TextBuffer::~TextBuffer() {
	XtFree(buf_);

	if (rangesetTable_)
		RangesetTableFree(rangesetTable_);
}

/*
** Get the entire contents of a text buffer.  Memory is allocated to contain
** the returned string, which the caller must free.
*/
char *TextBuffer::BufGetAll() {
	char *text;

	text = XtMalloc(length_ + 1);
	memcpy(text, buf_, gapStart_);
	memcpy(&text[gapStart_], &buf_[gapEnd_], length_ - gapStart_);
	text[length_] = '\0';
	return text;
}

/*
** Get the entire contents of a text buffer.  Memory is allocated to contain
** the returned string, which the caller must free.
*/
std::string TextBuffer::BufGetAllEx() {
	std::string text;
	text.reserve(length_);

	std::copy_n(buf_, gapStart_, std::back_inserter(text));
	std::copy_n(&text[gapStart_], length_ - gapStart_, std::back_inserter(text));

	return text;
}

/*
** Get the entire contents of a text buffer as a single string.  The gap is
** moved so that the buffer data can be accessed as a single contiguous
** character array.
** NB DO NOT ALTER THE TEXT THROUGH THE RETURNED POINTER!
** (we make an exception in TextBuffer::BufSubstituteNullChars() however)
** This function is intended ONLY to provide a searchable string without copying
** into a temporary buffer.
*/
const char *TextBuffer::BufAsString() {
	char *text;
	int bufLen = length_;
	int leftLen = gapStart_;
	int rightLen = bufLen - leftLen;

	/* find where best to put the gap to minimise memory movement */
	if (leftLen != 0 && rightLen != 0) {
		leftLen = (leftLen < rightLen) ? 0 : bufLen;
		moveGap(leftLen);
	}
	/* get the start position of the actual data */
	text = &buf_[(leftLen == 0) ? gapEnd_ : 0];
	/* make sure it's null-terminated */
	text[bufLen] = 0;

	return text;
}

/*
** Replace the entire contents of the text buffer
*/
void TextBuffer::BufSetAll(const char *text) {
	int length, deletedLength;
	length = strlen(text);

	callPreDeleteCBs(0, length_);

	/* Save information for redisplay, and get rid of the old buffer */
	std::string deletedText = BufGetAllEx();
	deletedLength = length_;
	XtFree(buf_);

	/* Start a new buffer with a gap of PreferredGapSize in the center */
	buf_ = XtMalloc(length + PreferredGapSize + 1);
	buf_[length + PreferredGapSize] = '\0';
	length_ = length;
	gapStart_ = length / 2;
	gapEnd_ = gapStart_ + PreferredGapSize;
	memcpy(buf_, text, gapStart_);
	memcpy(&buf_[gapEnd_], &text[gapStart_], length - gapStart_);
#ifdef PURIFY
	std::fill(&buf_[gapStart_], &buf_[gapEnd_], '.');
#endif

	/* Zero all of the existing selections */
	updateSelections(0, deletedLength, 0);

	/* Call the saved display routine(s) to update the screen */
	callModifyCBs(0, deletedLength, length, 0, deletedText);
}

/*
** Replace the entire contents of the text buffer
*/
void TextBuffer::BufSetAllEx(const std::string &text) {
	int length, deletedLength;
	length = text.size();

	callPreDeleteCBs(0, length_);

	/* Save information for redisplay, and get rid of the old buffer */
	std::string deletedText = BufGetAllEx();
	deletedLength = length_;
	XtFree(buf_);

	/* Start a new buffer with a gap of PreferredGapSize in the center */
	buf_ = XtMalloc(length + PreferredGapSize + 1);
	buf_[length + PreferredGapSize] = '\0';
	length_ = length;
	gapStart_ = length / 2;
	gapEnd_ = gapStart_ + PreferredGapSize;

	memcpy(buf_, &text[0], gapStart_);
	memcpy(&buf_[gapEnd_], &text[gapStart_], length - gapStart_);
#ifdef PURIFY
	std::fill(&buf_[gapStart_], &buf_[gapEnd_], '.');
#endif

	/* Zero all of the existing selections */
	updateSelections(0, deletedLength, 0);

	/* Call the saved display routine(s) to update the screen */
	callModifyCBs(0, deletedLength, length, 0, deletedText);
}

/*
** Return a copy of the text between "start" and "end" character positions
** from text buffer "buf".  Positions start at 0, and the range does not
** include the character pointed to by "end"
*/
char *TextBuffer::BufGetRange(int start, int end) {
	char *text;
	int length, part1Length;

	/* Make sure start and end are ok, and allocate memory for returned string.
	   If start is bad, return "", if end is bad, adjust it. */
	if (start < 0 || start > length_) {
		text = XtMalloc(1);
		text[0] = '\0';
		return text;
	}
	if (end < start) {
		int temp = start;
		start = end;
		end = temp;
	}
	if (end > length_)
		end = length_;
	length = end - start;
	text = XtMalloc(length + 1);

	/* Copy the text from the buffer to the returned string */
	if (end <= gapStart_) {
		memcpy(text, &buf_[start], length);
	} else if (start >= gapStart_) {
		memcpy(text, &buf_[start + (gapEnd_ - gapStart_)], length);
	} else {
		part1Length = gapStart_ - start;
		memcpy(text, &buf_[start], part1Length);
		memcpy(&text[part1Length], &buf_[gapEnd_], length - part1Length);
	}
	text[length] = '\0';
	return text;
}

/*
** Return a copy of the text between "start" and "end" character positions
** from text buffer "buf".  Positions start at 0, and the range does not
** include the character pointed to by "end"
*/
std::string TextBuffer::BufGetRangeEx(int start, int end) {
	std::string text;
	int length, part1Length;

	/* Make sure start and end are ok, and allocate memory for returned string.
	   If start is bad, return "", if end is bad, adjust it. */
	if (start < 0 || start > length_) {
		return text;
	}
	if (end < start) {
		int temp = start;
		start = end;
		end = temp;
	}

	if (end > length_)
		end = length_;

	length = end - start;
	text.reserve(length);

	/* Copy the text from the buffer to the returned string */
	if (end <= gapStart_) {
		std::copy_n(&buf_[start], length, std::back_inserter(text));
	} else if (start >= gapStart_) {
		std::copy_n(&buf_[start + (gapEnd_ - gapStart_)], length, std::back_inserter(text));
	} else {
		part1Length = gapStart_ - start;

		std::copy_n(&buf_[start], part1Length, std::back_inserter(text));
		std::copy_n(&buf_[gapEnd_], length - part1Length, std::back_inserter(text));
	}

	return text;
}

/*
** Return the character at buffer position "pos".  Positions start at 0.
*/
char TextBuffer::BufGetCharacter(int pos) const {
	if (pos < 0 || pos >= length_)
		return '\0';
	if (pos < gapStart_)
		return buf_[pos];
	else
		return buf_[pos + gapEnd_ - gapStart_];
}

/*
** Insert null-terminated string "text" at position "pos" in "buf"
*/
void TextBuffer::BufInsert(int pos, const char *text) {
	int nInserted;

	/* if pos is not contiguous to existing text, make it */
	if (pos > length_)
		pos = length_;
	if (pos < 0)
		pos = 0;

	/* Even if nothing is deleted, we must call these callbacks */
	callPreDeleteCBs(pos, 0);

	/* insert and redisplay */
	nInserted = insert(pos, text);
	cursorPosHint_ = pos + nInserted;
	callModifyCBs(pos, 0, nInserted, 0, std::string());
}

/*
** Insert null-terminated string "text" at position "pos" in "buf"
*/
void TextBuffer::BufInsertEx(int pos, const std::string &text) {
	int nInserted;

	/* if pos is not contiguous to existing text, make it */
	if (pos > length_)
		pos = length_;
	if (pos < 0)
		pos = 0;

	/* Even if nothing is deleted, we must call these callbacks */
	callPreDeleteCBs(pos, 0);

	/* insert and redisplay */
	nInserted = insertEx(pos, text);
	cursorPosHint_ = pos + nInserted;
	callModifyCBs(pos, 0, nInserted, 0, std::string());
}

/*
** Delete the characters between "start" and "end", and insert the
** null-terminated string "text" in their place in in "buf"
*/
void TextBuffer::BufReplace(int start, int end, const char *text) {
	int nInserted = strlen(text);

	callPreDeleteCBs(start, end - start);
	std::string deletedText = BufGetRangeEx(start, end);
	deleteRange(start, end);
	insert(start, text);
	cursorPosHint_ = start + nInserted;
	callModifyCBs(start, end - start, nInserted, 0, deletedText);
}

/*
** Delete the characters between "start" and "end", and insert the
** null-terminated string "text" in their place in in "buf"
*/
void TextBuffer::BufReplaceEx(int start, int end, const std::string &text) {
	int nInserted = text.size();

	callPreDeleteCBs(start, end - start);
	std::string deletedText = BufGetRangeEx(start, end);
	deleteRange(start, end);
	insertEx(start, text);
	cursorPosHint_ = start + nInserted;
	callModifyCBs(start, end - start, nInserted, 0, deletedText);
}

void TextBuffer::BufRemove(int start, int end) {
	/* Make sure the arguments make sense */
	if (start > end) {
		int temp = start;
		start = end;
		end = temp;
	}
	if (start > length_)
		start = length_;
	if (start < 0)
		start = 0;
	if (end > length_)
		end = length_;
	if (end < 0)
		end = 0;

	callPreDeleteCBs(start, end - start);
	/* Remove and redisplay */
	std::string deletedText = BufGetRangeEx(start, end);
	deleteRange(start, end);
	cursorPosHint_ = start;
	callModifyCBs(start, end - start, 0, 0, deletedText);
}

void TextBuffer::BufCopyFromBuf(TextBuffer *fromBuf, int fromStart, int fromEnd, int toPos) {
	int length = fromEnd - fromStart;
	int part1Length;

	/* Prepare the buffer to receive the new text.  If the new text fits in
	   the current buffer, just move the gap (if necessary) to where
	   the text should be inserted.  If the new text is too large, reallocate
	   the buffer with a gap large enough to accomodate the new text and a
	   gap of PreferredGapSize */
	if (length > gapEnd_ - gapStart_)
		reallocateBuf(toPos, length + PreferredGapSize);
	else if (toPos != gapStart_)
		moveGap(toPos);

	/* Insert the new text (toPos now corresponds to the start of the gap) */
	if (fromEnd <= fromBuf->gapStart_) {
		memcpy(&buf_[toPos], &fromBuf->buf_[fromStart], length);
	} else if (fromStart >= fromBuf->gapStart_) {
		memcpy(&buf_[toPos], &fromBuf->buf_[fromStart + (fromBuf->gapEnd_ - fromBuf->gapStart_)], length);
	} else {
		part1Length = fromBuf->gapStart_ - fromStart;
		memcpy(&buf_[toPos], &fromBuf->buf_[fromStart], part1Length);
		memcpy(&buf_[toPos + part1Length], &fromBuf->buf_[fromBuf->gapEnd_], length - part1Length);
	}
	gapStart_ += length;
	length_ += length;
	updateSelections(toPos, 0, length);
}

/*
** Insert "text" columnwise into buffer starting at displayed character
** position "column" on the line beginning at "startPos".  Opens a rectangular
** space the width and height of "text", by moving all text to the right of
** "column" right.  If charsInserted and charsDeleted are not nullptr, the
** number of characters inserted and deleted in the operation (beginning
** at startPos) are returned in these arguments
*/
void TextBuffer::BufInsertCol(int column, int startPos, const char *text, int *charsInserted, int *charsDeleted) {
	int nLines, lineStartPos, nDeleted, insertDeleted, nInserted;
	char *deletedText;

	nLines = countLines(text);
	lineStartPos = BufStartOfLine(startPos);
	nDeleted = BufEndOfLine(BufCountForwardNLines(startPos, nLines)) - lineStartPos;
	callPreDeleteCBs(lineStartPos, nDeleted);
	deletedText = BufGetRange(lineStartPos, lineStartPos + nDeleted);
	insertCol(column, lineStartPos, text, &insertDeleted, &nInserted, &cursorPosHint_);
	if (nDeleted != insertDeleted)
		fprintf(stderr, "NEdit internal consistency check ins1 failed");
	callModifyCBs(lineStartPos, nDeleted, nInserted, 0, deletedText);
	XtFree(deletedText);
	if (charsInserted != nullptr)
		*charsInserted = nInserted;
	if (charsDeleted != nullptr)
		*charsDeleted = nDeleted;
}

/*
** Insert "text" columnwise into buffer starting at displayed character
** position "column" on the line beginning at "startPos".  Opens a rectangular
** space the width and height of "text", by moving all text to the right of
** "column" right.  If charsInserted and charsDeleted are not nullptr, the
** number of characters inserted and deleted in the operation (beginning
** at startPos) are returned in these arguments
*/
void TextBuffer::BufInsertColEx(int column, int startPos, const std::string &text, int *charsInserted, int *charsDeleted) {
	int nLines, lineStartPos, nDeleted, insertDeleted, nInserted;

	nLines = countLinesEx(text);
	lineStartPos = BufStartOfLine(startPos);
	nDeleted = BufEndOfLine(BufCountForwardNLines(startPos, nLines)) - lineStartPos;
	callPreDeleteCBs(lineStartPos, nDeleted);
	std::string deletedText = BufGetRangeEx(lineStartPos, lineStartPos + nDeleted);
	insertColEx(column, lineStartPos, text, &insertDeleted, &nInserted, &cursorPosHint_);
	if (nDeleted != insertDeleted)
		fprintf(stderr, "NEdit internal consistency check ins1 failed");
	callModifyCBs(lineStartPos, nDeleted, nInserted, 0, deletedText);

	if (charsInserted != nullptr)
		*charsInserted = nInserted;
	if (charsDeleted != nullptr)
		*charsDeleted = nDeleted;
}

/*
** Overlay "text" between displayed character positions "rectStart" and
** "rectEnd" on the line beginning at "startPos".  If charsInserted and
** charsDeleted are not nullptr, the number of characters inserted and deleted
** in the operation (beginning at startPos) are returned in these arguments.
** If rectEnd equals -1, the width of the inserted text is measured first.
*/
void TextBuffer::BufOverlayRect(int startPos, int rectStart, int rectEnd, const char *text, int *charsInserted, int *charsDeleted) {
	int nLines, lineStartPos, nDeleted, insertDeleted, nInserted;
	char *deletedText;

	nLines = countLines(text);
	lineStartPos = BufStartOfLine(startPos);
	if (rectEnd == -1)
		rectEnd = rectStart + textWidth(text, tabDist_, nullSubsChar_);
	lineStartPos = BufStartOfLine(startPos);
	nDeleted = BufEndOfLine(BufCountForwardNLines(startPos, nLines)) - lineStartPos;
	callPreDeleteCBs(lineStartPos, nDeleted);
	deletedText = BufGetRange(lineStartPos, lineStartPos + nDeleted);
	overlayRect(lineStartPos, rectStart, rectEnd, text, &insertDeleted, &nInserted, &cursorPosHint_);
	if (nDeleted != insertDeleted)
		fprintf(stderr, "NEdit internal consistency check ovly1 failed");
	callModifyCBs(lineStartPos, nDeleted, nInserted, 0, deletedText);
	XtFree(deletedText);
	if (charsInserted != nullptr)
		*charsInserted = nInserted;
	if (charsDeleted != nullptr)
		*charsDeleted = nDeleted;
}

/*
** Overlay "text" between displayed character positions "rectStart" and
** "rectEnd" on the line beginning at "startPos".  If charsInserted and
** charsDeleted are not nullptr, the number of characters inserted and deleted
** in the operation (beginning at startPos) are returned in these arguments.
** If rectEnd equals -1, the width of the inserted text is measured first.
*/
void TextBuffer::BufOverlayRectEx(int startPos, int rectStart, int rectEnd, const std::string &text, int *charsInserted, int *charsDeleted) {
	int nLines, lineStartPos, nDeleted, insertDeleted, nInserted;

	nLines = countLinesEx(text);
	lineStartPos = BufStartOfLine(startPos);
	if (rectEnd == -1)
		rectEnd = rectStart + textWidthEx(text, tabDist_, nullSubsChar_);
	lineStartPos = BufStartOfLine(startPos);
	nDeleted = BufEndOfLine(BufCountForwardNLines(startPos, nLines)) - lineStartPos;
	callPreDeleteCBs(lineStartPos, nDeleted);
	std::string deletedText = BufGetRangeEx(lineStartPos, lineStartPos + nDeleted);
	overlayRectEx(lineStartPos, rectStart, rectEnd, text, &insertDeleted, &nInserted, &cursorPosHint_);
	if (nDeleted != insertDeleted)
		fprintf(stderr, "NEdit internal consistency check ovly1 failed");
	callModifyCBs(lineStartPos, nDeleted, nInserted, 0, deletedText);

	if (charsInserted != nullptr)
		*charsInserted = nInserted;
	if (charsDeleted != nullptr)
		*charsDeleted = nDeleted;
}

/*
** Replace a rectangular area in buf, given by "start", "end", "rectStart",
** and "rectEnd", with "text".  If "text" is vertically longer than the
** rectangle, add extra lines to make room for it.
*/
void TextBuffer::BufReplaceRect(int start, int end, int rectStart, int rectEnd, const char *text) {
	char *deletedText;
	char *insText = nullptr;
	int i, nInsertedLines, nDeletedLines, insLen, hint;
	int insertDeleted, insertInserted, deleteInserted;
	int linesPadded = 0;

	/* Make sure start and end refer to complete lines, since the
	   columnar delete and insert operations will replace whole lines */
	start = BufStartOfLine(start);
	end = BufEndOfLine(end);

	callPreDeleteCBs(start, end - start);

	/* If more lines will be deleted than inserted, pad the inserted text
	   with newlines to make it as long as the number of deleted lines.  This
	   will indent all of the text to the right of the rectangle to the same
	   column.  If more lines will be inserted than deleted, insert extra
	   lines in the buffer at the end of the rectangle to make room for the
	   additional lines in "text" */
	nInsertedLines = countLines(text);
	nDeletedLines = BufCountLines(start, end);
	if (nInsertedLines < nDeletedLines) {
		char *insPtr;

		insLen = strlen(text);
		insText = XtMalloc(insLen + nDeletedLines - nInsertedLines + 1);
		strcpy(insText, text);
		insPtr = insText + insLen;
		for (i = 0; i < nDeletedLines - nInsertedLines; i++)
			*insPtr++ = '\n';
		*insPtr = '\0';
	} else if (nDeletedLines < nInsertedLines) {
		linesPadded = nInsertedLines - nDeletedLines;
		for (i = 0; i < linesPadded; i++)
			insert(end, "\n");
	} else /* nDeletedLines == nInsertedLines */ {
	}

	/* Save a copy of the text which will be modified for the modify CBs */
	deletedText = BufGetRange(start, end);

	/* Delete then insert */
	deleteRect(start, end, rectStart, rectEnd, &deleteInserted, &hint);
	if (insText) {
		insertCol(rectStart, start, insText, &insertDeleted, &insertInserted, &cursorPosHint_);
		XtFree(insText);
	} else
		insertCol(rectStart, start, text, &insertDeleted, &insertInserted, &cursorPosHint_);

	/* Figure out how many chars were inserted and call modify callbacks */
	if (insertDeleted != deleteInserted + linesPadded)
		fprintf(stderr, "NEdit: internal consistency check repl1 failed\n");
	callModifyCBs(start, end - start, insertInserted, 0, deletedText);
	XtFree(deletedText);
}

/*
** Replace a rectangular area in buf, given by "start", "end", "rectStart",
** and "rectEnd", with "text".  If "text" is vertically longer than the
** rectangle, add extra lines to make room for it.
*/
void TextBuffer::BufReplaceRectEx(int start, int end, int rectStart, int rectEnd, const std::string &text) {
	char *deletedText;
	char *insText = nullptr;
	int i, nInsertedLines, nDeletedLines, insLen, hint;
	int insertDeleted, insertInserted, deleteInserted;
	int linesPadded = 0;

	/* Make sure start and end refer to complete lines, since the
	   columnar delete and insert operations will replace whole lines */
	start = BufStartOfLine(start);
	end = BufEndOfLine(end);

	callPreDeleteCBs(start, end - start);

	/* If more lines will be deleted than inserted, pad the inserted text
	   with newlines to make it as long as the number of deleted lines.  This
	   will indent all of the text to the right of the rectangle to the same
	   column.  If more lines will be inserted than deleted, insert extra
	   lines in the buffer at the end of the rectangle to make room for the
	   additional lines in "text" */
	nInsertedLines = countLinesEx(text);
	nDeletedLines = BufCountLines(start, end);
	if (nInsertedLines < nDeletedLines) {
		char *insPtr;

		insLen = text.size();
		insText = XtMalloc(insLen + nDeletedLines - nInsertedLines + 1);
		strcpy(insText, text.c_str());
		insPtr = insText + insLen;
		for (i = 0; i < nDeletedLines - nInsertedLines; i++)
			*insPtr++ = '\n';
		*insPtr = '\0';
	} else if (nDeletedLines < nInsertedLines) {
		linesPadded = nInsertedLines - nDeletedLines;
		for (i = 0; i < linesPadded; i++)
			insert(end, "\n");
	} else /* nDeletedLines == nInsertedLines */ {
	}

	/* Save a copy of the text which will be modified for the modify CBs */
	deletedText = BufGetRange(start, end);

	/* Delete then insert */
	deleteRect(start, end, rectStart, rectEnd, &deleteInserted, &hint);
	if (insText) {
		insertCol(rectStart, start, insText, &insertDeleted, &insertInserted, &cursorPosHint_);
		XtFree(insText);
	} else
		insertColEx(rectStart, start, text, &insertDeleted, &insertInserted, &cursorPosHint_);

	/* Figure out how many chars were inserted and call modify callbacks */
	if (insertDeleted != deleteInserted + linesPadded)
		fprintf(stderr, "NEdit: internal consistency check repl1 failed\n");
	callModifyCBs(start, end - start, insertInserted, 0, deletedText);
	XtFree(deletedText);
}

/*
** Remove a rectangular swath of characters between character positions start
** and end and horizontal displayed-character offsets rectStart and rectEnd.
*/
void TextBuffer::BufRemoveRect(int start, int end, int rectStart, int rectEnd) {
	char *deletedText;
	int nInserted;

	start = BufStartOfLine(start);
	end = BufEndOfLine(end);
	callPreDeleteCBs(start, end - start);
	deletedText = BufGetRange(start, end);
	deleteRect(start, end, rectStart, rectEnd, &nInserted, &cursorPosHint_);
	callModifyCBs(start, end - start, nInserted, 0, deletedText);
	XtFree(deletedText);
}

/*
** Clear a rectangular "hole" out of the buffer between character positions
** start and end and horizontal displayed-character offsets rectStart and
** rectEnd.
*/
void TextBuffer::BufClearRect(int start, int end, int rectStart, int rectEnd) {
	int i, nLines;
	char *newlineString;

	nLines = BufCountLines(start, end);
	newlineString = XtMalloc(nLines + 1);
	for (i = 0; i < nLines; i++)
		newlineString[i] = '\n';
	newlineString[i] = '\0';
	BufOverlayRect(start, rectStart, rectEnd, newlineString, nullptr, nullptr);
	XtFree(newlineString);
}

char *TextBuffer::BufGetTextInRect(int start, int end, int rectStart, int rectEnd) {
	int lineStart, selLeft, selRight, len;
	char *textOut, *textIn, *outPtr, *retabbedStr;

	start = BufStartOfLine(start);
	end = BufEndOfLine(end);
	textOut = XtMalloc((end - start) + 1);
	lineStart = start;
	outPtr = textOut;
	while (lineStart <= end) {
		findRectSelBoundariesForCopy(lineStart, rectStart, rectEnd, &selLeft, &selRight);
		textIn = BufGetRange(selLeft, selRight);
		len = selRight - selLeft;
		memcpy(outPtr, textIn, len);
		XtFree(textIn);
		outPtr += len;
		lineStart = BufEndOfLine(selRight) + 1;
		*outPtr++ = '\n';
	}
	if (outPtr != textOut)
		outPtr--; /* don't leave trailing newline */
	*outPtr = '\0';

	/* If necessary, realign the tabs in the selection as if the text were
	   positioned at the left margin */
	retabbedStr = realignTabs(textOut, rectStart, 0, tabDist_, useTabs_, nullSubsChar_, &len);
	XtFree(textOut);
	return retabbedStr;
}

std::string TextBuffer::BufGetTextInRectEx(int start, int end, int rectStart, int rectEnd) {
	int lineStart, selLeft, selRight, len;

	start = BufStartOfLine(start);
	end = BufEndOfLine(end);
	char *textOut = new char[(end - start) + 1];
	lineStart = start;
	char *outPtr = textOut;
	while (lineStart <= end) {
		findRectSelBoundariesForCopy(lineStart, rectStart, rectEnd, &selLeft, &selRight);
		std::string textIn = BufGetRangeEx(selLeft, selRight);
		len = selRight - selLeft;
		std::copy_n(textIn.data(), len, outPtr);
		outPtr += len;
		lineStart = BufEndOfLine(selRight) + 1;
		*outPtr++ = '\n';
	}
	if (outPtr != textOut)
		outPtr--; /* don't leave trailing newline */
	*outPtr = '\0';

	/* If necessary, realign the tabs in the selection as if the text were
	   positioned at the left margin */
	std::string retabbedStr = realignTabsEx(textOut, rectStart, 0, tabDist_, useTabs_, nullSubsChar_, &len);
	delete[] textOut;
	return retabbedStr;
}

/*
** Get the hardware tab distance used by all displays for this buffer,
** and used in computing offsets for rectangular selection operations.
*/
int TextBuffer::BufGetTabDistance() const {
	return tabDist_;
}

/*
** Set the hardware tab distance used by all displays for this buffer,
** and used in computing offsets for rectangular selection operations.
*/
void TextBuffer::BufSetTabDistance(int tabDist) {
	const char *deletedText;

	/* First call the pre-delete callbacks with the previous tab setting
	   still active. */
	callPreDeleteCBs(0, length_);

	/* Change the tab setting */
	tabDist_ = tabDist;

	/* Force any display routines to redisplay everything */
	deletedText = BufAsString();
	callModifyCBs(0, length_, length_, 0, deletedText);
}

void TextBuffer::BufCheckDisplay(int start, int end) {
	/* just to make sure colors in the selected region are up to date */
	callModifyCBs(start, 0, 0, end - start, std::string());
}

void TextBuffer::BufSelect(int start, int end) {
	Selection oldSelection = primary_;

	setSelection(&primary_, start, end);
	redisplaySelection(&oldSelection, &primary_);
}

void TextBuffer::BufUnselect() {
	Selection oldSelection = primary_;

	primary_.selected = False;
	primary_.zeroWidth = False;
	redisplaySelection(&oldSelection, &primary_);
}

void TextBuffer::BufRectSelect(int start, int end, int rectStart, int rectEnd) {
	Selection oldSelection = primary_;

	setRectSelect(&primary_, start, end, rectStart, rectEnd);
	redisplaySelection(&oldSelection, &primary_);
}

int TextBuffer::BufGetSelectionPos(int *start, int *end, int *isRect, int *rectStart, int *rectEnd) {
	return getSelectionPos(&primary_, start, end, isRect, rectStart, rectEnd);
}

/* Same as above, but also returns TRUE for empty selections */
int TextBuffer::BufGetEmptySelectionPos(int *start, int *end, int *isRect, int *rectStart, int *rectEnd) {
	return getSelectionPos(&primary_, start, end, isRect, rectStart, rectEnd) || primary_.zeroWidth;
}

char *TextBuffer::BufGetSelectionText() {
	return getSelectionText(&primary_);
}

std::string TextBuffer::BufGetSelectionTextEx() {
	return getSelectionTextEx(&primary_);
}

void TextBuffer::BufRemoveSelected() {
	removeSelected(&primary_);
}

void TextBuffer::BufReplaceSelected(const char *text) {
	replaceSelected(&primary_, text);
}

void TextBuffer::BufReplaceSelectedEx(const std::string &text) {
	replaceSelectedEx(&primary_, text);
}

void TextBuffer::BufSecondarySelect(int start, int end) {
	Selection oldSelection = secondary_;

	setSelection(&secondary_, start, end);
	redisplaySelection(&oldSelection, &secondary_);
}

void TextBuffer::BufSecondaryUnselect() {
	Selection oldSelection = secondary_;

	secondary_.selected = False;
	secondary_.zeroWidth = False;
	redisplaySelection(&oldSelection, &secondary_);
}

void TextBuffer::BufSecRectSelect(int start, int end, int rectStart, int rectEnd) {
	Selection oldSelection = secondary_;

	setRectSelect(&secondary_, start, end, rectStart, rectEnd);
	redisplaySelection(&oldSelection, &secondary_);
}

int TextBuffer::BufGetSecSelectPos(int *start, int *end, int *isRect, int *rectStart, int *rectEnd) {
	return getSelectionPos(&secondary_, start, end, isRect, rectStart, rectEnd);
}

char *TextBuffer::BufGetSecSelectText() {
	return getSelectionText(&secondary_);
}

std::string TextBuffer::BufGetSecSelectTextEx() {
	return getSelectionTextEx(&secondary_);
}

void TextBuffer::BufRemoveSecSelect() {
	removeSelected(&secondary_);
}

void TextBuffer::BufReplaceSecSelect(const char *text) {
	replaceSelected(&secondary_, text);
}

void TextBuffer::BufReplaceSecSelectEx(const std::string &text) {
	replaceSelectedEx(&secondary_, text);
}

void TextBuffer::BufHighlight(int start, int end) {
	Selection oldSelection = highlight_;

	setSelection(&highlight_, start, end);
	redisplaySelection(&oldSelection, &highlight_);
}

void TextBuffer::BufUnhighlight() {
	Selection oldSelection = highlight_;

	highlight_.selected = False;
	highlight_.zeroWidth = False;
	redisplaySelection(&oldSelection, &highlight_);
}

void TextBuffer::BufRectHighlight(int start, int end, int rectStart, int rectEnd) {
	Selection oldSelection = highlight_;

	setRectSelect(&highlight_, start, end, rectStart, rectEnd);
	redisplaySelection(&oldSelection, &highlight_);
}

int TextBuffer::BufGetHighlightPos(int *start, int *end, int *isRect, int *rectStart, int *rectEnd) {
	return getSelectionPos(&highlight_, start, end, isRect, rectStart, rectEnd);
}

/*
** Add a callback routine to be called when the buffer is modified
*/
void TextBuffer::BufAddModifyCB(bufModifyCallbackProc bufModifiedCB, void *cbArg) {
	CallbackPair<bufModifyCallbackProc> pair{bufModifiedCB, cbArg};
	modifyProcs_.push_back(pair);
}

/*
** Similar to the above, but makes sure that the callback is called before
** normal priority callbacks.
*/
void TextBuffer::BufAddHighPriorityModifyCB(bufModifyCallbackProc bufModifiedCB, void *cbArg) {
	CallbackPair<bufModifyCallbackProc> pair{bufModifiedCB, cbArg};
	modifyProcs_.push_front(pair);
}

void TextBuffer::BufRemoveModifyCB(bufModifyCallbackProc bufModifiedCB, void *cbArg) {
	for (auto it = modifyProcs_.begin(); it != modifyProcs_.end(); ++it) {
		CallbackPair<bufModifyCallbackProc> &pair = *it;
		if (pair.callback == bufModifiedCB && pair.argument == cbArg) {
			modifyProcs_.erase(it);
			return;
		}
	}

	fprintf(stderr, "NEdit Internal Error: Can't find modify CB to remove\n");
}

/*
** Add a callback routine to be called before text is deleted from the buffer.
*/
void TextBuffer::BufAddPreDeleteCB(bufPreDeleteCallbackProc bufPreDeleteCB, void *cbArg) {

	CallbackPair<bufPreDeleteCallbackProc> pair{bufPreDeleteCB, cbArg};
	preDeleteProcs_.push_back(pair);
}

void TextBuffer::BufRemovePreDeleteCB(bufPreDeleteCallbackProc bufPreDeleteCB, void *cbArg) {

	for (auto it = preDeleteProcs_.begin(); it != preDeleteProcs_.end(); ++it) {
		CallbackPair<bufPreDeleteCallbackProc> &pair = *it;
		if (pair.callback == bufPreDeleteCB && pair.argument == cbArg) {
			preDeleteProcs_.erase(it);
			return;
		}
	}

	fprintf(stderr, "NEdit Internal Error: Can't find pre-delete CB to remove\n");
}

/*
** Find the position of the start of the line containing position "pos"
*/
int TextBuffer::BufStartOfLine(int pos) const {
	int startPos;

	if (!searchBackward(pos, '\n', &startPos))
		return 0;
	return startPos + 1;
}

/*
** Find the position of the end of the line containing position "pos"
** (which is either a pointer to the newline character ending the line,
** or a pointer to one character beyond the end of the buffer)
*/
int TextBuffer::BufEndOfLine(int pos) const {
	int endPos;

	if (!searchForward(pos, '\n', &endPos))
		endPos = length_;
	return endPos;
}

/*
** Get a character from the text buffer expanded into it's screen
** representation (which may be several characters for a tab or a
** control code).  Returns the number of characters written to "outStr".
** "indent" is the number of characters from the start of the line
** for figuring tabs.  Output string is guranteed to be shorter or
** equal in length to MAX_EXP_CHAR_LEN
*/
int TextBuffer::BufGetExpandedChar(int pos, const int indent, char *outStr) const {
	return TextBuffer::BufExpandCharacter(BufGetCharacter(pos), indent, outStr, tabDist_, nullSubsChar_);
}

/*
** Expand a single character from the text buffer into it's screen
** representation (which may be several characters for a tab or a
** control code).  Returns the number of characters added to "outStr".
** "indent" is the number of characters from the start of the line
** for figuring tabs.  Output string is guranteed to be shorter or
** equal in length to MAX_EXP_CHAR_LEN
*/
int TextBuffer::BufExpandCharacter(char c, int indent, char *outStr, int tabDist, char nullSubsChar) {
	int i, nSpaces;

	/* Convert tabs to spaces */
	if (c == '\t') {
		nSpaces = tabDist - (indent % tabDist);
		for (i = 0; i < nSpaces; i++)
			outStr[i] = ' ';
		return nSpaces;
	}

	/* Convert ASCII control
	   codes to readable character sequences */
	if (c == nullSubsChar) {
		sprintf(outStr, "<nul>");
		return 5;
	}

	if (((unsigned char)c) <= 31) {
		sprintf(outStr, "<%s>", ControlCodeTable[(unsigned char)c]);
		return strlen(outStr);
	} else if (c == 127) {
		sprintf(outStr, "<del>");
		return 5;
	}

	/* Otherwise, just return the character */
	*outStr = c;
	return 1;
}

/*
** Return the length in displayed characters of character "c" expanded
** for display (as discussed above in TextBuffer::BufGetExpandedChar).  If the
** buffer for which the character width is being measured is doing null
** substitution, nullSubsChar should be passed as that character (or nul
** to ignore).
*/
int TextBuffer::BufCharWidth(char c, int indent, int tabDist, char nullSubsChar) {
	/* Note, this code must parallel that in TextBuffer::BufExpandCharacter */
	if (c == nullSubsChar)
		return 5;
	else if (c == '\t')
		return tabDist - (indent % tabDist);
	else if (((unsigned char)c) <= 31)
		return strlen(ControlCodeTable[(unsigned char)c]) + 2;
	else if (c == 127)
		return 5;
	return 1;
}

/*
** Count the number of displayed characters between buffer position
** "lineStartPos" and "targetPos". (displayed characters are the characters
** shown on the screen to represent characters in the buffer, where tabs and
** control characters are expanded)
*/
int TextBuffer::BufCountDispChars(int lineStartPos, int targetPos) const {
	int pos, charCount = 0;
	char expandedChar[MAX_EXP_CHAR_LEN];

	pos = lineStartPos;
	while (pos < targetPos && pos < length_)
		charCount += BufGetExpandedChar(pos++, charCount, expandedChar);
	return charCount;
}

/*
** Count forward from buffer position "startPos" in displayed characters
** (displayed characters are the characters shown on the screen to represent
** characters in the buffer, where tabs and control characters are expanded)
*/
int TextBuffer::BufCountForwardDispChars(int lineStartPos, int nChars) const {
	int pos, charCount = 0;
	char c;

	pos = lineStartPos;
	while (charCount < nChars && pos < length_) {
		c = BufGetCharacter(pos);
		if (c == '\n')
			return pos;
		charCount += BufCharWidth(c, charCount, tabDist_, nullSubsChar_);
		pos++;
	}
	return pos;
}

/*
** Count the number of newlines between startPos and endPos in buffer "buf".
** The character at position "endPos" is not counted.
*/
int TextBuffer::BufCountLines(int startPos, int endPos) const {
	int pos, gapLen = gapEnd_ - gapStart_;
	int lineCount = 0;

	pos = startPos;
	while (pos < gapStart_) {
		if (pos == endPos)
			return lineCount;
		if (buf_[pos++] == '\n')
			lineCount++;
	}
	while (pos < length_) {
		if (pos == endPos)
			return lineCount;
		if (buf_[pos++ + gapLen] == '\n')
			lineCount++;
	}
	return lineCount;
}

/*
** Find the first character of the line "nLines" forward from "startPos"
** in "buf" and return its position
*/
int TextBuffer::BufCountForwardNLines(int startPos, unsigned nLines) const {
	int pos, gapLen = gapEnd_ - gapStart_;
	unsigned lineCount = 0;

	if (nLines == 0)
		return startPos;

	pos = startPos;
	while (pos < gapStart_) {
		if (buf_[pos++] == '\n') {
			lineCount++;
			if (lineCount == nLines)
				return pos;
		}
	}
	while (pos < length_) {
		if (buf_[pos++ + gapLen] == '\n') {
			lineCount++;
			if (lineCount >= nLines)
				return pos;
		}
	}
	return pos;
}

/*
** Find the position of the first character of the line "nLines" backwards
** from "startPos" (not counting the character pointed to by "startpos" if
** that is a newline) in "buf".  nLines == 0 means find the beginning of
** the line
*/
int TextBuffer::BufCountBackwardNLines(int startPos, int nLines) const {
	int pos, gapLen = gapEnd_ - gapStart_;
	int lineCount = -1;

	pos = startPos - 1;
	if (pos <= 0)
		return 0;

	while (pos >= gapStart_) {
		if (buf_[pos + gapLen] == '\n') {
			if (++lineCount >= nLines)
				return pos + 1;
		}
		pos--;
	}
	while (pos >= 0) {
		if (buf_[pos] == '\n') {
			if (++lineCount >= nLines)
				return pos + 1;
		}
		pos--;
	}
	return 0;
}

/*
** Search forwards in buffer "buf" for characters in "searchChars", starting
** with the character "startPos", and returning the result in "foundPos"
** returns True if found, False if not.
*/
int TextBuffer::BufSearchForward(int startPos, const char *searchChars, int *foundPos) const {
	int pos, gapLen = gapEnd_ - gapStart_;
	const char *c;

	pos = startPos;
	while (pos < gapStart_) {
		for (c = searchChars; *c != '\0'; c++) {
			if (buf_[pos] == *c) {
				*foundPos = pos;
				return True;
			}
		}
		pos++;
	}
	while (pos < length_) {
		for (c = searchChars; *c != '\0'; c++) {
			if (buf_[pos + gapLen] == *c) {
				*foundPos = pos;
				return True;
			}
		}
		pos++;
	}
	*foundPos = length_;
	return False;
}

/*
** Search forwards in buffer "buf" for characters in "searchChars", starting
** with the character "startPos", and returning the result in "foundPos"
** returns True if found, False if not.
*/
int TextBuffer::BufSearchForwardEx(int startPos, const std::string &searchChars, int *foundPos) const {
	int pos, gapLen = gapEnd_ - gapStart_;

	pos = startPos;
	while (pos < gapStart_) {
		for (char ch : searchChars) {
			if (buf_[pos] == ch) {
				*foundPos = pos;
				return True;
			}
		}
		pos++;
	}
	while (pos < length_) {
		for (char ch : searchChars) {
			if (buf_[pos + gapLen] == ch) {
				*foundPos = pos;
				return True;
			}
		}
		pos++;
	}
	*foundPos = length_;
	return False;
}

/*
** Search backwards in buffer "buf" for characters in "searchChars", starting
** with the character BEFORE "startPos", returning the result in "foundPos"
** returns True if found, False if not.
*/
int TextBuffer::BufSearchBackward(int startPos, const char *searchChars, int *foundPos) const {
	int pos, gapLen = gapEnd_ - gapStart_;
	const char *c;

	if (startPos == 0) {
		*foundPos = 0;
		return False;
	}
	pos = startPos == 0 ? 0 : startPos - 1;
	while (pos >= gapStart_) {
		for (c = searchChars; *c != '\0'; c++) {
			if (buf_[pos + gapLen] == *c) {
				*foundPos = pos;
				return True;
			}
		}
		pos--;
	}
	while (pos >= 0) {
		for (c = searchChars; *c != '\0'; c++) {
			if (buf_[pos] == *c) {
				*foundPos = pos;
				return True;
			}
		}
		pos--;
	}
	*foundPos = 0;
	return False;
}

/*
** Search backwards in buffer "buf" for characters in "searchChars", starting
** with the character BEFORE "startPos", returning the result in "foundPos"
** returns True if found, False if not.
*/
int TextBuffer::BufSearchBackwardEx(int startPos, const std::string &searchChars, int *foundPos) const {
	int pos, gapLen = gapEnd_ - gapStart_;

	if (startPos == 0) {
		*foundPos = 0;
		return False;
	}
	pos = startPos == 0 ? 0 : startPos - 1;
	while (pos >= gapStart_) {
		for (char ch : searchChars) {
			if (buf_[pos + gapLen] == ch) {
				*foundPos = pos;
				return True;
			}
		}
		pos--;
	}
	while (pos >= 0) {
		for (char ch : searchChars) {
			if (buf_[pos] == ch) {
				*foundPos = pos;
				return True;
			}
		}
		pos--;
	}
	*foundPos = 0;
	return False;
}

/*
** A horrible design flaw in NEdit (from the very start, before we knew that
** NEdit would become so popular), is that it uses C nullptr terminated strings
** to hold text.  This means editing text containing NUL characters is not
** possible without special consideration.  Here is the special consideration.
** The routines below maintain a special substitution-character which stands
** in for a null, and translates strings an buffers back and forth from/to
** the substituted form, figure out what to substitute, and figure out
** when we're in over our heads and no translation is possible.
*/

/*
** The primary routine for integrating new text into a text buffer with
** substitution of another character for ascii nuls.  This substitutes null
** characters in the string in preparation for being copied or replaced
** into the buffer, and if neccessary, adjusts the buffer as well, in the
** event that the string contains the character it is currently using for
** substitution.  Returns False, if substitution is no longer possible
** because all non-printable characters are already in use.
*/
int TextBuffer::BufSubstituteNullChars(char *string, int length) {
	char histogram[256];

	/* Find out what characters the string contains */
	histogramCharacters(string, length, histogram, True);

	/* Does the string contain the null-substitute character?  If so, re-
	   histogram the buffer text to find a character which is ok in both the
	   string and the buffer, and change the buffer's null-substitution
	   character.  If none can be found, give up and return False */
	if (histogram[(unsigned char)nullSubsChar_] != 0) {
		char *bufString, newSubsChar;
		/* here we know we can modify the file buffer directly,
		   so we cast away constness */
		bufString = (char *)BufAsString();
		histogramCharacters(bufString, length_, histogram, False);
		newSubsChar = chooseNullSubsChar(histogram);
		if (newSubsChar == '\0') {
			return False;
		}
		/* bufString points to the buffer's data, so we substitute in situ */
		subsChars(bufString, length_, nullSubsChar_, newSubsChar);
		nullSubsChar_ = newSubsChar;
	}

	/* If the string contains null characters, substitute them with the
	   buffer's null substitution character */
	if (histogram[0] != 0)
		subsChars(string, length, '\0', nullSubsChar_);
	return True;
}

/*
** The primary routine for integrating new text into a text buffer with
** substitution of another character for ascii nuls.  This substitutes null
** characters in the string in preparation for being copied or replaced
** into the buffer, and if neccessary, adjusts the buffer as well, in the
** event that the string contains the character it is currently using for
** substitution.  Returns False, if substitution is no longer possible
** because all non-printable characters are already in use.
*/
int TextBuffer::BufSubstituteNullCharsEx(std::string &string) {
	char histogram[256];

	/* Find out what characters the string contains */
	histogramCharactersEx(string, histogram, True);

	/* Does the string contain the null-substitute character?  If so, re-
	   histogram the buffer text to find a character which is ok in both the
	   string and the buffer, and change the buffer's null-substitution
	   character.  If none can be found, give up and return False */
	if (histogram[(unsigned char)nullSubsChar_] != 0) {
		char *bufString;
		char newSubsChar;
		/* here we know we can modify the file buffer directly,
		   so we cast away constness */
		bufString = (char *)BufAsString();
		histogramCharacters(bufString, length_, histogram, False);
		newSubsChar = chooseNullSubsChar(histogram);
		if (newSubsChar == '\0') {
			return False;
		}
		/* bufString points to the buffer's data, so we substitute in situ */
		subsChars(bufString, length_, nullSubsChar_, newSubsChar);
		nullSubsChar_ = newSubsChar;
	}

	/* If the string contains null characters, substitute them with the
	   buffer's null substitution character */
	if (histogram[0] != 0)
		subsCharsEx(string, '\0', nullSubsChar_);
	return True;
}

/*
** Convert strings obtained from buffers which contain null characters, which
** have been substituted for by a special substitution character, back to
** a null-containing string.  There is no time penalty for calling this
** routine if no substitution has been done.
*/
void TextBuffer::BufUnsubstituteNullChars(char *string) {
	char *c, subsChar = nullSubsChar_;

	if (subsChar == '\0')
		return;
	for (c = string; *c != '\0'; c++)
		if (*c == subsChar)
			*c = '\0';
}

/*
** Convert strings obtained from buffers which contain null characters, which
** have been substituted for by a special substitution character, back to
** a null-containing string.  There is no time penalty for calling this
** routine if no substitution has been done.
*/
void TextBuffer::BufUnsubstituteNullCharsEx(std::string &string) {
	char subsChar = nullSubsChar_;

	if (subsChar == '\0')
		return;

	std::transform(string.begin(), string.end(), string.begin(), [subsChar](char ch) {
		if (ch == subsChar) {
			return '\0';
		}

		return ch;
	});
}

/*
** Compares len Bytes contained in buf starting at Position pos with
** the contens of cmpText. Returns 0 if there are no differences,
** != 0 otherwise.
**
*/
int TextBuffer::BufCmp(int pos, int len, const char *cmpText) {
	int posEnd;
	int part1Length;
	int result;

	posEnd = pos + len;
	if (posEnd > length_) {
		return (1);
	}
	if (pos < 0) {
		return (-1);
	}

	if (posEnd <= gapStart_) {
		return (strncmp(&(buf_[pos]), cmpText, len));
	} else if (pos >= gapStart_) {
		return (strncmp(&buf_[pos + (gapEnd_ - gapStart_)], cmpText, len));
	} else {
		part1Length = gapStart_ - pos;
		result = strncmp(&buf_[pos], cmpText, part1Length);
		if (result) {
			return (result);
		}
		return (strncmp(&buf_[gapEnd_], &cmpText[part1Length], len - part1Length));
	}
}

/*
** Compares len Bytes contained in buf starting at Position pos with
** the contens of cmpText. Returns 0 if there are no differences,
** != 0 otherwise.
**
*/
int TextBuffer::BufCmpEx(int pos, int len, const std::string &cmpText) {
	int posEnd;
	int part1Length;
	int result;

	posEnd = pos + len;
	if (posEnd > length_) {
		return (1);
	}
	if (pos < 0) {
		return (-1);
	}

	if (posEnd <= gapStart_) {
		return (strncmp(&(buf_[pos]), cmpText.c_str(), len));
	} else if (pos >= gapStart_) {
		return (strncmp(&buf_[pos + (gapEnd_ - gapStart_)], cmpText.c_str(), len));
	} else {
		part1Length = gapStart_ - pos;
		result = strncmp(&buf_[pos], cmpText.c_str(), part1Length);
		if (result) {
			return (result);
		}
		return (strncmp(&buf_[gapEnd_], &cmpText[part1Length], len - part1Length));
	}
}

char *TextBuffer::getSelectionText(Selection *sel) {
	int start, end, isRect, rectStart, rectEnd;
	char *text;

	/* If there's no selection, return an allocated empty string */
	if (!getSelectionPos(sel, &start, &end, &isRect, &rectStart, &rectEnd)) {
		text = XtMalloc(1);
		*text = '\0';
		return text;
	}

	/* If the selection is not rectangular, return the selected range */
	if (isRect)
		return BufGetTextInRect(start, end, rectStart, rectEnd);
	else
		return BufGetRange(start, end);
}

/*
** Internal (non-redisplaying) version of BufInsert.  Returns the length of
** text inserted (this is just strlen(text), however this calculation can be
** expensive and the length will be required by any caller who will continue
** on to call redisplay).  pos must be contiguous with the existing text in
** the buffer (i.e. not past the end).
*/
int TextBuffer::insert(int pos, const char *text) {
	int length = strlen(text);

	/* Prepare the buffer to receive the new text.  If the new text fits in
	   the current buffer, just move the gap (if necessary) to where
	   the text should be inserted.  If the new text is too large, reallocate
	   the buffer with a gap large enough to accomodate the new text and a
	   gap of PreferredGapSize */
	if (length > gapEnd_ - gapStart_)
		reallocateBuf(pos, length + PreferredGapSize);
	else if (pos != gapStart_)
		moveGap(pos);

	/* Insert the new text (pos now corresponds to the start of the gap) */
	memcpy(&buf_[pos], text, length);
	gapStart_ += length;
	length_ += length;
	updateSelections(pos, 0, length);

	return length;
}

/*
** Internal (non-redisplaying) version of BufInsert.  Returns the length of
** text inserted (this is just strlen(text), however this calculation can be
** expensive and the length will be required by any caller who will continue
** on to call redisplay).  pos must be contiguous with the existing text in
** the buffer (i.e. not past the end).
*/
int TextBuffer::insertEx(int pos, const std::string &text) {
	int length = text.size();

	/* Prepare the buffer to receive the new text.  If the new text fits in
	   the current buffer, just move the gap (if necessary) to where
	   the text should be inserted.  If the new text is too large, reallocate
	   the buffer with a gap large enough to accomodate the new text and a
	   gap of PreferredGapSize */
	if (length > gapEnd_ - gapStart_)
		reallocateBuf(pos, length + PreferredGapSize);
	else if (pos != gapStart_)
		moveGap(pos);

	/* Insert the new text (pos now corresponds to the start of the gap) */
	memcpy(&buf_[pos], &text[0], length);
	gapStart_ += length;
	length_ += length;
	updateSelections(pos, 0, length);

	return length;
}

/*
** Search forwards in buffer "buf" for character "searchChar", starting
** with the character "startPos", and returning the result in "foundPos"
** returns True if found, False if not.  (The difference between this and
** BufSearchForward is that it's optimized for single characters.  The
** overall performance of the text widget is dependent on its ability to
** count lines quickly, hence searching for a single character: newline)
*/
int TextBuffer::searchForward(int startPos, char searchChar, int *foundPos) const {
	int pos, gapLen = gapEnd_ - gapStart_;

	pos = startPos;
	while (pos < gapStart_) {
		if (buf_[pos] == searchChar) {
			*foundPos = pos;
			return True;
		}
		pos++;
	}
	while (pos < length_) {
		if (buf_[pos + gapLen] == searchChar) {
			*foundPos = pos;
			return True;
		}
		pos++;
	}
	*foundPos = length_;
	return False;
}

/*
** Search backwards in buffer "buf" for character "searchChar", starting
** with the character BEFORE "startPos", returning the result in "foundPos"
** returns True if found, False if not.  (The difference between this and
** BufSearchBackward is that it's optimized for single characters.  The
** overall performance of the text widget is dependent on its ability to
** count lines quickly, hence searching for a single character: newline)
*/
int TextBuffer::searchBackward(int startPos, char searchChar, int *foundPos) const {
	int pos, gapLen = gapEnd_ - gapStart_;

	if (startPos == 0) {
		*foundPos = 0;
		return False;
	}
	pos = startPos == 0 ? 0 : startPos - 1;
	while (pos >= gapStart_) {
		if (buf_[pos + gapLen] == searchChar) {
			*foundPos = pos;
			return True;
		}
		pos--;
	}
	while (pos >= 0) {
		if (buf_[pos] == searchChar) {
			*foundPos = pos;
			return True;
		}
		pos--;
	}
	*foundPos = 0;
	return False;
}

std::string TextBuffer::getSelectionTextEx(Selection *sel) {
	int start, end, isRect, rectStart, rectEnd;
	std::string text;

	/* If there's no selection, return an allocated empty string */
	if (!getSelectionPos(sel, &start, &end, &isRect, &rectStart, &rectEnd)) {
		return text;
	}

	/* If the selection is not rectangular, return the selected range */
	if (isRect)
		return BufGetTextInRectEx(start, end, rectStart, rectEnd);
	else
		return BufGetRangeEx(start, end);
}

/*
** Call the stored modify callback procedure(s) for this buffer to update the
** changed area(s) on the screen and any other listeners.
*/
void TextBuffer::callModifyCBs(int pos, int nDeleted, int nInserted, int nRestyled, const std::string &deletedText) {

	for (const auto &pair : modifyProcs_) {
		(pair.callback)(pos, nInserted, nDeleted, nRestyled, deletedText, pair.argument);
	}
}

/*
** Call the stored pre-delete callback procedure(s) for this buffer to update
** the changed area(s) on the screen and any other listeners.
*/
void TextBuffer::callPreDeleteCBs(int pos, int nDeleted) {

	for (const auto &pair : preDeleteProcs_) {
		(pair.callback)(pos, nDeleted, pair.argument);
	}
}

/*
** Internal (non-redisplaying) version of BufRemove.  Removes the contents
** of the buffer between start and end (and moves the gap to the site of
** the delete).
*/
void TextBuffer::deleteRange(int start, int end) {
	/* if the gap is not contiguous to the area to remove, move it there */
	if (start > gapStart_)
		moveGap(start);
	else if (end < gapStart_)
		moveGap(end);

	/* expand the gap to encompass the deleted characters */
	gapEnd_ += end - gapStart_;
	gapStart_ -= gapStart_ - start;

	/* update the length */
	length_ -= end - start;

	/* fix up any selections which might be affected by the change */
	updateSelections(start, end - start, 0);
}

/*
** Delete a rectangle of text without calling the modify callbacks.  Returns
** the number of characters replacing those between start and end.  Note that
** in some pathological cases, deleting can actually increase the size of
** the buffer because of tab expansions.  "endPos" returns the buffer position
** of the point in the last line where the text was removed (as a hint for
** routines which need to position the cursor after a delete operation)
*/
void TextBuffer::deleteRect(int start, int end, int rectStart, int rectEnd, int *replaceLen, int *endPos) {
	int nLines, lineStart, lineEnd, len, endOffset;
	char *outStr, *outPtr, *line, *text, *expText;

	/* allocate a buffer for the replacement string large enough to hold
	   possibly expanded tabs as well as an additional  MAX_EXP_CHAR_LEN * 2
	   characters per line for padding where tabs and control characters cross
	   the edges of the selection */
	start = BufStartOfLine(start);
	end = BufEndOfLine(end);
	nLines = BufCountLines(start, end) + 1;
	text = BufGetRange(start, end);
	expText = expandTabs(text, 0, tabDist_, nullSubsChar_, &len);
	XtFree(text);
	XtFree(expText);
	outStr = XtMalloc(len + nLines * MAX_EXP_CHAR_LEN * 2 + 1);

	/* loop over all lines in the buffer between start and end removing
	   the text between rectStart and rectEnd and padding appropriately */
	lineStart = start;
	outPtr = outStr;
	while (lineStart <= length_ && lineStart <= end) {
		lineEnd = BufEndOfLine(lineStart);
		line = BufGetRange(lineStart, lineEnd);
		deleteRectFromLine(line, rectStart, rectEnd, tabDist_, useTabs_, nullSubsChar_, outPtr, &len, &endOffset);
		XtFree(line);
		outPtr += len;
		*outPtr++ = '\n';
		lineStart = lineEnd + 1;
	}
	if (outPtr != outStr)
		outPtr--; /* trim back off extra newline */
	*outPtr = '\0';

	/* replace the text between start and end with the newly created string */
	deleteRange(start, end);
	insert(start, outStr);
	*replaceLen = outPtr - outStr;
	*endPos = start + (outPtr - outStr) - len + endOffset;
	XtFree(outStr);
}

/*
** Find the first and last character position in a line withing a rectangular
** selection (for copying).  Includes tabs which cross rectStart, but not
** control characters which do so.  Leaves off tabs which cross rectEnd.
**
** Technically, the calling routine should convert tab characters which
** cross the right boundary of the selection to spaces which line up with
** the edge of the selection.  Unfortunately, the additional memory
** management required in the parent routine to allow for the changes
** in string size is not worth all the extra work just for a couple of
** shifted characters, so if a tab protrudes, just lop it off and hope
** that there are other characters in the selection to establish the right
** margin for subsequent columnar pastes of this data.
*/
void TextBuffer::findRectSelBoundariesForCopy(int lineStartPos, int rectStart, int rectEnd, int *selStart, int *selEnd) {
	int pos, width, indent = 0;
	char c;

	/* find the start of the selection */
	for (pos = lineStartPos; pos < length_; pos++) {
		c = BufGetCharacter(pos);
		if (c == '\n')
			break;
		width = TextBuffer::BufCharWidth(c, indent, tabDist_, nullSubsChar_);
		if (indent + width > rectStart) {
			if (indent != rectStart && c != '\t') {
				pos++;
				indent += width;
			}
			break;
		}
		indent += width;
	}
	*selStart = pos;

	/* find the end */
	for (; pos < length_; pos++) {
		c = BufGetCharacter(pos);
		if (c == '\n')
			break;
		width = TextBuffer::BufCharWidth(c, indent, tabDist_, nullSubsChar_);
		indent += width;
		if (indent > rectEnd) {
			if (indent - width != rectEnd && c != '\t')
				pos++;
			break;
		}
	}
	*selEnd = pos;
}

/*
** Insert a column of text without calling the modify callbacks.  Note that
** in some pathological cases, inserting can actually decrease the size of
** the buffer because of spaces being coalesced into tabs.  "nDeleted" and
** "nInserted" return the number of characters deleted and inserted beginning
** at the start of the line containing "startPos".  "endPos" returns buffer
** position of the lower left edge of the inserted column (as a hint for
** routines which need to set a cursor position).
*/
void TextBuffer::insertCol(int column, int startPos, const char *insText, int *nDeleted, int *nInserted, int *endPos) {
	int nLines, start, end, insWidth, lineStart, lineEnd;
	int expReplLen, expInsLen, len, endOffset;
	char *outStr, *outPtr, *line, *replText, *expText, *insLine;
	const char *insPtr;

	if (column < 0)
		column = 0;

	/* Allocate a buffer for the replacement string large enough to hold
	   possibly expanded tabs in both the inserted text and the replaced
	   area, as well as per line: 1) an additional 2*MAX_EXP_CHAR_LEN
	   characters for padding where tabs and control characters cross the
	   column of the selection, 2) up to "column" additional spaces per
	   line for padding out to the position of "column", 3) padding up
	   to the width of the inserted text if that must be padded to align
	   the text beyond the inserted column.  (Space for additional
	   newlines if the inserted text extends beyond the end of the buffer
	   is counted with the length of insText) */
	start = BufStartOfLine(startPos);
	nLines = countLines(insText) + 1;
	insWidth = textWidth(insText, tabDist_, nullSubsChar_);
	end = BufEndOfLine(BufCountForwardNLines(start, nLines - 1));
	replText = BufGetRange(start, end);
	expText = expandTabs(replText, 0, tabDist_, nullSubsChar_, &expReplLen);
	XtFree(replText);
	XtFree(expText);
	expText = expandTabs(insText, 0, tabDist_, nullSubsChar_, &expInsLen);
	XtFree(expText);
	outStr = XtMalloc(expReplLen + expInsLen + nLines * (column + insWidth + MAX_EXP_CHAR_LEN) + 1);

	/* Loop over all lines in the buffer between start and end inserting
	   text at column, splitting tabs and adding padding appropriately */
	outPtr = outStr;
	lineStart = start;
	insPtr = insText;
	while (True) {
		lineEnd = BufEndOfLine(lineStart);
		line = BufGetRange(lineStart, lineEnd);
		insLine = copyLine(insPtr, &len);
		insPtr += len;
		insertColInLine(line, insLine, column, insWidth, tabDist_, useTabs_, nullSubsChar_, outPtr, &len, &endOffset);
		XtFree(line);
		XtFree(insLine);
#if 0 /* Earlier comments claimed that trailing whitespace could multiply on                                                                                                                                                                   \
      the ends of lines, but insertColInLine looks like it should never                                                                                                                                                                        \
      add space unnecessarily, and this trimming interfered with                                                                                                                                                                               \
      paragraph filling, so lets see if it works without it. MWE */
        {
            char *c;
    	    for (c=outPtr+len-1; c>outPtr && (*c == ' ' || *c == '\t'); c--)
                len--;
        }
#endif
		outPtr += len;
		*outPtr++ = '\n';
		lineStart = lineEnd < length_ ? lineEnd + 1 : length_;
		if (*insPtr == '\0')
			break;
		insPtr++;
	}
	if (outPtr != outStr)
		outPtr--; /* trim back off extra newline */
	*outPtr = '\0';

	/* replace the text between start and end with the new stuff */
	deleteRange(start, end);
	insert(start, outStr);
	*nInserted = outPtr - outStr;
	*nDeleted = end - start;
	*endPos = start + (outPtr - outStr) - len + endOffset;
	XtFree(outStr);
}

/*
** Insert a column of text without calling the modify callbacks.  Note that
** in some pathological cases, inserting can actually decrease the size of
** the buffer because of spaces being coalesced into tabs.  "nDeleted" and
** "nInserted" return the number of characters deleted and inserted beginning
** at the start of the line containing "startPos".  "endPos" returns buffer
** position of the lower left edge of the inserted column (as a hint for
** routines which need to set a cursor position).
*/
void TextBuffer::insertColEx(int column, int startPos, const std::string &insText, int *nDeleted, int *nInserted, int *endPos) {
	int nLines, start, end, insWidth, lineStart, lineEnd;
	int expReplLen, expInsLen, len, endOffset;
	char *outStr, *outPtr;

	if (column < 0)
		column = 0;

	/* Allocate a buffer for the replacement string large enough to hold
	   possibly expanded tabs in both the inserted text and the replaced
	   area, as well as per line: 1) an additional 2*MAX_EXP_CHAR_LEN
	   characters for padding where tabs and control characters cross the
	   column of the selection, 2) up to "column" additional spaces per
	   line for padding out to the position of "column", 3) padding up
	   to the width of the inserted text if that must be padded to align
	   the text beyond the inserted column.  (Space for additional
	   newlines if the inserted text extends beyond the end of the buffer
	   is counted with the length of insText) */
	start = BufStartOfLine(startPos);
	nLines = countLinesEx(insText) + 1;

	insWidth = textWidthEx(insText, tabDist_, nullSubsChar_);
	end = BufEndOfLine(BufCountForwardNLines(start, nLines - 1));
	std::string replText = BufGetRangeEx(start, end);
	std::string expText = expandTabsEx(replText, 0, tabDist_, nullSubsChar_, &expReplLen);

	expText = expandTabsEx(insText, 0, tabDist_, nullSubsChar_, &expInsLen);

	outStr = XtMalloc(expReplLen + expInsLen + nLines * (column + insWidth + MAX_EXP_CHAR_LEN) + 1);

	/* Loop over all lines in the buffer between start and end inserting
	   text at column, splitting tabs and adding padding appropriately */
	outPtr = outStr;
	lineStart = start;
	auto insPtr = insText.begin();
	while (True) {
		lineEnd = BufEndOfLine(lineStart);
		std::string line = BufGetRangeEx(lineStart, lineEnd);
		std::string insLine = copyLineEx(insPtr, insText.end(), &len);
		insPtr += len;
		insertColInLineEx(line, insLine, column, insWidth, tabDist_, useTabs_, nullSubsChar_, outPtr, &len, &endOffset);

#if 0 /* Earlier comments claimed that trailing whitespace could multiply on                                                                                                                                                                   \
      the ends of lines, but insertColInLine looks like it should never                                                                                                                                                                        \
      add space unnecessarily, and this trimming interfered with                                                                                                                                                                               \
      paragraph filling, so lets see if it works without it. MWE */
        {
            char *c;
    	    for (c=outPtr+len-1; c>outPtr && (*c == ' ' || *c == '\t'); c--)
                len--;
        }
#endif
		outPtr += len;
		*outPtr++ = '\n';
		lineStart = lineEnd < length_ ? lineEnd + 1 : length_;
		if (*insPtr == '\0')
			break;
		insPtr++;
	}
	if (outPtr != outStr)
		outPtr--; /* trim back off extra newline */
	*outPtr = '\0';

	/* replace the text between start and end with the new stuff */
	deleteRange(start, end);
	insert(start, outStr);
	*nInserted = outPtr - outStr;
	*nDeleted = end - start;
	*endPos = start + (outPtr - outStr) - len + endOffset;
	XtFree(outStr);
}

void TextBuffer::moveGap(int pos) {
	int gapLen = gapEnd_ - gapStart_;

	if (pos > gapStart_)
		memmove(&buf_[gapStart_], &buf_[gapEnd_], pos - gapStart_);
	else
		memmove(&buf_[pos + gapLen], &buf_[pos], gapStart_ - pos);
	gapEnd_ += pos - gapStart_;
	gapStart_ += pos - gapStart_;
}

/*
** Overlay a rectangular area of text without calling the modify callbacks.
** "nDeleted" and "nInserted" return the number of characters deleted and
** inserted beginning at the start of the line containing "startPos".
** "endPos" returns buffer position of the lower left edge of the inserted
** column (as a hint for routines which need to set a cursor position).
*/
void TextBuffer::overlayRect(int startPos, int rectStart, int rectEnd, const char *insText, int *nDeleted, int *nInserted, int *endPos) {
	int nLines, start, end, lineStart, lineEnd;
	int expInsLen, len, endOffset;
	char *c, *outStr, *outPtr, *line, *expText, *insLine;
	const char *insPtr;

	/* Allocate a buffer for the replacement string large enough to hold
	   possibly expanded tabs in the inserted text, as well as per line: 1)
	   an additional 2*MAX_EXP_CHAR_LEN characters for padding where tabs
	   and control characters cross the column of the selection, 2) up to
	   "column" additional spaces per line for padding out to the position
	   of "column", 3) padding up to the width of the inserted text if that
	   must be padded to align the text beyond the inserted column.  (Space
	   for additional newlines if the inserted text extends beyond the end
	   of the buffer is counted with the length of insText) */
	start = BufStartOfLine(startPos);
	nLines = countLines(insText) + 1;
	end = BufEndOfLine(BufCountForwardNLines(start, nLines - 1));
	expText = expandTabs(insText, 0, tabDist_, nullSubsChar_, &expInsLen);
	XtFree(expText);
	outStr = XtMalloc(end - start + expInsLen + nLines * (rectEnd + MAX_EXP_CHAR_LEN) + 1);

	/* Loop over all lines in the buffer between start and end overlaying the
	   text between rectStart and rectEnd and padding appropriately.  Trim
	   trailing space from line (whitespace at the ends of lines otherwise
	   tends to multiply, since additional padding is added to maintain it */
	outPtr = outStr;
	lineStart = start;
	insPtr = insText;
	while (True) {
		lineEnd = BufEndOfLine(lineStart);
		line = BufGetRange(lineStart, lineEnd);
		insLine = copyLine(insPtr, &len);
		insPtr += len;
		overlayRectInLine(line, insLine, rectStart, rectEnd, tabDist_, useTabs_, nullSubsChar_, outPtr, &len, &endOffset);
		XtFree(line);
		XtFree(insLine);
		for (c = outPtr + len - 1; c > outPtr && (*c == ' ' || *c == '\t'); c--)
			len--;
		outPtr += len;
		*outPtr++ = '\n';
		lineStart = lineEnd < length_ ? lineEnd + 1 : length_;
		if (*insPtr == '\0')
			break;
		insPtr++;
	}
	if (outPtr != outStr)
		outPtr--; /* trim back off extra newline */
	*outPtr = '\0';

	/* replace the text between start and end with the new stuff */
	deleteRange(start, end);
	insert(start, outStr);
	*nInserted = outPtr - outStr;
	*nDeleted = end - start;
	*endPos = start + (outPtr - outStr) - len + endOffset;
	XtFree(outStr);
}

/*
** Overlay a rectangular area of text without calling the modify callbacks.
** "nDeleted" and "nInserted" return the number of characters deleted and
** inserted beginning at the start of the line containing "startPos".
** "endPos" returns buffer position of the lower left edge of the inserted
** column (as a hint for routines which need to set a cursor position).
*/
void TextBuffer::overlayRectEx(int startPos, int rectStart, int rectEnd, const std::string &insText, int *nDeleted, int *nInserted, int *endPos) {
	int nLines, start, end, lineStart, lineEnd;
	int expInsLen, len, endOffset;
	char *c, *outStr, *outPtr;

	/* Allocate a buffer for the replacement string large enough to hold
	   possibly expanded tabs in the inserted text, as well as per line: 1)
	   an additional 2*MAX_EXP_CHAR_LEN characters for padding where tabs
	   and control characters cross the column of the selection, 2) up to
	   "column" additional spaces per line for padding out to the position
	   of "column", 3) padding up to the width of the inserted text if that
	   must be padded to align the text beyond the inserted column.  (Space
	   for additional newlines if the inserted text extends beyond the end
	   of the buffer is counted with the length of insText) */
	start = BufStartOfLine(startPos);
	nLines = countLinesEx(insText) + 1;
	end = BufEndOfLine(BufCountForwardNLines(start, nLines - 1));
	std::string expText = expandTabsEx(insText, 0, tabDist_, nullSubsChar_, &expInsLen);

	outStr = XtMalloc(end - start + expInsLen + nLines * (rectEnd + MAX_EXP_CHAR_LEN) + 1);

	/* Loop over all lines in the buffer between start and end overlaying the
	   text between rectStart and rectEnd and padding appropriately.  Trim
	   trailing space from line (whitespace at the ends of lines otherwise
	   tends to multiply, since additional padding is added to maintain it */
	outPtr = outStr;
	lineStart = start;
	auto insPtr = insText.begin();
	while (True) {
		lineEnd = BufEndOfLine(lineStart);
		std::string line = BufGetRangeEx(lineStart, lineEnd);
		std::string insLine = copyLineEx(insPtr, insText.end(), &len);
		insPtr += len;
		overlayRectInLineEx(line, insLine, rectStart, rectEnd, tabDist_, useTabs_, nullSubsChar_, outPtr, &len, &endOffset);

		for (c = outPtr + len - 1; c > outPtr && (*c == ' ' || *c == '\t'); c--)
			len--;
		outPtr += len;
		*outPtr++ = '\n';
		lineStart = lineEnd < length_ ? lineEnd + 1 : length_;
		if (insPtr == insText.end())
			break;
		insPtr++;
	}
	if (outPtr != outStr)
		outPtr--; /* trim back off extra newline */
	*outPtr = '\0';

	/* replace the text between start and end with the new stuff */
	deleteRange(start, end);
	insert(start, outStr);
	*nInserted = outPtr - outStr;
	*nDeleted = end - start;
	*endPos = start + (outPtr - outStr) - len + endOffset;
	XtFree(outStr);
}

/*
** reallocate the text storage in "buf" to have a gap starting at "newGapStart"
** and a gap size of "newGapLen", preserving the buffer's current contents.
*/
void TextBuffer::reallocateBuf(int newGapStart, int newGapLen) {
	char *newBuf;
	int newGapEnd;

	newBuf = XtMalloc(length_ + newGapLen + 1);
	newBuf[length_ + PreferredGapSize] = '\0';
	newGapEnd = newGapStart + newGapLen;
	if (newGapStart <= gapStart_) {
		memcpy(newBuf, buf_, newGapStart);
		memcpy(&newBuf[newGapEnd], &buf_[newGapStart], gapStart_ - newGapStart);
		memcpy(&newBuf[newGapEnd + gapStart_ - newGapStart], &buf_[gapEnd_], length_ - gapStart_);
	} else { /* newGapStart > gapStart_ */
		memcpy(newBuf, buf_, gapStart_);
		memcpy(&newBuf[gapStart_], &buf_[gapEnd_], newGapStart - gapStart_);
		memcpy(&newBuf[newGapEnd], &buf_[gapEnd_ + newGapStart - gapStart_], length_ - newGapStart);
	}
	XtFree(buf_);
	buf_ = newBuf;
	gapStart_ = newGapStart;
	gapEnd_ = newGapEnd;
#ifdef PURIFY
	std::fill(&buf_[gapStart_], &buf_[gapEnd_], '.');
#endif
}

/*
** Call the stored redisplay procedure(s) for this buffer to update the
** screen for a change in a selection.
*/
void TextBuffer::redisplaySelection(Selection *oldSelection, Selection *newSelection) {
	int oldStart, oldEnd, newStart, newEnd, ch1Start, ch1End, ch2Start, ch2End;

	/* If either selection is rectangular, add an additional character to
	   the end of the selection to request the redraw routines to wipe out
	   the parts of the selection beyond the end of the line */
	oldStart = oldSelection->start;
	newStart = newSelection->start;
	oldEnd = oldSelection->end;
	newEnd = newSelection->end;
	if (oldSelection->rectangular)
		oldEnd++;
	if (newSelection->rectangular)
		newEnd++;

	/* If the old or new selection is unselected, just redisplay the
	   single area that is (was) selected and return */
	if (!oldSelection->selected && !newSelection->selected)
		return;
	if (!oldSelection->selected) {
		callModifyCBs(newStart, 0, 0, newEnd - newStart, std::string());
		return;
	}
	if (!newSelection->selected) {
		callModifyCBs(oldStart, 0, 0, oldEnd - oldStart, std::string());
		return;
	}

	/* If the selection changed from normal to rectangular or visa versa, or
	   if a rectangular selection changed boundaries, redisplay everything */
	if ((oldSelection->rectangular && !newSelection->rectangular) || (!oldSelection->rectangular && newSelection->rectangular) ||
	    (oldSelection->rectangular && ((oldSelection->rectStart != newSelection->rectStart) || (oldSelection->rectEnd != newSelection->rectEnd)))) {
		callModifyCBs(std::min<int>(oldStart, newStart), 0, 0, std::max<int>(oldEnd, newEnd) - std::min<int>(oldStart, newStart), std::string());
		return;
	}

	/* If the selections are non-contiguous, do two separate updates
	   and return */
	if (oldEnd < newStart || newEnd < oldStart) {
		callModifyCBs(oldStart, 0, 0, oldEnd - oldStart, std::string());
		callModifyCBs(newStart, 0, 0, newEnd - newStart, std::string());
		return;
	}

	/* Otherwise, separate into 3 separate regions: ch1, and ch2 (the two
	   changed areas), and the unchanged area of their intersection,
	   and update only the changed area(s) */
	ch1Start = std::min<int>(oldStart, newStart);
	ch2End = std::max<int>(oldEnd, newEnd);
	ch1End = std::max<int>(oldStart, newStart);
	ch2Start = std::min<int>(oldEnd, newEnd);
	if (ch1Start != ch1End)
		callModifyCBs(ch1Start, 0, 0, ch1End - ch1Start, std::string());
	if (ch2Start != ch2End)
		callModifyCBs(ch2Start, 0, 0, ch2End - ch2Start, std::string());
}

void TextBuffer::removeSelected(Selection *sel) {
	int start, end;
	int isRect, rectStart, rectEnd;

	if (!getSelectionPos(sel, &start, &end, &isRect, &rectStart, &rectEnd))
		return;
	if (isRect)
		BufRemoveRect(start, end, rectStart, rectEnd);
	else
		BufRemove(start, end);
}

void TextBuffer::replaceSelected(Selection *sel, const char *text) {
	int start, end, isRect, rectStart, rectEnd;
	Selection oldSelection = *sel;

	/* If there's no selection, return */
	if (!getSelectionPos(sel, &start, &end, &isRect, &rectStart, &rectEnd))
		return;

	/* Do the appropriate type of replace */
	if (isRect)
		BufReplaceRect(start, end, rectStart, rectEnd, text);
	else
		BufReplace(start, end, text);

	/* Unselect (happens automatically in BufReplace, but BufReplaceRect
	   can't detect when the contents of a selection goes away) */
	sel->selected = False;
	redisplaySelection(&oldSelection, sel);
}

void TextBuffer::replaceSelectedEx(Selection *sel, const std::string &text) {
	int start, end, isRect, rectStart, rectEnd;
	Selection oldSelection = *sel;

	/* If there's no selection, return */
	if (!getSelectionPos(sel, &start, &end, &isRect, &rectStart, &rectEnd))
		return;

	/* Do the appropriate type of replace */
	if (isRect)
		BufReplaceRectEx(start, end, rectStart, rectEnd, text);
	else
		BufReplaceEx(start, end, text);

	/* Unselect (happens automatically in BufReplace, but BufReplaceRect
	   can't detect when the contents of a selection goes away) */
	sel->selected = False;
	redisplaySelection(&oldSelection, sel);
}

/*
** Update all of the selections in "buf" for changes in the buffer's text
*/
void TextBuffer::updateSelections(int pos, int nDeleted, int nInserted) {
	updateSelection(&primary_, pos, nDeleted, nInserted);
	updateSelection(&secondary_, pos, nDeleted, nInserted);
	updateSelection(&highlight_, pos, nDeleted, nInserted);
}
