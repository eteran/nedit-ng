/*******************************************************************************
*                                                                              *
* highlight.c -- Nirvana Editor syntax highlighting (text coloring and font    *
*                        selected by file content                              *
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
* June 24, 1996                                                                *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/

#include "highlight.h"
#include "DocumentWidget.h"
#include "HighlightData.h"
#include "HighlightPattern.h"
#include "PatternSet.h"
#include "ReparseContext.h"
#include "StyleTableEntry.h"
#include "TextArea.h"
#include "TextBuffer.h"
#include "WindowHighlightData.h"
#include "X11Colors.h"
#include "highlightData.h"
#include "preferences.h"
#include "regularExp.h"
#include <QMessageBox>
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

/* Initial forward expansion of parsing region in incremental reparsing,
   when style changes propagate forward beyond the original modification.
   This distance is increased by a factor of two for each subsequent step. */
constexpr int REPARSE_CHUNK_SIZE = 80;

constexpr bool is_plain(int style) {
	return (style == PLAIN_STYLE || style == UNFINISHED_STYLE);
}

/* Compare two styles where one of the styles may not yet have been processed
   with pass2 patterns */
constexpr bool equivalent_style(int style1, int style2, int firstPass2Style) {
	return (style1 == style2) || 
           (style1 == UNFINISHED_STYLE && (style2 == PLAIN_STYLE || static_cast<uint8_t>(style2) >= firstPass2Style)) ||
           (style2 == UNFINISHED_STYLE && (style1 == PLAIN_STYLE || static_cast<uint8_t>(style1) >= firstPass2Style));
}

/* Scanning context can be reduced (with big efficiency gains) if we
   know that patterns can't cross line boundaries, which is implied
   by a context requirement of 1 line and 0 characters */
bool can_cross_line_boundaries(const ReparseContext *contextRequirements) {
	return (contextRequirements->nLines != 1 || contextRequirements->nChars != 0);
}

}

static int findSafeParseRestartPos(TextBuffer *buf, const std::unique_ptr<WindowHighlightData> &highlightData, int *pos);
static bool isParentStyle(const QByteArray &parentStyles, int style1, int style2);
static int lastModified(const std::shared_ptr<TextBuffer> &styleBuf);
static int parentStyleOf(const QByteArray &parentStyles, int style);
static int parseBufferRange(HighlightData *pass1Patterns, HighlightData *pass2Patterns, TextBuffer *buf, const std::shared_ptr<TextBuffer> &styleBuf, ReparseContext *contextRequirements, int beginParse, int endParse, const QString &delimiters);
static void fillStyleString(const char **stringPtr, char **stylePtr, const char *toPtr, char style, char *prevChar);
static void incrementalReparse(const std::unique_ptr<WindowHighlightData> &highlightData, TextBuffer *buf, int pos, int nInserted, const QString &delimiters);
static void modifyStyleBuf(const std::shared_ptr<TextBuffer> &styleBuf, char *styleString, int startPos, int endPos, int firstPass2Style);
static void passTwoParseString(HighlightData *pattern, const char *string, char *styleString, int length, char *prevChar, const QString &delimiters, const char *lookBehindTo, const char *match_till);
static void recolorSubexpr(const std::shared_ptr<regexp> &re, int subexpr, int style, const char *string, char *styleString);

/*
** Buffer modification callback for triggering re-parsing of modified
** text and keeping the style buffer synchronized with the text buffer.
** This must be attached to the the text buffer BEFORE any widget text
** display callbacks, so it can get the style buffer ready to be used
** by the text display routines.
**
** Update the style buffer for changes to the text, and mark any style
** changes by selecting the region in the style buffer.  This strange
** protocol of informing the text display to redraw style changes by
** making selections in the style buffer is used because this routine
** is intended to be called BEFORE the text display callback paints the
** text (to minimize redraws and, most importantly, to synchronize the
** style buffer with the text buffer).  If we redraw now, the text
** display hasn't yet processed the modification, redrawing later is
** not only complicated, it will double-draw almost everything typed.
**
** Note: This routine must be kept efficient.  It is called for every
** character typed.
*/
void SyntaxHighlightModifyCBEx(int pos, int nInserted, int nDeleted, int nRestyled, view::string_view deletedText, void *user) {

    Q_UNUSED(nRestyled);
    Q_UNUSED(deletedText);

    auto document = static_cast<DocumentWidget *>(user);
    const std::unique_ptr<WindowHighlightData> &highlightData = document->highlightData_;

    if(!highlightData) {
        return;
    }

    /* Restyling-only modifications (usually a primary or secondary  selection)
       don't require any processing, but clear out the style buffer selection
       so the widget doesn't think it has to keep redrawing the old area */
    if (nInserted == 0 && nDeleted == 0) {
        highlightData->styleBuffer->BufUnselect();
        return;
    }

    /* First and foremost, the style buffer must track the text buffer
       accurately and correctly */
    if (nInserted > 0) {
        std::string insStyle(nInserted, UNFINISHED_STYLE);
        highlightData->styleBuffer->BufReplaceEx(pos, pos + nDeleted, insStyle);
    } else {
        highlightData->styleBuffer->BufRemove(pos, pos + nDeleted);
    }

    /* Mark the changed region in the style buffer as requiring redraw.  This
       is not necessary for getting it redrawn, it will be redrawn anyhow by
       the text display callback, but it clears the previous selection and
       saves the modifyStyleBuf routine from unnecessary work in tracking
       changes that are already scheduled for redraw */
    highlightData->styleBuffer->BufSelect(pos, pos + nInserted);

    // Re-parse around the changed region
    if (highlightData->pass1Patterns) {
        incrementalReparse(highlightData, document->buffer_, pos, nInserted, document->GetWindowDelimiters());
    }
}

