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

#include <QMessageBox>
#include "ui/DialogLanguageModes.h"
#include "ui/DialogSmartIndentCommon.h"
#include "ui/DialogSmartIndent.h"
#include "IndentStyle.h"
#include "WrapStyle.h"
#include "SmartIndent.h"

#include "smartIndent.h"
#include "Document.h"
#include "help.h"
#include "interpret.h"
#include "macro.h"
#include "MotifHelper.h"
#include "nedit.h"
#include "parse.h"
#include "preferences.h"
#include "shift.h"
#include "TextBuffer.h"
#include "text.h"
#include "misc.h"
#include "window.h"

#include <cstdio>
#include <cstring>
#include <climits>

namespace {

struct windowSmartIndentData {
	Program *newlineMacro;
	int inNewLineMacro;
	Program *modMacro;
	int inModMacro;
};

const char MacroEndBoundary[] = "--End-of-Macro--";

}

int NSmartIndentSpecs = 0;
SmartIndent *SmartIndentSpecs[MAX_LANGUAGE_MODES];
char *CommonMacros = nullptr;

DialogSmartIndent *SmartIndentDlg = nullptr;

static void executeNewlineMacro(Document *window, smartIndentCBStruct *cbInfo);
static void executeModMacro(Document *window, smartIndentCBStruct *cbInfo);
static void insertShiftedMacro(TextBuffer *buf, char *macro);
static int isDefaultIndentSpec(SmartIndent *indentSpec);
static int loadDefaultIndentSpec(const char *lmName);
static int siParseError(const char *stringStart, const char *stoppedAt, const char *message);

static char *readSIMacro(const char **inPtr);
static int indentSpecsDiffer(SmartIndent *is1, SmartIndent *is2);
static int LoadSmartIndentCommonString(char *inString);
static int LoadSmartIndentString(char *inString);

SmartIndent DefaultIndentSpecs[N_DEFAULT_INDENT_SPECS] = {
	{
		"C"
		,
		"# C Macros and tuning parameters are shared with C++, and are declared\n"
		"# in the common section.  Press Common / Shared Initialization above.\n"
		,
		"return cFindSmartIndentDist($1)\n"
		,
		"if ($2 == \"}\" || $2 == \"{\" || $2 == \"#\")\n"
		"    cBraceOrPound($1, $2)\n"
	},{
		"C++"
		,
		"# C++ Macros and tuning parameters are shared with C, and are declared\n"
		"# in the common section.  Press Common / Shared Initialization above.\n"
		,
		"return cFindSmartIndentDist($1)\n"
		,
		"if ($2 == \"}\" || $2 == \"{\" || $2 == \"#\")\n"
		"    cBraceOrPound($1, $2)\n"
	},{
		"Python"
		,
		"# Number of characters in a normal indent level.  May be a number, or the\n"
		"# string \"default\", meaning, guess the value from the current tab settings.\n"
		"$pyIndentDist = \"default\"\n"
		,
		"if (get_range($1-1, $1) != \":\")\n"
		"    return -1\n"
		"return measureIndent($1) + defaultIndent($pyIndentDist)\n"
		,
		nullptr
	},{
		"Matlab"
		,
		"# Number of spaces to indent \"case\" statements\n"
		"$caseDepth = 2\n"
		"define matlabNewlineMacro\n"
		"{\n"
		"   if (!$em_tab_dist)\n"
		"        tabsize = $tab_dist\n"
		"   else\n"
		"        tabsize = $em_tab_dist\n"
		"   startLine = startOfLine($1)\n"
		"   indentLevel = measureIndent($1)\n"
		"\n"
		"   # If this line is continued on next, return default:\n"
		"   lastLine = get_range(startLine, $1)\n"
		"   if (search_string(lastLine, \"...\", 0) != -1) {\n"
		"      if ($n_args == 2)\n"
		"         return matlabNewlineMacro(startLine - 1, 1)\n"
		"      else {\n"
		"         return -1\n"
		"      }\n"
		"   }\n"
		"\n"
		"   # Correct the indentLevel if this was a continued line.\n"
		"   while (startLine > 1)\n"
		"   {\n"
		"      endLine = startLine - 1\n"
		"      startLine = startOfLine(endLine)\n"
		"      lastLine = get_range(startLine, endLine)\n"
		"      # No \"...\" means we've found the root\n"
		"      if (search_string(lastLine, \"...\", 0) == -1) {\n"
		"         startLine = endLine + 1\n"
		"         break\n"
		"      }\n"
		"   }\n"
		"   indentLevel = measureIndent(startLine)\n"
		"\n"
		"   # Get the first word of the indentLevel line\n"
		"   FWend = search(\">|\\n\", startLine + indentLevel, \"regex\")\n"
		"   # This search fails on EOF\n"
		"   if (FWend == -1)\n"
		"      FWend = $1\n"
		"\n"
		"   firstWord = get_range(startLine + indentLevel, FWend)\n"
		"\n"
		"   # How shall we change the indent level based on the first word?\n"
		"   if (search_string(firstWord, \\\n"
		"         \"<for>|<function>|<if>|<switch>|<try>|<while>\", 0, \"regex\") == 0) {\n"
		"      return indentLevel + tabsize\n"
		"   }\n"
		"   else if ((firstWord == \"end\") || (search_string(firstWord, \\\n"
		"            \"<case>|<catch>|<else>|<elseif>|<otherwise>\", 0, \"regex\") == 0)) {\n"
		"      # Get the last indent level \n"
		"      if (startLine > 0) # avoid infinite loop\n"
		"	 last_indent = matlabNewlineMacro(startLine - 1, 1)\n"
		"      else\n"
		"         last_indent = indentLevel\n"
		"\n"
		"      # Re-indent this line\n"
		"      if ($n_args == 1) {\n"
		"         if (firstWord == \"case\" || firstWord == \"otherwise\")\n"
		"            replace_range(startLine, startLine + indentLevel, \\\n"
		"                          makeIndentString(last_indent - tabsize + $caseDepth))\n"
		"         else\n"
		"            replace_range(startLine, startLine + indentLevel, \\\n"
		"                          makeIndentString(last_indent - tabsize))\n"
		"      }\n"
		"\n"
		"      if (firstWord == \"end\") {\n"
		"         return max(last_indent - tabsize, 0)\n"
		"      }\n"
		"      else {\n"
		"         return last_indent\n"
		"      }\n"
		"   } \n"
		"   else {\n"
		"      return indentLevel\n"
		"   }\n"
		"}\n"
		,
		"return matlabNewlineMacro($1)\n"
		,
		nullptr
	}
};


