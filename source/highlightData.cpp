/*******************************************************************************
*                                                                              *
* highlightData.c -- Maintain, and allow user to edit, highlight pattern list  *
*                    used for syntax highlighting                              *
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
* April, 1997                                                                  *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/

#include "highlightData.h"
#include "TextBuffer.h"
#include "nedit.h"
#include "highlight.h"
#include "regularExp.h"
#include "preferences.h"
#include "help.h"
#include "window.h"
#include "WindowInfo.h"
#include "regexConvert.h"
#include "MotifHelper.h"
#include "highlightStyleRec.h"
#include "../util/misc.h"
#include "../util/DialogF.h"
#include "../util/managedList.h"

#include <cstdio>
#include <cstring>
#include <climits>
#include <sys/param.h>
#include <memory>

#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/Text.h>
#include <Xm/LabelG.h>
#include <Xm/PushB.h>
#include <Xm/ToggleB.h>
#include <Xm/RowColumn.h>
#include <Xm/SeparatoG.h>

/* Maximum allowed number of styles (also limited by representation of
   styles as a byte - 'b') */
#define MAX_HIGHLIGHT_STYLES 128

/* Maximum number of patterns allowed in a pattern set (regular expression
   limitations are probably much more restrictive).  */
#define MAX_PATTERNS 127

/* Names for the fonts that can be used for syntax highlighting */
#define N_FONT_TYPES 4
enum fontTypes { PLAIN_FONT, ITALIC_FONT, BOLD_FONT, BOLD_ITALIC_FONT };
static const char *FontTypeNames[N_FONT_TYPES] = {"Plain", "Italic", "Bold", "Bold Italic"};

static bool styleError(const char *stringStart, const char *stoppedAt, const char *message);
#if 0
static int lookupNamedPattern(patternSet *p, char *patternName);
#endif
static int lookupNamedStyle(const char *styleName);
static highlightPattern *readHighlightPatterns(const char **inPtr, int withBraces, const char **errMsg, int *nPatterns);
static int readHighlightPattern(const char **inPtr, const char **errMsg, highlightPattern *pattern);
static patternSet *readDefaultPatternSet(const char *langModeName);
static int isDefaultPatternSet(patternSet *patSet);
static patternSet *readPatternSet(const char **inPtr, int convertOld);
static patternSet *highlightError(const char *stringStart, const char *stoppedAt, const char *message);
static std::string createPatternsString(patternSet *patSet, const char *indentStr);
static void setStyleByName(const char *style);
static void hsDestroyCB(Widget w, XtPointer clientData, XtPointer callData);
static void hsOkCB(Widget w, XtPointer clientData, XtPointer callData);
static void hsApplyCB(Widget w, XtPointer clientData, XtPointer callData);
static void hsCloseCB(Widget w, XtPointer clientData, XtPointer callData);
static highlightStyleRec *copyHighlightStyleRec(highlightStyleRec *hs);
static void *hsGetDisplayedCB(void *oldItem, int explicitRequest, int *abort, void *cbArg);
static void hsSetDisplayedCB(void *item, void *cbArg);
static highlightStyleRec *readHSDialogFields(int silent);
static void hsFreeItemCB(void *item);
static void freeHighlightStyleRec(highlightStyleRec *hs);
static int hsDialogEmpty(void);
static int updateHSList(void);
static void updateHighlightStyleMenu(void);
static void convertOldPatternSet(patternSet *patSet);
static void convertPatternExpr(char **patternRE, const char *patSetName, const char *patName, int isSubsExpr);
static Widget createHighlightStylesMenu(Widget parent);
static void destroyCB(Widget w, XtPointer clientData, XtPointer callData);
static void langModeCB(Widget w, XtPointer clientData, XtPointer callData);
static void lmDialogCB(Widget w, XtPointer clientData, XtPointer callData);
static void styleDialogCB(Widget w, XtPointer clientData, XtPointer callData);
static void patTypeCB(Widget w, XtPointer clientData, XtPointer callData);
static void matchTypeCB(Widget w, XtPointer clientData, XtPointer callData);
static int checkHighlightDialogData(void);
static void updateLabels(void);
static void okCB(Widget w, XtPointer clientData, XtPointer callData);
static void applyCB(Widget w, XtPointer clientData, XtPointer callData);
static void checkCB(Widget w, XtPointer clientData, XtPointer callData);
static void restoreCB(Widget w, XtPointer clientData, XtPointer callData);
static void deleteCB(Widget w, XtPointer clientData, XtPointer callData);
static void closeCB(Widget w, XtPointer clientData, XtPointer callData);
static void helpCB(Widget w, XtPointer clientData, XtPointer callData);
static void *getDisplayedCB(void *oldItem, int explicitRequest, int *abort, void *cbArg);
static void setDisplayedCB(void *item, void *cbArg);
static void setStyleMenu(const char *styleName);
static highlightPattern *readDialogFields(int silent);
static int dialogEmpty(void);
static int updatePatternSet(void);
static patternSet *getDialogPatternSet(void);
static int patternSetsDiffer(patternSet *patSet1, patternSet *patSet2);
static highlightPattern *copyPatternSrc(highlightPattern *pat, highlightPattern *copyTo);
static void freeItemCB(void *item);
static void freePatternSrc(highlightPattern *pat, bool freeStruct);
static void freePatternSet(patternSet *p);

/* list of available highlight styles */
static int NHighlightStyles = 0;
static highlightStyleRec *HighlightStyles[MAX_HIGHLIGHT_STYLES];

/* Highlight styles dialog information */
static struct {
	Widget shell;
	Widget nameW;
	Widget colorW;
	Widget bgColorW;
	Widget plainW, boldW, italicW, boldItalicW;
	Widget managedListW;
	highlightStyleRec **highlightStyleList;
	int nHighlightStyles;
} HSDialog = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0};

/* Highlight dialog information */
static struct {
	Widget shell;
	Widget lmOptMenu;
	Widget lmPulldown;
	Widget styleOptMenu;
	Widget stylePulldown;
	Widget nameW;
	Widget topLevelW;
	Widget deferredW;
	Widget subPatW;
	Widget colorPatW;
	Widget simpleW;
	Widget rangeW;
	Widget parentW;
	Widget startW;
	Widget endW;
	Widget errorW;
	Widget lineContextW;
	Widget charContextW;
	Widget managedListW;
	Widget parentLbl;
	Widget startLbl;
	Widget endLbl;
	Widget errorLbl;
	Widget matchLbl;
	char *langModeName;
	int nPatterns;
	highlightPattern **patterns;
} HighlightDialog = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                     nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0,       nullptr};

/* Pattern sources loaded from the .nedit file or set by the user */
static int NPatternSets = 0;
static patternSet *PatternSets[MAX_LANGUAGE_MODES];

static const char *DefaultPatternSets[] = {
	#include "DefaultPatternSet00.inc"
	,
	#include "DefaultPatternSet01.inc"
	,
	#include "DefaultPatternSet02.inc"
	,
	#include "DefaultPatternSet03.inc"
	,
	#include "DefaultPatternSet04.inc"
	,
	#include "DefaultPatternSet05.inc"
	,
	#include "DefaultPatternSet06.inc"
	,
	#include "DefaultPatternSet07.inc"
	,
	#include "DefaultPatternSet08.inc"
	,
	#include "DefaultPatternSet09.inc"
	,
	#include "DefaultPatternSet10.inc"
	,
	#include "DefaultPatternSet11.inc"
	,
	#include "DefaultPatternSet12.inc"
	,
	#include "DefaultPatternSet13.inc"
	,
	#include "DefaultPatternSet14.inc"
	,
	#include "DefaultPatternSet15.inc"
	,
	#include "DefaultPatternSet16.inc"
	,
	#include "DefaultPatternSet17.inc"
	,
	#include "DefaultPatternSet18.inc"
	,
	#include "DefaultPatternSet19.inc"
	,
	#include "DefaultPatternSet20.inc"
	,
	#include "DefaultPatternSet21.inc"
	,
	#include "DefaultPatternSet22.inc"
	,
	#include "DefaultPatternSet23.inc"
	,
	#include "DefaultPatternSet24.inc"
	,
	#include "DefaultPatternSet25.inc"
	,
	#include "DefaultPatternSet26.inc"
	,
	#include "DefaultPatternSet27.inc"
};

/*
** Read a string (from the  value of the styles resource) containing highlight
** styles information, parse it, and load it into the stored highlight style
** list (HighlightStyles) for this NEdit session.
*/
bool LoadStylesString(const char *inString) {
	const char *errMsg;
	char *fontStr;
	const char *inPtr = inString;
	int i;

	for (;;) {

		/* skip over blank space */
		inPtr += strspn(inPtr, " \t");

		/* Allocate a language mode structure in which to store the info. */
		auto hs = new highlightStyleRec;

		/* read style name */
		char *name = ReadSymbolicField(&inPtr);
		if (name == nullptr) {
			return styleError(inString, inPtr, "style name required");
		}
		
		hs->name = name;
		XtFree(name);
		
		if (!SkipDelimiter(&inPtr, &errMsg)) {
			delete hs;
			return styleError(inString, inPtr, errMsg);
		}

		/* read color */
		hs->color = ReadSymbolicField(&inPtr);
		if (hs->color == nullptr) {
			delete hs;
			return styleError(inString, inPtr, "color name required");
		}
		hs->bgColor = nullptr;
		if (SkipOptSeparator('/', &inPtr)) {
			/* read bgColor */
			hs->bgColor = ReadSymbolicField(&inPtr); /* no error if fails */
		}
		if (!SkipDelimiter(&inPtr, &errMsg)) {
			freeHighlightStyleRec(hs);
			return styleError(inString, inPtr, errMsg);
		}

		/* read the font type */
		fontStr = ReadSymbolicField(&inPtr);
		for (i = 0; i < N_FONT_TYPES; i++) {
			if (!strcmp(FontTypeNames[i], fontStr)) {
				hs->font = i;
				break;
			}
		}
		if (i == N_FONT_TYPES) {
			XtFree(fontStr);
			freeHighlightStyleRec(hs);
			return styleError(inString, inPtr, "unrecognized font type");
		}
		XtFree(fontStr);

		/* pattern set was read correctly, add/change it in the list */
		for (i = 0; i < NHighlightStyles; i++) {
			if (hs->name == HighlightStyles[i]->name) {
				freeHighlightStyleRec(HighlightStyles[i]);
				HighlightStyles[i] = hs;
				break;
			}
		}
		if (i == NHighlightStyles) {
			HighlightStyles[NHighlightStyles++] = hs;
			if (NHighlightStyles > MAX_HIGHLIGHT_STYLES)
				return styleError(inString, inPtr, "maximum allowable number of styles exceeded");
		}

		/* if the string ends here, we're done */
		inPtr += strspn(inPtr, " \t\n");
		if (*inPtr == '\0')
			return true;
	}
}

/*
** Create a string in the correct format for the styles resource, containing
** all of the highlight styles information from the stored highlight style
** list (HighlightStyles) for this NEdit session.
*/
char *WriteStylesString(void) {
	int i;
	highlightStyleRec *style;

	auto outBuf = std::unique_ptr<TextBuffer>(new TextBuffer);

	for (i = 0; i < NHighlightStyles; i++) {
		style = HighlightStyles[i];
		outBuf->BufInsertEx(outBuf->BufGetLength(), "\t");
		outBuf->BufInsertEx(outBuf->BufGetLength(), style->name);
		outBuf->BufInsertEx(outBuf->BufGetLength(), ":");
		outBuf->BufInsertEx(outBuf->BufGetLength(), style->color);
		if (style->bgColor) {
			outBuf->BufInsertEx(outBuf->BufGetLength(), "/");
			outBuf->BufInsertEx(outBuf->BufGetLength(), style->bgColor);
		}
		outBuf->BufInsertEx(outBuf->BufGetLength(), ":");
		outBuf->BufInsertEx(outBuf->BufGetLength(), FontTypeNames[style->font]);
		outBuf->BufInsertEx(outBuf->BufGetLength(), "\\n\\\n");
	}

	/* Get the output, and lop off the trailing newlines */
	return outBuf->BufGetRange(0, outBuf->BufGetLength() - (i == 1 ? 0 : 4));
}

/*
** Create a string in the correct format for the styles resource, containing
** all of the highlight styles information from the stored highlight style
** list (HighlightStyles) for this NEdit session.
*/
std::string WriteStylesStringEx(void) {
	int i;
	highlightStyleRec *style;

	auto outBuf = std::unique_ptr<TextBuffer>(new TextBuffer);

	for (i = 0; i < NHighlightStyles; i++) {
		style = HighlightStyles[i];
		outBuf->BufInsertEx(outBuf->BufGetLength(), "\t");
		outBuf->BufInsertEx(outBuf->BufGetLength(), style->name);
		outBuf->BufInsertEx(outBuf->BufGetLength(), ":");
		outBuf->BufInsertEx(outBuf->BufGetLength(), style->color);
		if (style->bgColor) {
			outBuf->BufInsertEx(outBuf->BufGetLength(), "/");
			outBuf->BufInsertEx(outBuf->BufGetLength(), style->bgColor);
		}
		outBuf->BufInsertEx(outBuf->BufGetLength(), ":");
		outBuf->BufInsertEx(outBuf->BufGetLength(), FontTypeNames[style->font]);
		outBuf->BufInsertEx(outBuf->BufGetLength(), "\\n\\\n");
	}

	/* Get the output, and lop off the trailing newlines */
	return outBuf->BufGetRangeEx(0, outBuf->BufGetLength() - (i == 1 ? 0 : 4));
}