/*
** Remove style information from a text widget and redisplay it.
*/
void RemoveWidgetHighlightEx(TextArea *area) {
    area->TextDAttachHighlightData(nullptr, nullptr, 0, UNFINISHED_STYLE, nullptr, nullptr);
}

void handleUnparsedRegionCBEx(const TextArea *area, int pos, const void *user) {
    auto document = static_cast<const DocumentWidget *>(user);
    document->handleUnparsedRegionEx(area->getStyleBuffer(), pos);
}

/*
** Re-parse the smallest region possible around a modification to buffer "buf"
** to gurantee that the promised context lines and characters have
** been presented to the patterns.  Changes the style buffer in "highlightData"
** with the parsing result.
*/
static void incrementalReparse(const std::unique_ptr<WindowHighlightData> &highlightData, TextBuffer *buf, int pos, int nInserted, const QString &delimiters) {

    const std::shared_ptr<TextBuffer> &styleBuf = highlightData->styleBuffer;
	HighlightData *pass1Patterns = highlightData->pass1Patterns;
	HighlightData *pass2Patterns = highlightData->pass2Patterns;
	ReparseContext *context      = &highlightData->contextRequirements;
		
    QByteArray parentStyles = highlightData->parentStyles;

	/* Find the position "beginParse" at which to begin reparsing.  This is
	   far enough back in the buffer such that the guranteed number of
	   lines and characters of context are examined. */
    int beginParse = pos;
    int parseInStyle = findSafeParseRestartPos(buf, highlightData, &beginParse);

	/* Find the position "endParse" at which point it is safe to stop
	   parsing, unless styles are getting changed beyond the last
	   modification */
    int lastMod  = pos + nInserted;
    int endParse = forwardOneContext(buf, context, lastMod);

	/*
	** Parse the buffer from beginParse, until styles compare
	** with originals for one full context distance.  Distance increases
	** by powers of two until nothing changes from previous step.  If
	** parsing ends before endParse, start again one level up in the
	** pattern hierarchy
	*/
    for (int nPasses = 0;; ++nPasses) {

		/* Parse forward from beginParse to one context beyond the end
		   of the last modification */
        HighlightData *startPattern = patternOfStyle(pass1Patterns, parseInStyle);
		/* If there is no pattern matching the style, it must be a pass-2
		   style. It that case, it is (probably) safe to start parsing with
		   the root pass-1 pattern again. Anyway, passing a nullptr-pointer to
		   the parse routine would result in a crash; restarting with pass-1
		   patterns is certainly preferable, even if there is a slight chance
		   of a faulty coloring. */
		if (!startPattern) {
			startPattern = pass1Patterns;
		}

		int endAt = parseBufferRange(startPattern, pass2Patterns, buf, styleBuf, context, beginParse, endParse, delimiters);

		/* If parse completed at this level, move one style up in the
		   hierarchy and start again from where the previous parse left off. */
		if (endAt < endParse) {
			beginParse = endAt;
            endParse = forwardOneContext(buf, context, std::max(endAt, std::max(lastModified(styleBuf), lastMod)));
			if (is_plain(parseInStyle)) {
                qCritical("NEdit: internal error: incr. reparse fell short");
				return;
			}
			parseInStyle = parentStyleOf(parentStyles, parseInStyle);

			// One context distance beyond last style changed means we're done 
		} else if (lastModified(styleBuf) <= lastMod) {
			return;

			/* Styles are changing beyond the modification, continue extending
			   the end of the parse range by powers of 2 * REPARSE_CHUNK_SIZE and
			   reparse until nothing changes */
		} else {
			lastMod = lastModified(styleBuf);
            endParse = std::min(buf->BufGetLength(), forwardOneContext(buf, context, lastMod) + (REPARSE_CHUNK_SIZE << nPasses));
		}
	}
}

