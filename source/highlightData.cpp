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
#include "DialogDrawingStyles.h"
#include "DialogLanguageModes.h"
#include "DialogSyntaxPatterns.h"
#include "DocumentWidget.h"
#include "FontType.h"
#include "HighlightPattern.h"
#include "HighlightStyle.h"
#include "MainWindow.h"
#include "PatternSet.h"
#include "TextBuffer.h"
#include "highlight.h"
#include "nedit.h"
#include "preferences.h"
#include "regularExp.h"
#include <QMessageBox>
#include <QPushButton>
#include <QResource>
#include <QtDebug>
#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstring>
#include <memory>

namespace {

// Names for the fonts that can be used for syntax highlighting 
const int N_FONT_TYPES = 4;

const QString FontTypeNames[] = {
    QLatin1String("Plain"),
    QLatin1String("Italic"),
    QLatin1String("Bold"),
    QLatin1String("Bold Italic")
};


DialogDrawingStyles *DrawingStyles = nullptr;
DialogSyntaxPatterns *SyntaxPatterns = nullptr;

}

// list of available highlight styles 
QList<HighlightStyle> HighlightStyles;

static bool isDefaultPatternSet(const PatternSet *patSet);
static bool styleError(const char *stringStart, const char *stoppedAt, const char *message);
static QVector<HighlightPattern> readHighlightPatterns(const char **inPtr, int withBraces, const char **errMsg, bool *ok);
static int lookupNamedStyle(const QString &styleName);
static int readHighlightPattern(const char **inPtr, const char **errMsg, HighlightPattern *pattern);
static PatternSet *highlightError(const char *stringStart, const char *stoppedAt, const char *message);
static PatternSet *readPatternSet(const char **inPtr);
static QString createPatternsString(const PatternSet *patSet, const QString &indentStr);

// Pattern sources loaded from the .nedit file or set by the user 
QList<PatternSet> PatternSets;

/*
** Read a string (from the  value of the styles resource) containing highlight
** styles information, parse it, and load it into the stored highlight style
** list (HighlightStyles) for this NEdit session.
*/
bool LoadStylesStringEx(const QString &string) {

	// TODO(eteran): implement this using better algorithms

    QByteArray stringBytes = string.toLatin1();

    const char *inString = stringBytes.data();
    const char *errMsg;
    const char *inPtr = inString;
    int i;

	for (;;) {

		// skip over blank space 
		inPtr += strspn(inPtr, " \t");

		// Allocate a language mode structure in which to store the info. 
        HighlightStyle hs;

		// read style name 
		QString name = ReadSymbolicFieldEx(&inPtr);
		if (name.isNull()) {
			return styleError(inString, inPtr, "style name required");
		}		
        hs.name = name;
		
		if (!SkipDelimiter(&inPtr, &errMsg)) {
			return styleError(inString,inPtr, errMsg);
		}

		// read color 
		QString color = ReadSymbolicFieldEx(&inPtr);
		if (color.isNull()) {
			return styleError(inString,inPtr, "color name required");
		}
		
        hs.color   = color;
        hs.bgColor = QString();
		
		if (SkipOptSeparator('/', &inPtr)) {
			// read bgColor
			QString s = ReadSymbolicFieldEx(&inPtr); // no error if fails 
			if(!s.isNull()) {
                hs.bgColor = s;
			}
	
		}
		
		if (!SkipDelimiter(&inPtr, &errMsg)) {
			return styleError(inString,inPtr, errMsg);
		}

		// read the font type 
        // NOTE(eteran): assumes success!
        QString fontStr = ReadSymbolicFieldEx(&inPtr);
		for (i = 0; i < N_FONT_TYPES; i++) {
            if (FontTypeNames[i] == fontStr) {
                hs.font = i;
				break;
			}
		}
	
		if (i == N_FONT_TYPES) {
			return styleError(inString, inPtr, "unrecognized font type");
		}

		/* pattern set was read correctly, add/change it in the list */\
		for (i = 0; i < HighlightStyles.size(); i++) {
            if (HighlightStyles[i].name == hs.name) {
                HighlightStyles[i] = hs;
				break;
			}
		}
		
		if (i == HighlightStyles.size()) {
            HighlightStyles.push_back(hs);
		}

		// if the string ends here, we're done 
		inPtr += strspn(inPtr, " \t\n");
		if (*inPtr == '\0') {
			return true;
		}
	}
}