/*
** Read a string representing highlight pattern sets and add them
** to the PatternSets list of loaded highlight patterns.  Note that the
** patterns themselves are not parsed until they are actually used.
**
** The argument convertOld, reads patterns in pre 5.1 format (which means
** that they may contain regular expressions are of the older syntax where
** braces were not quoted, and \0 was a legal substitution character).
*/
bool LoadHighlightString(const char *inString, int convertOld) {
	const char *inPtr = inString;
	int i;

	for (;;) {

		/* Read each pattern set, abort on error */
		patternSet *patSet = readPatternSet(&inPtr, convertOld);
		if (patSet == nullptr)
			return false;

		/* Add/change the pattern set in the list */
		for (i = 0; i < NPatternSets; i++) {
			if (!strcmp(PatternSets[i]->languageMode, patSet->languageMode)) {
				freePatternSet(PatternSets[i]);
				PatternSets[i] = patSet;
				break;
			}
		}
		if (i == NPatternSets) {
			PatternSets[NPatternSets++] = patSet;
			if (NPatternSets > MAX_LANGUAGE_MODES)
				return false;
		}

		/* if the string ends here, we're done */
		inPtr += strspn(inPtr, " \t\n");
		if (*inPtr == '\0')
			return true;
	}
}

/*
** Create a string in the correct format for the highlightPatterns resource,
** containing all of the highlight pattern information from the stored
** highlight pattern list (PatternSets) for this NEdit session.
*/
char *WriteHighlightString(void) {

	bool written = false;
	auto outBuf = std::unique_ptr<TextBuffer>(new TextBuffer);

	for (int psn = 0; psn < NPatternSets; psn++) {
		patternSet *patSet = PatternSets[psn];
		if (patSet->nPatterns == 0) {
			continue;
		}
		
		written = true;
		outBuf->BufInsertEx(outBuf->BufGetLength(), patSet->languageMode);
		outBuf->BufInsertEx(outBuf->BufGetLength(), ":");
		if (isDefaultPatternSet(patSet))
			outBuf->BufInsertEx(outBuf->BufGetLength(), "Default\n\t");
		else {
			outBuf->BufInsertEx(outBuf->BufGetLength(), std::to_string(patSet->lineContext));
			outBuf->BufInsertEx(outBuf->BufGetLength(), ":");
			outBuf->BufInsertEx(outBuf->BufGetLength(), std::to_string(patSet->charContext));
			outBuf->BufInsertEx(outBuf->BufGetLength(), "{\n");
			outBuf->BufInsertEx(outBuf->BufGetLength(), createPatternsString(patSet, "\t\t"));
			outBuf->BufInsertEx(outBuf->BufGetLength(), "\t}\n\t");
		}
	}

	/* Get the output string, and lop off the trailing newline and tab */
	std::string outStr = outBuf->BufGetRangeEx(0, outBuf->BufGetLength() - (written ? 2 : 0));

	/* Protect newlines and backslashes from translation by the resource
	   reader */
	return EscapeSensitiveChars(outStr.c_str());
}

/*
** Create a string in the correct format for the highlightPatterns resource,
** containing all of the highlight pattern information from the stored
** highlight pattern list (PatternSets) for this NEdit session.
*/
std::string WriteHighlightStringEx(void) {

	bool written = false;
	auto outBuf = std::unique_ptr<TextBuffer>(new TextBuffer);

	for (int psn = 0; psn < NPatternSets; psn++) {
		patternSet *patSet = PatternSets[psn];
		if (patSet->nPatterns == 0) {
			continue;
		}
		
		written = true;
		
		outBuf->BufInsertEx(outBuf->BufGetLength(), patSet->languageMode);
		outBuf->BufInsertEx(outBuf->BufGetLength(), ":");
		if (isDefaultPatternSet(patSet))
			outBuf->BufInsertEx(outBuf->BufGetLength(), "Default\n\t");
		else {
			outBuf->BufInsertEx(outBuf->BufGetLength(), std::to_string(patSet->lineContext));
			outBuf->BufInsertEx(outBuf->BufGetLength(), ":");
			outBuf->BufInsertEx(outBuf->BufGetLength(), std::to_string(patSet->charContext));
			outBuf->BufInsertEx(outBuf->BufGetLength(), "{\n");
			outBuf->BufInsertEx(outBuf->BufGetLength(), createPatternsString(patSet, "\t\t"));
			outBuf->BufInsertEx(outBuf->BufGetLength(), "\t}\n\t");
		}
	}

	/* Get the output string, and lop off the trailing newline and tab */
	std::string outStr = outBuf->BufGetRangeEx(0, outBuf->BufGetLength() - (written ? 2 : 0));

	/* Protect newlines and backslashes from translation by the resource
	   reader */
	return EscapeSensitiveCharsEx(outStr);
}

/*
** Update regular expressions in stored pattern sets to version 5.1 regular
** expression syntax, in which braces and \0 have different meanings
*/
static void convertOldPatternSet(patternSet *patSet) {

	for (int p = 0; p < patSet->nPatterns; p++) {
		highlightPattern *pattern = &patSet->patterns[p];
		convertPatternExpr(&pattern->startRE, patSet->languageMode, pattern->name, pattern->flags & COLOR_ONLY);
		convertPatternExpr(&pattern->endRE, patSet->languageMode, pattern->name, pattern->flags & COLOR_ONLY);
		convertPatternExpr(&pattern->errorRE, patSet->languageMode, pattern->name, pattern->flags & COLOR_ONLY);
	}
}

/*
** Convert a single regular expression, patternRE, to version 5.1 regular
** expression syntax.  It will convert either a match expression or a
** substitution expression, which must be specified by the setting of
** isSubsExpr.  Error messages are directed to stderr, and include the
** pattern set name and pattern name as passed in patSetName and patName.
*/
static void convertPatternExpr(char **patternRE, const char *patSetName, const char *patName, int isSubsExpr) {
	char *newRE;
	const char *errorText;

	if (*patternRE == nullptr)
		return;
	if (isSubsExpr) {
		newRE = XtMalloc(strlen(*patternRE) + 5000);
		ConvertSubstituteRE(*patternRE, newRE, strlen(*patternRE) + 5000);
		XtFree(*patternRE);
		*patternRE = XtNewStringEx(newRE);
		XtFree(newRE);
	} else {
		newRE = ConvertRE(*patternRE, &errorText);
		if (newRE == nullptr) {
			fprintf(stderr, "NEdit error converting old format regular "
			                "expression in pattern set %s, pattern %s: %s\n",
			        patSetName, patName, errorText);
		}
		XtFree(*patternRE);
		*patternRE = newRE;
	}
}

/*
** Find the font (font struct) associated with a named style.
** This routine must only be called with a valid styleName (call
** NamedStyleExists to find out whether styleName is valid).
*/
XFontStruct *FontOfNamedStyle(WindowInfo *window, const char *styleName) {
	int styleNo = lookupNamedStyle(styleName), fontNum;
	XFontStruct *font;

	if (styleNo < 0)
		return GetDefaultFontStruct(window->fontList);
	fontNum = HighlightStyles[styleNo]->font;
	if (fontNum == BOLD_FONT)
		font = window->boldFontStruct;
	else if (fontNum == ITALIC_FONT)
		font = window->italicFontStruct;
	else if (fontNum == BOLD_ITALIC_FONT)
		font = window->boldItalicFontStruct;
	else /* fontNum == PLAIN_FONT */
		font = GetDefaultFontStruct(window->fontList);

	/* If font isn't loaded, silently substitute primary font */
	return font == nullptr ? GetDefaultFontStruct(window->fontList) : font;
}

int FontOfNamedStyleIsBold(const char *styleName) {
	int styleNo = lookupNamedStyle(styleName), fontNum;

	if (styleNo < 0)
		return 0;
	fontNum = HighlightStyles[styleNo]->font;
	return (fontNum == BOLD_FONT || fontNum == BOLD_ITALIC_FONT);
}

int FontOfNamedStyleIsItalic(const char *styleName) {
	int styleNo = lookupNamedStyle(styleName), fontNum;

	if (styleNo < 0)
		return 0;
	fontNum = HighlightStyles[styleNo]->font;
	return (fontNum == ITALIC_FONT || fontNum == BOLD_ITALIC_FONT);
}

/*
** Find the color associated with a named style.  This routine must only be
** called with a valid styleName (call NamedStyleExists to find out whether
** styleName is valid).
*/
std::string ColorOfNamedStyleEx(const char *styleName) {
	int styleNo = lookupNamedStyle(styleName);

	if (styleNo < 0) {
		return "black";
	}
		

	if(!HighlightStyles[styleNo]->color) {
		return std::string();
	}	

	return HighlightStyles[styleNo]->color;
}

/*
** Find the background color associated with a named style.
*/
std::string BgColorOfNamedStyleEx(const char *styleName) {
	int styleNo = lookupNamedStyle(styleName);

	if (styleNo < 0) {
		return "";
	}
	
	if(!HighlightStyles[styleNo]->bgColor) {
		return std::string();
	}	
		
	return HighlightStyles[styleNo]->bgColor;
}

/*
** Find the color associated with a named style.  This routine must only be
** called with a valid styleName (call NamedStyleExists to find out whether
** styleName is valid).
*/
const char *ColorOfNamedStyle(const char *styleName) {
	int styleNo = lookupNamedStyle(styleName);

	if (styleNo < 0)
		return "black";
	return HighlightStyles[styleNo]->color;
}

/*
** Find the background color associated with a named style.
*/
const char *BgColorOfNamedStyle(const char *styleName) {
	int styleNo = lookupNamedStyle(styleName);

	if (styleNo < 0)
		return "";
	return HighlightStyles[styleNo]->bgColor;
}

/*
** Determine whether a named style exists
*/
bool NamedStyleExists(const char *styleName) {
	return lookupNamedStyle(styleName) != -1;
}

/*
** Look through the list of pattern sets, and find the one for a particular
** language.  Returns nullptr if not found.
*/
patternSet *FindPatternSet(const char *langModeName) {
	int i;

	if (langModeName == nullptr) {
		return nullptr;
	}

	for (i = 0; i < NPatternSets; i++) {
		if (!strcmp(langModeName, PatternSets[i]->languageMode)) {
			return PatternSets[i];
		}
	}
	
	return nullptr;
}

/*
** Returns True if there are highlight patterns, or potential patterns
** not yet committed in the syntax highlighting dialog for a language mode,
*/
int LMHasHighlightPatterns(const char *languageMode) {
	if (FindPatternSet(languageMode) != nullptr)
		return True;
	return HighlightDialog.shell != nullptr && !strcmp(HighlightDialog.langModeName, languageMode) && HighlightDialog.nPatterns != 0;
}

/*
** Change the language mode name of pattern sets for language "oldName" to
** "newName" in both the stored patterns, and the pattern set currently being
** edited in the dialog.
*/
void RenameHighlightPattern(const char *oldName, const char *newName) {
	int i;

	for (i = 0; i < NPatternSets; i++) {
		if (!strcmp(oldName, PatternSets[i]->languageMode)) {
			XtFree((char *)PatternSets[i]->languageMode);
			PatternSets[i]->languageMode = XtNewStringEx(newName);
		}
	}
	if (HighlightDialog.shell != nullptr) {
		if (!strcmp(HighlightDialog.langModeName, oldName)) {
			XtFree(HighlightDialog.langModeName);
			HighlightDialog.langModeName = XtNewStringEx(newName);
		}
	}
}

/*
** Create a pulldown menu pane with the names of the current highlight styles.
** XmNuserData for each item contains a pointer to the name.
*/
static Widget createHighlightStylesMenu(Widget parent) {

	Widget menu = CreatePulldownMenu(parent, "highlightStyles", nullptr, 0);
	for (int i = 0; i < NHighlightStyles; i++) {
	
		XmString s1 = XmStringCreateSimpleEx(HighlightStyles[i]->name);
	
		XtVaCreateManagedWidget(
			"highlightStyles", 
			xmPushButtonWidgetClass, 
			menu, 
			XmNlabelString, 
			s1,
			XmNuserData, 
			HighlightStyles[i]->name.data(), // NOTE(eteran): is this safe? will it be invalidated at some point?
			nullptr);
			
		XmStringFree(s1);
	}
	return menu;
}

static std::string createPatternsString(patternSet *patSet, const char *indentStr) {
	char *str;

	auto outBuf = std::unique_ptr<TextBuffer>(new TextBuffer);

	for (int pn = 0; pn < patSet->nPatterns; pn++) {
		highlightPattern *pat = &patSet->patterns[pn];
		outBuf->BufInsert(outBuf->BufGetLength(), indentStr);
		outBuf->BufInsert(outBuf->BufGetLength(), pat->name);
		outBuf->BufInsert(outBuf->BufGetLength(), ":");
		if (pat->startRE != nullptr) {
			outBuf->BufInsert(outBuf->BufGetLength(), str = MakeQuotedString(pat->startRE));
			XtFree(str);
		}
		outBuf->BufInsert(outBuf->BufGetLength(), ":");
		if (pat->endRE != nullptr) {
			outBuf->BufInsert(outBuf->BufGetLength(), str = MakeQuotedString(pat->endRE));
			XtFree(str);
		}
		outBuf->BufInsert(outBuf->BufGetLength(), ":");
		if (pat->errorRE != nullptr) {
			outBuf->BufInsert(outBuf->BufGetLength(), str = MakeQuotedString(pat->errorRE));
			XtFree(str);
		}
		outBuf->BufInsert(outBuf->BufGetLength(), ":");
		outBuf->BufInsert(outBuf->BufGetLength(), pat->style);
		outBuf->BufInsert(outBuf->BufGetLength(), ":");
		if (pat->subPatternOf != nullptr)
			outBuf->BufInsert(outBuf->BufGetLength(), pat->subPatternOf);
		outBuf->BufInsert(outBuf->BufGetLength(), ":");
		if (pat->flags & DEFER_PARSING)
			outBuf->BufInsert(outBuf->BufGetLength(), "D");
		if (pat->flags & PARSE_SUBPATS_FROM_START)
			outBuf->BufInsert(outBuf->BufGetLength(), "R");
		if (pat->flags & COLOR_ONLY)
			outBuf->BufInsert(outBuf->BufGetLength(), "C");
		outBuf->BufInsert(outBuf->BufGetLength(), "\n");
	}
	return outBuf->BufGetAllEx();
}