/*
** Parse text in buffer "buf" between positions "beginParse" and "endParse"
** using pass 1 patterns over the entire range and pass 2 patterns where needed
** to determine whether re-parsed areas have changed and need to be redrawn.
** Deposits style information in "styleBuf" and expands the selection in
** styleBuf to show the additional areas which have changed and need
** redrawing.  beginParse must be a position from which pass 1 parsing may
** safely be started using the pass1Patterns given.  Internally, adds a
** "takeoff" safety region before beginParse, so that pass 2 patterns will be
** allowed to match properly if they begin before beginParse, and a "landing"
** safety region beyond endparse so that endParse is guranteed to be parsed
** correctly in both passes.  Returns the buffer position at which parsing
** finished (this will normally be endParse, unless the pass1Patterns is a
** pattern which does end and the end is reached).
*/
static int parseBufferRange(HighlightData *pass1Patterns, HighlightData *pass2Patterns, TextBuffer *buf, const std::shared_ptr<TextBuffer> &styleBuf, ReparseContext *contextRequirements, int beginParse, int endParse, const QString &delimiters) {

    int endSafety;
    int endPass2Safety;
    int startPass2Safety;
    int modStart;
    int modEnd;
    int beginSafety;
    int p;
    int style;
    int firstPass2Style = pass2Patterns == nullptr ? INT_MAX : static_cast<uint8_t>(pass2Patterns[1].style);

	// Begin parsing one context distance back (or to the last style change) 
    int beginStyle = pass1Patterns->style;
	if (can_cross_line_boundaries(contextRequirements)) {
        beginSafety = backwardOneContext(buf, contextRequirements, beginParse);
		for (p = beginParse; p >= beginSafety; p--) {
			style = styleBuf->BufGetCharacter(p - 1);
			if (!equivalent_style(style, beginStyle, firstPass2Style)) {
				beginSafety = p;
				break;
			}
		}
	} else {
        for (beginSafety = std::max(0, beginParse - 1); beginSafety > 0; beginSafety--) {
			style = styleBuf->BufGetCharacter(beginSafety);
			if (!equivalent_style(style, beginStyle, firstPass2Style) || buf->BufGetCharacter(beginSafety) == '\n') {
				beginSafety++;
				break;
			}
		}
	}

	/* Parse one parse context beyond requested end to gurantee that parsing
	   at endParse is complete, unless patterns can't cross line boundaries,
	   in which case the end of the line is fine */
    if (endParse == 0) {
		return 0;
    }

    if (can_cross_line_boundaries(contextRequirements)) {
		endSafety = forwardOneContext(buf, contextRequirements, endParse);
    } else if (endParse >= buf->BufGetLength() || (buf->BufGetCharacter(endParse - 1) == '\n')) {
		endSafety = endParse;
    } else {
        endSafety = std::min(buf->BufGetLength(), buf->BufEndOfLine(endParse) + 1);
    }

	// copy the buffer range into a string 
	
	std::string str      = buf->BufGetRangeEx(beginSafety, endSafety);
	std::string styleStr = styleBuf->BufGetRangeEx(beginSafety, endSafety);
	
	const char *const string   = &str[0];
	char *const styleString    = &styleStr[0];
	const char *const match_to = string + str.size();

	// Parse it with pass 1 patterns 
	// printf("parsing from %d thru %d\n", beginSafety, endSafety); 
	char prevChar         = getPrevChar(buf, beginParse);
	const char *stringPtr = &string[beginParse - beginSafety];
	char *stylePtr        = &styleString[beginParse - beginSafety];
	
	parseString(
		pass1Patterns, 
		&stringPtr, 
		&stylePtr, 
		endParse - beginParse, 
		&prevChar, 
		false, 
        delimiters,
		string, 
		match_to);

	// On non top-level patterns, parsing can end early 
    endParse = std::min<int>(endParse, stringPtr - string + beginSafety);

	// If there are no pass 2 patterns, we're done 
	if (!pass2Patterns)
		goto parseDone;

	/* Parsing of pass 2 patterns is done only as necessary for determining
	   where styles have changed.  Find the area to avoid, which is already
	   marked as changed (all inserted text and previously modified areas) */
	if (styleBuf->primary_.selected) {
		modStart = styleBuf->primary_.start;
		modEnd = styleBuf->primary_.end;
	} else {
		modStart = modEnd = 0;
	}

	/* Re-parse the areas before the modification with pass 2 patterns, from
	   beginSafety to far enough beyond modStart to gurantee that parsing at
	   modStart is correct (pass 2 patterns must match entirely within one
	   context distance, and only on the top level).  If the parse region
	   ends entirely before the modification or at or beyond modEnd, parse
	   the whole thing and take advantage of the safety region which will be
	   thrown away below.  Otherwise save the contents of the safety region
	   temporarily, and restore it after the parse. */
	if (beginSafety < modStart) {
		if (endSafety > modStart) {
			endPass2Safety = forwardOneContext(buf, contextRequirements, modStart);
			if (endPass2Safety + PASS_2_REPARSE_CHUNK_SIZE >= modEnd) {
				endPass2Safety = endSafety;
			}
		} else {
			endPass2Safety = endSafety;
		}
			
		prevChar = getPrevChar(buf, beginSafety);

		if (endPass2Safety == endSafety) {
			passTwoParseString(pass2Patterns, string, styleString, endParse - beginSafety, &prevChar, delimiters, string, match_to);
			goto parseDone;
		} else {
			passTwoParseString(pass2Patterns, string, styleString, modStart - beginSafety, &prevChar, delimiters, string, match_to);
		}
	}

	/* Re-parse the areas after the modification with pass 2 patterns, from
	   modEnd to endSafety, with an additional safety region before modEnd
	   to ensure that parsing at modEnd is correct. */
	if (endParse > modEnd) {
		if (beginSafety > modEnd) {
			prevChar = getPrevChar(buf, beginSafety);
			passTwoParseString(pass2Patterns, string, styleString, endParse - beginSafety, &prevChar, delimiters, string, match_to);
		} else {
			startPass2Safety = std::max<int>(beginSafety, backwardOneContext(buf, contextRequirements, modEnd));


			prevChar = getPrevChar(buf, startPass2Safety);
			passTwoParseString(pass2Patterns, &string[startPass2Safety - beginSafety], &styleString[startPass2Safety - beginSafety], endParse - startPass2Safety, &prevChar, delimiters, string, match_to);
		}
	}

parseDone:

	/* Update the style buffer with the new style information, but only
	   through endParse.  Skip the safety region at the end */
	styleString[endParse - beginSafety] = '\0';
	modifyStyleBuf(styleBuf, &styleString[beginParse - beginSafety], beginParse, endParse, firstPass2Style);

	return endParse;
}

