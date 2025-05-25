
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
#include "TextBuffer.h"
#include "Util/Input.h"
#include "Util/Raise.h"
#include "Util/Resource.h"
#include "Util/algorithm.h"
#include "WindowHighlightData.h"
#include "X11Colors.h"
#include "Yaml.h"

#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QtDebug>

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <memory>

#include <yaml-cpp/yaml.h>

namespace Highlight {

// list of available highlight styles
std::vector<HighlightStyle> HighlightStyles;

// Pattern sources loaded from the .nedit file or set by the user
std::vector<PatternSet> PatternSets;

namespace {

/* Initial forward expansion of parsing region in incremental re-parsing,
   when style changes propagate forward beyond the original modification.
   This distance is increased by a factor of two for each subsequent step. */
constexpr int ReparseChunkSize = 80;

constexpr auto StyleNotFound = static_cast<size_t>(-1);

/**
 * @brief Check if a style is plain or unfinished.
 *
 * @param style The style to check.
 * @return `true` if the style is plain or unfinished, `false` otherwise.
 */
constexpr bool IsPlain(uint8_t style) {
	return (style == PLAIN_STYLE || style == UNFINISHED_STYLE);
}

/**
 * @brief Compare two styles where one of the styles may not yet have been
 * processed with pass2 patterns
 *
 * @param style1 The first style to compare.
 * @param style2 The second style to compare.
 * @param firstPass2Style The style used for the first pass of pass2 patterns.
 * @return `true` if the styles are equivalent, `false` otherwise.
 */
constexpr bool EquivalentStyle(uint8_t style1, uint8_t style2, uint8_t firstPass2Style) {
	return (style1 == style2) ||
		   (style1 == UNFINISHED_STYLE && (style2 == PLAIN_STYLE || style2 >= firstPass2Style)) ||
		   (style2 == UNFINISHED_STYLE && (style1 == PLAIN_STYLE || style1 >= firstPass2Style));
}

/**
 * @brief Determine if the context requirements allow crossing line boundaries.
 * Scanning context can be reduced (with big efficiency gains) if we
 * know that patterns can't cross line boundaries, which is implied
 * by a context requirement of 1 line and 0 characters
 *
 * @param contextRequirements The context requirements to check.
 * @return `true` if crossing line boundaries is allowed, `false` otherwise.
 */
bool CanCrossLineBoundaries(const ReparseContext &contextRequirements) {
	return (contextRequirements.nLines != 1 || contextRequirements.nChars != 0);
}

/**
 * @brief Get the parent style of a given style.
 *
 * @param parentStyles The parent styles vector, which maps styles to their parent styles.
 * @param style The style for which to find the parent.
 * @return The parent style of the given style.
 */
uint8_t ParentStyleOf(const std::vector<uint8_t> &parentStyles, uint8_t style) {
	Q_ASSERT(style != 0);
	return parentStyles[style - UNFINISHED_STYLE];
}

/**
 * @brief Check if one style is a an ancestor of another style.
 *
 * @param parentStyles The parent styles vector, which maps styles to their parent styles.
 * @param style1 The style to check if it is a parent of `style2`.
 * @param style2 The style to check if it is a child of `style1`.
 * @return `true` if `style1` is in the parent chain of `style2`, `false` otherwise.
 */
bool IsParentStyle(const std::vector<uint8_t> &parentStyles, uint8_t style1, uint8_t style2) {

	for (uint8_t p = ParentStyleOf(parentStyles, style2); p != 0; p = ParentStyleOf(parentStyles, p)) {
		if (style1 == p) {
			return true;
		}
	}

	return false;
}

/**
 * @brief Get the last modified position in a text buffer (as marked by ModifyStyleBuffer
 * by the convention used for conveying modification information to the text widget,
 * which is selecting the text)
 *
 * @param buffer The text buffer to check for modifications.
 * @return The last modified position as a TextCursor.
 */
template <class Ptr>
TextCursor LastModified(const Ptr &buffer) {
	if (buffer->primary.hasSelection()) {
		return std::max(buffer->BufStartOfBuffer(), buffer->primary.end());
	}

	return buffer->BufStartOfBuffer();
}

/**
 * @brief Check if a given pattern set is the default pattern set for its language mode.
 *
 * @param patternSet The pattern set to check.
 * @return `true` if `patternSet` exactly matches one of the default pattern sets.
 */
bool IsDefaultPatternSet(const PatternSet &patternSet) {
	if (std::optional<PatternSet> defaultPatSet = ReadDefaultPatternSet(patternSet.languageMode)) {
		return patternSet == *defaultPatSet;
	}

	return false;
}

/**
 * @brief Discriminates patterns which can be used with ParseString from those which
 * can't. Leaf patterns are not suitable for parsing, because patterns
 * contain the expressions used for parsing within the context of their own
 * operation, i.e. the parent pattern initiates, and leaf patterns merely
 * confirm and color.
 *
 * @param pattern The pattern to check for parsability.
 * @return Returns `true` if the pattern is suitable for parsing, `false` otherwise.
 */
bool PatternIsParsable(HighlightData *pattern) {
	return pattern && pattern->subPatternRE;
}

/**
 * @brief Advance `string_ptr` and `style_ptr` until `string_ptr` == `to_ptr`, filling
 * `style_ptr` with style `style`. Can also optionally update the pre-string
 * character, `prev_char`, which is fed to regular the expression matching
 * routines for determining word and line boundaries at the start of the string.
 *
 * @param string_ptr The pointer to the current position in the string.
 * @param style_ptr The pointer to the current position in the style string.
 * @param to_ptr The pointer to the end of the string segment to fill with style.
 * @param style The style to fill the segment with.
 */
void FillStyleString(const char *string_ptr, uint8_t *style_ptr, const char *to_ptr, uint8_t style) {

	const ptrdiff_t len = to_ptr - string_ptr;

	if (string_ptr >= to_ptr) {
		return;
	}

	for (ptrdiff_t i = 0; i < len; i++) {
		*style_ptr++ = style;
	}
}

/**
 * @brief Advance `string_ptr` and `style_ptr` until `string_ptr` == `to_ptr`, filling
 * `style_ptr` with style `style`. Can also optionally update the pre-string
 * character, `prev_char`, which is fed to regular the expression matching
 * routines for determining word and line boundaries at the start of the string.
 *
 * @param string_ptr The pointer to the current position in the string.
 * @param style_ptr The pointer to the current position in the style string.
 * @param to_ptr The pointer to the end of the string segment to fill with style.
 * @param style The style to fill the segment with.
 * @param ctx The parse context, which contains the previous character pointer.
 */
void FillStyleString(const char *&string_ptr, uint8_t *&style_ptr, const char *to_ptr, uint8_t style, const ParseContext *ctx) {

	const ptrdiff_t len = to_ptr - string_ptr;

	if (string_ptr >= to_ptr) {
		return;
	}

	for (ptrdiff_t i = 0; i < len; i++) {
		*style_ptr++ = style;
	}

	if (ctx->prev_char) {
		*ctx->prev_char = static_cast<unsigned char>(*(to_ptr - 1));
	}

	string_ptr = to_ptr;
}

/**
 * @brief Recolor a subexpression in a regex match.
 * Change styles in the portion of `style_base` to `style` where a particular
 * sub-expression, `subexpr`, of regular expression `re` applies to the
 * corresponding portion of `string_base`.
 *
 * @param re The regex object containing the match information.
 * @param subexpr The index of the subexpression to recolor.
 * @param style The style to apply to the subexpression.
 * @param string_base The base pointer to the original string.
 * @param style_base The base pointer to the style string.
 */
void RecolorSubexpression(const std::unique_ptr<Regex> &re, size_t subexpr, uint8_t style, const char *string_base, uint8_t *style_base) {

	const char *string_ptr = re->startp[subexpr];
	const char *to_ptr     = re->endp[subexpr];
	uint8_t *style_ptr     = &style_base[string_ptr - string_base];

	FillStyleString(string_ptr, style_ptr, to_ptr, style);
}

/**
 * @brief Takes a string which has already been parsed through pass1 parsing and
 * re-parses the areas where pass two patterns are applicable. Parameters
 * have the same meaning as in ParseString, except that string pointers are
 * not updated.
 *
 * @param pattern The pattern to use for parsing.
 * @param string The string to parse.
 * @param styleString The style string to fill with styles.
 * @param length The length of the string to parse.
 * @param ctx The parse context, which contains the previous character pointer and delimiters.
 * @param lookBehindTo The pointer to the end of the look-behind region.
 * @param match_to The pointer to the end of the match region.
 */
void PassTwoParseString(const HighlightData *pattern, const char *string, uint8_t *styleString, int64_t length, const ParseContext *ctx, const char *lookBehindTo, const char *match_to) {

	bool inParseRegion            = false;
	const char *parseStart        = nullptr;
	uint8_t *s                    = styleString;
	const char *c                 = string;
	const uint8_t firstPass2Style = pattern[1].style;

	for (;; c++, s++) {
		if (!inParseRegion && c != match_to && (*s == UNFINISHED_STYLE || *s == PLAIN_STYLE || *s >= firstPass2Style)) {
			parseStart    = c;
			inParseRegion = true;
		}

		if (inParseRegion && (c == match_to || !(*s == UNFINISHED_STYLE || *s == PLAIN_STYLE || *s >= firstPass2Style))) {
			const char *parseEnd = c;
			if (parseStart != string) {
				*ctx->prev_char = static_cast<unsigned char>(*(parseStart - 1));
			}

			const char *stringPtr = parseStart;
			uint8_t *stylePtr     = &styleString[parseStart - string];

			match_to = parseEnd;
			length   = std::min<int64_t>(parseEnd - parseStart, length - (parseStart - string));

			ParseString(
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

*/

/**
 * @brief Modify the style buffer with the new style string.
 * Incorporate changes from `styleString` into `styleBuf`, tracking changes
 * in need of redisplay, and marking them for redisplay by the text
 * modification callback. `firstPass2Style` is necessary
 * for distinguishing pass 2 styles which compare as equal to the unfinished
 * style in the original buffer, from pass1 styles which signal a change.
 *
 * @param styleBuf The style buffer to modify.
 * @param styleString The new style string to apply.
 * @param startPos The starting position in the style buffer to modify.
 * @param endPos The ending position in the style buffer to modify.
 * @param firstPass2Style The style used for the first pass of pass2 patterns.
 */
void ModifyStyleBuffer(const std::shared_ptr<UTextBuffer> &styleBuf, uint8_t *styleString, TextCursor startPos, TextCursor endPos, uint8_t firstPass2Style) {
	uint8_t *ch;
	TextCursor pos;
	TextCursor modStart;
	TextCursor modEnd;
	auto minPos                       = TextCursor(INT32_MAX);
	auto maxPos                       = TextCursor();
	const UTextBuffer::Selection *sel = &styleBuf->primary;

	// Skip the range already marked for redraw
	if (sel->hasSelection()) {
		modStart = sel->start();
		modEnd   = sel->end();
	} else {
		modStart = modEnd = startPos;
	}

	/* Compare the original style buffer (outside of the modified range) with
	   the new string with which it will be updated, to find the extent of
	   the modifications. Unfinished styles in the original match any
	   pass 2 style */
	for (ch = styleString, pos = startPos; pos < modStart && pos < endPos; ++ch, ++pos) {
		const uint8_t bufChar = styleBuf->BufGetCharacter(pos);
		if (*ch != bufChar && !(bufChar == UNFINISHED_STYLE && (*ch == PLAIN_STYLE || *ch >= firstPass2Style))) {

			minPos = std::min(minPos, pos);
			maxPos = std::max(maxPos, pos);
		}
	}

	for (ch = &styleString[std::max(0, modEnd - startPos)], pos = std::max(modEnd, startPos); pos < endPos; ++ch, ++pos) {
		const uint8_t bufChar = styleBuf->BufGetCharacter(pos);
		if (*ch != bufChar && !(bufChar == UNFINISHED_STYLE && (*ch == PLAIN_STYLE || *ch >= firstPass2Style))) {

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

/**
 * @brief Parse a range of text in a buffer using pass 1 and pass 2 patterns.
 * Parse text in buffer `buf` between positions `beginParse` and `endParse`
 * using pass 1 patterns over the entire range and pass 2 patterns where needed
 * to determine whether re-parsed areas have changed and need to be redrawn.
 * Deposits style information in `styleBuf` and expands the selection in
 * `styleBuf` to show the additional areas which have changed and need
 * redrawing. `beginParse` must be a position from which pass 1 parsing may
 * safely be started using the pass1Patterns given. Internally, adds a
 * "takeoff" safety region before `beginParse`, so that pass 2 patterns will be
 * allowed to match properly if they begin before `beginParse`, and a "landing"
 * safety region beyond `endparse` so that endParse is guaranteed to be parsed
 * correctly in both passes.
 *
 * @param pass1Patterns The first pass patterns to use for parsing.
 * @param pass2Patterns The second pass patterns to use for parsing, or `nullptr` if no pass 2 patterns are used.
 * @param buf The text buffer to parse.
 * @param styleBuf The style buffer to fill with styles.
 * @param contextRequirements The context requirements for parsing, which determine how far back and forward to parse.
 * @param beginParse The position in the buffer where parsing should begin.
 * @param endParse The position in the buffer where parsing should end.
 * @return the buffer position at which parsing finished
 * (this will normally be `endParse`, unless the pass1Patterns is a
 * pattern which does end and the end is reached).
 */
TextCursor ParseBufferRange(const HighlightData *pass1Patterns, const std::unique_ptr<HighlightData[]> &pass2Patterns, TextBuffer *buf, const std::shared_ptr<UTextBuffer> &styleBuf, const ReparseContext &contextRequirements, TextCursor beginParse, TextCursor endParse) {

	TextCursor endSafety;
	TextCursor endPass2Safety;
	TextCursor startPass2Safety;
	TextCursor modStart;
	TextCursor modEnd;
	TextCursor beginSafety;
	TextCursor p;
	uint8_t style;
	const uint8_t firstPass2Style = (!pass2Patterns) ? UINT8_MAX : pass2Patterns[1].style;

	// Begin parsing one context distance back (or to the last style change)
	const uint8_t beginStyle = pass1Patterns->style;
	if (CanCrossLineBoundaries(contextRequirements)) {
		beginSafety = BackwardOneContext(buf, contextRequirements, beginParse);
		for (p = beginParse; p >= beginSafety; --p) {
			style = styleBuf->BufGetCharacter(p - 1);
			if (!EquivalentStyle(style, beginStyle, firstPass2Style)) {
				beginSafety = p;
				break;
			}
		}
	} else {
		for (beginSafety = std::max(TextCursor(), beginParse - 1); beginSafety > 0; --beginSafety) {
			style = styleBuf->BufGetCharacter(beginSafety);
			if (!EquivalentStyle(style, beginStyle, firstPass2Style) || buf->BufGetCharacter(beginSafety) == '\n') {
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

	if (CanCrossLineBoundaries(contextRequirements)) {
		endSafety = ForwardOneContext(buf, contextRequirements, endParse);
	} else if (endParse >= buf->length() || (buf->BufGetCharacter(endParse - 1) == '\n')) {
		endSafety = endParse;
	} else {
		endSafety = std::min(buf->BufEndOfBuffer(), buf->BufEndOfLine(endParse) + 1);
	}

	// copy the buffer range into a string

	const std::string str               = buf->BufGetRange(beginSafety, endSafety);
	std::basic_string<uint8_t> styleStr = styleBuf->BufGetRange(beginSafety, endSafety);

	const char *const string   = str.data();
	uint8_t *const styleString = styleStr.data();
	const char *const match_to = string + str.size();

	// Parse it with pass 1 patterns
	int prev_char = GetPrevChar(buf, beginParse);
	ParseContext ctx;
	ctx.prev_char         = &prev_char;
	ctx.text              = str;
	const char *stringPtr = &string[beginParse - beginSafety];
	uint8_t *stylePtr     = &styleString[beginParse - beginSafety];

	ParseString(
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
		ModifyStyleBuffer(styleBuf, &styleString[beginParse - beginSafety], beginParse, endParse, firstPass2Style);
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
			endPass2Safety = ForwardOneContext(buf, contextRequirements, modStart);
			if (endPass2Safety + PASS_2_REPARSE_CHUNK_SIZE >= modEnd) {
				endPass2Safety = endSafety;
			}
		} else {
			endPass2Safety = endSafety;
		}

		safe_prev_char = GetPrevChar(buf, beginSafety);
		ctx.prev_char  = &safe_prev_char;

		if (endPass2Safety == endSafety) {
			PassTwoParseString(
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
			ModifyStyleBuffer(styleBuf, &styleString[beginParse - beginSafety], beginParse, endParse, firstPass2Style);
			return endParse;
		}

		PassTwoParseString(
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
			prev_char = GetPrevChar(buf, beginSafety);
			PassTwoParseString(
				&pass2Patterns[0],
				string,
				styleString,
				endParse - beginSafety,
				&ctx,
				string,
				match_to);
		} else {
			startPass2Safety = std::max(beginSafety, BackwardOneContext(buf, contextRequirements, modEnd));

			prev_char = GetPrevChar(buf, startPass2Safety);
			PassTwoParseString(
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
	ModifyStyleBuffer(styleBuf, &styleString[beginParse - beginSafety], beginParse, endParse, firstPass2Style);
	return endParse;
}

/**
 * @brief Back up position pointed to by `pos` enough that parsing from that point
 * on will satisfy context guarantees for pattern matching for modifications
 * at `pos`. The caller is guaranteed that parsing may safely BEGIN with that style,
 * but not that it will continue at that level.
 *
 * This routine can be fooled if a continuous style run of more than one
 * context distance in length is produced by multiple pattern matches which
 * abut, rather than by a single continuous match. In this case the
 * position returned by this routine may be a bad starting point which will
 * result in an incorrect re-parse. However this will happen very rarely,
 * and, if it does, is unlikely to result in incorrect highlighting.
 *
 * @param buf The text buffer to check for styles.
 * @param highlightData The highlight data containing the patterns and context requirements.
 * @param pos The position in the buffer to check for styles.
 * @return Returns the style with which to begin parsing.
 */
uint8_t FindSafeParseRestartPos(TextBuffer *buf, const std::unique_ptr<WindowHighlightData> &highlightData, TextCursor *pos) {

	TextCursor checkBackTo;
	TextCursor safeParseStart;

	const std::vector<uint8_t> &parentStyles              = highlightData->parentStyles;
	const std::unique_ptr<HighlightData[]> &pass1Patterns = highlightData->pass1Patterns;
	const ReparseContext &context                         = highlightData->contextRequirements;

	// We must begin at least one context distance back from the change
	*pos = BackwardOneContext(buf, context, *pos);

	/* If the new position is outside of any styles or at the beginning of
	   the buffer, this is a safe place to begin parsing, and we're done */
	if (*pos == 0) {
		return PLAIN_STYLE;
	}

	const uint8_t startStyle = highlightData->styleBuffer->BufGetCharacter(*pos);

	if (IsPlain(startStyle)) {
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

	if (PatternIsParsable(patternOfStyle(pass1Patterns, startStyle))) {
		safeParseStart = BackwardOneContext(buf, context, *pos);
		checkBackTo    = BackwardOneContext(buf, context, safeParseStart);
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
		const uint8_t style = highlightData->styleBuffer->BufGetCharacter(i);
		if (IsParentStyle(parentStyles, style, runningStyle)) {
			if (PatternIsParsable(patternOfStyle(pass1Patterns, style))) {
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
		else if (IsParentStyle(parentStyles, runningStyle, style)) {
			if (PatternIsParsable(patternOfStyle(pass1Patterns, runningStyle))) {
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
		else if (runningStyle != style && IsParentStyle(parentStyles, ParentStyleOf(parentStyles, runningStyle), style)) {
			const uint8_t parentStyle = ParentStyleOf(parentStyles, runningStyle);
			if (PatternIsParsable(patternOfStyle(pass1Patterns, parentStyle))) {
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
			while (!PatternIsParsable(patternOfStyle(pass1Patterns, runningStyle))) {
				runningStyle = ParentStyleOf(parentStyles, runningStyle);
			}

			return runningStyle;
		}
	}
}

/**
 * @brief Re-parse the smallest region possible around a modification to buffer `buf`
 * to guarantee that the promised context lines and characters have
 * been presented to the patterns. Changes the style buffer in "highlightData"
 * with the parsing result.
 *
 * @param highlightData The highlight data containing the patterns and context requirements.
 * @param buf The text buffer to re-parse.
 * @param pos The position in the buffer where the modification occurred.
 * @param nInserted The number of characters inserted at `pos`.
 */
void IncrementalReparse(const std::unique_ptr<WindowHighlightData> &highlightData, TextBuffer *buf, TextCursor pos, int64_t nInserted) {

	const std::shared_ptr<UTextBuffer> &styleBuf          = highlightData->styleBuffer;
	const std::unique_ptr<HighlightData[]> &pass1Patterns = highlightData->pass1Patterns;
	const std::unique_ptr<HighlightData[]> &pass2Patterns = highlightData->pass2Patterns;
	const ReparseContext &context                         = highlightData->contextRequirements;
	const std::vector<uint8_t> &parentStyles              = highlightData->parentStyles;

	/* Find the position "beginParse" at which to begin re-parsing.  This is
	   far enough back in the buffer such that the guaranteed number of
	   lines and characters of context are examined. */
	TextCursor beginParse = pos;
	uint8_t parseInStyle  = FindSafeParseRestartPos(buf, highlightData, &beginParse);

	/* Find the position "endParse" at which point it is safe to stop
	   parsing, unless styles are getting changed beyond the last
	   modification */
	TextCursor lastMod  = pos + nInserted;
	TextCursor endParse = ForwardOneContext(buf, context, lastMod);

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

		const TextCursor endAt = ParseBufferRange(startPattern, pass2Patterns, buf, styleBuf, context, beginParse, endParse);

		/* If parse completed at this level, move one style up in the
		   hierarchy and start again from where the previous parse left off. */
		if (endAt < endParse) {
			beginParse = endAt;
			endParse   = ForwardOneContext(buf, context, std::max(endAt, std::max(LastModified(styleBuf), lastMod)));
			if (IsPlain(parseInStyle)) {
				qCritical("NEdit: internal error: incr. reparse fell short");
				return;
			}
			parseInStyle = ParentStyleOf(parentStyles, parseInStyle);

			// One context distance beyond last style changed means we're done
		} else if (LastModified(styleBuf) <= lastMod) {
			return;

			/* Styles are changing beyond the modification, continue extending
			   the end of the parse range by powers of 2 * ReparseChunkSize and
			   reparse until nothing changes */
		} else {
			lastMod  = LastModified(styleBuf);
			endParse = std::min(buf->BufEndOfBuffer(), ForwardOneContext(buf, context, lastMod) + (ReparseChunkSize << nPasses));
		}
	}
}

/**
 * @brief Read a highlight pattern from the input stream.
 *
 * @param in The input stream to read from.
 * @param errMsg Where to store error messages.
 * @param pattern The HighlightPattern structure to fill with the read data.
 * @return Returns `true` if the pattern was read successfully, `false` otherwise.
 */
bool ReadHighlightPattern(Input &in, QString *errMsg, HighlightPattern *pattern) {

	// read the name field
	QString name = Preferences::ReadSymbolicField(in);
	if (name.isNull()) {
		*errMsg = tr("pattern name is required");
		return false;
	}
	pattern->name = std::move(name);

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

/**
 * @brief Parse a set of highlight patterns. If unsuccessful, places an error message
 * in `errMsg` and returns an empty optional.
 *
 * @param in The input stream to read from.
 * @param errMsg Where to store error messages.
 * @return The HighlightPatterns if successful, or an empty optional if there was an error.
 */
std::optional<std::vector<HighlightPattern>> ReadHighlightPatterns(Input &in, QString *errMsg) {
	// skip over blank space
	in.skipWhitespaceNL();

	// look for initial brace
	if (*in != QLatin1Char('{')) {
		*errMsg = tr("pattern list must begin with \"{\"");
		return {};
	}
	++in;

	// parse each pattern in the list
	std::vector<HighlightPattern> ret;

	while (true) {
		in.skipWhitespaceNL();
		if (in.atEnd()) {
			*errMsg = tr("end of pattern list not found");
			return {};
		}

		if (*in == QLatin1Char('}')) {
			++in;
			break;
		}

		HighlightPattern pat;
		if (!ReadHighlightPattern(in, errMsg, &pat)) {
			return {};
		}

		ret.push_back(pat);
	}

	return ret;
}

/**
 * @brief Read a highlight pattern from a YAML node.
 *
 * @param patterns The YAML node containing the pattern data.
 * @return The HighlightPattern read from the YAML node.
 */
HighlightPattern ReadPatternYaml(const YAML::Node &patterns) {
	HighlightPattern pattern;
	pattern.flags = 0;

	for (auto it = patterns.begin(); it != patterns.end(); ++it) {

		const std::string key  = it->first.as<std::string>();
		const YAML::Node value = it->second;

		if (key == "name") {
			pattern.name = value.as<QString>();
		} else if (key == "style") {
			pattern.style = value.as<QString>();
		} else if (key == "regex_start") {
			pattern.startRE = value.as<QString>();
		} else if (key == "regex_end") {
			pattern.endRE = value.as<QString>();
		} else if (key == "regex_error") {
			pattern.errorRE = value.as<QString>();
		} else if (key == "parent") {
			pattern.subPatternOf = value.as<QString>();
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
 * @brief Reqads a pattern set from a YAML iterator.
 *
 * @param it The YAML iterator pointing to the pattern set data.
 * @return The PatternSet if successful, or an empty optional if there was an error.
 */
std::optional<PatternSet> ReadPatternSetYaml(YAML::const_iterator it) {
	struct HighlightError {
		QString message;
	};

	try {
		PatternSet patternSet;
		patternSet.languageMode = it->first.as<QString>();
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

						HighlightPattern pattern = ReadPatternYaml(entry);

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
			return ReadDefaultPatternSet(patternSet.languageMode);
		}

		return patternSet;
	} catch (const YAML::Exception &ex) {
		qWarning("NEdit: Invalid YAML:\n%s", ex.what());
	} catch (const HighlightError &ex) {
		qWarning("NEdit: %s", qPrintable(ex.message));
	}

	return {};
}

/**
 * @brief Read a pattern set from the input stream.
 *
 * @param in The input stream to read from.
 * @return The PatternSet if successful, or an empty optional if there was an error.
 */
std::optional<PatternSet> ReadPatternSet(Input &in) {

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
		if (in.match(QStringLiteral("Default"))) {
			std::optional<PatternSet> retPatSet = ReadDefaultPatternSet(patSet.languageMode);
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
		std::optional<std::vector<HighlightPattern>> patterns = ReadHighlightPatterns(in, &errMsg);
		if (!patterns) {
			Raise<HighlightError>(errMsg);
		}

		patSet.patterns = std::move(*patterns);

		// pattern set was read correctly, make an allocated copy to return
		return patSet;
	} catch (const HighlightError &error) {
		Preferences::ReportError(
			nullptr,
			*in.string(),
			in.index(),
			tr("highlight pattern"),
			error.message);

		return {};
	}
}

/**
 * @brief Find a sub-pattern in a highlight pattern by its index.
 *
 * @param pattern The highlight pattern to search in.
 * @param index The index of the sub-pattern to find. This index is relative to the
 *              sub-patterns of the main pattern, skipping over color-only sub-patterns.
 * @return The sub-pattern found at the specified index, or `nullptr` if no such sub-pattern exists.
 */
HighlightData *FindSubPattern(const HighlightData *pattern, size_t index) {

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
 * @brief Read the default pattern sets from a resource file.
 *
 * @return A vector of PatternSet objects containing the default patterns.
 */
std::vector<PatternSet> ReadDefaultPatternSets() {
	static QByteArray defaultPatternSets = LoadResource(QStringLiteral("DefaultPatternSets.yaml"));
	static YAML::Node patternSets        = YAML::Load(defaultPatternSets.data());

	std::vector<PatternSet> defaultPatterns;

	for (auto it = patternSets.begin(); it != patternSets.end(); ++it) {
		if (std::optional<PatternSet> patSet = ReadPatternSetYaml(it)) {
			defaultPatterns.push_back(*patSet);
		}
	}

	return defaultPatterns;
}

}

/**
 * @brief Buffer modification callback for triggering re-parsing of modified
 * text and keeping the style buffer synchronized with the text buffer.
 * This must be attached to the the text buffer BEFORE any widget text
 * display callbacks, so it can get the style buffer ready to be used
 * by the text display routines.
 *
 * Update the style buffer for changes to the text, and mark any style
 * changes by selecting the region in the style buffer. This strange
 * protocol of informing the text display to redraw style changes by
 * making selections in the style buffer is used because this routine
 * is intended to be called BEFORE the text display callback paints the
 * text (to minimize redraws and, most importantly, to synchronize the
 * style buffer with the text buffer). If we redraw now, the text
 * display hasn't yet processed the modification, redrawing later is
 * not only complicated, it will double-draw almost everything typed.
 *
 *
 * @param pos The position in the text buffer where the modification occurred.
 * @param nInserted The number of characters inserted at `pos`.
 * @param nDeleted The number of characters deleted at `pos`.
 * @param nRestyled The number of characters restyled at `pos`.
 * @param deletedText The text that was deleted at `pos`.
 * @param user The DocumentWidget that owns the text buffer being modified.
 *
 * @note This function must be kept efficient, as it is called for every
 * character typed in the text buffer.
 */
void SyntaxHighlightModifyCallback(TextCursor pos, int64_t nInserted, int64_t nDeleted, int64_t nRestyled, std::string_view deletedText, void *user) {

	Q_UNUSED(nRestyled)
	Q_UNUSED(deletedText)
	Q_ASSERT(user);

	auto document = static_cast<DocumentWidget *>(user);

	const std::unique_ptr<WindowHighlightData> &highlightData = document->highlightData_;
	if (!highlightData) {
		return;
	}

	const std::shared_ptr<UTextBuffer> &styleBuffer = highlightData->styleBuffer;

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
		const std::basic_string<uint8_t> insStyle(static_cast<size_t>(nInserted), UNFINISHED_STYLE);
		styleBuffer->BufReplace(pos, pos + nDeleted, insStyle);
	} else {
		styleBuffer->BufRemove(pos, pos + nDeleted);
	}

	/* Mark the changed region in the style buffer as requiring redraw.  This
	   is not necessary for getting it redrawn, it will be redrawn anyhow by
	   the text display callback, but it clears the previous selection and
	   saves the ModifyStyleBuffer routine from unnecessary work in tracking
	   changes that are already scheduled for redraw */
	styleBuffer->BufSelect(pos, pos + nInserted);

	// Re-parse around the changed region
	if (highlightData->pass1Patterns) {
		IncrementalReparse(highlightData, document->buffer(), pos, nInserted);
	}
}

/**
 * @brief Parses `string_ptr` according to compiled regular expressions in `pattern`
 * until endRE is or errorRE are matched, or end of string is reached.
 * Advances `string_ptr`, `style_ptr` pointers to the next character past
 * the end of the parsed section.
 *
 * @param pattern The highlight pattern to parse.
 * @param string_ptr The current position in the string being parsed.
 * @param style_ptr The current position in the style buffer.
 * @param length The length of the string to parse, starting from `string_ptr`.
 * @param ctx The parse context containing the text and previous character.
 * @param look_behind_to The position in the string where look-behind patterns may look back.
 * @param match_to The position in the string where matches may extend to. If `nullptr`, it is assumed that the end of the string is the boundary.
 * @return `true` if parsing was done and the parse succeeded. `false` if
 * the error pattern matched, if the end of the string was reached without
 * matching the end expression, or in the unlikely event of an internal error.
 */
bool ParseString(const HighlightData *pattern, const char *&string_ptr, uint8_t *&style_ptr, int64_t length, const ParseContext *ctx, const char *look_behind_to, const char *match_to) {

	if (length <= 0) {
		return false;
	}

	const char *const begin_ptr = ctx->text.data();
	const char *const end_ptr   = ctx->text.data() + ctx->text.size();

	if (!look_behind_to) {
		look_behind_to = begin_ptr;
	}

	if (!match_to) {
		match_to = end_ptr;
	}

	bool subExecuted;
	const int next_char = (match_to != end_ptr) ? (*match_to) : -1;

	const char *stringPtr = string_ptr;
	uint8_t *stylePtr     = style_ptr;

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
		end_ptr)) {

		/* Beware of the case where only one real branch exists, but that
		   branch has sub-branches itself. In that case the top_branch refers
		   to the matching sub-branch and must be ignored. */
		size_t subIndex = (pattern->nSubBranches > 1) ? subPatternRE->top_branch : 0;

		// Combination of all sub-patterns and end pattern matched
		const char *const startingStringPtr = stringPtr;

		/* Fill in the pattern style for the text that was skipped over before
		   the match, and advance the pointers to the start of the pattern */
		FillStyleString(stringPtr, stylePtr, subPatternRE->startp[0], pattern->style, ctx);

		/* If the combined pattern matched this pattern's end pattern, we're
		   done.  Fill in the style string, update the pointers, color the
		   end expression if there were coloring sub-patterns, and return */
		const char *const savedStartPtr = stringPtr;
		const int savedPrevChar         = *ctx->prev_char;

		if (pattern->endRE) {
			if (subIndex == 0) {
				FillStyleString(stringPtr, stylePtr, subPatternRE->endp[0], pattern->style, ctx);
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
									end_ptr)) {
								qCritical("NEdit: Internal error, failed to recover end match in ParseString");
								return false;
							}
							subExecuted = true;
						}

						for (const size_t subExpr : subPat->endSubexpressions) {
							RecolorSubexpression(pattern->endRE, subExpr, subPat->style, string_ptr, style_ptr);
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
				FillStyleString(stringPtr, stylePtr, subPatternRE->startp[0], pattern->style, ctx);
				string_ptr = stringPtr;
				style_ptr  = stylePtr;
				return false;
			}
			--subIndex;
		}

		HighlightData *subPat = FindSubPattern(pattern, subIndex);
		Q_ASSERT(subPat);

		// the sub-pattern is a simple match, just color it
		if (!subPat->subPatternRE) {
			FillStyleString(stringPtr, stylePtr, subPatternRE->endp[0], /* subPat->startRE->endp[0],*/ subPat->style, ctx);

			// Parse the remainder of the sub-pattern
		} else if (subPat->endRE) {
			/* The pattern is a starting/ending type of pattern, proceed with
			   the regular hierarchical parsing. */

			/* If parsing should start after the start pattern, advance
			   to that point (this is currently always the case) */
			if (!(subPat->flags & PARSE_SUBPATS_FROM_START)) {
				FillStyleString(
					stringPtr,
					stylePtr,
					subPatternRE->endp[0], // subPat->startRE->endp[0],
					subPat->style,
					ctx);
			}

			// Parse to the end of the subPattern
			ParseString(
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
			ParseString(
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
							end_ptr)) {
						qCritical("NEdit: Internal error, failed to recover start match in ParseString");
						return false;
					}
					subExecuted = true;
				}

				for (const size_t subExpr : subSubPat->startSubexpressions) {
					RecolorSubexpression(subPat->startRE, subExpr, subSubPat->style, string_ptr, style_ptr);
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

			FillStyleString(stringPtr, stylePtr, stringPtr + 1, pattern->style, ctx);
		}
	}

	// Reached end of string, fill in the remaining text with pattern style
	FillStyleString(stringPtr, stylePtr, string_ptr + length, pattern->style, ctx);

	// Advance the string and style pointers to the end of the parsed text
	string_ptr = stringPtr;
	style_ptr  = stylePtr;
	return pattern->endRE == nullptr;
}

/**
 * @brief Get the character before position "pos" in buffer "buf"
 *
 * @param buf The text buffer to get the character from.
 * @param pos The position in the buffer from which to get the previous character.
 * @return The character before "pos" in the buffer, or -1 if "pos" is at the start of the buffer.
 */
int GetPrevChar(TextBuffer *buf, TextCursor pos) {
	return pos == buf->BufStartOfBuffer() ? -1 : buf->BufGetCharacter(pos - 1);
}

/**
 * @brief Calculates a position far enough back in `buf` from `fromPos` to give patterns
 * their guaranteed amount of context for matching (from `context`). If
 * backing up by lines yields the greater distance, the returned position will
 * be to the newline character before the start of the line, rather than to
 * the first character of the line. (I did this because earlier prototypes of
 * the syntax highlighting code, which were based on single-line context, used
 * this to ensure that line-spanning expressions would be detected. I think
 * it may reduce some 2 line context requirements to one line, at a cost of
 * only one extra character, but I'm not sure, and my brain hurts from
 * thinking about it).
 *
 * @param buf The text buffer to search in.
 * @param context The reparse context containing the number of lines and characters required.
 * @param fromPos The position in the buffer from which to start searching.
 * @return A position in the buffer that is far enough back to ensure the required context for matching.
 */
TextCursor BackwardOneContext(TextBuffer *buf, const ReparseContext &context, TextCursor fromPos) {

	const TextCursor begin = buf->BufStartOfBuffer();

	if (context.nLines == 0) {
		return std::max(begin, fromPos - context.nChars);
	}

	if (context.nChars == 0) {
		return std::max(begin, buf->BufCountBackwardNLines(fromPos, context.nLines - 1) - 1);
	}

	return std::max(begin, std::min(std::max(begin, buf->BufCountBackwardNLines(fromPos, context.nLines - 1) - 1), fromPos - context.nChars));
}

/**
 * @brief Get a position far enough forward in `buf` from `fromPos` to ensure
 * that patterns are given their required amount of context for matching
 * (from `context`). If moving forward by lines yields the greater distance,
 * the returned position will be the first character of the next line, rather
 * than the newline character at the end (see notes in BackwardOneContext).
 *
 * @param buf The text buffer to search in.
 * @param context The reparse context containing the number of lines and characters required.
 * @param fromPos The position in the buffer from which to start searching.
 * @return A position in the buffer that is far enough forward to ensure the required context for matching.
 */
TextCursor ForwardOneContext(TextBuffer *buf, const ReparseContext &context, TextCursor fromPos) {

	const TextCursor end = buf->BufEndOfBuffer();

	if (context.nLines == 0) {
		return std::min(end, fromPos + context.nChars);
	}

	if (context.nChars == 0) {
		return std::min(end, buf->BufCountForwardNLines(fromPos, context.nLines));
	}

	return std::min(end, std::max(buf->BufCountForwardNLines(fromPos, context.nLines), fromPos + context.nChars));
}

/**
 * @brief Search for a pattern in pattern list `patterns` with style `style`.
 *
 * @param patterns The HighlightData patterns to search in.
 * @param style The style to search for in the patterns.
 * @return The HighlightData pattern with the specified style, or nullptr if no such pattern exists.
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
 * @brief Find the index of a named pattern in the list of patterns.
 *
 * @param patterns The HighlightPatterns to search in.
 * @param name The name of the pattern to find.
 * @return The index of the pattern with the specified name, or PATTERN_NOT_FOUND if no such pattern exists.
 */
size_t IndexOfNamedPattern(const std::vector<HighlightPattern> &patterns, const QString &name) {

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
 * @brief Find the top-level parent index of a pattern in the list of patterns.
 *
 * @param patterns The HighlightPatterns to search in.
 * @param index The index of the pattern whose top-level parent index is to be found.
 * @return The index of the top-level parent pattern, or PATTERN_NOT_FOUND if the pattern has no parent.
 */
size_t FindTopLevelParentIndex(const std::vector<HighlightPattern> &patterns, size_t index) {

	size_t topIndex = index;
	while (!patterns[topIndex].subPatternOf.isNull()) {
		topIndex = IndexOfNamedPattern(patterns, patterns[topIndex].subPatternOf);
		if (index == topIndex) {
			return PATTERN_NOT_FOUND; // amai: circular dependency ?!
		}
	}
	return topIndex;
}

/**
 * @brief Read a string representing highlight pattern sets and add them
 * to the PatternSets list of loaded highlight patterns.
 *
 * @param string The string containing the highlight pattern sets to load.
 * If the string is "*", all highlight patterns from the default pattern
 * sets file are loaded.
 *
 * @note the patterns themselves are not parsed until they are actually used.
 */
void LoadHighlightString(const QString &string) {

	if (string == QStringLiteral("*")) {

		YAML::Node patternSets;

		const QString HighlightPatternsFile = Settings::HighlightPatternsFile();
		if (QFileInfo::exists(HighlightPatternsFile)) {
			patternSets = YAML::LoadFile(HighlightPatternsFile.toUtf8().data());
		} else {
			static QByteArray defaultPatternSets = LoadResource(QStringLiteral("DefaultPatternSets.yaml"));
			patternSets                          = YAML::Load(defaultPatternSets.data());
		}

		for (auto it = patternSets.begin(); it != patternSets.end(); ++it) {
			// Read each pattern set, abort on error
			std::optional<PatternSet> patSet = ReadPatternSetYaml(it);
			if (!patSet) {
				break;
			}

			// Add/change the pattern set in the list
			Upsert(PatternSets, *patSet, [&patSet](const PatternSet &patternSet) {
				return patternSet.languageMode == patSet->languageMode;
			});
		}
	} else {
		Input in(&string);

		Q_FOREVER {

			// Read each pattern set, abort on error
			std::optional<PatternSet> patSet = ReadPatternSet(in);
			if (!patSet) {
				break;
			}

			// Add/change the pattern set in the list
			Upsert(PatternSets, *patSet, [&patSet](const PatternSet &patternSet) {
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

/**
 * @brief Create a string in the correct format for the highlightPatterns resource,
 * containing all of the highlight pattern information from the stored
 * highlight pattern list (PatternSets) for this session.
 *
 * @return A string containing the highlight patterns in the correct format, or an empty string if there was an error.
 */
QString WriteHighlightString() {

	const QString filename = Settings::HighlightPatternsFile();

	try {
		YAML::Emitter out;
		out << YAML::BeginMap;
		for (const PatternSet &patternSet : PatternSets) {
			if (IsDefaultPatternSet(patternSet)) {
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

		return QStringLiteral("*");
	} catch (const YAML::Exception &ex) {
		qWarning("NEdit: Error writing %s in config directory:\n%s", qPrintable(filename), ex.what());
	}

	return QString();
}

/**
 * @brief Check if the font of a named style is bold.
 *
 * @param styleName The name of the style to check.
 * @return `true` if the font is bold, `false` otherwise.
 */
bool FontOfNamedStyleIsBold(const QString &styleName) {
	const size_t styleNo = IndexOfNamedStyle(styleName);

	if (styleNo == StyleNotFound) {
		return false;
	}

	const int fontNum = HighlightStyles[styleNo].font;
	return (fontNum & Font::Bold);
}

/**
 * @brief Check if the font of a named style is italic.
 *
 * @param styleName The name of the style to check.
 * @return `true` if the font is italic, `false` otherwise.
 */
bool FontOfNamedStyleIsItalic(const QString &styleName) {
	const size_t styleNo = IndexOfNamedStyle(styleName);

	if (styleNo == StyleNotFound) {
		return false;
	}

	const int fontNum = HighlightStyles[styleNo].font;
	return (fontNum & Font::Italic);
}

/**
 * @brief Find the foreground color associated with a named style.
 *
 * @param styleName The name of the style to find the foreground color for.
 * @return The foreground color of the named style, or "black" if the style is not found.
 */
QString FgColorOfNamedStyle(const QString &styleName) {
	const size_t styleNo = IndexOfNamedStyle(styleName);

	if (styleNo == StyleNotFound) {
		return QStringLiteral("black");
	}

	return HighlightStyles[styleNo].color;
}

/**
 * @brief Find the background color associated with a named style.
 *
 * @param styleName The name of the style to find the background color for.
 * @return The background color of the named style, or an empty string if the style is not found.
 */
QString BgColorOfNamedStyle(const QString &styleName) {
	const size_t styleNo = IndexOfNamedStyle(styleName);

	if (styleNo == StyleNotFound) {
		return QStringLiteral("");
	}

	return HighlightStyles[styleNo].bgColor;
}

/**
 * @brief Check if a named style exists in the HighlightStyles list.
 *
 * @param styleName The name of the style to check for existence.
 * @return `true` if the style exists, `false` otherwise.
 */
bool NamedStyleExists(const QString &styleName) {
	return IndexOfNamedStyle(styleName) != StyleNotFound;
}

/**
 * @brief Look through the list of pattern sets, and find the one for a particular language.
 *
 * @param languageMode The language mode name to search for in the pattern sets.
 * @return The PatternSet for the specified language mode, or `nullptr` if no such pattern set exists.
 */
PatternSet *FindPatternSet(const QString &languageMode) {

	for (size_t i = 0; i < PatternSets.size(); i++) {
		if (PatternSets[i].languageMode == languageMode) {
			return &PatternSets[i];
		}
	}

	return nullptr;
}

/**
 * @brief Given a language mode name, determine if there is a default (built-in)
 * pattern set available for that language mode, and if so, return it
 *
 * @param langModeName The name of the language mode to search for in the default pattern sets.
 * @return The PatternSet for the specified language mode if it exists, or an empty optional if no such pattern set exists.
 */
std::optional<PatternSet> ReadDefaultPatternSet(const QString &langModeName) {

	static const std::vector<PatternSet> defaultPatternSets = ReadDefaultPatternSets();

	auto it = std::find_if(defaultPatternSets.begin(), defaultPatternSets.end(), [&langModeName](const PatternSet &patternSet) {
		return langModeName == patternSet.languageMode;
	});

	if (it != defaultPatternSets.end()) {
		return *it;
	}

	return {};
}

/**
 * @brief Find the index of a named style in the HighlightStyles list.
 *
 * @param styleName The name of the style to find the index for.
 * @return The index of the style in the HighlightStyles list, or StyleNotFound if the style does not exist.
 */
size_t IndexOfNamedStyle(const QString &styleName) {
	for (size_t i = 0; i < HighlightStyles.size(); i++) {
		if (HighlightStyles[i].name == styleName) {
			return i;
		}
	}

	return StyleNotFound;
}

/**
 * @brief Rename a highlight pattern set from `oldName` to `newName`.
 *
 * @param oldName The old name of the highlight pattern set.
 * @param newName The new name to assign to the highlight pattern set.
 */
void RenameHighlightPattern(const QString &oldName, const QString &newName) {

	for (PatternSet &patternSet : Highlight::PatternSets) {
		if (patternSet.languageMode == oldName) {
			patternSet.languageMode = newName;
		}
	}
}

}