/*
** Read in a pattern set character string, and advance *inPtr beyond it.
** Returns nullptr and outputs an error to stderr on failure.
*/
static patternSet *readPatternSet(const char **inPtr, int convertOld) {
	const char *errMsg;
	const char *stringStart = *inPtr;
	patternSet patSet;
	patternSet *retPatSet;

	/* remove leading whitespace */
	*inPtr += strspn(*inPtr, " \t\n");

	/* read language mode field */
	patSet.languageMode = ReadSymbolicField(inPtr);
	if (patSet.languageMode == nullptr)
		return highlightError(stringStart, *inPtr, "language mode must be specified");
	if (!SkipDelimiter(inPtr, &errMsg))
		return highlightError(stringStart, *inPtr, errMsg);

	/* look for "Default" keyword, and if it's there, return the default
	   pattern set */
	if (!strncmp(*inPtr, "Default", 7)) {
		*inPtr += 7;
		retPatSet = readDefaultPatternSet(patSet.languageMode);
		XtFree((char *)patSet.languageMode);
		if (retPatSet == nullptr)
			return highlightError(stringStart, *inPtr, "No default pattern set");
		return retPatSet;
	}

	/* read line context field */
	if (!ReadNumericField(inPtr, &patSet.lineContext))
		return highlightError(stringStart, *inPtr, "unreadable line context field");
	if (!SkipDelimiter(inPtr, &errMsg))
		return highlightError(stringStart, *inPtr, errMsg);

	/* read character context field */
	if (!ReadNumericField(inPtr, &patSet.charContext))
		return highlightError(stringStart, *inPtr, "unreadable character context field");

	/* read pattern list */
	patSet.patterns = readHighlightPatterns(inPtr, True, &errMsg, &patSet.nPatterns);
	if (patSet.patterns == nullptr)
		return highlightError(stringStart, *inPtr, errMsg);

	/* pattern set was read correctly, make an allocated copy to return */
	retPatSet = (patternSet *)XtMalloc(sizeof(patternSet));
	*retPatSet = patSet;

	/* Convert pre-5.1 pattern sets which use old regular expression
	   syntax to quote braces and use & rather than \0 */
	if (convertOld)
		convertOldPatternSet(retPatSet);

	return retPatSet;
}

/*
** Parse a set of highlight patterns into an array of highlightPattern
** structures, and a language mode name.  If unsuccessful, returns nullptr with
** (statically allocated) message in "errMsg".
*/
static highlightPattern *readHighlightPatterns(const char **inPtr, int withBraces, const char **errMsg, int *nPatterns) {
	highlightPattern *pat, *returnedList, patternList[MAX_PATTERNS];

	/* skip over blank space */
	*inPtr += strspn(*inPtr, " \t\n");

	/* look for initial brace */
	if (withBraces) {
		if (**inPtr != '{') {
			*errMsg = "pattern list must begin with \"{\"";
			return False;
		}
		(*inPtr)++;
	}

	/*
	** parse each pattern in the list
	*/
	pat = patternList;
	while (True) {
		*inPtr += strspn(*inPtr, " \t\n");
		if (**inPtr == '\0') {
			if (withBraces) {
				*errMsg = "end of pattern list not found";
				return nullptr;
			} else
				break;
		} else if (**inPtr == '}') {
			(*inPtr)++;
			break;
		}
		if (pat - patternList >= MAX_PATTERNS) {
			*errMsg = "max number of patterns exceeded\n";
			return nullptr;
		}
		if (!readHighlightPattern(inPtr, errMsg, pat++))
			return nullptr;
	}

	/* allocate a more appropriately sized list to return patterns */
	*nPatterns = pat - patternList;
	returnedList = new highlightPattern[*nPatterns];
	memcpy(returnedList, patternList, sizeof(highlightPattern) * *nPatterns);
	return returnedList;
}

static int readHighlightPattern(const char **inPtr, const char **errMsg, highlightPattern *pattern) {
	/* read the name field */
	pattern->name = ReadSymbolicField(inPtr);
	if (pattern->name == nullptr) {
		*errMsg = "pattern name is required";
		return False;
	}
	if (!SkipDelimiter(inPtr, errMsg))
		return False;

	/* read the start pattern */
	if (!ReadQuotedString(inPtr, errMsg, &pattern->startRE))
		return False;
	if (!SkipDelimiter(inPtr, errMsg))
		return False;

	/* read the end pattern */
	if (**inPtr == ':')
		pattern->endRE = nullptr;
	else if (!ReadQuotedString(inPtr, errMsg, &pattern->endRE))
		return False;
	if (!SkipDelimiter(inPtr, errMsg))
		return False;

	/* read the error pattern */
	if (**inPtr == ':')
		pattern->errorRE = nullptr;
	else if (!ReadQuotedString(inPtr, errMsg, &pattern->errorRE))
		return False;
	if (!SkipDelimiter(inPtr, errMsg))
		return False;

	/* read the style field */
	pattern->style = ReadSymbolicField(inPtr);
	if (pattern->style == nullptr) {
		*errMsg = "style field required in pattern";
		return False;
	}
	if (!SkipDelimiter(inPtr, errMsg))
		return False;

	/* read the sub-pattern-of field */
	pattern->subPatternOf = ReadSymbolicField(inPtr);
	if (!SkipDelimiter(inPtr, errMsg))
		return False;

	/* read flags field */
	pattern->flags = 0;
	for (; **inPtr != '\n' && **inPtr != '}'; (*inPtr)++) {
		if (**inPtr == 'D')
			pattern->flags |= DEFER_PARSING;
		else if (**inPtr == 'R')
			pattern->flags |= PARSE_SUBPATS_FROM_START;
		else if (**inPtr == 'C')
			pattern->flags |= COLOR_ONLY;
		else if (**inPtr != ' ' && **inPtr != '\t') {
			*errMsg = "unreadable flag field";
			return False;
		}
	}
	return True;
}

/*
** Given a language mode name, determine if there is a default (built-in)
** pattern set available for that language mode, and if so, read it and
** return a new allocated copy of it.  The returned pattern set should be
** freed by the caller with freePatternSet()
*/
static patternSet *readDefaultPatternSet(const char *langModeName) {
	int i;
	size_t modeNameLen;
	const char *strPtr;

	modeNameLen = strlen(langModeName);
	for (i = 0; i < (int)XtNumber(DefaultPatternSets); i++) {
		if (!strncmp(langModeName, DefaultPatternSets[i], modeNameLen) && DefaultPatternSets[i][modeNameLen] == ':') {
			strPtr = DefaultPatternSets[i];
			return readPatternSet(&strPtr, False);
		}
	}
	return nullptr;
}

/*
** Return True if patSet exactly matches one of the default pattern sets
*/
static int isDefaultPatternSet(patternSet *patSet) {
	patternSet *defaultPatSet;
	int retVal;

	defaultPatSet = readDefaultPatternSet(patSet->languageMode);
	if (defaultPatSet == nullptr)
		return False;
	retVal = !patternSetsDiffer(patSet, defaultPatSet);
	freePatternSet(defaultPatSet);
	return retVal;
}

/*
** Short-hand functions for formating and outputing errors for
*/
static patternSet *highlightError(const char *stringStart, const char *stoppedAt, const char *message) {
	ParseError(nullptr, stringStart, stoppedAt, "highlight pattern", message);
	return nullptr;
}

static bool styleError(const char *stringStart, const char *stoppedAt, const char *message) {
	ParseError(nullptr, stringStart, stoppedAt, "style specification", message);
	return false;
}

/*
** Present a dialog for editing highlight style information
*/
void EditHighlightStyles(const char *initialStyle) {
#define HS_LIST_RIGHT 60
#define HS_LEFT_MARGIN_POS 1
#define HS_RIGHT_MARGIN_POS 99
#define HS_H_MARGIN 10
	Widget form, nameLbl, topLbl, colorLbl, bgColorLbl, fontLbl;
	Widget fontBox, sep1, okBtn, applyBtn, closeBtn;
	XmString s1;
	int i, ac;
	Arg args[20];

	/* if the dialog is already displayed, just pop it to the top and return */
	if (HSDialog.shell != nullptr) {
		if (initialStyle != nullptr)
			setStyleByName(initialStyle);
		RaiseDialogWindow(HSDialog.shell);
		return;
	}

	/* Copy the list of highlight style information to one that the user
	   can freely edit (via the dialog and managed-list code) */
	HSDialog.highlightStyleList = (highlightStyleRec **)XtMalloc(sizeof(highlightStyleRec *) * MAX_HIGHLIGHT_STYLES);
	for (i = 0; i < NHighlightStyles; i++)
		HSDialog.highlightStyleList[i] = copyHighlightStyleRec(HighlightStyles[i]);
	HSDialog.nHighlightStyles = NHighlightStyles;

	/* Create a form widget in an application shell */
	ac = 0;
	XtSetArg(args[ac], XmNdeleteResponse, XmDO_NOTHING);
	ac++;
	XtSetArg(args[ac], XmNiconName, "NEdit Text Drawing Styles");
	ac++;
	XtSetArg(args[ac], XmNtitle, "Text Drawing Styles");
	ac++;
	HSDialog.shell = CreateWidget(TheAppShell, "textStyles", topLevelShellWidgetClass, args, ac);
	AddSmallIcon(HSDialog.shell);
	form = XtVaCreateManagedWidget("editHighlightStyles", xmFormWidgetClass, HSDialog.shell, XmNautoUnmanage, False, XmNresizePolicy, XmRESIZE_NONE, nullptr);
	XtAddCallback(form, XmNdestroyCallback, hsDestroyCB, nullptr);
	AddMotifCloseCallback(HSDialog.shell, hsCloseCB, nullptr);

	topLbl = XtVaCreateManagedWidget("topLabel", xmLabelGadgetClass, form, XmNlabelString, s1 = XmStringCreateLtoREx("To modify the properties of an existing highlight style, select the name\n\
from the list on the left.  Select \"New\" to add a new style to the list."),
	                                 XmNmnemonic, 'N', XmNtopAttachment, XmATTACH_POSITION, XmNtopPosition, 2, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, HS_LEFT_MARGIN_POS, XmNrightAttachment, XmATTACH_POSITION,
	                                 XmNrightPosition, HS_RIGHT_MARGIN_POS, nullptr);
	XmStringFree(s1);

	nameLbl = XtVaCreateManagedWidget("nameLbl", xmLabelGadgetClass, form, XmNlabelString, s1 = XmStringCreateSimpleEx("Name:"), XmNmnemonic, 'm', XmNalignment, XmALIGNMENT_BEGINNING, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition,
	                                  HS_LIST_RIGHT, XmNtopAttachment, XmATTACH_WIDGET, XmNtopOffset, HS_H_MARGIN, XmNtopWidget, topLbl, nullptr);
	XmStringFree(s1);

	HSDialog.nameW = XtVaCreateManagedWidget("name", xmTextWidgetClass, form, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, HS_LIST_RIGHT, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, nameLbl, XmNrightAttachment,
	                                         XmATTACH_POSITION, XmNrightPosition, HS_RIGHT_MARGIN_POS, nullptr);
	RemapDeleteKey(HSDialog.nameW);
	XtVaSetValues(nameLbl, XmNuserData, HSDialog.nameW, nullptr);

	colorLbl = XtVaCreateManagedWidget("colorLbl", xmLabelGadgetClass, form, XmNlabelString, s1 = XmStringCreateSimpleEx("Foreground Color:"), XmNmnemonic, 'C', XmNalignment, XmALIGNMENT_BEGINNING, XmNleftAttachment, XmATTACH_POSITION,
	                                   XmNleftPosition, HS_LIST_RIGHT, XmNtopAttachment, XmATTACH_WIDGET, XmNtopOffset, HS_H_MARGIN, XmNtopWidget, HSDialog.nameW, nullptr);
	XmStringFree(s1);

	HSDialog.colorW = XtVaCreateManagedWidget("color", xmTextWidgetClass, form, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, HS_LIST_RIGHT, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, colorLbl, XmNrightAttachment,
	                                          XmATTACH_POSITION, XmNrightPosition, HS_RIGHT_MARGIN_POS, nullptr);
	RemapDeleteKey(HSDialog.colorW);
	XtVaSetValues(colorLbl, XmNuserData, HSDialog.colorW, nullptr);

	bgColorLbl = XtVaCreateManagedWidget("bgColorLbl", xmLabelGadgetClass, form, XmNlabelString, s1 = XmStringCreateSimpleEx("Background Color (optional)"), XmNmnemonic, 'g', XmNalignment, XmALIGNMENT_BEGINNING, XmNleftAttachment,
	                                     XmATTACH_POSITION, XmNleftPosition, HS_LIST_RIGHT, XmNtopAttachment, XmATTACH_WIDGET, XmNtopOffset, HS_H_MARGIN, XmNtopWidget, HSDialog.colorW, nullptr);
	XmStringFree(s1);

	HSDialog.bgColorW = XtVaCreateManagedWidget("bgColor", xmTextWidgetClass, form, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, HS_LIST_RIGHT, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, bgColorLbl, XmNrightAttachment,
	                                            XmATTACH_POSITION, XmNrightPosition, HS_RIGHT_MARGIN_POS, nullptr);
	RemapDeleteKey(HSDialog.bgColorW);
	XtVaSetValues(bgColorLbl, XmNuserData, HSDialog.bgColorW, nullptr);

	fontLbl = XtVaCreateManagedWidget("fontLbl", xmLabelGadgetClass, form, XmNlabelString, s1 = XmStringCreateSimpleEx("Font:"), XmNalignment, XmALIGNMENT_BEGINNING, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, HS_LIST_RIGHT,
	                                  XmNtopAttachment, XmATTACH_WIDGET, XmNtopOffset, HS_H_MARGIN, XmNtopWidget, HSDialog.bgColorW, nullptr);
	XmStringFree(s1);

	fontBox = XtVaCreateManagedWidget("fontBox", xmRowColumnWidgetClass, form, XmNpacking, XmPACK_COLUMN, XmNnumColumns, 2, XmNradioBehavior, True, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, HS_LIST_RIGHT, XmNtopAttachment,
	                                  XmATTACH_WIDGET, XmNtopWidget, fontLbl, nullptr);
	HSDialog.plainW = XtVaCreateManagedWidget("plain", xmToggleButtonWidgetClass, fontBox, XmNset, True, XmNlabelString, s1 = XmStringCreateSimpleEx("Plain"), XmNmnemonic, 'P', nullptr);
	XmStringFree(s1);
	HSDialog.boldW = XtVaCreateManagedWidget("bold", xmToggleButtonWidgetClass, fontBox, XmNlabelString, s1 = XmStringCreateSimpleEx("Bold"), XmNmnemonic, 'B', nullptr);
	XmStringFree(s1);
	HSDialog.italicW = XtVaCreateManagedWidget("italic", xmToggleButtonWidgetClass, fontBox, XmNlabelString, s1 = XmStringCreateSimpleEx("Italic"), XmNmnemonic, 'I', nullptr);
	XmStringFree(s1);
	HSDialog.boldItalicW = XtVaCreateManagedWidget("boldItalic", xmToggleButtonWidgetClass, fontBox, XmNlabelString, s1 = XmStringCreateSimpleEx("Bold Italic"), XmNmnemonic, 'o', nullptr);
	XmStringFree(s1);

	okBtn = XtVaCreateManagedWidget("ok", xmPushButtonWidgetClass, form, XmNlabelString, s1 = XmStringCreateSimpleEx("OK"), XmNmarginWidth, BUTTON_WIDTH_MARGIN, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 10, XmNrightAttachment,
	                                XmATTACH_POSITION, XmNrightPosition, 30, XmNbottomAttachment, XmATTACH_POSITION, XmNbottomPosition, 99, nullptr);
	XtAddCallback(okBtn, XmNactivateCallback, hsOkCB, nullptr);
	XmStringFree(s1);

	applyBtn = XtVaCreateManagedWidget("apply", xmPushButtonWidgetClass, form, XmNlabelString, s1 = XmStringCreateSimpleEx("Apply"), XmNmnemonic, 'A', XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 40, XmNrightAttachment,
	                                   XmATTACH_POSITION, XmNrightPosition, 60, XmNbottomAttachment, XmATTACH_POSITION, XmNbottomPosition, 99, nullptr);
	XtAddCallback(applyBtn, XmNactivateCallback, hsApplyCB, nullptr);
	XmStringFree(s1);

	closeBtn = XtVaCreateManagedWidget("close", xmPushButtonWidgetClass, form, XmNlabelString, s1 = XmStringCreateSimpleEx("Close"), XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 70, XmNrightAttachment, XmATTACH_POSITION,
	                                   XmNrightPosition, 90, XmNbottomAttachment, XmATTACH_POSITION, XmNbottomPosition, 99, nullptr);
	XtAddCallback(closeBtn, XmNactivateCallback, hsCloseCB, nullptr);
	XmStringFree(s1);

	sep1 = XtVaCreateManagedWidget("sep1", xmSeparatorGadgetClass, form, XmNleftAttachment, XmATTACH_FORM, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, fontBox, XmNtopOffset, HS_H_MARGIN, XmNrightAttachment, XmATTACH_FORM,
	                               XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, closeBtn, 0, XmNbottomOffset, HS_H_MARGIN, nullptr);

	ac = 0;
	XtSetArg(args[ac], XmNtopAttachment, XmATTACH_WIDGET);
	ac++;
	XtSetArg(args[ac], XmNtopOffset, HS_H_MARGIN);
	ac++;
	XtSetArg(args[ac], XmNtopWidget, topLbl);
	ac++;
	XtSetArg(args[ac], XmNleftAttachment, XmATTACH_POSITION);
	ac++;
	XtSetArg(args[ac], XmNleftPosition, HS_LEFT_MARGIN_POS);
	ac++;
	XtSetArg(args[ac], XmNrightAttachment, XmATTACH_POSITION);
	ac++;
	XtSetArg(args[ac], XmNrightPosition, HS_LIST_RIGHT - 1);
	ac++;
	XtSetArg(args[ac], XmNbottomAttachment, XmATTACH_WIDGET);
	ac++;
	XtSetArg(args[ac], XmNbottomWidget, sep1);
	ac++;
	XtSetArg(args[ac], XmNbottomOffset, HS_H_MARGIN);
	ac++;
	HSDialog.managedListW = CreateManagedList(form, (String) "list", args, ac, (void **)HSDialog.highlightStyleList, &HSDialog.nHighlightStyles, MAX_HIGHLIGHT_STYLES, 20, hsGetDisplayedCB, nullptr, hsSetDisplayedCB, form, hsFreeItemCB);
	XtVaSetValues(topLbl, XmNuserData, HSDialog.managedListW, nullptr);

	/* Set initial default button */
	XtVaSetValues(form, XmNdefaultButton, okBtn, nullptr);
	XtVaSetValues(form, XmNcancelButton, closeBtn, nullptr);

	/* If there's a suggestion for an initial selection, make it */
	if (initialStyle != nullptr)
		setStyleByName(initialStyle);

	/* Handle mnemonic selection of buttons and focus to dialog */
	AddDialogMnemonicHandler(form, FALSE);

	/* Realize all of the widgets in the new dialog */
	RealizeWithoutForcingPosition(HSDialog.shell);
}