/*
** Parses "string" according to compiled regular expressions in "pattern"
** until endRE is or errorRE are matched, or end of string is reached.
** Advances "string", "styleString" pointers to the next character past
** the end of the parsed section, and updates "prevChar" to reflect
** the new character before "string".
** If "anchored" is true, just scan the sub-pattern starting at the beginning
** of the string.  "length" is how much of the string must be parsed, but
** "string" must still be null terminated, the termination indicating how
** far the string should be searched, and "length" the part which is actually
** required (the string may or may not be parsed beyond "length").
**
** "lookBehindTo" indicates the boundary till where look-behind patterns may
** look back. If nullptr, the start of the string is assumed to be the boundary.
**
** "match_till" indicates the boundary till where matches may extend. If nullptr,
** it is assumed that the terminating \0 indicates the boundary. Note that
** look-ahead patterns can peek beyond the boundary, if supplied.
**
** Returns true if parsing was done and the parse succeeded.  Returns false if
** the error pattern matched, if the end of the string was reached without
** matching the end expression, or in the unlikely event of an internal error.
*/
bool parseString(HighlightData *pattern, const char **string, char **styleString, long length, char *prevChar, bool anchored, const QString &delimiters, const char *lookBehindTo, const char *match_till) {

	bool subExecuted;
	int *subExpr;
	char succChar = match_till ? (*match_till) : '\0';
	HighlightData *subPat = nullptr;
	HighlightData *subSubPat;

	if (length <= 0) {
		return false;
	}

	const char *stringPtr = *string;
	char *stylePtr        = *styleString;
	
    while (pattern->subPatternRE->ExecRE(
               stringPtr,
               anchored ? *string + 1 : *string + length + 1,
               false,
               *prevChar,
               succChar,
               delimiters.isNull() ? nullptr : delimiters.toLatin1().data(),
               lookBehindTo,
               match_till)) {
	
		/* Beware of the case where only one real branch exists, but that
		   branch has sub-branches itself. In that case the top_branch refers
		   to the matching sub-branch and must be ignored. */
		int subIndex = (pattern->nSubBranches > 1) ? pattern->subPatternRE->top_branch : 0;
		// Combination of all sub-patterns and end pattern matched 
		// printf("combined patterns RE matched at %d\n", pattern->subPatternRE->startp[0] - *string); 
		const char *startingStringPtr = stringPtr;

		/* Fill in the pattern style for the text that was skipped over before
		   the match, and advance the pointers to the start of the pattern */
		fillStyleString(&stringPtr, &stylePtr, pattern->subPatternRE->startp[0], pattern->style, prevChar);

		/* If the combined pattern matched this pattern's end pattern, we're
		   done.  Fill in the style string, update the pointers, color the
	       end expression if there were coloring sub-patterns, and return */
		const char *savedStartPtr = stringPtr;
		char savedPrevChar        = *prevChar;
		if (pattern->endRE) {
			if (subIndex == 0) {
				fillStyleString(&stringPtr, &stylePtr, pattern->subPatternRE->endp[0], pattern->style, prevChar);
				subExecuted = false;
				for (int i = 0; i < pattern->nSubPatterns; i++) {
					subPat = pattern->subPatterns[i];
					if (subPat->colorOnly) {
						if (!subExecuted) {
                            if (!pattern->endRE->ExecRE(
                                        savedStartPtr,
                                        savedStartPtr + 1,
                                        false,
                                        savedPrevChar,
                                        succChar,
                                        delimiters.isNull() ? nullptr : delimiters.toLatin1().data(),
                                        lookBehindTo,
                                        match_till)) {
                                qCritical("NEdit: Internal error, failed to recover end match in parseString");
								return false;
							}
							subExecuted = true;
						}

                        for (subExpr = subPat->endSubexprs; *subExpr != -1; subExpr++) {
							recolorSubexpr(pattern->endRE, *subExpr, subPat->style, *string, *styleString);
                        }
					}
				}
				*string = stringPtr;
				*styleString = stylePtr;
				return true;
			}
			--subIndex;
		}

		/* If the combined pattern matched this pattern's error pattern, we're
		   done.  Fill in the style string, update the pointers, and return */
		if (pattern->errorRE) {
			if (subIndex == 0) {
				fillStyleString(&stringPtr, &stylePtr, pattern->subPatternRE->startp[0], pattern->style, prevChar);
				*string      = stringPtr;
				*styleString = stylePtr;
				return false;
			}
			--subIndex;
		}

		int i;
		// Figure out which sub-pattern matched 
		for (i = 0; i < pattern->nSubPatterns; i++) {
			subPat = pattern->subPatterns[i];
			if (subPat->colorOnly) {
				++subIndex;
			} else if (i == subIndex) {
				break;
			}
		}
		
		if (i == pattern->nSubPatterns) {
            qCritical("NEdit: Internal error, failed to match in parseString");
			return false;
		}
		
		Q_ASSERT(subPat);

		// the sub-pattern is a simple match, just color it 
		if (!subPat->subPatternRE) {
			fillStyleString(&stringPtr, &stylePtr, pattern->subPatternRE->endp[0], /* subPat->startRE->endp[0],*/ subPat->style, prevChar);

			// Parse the remainder of the sub-pattern 
		} else if (subPat->endRE) {
			/* The pattern is a starting/ending type of pattern, proceed with
			   the regular hierarchical parsing. */

			/* If parsing should start after the start pattern, advance
			   to that point (this is currently always the case) */
            if (!(subPat->flags & PARSE_SUBPATS_FROM_START)) {
                fillStyleString(
                            &stringPtr,
                            &stylePtr,
                            pattern->subPatternRE->endp[0], // subPat->startRE->endp[0],
                            subPat->style,
                            prevChar);
            }

			// Parse to the end of the subPattern 
			parseString(
				subPat, 
				&stringPtr, 
				&stylePtr, 
				length - (stringPtr - *string), 
				prevChar, 
				false, 
				delimiters, 
				lookBehindTo, 
				match_till);
				
		} else {
			/* If the parent pattern is not a start/end pattern, the
			   sub-pattern can between the boundaries of the parent's
			   match. Note that we must limit the recursive matches such
			   that they do not exceed the parent's ending boundary.
			   Without that restriction, matching becomes unstable. */

			// Parse to the end of the subPattern 
			parseString(
				subPat, 
				&stringPtr, 
				&stylePtr, 
				pattern->subPatternRE->endp[0] - stringPtr, 
				prevChar, 
				false, 
				delimiters, 
				lookBehindTo, 
				pattern->subPatternRE->endp[0]);
		}

		/* If the sub-pattern has color-only sub-sub-patterns, add color
	       based on the coloring sub-expression references */
		subExecuted = false;
		for (i = 0; i < subPat->nSubPatterns; i++) {
			subSubPat = subPat->subPatterns[i];
			if (subSubPat->colorOnly) {
				if (!subExecuted) {
                    if (!subPat->startRE->ExecRE(
                                savedStartPtr,
                                savedStartPtr + 1,
                                false,
                                savedPrevChar,
                                succChar,
                                delimiters.isNull() ? nullptr : delimiters.toLatin1().data(),
                                lookBehindTo,
                                match_till)) {
                        qCritical("NEdit: Internal error, failed to recover start match in parseString");
						return false;
					}
					subExecuted = true;
				}
				for (subExpr = subSubPat->startSubexprs; *subExpr != -1; subExpr++)
					recolorSubexpr(subPat->startRE, *subExpr, subSubPat->style, *string, *styleString);
			}
		}

		/* Make sure parsing progresses.  If patterns match the empty string,
		   they can get stuck and hang the process */
		if (stringPtr == startingStringPtr) {
			/* Avoid stepping over the end of the string (possible for 
			   zero-length matches at end of the string) */
			if (*stringPtr == '\0') {
				break;
			}
			
			fillStyleString(&stringPtr, &stylePtr, stringPtr + 1, pattern->style, prevChar);
		}
	}

	/* If this is an anchored match (must match on first character), and
	   nothing matched, return false */
	if (anchored && stringPtr == *string) {
		return false;
	}

	/* Reached end of string, fill in the remaining text with pattern style
	   (unless this was an anchored match) */
	if (!anchored) {
		fillStyleString(&stringPtr, &stylePtr, *string + length, pattern->style, prevChar);
	}

	// Advance the string and style pointers to the end of the parsed text 
	*string      = stringPtr;
	*styleString = stylePtr;
	return pattern->endRE == nullptr;
}