/*
** Create a string in the correct format for the styles resource, containing
** all of the highlight styles information from the stored highlight style
** list (HighlightStyles) for this NEdit session.
*/
QString WriteStylesStringEx() {

    QString str;
    QTextStream out(&str);

    for (const HighlightStyle  &style : HighlightStyles) {

        out << QLatin1Char('\t')
            << style.name
            << QLatin1Char(':')
            << style.color;

        if (!style.bgColor.isNull()) {
            out << QLatin1Char('/')
                << style.bgColor;
        }

        out << QLatin1Char(':')
            << FontTypeNames[style.font]
            << QLatin1Char('\n');

	}

    // Get the output, and lop off the trailing newlines
    if(!str.isEmpty()) {
        str.chop(1);
    }
    return str;
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
bool LoadHighlightStringEx(const QString &string) {

	// TODO(eteran): rework this to actually use a modern approach
    QByteArray stringBytes = string.toLatin1();

    const char *inString = stringBytes.data();
	const char *inPtr = inString;

	for (;;) {

		// Read each pattern set, abort on error 
        PatternSet *patSet = readPatternSet(&inPtr);
		if(!patSet) {
			return false;
		}

		// Add/change the pattern set in the list 
        auto it = PatternSets.begin();
        for (; it != PatternSets.end(); ++it) {
            if (it->languageMode == patSet->languageMode) {
                *it = *patSet;
				break;
			}
		}

        if (it == PatternSets.end()) {
            PatternSets.push_back(*patSet);
		}

        delete patSet;

		// if the string ends here, we're done 
		inPtr += strspn(inPtr, " \t\n");
        if (*inPtr == '\0') {
			return true;
		}
	}
}

/*
** Create a string in the correct format for the highlightPatterns resource,
** containing all of the highlight pattern information from the stored
** highlight pattern list (PatternSets) for this NEdit session.
*/
QString WriteHighlightStringEx() {

    QString str;
    QTextStream out(&str);


    for (const PatternSet &patSet : PatternSets) {
        if (patSet.patterns.isEmpty()) {
			continue;
		}
		
        out << patSet.languageMode
            << QLatin1Char(':');

        if (isDefaultPatternSet(&patSet)) {
            out << QLatin1String("Default\n\t");
        } else {
            out << QString(QLatin1String("%1")).arg(patSet.lineContext)
                << QLatin1Char(':')
                << QString(QLatin1String("%1")).arg(patSet.charContext)
                << QLatin1String("{\n")
                << createPatternsString(&patSet, QLatin1String("\t\t"))
                << QLatin1String("\t}\n\t");
        }
	}

    if(!str.isEmpty()) {
        str.chop(2);
    };

    return str;
}

/*
** Find the font (font struct) associated with a named style.
** This routine must only be called with a valid styleName (call
** NamedStyleExists to find out whether styleName is valid).
*/
QFont FontOfNamedStyleEx(DocumentWidget *document, const QString &styleName) {

    int styleNo = lookupNamedStyle(styleName);

    if (styleNo < 0) {
        return GetPrefDefaultFont();
    } else {

        int fontNum = HighlightStyles[styleNo].font;

        switch(fontNum) {
        case BOLD_FONT:
            return document->boldFontStruct_;
        case ITALIC_FONT:
            return document->italicFontStruct_;
        case BOLD_ITALIC_FONT:
            return document->boldItalicFontStruct_;
        case PLAIN_FONT:
        default:
            return document->fontStruct_;
        }
    }
}

int FontOfNamedStyleIsBold(const QString &styleName) {
    int styleNo = lookupNamedStyle(styleName);

    if (styleNo < 0) {
		return 0;
    }

    int fontNum = HighlightStyles[styleNo].font;
	return (fontNum == BOLD_FONT || fontNum == BOLD_ITALIC_FONT);
}

int FontOfNamedStyleIsItalic(const QString &styleName) {
    int styleNo = lookupNamedStyle(styleName);

    if (styleNo < 0) {
		return 0;
    }

    int fontNum = HighlightStyles[styleNo].font;
	return (fontNum == ITALIC_FONT || fontNum == BOLD_ITALIC_FONT);
}

/*
** Find the color associated with a named style.  This routine must only be
** called with a valid styleName (call NamedStyleExists to find out whether
** styleName is valid).
*/
QString ColorOfNamedStyleEx(const QString &styleName) {
	int styleNo = lookupNamedStyle(styleName);

	if (styleNo < 0) {
		return QLatin1String("black");
	}
		
    return HighlightStyles[styleNo].color;
}

/*
** Find the background color associated with a named style.
*/
QString BgColorOfNamedStyleEx(const QString &styleName) {
	int styleNo = lookupNamedStyle(styleName);

	if (styleNo < 0) {
		return QLatin1String("");
	}
	
    return HighlightStyles[styleNo].bgColor;
	
}

/*
** Determine whether a named style exists
*/
bool NamedStyleExists(const QString &styleName) {
	return lookupNamedStyle(styleName) != -1;
}

/*
** Look through the list of pattern sets, and find the one for a particular
** language.  Returns nullptr if not found.
*/
PatternSet *FindPatternSet(const QString &langModeName) {

    for(PatternSet &patternSet : PatternSets) {
        if (langModeName == patternSet.languageMode) {
            return &patternSet;
		}
	}
	
	return nullptr;
}

/*
** Returns True if there are highlight patterns, or potential patterns
** not yet committed in the syntax highlighting dialog for a language mode,
*/
bool LMHasHighlightPatterns(const QString &languageMode) {
	if (FindPatternSet(languageMode) != nullptr) {
		return true;
	}
	
	
	return SyntaxPatterns != nullptr && SyntaxPatterns->LMHasHighlightPatterns(languageMode);
}

/*
** Change the language mode name of pattern sets for language "oldName" to
** "newName" in both the stored patterns, and the pattern set currently being
** edited in the dialog.
*/
void RenameHighlightPattern(const QString &oldName, const QString &newName) {

    for(PatternSet &patternSet : PatternSets) {
        if (patternSet.languageMode == oldName) {
            patternSet.languageMode = newName;
		}
	}
	
	if(SyntaxPatterns) {
        SyntaxPatterns->RenameHighlightPattern(oldName, newName);
	}
}

QString createPatternsString(const PatternSet *patSet, const QString &indentStr) {

    QString str;
    QTextStream out(&str);

    for(const HighlightPattern &pat : patSet->patterns) {

        out << indentStr
            << pat.name
            << QLatin1Char(':');

        if (!pat.startRE.isNull()) {
            out << MakeQuotedStringEx(pat.startRE);
        }
        out << QLatin1Char(':');

        if (!pat.endRE.isNull()) {
            out << MakeQuotedStringEx(pat.endRE);
        }
        out << QLatin1Char(':');

        if (!pat.errorRE.isNull()) {
            out << MakeQuotedStringEx(pat.errorRE);
        }
        out << QLatin1Char(':')
            << pat.style
            << QLatin1Char(':');

        if (!pat.subPatternOf.isNull()) {
            out << pat.subPatternOf;
        }

        out << QLatin1Char(':');

        if (pat.flags & DEFER_PARSING)            out << QLatin1Char('D');
        if (pat.flags & PARSE_SUBPATS_FROM_START) out << QLatin1Char('R');
        if (pat.flags & COLOR_ONLY)               out << QLatin1Char('C');
        out << QLatin1Char('\n');
	}

    return str;
}

/*
** Read in a pattern set character string, and advance *inPtr beyond it.
** Returns nullptr and outputs an error to stderr on failure.
*/
static PatternSet *readPatternSet(const char **inPtr) {
	const char *errMsg;
	const char *stringStart = *inPtr;

	// remove leading whitespace 
	*inPtr += strspn(*inPtr, " \t\n");

    auto patSet = std::make_unique<PatternSet>();

	// read language mode field 
    patSet->languageMode = ReadSymbolicFieldEx(inPtr);
	
    if (patSet->languageMode.isNull()) {
		return highlightError(stringStart, *inPtr, "language mode must be specified");
	}

	if (!SkipDelimiter(inPtr, &errMsg)) {
		return highlightError(stringStart, *inPtr, errMsg);
	}

	/* look for "Default" keyword, and if it's there, return the default
	   pattern set */
	if (!strncmp(*inPtr, "Default", 7)) {
		*inPtr += 7;
        PatternSet *retPatSet = readDefaultPatternSet(patSet->languageMode);
        if(!retPatSet) {
			return highlightError(stringStart, *inPtr, "No default pattern set");
        }
		return retPatSet;
	}

	// read line context field 
    if (!ReadNumericField(inPtr, &patSet->lineContext))
		return highlightError(stringStart, *inPtr, "unreadable line context field");
	if (!SkipDelimiter(inPtr, &errMsg))
		return highlightError(stringStart, *inPtr, errMsg);

	// read character context field 
    if (!ReadNumericField(inPtr, &patSet->charContext))
		return highlightError(stringStart, *inPtr, "unreadable character context field");

	// read pattern list 
	bool ok;
    QVector<HighlightPattern> patterns = readHighlightPatterns(inPtr, true, &errMsg, &ok);
	if (!ok) {
		return highlightError(stringStart, *inPtr, errMsg);
	}
	
    patSet->patterns = patterns;

	// pattern set was read correctly, make an allocated copy to return 
    return patSet.release();
}

/*
** Parse a set of highlight patterns into an array of HighlightPattern
** structures, and a language mode name.  If unsuccessful, returns nullptr with
** (statically allocated) message in "errMsg".
*/
static QVector<HighlightPattern> readHighlightPatterns(const char **inPtr, int withBraces, const char **errMsg, bool *ok) {

	// skip over blank space 
	*inPtr += strspn(*inPtr, " \t\n");

	// look for initial brace 
	if (withBraces) {
		if (**inPtr != '{') {
			*errMsg = "pattern list must begin with \"{\"";
			*ok = false;
			return QVector<HighlightPattern>();
		}
		(*inPtr)++;
	}

	/*
	** parse each pattern in the list
	*/
	
	QVector<HighlightPattern> ret;
	
	while (true) {
		*inPtr += strspn(*inPtr, " \t\n");
		if (**inPtr == '\0') {
			if (withBraces) {
				*errMsg = "end of pattern list not found";
				*ok = false;
				return QVector<HighlightPattern>();
			} else
				break;
		} else if (**inPtr == '}') {
			(*inPtr)++;
			break;
		}
		
	
		HighlightPattern pat;
		
		if (!readHighlightPattern(inPtr, errMsg, &pat)) {
			*ok = false;
			return QVector<HighlightPattern>();
		}
		
		ret.push_back(pat);
	}
	
	
	*ok = true;
	return ret;
}

static int readHighlightPattern(const char **inPtr, const char **errMsg, HighlightPattern *pattern) {

	// read the name field 
	QString name = ReadSymbolicFieldEx(inPtr);
	if (name.isNull()) {
		*errMsg = "pattern name is required";
		return false;
	}
	pattern->name = name;
	
	if (!SkipDelimiter(inPtr, errMsg))
		return false;

	// read the start pattern 
	if (!ReadQuotedStringEx(inPtr, errMsg, &pattern->startRE))
		return false;
	if (!SkipDelimiter(inPtr, errMsg))
		return false;

	// read the end pattern 
	
	if (**inPtr == ':') {
		pattern->endRE = QString();
	} else if (!ReadQuotedStringEx(inPtr, errMsg, &pattern->endRE)) {
		return false;
	}

	if (!SkipDelimiter(inPtr, errMsg))
		return false;

	// read the error pattern 
	if (**inPtr == ':') {
		pattern->errorRE = QString();
	} else if (!ReadQuotedStringEx(inPtr, errMsg, &pattern->errorRE)) {
		return false;
	}
	
	if (!SkipDelimiter(inPtr, errMsg))
		return false;

	// read the style field 
    pattern->style = ReadSymbolicFieldEx(inPtr);
	
	if (pattern->style.isNull()) {
		*errMsg = "style field required in pattern";
		return false;
	}
	
	if (!SkipDelimiter(inPtr, errMsg))
		return false;

	// read the sub-pattern-of field 
    pattern->subPatternOf = ReadSymbolicFieldEx(inPtr);
	
	if (!SkipDelimiter(inPtr, errMsg))
		return false;

	// read flags field 
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
			return false;
		}
	}
	return true;
}