static void hsDestroyCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	int i;

	for (i = 0; i < HSDialog.nHighlightStyles; i++)
		freeHighlightStyleRec(HSDialog.highlightStyleList[i]);
	XtFree((char *)HSDialog.highlightStyleList);
}

static void hsOkCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	if (!updateHSList())
		return;

	/* pop down and destroy the dialog */
	XtDestroyWidget(HSDialog.shell);
	HSDialog.shell = nullptr;
}

static void hsApplyCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	updateHSList();
}

static void hsCloseCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	/* pop down and destroy the dialog */
	XtDestroyWidget(HSDialog.shell);
	HSDialog.shell = nullptr;
}

static void *hsGetDisplayedCB(void *oldItem, int explicitRequest, int *abort, void *cbArg) {

	(void)cbArg;

	highlightStyleRec *hs;

	/* If the dialog is currently displaying the "new" entry and the
	   fields are empty, that's just fine */
	if (oldItem == nullptr && hsDialogEmpty())
		return nullptr;

	/* If there are no problems reading the data, just return it */
	hs = readHSDialogFields(True);
	if (hs != nullptr)
		return (void *)hs;

	/* If there are problems, and the user didn't ask for the fields to be
	   read, give more warning */
	if (!explicitRequest) {
		if (DialogF(DF_WARN, HSDialog.shell, 2, "Incomplete Style", "Discard incomplete entry\nfor current highlight style?", "Keep", "Discard") == 2) {
			return oldItem == nullptr ? nullptr : (void *)copyHighlightStyleRec((highlightStyleRec *)oldItem);
		}
	}

	/* Do readHSDialogFields again without "silent" mode to display warning */
	hs = readHSDialogFields(False);
	*abort = True;
	return nullptr;
}

static void hsSetDisplayedCB(void *item, void *cbArg) {
	highlightStyleRec *hs = (highlightStyleRec *)item;

	if (item == nullptr) {
		XmTextSetStringEx(HSDialog.nameW, "");
		XmTextSetStringEx(HSDialog.colorW, "");
		XmTextSetStringEx(HSDialog.bgColorW, "");
		RadioButtonChangeState(HSDialog.plainW, True, False);
		RadioButtonChangeState(HSDialog.boldW, False, False);
		RadioButtonChangeState(HSDialog.italicW, False, False);
		RadioButtonChangeState(HSDialog.boldItalicW, False, False);
	} else {
		if (hs->name == "Plain") {
			/* you should not be able to delete the reserved style "Plain" */
			int i;
			int others = 0;
			int nList = HSDialog.nHighlightStyles;
			highlightStyleRec **list = HSDialog.highlightStyleList;
			/* do we have other styles called Plain? */
			for (i = 0; i < nList; i++) {
				if (list[i] != hs && (list[i]->name == "Plain")) {
					others++;
				}
			}
			if (others == 0) {
				/* this is the last style entry named "Plain" */
				Widget form = (Widget)cbArg;
				Widget deleteBtn = XtNameToWidget(form, "*delete");
				/* disable delete button */
				if (deleteBtn) {
					XtSetSensitive(deleteBtn, False);
				}
			}
		}
		XmTextSetStringEx(HSDialog.nameW, hs->name);
		XmTextSetStringEx(HSDialog.colorW, hs->color);
		XmTextSetStringEx(HSDialog.bgColorW, hs->bgColor ? hs->bgColor : "");
		RadioButtonChangeState(HSDialog.plainW, hs->font == PLAIN_FONT, False);
		RadioButtonChangeState(HSDialog.boldW, hs->font == BOLD_FONT, False);
		RadioButtonChangeState(HSDialog.italicW, hs->font == ITALIC_FONT, False);
		RadioButtonChangeState(HSDialog.boldItalicW, hs->font == BOLD_ITALIC_FONT, False);
	}
}

static void hsFreeItemCB(void *item) {
	freeHighlightStyleRec((highlightStyleRec *)item);
}

static highlightStyleRec *readHSDialogFields(int silent) {
	highlightStyleRec *hs;
	Display *display = XtDisplay(HSDialog.shell);
	int screenNum = XScreenNumberOfScreen(XtScreen(HSDialog.shell));
	XColor rgb;

	/* Allocate a language mode structure to return */
	hs = new highlightStyleRec;

	/* read the name field */
	char *name = ReadSymbolicFieldTextWidget(HSDialog.nameW, "highlight style name", silent);
	if (name == nullptr) {
		delete hs;
		return nullptr;
	}
	hs->name = name;

	if (hs->name.empty()) {
		if (!silent) {
			DialogF(DF_WARN, HSDialog.shell, 1, "Highlight Style", "Please specify a name\nfor the highlight style", "OK");
			XmProcessTraversal(HSDialog.nameW, XmTRAVERSE_CURRENT);
		}
		delete hs;
		return nullptr;
	}

	/* read the color field */
	hs->color = ReadSymbolicFieldTextWidget(HSDialog.colorW, "color", silent);
	if (hs->color == nullptr) {
		delete hs;
		return nullptr;
	}

	if (*hs->color == '\0') {
		if (!silent) {
			DialogF(DF_WARN, HSDialog.shell, 1, "Style Color", "Please specify a color\nfor the highlight style", "OK");
			XmProcessTraversal(HSDialog.colorW, XmTRAVERSE_CURRENT);
		}
		delete hs;
		return nullptr;
	}

	/* Verify that the color is a valid X color spec */
	if (!XParseColor(display, DefaultColormap(display, screenNum), hs->color, &rgb)) {
		if (!silent) {
			DialogF(DF_WARN, HSDialog.shell, 1, "Invalid Color", "Invalid X color specification: %s\n", "OK", hs->color);
			XmProcessTraversal(HSDialog.colorW, XmTRAVERSE_CURRENT);
		}
		delete hs;
		return nullptr;
	}

	/* read the background color field - this may be empty */
	hs->bgColor = ReadSymbolicFieldTextWidget(HSDialog.bgColorW, "bgColor", silent);
	if (hs->bgColor && *hs->bgColor == '\0') {
		hs->bgColor = nullptr;
	}

	/* Verify that the background color (if present) is a valid X color spec */
	if (hs->bgColor && !XParseColor(display, DefaultColormap(display, screenNum), hs->bgColor, &rgb)) {
		if (!silent) {
			DialogF(DF_WARN, HSDialog.shell, 1, "Invalid Color", "Invalid X background color specification: %s\n", "OK", hs->bgColor);
			XmProcessTraversal(HSDialog.bgColorW, XmTRAVERSE_CURRENT);
		}
		delete hs;
		return nullptr;
	}

	/* read the font buttons */
	if (XmToggleButtonGetState(HSDialog.boldW))
		hs->font = BOLD_FONT;
	else if (XmToggleButtonGetState(HSDialog.italicW))
		hs->font = ITALIC_FONT;
	else if (XmToggleButtonGetState(HSDialog.boldItalicW))
		hs->font = BOLD_ITALIC_FONT;
	else
		hs->font = PLAIN_FONT;

	return hs;
}

/*
** Copy a highlightStyleRec data structure, and all of the allocated memory
** it contains.
*/
static highlightStyleRec *copyHighlightStyleRec(highlightStyleRec *hs) {
	highlightStyleRec *newHS;

	newHS = new highlightStyleRec;
	newHS->name = hs->name;

	if (hs->color == nullptr) {
		newHS->color = nullptr;
	} else {
		newHS->color = XtStringDup(hs->color);
	}
	
	if (hs->bgColor == nullptr) {
		newHS->bgColor = nullptr;
	} else {
		newHS->bgColor = XtStringDup(hs->bgColor);
	}
	newHS->font = hs->font;
	return newHS;
}

/*
** Free all of the allocated data in a highlightStyleRec, including the
** structure itself.
*/
static void freeHighlightStyleRec(highlightStyleRec *hs) {
	delete hs;
}

/*
** Select a particular style in the highlight styles dialog
*/
static void setStyleByName(const char *style) {

	for (int i = 0; i < HSDialog.nHighlightStyles; i++) {
		if (HSDialog.highlightStyleList[i]->name == style) {
			SelectManagedListItem(HSDialog.managedListW, i);
			break;
		}
	}
}