/*
** Takes a string which has already been parsed through pass1 parsing and
** re-parses the areas where pass two patterns are applicable.  Parameters
** have the same meaning as in parseString, except that strings aren't doubly
** indirect and string pointers are not updated.
*/
static void passTwoParseString(HighlightData *pattern, const char *string, char *styleString, int length, char *prevChar, const QString &delimiters, const char *lookBehindTo, const char *match_till) {

	bool inParseRegion = false;
	char *stylePtr;
	const char *parseStart = nullptr;
	const char *parseEnd;
	char *s = styleString;
	const char *c = string;
	const char *stringPtr;
    int firstPass2Style = static_cast<uint8_t>(pattern[1].style);

	for (;; c++, s++) {
        if (!inParseRegion && c != match_till && (*s == UNFINISHED_STYLE || *s == PLAIN_STYLE || static_cast<uint8_t>(*s) >= firstPass2Style)) {
			parseStart = c;
			inParseRegion = true;
		}

        if (inParseRegion && (c == match_till || !(*s == UNFINISHED_STYLE || *s == PLAIN_STYLE || static_cast<uint8_t>(*s) >= firstPass2Style))) {
			parseEnd = c;
			if (parseStart != string)
				*prevChar = *(parseStart - 1);
			stringPtr = parseStart;
			stylePtr = &styleString[parseStart - string];

            match_till = parseEnd;

			parseString(
				pattern, 
				&stringPtr, 
				&stylePtr, 
                std::min(parseEnd - parseStart,
				length - (parseStart - string)), 
				prevChar, 
				false, 
                delimiters,
				lookBehindTo, 
				match_till);
				
			inParseRegion = false;
		}

        if (c == match_till || (!inParseRegion && c - string >= length)) {
			break;
        }
	}
}

