/*******************************************************************************
*                                                                              *
* smartIndent.c -- Maintain, and allow user to edit, macros for smart indent   *
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
* July, 1997                                                                   *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/


#include "smartIndent.h"
#include "DialogLanguageModes.h"
#include "DialogSmartIndent.h"
#include "DialogSmartIndentCommon.h"
#include "DocumentWidget.h"
#include "IndentStyle.h"
#include "SmartIndentEntry.h"
#include "TextBuffer.h"
#include "WrapStyle.h"
#include "interpret.h"
#include "macro.h"
#include "nedit.h"
#include "parse.h"
#include "preferences.h"
#include "Input.h"
#include "shift.h"

#include <QMessageBox>
#include <QResource>
#include <QtDebug>
#include <climits>
#include <cstdio>
#include <cstring>

namespace {

const char MacroEndBoundary[] = "--End-of-Macro--";

DialogSmartIndent *SmartIndentDlg = nullptr;

}

QList<SmartIndentEntry> SmartIndentSpecs;
QString CommonMacros;

static void insertShiftedMacro(QTextStream &ts, const QString &macro);
static bool isDefaultIndentSpec(const SmartIndentEntry *indentSpec);
static bool loadDefaultIndentSpec(const QString &lmName);
static int siParseError(const Input &in, const QString &message);
static QString readSIMacroEx(Input &in);


/**
 * @brief defaultCommonMacros
 * @return
 */
QByteArray defaultCommonMacros() {

	static bool loaded = false;
	static QByteArray defaultsMacros;
	
	if(!loaded) {
		QResource res(QLatin1String("res/DefaultCommonMacros.txt"));
		if(res.isValid()) {
            // don't copy the data, if it's uncompressed, we can deal with it in place :-)
			defaultsMacros = QByteArray::fromRawData(reinterpret_cast<const char *>(res.data()), res.size());

			if(res.isCompressed()) {
				defaultsMacros = qUncompress(defaultsMacros);
			}

			loaded = true;
		}
	}
	
	return defaultsMacros;
}