/*
** Return True if the fields of the highlight styles dialog are consistent
** with a blank "New" style in the dialog.
*/
static int hsDialogEmpty(void) {
	return TextWidgetIsBlank(HSDialog.nameW) && TextWidgetIsBlank(HSDialog.colorW) && XmToggleButtonGetState(HSDialog.plainW);
}

/*
** Apply the changes made in the highlight styles dialog to the stored
** highlight style information in HighlightStyles
*/
static int updateHSList(void) {
	WindowInfo *window;

	/* Get the current contents of the dialog fields */
	if (!UpdateManagedList(HSDialog.managedListW, True))
		return False;

	/* Replace the old highlight styles list with the new one from the dialog */
	for (int i = 0; i < NHighlightStyles; i++)
		freeHighlightStyleRec(HighlightStyles[i]);
	for (int i = 0; i < HSDialog.nHighlightStyles; i++)
		HighlightStyles[i] = copyHighlightStyleRec(HSDialog.highlightStyleList[i]);
	NHighlightStyles = HSDialog.nHighlightStyles;

	/* If a syntax highlighting dialog is up, update its menu */
	updateHighlightStyleMenu();

	/* Redisplay highlighted windows which use changed style(s) */
	for (window = WindowList; window != nullptr; window = window->next)
		UpdateHighlightStyles(window);

	/* Note that preferences have been changed */
	MarkPrefsChanged();

	return True;
}

/*
** Present a dialog for editing highlight pattern information
*/
void EditHighlightPatterns(WindowInfo *window) {
#define BORDER 4
#define LIST_RIGHT 41
	Widget form, lmOptMenu, patternsForm, patternsFrame, patternsLbl;
	Widget lmForm, contextFrame, contextForm, styleLbl, styleBtn;
	Widget okBtn, applyBtn, checkBtn, deleteBtn, closeBtn, helpBtn;
	Widget restoreBtn, nameLbl, typeLbl, typeBox, lmBtn, matchBox;
	patternSet *patSet;
	XmString s1;
	int i, n, nPatterns;
	Arg args[20];

	/* if the dialog is already displayed, just pop it to the top and return */
	if (HighlightDialog.shell != nullptr) {
		RaiseDialogWindow(HighlightDialog.shell);
		return;
	}

	if (LanguageModeName(0) == nullptr) {
		DialogF(DF_WARN, window->shell, 1, "No Language Modes", "No Language Modes available for syntax highlighting\n"
		                                                        "Add language modes under Preferenses->Language Modes",
		        "OK");
		return;
	}

	/* Decide on an initial language mode */
	HighlightDialog.langModeName = XtNewStringEx(LanguageModeName(window->languageMode == PLAIN_LANGUAGE_MODE ? 0 : window->languageMode));

	/* Find the associated pattern set (patSet) to edit */
	patSet = FindPatternSet(HighlightDialog.langModeName);

	/* Copy the list of patterns to one that the user can freely edit */
	HighlightDialog.patterns = new highlightPattern *[MAX_PATTERNS];
	nPatterns = patSet == nullptr ? 0 : patSet->nPatterns;
	for (i = 0; i < nPatterns; i++)
		HighlightDialog.patterns[i] = copyPatternSrc(&patSet->patterns[i], nullptr);
	HighlightDialog.nPatterns = nPatterns;

	/* Create a form widget in an application shell */
	n = 0;
	XtSetArg(args[n], XmNdeleteResponse, XmDO_NOTHING);
	n++;
	XtSetArg(args[n], XmNiconName, "NEdit Highlight Patterns");
	n++;
	XtSetArg(args[n], XmNtitle, "Syntax Highlighting Patterns");
	n++;
	HighlightDialog.shell = CreateWidget(TheAppShell, "syntaxHighlight", topLevelShellWidgetClass, args, n);
	AddSmallIcon(HighlightDialog.shell);
	form = XtVaCreateManagedWidget("editHighlightPatterns", xmFormWidgetClass, HighlightDialog.shell, XmNautoUnmanage, False, XmNresizePolicy, XmRESIZE_NONE, nullptr);
	XtAddCallback(form, XmNdestroyCallback, destroyCB, nullptr);
	AddMotifCloseCallback(HighlightDialog.shell, closeCB, nullptr);

	lmForm = XtVaCreateManagedWidget("lmForm", xmFormWidgetClass, form, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 1, XmNtopAttachment, XmATTACH_POSITION, XmNtopPosition, 1, XmNrightAttachment, XmATTACH_POSITION,
	                                 XmNrightPosition, 99, nullptr);

	HighlightDialog.lmPulldown = CreateLanguageModeMenu(lmForm, langModeCB, nullptr);
	n = 0;
	XtSetArg(args[n], XmNspacing, 0);
	n++;
	XtSetArg(args[n], XmNmarginWidth, 0);
	n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM);
	n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION);
	n++;
	XtSetArg(args[n], XmNleftPosition, 50);
	n++;
	XtSetArg(args[n], XmNsubMenuId, HighlightDialog.lmPulldown);
	n++;
	lmOptMenu = XmCreateOptionMenu(lmForm, (String) "langModeOptMenu", args, n);
	XtManageChild(lmOptMenu);
	HighlightDialog.lmOptMenu = lmOptMenu;

	XtVaCreateManagedWidget("lmLbl", xmLabelGadgetClass, lmForm, XmNlabelString, s1 = XmStringCreateSimpleEx("Language Mode:"), XmNmnemonic, 'M', XmNuserData, XtParent(HighlightDialog.lmOptMenu), XmNalignment, XmALIGNMENT_END,
	                        XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, 50, XmNtopAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET, XmNbottomWidget, lmOptMenu, nullptr);
	XmStringFree(s1);

	lmBtn = XtVaCreateManagedWidget("lmBtn", xmPushButtonWidgetClass, lmForm, XmNlabelString, s1 = XmStringCreateLtoREx("Add / Modify\nLanguage Mode..."), XmNmnemonic, 'A', XmNrightAttachment, XmATTACH_FORM, XmNtopAttachment, XmATTACH_FORM,
	                                nullptr);
	XtAddCallback(lmBtn, XmNactivateCallback, lmDialogCB, nullptr);
	XmStringFree(s1);

	okBtn = XtVaCreateManagedWidget("ok", xmPushButtonWidgetClass, form, XmNlabelString, s1 = XmStringCreateSimpleEx("OK"), XmNmarginWidth, BUTTON_WIDTH_MARGIN, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 1, XmNrightAttachment,
	                                XmATTACH_POSITION, XmNrightPosition, 13, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, BORDER, nullptr);
	XtAddCallback(okBtn, XmNactivateCallback, okCB, nullptr);
	XmStringFree(s1);

	applyBtn = XtVaCreateManagedWidget("apply", xmPushButtonWidgetClass, form, XmNlabelString, s1 = XmStringCreateSimpleEx("Apply"), XmNmnemonic, 'y', XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 13, XmNrightAttachment,
	                                   XmATTACH_POSITION, XmNrightPosition, 26, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, BORDER, nullptr);
	XtAddCallback(applyBtn, XmNactivateCallback, applyCB, nullptr);
	XmStringFree(s1);

	checkBtn = XtVaCreateManagedWidget("check", xmPushButtonWidgetClass, form, XmNlabelString, s1 = XmStringCreateSimpleEx("Check"), XmNmnemonic, 'k', XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 26, XmNrightAttachment,
	                                   XmATTACH_POSITION, XmNrightPosition, 39, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, BORDER, nullptr);
	XtAddCallback(checkBtn, XmNactivateCallback, checkCB, nullptr);
	XmStringFree(s1);

	deleteBtn = XtVaCreateManagedWidget("delete", xmPushButtonWidgetClass, form, XmNlabelString, s1 = XmStringCreateSimpleEx("Delete"), XmNmnemonic, 'D', XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 39, XmNrightAttachment,
	                                    XmATTACH_POSITION, XmNrightPosition, 52, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, BORDER, nullptr);
	XtAddCallback(deleteBtn, XmNactivateCallback, deleteCB, nullptr);
	XmStringFree(s1);

	restoreBtn = XtVaCreateManagedWidget("restore", xmPushButtonWidgetClass, form, XmNlabelString, s1 = XmStringCreateSimpleEx("Restore Defaults"), XmNmnemonic, 'f', XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 52,
	                                     XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, 73, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, BORDER, nullptr);
	XtAddCallback(restoreBtn, XmNactivateCallback, restoreCB, nullptr);
	XmStringFree(s1);

	closeBtn = XtVaCreateManagedWidget("close", xmPushButtonWidgetClass, form, XmNlabelString, s1 = XmStringCreateSimpleEx("Close"), XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 73, XmNrightAttachment, XmATTACH_POSITION,
	                                   XmNrightPosition, 86, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, BORDER, nullptr);
	XtAddCallback(closeBtn, XmNactivateCallback, closeCB, nullptr);
	XmStringFree(s1);

	helpBtn = XtVaCreateManagedWidget("help", xmPushButtonWidgetClass, form, XmNlabelString, s1 = XmStringCreateSimpleEx("Help"), XmNmnemonic, 'H', XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 86, XmNrightAttachment,
	                                  XmATTACH_POSITION, XmNrightPosition, 99, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, BORDER, nullptr);
	XtAddCallback(helpBtn, XmNactivateCallback, helpCB, nullptr);
	XmStringFree(s1);

	contextFrame = XtVaCreateManagedWidget("contextFrame", xmFrameWidgetClass, form, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 1, XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, 99, XmNbottomAttachment,
	                                       XmATTACH_WIDGET, XmNbottomWidget, okBtn, XmNbottomOffset, BORDER, nullptr);
	contextForm = XtVaCreateManagedWidget("contextForm", xmFormWidgetClass, contextFrame, nullptr);
	XtVaCreateManagedWidget("contextLbl", xmLabelGadgetClass, contextFrame, XmNlabelString, s1 = XmStringCreateSimpleEx("Context requirements for incremental re-parsing after changes"), XmNchildType, XmFRAME_TITLE_CHILD, nullptr);
	XmStringFree(s1);

	HighlightDialog.lineContextW = XtVaCreateManagedWidget("lineContext", xmTextWidgetClass, contextForm, XmNcolumns, 5, XmNmaxLength, 12, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 15, XmNrightAttachment, XmATTACH_POSITION,
	                                                       XmNrightPosition, 25, nullptr);
	RemapDeleteKey(HighlightDialog.lineContextW);

	XtVaCreateManagedWidget("lineContLbl", xmLabelGadgetClass, contextForm, XmNlabelString, s1 = XmStringCreateSimpleEx("lines"), XmNmnemonic, 'l', XmNuserData, HighlightDialog.lineContextW, XmNalignment, XmALIGNMENT_BEGINNING,
	                        XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, HighlightDialog.lineContextW, XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET, XmNtopWidget, HighlightDialog.lineContextW, XmNbottomAttachment,
	                        XmATTACH_OPPOSITE_WIDGET, XmNbottomWidget, HighlightDialog.lineContextW, nullptr);
	XmStringFree(s1);

	HighlightDialog.charContextW = XtVaCreateManagedWidget("charContext", xmTextWidgetClass, contextForm, XmNcolumns, 5, XmNmaxLength, 12, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 58, XmNrightAttachment, XmATTACH_POSITION,
	                                                       XmNrightPosition, 68, nullptr);
	RemapDeleteKey(HighlightDialog.lineContextW);

	XtVaCreateManagedWidget("charContLbl", xmLabelGadgetClass, contextForm, XmNlabelString, s1 = XmStringCreateSimpleEx("characters"), XmNmnemonic, 'c', XmNuserData, HighlightDialog.charContextW, XmNalignment, XmALIGNMENT_BEGINNING,
	                        XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, HighlightDialog.charContextW, XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET, XmNtopWidget, HighlightDialog.charContextW, XmNbottomAttachment,
	                        XmATTACH_OPPOSITE_WIDGET, XmNbottomWidget, HighlightDialog.charContextW, nullptr);
	XmStringFree(s1);

	patternsFrame = XtVaCreateManagedWidget("patternsFrame", xmFrameWidgetClass, form, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 1, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, lmForm, XmNrightAttachment, XmATTACH_POSITION,
	                                        XmNrightPosition, 99, XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, contextFrame, XmNbottomOffset, BORDER, nullptr);
	patternsForm = XtVaCreateManagedWidget("patternsForm", xmFormWidgetClass, patternsFrame, nullptr);
	patternsLbl = XtVaCreateManagedWidget("patternsLbl", xmLabelGadgetClass, patternsFrame, XmNlabelString, s1 = XmStringCreateSimpleEx("Patterns"), XmNmnemonic, 'P', XmNmarginHeight, 0, XmNchildType, XmFRAME_TITLE_CHILD, nullptr);
	XmStringFree(s1);

	typeLbl = XtVaCreateManagedWidget("typeLbl", xmLabelGadgetClass, patternsForm, XmNlabelString, s1 = XmStringCreateSimpleEx("Pattern Type:"), XmNmarginHeight, 0, XmNalignment, XmALIGNMENT_BEGINNING, XmNleftAttachment, XmATTACH_POSITION,
	                                  XmNleftPosition, LIST_RIGHT, XmNtopAttachment, XmATTACH_FORM, nullptr);
	XmStringFree(s1);

	typeBox = XtVaCreateManagedWidget("typeBox", xmRowColumnWidgetClass, patternsForm, XmNpacking, XmPACK_COLUMN, XmNradioBehavior, True, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, LIST_RIGHT, XmNtopAttachment, XmATTACH_WIDGET,
	                                  XmNtopWidget, typeLbl, nullptr);
	HighlightDialog.topLevelW =
	    XtVaCreateManagedWidget("top", xmToggleButtonWidgetClass, typeBox, XmNset, True, XmNmarginHeight, 0, XmNlabelString, s1 = XmStringCreateSimpleEx("Pass-1 (applied to all text when loaded or modified)"), XmNmnemonic, '1', nullptr);
	XmStringFree(s1);
	XtAddCallback(HighlightDialog.topLevelW, XmNvalueChangedCallback, patTypeCB, nullptr);
	HighlightDialog.deferredW =
	    XtVaCreateManagedWidget("deferred", xmToggleButtonWidgetClass, typeBox, XmNmarginHeight, 0, XmNlabelString, s1 = XmStringCreateSimpleEx("Pass-2 (parsing is deferred until text is exposed)"), XmNmnemonic, '2', nullptr);
	XmStringFree(s1);
	XtAddCallback(HighlightDialog.deferredW, XmNvalueChangedCallback, patTypeCB, nullptr);
	HighlightDialog.subPatW =
	    XtVaCreateManagedWidget("subPat", xmToggleButtonWidgetClass, typeBox, XmNmarginHeight, 0, XmNlabelString, s1 = XmStringCreateSimpleEx("Sub-pattern (processed within start & end of parent)"), XmNmnemonic, 'u', nullptr);
	XmStringFree(s1);
	XtAddCallback(HighlightDialog.subPatW, XmNvalueChangedCallback, patTypeCB, nullptr);
	HighlightDialog.colorPatW =
	    XtVaCreateManagedWidget("color", xmToggleButtonWidgetClass, typeBox, XmNmarginHeight, 0, XmNlabelString, s1 = XmStringCreateSimpleEx("Coloring for sub-expressions of parent pattern"), XmNmnemonic, 'g', nullptr);
	XmStringFree(s1);
	XtAddCallback(HighlightDialog.colorPatW, XmNvalueChangedCallback, patTypeCB, nullptr);

	HighlightDialog.matchLbl = XtVaCreateManagedWidget("matchLbl", xmLabelGadgetClass, patternsForm, XmNlabelString, s1 = XmStringCreateSimpleEx("Matching:"), XmNmarginHeight, 0, XmNalignment, XmALIGNMENT_BEGINNING, XmNleftAttachment,
	                                                   XmATTACH_POSITION, XmNleftPosition, LIST_RIGHT, XmNtopAttachment, XmATTACH_WIDGET, XmNtopOffset, BORDER, XmNtopWidget, typeBox, nullptr);
	XmStringFree(s1);

	matchBox = XtVaCreateManagedWidget("matchBox", xmRowColumnWidgetClass, patternsForm, XmNpacking, XmPACK_COLUMN, XmNradioBehavior, True, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, LIST_RIGHT, XmNtopAttachment,
	                                   XmATTACH_WIDGET, XmNtopWidget, HighlightDialog.matchLbl, nullptr);
	HighlightDialog.simpleW =
	    XtVaCreateManagedWidget("simple", xmToggleButtonWidgetClass, matchBox, XmNset, True, XmNmarginHeight, 0, XmNlabelString, s1 = XmStringCreateSimpleEx("Highlight text matching regular expression"), XmNmnemonic, 'x', nullptr);
	XmStringFree(s1);
	XtAddCallback(HighlightDialog.simpleW, XmNvalueChangedCallback, matchTypeCB, nullptr);
	HighlightDialog.rangeW =
	    XtVaCreateManagedWidget("range", xmToggleButtonWidgetClass, matchBox, XmNmarginHeight, 0, XmNlabelString, s1 = XmStringCreateSimpleEx("Highlight text between starting and ending REs"), XmNmnemonic, 'b', nullptr);
	XmStringFree(s1);
	XtAddCallback(HighlightDialog.rangeW, XmNvalueChangedCallback, matchTypeCB, nullptr);

	nameLbl = XtVaCreateManagedWidget("nameLbl", xmLabelGadgetClass, patternsForm, XmNlabelString, s1 = XmStringCreateSimpleEx("Pattern Name"), XmNmnemonic, 'N', XmNrows, 20, XmNalignment, XmALIGNMENT_BEGINNING, XmNleftAttachment,
	                                  XmATTACH_POSITION, XmNleftPosition, LIST_RIGHT, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, matchBox, XmNtopOffset, BORDER, nullptr);
	XmStringFree(s1);

	HighlightDialog.nameW = XtVaCreateManagedWidget("name", xmTextWidgetClass, patternsForm, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, LIST_RIGHT, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, nameLbl, XmNrightAttachment,
	                                                XmATTACH_POSITION, XmNrightPosition, (99 + LIST_RIGHT) / 2, nullptr);
	RemapDeleteKey(HighlightDialog.nameW);
	XtVaSetValues(nameLbl, XmNuserData, HighlightDialog.nameW, nullptr);

	HighlightDialog.parentLbl = XtVaCreateManagedWidget("parentLbl", xmLabelGadgetClass, patternsForm, XmNlabelString, s1 = XmStringCreateSimpleEx("Parent Pattern"), XmNmnemonic, 't', XmNrows, 20, XmNalignment, XmALIGNMENT_BEGINNING,
	                                                    XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, (99 + LIST_RIGHT) / 2 + 1, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, matchBox, XmNtopOffset, BORDER, nullptr);
	XmStringFree(s1);

	HighlightDialog.parentW = XtVaCreateManagedWidget("parent", xmTextWidgetClass, patternsForm, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, (99 + LIST_RIGHT) / 2 + 1, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget,
	                                                  HighlightDialog.parentLbl, XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, 99, nullptr);
	RemapDeleteKey(HighlightDialog.parentW);
	XtVaSetValues(HighlightDialog.parentLbl, XmNuserData, HighlightDialog.parentW, nullptr);

	HighlightDialog.startLbl = XtVaCreateManagedWidget("startLbl", xmLabelGadgetClass, patternsForm, XmNalignment, XmALIGNMENT_BEGINNING, XmNmnemonic, 'R', XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, HighlightDialog.parentW,
	                                                   XmNtopOffset, BORDER, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 1, nullptr);

	HighlightDialog.errorW = XtVaCreateManagedWidget("error", xmTextWidgetClass, patternsForm, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 1, XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, 99, XmNbottomAttachment,
	                                                 XmATTACH_POSITION, XmNbottomPosition, 99, nullptr);
	RemapDeleteKey(HighlightDialog.errorW);

	HighlightDialog.errorLbl =
	    XtVaCreateManagedWidget("errorLbl", xmLabelGadgetClass, patternsForm, XmNlabelString, s1 = XmStringCreateSimpleEx("Regular Expression Indicating Error in Match (Optional)"), XmNmnemonic, 'o', XmNuserData, HighlightDialog.errorW,
	                            XmNalignment, XmALIGNMENT_BEGINNING, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 1, XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, HighlightDialog.errorW, nullptr);
	XmStringFree(s1);

	HighlightDialog.endW = XtVaCreateManagedWidget("end", xmTextWidgetClass, patternsForm, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 1, XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, HighlightDialog.errorLbl,
	                                               XmNbottomOffset, BORDER, XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, 99, nullptr);
	RemapDeleteKey(HighlightDialog.endW);

	HighlightDialog.endLbl = XtVaCreateManagedWidget("endLbl", xmLabelGadgetClass, patternsForm, XmNmnemonic, 'E', XmNuserData, HighlightDialog.endW, XmNalignment, XmALIGNMENT_BEGINNING, XmNleftAttachment, XmATTACH_POSITION,
	                                                 XmNleftPosition, 1, XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, HighlightDialog.endW, nullptr);

	n = 0;
	XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT);
	n++;
	XtSetArg(args[n], XmNscrollHorizontal, False);
	n++;
	XtSetArg(args[n], XmNwordWrap, True);
	n++;
	XtSetArg(args[n], XmNrows, 3);
	n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET);
	n++;
	XtSetArg(args[n], XmNbottomWidget, HighlightDialog.endLbl);
	n++;
	XtSetArg(args[n], XmNbottomOffset, BORDER);
	n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET);
	n++;
	XtSetArg(args[n], XmNtopWidget, HighlightDialog.startLbl);
	n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION);
	n++;
	XtSetArg(args[n], XmNleftPosition, 1);
	n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_POSITION);
	n++;
	XtSetArg(args[n], XmNrightPosition, 99);
	n++;
	HighlightDialog.startW = XmCreateScrolledText(patternsForm, (String) "start", args, n);
	AddMouseWheelSupport(HighlightDialog.startW);
	XtManageChild(HighlightDialog.startW);
	MakeSingleLineTextW(HighlightDialog.startW);
	RemapDeleteKey(HighlightDialog.startW);
	XtVaSetValues(HighlightDialog.startLbl, XmNuserData, HighlightDialog.startW, nullptr);

	styleBtn = XtVaCreateManagedWidget("styleLbl", xmPushButtonWidgetClass, patternsForm, XmNlabelString, s1 = XmStringCreateLtoREx("Add / Modify\nStyle..."), XmNmnemonic, 'i', XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition,
	                                   LIST_RIGHT - 1, XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET, XmNbottomWidget, HighlightDialog.parentW, nullptr);
	XmStringFree(s1);
	XtAddCallback(styleBtn, XmNactivateCallback, styleDialogCB, nullptr);

	HighlightDialog.stylePulldown = createHighlightStylesMenu(patternsForm);
	n = 0;
	XtSetArg(args[n], XmNspacing, 0);
	n++;
	XtSetArg(args[n], XmNmarginWidth, 0);
	n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET);
	n++;
	XtSetArg(args[n], XmNbottomWidget, HighlightDialog.parentW);
	n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION);
	n++;
	XtSetArg(args[n], XmNleftPosition, 1);
	n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET);
	n++;
	XtSetArg(args[n], XmNrightWidget, styleBtn);
	n++;
	XtSetArg(args[n], XmNsubMenuId, HighlightDialog.stylePulldown);
	n++;
	HighlightDialog.styleOptMenu = XmCreateOptionMenu(patternsForm, (String) "styleOptMenu", args, n);
	XtManageChild(HighlightDialog.styleOptMenu);

	styleLbl = XtVaCreateManagedWidget("styleLbl", xmLabelGadgetClass, patternsForm, XmNlabelString, s1 = XmStringCreateSimpleEx("Highlight Style"), XmNmnemonic, 'S', XmNuserData, XtParent(HighlightDialog.styleOptMenu), XmNalignment,
	                                   XmALIGNMENT_BEGINNING, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 1, XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, HighlightDialog.styleOptMenu, nullptr);
	XmStringFree(s1);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM);
	n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION);
	n++;
	XtSetArg(args[n], XmNleftPosition, 1);
	n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_POSITION);
	n++;
	XtSetArg(args[n], XmNrightPosition, LIST_RIGHT - 1);
	n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET);
	n++;
	XtSetArg(args[n], XmNbottomWidget, styleLbl);
	n++;
	XtSetArg(args[n], XmNbottomOffset, BORDER);
	n++;
	HighlightDialog.managedListW = CreateManagedList(patternsForm, (String) "list", args, n, (void **)HighlightDialog.patterns, &HighlightDialog.nPatterns, MAX_PATTERNS, 18, getDisplayedCB, nullptr, setDisplayedCB, nullptr, freeItemCB);
	XtVaSetValues(patternsLbl, XmNuserData, HighlightDialog.managedListW, nullptr);

	/* Set initial default button */
	XtVaSetValues(form, XmNdefaultButton, okBtn, nullptr);
	XtVaSetValues(form, XmNcancelButton, closeBtn, nullptr);

	/* Handle mnemonic selection of buttons and focus to dialog */
	AddDialogMnemonicHandler(form, FALSE);

	/* Fill in the dialog information for the selected language mode */
	SetIntText(HighlightDialog.lineContextW, patSet == nullptr ? 1 : patSet->lineContext);
	SetIntText(HighlightDialog.charContextW, patSet == nullptr ? 0 : patSet->charContext);
	SetLangModeMenu(HighlightDialog.lmOptMenu, HighlightDialog.langModeName);
	updateLabels();

	/* Realize all of the widgets in the new dialog */
	RealizeWithoutForcingPosition(HighlightDialog.shell);
}