const char DefaultCommonMacros[] = 
#include "DefaultCommonMacros.inc"
;

/*
** Turn on smart-indent (well almost).  Unfortunately, this doesn't do
** everything.  It requires that the smart indent callback (SmartIndentCB)
** is already attached to all of the text widgets in the window, and that the
** smartIndent resource must be turned on in the widget.  These are done
** separately, because they are required per-text widget, and therefore must
** be repeated whenever a new text widget is created within this window
** (a split-window command).
*/
void BeginSmartIndent(Document *window, int warn) {
	windowSmartIndentData *winData;
	SmartIndent *indentMacros;
	const char *stoppedAt;
	const char *errMsg;
	static bool initialized = false;

	// Find the window's language mode.  If none is set, warn the user 
	QString modeName = LanguageModeName(window->languageMode_);
	if(modeName.isNull()) {
		if (warn) {
			QMessageBox::warning(nullptr /*window->shell_*/, QLatin1String("Smart Indent"), QLatin1String("No language-specific mode has been set for this file.\n\nTo use smart indent in this window, please select a\nlanguage from the Preferences -> Language Modes menu."));
		}
		return;
	}

	// Look up the appropriate smart-indent macros for the language 
	indentMacros = findIndentSpec(modeName.toLatin1().data());
	if(!indentMacros) {
		if (warn) {
			QMessageBox::warning(nullptr /*window->shell_*/, QLatin1String("Smart Indent"), QString(QLatin1String("Smart indent is not available in languagemode\n%1.\n\nYou can create new smart indent macros in the\nPreferences -> Default Settings -> Smart Indent\ndialog, or choose a different language mode from:\nPreferences -> Language Mode.")).arg(modeName));
		}
		return;
	}

	/* Make sure that the initial macro file is loaded before we execute
	   any of the smart-indent macros. Smart-indent macros may reference
	   routines defined in that file. */
	ReadMacroInitFile(window);

	/* Compile and run the common and language-specific initialization macros
	   (Note that when these return, the immediate commands in the file have not
	   necessarily been executed yet.  They are only SCHEDULED for execution) */
	if (!initialized) {
		if (!ReadMacroString(window, CommonMacros, "smart indent common initialization macros"))
			return;
		initialized = true;
	}
	if (indentMacros->initMacro) {
		if (!ReadMacroString(window, indentMacros->initMacro, "smart indent initialization macro"))
			return;
	}

	// Compile the newline and modify macros and attach them to the window 
	winData = new windowSmartIndentData;
	winData->inNewLineMacro = 0;
	winData->inModMacro = 0;
	winData->newlineMacro = ParseMacro((String)indentMacros->newlineMacro, &errMsg, &stoppedAt);
	if (!winData->newlineMacro) {
		delete winData;
		ParseError(window->shell_, indentMacros->newlineMacro, stoppedAt, "newline macro", errMsg);
		return;
	}
	if (!indentMacros->modMacro)
		winData->modMacro = nullptr;
	else {
		winData->modMacro = ParseMacro((String)indentMacros->modMacro, &errMsg, &stoppedAt);
		if (!winData->modMacro) {
			FreeProgram(winData->newlineMacro);
			delete winData;
			ParseError(window->shell_, indentMacros->modMacro, stoppedAt, "smart indent modify macro", errMsg);
			return;
		}
	}
	window->smartIndentData_ = winData;
}

