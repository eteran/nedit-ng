
#include "Highlight.h"
#include "DocumentWidget.h"
#include "HighlightData.h"
#include "HighlightPattern.h"
#include "HighlightStyle.h"
#include "PatternSet.h"
#include "Preferences.h"
#include "Regex.h"
#include "ReparseContext.h"
#include "Settings.h"
#include "StyleTableEntry.h"
#include "TextBuffer.h"
#include "Util/Input.h"
#include "Util/Resource.h"
#include "Util/algorithm.h"
#include "WindowHighlightData.h"
#include "X11Colors.h"

#include <yaml-cpp/yaml.h>

#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QtDebug>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

// list of available highlight styles
namespace Highlight {

std::vector<HighlightStyle> HighlightStyles;

// Pattern sources loaded from the .nedit file or set by the user
std::vector<PatternSet> PatternSets;

namespace {

/* Initial forward expansion of parsing region in incremental re-parsing,
   when style changes propagate forward beyond the original modification.
   This distance is increased by a factor of two for each subsequent step. */
constexpr int REPARSE_CHUNK_SIZE = 80;

constexpr auto STYLE_NOT_FOUND = static_cast<size_t>(-1);

constexpr bool isPlain(uint8_t style) {
	return (style == PLAIN_STYLE || style == UNFINISHED_STYLE);
}

/* Compare two styles where one of the styles may not yet have been processed
   with pass2 patterns */
constexpr bool equivalentStyle(uint8_t style1, uint8_t style2, uint8_t firstPass2Style) {
	return (style1 == style2) ||
		   (style1 == UNFINISHED_STYLE && (style2 == PLAIN_STYLE || style2 >= firstPass2Style)) ||
		   (style2 == UNFINISHED_STYLE && (style1 == PLAIN_STYLE || style1 >= firstPass2Style));
}

/* Scanning context can be reduced (with big efficiency gains) if we
   know that patterns can't cross line boundaries, which is implied
   by a context requirement of 1 line and 0 characters */
bool canCrossLineBoundaries(const ReparseContext &contextRequirements) {
	return (contextRequirements.nLines != 1 || contextRequirements.nChars != 0);
}

uint8_t parentStyleOf(const std::vector<uint8_t> &parentStyles, uint8_t style) {
	Q_ASSERT(style != 0);
	return parentStyles[style - UNFINISHED_STYLE];
}

bool isParentStyle(const std::vector<uint8_t> &parentStyles, uint8_t style1, uint8_t style2) {

	for (uint8_t p = parentStyleOf(parentStyles, style2); p != 0; p = parentStyleOf(parentStyles, p)) {
		if (style1 == p) {
			return true;
		}
	}

	return false;
}

/*
** Return the last modified position in buffer (as marked by modifyStyleBuf
** by the convention used for conveying modification information to the
** text widget, which is selecting the text)
*/
template <class Ptr>
TextCursor lastModified(const Ptr &buffer) {
	if (buffer->primary.hasSelection()) {
		return std::max(buffer->BufStartOfBuffer(), buffer->primary.end());
	}

	return buffer->BufStartOfBuffer();
}

/*
** Return true if patSet exactly matches one of the default pattern sets
*/
bool isDefaultPatternSet(const PatternSet &patternSet) {

	boost::optional<PatternSet> defaultPatSet = readDefaultPatternSet(patternSet.languageMode);
	if (!defaultPatSet) {
		return false;
	}

	return patternSet == *defaultPatSet;
}

/*
** Discriminates patterns which can be used with parseString from those which
** can't.  Leaf patterns are not suitable for parsing, because patterns
** contain the expressions used for parsing within the context of their own
** operation, i.e. the parent pattern initiates, and leaf patterns merely
** confirm and color.  Returns true if the pattern is suitable for parsing.
*/
bool patternIsParsable(HighlightData *pattern) {
	return pattern && pattern->subPatternRE;
}

/*
** Advance "string_ptr" and "style_ptr" until "string_ptr" == "to_ptr", filling
** "style_ptr" with style "style".  Can also optionally update the pre-string
** character, prev_char, which is fed to regular the expression matching
** routines for determining word and line boundaries at the start of the string.
*/
void fillStyleString(const char *string_ptr, char *style_ptr, const char *to_ptr, uint8_t style) {

	const ptrdiff_t len = to_ptr - string_ptr;

	if (string_ptr >= to_ptr) {
		return;
	}

	for (ptrdiff_t i = 0; i < len; i++) {
		*style_ptr++ = static_cast<char>(style);
	}
}

void fillStyleString(const char *&string_ptr, char *&style_ptr, const char *to_ptr, uint8_t style, const ParseContext *ctx) {

	const ptrdiff_t len = to_ptr - string_ptr;

	if (string_ptr >= to_ptr) {
		return;
	}

	for (ptrdiff_t i = 0; i < len; i++) {
		*style_ptr++ = static_cast<char>(style);
	}

	if (ctx->prev_char) {
		*ctx->prev_char = static_cast<unsigned char>(*(to_ptr - 1));
	}

	string_ptr = to_ptr;
}

/*
** Change styles in the portion of "styleString" to "style" where a particular
** sub-expression, "subExpr", of regular expression "re" applies to the
** corresponding portion of "string".
*/
void recolorSubexpression(const std::unique_ptr<Regex> &re, size_t subexpr, uint8_t style, const char *string_base, char *style_base) {

	const char *string_ptr = re->startp[subexpr];
	const char *to_ptr     = re->endp[subexpr];
	char *style_ptr        = &style_base[string_ptr - string_base];

	fillStyleString(string_ptr, style_ptr, to_ptr, style);
}

/*
** Takes a string which has already been parsed through pass1 parsing and
** re-parses the areas where pass two patterns are applicable.  Parameters
** have the same meaning as in parseString, except that string pointers are
** not updated.
*/
void passTwoParseString(const HighlightData *pattern, const char *string, char *styleString, int64_t length, const ParseContext *ctx, const char *lookBehindTo, const char *match_to) {

	bool inParseRegion     = false;
	const char *parseStart = nullptr;
	const char *parseEnd;
	char *s                 = styleString;
	const char *c           = string;
	uint8_t firstPass2Style = pattern[1].style;

	for (;; c++, s++) {
		if (!inParseRegion && c != match_to && (*s == UNFINISHED_STYLE || *s == PLAIN_STYLE || static_cast<uint8_t>(*s) >= firstPass2Style)) {
			parseStart    = c;
			inParseRegion = true;
		}

		if (inParseRegion && (c == match_to || !(*s == UNFINISHED_STYLE || *s == PLAIN_STYLE || static_cast<uint8_t>(*s) >= firstPass2Style))) {
			parseEnd = c;
			if (parseStart != string) {
				*ctx->prev_char = static_cast<unsigned char>(*(parseStart - 1));
			}

			const char *stringPtr = parseStart;
			char *stylePtr        = &styleString[parseStart - string];

			match_to = parseEnd;
			length   = std::min<int64_t>(parseEnd - parseStart, length - (parseStart - string));

			parseString(
				pattern,
				stringPtr,
				stylePtr,
				length,
				ctx,
				lookBehindTo,
				match_to);

			inParseRegion = false;
		}

		if (c == match_to || (!inParseRegion && c - string >= length)) {
			break;
		}
	}
}

/*
** Incorporate changes from styleString into styleBuf, tracking changes
** in need of redisplay, and marking them for redisplay by the text
** modification callback in TextDisplay.c.  "firstPass2Style" is necessary
** for distinguishing pass 2 styles which compare as equal to the unfinished
** style in the original buffer, from pass1 styles which signal a change.
*/
void modifyStyleBuf(const std::shared_ptr<TextBuffer> &styleBuf, char *styleString, TextCursor startPos, TextCursor endPos, uint8_t firstPass2Style) {
	char *ch;
	TextCursor pos;
	TextCursor modStart;
	TextCursor modEnd;
	auto minPos                      = TextCursor(INT_MAX);
	auto maxPos                      = TextCursor();
	const TextBuffer::Selection *sel = &styleBuf->primary;

	// Skip the range already marked for redraw
	if (sel->hasSelection()) {
		modStart = sel->start();
		modEnd   = sel->end();
	} else {
		modStart = modEnd = startPos;
	}

	/* Compare the original style buffer (outside of the modified range) with
	   the new string with which it will be updated, to find the extent of
	   the modifications.  Unfinished styles in the original match any
	   pass 2 style */
	for (ch = styleString, pos = startPos; pos < modStart && pos < endPos; ++ch, ++pos) {
		char bufChar = styleBuf->BufGetCharacter(pos);
		if (*ch != bufChar && !(bufChar == UNFINISHED_STYLE && (*ch == PLAIN_STYLE || static_cast<uint8_t>(*ch) >= firstPass2Style))) {

			minPos = std::min(minPos, pos);
			maxPos = std::max(maxPos, pos);
		}
	}

	for (ch = &styleString[std::max(0, modEnd - startPos)], pos = std::max(modEnd, startPos); pos < endPos; ++ch, ++pos) {
		char bufChar = styleBuf->BufGetCharacter(pos);
		if (*ch != bufChar && !(bufChar == UNFINISHED_STYLE && (*ch == PLAIN_STYLE || static_cast<uint8_t>(*ch) >= firstPass2Style))) {

			minPos = std::min(minPos, pos);
			maxPos = std::max(maxPos, pos + 1);
		}
	}

	// Make the modification
	styleBuf->BufReplace(startPos, endPos, styleString);

	/* Mark or extend the range that needs to be redrawn.  Even if no
	   change was made, it's important to re-establish the selection,
	   because it can get damaged by the BufReplaceEx above */
	styleBuf->BufSelect(std::min(modStart, minPos), std::max(modEnd, maxPos));
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
** safety region beyond endparse so that endParse is guaranteed to be parsed
** correctly in both passes.  Returns the buffer position at which parsing
** finished (this will normally be endParse, unless the pass1Patterns is a
** pattern which does end and the end is reached).
*/
TextCursor parseBufferRange(const HighlightData *pass1Patterns, const std::unique_ptr<HighlightData[]> &pass2Patterns, TextBuffer *buf, const std::shared_ptr<TextBuffer> &styleBuf, const ReparseContext &contextRequirements, TextCursor beginParse, TextCursor endParse) {

	TextCursor endSafety;
	TextCursor endPass2Safety;
	TextCursor startPass2Safety;
	TextCursor modStart;
	TextCursor modEnd;
	TextCursor beginSafety;
	TextCursor p;
	uint8_t style;
	uint8_t firstPass2Style = (!pass2Patterns) ? UINT8_MAX : pass2Patterns[1].style;

	// Begin parsing one context distance back (or to the last style change)
	uint8_t beginStyle = pass1Patterns->style;
	if (canCrossLineBoundaries(contextRequirements)) {
		beginSafety = backwardOneContext(buf, contextRequirements, beginParse);
		for (p = beginParse; p >= beginSafety; --p) {
			style = styleBuf->BufGetCharacter(p - 1);
			if (!equivalentStyle(style, beginStyle, firstPass2Style)) {
				beginSafety = p;
				break;
			}
		}
	} else {
		for (beginSafety = std::max(TextCursor(), beginParse - 1); beginSafety > 0; --beginSafety) {
			style = styleBuf->BufGetCharacter(beginSafety);
			if (!equivalentStyle(style, beginStyle, firstPass2Style) || buf->BufGetCharacter(beginSafety) == '\n') {
				++beginSafety;
				break;
			}
		}
	}

	/* Parse one parse context beyond requested end to guarantee that parsing
	   at endParse is complete, unless patterns can't cross line boundaries,
	   in which case the end of the line is fine */
	if (endParse == 0) {
		return TextCursor();
	}

	if (canCrossLineBoundaries(contextRequirements)) {
		endSafety = forwardOneContext(buf, contextRequirements, endParse);
	} else if (endParse >= buf->length() || (buf->BufGetCharacter(endParse - 1) == '\n')) {
		endSafety = endParse;
	} else {
		endSafety = std::min(buf->BufEndOfBuffer(), buf->BufEndOfLine(endParse) + 1);
	}

	// copy the buffer range into a string

	std::string str      = buf->BufGetRange(beginSafety, endSafety);
	std::string styleStr = styleBuf->BufGetRange(beginSafety, endSafety);

	const char *const string   = str.data();
	char *const styleString    = &styleStr[0];
	const char *const match_to = string + str.size();

	// Parse it with pass 1 patterns
	int prev_char = getPrevChar(buf, beginParse);
	ParseContext ctx;
	ctx.prev_char         = &prev_char;
	ctx.text              = str;
	const char *stringPtr = &string[beginParse - beginSafety];
	char *stylePtr        = &styleString[beginParse - beginSafety];

	parseString(
		&pass1Patterns[0],
		stringPtr,
		stylePtr,
		endParse - beginParse,
		&ctx,
		nullptr,
		nullptr);

	// On non top-level patterns, parsing can end early
	endParse = std::min(endParse, stringPtr - string + beginSafety);

	// If there are no pass 2 patterns, we're done
	if (!pass2Patterns) {
		/* Update the style buffer with the new style information, but only
		 * through endParse.  Skip the safety region at the end */
		styleString[endParse - beginSafety] = '\0';
		modifyStyleBuf(styleBuf, &styleString[beginParse - beginSafety], beginParse, endParse, firstPass2Style);
		return endParse;
	}

	/* Parsing of pass 2 patterns is done only as necessary for determining
	   where styles have changed.  Find the area to avoid, which is already
	   marked as changed (all inserted text and previously modified areas) */
	if (styleBuf->primary.hasSelection()) {
		modStart = styleBuf->primary.start();
		modEnd   = styleBuf->primary.end();
	} else {
		modStart = styleBuf->BufStartOfBuffer();
		modEnd   = styleBuf->BufStartOfBuffer();
	}

	/* Re-parse the areas before the modification with pass 2 patterns, from
	   beginSafety to far enough beyond modStart to guarantee that parsing at
	   modStart is correct (pass 2 patterns must match entirely within one
	   context distance, and only on the top level).  If the parse region
	   ends entirely before the modification or at or beyond modEnd, parse
	   the whole thing and take advantage of the safety region which will be
	   thrown away below.  Otherwise save the contents of the safety region
	   temporarily, and restore it after the parse. */
	int safe_prev_char = '\0';
	if (beginSafety < modStart) {
		if (endSafety > modStart) {
			endPass2Safety = forwardOneContext(buf, contextRequirements, modStart);
			if (endPass2Safety + PASS_2_REPARSE_CHUNK_SIZE >= modEnd) {
				endPass2Safety = endSafety;
			}
		} else {
			endPass2Safety = endSafety;
		}

		safe_prev_char = getPrevChar(buf, beginSafety);
		ctx.prev_char  = &safe_prev_char;

		if (endPass2Safety == endSafety) {
			passTwoParseString(
				&pass2Patterns[0],
				string,
				styleString,
				endParse - beginSafety,
				&ctx,
				string,
				match_to);

			/* Update the style buffer with the new style information, but only
			 * through endParse.  Skip the safety region at the end */
			styleString[endParse - beginSafety] = '\0';
			modifyStyleBuf(styleBuf, &styleString[beginParse - beginSafety], beginParse, endParse, firstPass2Style);
			return endParse;
		}

		passTwoParseString(
			&pass2Patterns[0],
			string,
			styleString,
			modStart - beginSafety,
			&ctx,
			string,
			match_to);
	}

	/* Re-parse the areas after the modification with pass 2 patterns, from
	   modEnd to endSafety, with an additional safety region before modEnd
	   to ensure that parsing at modEnd is correct. */
	if (endParse > modEnd) {
		if (beginSafety > modEnd) {
			prev_char = getPrevChar(buf, beginSafety);
			passTwoParseString(
				&pass2Patterns[0],
				string,
				styleString,
				endParse - beginSafety,
				&ctx,
				string,
				match_to);
		} else {
			startPass2Safety = std::max(beginSafety, backwardOneContext(buf, contextRequirements, modEnd));

			prev_char = getPrevChar(buf, startPass2Safety);
			passTwoParseString(
				&pass2Patterns[0],
				&string[startPass2Safety - beginSafety],
				&styleString[startPass2Safety - beginSafety],
				endParse - startPass2Safety,
				&ctx,
				string,
				match_to);
		}
	}

	/* Update the style buffer with the new style information, but only
	   through endParse.  Skip the safety region at the end */
	styleString[endParse - beginSafety] = '\0';
	modifyStyleBuf(styleBuf, &styleString[beginParse - beginSafety], beginParse, endParse, firstPass2Style);
	return endParse;
}

/*
** Back up position pointed to by "pos" enough that parsing from that point
** on will satisfy context guarantees for pattern matching for modifications
** at pos.  Returns the style with which to begin parsing.  The caller is
** guaranteed that parsing may safely BEGIN with that style, but not that it
** will continue at that level.
**
** This routine can be fooled if a continuous style run of more than one
** context distance in length is produced by multiple pattern matches which
** abut, rather than by a single continuous match.  In this  case the
** position returned by this routine may be a bad starting point which will
** result in an incorrect re-parse.  However this will happen very rarely,
** and, if it does, is unlikely to result in incorrect highlighting.
*/
uint8_t findSafeParseRestartPos(TextBuffer *buf, const std::unique_ptr<WindowHighlightData> &highlightData, TextCursor *pos) {

	TextCursor checkBackTo;
	TextCursor safeParseStart;

	std::vector<uint8_t> &parentStyles                    = highlightData->parentStyles;
	const std::unique_ptr<HighlightData[]> &pass1Patterns = highlightData->pass1Patterns;
	const ReparseContext &context                         = highlightData->contextRequirements;

	// We must begin at least one context distance back from the change
	*pos = backwardOneContext(buf, context, *pos);

	/* If the new position is outside of any styles or at the beginning of
	   the buffer, this is a safe place to begin parsing, and we're done */
	if (*pos == 0) {
		return PLAIN_STYLE;
	}

	auto startStyle = static_cast<uint8_t>(highlightData->styleBuffer->BufGetCharacter(*pos));

	if (isPlain(startStyle)) {
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
	const TextCursor begin = buf->BufStartOfBuffer();

	if (patternIsParsable(patternOfStyle(pass1Patterns, startStyle))) {
		safeParseStart = backwardOneContext(buf, context, *pos);
		checkBackTo    = backwardOneContext(buf, context, safeParseStart);
	} else {
		safeParseStart = begin;
		checkBackTo    = begin;
	}

	uint8_t runningStyle = startStyle;
	for (TextCursor i = *pos - 1;; --i) {

		// The start of the buffer is certainly a safe place to parse from
		if (i == 0) {
			*pos = begin;
			return PLAIN_STYLE;
		}

		/* If the style is preceded by a parent style, it's safe to parse
		 * with the parent style, provided that the parent is parsable. */
		auto style = static_cast<uint8_t>(highlightData->styleBuffer->BufGetCharacter(i));
		if (isParentStyle(parentStyles, style, runningStyle)) {
			if (patternIsParsable(patternOfStyle(pass1Patterns, style))) {
				*pos = i + 1;
				return style;
			}
			/* The parent is not parsable, so well have to continue
			   searching. The parent now becomes the running style. */
			runningStyle = style;

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
			uint8_t parentStyle = parentStyleOf(parentStyles, runningStyle);
			if (patternIsParsable(patternOfStyle(pass1Patterns, parentStyle))) {
				*pos = i + 1;
				return parentStyle;
			}
			// Switch to the new style
			runningStyle = style;

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
** Re-parse the smallest region possible around a modification to buffer "buf"
** to guarantee that the promised context lines and characters have
** been presented to the patterns.  Changes the style buffer in "highlightData"
** with the parsing result.
*/
void incrementalReparse(const std::unique_ptr<WindowHighlightData> &highlightData, TextBuffer *buf, TextCursor pos, int64_t nInserted) {

	const std::shared_ptr<TextBuffer> &styleBuf           = highlightData->styleBuffer;
	const std::unique_ptr<HighlightData[]> &pass1Patterns = highlightData->pass1Patterns;
	const std::unique_ptr<HighlightData[]> &pass2Patterns = highlightData->pass2Patterns;
	const ReparseContext &context                         = highlightData->contextRequirements;
	const std::vector<uint8_t> &parentStyles              = highlightData->parentStyles;

	/* Find the position "beginParse" at which to begin re-parsing.  This is
	   far enough back in the buffer such that the guaranteed number of
	   lines and characters of context are examined. */
	TextCursor beginParse = pos;
	uint8_t parseInStyle  = findSafeParseRestartPos(buf, highlightData, &beginParse);

	/* Find the position "endParse" at which point it is safe to stop
	   parsing, unless styles are getting changed beyond the last
	   modification */
	TextCursor lastMod  = pos + nInserted;
	TextCursor endParse = forwardOneContext(buf, context, lastMod);

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
		const HighlightData *startPattern = patternOfStyle(pass1Patterns, parseInStyle);
		/* If there is no pattern matching the style, it must be a pass-2
		   style. It that case, it is (probably) safe to start parsing with
		   the root pass-1 pattern again. Anyway, passing a nullptr-pointer to
		   the parse routine would result in a crash; restarting with pass-1
		   patterns is certainly preferable, even if there is a slight chance
		   of a faulty coloring. */
		if (!startPattern) {
			startPattern = &pass1Patterns[0];
		}

		TextCursor endAt = parseBufferRange(startPattern, pass2Patterns, buf, styleBuf, context, beginParse, endParse);

		/* If parse completed at this level, move one style up in the
		   hierarchy and start again from where the previous parse left off. */
		if (endAt < endParse) {
			beginParse = endAt;
			endParse   = forwardOneContext(buf, context, std::max(endAt, std::max(lastModified(styleBuf), lastMod)));
			if (isPlain(parseInStyle)) {
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
			lastMod  = lastModified(styleBuf);
			endParse = std::min(buf->BufEndOfBuffer(), forwardOneContext(buf, context, lastMod) + (REPARSE_CHUNK_SIZE << nPasses));
		}
	}
}

/**
 * @brief readHighlightPattern
 * @param in
 * @param errMsg
 * @param pattern
 * @return
 */
bool readHighlightPattern(Input &in, QString *errMsg, HighlightPattern *pattern) {

	// read the name field
	QString name = Preferences::ReadSymbolicField(in);
	if (name.isNull()) {
		*errMsg = tr("pattern name is required");
		return false;
	}
	pattern->name = name;

	if (!Preferences::SkipDelimiter(in, errMsg)) {
		return false;
	}

	// read the start pattern
	if (!Preferences::ReadQuotedString(in, errMsg, &pattern->startRE)) {
		return false;
	}

	if (!Preferences::SkipDelimiter(in, errMsg)) {
		return false;
	}

	// read the end pattern
	if (*in == QLatin1Char(':')) {
		pattern->endRE = QString();
	} else if (!Preferences::ReadQuotedString(in, errMsg, &pattern->endRE)) {
		return false;
	}

	if (!Preferences::SkipDelimiter(in, errMsg)) {
		return false;
	}

	// read the error pattern
	if (*in == QLatin1Char(':')) {
		pattern->errorRE = QString();
	} else if (!Preferences::ReadQuotedString(in, errMsg, &pattern->errorRE)) {
		return false;
	}

	if (!Preferences::SkipDelimiter(in, errMsg)) {
		return false;
	}

	// read the style field
	pattern->style = Preferences::ReadSymbolicField(in);

	if (pattern->style.isNull()) {
		*errMsg = tr("style field required in pattern");
		return false;
	}

	if (!Preferences::SkipDelimiter(in, errMsg)) {
		return false;
	}

	// read the sub-pattern-of field
	pattern->subPatternOf = Preferences::ReadSymbolicField(in);

	if (!Preferences::SkipDelimiter(in, errMsg)) {
		return false;
	}

	// read flags field
	pattern->flags = 0;
	for (; *in != QLatin1Char('\n') && *in != QLatin1Char('}'); ++in) {

		if (*in == QLatin1Char('D')) {
			pattern->flags |= DEFER_PARSING;
		} else if (*in == QLatin1Char('R')) {
			pattern->flags |= PARSE_SUBPATS_FROM_START;
		} else if (*in == QLatin1Char('C')) {
			pattern->flags |= COLOR_ONLY;
		} else if (*in != QLatin1Char(' ') && *in != QLatin1Char('\t')) {
			*errMsg = tr("unreadable flag field");
			return false;
		}
	}
	return true;
}

/*
** Parse a set of highlight patterns into a vector of HighlightPattern
** structures, and a language mode name.  If unsuccessful, returns an empty
** vector message in "errMsg".
*/
boost::optional<std::vector<HighlightPattern>> readHighlightPatterns(Input &in, QString *errMsg) {
	// skip over blank space
	in.skipWhitespaceNL();

	// look for initial brace
	if (*in != QLatin1Char('{')) {
		*errMsg = tr("pattern list must begin with \"{\"");
		return boost::none;
	}
	++in;

	// parse each pattern in the list
	std::vector<HighlightPattern> ret;

	while (true) {
		in.skipWhitespaceNL();
		if (in.atEnd()) {
			*errMsg = tr("end of pattern list not found");
			return boost::none;
		}

		if (*in == QLatin1Char('}')) {
			++in;
			break;
		}

		HighlightPattern pat;
		if (!readHighlightPattern(in, errMsg, &pat)) {
			return boost::none;
		}

		ret.push_back(pat);
	}

	return ret;
}

/**
 * @brief readPatternYaml
 * @param patterns
 * @return
 */
HighlightPattern readPatternYaml(const YAML::Node &patterns) {
	HighlightPattern pattern;
	pattern.flags = 0;

	for (auto it = patterns.begin(); it != patterns.end(); ++it) {

		const std::string key  = it->first.as<std::string>();
		const YAML::Node value = it->second;

		if (key == "name") {
			pattern.name = QString::fromUtf8(value.as<std::string>().c_str());
		} else if (key == "style") {
			pattern.style = QString::fromUtf8(value.as<std::string>().c_str());
		} else if (key == "regex_start") {
			pattern.startRE = QString::fromUtf8(value.as<std::string>().c_str());
		} else if (key == "regex_end") {
			pattern.endRE = QString::fromUtf8(value.as<std::string>().c_str());
		} else if (key == "regex_error") {
			pattern.errorRE = QString::fromUtf8(value.as<std::string>().c_str());
		} else if (key == "parent") {
			pattern.subPatternOf = QString::fromUtf8(value.as<std::string>().c_str());
		} else if (key == "defer_parsing") {
			pattern.flags |= value.as<bool>() ? DEFER_PARSING : 0;
		} else if (key == "color_only") {
			pattern.flags |= value.as<bool>() ? COLOR_ONLY : 0;
		} else if (key == "parse_subpatterns_from_start") {
			pattern.flags |= value.as<bool>() ? PARSE_SUBPATS_FROM_START : 0;
		}
	}

	return pattern;
}

/**
 * @brief readPatternSetYaml
 * @param it
 * @return
 */
boost::optional<PatternSet> readPatternSetYaml(YAML::const_iterator it) {
	struct HighlightError {
		QString message;
	};

	try {
		PatternSet patternSet;
		patternSet.languageMode = QString::fromUtf8(it->first.as<std::string>().c_str());
		YAML::Node entries      = it->second;

		if (entries.IsMap()) {
			for (auto set_it = entries.begin(); set_it != entries.end(); ++set_it) {

				const std::string key  = set_it->first.as<std::string>();
				const YAML::Node value = set_it->second;

				if (key == "char_context") {
					patternSet.charContext = value.as<int>();
				} else if (key == "line_context") {
					patternSet.lineContext = value.as<int>();
				} else if (key == "patterns") {
					for (YAML::Node entry : value) {

						HighlightPattern pattern = readPatternYaml(entry);

						if (pattern.name.isEmpty()) {
							Raise<HighlightError>(tr("pattern name field required"));
						}

						if (pattern.style.isEmpty()) {
							Raise<HighlightError>(tr("pattern style field required"));
						}

						patternSet.patterns.emplace_back(std::move(pattern));
					}
				}
			}
		} else if (entries.as<std::string>() == "Default") {
			return readDefaultPatternSet(patternSet.languageMode);
		}

		return patternSet;
	} catch (const YAML::Exception &ex) {
		qWarning("NEdit: Invalid YAML:\n%s", ex.what());
	} catch (const HighlightError &ex) {
		qWarning("NEdit: %s", qPrintable(ex.message));
	}

	return boost::none;
}

/*
** Read in a pattern set character string, and advance *inPtr beyond it.
** Returns nullptr and outputs an error to stderr on failure.
*/
boost::optional<PatternSet> readPatternSet(Input &in) {

	struct HighlightError {
		QString message;
	};

	try {
		QString errMsg;

		// remove leading whitespace
		in.skipWhitespaceNL();

		PatternSet patSet;

		// read language mode field
		patSet.languageMode = Preferences::ReadSymbolicField(in);

		if (patSet.languageMode.isNull()) {
			Raise<HighlightError>(tr("language mode must be specified"));
		}

		if (!Preferences::SkipDelimiter(in, &errMsg)) {
			Raise<HighlightError>(errMsg);
		}

		/* look for "Default" keyword, and if it's there, return the default
		   pattern set */
		if (in.match(QLatin1String("Default"))) {
			boost::optional<PatternSet> retPatSet = readDefaultPatternSet(patSet.languageMode);
			if (!retPatSet) {
				Raise<HighlightError>(tr("No default pattern set"));
			}
			return retPatSet;
		}

		// read line context field
		if (!Preferences::ReadNumericField(in, &patSet.lineContext)) {
			Raise<HighlightError>(tr("unreadable line context field"));
		}

		if (!Preferences::SkipDelimiter(in, &errMsg)) {
			Raise<HighlightError>(errMsg);
		}

		// read character context field
		if (!Preferences::ReadNumericField(in, &patSet.charContext)) {
			Raise<HighlightError>(tr("unreadable character context field"));
		}

		// read pattern list
		boost::optional<std::vector<HighlightPattern>> patterns = readHighlightPatterns(in, &errMsg);
		if (!patterns) {
			Raise<HighlightError>(errMsg);
		}

		patSet.patterns = std::move(*patterns);

		// pattern set was read correctly, make an allocated copy to return
		return patSet;
	} catch (const HighlightError &error) {
		Preferences::reportError(
			nullptr,
			*in.string(),
			in.index(),
			tr("highlight pattern"),
			error.message);

		return boost::none;
	}
}

/**
 * @brief find_subpattern
 * @param pattern
 * @param index
 * @return
 */
HighlightData *find_subpattern(const HighlightData *pattern, size_t index) {

	// Figure out which sub-pattern matched
	for (size_t i = 0; i < pattern->nSubPatterns; i++) {
		HighlightData *subPat = pattern->subPatterns[i];
		if (subPat->colorOnly) {
			++index;
		} else if (i == index) {
			return subPat;
		}
	}

	qCritical("NEdit: Internal error, failed to find sub-pattern");
	return nullptr;
}

/**
 * @brief readDefaultPatternSets
 * @return
 */
std::vector<PatternSet> readDefaultPatternSets() {
	QByteArray defaultPatternSets = loadResource(QLatin1String("DefaultPatternSets.yaml"));
	YAML::Node patternSets        = YAML::Load(defaultPatternSets.data());

	std::vector<PatternSet> defaultPatterns;

	for (auto it = patternSets.begin(); it != patternSets.end(); ++it) {
		// Read each pattern set, abort on error
		if (boost::optional<PatternSet> patSet = readPatternSetYaml(it)) {
			defaultPatterns.push_back(*patSet);
		}
	}

	return defaultPatterns;
}

}

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
void SyntaxHighlightModifyCB(TextCursor pos, int64_t nInserted, int64_t nDeleted, int64_t nRestyled, view::string_view deletedText, void *user) {

	Q_UNUSED(nRestyled)
	Q_UNUSED(deletedText)
	Q_ASSERT(user);

	auto document = static_cast<DocumentWidget *>(user);

	const std::unique_ptr<WindowHighlightData> &highlightData = document->highlightData_;
	if (!highlightData) {
		return;
	}

	const std::shared_ptr<TextBuffer> &styleBuffer = highlightData->styleBuffer;

	/* Restyling-only modifications (usually a primary or secondary  selection)
	   don't require any processing, but clear out the style buffer selection
	   so the widget doesn't think it has to keep redrawing the old area */
	if (nInserted == 0 && nDeleted == 0) {
		styleBuffer->BufUnselect();
		return;
	}

	/* First and foremost, the style buffer must track the text buffer
	   accurately and correctly */
	if (nInserted > 0) {
		std::string insStyle(static_cast<size_t>(nInserted), UNFINISHED_STYLE);
		styleBuffer->BufReplace(pos, pos + nDeleted, insStyle);
	} else {
		styleBuffer->BufRemove(pos, pos + nDeleted);
	}

	/* Mark the changed region in the style buffer as requiring redraw.  This
	   is not necessary for getting it redrawn, it will be redrawn anyhow by
	   the text display callback, but it clears the previous selection and
	   saves the modifyStyleBuf routine from unnecessary work in tracking
	   changes that are already scheduled for redraw */
	styleBuffer->BufSelect(pos, pos + nInserted);

	// Re-parse around the changed region
	if (highlightData->pass1Patterns) {
		incrementalReparse(highlightData, document->buffer(), pos, nInserted);
	}
}

/*
** Parses "string" according to compiled regular expressions in "pattern"
** until endRE is or errorRE are matched, or end of string is reached.
** Advances "string_ptr", "style_ptr" pointers to the next character past
** the end of the parsed section, and updates "prevChar" to reflect
** the new character before "string_ptr".
**
** "length" is how much of the string must be parsed.
**
** "look_behind_to" indicates the boundary till where look-behind patterns may
** look back. If nullptr, the start of the string is assumed to be the boundary.
**
** "match_to" indicates the boundary till where matches may extend. If nullptr,
** it is assumed that the end of the string indicates the boundary.
**
** Returns true if parsing was done and the parse succeeded.  Returns false if
** the error pattern matched, if the end of the string was reached without
** matching the end expression, or in the unlikely event of an internal error.
*/
bool parseString(const HighlightData *pattern, const char *&string_ptr, char *&style_ptr, int64_t length, const ParseContext *ctx, const char *look_behind_to, const char *match_to) {

	if (length <= 0) {
		return false;
	}

	if (!look_behind_to) {
		look_behind_to = ctx->text.begin();
	}

	if (!match_to) {
		match_to = ctx->text.end();
	}

	bool subExecuted;
	const int next_char = (match_to != ctx->text.end()) ? (*match_to) : -1;

	const char *stringPtr = string_ptr;
	char *stylePtr        = style_ptr;

	const std::unique_ptr<Regex> &subPatternRE = pattern->subPatternRE;

	const QByteArray delimitersString = ctx->delimiters.toLatin1();
	const char *delimitersPtr         = ctx->delimiters.isNull() ? nullptr : delimitersString.data();

	while (subPatternRE->ExecRE(
		stringPtr,
		string_ptr + length + 1,
		false,
		*ctx->prev_char,
		next_char,
		delimitersPtr,
		look_behind_to,
		match_to,
		ctx->text.end())) {

		/* Beware of the case where only one real branch exists, but that
		   branch has sub-branches itself. In that case the top_branch refers
		   to the matching sub-branch and must be ignored. */
		size_t subIndex = (pattern->nSubBranches > 1) ? subPatternRE->top_branch : 0;

		// Combination of all sub-patterns and end pattern matched
		const char *const startingStringPtr = stringPtr;

		/* Fill in the pattern style for the text that was skipped over before
		   the match, and advance the pointers to the start of the pattern */
		fillStyleString(stringPtr, stylePtr, subPatternRE->startp[0], pattern->style, ctx);

		/* If the combined pattern matched this pattern's end pattern, we're
		   done.  Fill in the style string, update the pointers, color the
		   end expression if there were coloring sub-patterns, and return */
		const char *const savedStartPtr = stringPtr;
		const int savedPrevChar         = *ctx->prev_char;

		if (pattern->endRE) {
			if (subIndex == 0) {
				fillStyleString(stringPtr, stylePtr, subPatternRE->endp[0], pattern->style, ctx);
				subExecuted = false;

				for (size_t i = 0; i < pattern->nSubPatterns; i++) {
					HighlightData *const subPat = pattern->subPatterns[i];
					if (subPat->colorOnly) {
						if (!subExecuted) {
							if (!pattern->endRE->ExecRE(
									savedStartPtr,
									savedStartPtr + 1,
									false,
									savedPrevChar,
									next_char,
									delimitersPtr,
									look_behind_to,
									match_to,
									ctx->text.end())) {
								qCritical("NEdit: Internal error, failed to recover end match in parseString");
								return false;
							}
							subExecuted = true;
						}

						for (size_t subExpr : subPat->endSubexpressions) {
							recolorSubexpression(pattern->endRE, subExpr, subPat->style, string_ptr, style_ptr);
						}
					}
				}

				string_ptr = stringPtr;
				style_ptr  = stylePtr;
				return true;
			}
			--subIndex;
		}

		/* If the combined pattern matched this pattern's error pattern, we're
		   done.  Fill in the style string, update the pointers, and return */
		if (pattern->errorRE) {
			if (subIndex == 0) {
				fillStyleString(stringPtr, stylePtr, subPatternRE->startp[0], pattern->style, ctx);
				string_ptr = stringPtr;
				style_ptr  = stylePtr;
				return false;
			}
			--subIndex;
		}

		HighlightData *subPat = find_subpattern(pattern, subIndex);
		Q_ASSERT(subPat);

		// the sub-pattern is a simple match, just color it
		if (!subPat->subPatternRE) {
			fillStyleString(stringPtr, stylePtr, subPatternRE->endp[0], /* subPat->startRE->endp[0],*/ subPat->style, ctx);

			// Parse the remainder of the sub-pattern
		} else if (subPat->endRE) {
			/* The pattern is a starting/ending type of pattern, proceed with
			   the regular hierarchical parsing. */

			/* If parsing should start after the start pattern, advance
			   to that point (this is currently always the case) */
			if (!(subPat->flags & PARSE_SUBPATS_FROM_START)) {
				fillStyleString(
					stringPtr,
					stylePtr,
					subPatternRE->endp[0], // subPat->startRE->endp[0],
					subPat->style,
					ctx);
			}

			// Parse to the end of the subPattern
			parseString(
				subPat,
				stringPtr,
				stylePtr,
				length - (stringPtr - string_ptr),
				ctx,
				look_behind_to,
				match_to);

		} else {
			/* If the parent pattern is not a start/end pattern, the
			   sub-pattern can between the boundaries of the parent's
			   match. Note that we must limit the recursive matches such
			   that they do not exceed the parent's ending boundary.
			   Without that restriction, matching becomes unstable. */

			// Parse to the end of the subPattern
			parseString(
				subPat,
				stringPtr,
				stylePtr,
				subPatternRE->endp[0] - stringPtr,
				ctx,
				look_behind_to,
				subPatternRE->endp[0]);
		}

		/* If the sub-pattern has color-only sub-sub-patterns, add color
		   based on the coloring sub-expression references */
		subExecuted = false;
		for (size_t i = 0; i < subPat->nSubPatterns; i++) {
			HighlightData *subSubPat = subPat->subPatterns[i];
			if (subSubPat->colorOnly) {
				if (!subExecuted) {
					if (!subPat->startRE->ExecRE(
							savedStartPtr,
							savedStartPtr + 1,
							false,
							savedPrevChar,
							next_char,
							delimitersPtr,
							look_behind_to,
							match_to,
							ctx->text.end())) {
						qCritical("NEdit: Internal error, failed to recover start match in parseString");
						return false;
					}
					subExecuted = true;
				}

				for (size_t subExpr : subSubPat->startSubexpressions) {
					recolorSubexpression(subPat->startRE, subExpr, subSubPat->style, string_ptr, style_ptr);
				}
			}
		}

		/* Make sure parsing progresses.  If patterns match the empty string,
		   they can get stuck and hang the process */
		if (stringPtr == startingStringPtr) {
			/* Avoid stepping over the end of the string (possible for
			   zero-length matches at end of the string) */
			if (stringPtr == match_to) {
				break;
			}

			fillStyleString(stringPtr, stylePtr, stringPtr + 1, pattern->style, ctx);
		}
	}

	// Reached end of string, fill in the remaining text with pattern style
	fillStyleString(stringPtr, stylePtr, string_ptr + length, pattern->style, ctx);

	// Advance the string and style pointers to the end of the parsed text
	string_ptr = stringPtr;
	style_ptr  = stylePtr;
	return pattern->endRE == nullptr;
}

/*
** Get the character before position "pos" in buffer "buf"
*/
int getPrevChar(TextBuffer *buf, TextCursor pos) {
	return pos == buf->BufStartOfBuffer() ? -1 : buf->BufGetCharacter(pos - 1);
}

/*
** Return a position far enough back in "buf" from "fromPos" to give patterns
** their guaranteed amount of context for matching (from "context").  If
** backing up by lines yields the greater distance, the returned position will
** be to the newline character before the start of the line, rather than to
** the first character of the line.  (I did this because earlier prototypes of
** the syntax highlighting code, which were based on single-line context, used
** this to ensure that line-spanning expressions would be detected.  I think
** it may reduce some 2 line context requirements to one line, at a cost of
** only one extra character, but I'm not sure, and my brain hurts from
** thinking about it).
*/
TextCursor backwardOneContext(TextBuffer *buf, const ReparseContext &context, TextCursor fromPos) {

	const TextCursor begin = buf->BufStartOfBuffer();

	if (context.nLines == 0) {
		return std::max(begin, fromPos - context.nChars);
	}

	if (context.nChars == 0) {
		return std::max(begin, buf->BufCountBackwardNLines(fromPos, context.nLines - 1) - 1);
	}

	return std::max(begin, std::min(std::max(begin, buf->BufCountBackwardNLines(fromPos, context.nLines - 1) - 1), fromPos - context.nChars));
}

/*
** Return a position far enough forward in "buf" from "fromPos" to ensure
** that patterns are given their required amount of context for matching
** (from "context").  If moving forward by lines yields the greater
** distance, the returned position will be the first character of of the
** next line, rather than the newline character at the end (see notes in
** backwardOneContext).
*/
TextCursor forwardOneContext(TextBuffer *buf, const ReparseContext &context, TextCursor fromPos) {

	const TextCursor end = buf->BufEndOfBuffer();

	if (context.nLines == 0) {
		return std::min(end, fromPos + context.nChars);
	}

	if (context.nChars == 0) {
		return std::min(end, buf->BufCountForwardNLines(fromPos, context.nLines));
	}

	return std::min(end, std::max(buf->BufCountForwardNLines(fromPos, context.nLines), fromPos + context.nChars));
}

/*
** Search for a pattern in pattern list "patterns" with style "style"
*/
HighlightData *patternOfStyle(const std::unique_ptr<HighlightData[]> &patterns, uint8_t style) {

	for (size_t i = 0; patterns[i].style != 0; ++i) {
		if (patterns[i].style == style) {
			return &patterns[i];
		}
	}

	if (style == PLAIN_STYLE || style == UNFINISHED_STYLE) {
		return &patterns[0];
	}

	return nullptr;
}

/**
 * @brief indexOfNamedPattern
 * @param patterns
 * @param name
 * @return
 */
size_t indexOfNamedPattern(const std::vector<HighlightPattern> &patterns, const QString &name) {

	if (name.isNull()) {
		return PATTERN_NOT_FOUND;
	}

	for (size_t i = 0; i < patterns.size(); ++i) {
		if (patterns[i].name == name) {
			return i;
		}
	}

	return PATTERN_NOT_FOUND;
}

/**
 * @brief findTopLevelParentIndex
 * @param patterns
 * @param index
 * @return
 */
size_t findTopLevelParentIndex(const std::vector<HighlightPattern> &patterns, size_t index) {

	size_t topIndex = index;
	while (!patterns[topIndex].subPatternOf.isNull()) {
		topIndex = indexOfNamedPattern(patterns, patterns[topIndex].subPatternOf);
		if (index == topIndex) {
			return PATTERN_NOT_FOUND; // amai: circular dependency ?!
		}
	}
	return topIndex;
}

/*
** Read a string representing highlight pattern sets and add them
** to the PatternSets list of loaded highlight patterns.  Note that the
** patterns themselves are not parsed until they are actually used.
*/
void LoadHighlightString(const QString &string) {

	if (string == QLatin1String("*")) {

		YAML::Node patternSets;

		const QString highlightPatternsFile = Settings::highlightPatternsFile();
		if (QFileInfo::exists(highlightPatternsFile)) {
			patternSets = YAML::LoadFile(highlightPatternsFile.toUtf8().data());
		} else {
			static QByteArray defaultPatternSets = loadResource(QLatin1String("DefaultPatternSets.yaml"));
			patternSets                          = YAML::Load(defaultPatternSets.data());
		}

		for (auto it = patternSets.begin(); it != patternSets.end(); ++it) {
			// Read each pattern set, abort on error
			boost::optional<PatternSet> patSet = readPatternSetYaml(it);
			if (!patSet) {
				break;
			}

			// Add/change the pattern set in the list
			insert_or_replace(PatternSets, *patSet, [&patSet](const PatternSet &patternSet) {
				return patternSet.languageMode == patSet->languageMode;
			});
		}
	} else {
		Input in(&string);

		Q_FOREVER {

			// Read each pattern set, abort on error
			boost::optional<PatternSet> patSet = readPatternSet(in);
			if (!patSet) {
				break;
			}

			// Add/change the pattern set in the list
			insert_or_replace(PatternSets, *patSet, [&patSet](const PatternSet &patternSet) {
				return patternSet.languageMode == patSet->languageMode;
			});

			// if the string ends here, we're done
			in.skipWhitespaceNL();
			if (in.atEnd()) {
				break;
			}
		}
	}
}

/*
** Create a string in the correct format for the highlightPatterns resource,
** containing all of the highlight pattern information from the stored
** highlight pattern list (PatternSets) for this session.
*/
QString WriteHighlightString() {

	const QString filename = Settings::highlightPatternsFile();

	try {
		YAML::Emitter out;
		out << YAML::BeginMap;
		for (const PatternSet &patternSet : PatternSets) {
			if (isDefaultPatternSet(patternSet)) {
				out << YAML::Key << patternSet.languageMode.toUtf8().data();
				out << YAML::Value << "Default";
			} else {

				out << YAML::Key << patternSet.languageMode.toUtf8().data();
				out << YAML::Value << YAML::BeginMap;
				out << YAML::Key << "line_context" << YAML::Value << patternSet.lineContext;
				out << YAML::Key << "char_context" << YAML::Value << patternSet.charContext;
				out << YAML::Key << "patterns" << YAML::Value;
				out << YAML::BeginSeq;
				for (const HighlightPattern &pat : patternSet.patterns) {
					out << YAML::BeginMap;
					out << YAML::Key << "name" << YAML::Value << pat.name.toUtf8().data();
					out << YAML::Key << "style" << YAML::Value << pat.style.toUtf8().data();

					out << YAML::Key << "defer_parsing" << YAML::Value << ((pat.flags & DEFER_PARSING) != 0);
					out << YAML::Key << "color_only" << YAML::Value << ((pat.flags & COLOR_ONLY) != 0);
					out << YAML::Key << "parse_subpatterns_from_start" << YAML::Value << ((pat.flags & PARSE_SUBPATS_FROM_START) != 0);

					if (!pat.subPatternOf.isNull()) {
						out << YAML::Key << "parent" << YAML::Value << pat.subPatternOf.toUtf8().data();
					}

					if (!pat.startRE.isNull()) {
						out << YAML::Key << "regex_start" << YAML::Value << pat.startRE.toUtf8().data();
					}

					if (!pat.endRE.isNull()) {
						out << YAML::Key << "regex_end" << YAML::Value << pat.endRE.toUtf8().data();
					}

					if (!pat.errorRE.isNull()) {
						out << YAML::Key << "regex_error" << YAML::Value << pat.errorRE.toUtf8().data();
					}
					out << YAML::EndMap;
				}
				out << YAML::EndSeq;
				out << YAML::EndMap;
			}
		}
		out << YAML::EndMap;

		QFile file(filename);
		if (file.open(QIODevice::WriteOnly)) {
			file.write(out.c_str());
			file.write("\n");
		}

		return QLatin1String("*");
	} catch (const YAML::Exception &ex) {
		qWarning("NEdit: Error writing %s in config directory:\n%s", qPrintable(filename), ex.what());
	}

	return QString();
}

bool FontOfNamedStyleIsBold(const QString &styleName) {
	const size_t styleNo = IndexOfNamedStyle(styleName);

	if (styleNo == STYLE_NOT_FOUND) {
		return false;
	}

	const int fontNum = HighlightStyles[styleNo].font;
	return (fontNum & Font::Bold);
}

bool FontOfNamedStyleIsItalic(const QString &styleName) {
	const size_t styleNo = IndexOfNamedStyle(styleName);

	if (styleNo == STYLE_NOT_FOUND) {
		return false;
	}

	const int fontNum = HighlightStyles[styleNo].font;
	return (fontNum & Font::Italic);
}

/*
** Find the color associated with a named style.  This routine must only be
** called with a valid styleName (call NamedStyleExists to find out whether
** styleName is valid).
*/
QString FgColorOfNamedStyle(const QString &styleName) {
	const size_t styleNo = IndexOfNamedStyle(styleName);

	if (styleNo == STYLE_NOT_FOUND) {
		return QLatin1String("black");
	}

	return HighlightStyles[styleNo].color;
}

/*
** Find the background color associated with a named style.
*/
QString BgColorOfNamedStyle(const QString &styleName) {
	const size_t styleNo = IndexOfNamedStyle(styleName);

	if (styleNo == STYLE_NOT_FOUND) {
		return QLatin1String("");
	}

	return HighlightStyles[styleNo].bgColor;
}

/*
** Determine whether a named style exists
*/
bool NamedStyleExists(const QString &styleName) {
	return IndexOfNamedStyle(styleName) != STYLE_NOT_FOUND;
}

/**
 * Look through the list of pattern sets, and find the one for a particular
 * language.
 *
 * @brief FindPatternSet
 * @param languageMode
 * @return nullptr if not found.
 */
PatternSet *FindPatternSet(const QString &languageMode) {

	auto it = std::find_if(PatternSets.begin(), PatternSets.end(), [&languageMode](PatternSet &patternSet) {
		return (patternSet.languageMode == languageMode);
	});

	if (it != PatternSets.end()) {
		return &*it;
	}

	return nullptr;
}

/*
** Given a language mode name, determine if there is a default (built-in)
** pattern set available for that language mode, and if so, return it
*/
boost::optional<PatternSet> readDefaultPatternSet(const QString &langModeName) {

	static const std::vector<PatternSet> defaultPatternSets = readDefaultPatternSets();

	auto it = std::find_if(defaultPatternSets.begin(), defaultPatternSets.end(), [&langModeName](const PatternSet &patternSet) {
		return langModeName == patternSet.languageMode;
	});

	if (it != defaultPatternSets.end()) {
		return *it;
	}

	return boost::none;
}

/*
** Returns a unique number of a given style name
** If styleName is not found, return STYLE_NOT_FOUND.
*/
size_t IndexOfNamedStyle(const QString &styleName) {
	for (size_t i = 0; i < HighlightStyles.size(); i++) {
		if (HighlightStyles[i].name == styleName) {
			return i;
		}
	}

	return STYLE_NOT_FOUND;
}

/*
** Change the language mode name of pattern sets for language "oldName" to
** "newName" in both the stored patterns, and the pattern set currently being
** edited in the dialog.
*/
void RenameHighlightPattern(const QString &oldName, const QString &newName) {

	for (PatternSet &patternSet : Highlight::PatternSets) {
		if (patternSet.languageMode == oldName) {
			patternSet.languageMode = newName;
		}
	}
}

}