/*
** If a syntax highlighting dialog is up, ask to have the option menu for
** chosing highlight styles updated (via a call to createHighlightStylesMenu)
*/
static void updateHighlightStyleMenu(void) {
	Widget oldMenu;
	int patIndex;

	if (HighlightDialog.shell == nullptr)
		return;

	oldMenu = HighlightDialog.stylePulldown;
	HighlightDialog.stylePulldown = createHighlightStylesMenu(XtParent(XtParent(oldMenu)));
	XtVaSetValues(XmOptionButtonGadget(HighlightDialog.styleOptMenu), XmNsubMenuId, HighlightDialog.stylePulldown, nullptr);
	patIndex = ManagedListSelectedIndex(HighlightDialog.managedListW);
	if (patIndex == -1)
		setStyleMenu("Plain");
	else
		setStyleMenu(HighlightDialog.patterns[patIndex]->style);

	XtDestroyWidget(oldMenu);
}

/*
** If a syntax highlighting dialog is up, ask to have the option menu for
** chosing language mode updated (via a call to CreateLanguageModeMenu)
*/
void UpdateLanguageModeMenu(void) {
	Widget oldMenu;

	if (HighlightDialog.shell == nullptr)
		return;

	oldMenu = HighlightDialog.lmPulldown;
	HighlightDialog.lmPulldown = CreateLanguageModeMenu(XtParent(XtParent(oldMenu)), langModeCB, nullptr);
	XtVaSetValues(XmOptionButtonGadget(HighlightDialog.lmOptMenu), XmNsubMenuId, HighlightDialog.lmPulldown, nullptr);
	SetLangModeMenu(HighlightDialog.lmOptMenu, HighlightDialog.langModeName);

	XtDestroyWidget(oldMenu);
}

static void destroyCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	int i;

	XtFree((char *)HighlightDialog.langModeName);
	for (i = 0; i < HighlightDialog.nPatterns; i++)
		freePatternSrc(HighlightDialog.patterns[i], true);
	HighlightDialog.shell = nullptr;
}