void EndSmartIndent(Document *window) {
	auto winData = static_cast<windowSmartIndentData *>(window->smartIndentData_);

	if(!winData)
		return;

	// Free programs and allocated data 
	if (winData->modMacro) {
		FreeProgram(winData->modMacro);
	}
	
	FreeProgram(winData->newlineMacro);
	
	delete winData;
	window->smartIndentData_ = nullptr;
}

/*
** Returns true if there are smart indent macros for a named language
*/
int SmartIndentMacrosAvailable(char *languageModeName) {
	return findIndentSpec(languageModeName) != nullptr;
}

/*
** Attaches to the text widget's smart-indent callback to invoke a user
** defined macro when the text widget requires an indent (not just when the
** user types a newline, but also when the widget does an auto-wrap with
** auto-indent on), or the user types some other character.
*/
void SmartIndentCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	Document *window = Document::WidgetToWindow(w);
	auto cbInfo = static_cast<smartIndentCBStruct *>(callData);

	if (!window->smartIndentData_)
		return;
	if (cbInfo->reason == CHAR_TYPED)
		executeModMacro(window, cbInfo);
	else if (cbInfo->reason == NEWLINE_INDENT_NEEDED)
		executeNewlineMacro(window, cbInfo);
}

/*
** Run the newline macro with information from the smart-indent callback
** structure passed by the widget
*/
static void executeNewlineMacro(Document *window, smartIndentCBStruct *cbInfo) {
	auto winData = static_cast<windowSmartIndentData *>(window->smartIndentData_);
	// posValue probably shouldn't be static due to re-entrance issues <slobasso> 
	static DataValue posValue = {INT_TAG, {0}, {0}};
	DataValue result;
	RestartData *continuation;
	const char *errMsg;
	int stat;

	/* Beware of recursion: the newline macro may insert a string which
	   triggers the newline macro to be called again and so on. Newline
	   macros shouldn't insert strings, but nedit must not crash either if
	   they do. */
	if (winData->inNewLineMacro)
		return;

	// Call newline macro with the position at which to add newline/indent 
	posValue.val.n = cbInfo->pos;
	++(winData->inNewLineMacro);
	stat = ExecuteMacro(window, winData->newlineMacro, 1, &posValue, &result, &continuation, &errMsg);

	// Don't allow preemption or time limit.  Must get return value 
	while (stat == MACRO_TIME_LIMIT)
		stat = ContinueMacro(continuation, &result, &errMsg);

	--(winData->inNewLineMacro);
	/* Collect Garbage.  Note that the mod macro does not collect garbage,
	   (because collecting per-line is more efficient than per-character)
	   but GC now depends on the newline macro being mandatory */
	SafeGC();

	// Process errors in macro execution 
	if (stat == MACRO_PREEMPT || stat == MACRO_ERROR) {
		QMessageBox::critical(nullptr /*parent*/, QLatin1String("Smart Indent"), QString(QLatin1String("Error in smart indent macro:\n%1")).arg(QLatin1String(stat == MACRO_ERROR ? errMsg : "dialogs and shell commands not permitted")));
		EndSmartIndent(window);
		return;
	}

	// Validate and return the result 
	if (result.tag != INT_TAG || result.val.n < -1 || result.val.n > 1000) {
		QMessageBox::critical(nullptr /*parent*/, QLatin1String("Smart Indent"), QLatin1String("Smart indent macros must return\ninteger indent distance"));
		EndSmartIndent(window);
		return;
	}

	cbInfo->indentRequest = result.val.n;
}