SmartIndentEntry DefaultIndentSpecs[N_DEFAULT_INDENT_SPECS] = {
	{
		QLatin1String("C")
		,
		QLatin1String("# C Macros and tuning parameters are shared with C++, and are declared\n"
		              "# in the common section.  Press Common / Shared Initialization above.\n")
		,
		QLatin1String("return cFindSmartIndentDist($1)\n")
		,
		QLatin1String("if ($2 == \"}\" || $2 == \"{\" || $2 == \"#\")\n"
		              "    cBraceOrPound($1, $2)\n")
	},{
		QLatin1String("C++")
		,
		QLatin1String("# C++ Macros and tuning parameters are shared with C, and are declared\n"
		              "# in the common section.  Press Common / Shared Initialization above.\n")
		,
		QLatin1String("return cFindSmartIndentDist($1)\n")
		,
		QLatin1String("if ($2 == \"}\" || $2 == \"{\" || $2 == \"#\")\n"
		              "    cBraceOrPound($1, $2)\n")
	},{
		QLatin1String("Python")
		,
		QLatin1String("# Number of characters in a normal indent level.  May be a number, or the\n"
		              "# string \"default\", meaning, guess the value from the current tab settings.\n"
		              "$pyIndentDist = \"default\"\n")
		,
		QLatin1String("if (get_range($1-1, $1) != \":\")\n"
		              "    return -1\n"
		              "return measureIndent($1) + defaultIndent($pyIndentDist)\n")
		,
		QString()
	},{
		QLatin1String("Matlab")
		,
		QLatin1String("# Number of spaces to indent \"case\" statements\n"
		              "$caseDepth = 2\n"
		              "define matlabNewlineMacro\n"
		              "{\n"
		              "   if (!$em_tab_dist)\n"
		              "        tabsize = $tab_dist\n"
		              "   else\n"
		              " 	   tabsize = $em_tab_dist\n"
		              "   startLine = startOfLine($1)\n"
		              "   indentLevel = measureIndent($1)\n"
		              "\n"
		              "   # If this line is continued on next, return default:\n"
		              "   lastLine = get_range(startLine, $1)\n"
		              "   if (search_string(lastLine, \"...\", 0) != -1) {\n"
		              " 	 if ($n_args == 2)\n"
		              " 		return matlabNewlineMacro(startLine - 1, 1)\n"
		              " 	 else {\n"
		              " 		return -1\n"
		              " 	 }\n"
		              "   }\n"
		              "\n"
		              "   # Correct the indentLevel if this was a continued line.\n"
		              "   while (startLine > 1)\n"
		              "   {\n"
		              " 	 endLine = startLine - 1\n"
		              " 	 startLine = startOfLine(endLine)\n"
		              " 	 lastLine = get_range(startLine, endLine)\n"
		              " 	 # No \"...\" means we've found the root\n"
		              " 	 if (search_string(lastLine, \"...\", 0) == -1) {\n"
		              " 		startLine = endLine + 1\n"
		              " 		break\n"
					  " 	 }\n"
					  "   }\n"
					  "   indentLevel = measureIndent(startLine)\n"
		              "\n"
		              "   # Get the first word of the indentLevel line\n"
		              "   FWend = search(\">|\\n\", startLine + indentLevel, \"regex\")\n"
		              "   # This search fails on EOF\n"
		              "   if (FWend == -1)\n"
		              " 	 FWend = $1\n"
		              "\n"
		              "   firstWord = get_range(startLine + indentLevel, FWend)\n"
		              "\n"
		              "   # How shall we change the indent level based on the first word?\n"
		              "   if (search_string(firstWord, \\\n"
		              " 		\"<for>|<function>|<if>|<switch>|<try>|<while>\", 0, \"regex\") == 0) {\n"
		              " 	 return indentLevel + tabsize\n"
		              "   }\n"
		              "   else if ((firstWord == \"end\") || (search_string(firstWord, \\\n"
					  " 		   \"<case>|<catch>|<else>|<elseif>|<otherwise>\", 0, \"regex\") == 0)) {\n"
		              " 	 # Get the last indent level \n"
		              " 	 if (startLine > 0) # avoid infinite loop\n"
		              "    last_indent = matlabNewlineMacro(startLine - 1, 1)\n"
		              " 	 else\n"
		              " 		last_indent = indentLevel\n"
		              "\n"
		              " 	 # Re-indent this line\n"
		              " 	 if ($n_args == 1) {\n"
		              " 		if (firstWord == \"case\" || firstWord == \"otherwise\")\n"
		              " 		   replace_range(startLine, startLine + indentLevel, \\\n"
		              " 						 makeIndentString(last_indent - tabsize + $caseDepth))\n"
		              " 		else\n"
		              " 		   replace_range(startLine, startLine + indentLevel, \\\n"
		              " 						 makeIndentString(last_indent - tabsize))\n"
		              " 	 }\n"
					  "\n"
					  " 	 if (firstWord == \"end\") {\n"
					  " 		return max(last_indent - tabsize, 0)\n"
					  " 	 }\n"
					  " 	 else {\n"
					  " 		return last_indent\n"
					  " 	 }\n"
					  "   } \n"
					  "   else {\n"
					  " 	 return indentLevel\n"
					  "   }\n"
					  "}\n")
		,
		QLatin1String("return matlabNewlineMacro($1)\n")
		,
		QString()
	}
};

void EndSmartIndentEx(DocumentWidget *document) {
	SmartIndentData *winData = document->smartIndentData_;

	if(!winData) {
        return;
	}

    // Free programs and allocated data
    if (winData->modMacro) {
        FreeProgram(winData->modMacro);
    }

    FreeProgram(winData->newlineMacro);

    delete winData;
	document->smartIndentData_ = nullptr;
}

/*
** Returns true if there are smart indent macros for a named language
*/
int SmartIndentMacrosAvailable(const QString &languageModeName) {
	return findIndentSpec(languageModeName) != nullptr;
}

bool InSmartIndentMacrosEx(DocumentWidget *document) {
	SmartIndentData *winData = document->smartIndentData_;
	return winData && (winData->inModMacro || winData->inNewLineMacro);
}