PatternSet *readDefaultPatternSet(QByteArray &patternData, const QString &langModeName) {
	size_t modeNameLen = langModeName.size();
	
    if(patternData.startsWith(langModeName.toLatin1()) && patternData.data()[modeNameLen] == ':') {
		const char *strPtr = patternData.data();
        return readPatternSet(&strPtr);
	}

	return nullptr;
}

/*
** Given a language mode name, determine if there is a default (built-in)
** pattern set available for that language mode, and if so, read it and
** return a new allocated copy of it.  The returned pattern set should be
** freed by the caller with delete
*/

PatternSet *readDefaultPatternSet(const QString &langModeName) {
	for(int i = 0; i < 28; ++i) {
		QResource res(QString(QLatin1String("res/DefaultPatternSet%1.txt")).arg(i, 2, 10, QLatin1Char('0')));

		if(res.isValid()) {
			// NOTE(eteran): don't copy the data, if it's uncompressed, we can deal with it in place :-)
			auto data = QByteArray::fromRawData(reinterpret_cast<const char *>(res.data()), res.size());
			
			if(res.isCompressed()) {
				data = qUncompress(data);
			}

			if(PatternSet* patternSet = readDefaultPatternSet(data, langModeName)) {
				return patternSet;
			}
		}
	}
	
	return nullptr;
}