/*
** Advance "stringPtr" and "stylePtr" until "stringPtr" == "toPtr", filling
** "stylePtr" with style "style".  Can also optionally update the pre-string
** character, prevChar, which is fed to regular the expression matching
** routines for determining word and line boundaries at the start of the string.
*/
static void fillStyleString(const char **stringPtr, char **stylePtr, const char *toPtr, char style, char *prevChar) {

    const long len = toPtr - *stringPtr;

    if (*stringPtr >= toPtr) {
		return;
    }

    for (long i = 0; i < len; i++) {
		*(*stylePtr)++ = style;
    }

    if(prevChar) {
        *prevChar = *(toPtr - 1);
    }

	*stringPtr = toPtr;
}

/*
** Incorporate changes from styleString into styleBuf, tracking changes
** in need of redisplay, and marking them for redisplay by the text
** modification callback in TextDisplay.c.  "firstPass2Style" is necessary
** for distinguishing pass 2 styles which compare as equal to the unfinished
** style in the original buffer, from pass1 styles which signal a change.
*/
static void modifyStyleBuf(const std::shared_ptr<TextBuffer> &styleBuf, char *styleString, int startPos, int endPos, int firstPass2Style) {
    char *c;
    int pos;
    int modStart;
    int modEnd;
    int minPos = INT_MAX;
    int maxPos = 0;
	TextSelection *sel = &styleBuf->primary_;

	// Skip the range already marked for redraw 
	if (sel->selected) {
		modStart = sel->start;
		modEnd = sel->end;
    } else {
		modStart = modEnd = startPos;
    }

	/* Compare the original style buffer (outside of the modified range) with
	   the new string with which it will be updated, to find the extent of
	   the modifications.  Unfinished styles in the original match any
	   pass 2 style */
	for (c = styleString, pos = startPos; pos < modStart && pos < endPos; c++, pos++) {
        char bufChar = styleBuf->BufGetCharacter(pos);
        if (*c != bufChar && !(bufChar == UNFINISHED_STYLE && (*c == PLAIN_STYLE || static_cast<uint8_t>(*c) >= firstPass2Style))) {
            minPos = std::min(minPos, pos);
            maxPos = std::max(maxPos, pos);
		}
    }

    for (c = &styleString[std::max(0, modEnd - startPos)], pos = std::max(modEnd, startPos); pos < endPos; c++, pos++) {
        char bufChar = styleBuf->BufGetCharacter(pos);
        if (*c != bufChar && !(bufChar == UNFINISHED_STYLE && (*c == PLAIN_STYLE || static_cast<uint8_t>(*c) >= firstPass2Style))) {

            minPos = std::min(minPos, pos);
            maxPos = std::max(maxPos, pos + 1);
		}
	}

	// Make the modification 
	styleBuf->BufReplaceEx(startPos, endPos, styleString);

	/* Mark or extend the range that needs to be redrawn.  Even if no
	   change was made, it's important to re-establish the selection,
	   because it can get damaged by the BufReplaceEx above */
	styleBuf->BufSelect(std::min(modStart, minPos), std::max(modEnd, maxPos));
}

/*
** Return the last modified position in styleBuf (as marked by modifyStyleBuf
** by the convention used for conveying modification information to the
** text widget, which is selecting the text)
*/
static int lastModified(const std::shared_ptr<TextBuffer> &styleBuf) {
	if (styleBuf->primary_.selected)
		return std::max(0, styleBuf->primary_.end);
	return 0;
}