static void langModeCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	char *modeName;
	patternSet *oldPatSet, *newPatSet;
	patternSet emptyPatSet = {nullptr, 1, 0, 0, nullptr};
	int i, resp;

	/* Get the newly selected mode name.  If it's the same, do nothing */
	XtVaGetValues(w, XmNuserData, &modeName, nullptr);
	if (!strcmp(modeName, HighlightDialog.langModeName))
		return;

	/* Look up the original version of the patterns being edited */
	oldPatSet = FindPatternSet(HighlightDialog.langModeName);
	if (oldPatSet == nullptr)
		oldPatSet = &emptyPatSet;

	/* Get the current information displayed by the dialog.  If it's bad,
	   give the user the chance to throw it out or go back and fix it.  If
	   it has changed, give the user the chance to apply discard or cancel. */
	newPatSet = getDialogPatternSet();

	if (newPatSet == nullptr) {
		if (DialogF(DF_WARN, HighlightDialog.shell, 2, "Incomplete Language Mode", "Discard incomplete entry\n"
		                                                                           "for current language mode?",
		            "Keep", "Discard") == 1) {
			SetLangModeMenu(HighlightDialog.lmOptMenu, HighlightDialog.langModeName);
			return;
		}
	} else if (patternSetsDiffer(oldPatSet, newPatSet)) {
		resp = DialogF(DF_WARN, HighlightDialog.shell, 3, "Language Mode", "Apply changes for language mode %s?", "Apply Changes", "Discard Changes", "Cancel", HighlightDialog.langModeName);
		if (resp == 3) {
			SetLangModeMenu(HighlightDialog.lmOptMenu, HighlightDialog.langModeName);
			return;
		}
		if (resp == 1) {
			updatePatternSet();
		}
	}

	if (newPatSet != nullptr)
		freePatternSet(newPatSet);

	/* Free the old dialog information */
	XtFree((char *)HighlightDialog.langModeName);
	for (i = 0; i < HighlightDialog.nPatterns; i++)
		freePatternSrc(HighlightDialog.patterns[i], true);

	/* Fill the dialog with the new language mode information */
	HighlightDialog.langModeName = XtNewStringEx(modeName);
	newPatSet = FindPatternSet(modeName);
	if (newPatSet == nullptr) {
		HighlightDialog.nPatterns = 0;
		SetIntText(HighlightDialog.lineContextW, 1);
		SetIntText(HighlightDialog.charContextW, 0);
	} else {
		for (i = 0; i < newPatSet->nPatterns; i++)
			HighlightDialog.patterns[i] = copyPatternSrc(&newPatSet->patterns[i], nullptr);
		HighlightDialog.nPatterns = newPatSet->nPatterns;
		SetIntText(HighlightDialog.lineContextW, newPatSet->lineContext);
		SetIntText(HighlightDialog.charContextW, newPatSet->charContext);
	}
	ChangeManagedListData(HighlightDialog.managedListW);
}

static void lmDialogCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	EditLanguageModes();
}

static void styleDialogCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	Widget selectedItem;
	char *style;

	XtVaGetValues(HighlightDialog.styleOptMenu, XmNmenuHistory, &selectedItem, nullptr);
	XtVaGetValues(selectedItem, XmNuserData, &style, nullptr);
	EditHighlightStyles(style);
}

static void okCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	/* change the patterns */
	if (!updatePatternSet())
		return;

	/* pop down and destroy the dialog */
	CloseAllPopupsFor(HighlightDialog.shell);
	XtDestroyWidget(HighlightDialog.shell);
}

static void applyCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	/* change the patterns */
	updatePatternSet();
}

static void checkCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	if (checkHighlightDialogData()) {
		DialogF(DF_INF, HighlightDialog.shell, 1, "Pattern compiled", "Patterns compiled without error", "OK");
	}
}

static void restoreCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	patternSet *defaultPatSet;
	int i, psn;

	defaultPatSet = readDefaultPatternSet(HighlightDialog.langModeName);
	if (defaultPatSet == nullptr) {
		DialogF(DF_WARN, HighlightDialog.shell, 1, "No Default Pattern", "There is no default pattern set\nfor language mode %s", "OK", HighlightDialog.langModeName);
		return;
	}

	if (DialogF(DF_WARN, HighlightDialog.shell, 2, "Discard Changes", "Are you sure you want to discard\n"
	                                                                  "all changes to syntax highlighting\n"
	                                                                  "patterns for language mode %s?",
	            "Discard", "Cancel", HighlightDialog.langModeName) == 2) {
		return;
	}

	/* if a stored version of the pattern set exists, replace it, if it
	   doesn't, add a new one */
	for (psn = 0; psn < NPatternSets; psn++)
		if (!strcmp(HighlightDialog.langModeName, PatternSets[psn]->languageMode))
			break;
	if (psn < NPatternSets) {
		freePatternSet(PatternSets[psn]);
		PatternSets[psn] = defaultPatSet;
	} else
		PatternSets[NPatternSets++] = defaultPatSet;

	/* Free the old dialog information */
	for (i = 0; i < HighlightDialog.nPatterns; i++)
		freePatternSrc(HighlightDialog.patterns[i], true);

	/* Update the dialog */
	HighlightDialog.nPatterns = defaultPatSet->nPatterns;
	for (i = 0; i < defaultPatSet->nPatterns; i++)
		HighlightDialog.patterns[i] = copyPatternSrc(&defaultPatSet->patterns[i], nullptr);
	SetIntText(HighlightDialog.lineContextW, defaultPatSet->lineContext);
	SetIntText(HighlightDialog.charContextW, defaultPatSet->charContext);
	ChangeManagedListData(HighlightDialog.managedListW);
}

static void deleteCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	int i, psn;

	if (DialogF(DF_WARN, HighlightDialog.shell, 2, "Delete Pattern", "Are you sure you want to delete\n"
	                                                                 "syntax highlighting patterns for\n"
	                                                                 "language mode %s?",
	            "Yes, Delete", "Cancel", HighlightDialog.langModeName) == 2) {
		return;
	}

	/* if a stored version of the pattern set exists, delete it from the list */
	for (psn = 0; psn < NPatternSets; psn++)
		if (!strcmp(HighlightDialog.langModeName, PatternSets[psn]->languageMode))
			break;
	if (psn < NPatternSets) {
		freePatternSet(PatternSets[psn]);
		memmove(&PatternSets[psn], &PatternSets[psn + 1], (NPatternSets - 1 - psn) * sizeof(patternSet *));
		NPatternSets--;
	}

	/* Free the old dialog information */
	for (i = 0; i < HighlightDialog.nPatterns; i++)
		freePatternSrc(HighlightDialog.patterns[i], true);

	/* Clear out the dialog */
	HighlightDialog.nPatterns = 0;
	SetIntText(HighlightDialog.lineContextW, 1);
	SetIntText(HighlightDialog.charContextW, 0);
	ChangeManagedListData(HighlightDialog.managedListW);
}

static void closeCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	/* pop down and destroy the dialog */
	CloseAllPopupsFor(HighlightDialog.shell);
	XtDestroyWidget(HighlightDialog.shell);
}

static void helpCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	Help(HELP_PATTERNS);
}

static void patTypeCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	updateLabels();
}

static void matchTypeCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	updateLabels();
}

static void *getDisplayedCB(void *oldItem, int explicitRequest, int *abort, void *cbArg) {

	(void)cbArg;

	highlightPattern *pat;

	/* If the dialog is currently displaying the "new" entry and the
	   fields are empty, that's just fine */
	if (oldItem == nullptr && dialogEmpty())
		return nullptr;

	/* If there are no problems reading the data, just return it */
	pat = readDialogFields(True);
	if (pat != nullptr)
		return (void *)pat;

	/* If there are problems, and the user didn't ask for the fields to be
	   read, give more warning */
	if (!explicitRequest) {
		if (DialogF(DF_WARN, HighlightDialog.shell, 2, "Discard Entry", "Discard incomplete entry\nfor current pattern?", "Keep", "Discard") == 2) {
			return oldItem == nullptr ? nullptr : (void *)copyPatternSrc((highlightPattern *)oldItem, nullptr);
		}
	}

	/* Do readDialogFields again without "silent" mode to display warning */
	pat = readDialogFields(False);
	*abort = True;
	return nullptr;
}

static void setDisplayedCB(void *item, void *cbArg) {

	(void)cbArg;

	highlightPattern *pat = (highlightPattern *)item;
	int isSubpat, isDeferred, isColorOnly, isRange;

	if (item == nullptr) {
		XmTextSetStringEx(HighlightDialog.nameW, "");
		XmTextSetStringEx(HighlightDialog.parentW, "");
		XmTextSetStringEx(HighlightDialog.startW, "");
		XmTextSetStringEx(HighlightDialog.endW, "");
		XmTextSetStringEx(HighlightDialog.errorW, "");
		RadioButtonChangeState(HighlightDialog.topLevelW, True, False);
		RadioButtonChangeState(HighlightDialog.deferredW, False, False);
		RadioButtonChangeState(HighlightDialog.subPatW, False, False);
		RadioButtonChangeState(HighlightDialog.colorPatW, False, False);
		RadioButtonChangeState(HighlightDialog.simpleW, True, False);
		RadioButtonChangeState(HighlightDialog.rangeW, False, False);
		setStyleMenu("Plain");
	} else {
		isSubpat = pat->subPatternOf != nullptr;
		isDeferred = pat->flags & DEFER_PARSING;
		isColorOnly = pat->flags & COLOR_ONLY;
		isRange = pat->endRE != nullptr;
		XmTextSetStringEx(HighlightDialog.nameW, pat->name);
		XmTextSetStringEx(HighlightDialog.parentW, pat->subPatternOf);
		XmTextSetStringEx(HighlightDialog.startW, pat->startRE);
		XmTextSetStringEx(HighlightDialog.endW, pat->endRE);
		XmTextSetStringEx(HighlightDialog.errorW, pat->errorRE);
		RadioButtonChangeState(HighlightDialog.topLevelW, !isSubpat && !isDeferred, False);
		RadioButtonChangeState(HighlightDialog.deferredW, !isSubpat && isDeferred, False);
		RadioButtonChangeState(HighlightDialog.subPatW, isSubpat && !isColorOnly, False);
		RadioButtonChangeState(HighlightDialog.colorPatW, isSubpat && isColorOnly, False);
		RadioButtonChangeState(HighlightDialog.simpleW, !isRange, False);
		RadioButtonChangeState(HighlightDialog.rangeW, isRange, False);
		setStyleMenu(pat->style);
	}
	updateLabels();
}

static void freeItemCB(void *item) {
	freePatternSrc((highlightPattern *)item, true);
}

/*
** Do a test compile of the patterns currently displayed in the highlight
** patterns dialog, and display warning dialogs if there are problems
*/
static int checkHighlightDialogData(void) {
	patternSet *patSet;
	int result;

	/* Get the pattern information from the dialog */
	patSet = getDialogPatternSet();
	if (patSet == nullptr)
		return False;

	/* Compile the patterns  */
	result = patSet->nPatterns == 0 ? True : TestHighlightPatterns(patSet);
	freePatternSet(patSet);
	return result;
}

/*
** Update the text field labels and sensitivity of various fields, based on
** the settings of the Pattern Type and Matching radio buttons in the highlight
** patterns dialog.
*/
static void updateLabels(void) {
	const char *startLbl;
	const char *endLbl;
	int endSense, errSense, matchSense, parentSense;
	XmString s1;

	if (XmToggleButtonGetState(HighlightDialog.colorPatW)) {
		startLbl = "Sub-expressions to Highlight in Parent's Starting \
Regular Expression (\\1, &, etc.)";
		endLbl = "Sub-expressions to Highlight in Parent Pattern's Ending \
Regular Expression";
		endSense = True;
		errSense = False;
		matchSense = False;
		parentSense = True;
	} else {
		endLbl = "Ending Regular Expression";
		matchSense = True;
		parentSense = XmToggleButtonGetState(HighlightDialog.subPatW);
		if (XmToggleButtonGetState(HighlightDialog.simpleW)) {
			startLbl = "Regular Expression to Match";
			endSense = False;
			errSense = False;
		} else {
			startLbl = "Starting Regular Expression";
			endSense = True;
			errSense = True;
		}
	}

	XtSetSensitive(HighlightDialog.parentLbl, parentSense);
	XtSetSensitive(HighlightDialog.parentW, parentSense);
	XtSetSensitive(HighlightDialog.endW, endSense);
	XtSetSensitive(HighlightDialog.endLbl, endSense);
	XtSetSensitive(HighlightDialog.errorW, errSense);
	XtSetSensitive(HighlightDialog.errorLbl, errSense);
	XtSetSensitive(HighlightDialog.errorLbl, errSense);
	XtSetSensitive(HighlightDialog.simpleW, matchSense);
	XtSetSensitive(HighlightDialog.rangeW, matchSense);
	XtSetSensitive(HighlightDialog.matchLbl, matchSense);
	XtVaSetValues(HighlightDialog.startLbl, XmNlabelString, s1 = XmStringCreateSimpleEx(startLbl), nullptr);
	XmStringFree(s1);
	XtVaSetValues(HighlightDialog.endLbl, XmNlabelString, s1 = XmStringCreateSimpleEx(endLbl), nullptr);
	XmStringFree(s1);
}

/*
** Set the styles menu in the currently displayed highlight dialog to show
** a particular style
*/
static void setStyleMenu(const char *styleName) {
	int i;
	Cardinal nItems;
	WidgetList items;
	Widget selectedItem;
	char *itemStyle;

	XtVaGetValues(HighlightDialog.stylePulldown, XmNchildren, &items, XmNnumChildren, &nItems, nullptr);
	if (nItems == 0)
		return;
	selectedItem = items[0];
	for (i = 0; i < (int)nItems; i++) {
		XtVaGetValues(items[i], XmNuserData, &itemStyle, nullptr);
		if (!strcmp(itemStyle, styleName)) {
			selectedItem = items[i];
			break;
		}
	}
	XtVaSetValues(HighlightDialog.styleOptMenu, XmNmenuHistory, selectedItem, nullptr);
}