Boolean InSmartIndentMacros(Document *window) {
	auto winData = static_cast<windowSmartIndentData *>(window->smartIndentData_);

	return ((winData && (winData->inModMacro || winData->inNewLineMacro)));
}

/*
** Run the modification macro with information from the smart-indent callback
** structure passed by the widget
*/
static void executeModMacro(Document *window, smartIndentCBStruct *cbInfo) {
	auto winData = static_cast<windowSmartIndentData *>(window->smartIndentData_);
	// args probably shouldn't be static due to future re-entrance issues <slobasso> 
	static DataValue args[2] = {{INT_TAG, {0}, {0}}, {STRING_TAG, {0}, {0}}};
	// after 5.2 release remove inModCB and use new winData->inModMacro value 
	static int inModCB = False;
	DataValue result;
	RestartData *continuation;
	const char *errMsg;
	int stat;

	/* Check for inappropriate calls and prevent re-entering if the macro
	   makes a buffer modification */
	if (winData == nullptr || winData->modMacro == nullptr || inModCB)
		return;

	/* Call modification macro with the position of the modification,
	   and the character(s) inserted.  Don't allow
	   preemption or time limit.  Execution must not overlap or re-enter */
	args[0].val.n = cbInfo->pos;
	AllocNStringCpy(&args[1].val.str, cbInfo->charsTyped);

	inModCB = True;
	++(winData->inModMacro);

	stat = ExecuteMacro(window, winData->modMacro, 2, args, &result, &continuation, &errMsg);
	while (stat == MACRO_TIME_LIMIT)
		stat = ContinueMacro(continuation, &result, &errMsg);

	--(winData->inModMacro);
	inModCB = False;

	// Process errors in macro execution 
	if (stat == MACRO_PREEMPT || stat == MACRO_ERROR) {
		QMessageBox::critical(nullptr /*parent*/, QLatin1String("Smart Indent"), QString(QLatin1String("Error in smart indent modification macro:\n%1")).arg(QLatin1String(stat == MACRO_ERROR ? errMsg : "dialogs and shell commands not permitted")));
		EndSmartIndent(window);
		return;
	}
}

void EditSmartIndentMacros(Document *window) {


	if (SmartIndentDlg) {
		SmartIndentDlg->show();
		SmartIndentDlg->raise();
		return;
	}

	if (LanguageModeName(0).isNull()) {
		QMessageBox::warning(nullptr /*parent*/, QLatin1String("Language Mode"), QLatin1String("No Language Modes defined"));
		return;
	}	
	
	
	SmartIndentDlg = new DialogSmartIndent(window, nullptr /*parent*/);
	SmartIndentDlg->show();
}

static int loadDefaultIndentSpec(const char *lmName) {

	for (int i = 0; i < N_DEFAULT_INDENT_SPECS; i++) {
		if (!strcmp(lmName, DefaultIndentSpecs[i].lmName)) {
			SmartIndentSpecs[NSmartIndentSpecs++] = copyIndentSpec(&DefaultIndentSpecs[i]);
			return True;
		}
	}
	return False;
}

int LoadSmartIndentStringEx(const QString &string) {
	return LoadSmartIndentString(string.toLatin1().data());
}