/*
** Allocate a read-only (shareable) colormap cell for a named color, from the
** the default colormap of the screen on which the widget (w) is displayed. If
** the colormap is full and there's no suitable substitute, print an error on
** stderr, and return the widget's foreground color as a backup.
*/
QColor AllocColor(const QString &colorName) {
     return X11Colors::fromString(colorName);
}

/*
** Get the character before position "pos" in buffer "buf"
*/
char getPrevChar(TextBuffer *buf, int pos) {
	return pos == 0 ? '\0' : buf->BufGetCharacter(pos - 1);
}

static int parentStyleOf(const QByteArray &parentStyles, int style) {
    return parentStyles[static_cast<uint8_t>(style) - UNFINISHED_STYLE];
}

static bool isParentStyle(const QByteArray &parentStyles, int style1, int style2) {

	for (int p = parentStyleOf(parentStyles, style2); p != 0; p = parentStyleOf(parentStyles, p)) {
		if (style1 == p) {
			return true;
		}
	}
	
	return false;
}

/*
** Discriminates patterns which can be used with parseString from those which
** can't.  Leaf patterns are not suitable for parsing, because patterns
** contain the expressions used for parsing within the context of their own
** operation, i.e. the parent pattern initiates, and leaf patterns merely
** confirm and color.  Returns true if the pattern is suitable for parsing.
*/
static int patternIsParsable(HighlightData *pattern) {
	return pattern != nullptr && pattern->subPatternRE != nullptr;
}

/*
** Back up position pointed to by "pos" enough that parsing from that point
** on will satisfy context gurantees for pattern matching for modifications
** at pos.  Returns the style with which to begin parsing.  The caller is
** guranteed that parsing may safely BEGIN with that style, but not that it
** will continue at that level.
**
** This routine can be fooled if a continuous style run of more than one
** context distance in length is produced by multiple pattern matches which
** abut, rather than by a single continuous match.  In this  case the
** position returned by this routine may be a bad starting point which will
** result in an incorrect re-parse.  However this will happen very rarely,
** and, if it does, is unlikely to result in incorrect highlighting.
*/
static int findSafeParseRestartPos(TextBuffer *buf, const std::unique_ptr<WindowHighlightData> &highlightData, int *pos) {
	
	int checkBackTo;
	int safeParseStart;

	QByteArray parentStyles      = highlightData->parentStyles;
	HighlightData *pass1Patterns = highlightData->pass1Patterns;
	ReparseContext *context      = &highlightData->contextRequirements;

	// We must begin at least one context distance back from the change 
	*pos = backwardOneContext(buf, context, *pos);

	/* If the new position is outside of any styles or at the beginning of
	   the buffer, this is a safe place to begin parsing, and we're done */
	if (*pos == 0) {
		return PLAIN_STYLE;
	}

	int startStyle = highlightData->styleBuffer->BufGetCharacter(*pos);

	if (is_plain(startStyle)) {
		return PLAIN_STYLE;
	}

	/*
	** The new position is inside of a styled region, meaning, its pattern
	** could potentially be affected by the modification.
	**
	** Follow the style back by enough context to ensure that if we don't find
	** its beginning, at least we've found a safe place to begin parsing
	** within the styled region.
	**
	** A safe starting position within a style is either at a style
	** boundary, or far enough from the beginning and end of the style run
	** to ensure that it's not within the start or end expression match
	** (unfortunately, abutting styles can produce false runs so we're not
	** really ensuring it, just making it likely).
	*/
	if (patternIsParsable(patternOfStyle(pass1Patterns, startStyle))) {
		safeParseStart = backwardOneContext(buf, context, *pos);
        checkBackTo    = backwardOneContext(buf, context, safeParseStart);
	} else {
		safeParseStart = 0;
        checkBackTo    = 0;
	}

	int runningStyle = startStyle;
	for (int i = *pos - 1;; i--) {

		// The start of the buffer is certainly a safe place to parse from 
		if (i == 0) {
			*pos = 0;
			return PLAIN_STYLE;
		}

        /* If the style is preceded by a parent style, it's safe to parse
         * with the parent style, provided that the parent is parsable. */
		int style = highlightData->styleBuffer->BufGetCharacter(i);
		if (isParentStyle(parentStyles, style, runningStyle)) {
			if (patternIsParsable(patternOfStyle(pass1Patterns, style))) {
				*pos = i + 1;
				return style;
			} else {
				/* The parent is not parsable, so well have to continue
				   searching. The parent now becomes the running style. */
				runningStyle = style;
			}
		}

		/* If the style is preceded by a child style, it's safe to resume
		   parsing with the running style, provided that the running
		   style is parsable. */
		else if (isParentStyle(parentStyles, runningStyle, style)) {
			if (patternIsParsable(patternOfStyle(pass1Patterns, runningStyle))) {
				*pos = i + 1;
				return runningStyle;
			}
			/* Else: keep searching; it's no use switching to the child style
			   because even the running one is not parsable. */
		}

		/* If the style is preceded by a sibling style, it's safe to resume
		   parsing with the common ancestor style, provided that the ancestor
		   is parsable. Checking for siblings is very hard; checking whether
		   the style has the same parent will probably catch 99% of the cases
		   in practice. */
		else if (runningStyle != style && isParentStyle(parentStyles, parentStyleOf(parentStyles, runningStyle), style)) {
			int parentStyle = parentStyleOf(parentStyles, runningStyle);
			if (patternIsParsable(patternOfStyle(pass1Patterns, parentStyle))) {
				*pos = i + 1;
				return parentStyle;
			} else {
				// Switch to the new style 
				runningStyle = style;
			}
		}

		/* If the style is preceded by an unrelated style, it's safe to
		   resume parsing with PLAIN_STYLE. (Actually, it isn't, because
		   we didn't really check for all possible sibling relations; but
		   it will be ok in practice.) */
		else if (runningStyle != style) {
			*pos = i + 1;
			return PLAIN_STYLE;
		}

		/* If the style is parsable and didn't change for one whole context
		   distance on either side of safeParseStart, safeParseStart is a
		   reasonable guess at a place to start parsing.
		   Note: No 'else' here! We may come from one of the 'fall-through
		   cases' above. */
		if (i == checkBackTo) {
			*pos = safeParseStart;

			/* We should never return a non-parsable style, because it will
			   result in an internal error. If the current style is not
			   parsable, the pattern set most probably contains a context
			   distance violation. In that case we can only avoid internal
			   errors (by climbing the pattern hierarchy till we find a
			   parsable ancestor) and hope that the highlighting errors are
			   minor. */
            while (!patternIsParsable(patternOfStyle(pass1Patterns, runningStyle))) {
				runningStyle = parentStyleOf(parentStyles, runningStyle);
            }

			return runningStyle;
		}
	}
}