/*
** Read the pattern fields of the highlight dialog, and produce an allocated
** highlightPattern structure reflecting the contents, or pop up dialogs
** telling the user what's wrong (Passing "silent" as True, suppresses these
** dialogs).  Returns nullptr on error.
*/
static highlightPattern *readDialogFields(int silent) {
	highlightPattern *pat;
	char *inPtr, *outPtr, *style;
	Widget selectedItem;
	int colorOnly;

	/* Allocate a pattern source structure to return, zero out fields
	   so that the whole pattern can be freed on error with freePatternSrc */
	pat = new highlightPattern;
	pat->endRE        = nullptr;
	pat->errorRE      = nullptr;
	pat->style        = nullptr;
	pat->subPatternOf = nullptr;

	/* read the type buttons */
	pat->flags = 0;
	colorOnly = XmToggleButtonGetState(HighlightDialog.colorPatW);
	if (XmToggleButtonGetState(HighlightDialog.deferredW))
		pat->flags |= DEFER_PARSING;
	else if (colorOnly)
		pat->flags = COLOR_ONLY;

	/* read the name field */
	pat->name = ReadSymbolicFieldTextWidget(HighlightDialog.nameW, "highlight pattern name", silent);
	if (pat->name == nullptr) {
		delete pat;
		return nullptr;
	}

	if (*pat->name == '\0') {
		if (!silent) {
			DialogF(DF_WARN, HighlightDialog.shell, 1, "Pattern Name", "Please specify a name\nfor the pattern", "OK");
			XmProcessTraversal(HighlightDialog.nameW, XmTRAVERSE_CURRENT);
		}
		XtFree((char *)pat->name);
		delete pat;
		return nullptr;
	}

	/* read the startRE field */
	pat->startRE = XmTextGetString(HighlightDialog.startW);
	if (*pat->startRE == '\0') {
		if (!silent) {
			DialogF(DF_WARN, HighlightDialog.shell, 1, "Matching Regex", "Please specify a regular\nexpression to match", "OK");
			XmProcessTraversal(HighlightDialog.startW, XmTRAVERSE_CURRENT);
		}
		freePatternSrc(pat, true);
		return nullptr;
	}

	/* Make sure coloring patterns contain only sub-expression references
	   and put it in replacement regular-expression form */
	if (colorOnly) {
		for (inPtr = pat->startRE, outPtr = pat->startRE; *inPtr != '\0'; inPtr++) {
			if (*inPtr != ' ' && *inPtr != '\t') {
				*outPtr++ = *inPtr;
			}
		}

		*outPtr = '\0';
		if (strspn(pat->startRE, "&\\123456789 \t") != strlen(pat->startRE) || (*pat->startRE != '\\' && *pat->startRE != '&') || strstr(pat->startRE, "\\\\") != nullptr) {
			if (!silent) {
				DialogF(DF_WARN, HighlightDialog.shell, 1, "Pattern Error", "The expression field in patterns which specify highlighting for\n"
				                                                            "a parent, must contain only sub-expression references in regular\n"
				                                                            "expression replacement form (&\\1\\2 etc.).  See Help -> Regular\n"
				                                                            "Expressions and Help -> Syntax Highlighting for more information",
				        "OK");
				XmProcessTraversal(HighlightDialog.startW, XmTRAVERSE_CURRENT);
			}
			freePatternSrc(pat, true);
			return nullptr;
		}
	}

	/* read the parent field */
	if (XmToggleButtonGetState(HighlightDialog.subPatW) || colorOnly) {
		if (TextWidgetIsBlank(HighlightDialog.parentW)) {
			if (!silent) {
				DialogF(DF_WARN, HighlightDialog.shell, 1, "Specify Parent Pattern", "Please specify a parent pattern", "OK");
				XmProcessTraversal(HighlightDialog.parentW, XmTRAVERSE_CURRENT);
			}
			freePatternSrc(pat, true);
			return nullptr;
		}
		pat->subPatternOf = XmTextGetString(HighlightDialog.parentW);
	}

	/* read the styles option menu */
	XtVaGetValues(HighlightDialog.styleOptMenu, XmNmenuHistory, &selectedItem, nullptr);
	XtVaGetValues(selectedItem, XmNuserData, &style, nullptr);
	pat->style = XtStringDup(style);

	/* read the endRE field */
	if (colorOnly || XmToggleButtonGetState(HighlightDialog.rangeW)) {
		pat->endRE = XmTextGetString(HighlightDialog.endW);
		if (!colorOnly && *pat->endRE == '\0') {
			if (!silent) {
				DialogF(DF_WARN, HighlightDialog.shell, 1, "Specify Regex", "Please specify an ending\nregular expression", "OK");
				XmProcessTraversal(HighlightDialog.endW, XmTRAVERSE_CURRENT);
			}
			freePatternSrc(pat, true);
			return nullptr;
		}
	}

	/* read the errorRE field */
	if (XmToggleButtonGetState(HighlightDialog.rangeW)) {
		pat->errorRE = XmTextGetString(HighlightDialog.errorW);
		if (*pat->errorRE == '\0') {
			XtFree((char *)pat->errorRE);
			pat->errorRE = nullptr;
		}
	}
	return pat;
}

/*
** Returns true if the pattern fields of the highlight dialog are set to
** the default ("New" pattern) state.
*/
static int dialogEmpty(void) {
	return TextWidgetIsBlank(HighlightDialog.nameW) && XmToggleButtonGetState(HighlightDialog.topLevelW) && XmToggleButtonGetState(HighlightDialog.simpleW) && TextWidgetIsBlank(HighlightDialog.parentW) &&
	       TextWidgetIsBlank(HighlightDialog.startW) && TextWidgetIsBlank(HighlightDialog.endW) && TextWidgetIsBlank(HighlightDialog.errorW);
}

/*
** Update the pattern set being edited in the Syntax Highlighting dialog
** with the information that the dialog is currently displaying, and
** apply changes to any window which is currently using the patterns.
*/
static int updatePatternSet(void) {
	patternSet *patSet;
	WindowInfo *window;
	int psn, oldNum = -1;

	/* Make sure the patterns are valid and compile */
	if (!checkHighlightDialogData())
		return False;

	/* Get the current data */
	patSet = getDialogPatternSet();
	if (patSet == nullptr)
		return False;

	/* Find the pattern being modified */
	for (psn = 0; psn < NPatternSets; psn++)
		if (!strcmp(HighlightDialog.langModeName, PatternSets[psn]->languageMode))
			break;

	/* If it's a new pattern, add it at the end, otherwise free the
	   existing pattern set and replace it */
	if (psn == NPatternSets) {
		PatternSets[NPatternSets++] = patSet;
		oldNum = 0;
	} else {
		oldNum = PatternSets[psn]->nPatterns;
		freePatternSet(PatternSets[psn]);
		PatternSets[psn] = patSet;
	}

	/* Find windows that are currently using this pattern set and
	   re-do the highlighting */
	for (window = WindowList; window != nullptr; window = window->next) {
		if (patSet->nPatterns > 0) {
			if (window->languageMode != PLAIN_LANGUAGE_MODE && strcmp(LanguageModeName(window->languageMode), patSet->languageMode) == 0) {
				/*  The user worked on the current document's language mode, so
				    we have to make some changes immediately. For inactive
				    modes, the changes will be activated on activation.  */
				if (oldNum == 0) {
					/*  Highlighting (including menu entry) was deactivated in
					    this function or in preferences.c::reapplyLanguageMode()
					    if the old set had no patterns, so reactivate menu entry. */
					if (window->IsTopDocument()) {
						XtSetSensitive(window->highlightItem, True);
					}

					/*  Reactivate highlighting if it's default  */
					window->highlightSyntax = GetPrefHighlightSyntax();
				}

				if (window->highlightSyntax) {
					StopHighlighting(window);
					if (window->IsTopDocument()) {
						XtSetSensitive(window->highlightItem, True);
						SetToggleButtonState(window, window->highlightItem, True, False);
					}
					StartHighlighting(window, True);
				}
			}
		} else {
			/*  No pattern in pattern set. This will probably not happen much,
			    but you never know.  */
			StopHighlighting(window);
			window->highlightSyntax = False;

			if (window->IsTopDocument()) {
				XtSetSensitive(window->highlightItem, False);
				SetToggleButtonState(window, window->highlightItem, False, False);
			}
		}
	}

	/* Note that preferences have been changed */
	MarkPrefsChanged();

	return True;
}

/*
** Get the current information that the user has entered in the syntax
** highlighting dialog.  Return nullptr if the data is currently invalid
*/
static patternSet *getDialogPatternSet(void) {
	int i, lineContext, charContext;
	patternSet *patSet;

	/* Get the current contents of the "patterns" dialog fields */
	if (!UpdateManagedList(HighlightDialog.managedListW, True))
		return nullptr;

	/* Get the line and character context values */
	if (GetIntTextWarn(HighlightDialog.lineContextW, &lineContext, "context lines", True) != TEXT_READ_OK)
		return nullptr;
	if (GetIntTextWarn(HighlightDialog.charContextW, &charContext, "context lines", True) != TEXT_READ_OK)
		return nullptr;

	/* Allocate a new pattern set structure and copy the fields read from the
	   dialog, including the modified pattern list into it */
	patSet = (patternSet *)XtMalloc(sizeof(patternSet));
	patSet->languageMode = XtNewStringEx(HighlightDialog.langModeName);
	patSet->lineContext  = lineContext;
	patSet->charContext  = charContext;
	patSet->nPatterns    = HighlightDialog.nPatterns;
	patSet->patterns     = (highlightPattern *)XtMalloc(sizeof(highlightPattern) * HighlightDialog.nPatterns);
	
	for (i = 0; i < HighlightDialog.nPatterns; i++)
		copyPatternSrc(HighlightDialog.patterns[i], &patSet->patterns[i]);
	return patSet;
}

/*
** Return True if "patSet1" and "patSet2" differ
*/
static int patternSetsDiffer(patternSet *patSet1, patternSet *patSet2) {
	int i;
	highlightPattern *pat1, *pat2;

	if (patSet1->lineContext != patSet2->lineContext)
		return True;
	if (patSet1->charContext != patSet2->charContext)
		return True;
	if (patSet1->nPatterns != patSet2->nPatterns)
		return True;
	for (i = 0; i < patSet2->nPatterns; i++) {
		pat1 = &patSet1->patterns[i];
		pat2 = &patSet2->patterns[i];
		if (pat1->flags != pat2->flags)
			return True;
		if (AllocatedStringsDiffer(pat1->name, pat2->name))
			return True;
		if (AllocatedStringsDiffer(pat1->startRE, pat2->startRE))
			return True;
		if (AllocatedStringsDiffer(pat1->endRE, pat2->endRE))
			return True;
		if (AllocatedStringsDiffer(pat1->errorRE, pat2->errorRE))
			return True;
		if (AllocatedStringsDiffer(pat1->style, pat2->style))
			return True;
		if (AllocatedStringsDiffer(pat1->subPatternOf, pat2->subPatternOf))
			return True;
	}
	return False;
}

/*
** Copy a highlight pattern data structure and all of the allocated data
** it contains.  If "copyTo" is non-null, use that as the top-level structure,
** otherwise allocate a new highlightPattern structure and return it as the
** function value.
*/
static highlightPattern *copyPatternSrc(highlightPattern *pat, highlightPattern *copyTo) {
	highlightPattern *newPat;

	if (copyTo == nullptr) {
		newPat = new highlightPattern;
	} else {
		newPat = copyTo;
	}

	newPat->name         = XtNewStringEx(pat->name);
	newPat->startRE      = XtNewStringEx(pat->startRE);
	newPat->endRE        = XtNewStringEx(pat->endRE);
	newPat->errorRE      = XtNewStringEx(pat->errorRE);
	newPat->style        = XtNewStringEx(pat->style);
	newPat->subPatternOf = XtNewStringEx(pat->subPatternOf);
	newPat->flags        = pat->flags;
	return newPat;
}

/*
** Free the allocated memory contained in a highlightPattern data structure
** If "freeStruct" is true, free the structure itself as well.
*/
static void freePatternSrc(highlightPattern *pat, bool freeStruct) {
	XtFree((char *)pat->name);
	XtFree(pat->startRE);
	XtFree(pat->endRE);
	XtFree(pat->errorRE);
	XtFree(pat->style);
	XtFree(pat->subPatternOf);
	if(freeStruct) {
		delete pat;
	}
}

/*
** Free the allocated memory contained in a patternSet data structure
** If "freeStruct" is true, free the structure itself as well.
*/
static void freePatternSet(patternSet *p) {

	for (int i = 0; i < p->nPatterns; i++)
		freePatternSrc(&p->patterns[i], false);
	XtFree((char *)p->languageMode);
	XtFree((char *)p->patterns);
	XtFree((char *)p);
}

#if 0
/*
** Free the allocated memory contained in a patternSet data structure
** If "freeStruct" is true, free the structure itself as well.
*/
static int lookupNamedPattern(patternSet *p, char *patternName)
{
    int i;
    
    for (i=0; i<p->nPatterns; i++)
      if (strcmp(p->patterns[i].name, patternName))
              return i;
    return -1;
}
#endif

/*
** Find the index into the HighlightStyles array corresponding to "styleName".
** If styleName is not found, return -1.
*/
static int lookupNamedStyle(const char *styleName) {

	for (int i = 0; i < NHighlightStyles; i++) {
		if (styleName == HighlightStyles[i]->name) {
			return i;
		}
	}

	return -1;
}

/*
** Returns a unique number of a given style name
*/
int IndexOfNamedStyle(const char *styleName) {
	return lookupNamedStyle(styleName);
}