int LoadSmartIndentString(char *inString) {
	const char *errMsg;
	const char *inPtr = inString;
	SmartIndent is;
	SmartIndent *isCopy;
	int i;

	for (;;) {

		// skip over blank space 
		inPtr += strspn(inPtr, " \t\n");

		// finished 
		if (*inPtr == '\0')
			return True;

		// read language mode name 
		is.lmName = ReadSymbolicField(&inPtr);
		if (is.lmName == nullptr)
			return siParseError(inString, inPtr, "language mode name required");
		if (!SkipDelimiter(&inPtr, &errMsg)) {
			XtFree((char *)is.lmName);
			return siParseError(inString, inPtr, errMsg);
		}

		/* look for "Default" keyword, and if it's there, return the default
		   smart indent macros */
		if (!strncmp(inPtr, "Default", 7)) {
			inPtr += 7;
			if (!loadDefaultIndentSpec(is.lmName)) {
				XtFree((char *)is.lmName);
				return siParseError(inString, inPtr, "no default smart indent macros");
			}
			XtFree((char *)is.lmName);
			continue;
		}

		/* read the initialization macro (arbitrary text terminated by the
		   macro end boundary string) */
		is.initMacro = readSIMacro(&inPtr);
		if(!is.initMacro) {
			XtFree((char *)is.lmName);
			return siParseError(inString, inPtr, "no end boundary to initialization macro");
		}

		// read the newline macro 
		is.newlineMacro = readSIMacro(&inPtr);
		if(!is.newlineMacro) {
			XtFree((char *)is.lmName);
			XtFree((char *)is.initMacro);
			return siParseError(inString, inPtr, "no end boundary to newline macro");
		}

		// read the modify macro 
		is.modMacro = readSIMacro(&inPtr);
		if(!is.modMacro) {
			XtFree((char *)is.lmName);
			XtFree((char *)is.initMacro);
			XtFree((char *)is.newlineMacro);
			return siParseError(inString, inPtr, "no end boundary to modify macro");
		}

		// if there's no mod macro, make it null so it won't be executed 
		if (is.modMacro[0] == '\0') {
			XtFree((char *)is.modMacro);
			is.modMacro = nullptr;
		}

		// create a new data structure and add/change it in the list 
		isCopy = new SmartIndent(is);

		for (i = 0; i < NSmartIndentSpecs; i++) {
			if (!strcmp(SmartIndentSpecs[i]->lmName, is.lmName)) {
				freeIndentSpec(SmartIndentSpecs[i]);
				SmartIndentSpecs[i] = isCopy;
				break;
			}
		}
		if (i == NSmartIndentSpecs)
			SmartIndentSpecs[NSmartIndentSpecs++] = isCopy;
	}
}

int LoadSmartIndentCommonStringEx(view::string_view string) {

	auto buffer = new char[string.size() + 1];
	std::copy(string.begin(), string.end(), buffer);
	buffer[string.size()] = '\0';
	int r = LoadSmartIndentCommonString(buffer);
	delete [] buffer;
	return r;
}

int LoadSmartIndentCommonString(char *inString) {
	int shiftedLen;
	char *inPtr = inString;

	// If called from -import, can replace existing ones 
	XtFree(CommonMacros);

	// skip over blank space 
	inPtr += strspn(inPtr, " \t\n");

	/* look for "Default" keyword, and if it's there, return the default
	   smart common macro */
	if (!strncmp(inPtr, "Default", 7)) {
		CommonMacros = XtNewStringEx(DefaultCommonMacros);
		return True;
	}

	// Remove leading tabs added by writer routine 
	CommonMacros = ShiftText(inPtr, SHIFT_LEFT, True, 8, 8, &shiftedLen);
	return True;
}

/*
** Read a macro (arbitrary text terminated by the macro end boundary string)
** from the position pointed to by *inPtr, trim off added tabs and return an
** allocated copy of the string, and advance *inPtr to the end of the macro.
** Returns nullptr if the macro end boundary string is not found.
*/
static char *readSIMacro(const char **inPtr) {
	
	// Strip leading newline 
	if (**inPtr == '\n') {
		(*inPtr)++;
	}

	// Find the end of the macro 
	const char *macroEnd = strstr(*inPtr, MacroEndBoundary);
	if(!macroEnd) {
		return nullptr;
	}

	// Copy the macro 
	auto macroStr = new char[macroEnd - *inPtr + 1];
	strncpy(macroStr, *inPtr, macroEnd - *inPtr);
	macroStr[macroEnd - *inPtr] = '\0';

	// Remove leading tabs added by writer routine 
	*inPtr = macroEnd + strlen(MacroEndBoundary);
	int shiftedLen;
	char *retStr = ShiftText(macroStr, SHIFT_LEFT, True, 8, 8, &shiftedLen);
	delete [] macroStr;
	return retStr;
}

static int indentSpecsDiffer(SmartIndent *is1, SmartIndent *is2) {
	return AllocatedStringsDiffer(is1->initMacro, is2->initMacro) || AllocatedStringsDiffer(is1->newlineMacro, is2->newlineMacro) || AllocatedStringsDiffer(is1->modMacro, is2->modMacro);
}

static int siParseError(const char *stringStart, const char *stoppedAt, const char *message) {
	return ParseError(nullptr, stringStart, stoppedAt, "smart indent specification", message);
}