/*
** Return True if patSet exactly matches one of the default pattern sets
*/
static bool isDefaultPatternSet(const PatternSet *patSet) {

	PatternSet *defaultPatSet = readDefaultPatternSet(patSet->languageMode);
	if(!defaultPatSet) {
		return false;
	}
	
    bool retVal = (*patSet == *defaultPatSet);
	delete defaultPatSet;
	return retVal;
}

/*
** Short-hand functions for formating and outputing errors for
*/
static PatternSet *highlightError(const char *stringStart, const char *stoppedAt, const char *message) {
    ParseErrorEx(nullptr, QString::fromLatin1(stringStart), stoppedAt - stringStart, QLatin1String("highlight pattern"), QString::fromLatin1(message));
	return nullptr;
}

static bool styleError(const char *stringStart, const char *stoppedAt, const char *message) {
    ParseErrorEx(nullptr, QString::fromLatin1(stringStart), stoppedAt - stringStart, QLatin1String("style specification"), QString::fromLatin1(message));
	return false;
}

/*
** Present a dialog for editing highlight style information
*/
void EditHighlightStyles(QWidget *parent, const QString &initialStyle) {

	if(!DrawingStyles) {
        DrawingStyles = new DialogDrawingStyles(parent);
	}
	
    DrawingStyles->setStyleByName(initialStyle);
	DrawingStyles->show();
	DrawingStyles->raise();
}