static bool loadDefaultIndentSpec(const QString &lmName) {

	for (int i = 0; i < N_DEFAULT_INDENT_SPECS; i++) {
        if (DefaultIndentSpecs[i].lmName == lmName) {
            SmartIndentSpecs.push_back(DefaultIndentSpecs[i]);
			return true;
		}
	}
	return false;
}

int LoadSmartIndentStringEx(const QString &string) {

	Input in(&string);
	QString errMsg;
	int i;

	for (;;) {
		SmartIndentEntry is;

		// skip over blank space
		in.skipWhitespaceNL();

		// finished
		if (in.atEnd()) {
			return true;
		}

		// read language mode name
		is.lmName = ReadSymbolicFieldEx(in);
		if (is.lmName.isNull()) {
			return siParseError(in, QLatin1String("language mode name required"));
		}

		if (!SkipDelimiterEx(in, &errMsg)) {
			return siParseError(in, errMsg);
		}

		/* look for "Default" keyword, and if it's there, return the default
		   smart indent macros */
		if (in.match(QLatin1String("Default"))) {
			in += 7;
			if (!loadDefaultIndentSpec(is.lmName)) {
				return siParseError(in, QLatin1String("no default smart indent macros"));
			}
			continue;
		}

		/* read the initialization macro (arbitrary text terminated by the
		   macro end boundary string) */
		is.initMacro = readSIMacroEx(in);
		if(is.initMacro.isNull()) {
			return siParseError(in, QLatin1String("no end boundary to initialization macro"));
		}

		// read the newline macro
		is.newlineMacro = readSIMacroEx(in);
		if(is.newlineMacro.isNull()) {
			return siParseError(in, QLatin1String("no end boundary to newline macro"));
		}

		// read the modify macro
		is.modMacro = readSIMacroEx(in);
		if(is.modMacro.isNull()) {
			return siParseError(in, QLatin1String("no end boundary to modify macro"));
		}

		// if there's no mod macro, make it null so it won't be executed
		if (is.modMacro.isEmpty()) {
			is.modMacro = QString();
		}

		for (i = 0; i < SmartIndentSpecs.size(); i++) {
			if (SmartIndentSpecs[i].lmName == is.lmName) {
				SmartIndentSpecs[i] = is;
				break;
			}
		}

		if (i == SmartIndentSpecs.size()) {
			SmartIndentSpecs.push_back(is);
		}
	}
}

int LoadSmartIndentCommonStringEx(const QString &string) {

	Input in(&string);

	// If called from -import, can replace existing ones

	// skip over blank space
	in.skipWhitespaceNL();

	/* look for "Default" keyword, and if it's there, return the default
	   smart common macro */
	if (in.match(QLatin1String("Default"))) {

		QByteArray defaults = defaultCommonMacros();
		CommonMacros = QString::fromLatin1(defaults);
		return true;
	}

	// Remove leading tabs added by writer routine
	std::string newMacros = ShiftTextEx(in.mid().toStdString(), SHIFT_LEFT, true, 8, 8);
	CommonMacros = QString::fromStdString(newMacros);
	return true;
}

/*
** Read a macro (arbitrary text terminated by the macro end boundary string)
** from the position pointed to by *inPtr, trim off added tabs and return an
** allocated copy of the string, and advance *inPtr to the end of the macro.
** Returns nullptr if the macro end boundary string is not found.
*/
static QString readSIMacroEx(Input &in) {
	// Strip leading newline
	if (*in == QLatin1Char('\n')) {
		++in;
	}

	// Find the end of the macro
	int macroEnd = in.find(QString::fromLatin1(MacroEndBoundary));
	if(macroEnd == -1) {
		return QString();
	}

	// Copy the macro
	std::string macroStr = in.mid(macroEnd - in.index()).toStdString();

	// Remove leading tabs added by writer routine
	in += macroEnd - in.index();
	in += strlen(MacroEndBoundary);

	return QString::fromStdString(ShiftTextEx(macroStr, SHIFT_LEFT, true, 8, 8));
}

static int siParseError(const Input &in, const QString &message) {
	return ParseErrorEx(
	            nullptr,
                *in.string(),
                in.index(),
	            QLatin1String("smart indent specification"),
	            message);

}