QString WriteSmartIndentStringEx(void) {

	auto outBuf = new TextBuffer;
	for (int i = 0; i < NSmartIndentSpecs; i++) {
		SmartIndent *sis = SmartIndentSpecs[i];
		
		outBuf->BufAppendEx("\t");
		outBuf->BufAppendEx(sis->lmName);
		outBuf->BufAppendEx(":");
		if (isDefaultIndentSpec(sis)) {
			outBuf->BufAppendEx("Default\n");
		} else {
			insertShiftedMacro(outBuf, (String)sis->initMacro);
			insertShiftedMacro(outBuf, (String)sis->newlineMacro);
			insertShiftedMacro(outBuf, (String)sis->modMacro);
		}
	}

	// Get the output string, and lop off the trailing newline 
	std::string outStr = outBuf->BufGetRangeEx(0, outBuf->BufGetLength() > 0 ? outBuf->BufGetLength() - 1 : 0);
	delete outBuf;

	/* Protect newlines and backslashes from translation by the resource
	   reader */
	return QString::fromStdString(EscapeSensitiveCharsEx(outStr));
}

QString WriteSmartIndentCommonStringEx(void) {

	if (!strcmp(CommonMacros, DefaultCommonMacros))
		return QLatin1String("Default");

	if(!CommonMacros)
		return QLatin1String("");

	// Shift the macro over by a tab to keep .nedit file bright and clean 
	std::string outStr = ShiftTextEx(CommonMacros, SHIFT_RIGHT, True, 8, 8);

	/* Protect newlines and backslashes from translation by the resource
	   reader */
	QString escapedStr = QString::fromStdString(EscapeSensitiveCharsEx(outStr));

	// If there's a trailing escaped newline, remove it 
	int len = escapedStr.size();
	if (len > 1 && escapedStr[len - 1] == QLatin1Char('\n') && escapedStr[len - 2] == QLatin1Char('\\')) {
		escapedStr.resize(len - 2);
	}
	
	return escapedStr;
}

/*
** Insert macro text "macro" into buffer "buf" shifted right by 8 characters
** (so it looks nice in the .nedit file), and terminated with a macro-end-
** boundary string.
*/
static void insertShiftedMacro(TextBuffer *buf, char *macro) {

	if (macro) {
		std::string shiftedMacro = ShiftTextEx(macro, SHIFT_RIGHT, True, 8, 8);
		buf->BufAppendEx(shiftedMacro);
	}
	buf->BufAppendEx("\t");
	buf->BufAppendEx(MacroEndBoundary);
	buf->BufAppendEx("\n");
}

static int isDefaultIndentSpec(SmartIndent *indentSpec) {

	for (int i = 0; i < N_DEFAULT_INDENT_SPECS; i++)
		if (!strcmp(indentSpec->lmName, DefaultIndentSpecs[i].lmName))
			return !indentSpecsDiffer(indentSpec, &DefaultIndentSpecs[i]);
	return False;
}

SmartIndent *findIndentSpec(const char *modeName) {
	int i;

	if(!modeName)
		return nullptr;

	for (i = 0; i < NSmartIndentSpecs; i++)
		if (!strcmp(modeName, SmartIndentSpecs[i]->lmName))
			return SmartIndentSpecs[i];
	return nullptr;
}

/*
** Returns True if there are smart indent macros, or potential macros
** not yet committed in the smart indent dialog for a language mode,
*/
int LMHasSmartIndentMacros(const char *languageMode) {
	if (findIndentSpec(languageMode) != nullptr) {
		return true;
	}
	
	
	return SmartIndentDlg != nullptr && SmartIndentDlg->hasSmartIndentMacros(QLatin1String(languageMode));
}

/*
** Change the language mode name of smart indent macro sets for language
** "oldName" to "newName" in both the stored macro sets, and the pattern set
** currently being edited in the dialog.
*/
void RenameSmartIndentMacros(const char *oldName, const char *newName) {

	for (int i = 0; i < NSmartIndentSpecs; i++) {
		if (!strcmp(oldName, SmartIndentSpecs[i]->lmName)) {
			XtFree((char *)SmartIndentSpecs[i]->lmName);
			SmartIndentSpecs[i]->lmName = XtNewStringEx(newName);
		}
	}
	if (SmartIndentDlg) {
		if(SmartIndentDlg->languageMode_ == QLatin1String(oldName)) {
			SmartIndentDlg->setLanguageMode(QLatin1String(newName));
		}
	}
}

/*
** If a smart indent dialog is up, ask to have the option menu for
** chosing language mode updated (via a call to CreateLanguageModeMenu)
*/
void UpdateLangModeMenuSmartIndent(void) {

	if(SmartIndentDlg) {
		SmartIndentDlg->updateLanguageModes();
	}
}