/*
** Present a dialog for editing highlight pattern information
*/
void EditHighlightPatterns(MainWindow *window) {

	if(SyntaxPatterns) {
		SyntaxPatterns->show();
		SyntaxPatterns->raise();
		return;	
	}
	
	if (LanguageModeName(0).isNull()) {
	
        QMessageBox::warning(window, QLatin1String("No Language Modes"),
			QLatin1String("No Language Modes available for syntax highlighting\n"
			              "Add language modes under Preferenses->Language Modes"));
		return;
    }

    DocumentWidget *document = window->currentDocument();
	
    QString languageName = LanguageModeName(document->languageMode_ == PLAIN_LANGUAGE_MODE ? 0 : document->languageMode_);
    SyntaxPatterns = new DialogSyntaxPatterns(window);
	SyntaxPatterns->setLanguageName(languageName);
	SyntaxPatterns->show();
	SyntaxPatterns->raise();	
}

/*
** If a syntax highlighting dialog is up, ask to have the option menu for
** chosing highlight styles updated (via a call to createHighlightStylesMenu)
*/
void updateHighlightStyleMenu() {
	if(!SyntaxPatterns) {
		return;
	}
	
	SyntaxPatterns->updateHighlightStyleMenu();
}

/*
** If a syntax highlighting dialog is up, ask to have the option menu for
** chosing language mode updated (via a call to CreateLanguageModeMenu)
*/
void UpdateLanguageModeMenu() {

	if(!SyntaxPatterns) {
		return;
	}
	
	SyntaxPatterns->UpdateLanguageModeMenu();
}

/*
** Find the index into the HighlightStyles array corresponding to "styleName".
** If styleName is not found, return -1.
*/
static int lookupNamedStyle(const QString &styleName) {

	for (int i = 0; i < HighlightStyles.size(); i++) {
        if (HighlightStyles[i].name == styleName) {
			return i;
		}
	}
	
	return -1;
}

/*
** Returns a unique number of a given style name
*/
int IndexOfNamedStyle(const QString &styleName) {
	return lookupNamedStyle(styleName);
}