/*
** Return a position far enough back in "buf" from "fromPos" to give patterns
** their guranteed amount of context for matching (from "context").  If
** backing up by lines yields the greater distance, the returned position will
** be to the newline character before the start of the line, rather than to
** the first character of the line.  (I did this because earlier prototypes of
** the syntax highlighting code, which were based on single-line context, used
** this to ensure that line-spanning expressions would be detected.  I think
** it may reduce some 2 line context requirements to one line, at a cost of
** only one extra character, but I'm not sure, and my brain hurts from
** thinking about it).
*/
int backwardOneContext(TextBuffer *buf, ReparseContext *context, int fromPos) {
	if (context->nLines == 0)
        return std::max(0, fromPos - context->nChars);
	else if (context->nChars == 0)
        return std::max(0, buf->BufCountBackwardNLines(fromPos, context->nLines - 1) - 1);
	else
        return std::max(0, std::min(std::max(0, buf->BufCountBackwardNLines(fromPos, context->nLines - 1) - 1), fromPos - context->nChars));
}

/*
** Return a position far enough forward in "buf" from "fromPos" to ensure
** that patterns are given their required amount of context for matching
** (from "context").  If moving forward by lines yields the greater
** distance, the returned position will be the first character of of the
** next line, rather than the newline character at the end (see notes in
** backwardOneContext).
*/
int forwardOneContext(TextBuffer *buf, ReparseContext *context, int fromPos) {
	if (context->nLines == 0)
        return std::min(buf->BufGetLength(), fromPos + context->nChars);
	else if (context->nChars == 0)
        return std::min(buf->BufGetLength(), buf->BufCountForwardNLines(fromPos, context->nLines));
	else
        return std::min(buf->BufGetLength(), std::max(buf->BufCountForwardNLines(fromPos, context->nLines), fromPos + context->nChars));
}

/*
** Change styles in the portion of "styleString" to "style" where a particular
** sub-expression, "subExpr", of regular expression "re" applies to the
** corresponding portion of "string".
*/
static void recolorSubexpr(const std::shared_ptr<regexp> &re, int subexpr, int style, const char *string, char *styleString) {

	const char *stringPtr = re->startp[subexpr];
	char *stylePtr        = &styleString[stringPtr - string];
	fillStyleString(&stringPtr, &stylePtr, re->endp[subexpr], style, nullptr);
}

/*
** Search for a pattern in pattern list "patterns" with style "style"
*/
HighlightData *patternOfStyle(HighlightData *patterns, int style) {

	for (int i = 0; patterns[i].style != 0; i++)
		if (patterns[i].style == style)
			return &patterns[i];

	if (style == PLAIN_STYLE || style == UNFINISHED_STYLE)
		return &patterns[0];

	return nullptr;
}

int indexOfNamedPattern(HighlightPattern *patList, int nPats, const QString &patName) {

    if(patName.isNull()) {
		return -1;
	}
	
	for (int i = 0; i < nPats; i++) {
        if (patList[i].name == patName) {
			return i;
		}
	}
	
	return -1;
}

int findTopLevelParentIndex(HighlightPattern *patList, int nPats, int index) {

	int topIndex = index;
	while (!patList[topIndex].subPatternOf.isNull()) {
        topIndex = indexOfNamedPattern(patList, nPats, patList[topIndex].subPatternOf);
		if (index == topIndex)
			return -1; // amai: circular dependency ?! 
	}
	return topIndex;
}