QString WriteSmartIndentStringEx() {

	QString s;
	QTextStream ts(&s);

    for(const SmartIndentEntry &sis : SmartIndentSpecs) {

		ts << QLatin1String("\t")
           << sis.lmName
		   << QLatin1String(":");
		
        if (isDefaultIndentSpec(&sis)) {
			ts << QLatin1String("Default\n");
		} else {
            insertShiftedMacro(ts, sis.initMacro);
            insertShiftedMacro(ts, sis.newlineMacro);
            insertShiftedMacro(ts, sis.modMacro);
		}
	}
	
	// Get the output string, and lop off the trailing newline 
	if(!s.isEmpty()) {
		s.chop(1);
	}

	// Protect newlines and backslashes from translation by the resource reader
    return s;
}

QString WriteSmartIndentCommonStringEx() {

	QByteArray defaults = defaultCommonMacros();
	if (CommonMacros == QString::fromLatin1(defaults.data(), defaults.size())) {
		return QLatin1String("Default");
	}

	if(CommonMacros.isNull()) {
		return QLatin1String("");
	}

	// Shift the macro over by a tab to keep .nedit file bright and clean 
    std::string outStr = ShiftTextEx(CommonMacros.toStdString(), SHIFT_RIGHT, true, 8, 8);

	/* Protect newlines and backslashes from translation by the resource
	   reader */

	// If there's a trailing escaped newline, remove it 
	const size_t len = outStr.size();
	if (len > 1 && outStr[len - 1] == '\n' && outStr[len - 2] == '\\') {
		outStr.resize(len - 2);
	}
	
	return QString::fromStdString(outStr);
}

/*
** Insert macro text "macro" into buffer "buf" shifted right by 8 characters
** (so it looks nice in the .nedit file), and terminated with a macro-end-
** boundary string.
*/
static void insertShiftedMacro(QTextStream &ts, const QString &macro) {

	if (!macro.isNull()) {
		std::string shiftedMacro = ShiftTextEx(macro.toStdString(), SHIFT_RIGHT, true, 8, 8);
		ts << QString::fromStdString(shiftedMacro);
	}
	
	ts << QLatin1String("\t");
    ts << QString::fromLatin1(MacroEndBoundary);
	ts << QLatin1String("\n");
}

static bool isDefaultIndentSpec(const SmartIndentEntry *indentSpec) {

	for (int i = 0; i < N_DEFAULT_INDENT_SPECS; i++) {
		if (indentSpec->lmName == DefaultIndentSpecs[i].lmName) {
			return (*indentSpec == DefaultIndentSpecs[i]);
		}
	}
	return false;
}

const SmartIndentEntry *findIndentSpec(const QString &modeName) {

    if(modeName.isNull()) {
		return nullptr;
	}

    for(const SmartIndentEntry &sis : SmartIndentSpecs) {
        if (sis.lmName == modeName) {
            return &sis;
		}
	}
	return nullptr;
}

/*
** Returns True if there are smart indent macros, or potential macros
** not yet committed in the smart indent dialog for a language mode,
*/
int LMHasSmartIndentMacros(const QString &languageMode) {
	if (findIndentSpec(languageMode) != nullptr) {
		return true;
	}
	
	
    return SmartIndentDlg != nullptr && SmartIndentDlg->hasSmartIndentMacros(languageMode);
}

/*
** Change the language mode name of smart indent macro sets for language
** "oldName" to "newName" in both the stored macro sets, and the pattern set
** currently being edited in the dialog.
*/
void RenameSmartIndentMacros(const QString &oldName, const QString &newName) {

    for(SmartIndentEntry &sis : SmartIndentSpecs) {
        if (sis.lmName == oldName) {
            sis.lmName = newName;
		}
    }
	if (SmartIndentDlg) {
        if(SmartIndentDlg->languageMode_ == oldName) {
            SmartIndentDlg->setLanguageMode(newName);
		}
	}
}

/*
** If a smart indent dialog is up, ask to have the option menu for
** chosing language mode updated (via a call to CreateLanguageModeMenu)
*/
void UpdateLangModeMenuSmartIndent() {

	if(SmartIndentDlg) {
		SmartIndentDlg->updateLanguageModes();
	}
}
