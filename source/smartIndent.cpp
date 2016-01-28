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
#include "TextBuffer.h"
#include "nedit.h"
#include "text.h"
#include "preferences.h"
#include "WindowInfo.h"
#include "interpret.h"
#include "macro.h"
#include "window.h"
#include "parse.h"
#include "shift.h"
#include "help.h"
#include "MotifHelper.h"
#include "../util/DialogF.h"
#include "../util/misc.h"

#include <cstdio>
#include <cstring>
#include <climits>

#include <Xm/Xm.h>
#include <sys/param.h>

#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Text.h>
#include <Xm/LabelG.h>
#include <Xm/PushB.h>
#include <Xm/RowColumn.h>
#include <Xm/SeparatoG.h>
#include <Xm/PanedW.h>

static char MacroEndBoundary[] = "--End-of-Macro--";

struct smartIndentRec {
	const char *lmName;
	const char *initMacro;
	const char *newlineMacro;
	const char *modMacro;
};

struct windowSmartIndentData {
	Program *newlineMacro;
	int inNewLineMacro;
	Program *modMacro;
	int inModMacro;
};

/* Smart indent macros dialog information */
static struct {
	Widget shell;
	Widget lmOptMenu;
	Widget lmPulldown;
	Widget initMacro;
	Widget newlineMacro;
	Widget modMacro;
	char *langModeName;
} SmartIndentDialog = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

/* Common smart indent macros dialog information */
static struct {
	Widget shell;
	Widget text;
} CommonDialog = {nullptr, nullptr};

static int NSmartIndentSpecs = 0;
static smartIndentRec *SmartIndentSpecs[MAX_LANGUAGE_MODES];
static char *CommonMacros = nullptr;

static void executeNewlineMacro(WindowInfo *window, smartIndentCBStruct *cbInfo);
static void executeModMacro(WindowInfo *window, smartIndentCBStruct *cbInfo);
static void insertShiftedMacro(TextBuffer *buf, char *macro);
static int isDefaultIndentSpec(smartIndentRec *indentSpec);
static smartIndentRec *findIndentSpec(const char *modeName);
static char *ensureNewline(char *string);
static int loadDefaultIndentSpec(const char *lmName);
static int siParseError(const char *stringStart, const char *stoppedAt, const char *message);
static void destroyCB(Widget w, XtPointer clientData, XtPointer callData);
static void langModeCB(Widget w, XtPointer clientData, XtPointer callData);
static void commonDialogCB(Widget w, XtPointer clientData, XtPointer callData);
static void lmDialogCB(Widget w, XtPointer clientData, XtPointer callData);
static void okCB(Widget w, XtPointer clientData, XtPointer callData);
static void applyCB(Widget w, XtPointer clientData, XtPointer callData);
static void checkCB(Widget w, XtPointer clientData, XtPointer callData);
static void restoreCB(Widget w, XtPointer clientData, XtPointer callData);
static void deleteCB(Widget w, XtPointer clientData, XtPointer callData);
static void closeCB(Widget w, XtPointer clientData, XtPointer callData);
static void helpCB(Widget w, XtPointer clientData, XtPointer callData);
static int checkSmartIndentDialogData(void);
static smartIndentRec *getSmartIndentDialogData(void);
static void setSmartIndentDialogData(smartIndentRec *is);
static void comDestroyCB(Widget w, XtPointer clientData, XtPointer callData);
static void comOKCB(Widget w, XtPointer clientData, XtPointer callData);
static void comApplyCB(Widget w, XtPointer clientData, XtPointer callData);
static void comCheckCB(Widget w, XtPointer clientData, XtPointer callData);
static void comRestoreCB(Widget w, XtPointer clientData, XtPointer callData);
static void comCloseCB(Widget w, XtPointer clientData, XtPointer callData);
static int updateSmartIndentCommonData(void);
static int checkSmartIndentCommonDialogData(void);
static int updateSmartIndentData(void);
static char *readSIMacro(const char **inPtr);
static smartIndentRec *copyIndentSpec(smartIndentRec *is);
static void freeIndentSpec(smartIndentRec *is);
static int indentSpecsDiffer(smartIndentRec *is1, smartIndentRec *is2);
static int LoadSmartIndentCommonString(char *inString);
static int LoadSmartIndentString(char *inString);

#define N_DEFAULT_INDENT_SPECS 4
static smartIndentRec DefaultIndentSpecs[N_DEFAULT_INDENT_SPECS] = {{"C", "# C Macros and tuning parameters are shared with C++, and are declared\n\
# in the common section.  Press Common / Shared Initialization above.\n",
                                                                     "return cFindSmartIndentDist($1)\n", "if ($2 == \"}\" || $2 == \"{\" || $2 == \"#\")\n\
    cBraceOrPound($1, $2)\n"},
                                                                    {"C++", "# C++ Macros and tuning parameters are shared with C, and are declared\n\
# in the common section.  Press Common / Shared Initialization above.\n",
                                                                     "return cFindSmartIndentDist($1)\n", "if ($2 == \"}\" || $2 == \"{\" || $2 == \"#\")\n\
    cBraceOrPound($1, $2)\n"},
                                                                    {"Python", "# Number of characters in a normal indent level.  May be a number, or the\n\
# string \"default\", meaning, guess the value from the current tab settings.\n\
$pyIndentDist = \"default\"\n",
                                                                     "if (get_range($1-1, $1) != \":\")\n\
    return -1\n\
return measureIndent($1) + defaultIndent($pyIndentDist)\n",
                                                                     nullptr},
                                                                    {"Matlab", "# Number of spaces to indent \"case\" statements\n\
$caseDepth = 2\n\
define matlabNewlineMacro\n\
{\n\
   if (!$em_tab_dist)\n\
        tabsize = $tab_dist\n\
   else\n\
        tabsize = $em_tab_dist\n\
   startLine = startOfLine($1)\n\
   indentLevel = measureIndent($1)\n\
\n\
   # If this line is continued on next, return default:\n\
   lastLine = get_range(startLine, $1)\n\
   if (search_string(lastLine, \"...\", 0) != -1) {\n\
      if ($n_args == 2)\n\
         return matlabNewlineMacro(startLine - 1, 1)\n\
      else {\n\
         return -1\n\
      }\n\
   }\n\
\n\
   # Correct the indentLevel if this was a continued line.\n\
   while (startLine > 1)\n\
   {\n\
      endLine = startLine - 1\n\
      startLine = startOfLine(endLine)\n\
      lastLine = get_range(startLine, endLine)\n\
      # No \"...\" means we've found the root\n\
      if (search_string(lastLine, \"...\", 0) == -1) {\n\
         startLine = endLine + 1\n\
         break\n\
      }\n\
   }\n\
   indentLevel = measureIndent(startLine)\n\
\n\
   # Get the first word of the indentLevel line\n\
   FWend = search(\">|\\n\", startLine + indentLevel, \"regex\")\n\
   # This search fails on EOF\n\
   if (FWend == -1)\n\
      FWend = $1\n\
\n\
   firstWord = get_range(startLine + indentLevel, FWend)\n\
\n\
   # How shall we change the indent level based on the first word?\n\
   if (search_string(firstWord, \\\n\
         \"<for>|<function>|<if>|<switch>|<try>|<while>\", 0, \"regex\") == 0) {\n\
      return indentLevel + tabsize\n\
   }\n\
   else if ((firstWord == \"end\") || (search_string(firstWord, \\\n\
            \"<case>|<catch>|<else>|<elseif>|<otherwise>\", 0, \"regex\") == 0)) {\n\
      # Get the last indent level \n\
      if (startLine > 0) # avoid infinite loop\n\
	 last_indent = matlabNewlineMacro(startLine - 1, 1)\n\
      else\n\
         last_indent = indentLevel\n\
\n\
      # Re-indent this line\n\
      if ($n_args == 1) {\n\
         if (firstWord == \"case\" || firstWord == \"otherwise\")\n\
            replace_range(startLine, startLine + indentLevel, \\\n\
                          makeIndentString(last_indent - tabsize + $caseDepth))\n\
         else\n\
            replace_range(startLine, startLine + indentLevel, \\\n\
                          makeIndentString(last_indent - tabsize))\n\
      }\n\
\n\
      if (firstWord == \"end\") {\n\
         return max(last_indent - tabsize, 0)\n\
      }\n\
      else {\n\
         return last_indent\n\
      }\n\
   } \n\
   else {\n\
      return indentLevel\n\
   }\n\
}\n\
",
                                                                     "return matlabNewlineMacro($1)\n", nullptr}};

static const char DefaultCommonMacros[] = 
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
void BeginSmartIndent(WindowInfo *window, int warn) {
	windowSmartIndentData *winData;
	smartIndentRec *indentMacros;
	char *modeName;
	const char *stoppedAt;
	const char *errMsg;
	static int initialized;

	/* Find the window's language mode.  If none is set, warn the user */
	modeName = LanguageModeName(window->languageMode);
	if(!modeName) {
		if (warn) {
			DialogF(DF_WARN, window->shell, 1, "Smart Indent", "No language-specific mode has been set for this file.\n\n"
			                                                   "To use smart indent in this window, please select a\n"
			                                                   "language from the Preferences -> Language Modes menu.",
			        "OK");
		}
		return;
	}

	/* Look up the appropriate smart-indent macros for the language */
	indentMacros = findIndentSpec(modeName);
	if(!indentMacros) {
		if (warn) {
			DialogF(DF_WARN, window->shell, 1, "Smart Indent", "Smart indent is not available in languagemode\n%s.\n\n"
			                                                   "You can create new smart indent macros in the\n"
			                                                   "Preferences -> Default Settings -> Smart Indent\n"
			                                                   "dialog, or choose a different language mode from:\n"
			                                                   "Preferences -> Language Mode.",
			        "OK", modeName);
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
		initialized = True;
	}
	if (indentMacros->initMacro) {
		if (!ReadMacroString(window, indentMacros->initMacro, "smart indent initialization macro"))
			return;
	}

	/* Compile the newline and modify macros and attach them to the window */
	winData = new windowSmartIndentData;
	winData->inNewLineMacro = 0;
	winData->inModMacro = 0;
	winData->newlineMacro = ParseMacro((String)indentMacros->newlineMacro, &errMsg, &stoppedAt);
	if (!winData->newlineMacro) {
		delete winData;
		ParseError(window->shell, indentMacros->newlineMacro, stoppedAt, "newline macro", errMsg);
		return;
	}
	if (!indentMacros->modMacro)
		winData->modMacro = nullptr;
	else {
		winData->modMacro = ParseMacro((String)indentMacros->modMacro, &errMsg, &stoppedAt);
		if (!winData->modMacro) {
			FreeProgram(winData->newlineMacro);
			delete winData;
			ParseError(window->shell, indentMacros->modMacro, stoppedAt, "smart indent modify macro", errMsg);
			return;
		}
	}
	window->smartIndentData = winData;
}

void EndSmartIndent(WindowInfo *window) {
	auto winData = static_cast<windowSmartIndentData *>(window->smartIndentData);

	if(!winData)
		return;

	/* Free programs and allocated data */
	if (winData->modMacro)
		FreeProgram(winData->modMacro);
	FreeProgram(winData->newlineMacro);
	
	delete winData;
	window->smartIndentData = nullptr;
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

	WindowInfo *window = WidgetToWindow(w);
	smartIndentCBStruct *cbInfo = (smartIndentCBStruct *)callData;

	if (!window->smartIndentData)
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
static void executeNewlineMacro(WindowInfo *window, smartIndentCBStruct *cbInfo) {
	auto winData = static_cast<windowSmartIndentData *>(window->smartIndentData);
	/* posValue probably shouldn't be static due to re-entrance issues <slobasso> */
	static DataValue posValue = {INT_TAG, {0}};
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

	/* Call newline macro with the position at which to add newline/indent */
	posValue.val.n = cbInfo->pos;
	++(winData->inNewLineMacro);
	stat = ExecuteMacro(window, winData->newlineMacro, 1, &posValue, &result, &continuation, &errMsg);

	/* Don't allow preemption or time limit.  Must get return value */
	while (stat == MACRO_TIME_LIMIT)
		stat = ContinueMacro(continuation, &result, &errMsg);

	--(winData->inNewLineMacro);
	/* Collect Garbage.  Note that the mod macro does not collect garbage,
	   (because collecting per-line is more efficient than per-character)
	   but GC now depends on the newline macro being mandatory */
	SafeGC();

	/* Process errors in macro execution */
	if (stat == MACRO_PREEMPT || stat == MACRO_ERROR) {
		DialogF(DF_ERR, window->shell, 1, "Smart Indent", "Error in smart indent macro:\n%s", "OK", stat == MACRO_ERROR ? errMsg : "dialogs and shell commands not permitted");
		EndSmartIndent(window);
		return;
	}

	/* Validate and return the result */
	if (result.tag != INT_TAG || result.val.n < -1 || result.val.n > 1000) {
		DialogF(DF_ERR, window->shell, 1, "Smart Indent", "Smart indent macros must return\ninteger indent distance", "OK");
		EndSmartIndent(window);
		return;
	}

	cbInfo->indentRequest = result.val.n;
}

Boolean InSmartIndentMacros(WindowInfo *window) {
	auto winData = static_cast<windowSmartIndentData *>(window->smartIndentData);

	return ((winData && (winData->inModMacro || winData->inNewLineMacro)));
}

/*
** Run the modification macro with information from the smart-indent callback
** structure passed by the widget
*/
static void executeModMacro(WindowInfo *window, smartIndentCBStruct *cbInfo) {
	auto winData = static_cast<windowSmartIndentData *>(window->smartIndentData);
	/* args probably shouldn't be static due to future re-entrance issues <slobasso> */
	static DataValue args[2] = {{INT_TAG, {0}}, {STRING_TAG, {0}}};
	/* after 5.2 release remove inModCB and use new winData->inModMacro value */
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

	/* Process errors in macro execution */
	if (stat == MACRO_PREEMPT || stat == MACRO_ERROR) {
		DialogF(DF_ERR, window->shell, 1, "Smart Indent", "Error in smart indent modification macro:\n%s", "OK", stat == MACRO_ERROR ? errMsg : "dialogs and shell commands not permitted");
		EndSmartIndent(window);
		return;
	}
}

void EditSmartIndentMacros(WindowInfo *window) {
#define BORDER 4
	Widget form, lmOptMenu, lmForm, lmBtn;
	Widget okBtn, applyBtn, checkBtn, deleteBtn, commonBtn;
	Widget closeBtn, helpBtn, restoreBtn, pane;
	Widget initForm, newlineForm, modifyForm;
	Widget initLbl, newlineLbl, modifyLbl;
	XmString s1;
	char *lmName;
	Arg args[20];
	int n;

	/* if the dialog is already displayed, just pop it to the top and return */
	if (SmartIndentDialog.shell) {
		RaiseDialogWindow(SmartIndentDialog.shell);
		return;
	}

	if (LanguageModeName(0) == nullptr) {
		DialogF(DF_WARN, window->shell, 1, "Language Mode", "No Language Modes defined", "OK");
		return;
	}

	/* Decide on an initial language mode */
	lmName = LanguageModeName(window->languageMode == PLAIN_LANGUAGE_MODE ? 0 : window->languageMode);
	SmartIndentDialog.langModeName = XtNewStringEx(lmName);

	/* Create a form widget in an application shell */
	n = 0;
	XtSetArg(args[n], XmNdeleteResponse, XmDO_NOTHING);
	n++;
	XtSetArg(args[n], XmNiconName, "NEdit Smart Indent Macros");
	n++;
	XtSetArg(args[n], XmNtitle, "Program Smart Indent Macros");
	n++;
	SmartIndentDialog.shell = CreateWidget(TheAppShell, "smartIndent", topLevelShellWidgetClass, args, n);
	AddSmallIcon(SmartIndentDialog.shell);
	form = XtVaCreateManagedWidget("editSmartIndentMacros", xmFormWidgetClass, SmartIndentDialog.shell, XmNautoUnmanage, False, XmNresizePolicy, XmRESIZE_NONE, nullptr);
	XtAddCallback(form, XmNdestroyCallback, destroyCB, nullptr);
	AddMotifCloseCallback(SmartIndentDialog.shell, closeCB, nullptr);

	lmForm = XtVaCreateManagedWidget("lmForm", xmFormWidgetClass, form, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 1, XmNtopAttachment, XmATTACH_POSITION, XmNtopPosition, 1, XmNrightAttachment, XmATTACH_POSITION,
	                                 XmNrightPosition, 99, nullptr);

	SmartIndentDialog.lmPulldown = CreateLanguageModeMenu(lmForm, langModeCB, nullptr);
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
	XtSetArg(args[n], XmNsubMenuId, SmartIndentDialog.lmPulldown);
	n++;
	lmOptMenu = XmCreateOptionMenu(lmForm, (String) "langModeOptMenu", args, n);
	XtManageChild(lmOptMenu);
	SmartIndentDialog.lmOptMenu = lmOptMenu;

	XtVaCreateManagedWidget("lmLbl", xmLabelGadgetClass, lmForm, XmNlabelString, s1 = XmStringCreateSimpleEx("Language Mode:"), XmNmnemonic, 'L', XmNuserData, XtParent(SmartIndentDialog.lmOptMenu), XmNalignment, XmALIGNMENT_END,
	                        XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, 50, XmNtopAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET, XmNbottomWidget, lmOptMenu, nullptr);
	XmStringFree(s1);

	lmBtn = XtVaCreateManagedWidget("lmBtn", xmPushButtonWidgetClass, lmForm, XmNlabelString, s1 = XmStringCreateLtoREx("Add / Modify\nLanguage Mode..."), XmNmnemonic, 'A', XmNrightAttachment, XmATTACH_FORM, XmNtopAttachment, XmATTACH_FORM,
	                                nullptr);
	XtAddCallback(lmBtn, XmNactivateCallback, lmDialogCB, nullptr);
	XmStringFree(s1);

	commonBtn = XtVaCreateManagedWidget("commonBtn", xmPushButtonWidgetClass, lmForm, XmNlabelString, s1 = XmStringCreateLtoREx("Common / Shared\nInitialization..."), XmNmnemonic, 'C', XmNleftAttachment, XmATTACH_FORM, XmNtopAttachment,
	                                    XmATTACH_FORM, nullptr);
	XtAddCallback(commonBtn, XmNactivateCallback, commonDialogCB, nullptr);
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

	pane = XtVaCreateManagedWidget("pane", xmPanedWindowWidgetClass, form, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 1, XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, 99, XmNtopAttachment, XmATTACH_WIDGET,
	                               XmNtopWidget, lmForm, XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, okBtn, nullptr);
	/* XmNmarginWidth, 0, XmNmarginHeight, 0, XmNseparatorOn, False,
	XmNspacing, 3, XmNsashIndent, -2, */

	initForm = XtVaCreateManagedWidget("initForm", xmFormWidgetClass, pane, nullptr);
	initLbl = XtVaCreateManagedWidget("initLbl", xmLabelGadgetClass, initForm, XmNlabelString, s1 = XmStringCreateSimpleEx("Language Specific Initialization Macro Commands and Definitions"), XmNmnemonic, 'I', nullptr);
	XmStringFree(s1);
	n = 0;
	XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT);
	n++;
	XtSetArg(args[n], XmNrows, 5);
	n++;
	XtSetArg(args[n], XmNcolumns, 80);
	n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET);
	n++;
	XtSetArg(args[n], XmNtopWidget, initLbl);
	n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM);
	n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM);
	n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM);
	n++;
	SmartIndentDialog.initMacro = XmCreateScrolledText(initForm, (String) "initMacro", args, n);
	AddMouseWheelSupport(SmartIndentDialog.initMacro);
	XtManageChild(SmartIndentDialog.initMacro);
	RemapDeleteKey(SmartIndentDialog.initMacro);
	XtVaSetValues(initLbl, XmNuserData, SmartIndentDialog.initMacro, nullptr);

	newlineForm = XtVaCreateManagedWidget("newlineForm", xmFormWidgetClass, pane, nullptr);
	newlineLbl = XtVaCreateManagedWidget("newlineLbl", xmLabelGadgetClass, newlineForm, XmNlabelString, s1 = XmStringCreateSimpleEx("Newline Macro"), XmNmnemonic, 'N', nullptr);
	XmStringFree(s1);
	XtVaCreateManagedWidget("newlineArgsLbl", xmLabelGadgetClass, newlineForm, XmNalignment, XmALIGNMENT_END, XmNlabelString, s1 = XmStringCreateSimpleEx("($1 is insert position, return indent request or -1)"), XmNrightAttachment,
	                        XmATTACH_FORM, nullptr);
	XmStringFree(s1);
	n = 0;
	XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT);
	n++;
	XtSetArg(args[n], XmNrows, 5);
	n++;
	XtSetArg(args[n], XmNcolumns, 80);
	n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET);
	n++;
	XtSetArg(args[n], XmNtopWidget, newlineLbl);
	n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM);
	n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM);
	n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM);
	n++;
	SmartIndentDialog.newlineMacro = XmCreateScrolledText(newlineForm, (String) "newlineMacro", args, n);
	AddMouseWheelSupport(SmartIndentDialog.newlineMacro);
	XtManageChild(SmartIndentDialog.newlineMacro);
	RemapDeleteKey(SmartIndentDialog.newlineMacro);
	XtVaSetValues(newlineLbl, XmNuserData, SmartIndentDialog.newlineMacro, nullptr);

	modifyForm = XtVaCreateManagedWidget("modifyForm", xmFormWidgetClass, pane, nullptr);
	modifyLbl = XtVaCreateManagedWidget("modifyLbl", xmLabelGadgetClass, modifyForm, XmNlabelString, s1 = XmStringCreateSimpleEx("Type-in Macro"), XmNmnemonic, 'M', nullptr);
	XmStringFree(s1);
	XtVaCreateManagedWidget("modifyArgsLbl", xmLabelGadgetClass, modifyForm, XmNalignment, XmALIGNMENT_END, XmNlabelString, s1 = XmStringCreateSimpleEx("($1 is position, $2 is character to be inserted)"), XmNrightAttachment, XmATTACH_FORM,
	                        nullptr);
	XmStringFree(s1);
	n = 0;
	XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT);
	n++;
	XtSetArg(args[n], XmNrows, 5);
	n++;
	XtSetArg(args[n], XmNcolumns, 80);
	n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET);
	n++;
	XtSetArg(args[n], XmNtopWidget, modifyLbl);
	n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM);
	n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM);
	n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM);
	n++;
	SmartIndentDialog.modMacro = XmCreateScrolledText(modifyForm, (String) "modifyMacro", args, n);
	AddMouseWheelSupport(SmartIndentDialog.modMacro);
	XtManageChild(SmartIndentDialog.modMacro);
	RemapDeleteKey(SmartIndentDialog.modMacro);
	XtVaSetValues(modifyLbl, XmNuserData, SmartIndentDialog.modMacro, nullptr);

	/* Set initial default button */
	XtVaSetValues(form, XmNdefaultButton, okBtn, nullptr);
	XtVaSetValues(form, XmNcancelButton, closeBtn, nullptr);

	/* Handle mnemonic selection of buttons and focus to dialog */
	AddDialogMnemonicHandler(form, FALSE);

	/* Fill in the dialog information for the selected language mode */
	setSmartIndentDialogData(findIndentSpec(lmName));
	SetLangModeMenu(SmartIndentDialog.lmOptMenu, SmartIndentDialog.langModeName);

	/* Realize all of the widgets in the new dialog */
	RealizeWithoutForcingPosition(SmartIndentDialog.shell);
}

static void destroyCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	XtFree(SmartIndentDialog.langModeName);
	SmartIndentDialog.shell = nullptr;
}

static void langModeCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	char *modeName;
	int i, resp;
	static smartIndentRec emptyIndentSpec = {nullptr, nullptr, nullptr, nullptr};
	smartIndentRec *oldMacros, *newMacros;

	/* Get the newly selected mode name.  If it's the same, do nothing */
	XtVaGetValues(w, XmNuserData, &modeName, nullptr);
	if (!strcmp(modeName, SmartIndentDialog.langModeName))
		return;

	/* Find the original macros */
	for (i = 0; i < NSmartIndentSpecs; i++)
		if (!strcmp(SmartIndentDialog.langModeName, SmartIndentSpecs[i]->lmName))
			break;
	oldMacros = i == NSmartIndentSpecs ? &emptyIndentSpec : SmartIndentSpecs[i];

	/* Check if the macros have changed, if so allow user to apply, discard,
	   or cancel */
	newMacros = getSmartIndentDialogData();
	if (indentSpecsDiffer(oldMacros, newMacros)) {
		resp = DialogF(DF_QUES, SmartIndentDialog.shell, 3, "Smart Indent", "Smart indent macros for language mode\n"
		                                                                    "%s were changed.  Apply changes?",
		               "Apply", "Discard", "Cancel", SmartIndentDialog.langModeName);

		if (resp == 3) {
			SetLangModeMenu(SmartIndentDialog.lmOptMenu, SmartIndentDialog.langModeName);
			return;
		} else if (resp == 1) {
			if (checkSmartIndentDialogData()) {
				if (oldMacros == &emptyIndentSpec) {
					SmartIndentSpecs[NSmartIndentSpecs++] = copyIndentSpec(newMacros);
				} else {
					freeIndentSpec(oldMacros);
					SmartIndentSpecs[i] = copyIndentSpec(newMacros);
				}
			} else {
				SetLangModeMenu(SmartIndentDialog.lmOptMenu, SmartIndentDialog.langModeName);
				return;
			}
		}
	}
	freeIndentSpec(newMacros);

	/* Fill the dialog with the new language mode information */
	SmartIndentDialog.langModeName = XtNewStringEx(modeName);
	setSmartIndentDialogData(findIndentSpec(modeName));
}

static void lmDialogCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;
	EditLanguageModes();
}

static void commonDialogCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	EditCommonSmartIndentMacro();
}

static void okCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	/* change the macro */
	if (!updateSmartIndentData())
		return;

	/* pop down and destroy the dialog */
	CloseAllPopupsFor(SmartIndentDialog.shell);
	XtDestroyWidget(SmartIndentDialog.shell);
}

static void applyCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	/* change the patterns */
	updateSmartIndentData();
}

static void checkCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	if (checkSmartIndentDialogData())
		DialogF(DF_INF, SmartIndentDialog.shell, 1, "Macro compiled", "Macros compiled without error", "OK");
}

static void restoreCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	int i;
	smartIndentRec *defaultIS;

	/* Find the default indent spec */
	for (i = 0; i < N_DEFAULT_INDENT_SPECS; i++) {
		if (!strcmp(SmartIndentDialog.langModeName, DefaultIndentSpecs[i].lmName)) {
			break;
		}
	}

	if (i == N_DEFAULT_INDENT_SPECS) {
		DialogF(DF_WARN, SmartIndentDialog.shell, 1, "Smart Indent", "There are no default indent macros\nfor language mode %s", "OK", SmartIndentDialog.langModeName);
		return;
	}
	defaultIS = &DefaultIndentSpecs[i];

	if (DialogF(DF_WARN, SmartIndentDialog.shell, 2, "Discard Changes", "Are you sure you want to discard\n"
	                                                                    "all changes to smart indent macros\n"
	                                                                    "for language mode %s?",
	            "Discard", "Cancel", SmartIndentDialog.langModeName) == 2) {
		return;
	}

	/* if a stored version of the indent macros exist, replace them, if not,
	   add a new one */
	for (i = 0; i < NSmartIndentSpecs; i++)
		if (!strcmp(SmartIndentDialog.langModeName, SmartIndentSpecs[i]->lmName))
			break;
	if (i < NSmartIndentSpecs) {
		freeIndentSpec(SmartIndentSpecs[i]);
		SmartIndentSpecs[i] = copyIndentSpec(defaultIS);
	} else
		SmartIndentSpecs[NSmartIndentSpecs++] = copyIndentSpec(defaultIS);

	/* Update the dialog */
	setSmartIndentDialogData(defaultIS);
}

static void deleteCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	int i;

	if (DialogF(DF_WARN, SmartIndentDialog.shell, 2, "Delete Macros", "Are you sure you want to delete smart indent\n"
	                                                                  "macros for language mode %s?",
	            "Yes, Delete", "Cancel", SmartIndentDialog.langModeName) == 2) {
		return;
	}

	/* if a stored version of the pattern set exists, delete it from the list */
	for (i = 0; i < NSmartIndentSpecs; i++)
		if (!strcmp(SmartIndentDialog.langModeName, SmartIndentSpecs[i]->lmName))
			break;
	if (i < NSmartIndentSpecs) {
		freeIndentSpec(SmartIndentSpecs[i]);
		memmove(&SmartIndentSpecs[i], &SmartIndentSpecs[i + 1], (NSmartIndentSpecs - 1 - i) * sizeof(smartIndentRec *));
		NSmartIndentSpecs--;
	}

	/* Clear out the dialog */
	setSmartIndentDialogData(nullptr);
}

static void closeCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	/* pop down and destroy the dialog */
	CloseAllPopupsFor(SmartIndentDialog.shell);
	XtDestroyWidget(SmartIndentDialog.shell);
}

static void helpCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	Help(HELP_SMART_INDENT);
}

static int checkSmartIndentDialogData(void) {
	char *widgetText;
	const char *errMsg;
	const char *stoppedAt;
	Program *prog;

	/* Check the initialization macro */
	if (!TextWidgetIsBlank(SmartIndentDialog.initMacro)) {
		widgetText = ensureNewline(XmTextGetString(SmartIndentDialog.initMacro));
		if (!CheckMacroString(SmartIndentDialog.shell, widgetText, "initialization macro", &stoppedAt)) {
			XmTextSetInsertionPosition(SmartIndentDialog.initMacro, stoppedAt - widgetText);
			XmProcessTraversal(SmartIndentDialog.initMacro, XmTRAVERSE_CURRENT);
			XtFree(widgetText);
			return False;
		}
		XtFree(widgetText);
	}

	/* Test compile the newline macro */
	if (TextWidgetIsBlank(SmartIndentDialog.newlineMacro)) {
		DialogF(DF_WARN, SmartIndentDialog.shell, 1, "Smart Indent", "Newline macro required", "OK");
		return False;
	}

	widgetText = ensureNewline(XmTextGetString(SmartIndentDialog.newlineMacro));
	prog = ParseMacro(widgetText, &errMsg, &stoppedAt);
	if(!prog) {
		ParseError(SmartIndentDialog.shell, widgetText, stoppedAt, "newline macro", errMsg);
		XmTextSetInsertionPosition(SmartIndentDialog.newlineMacro, stoppedAt - widgetText);
		XmProcessTraversal(SmartIndentDialog.newlineMacro, XmTRAVERSE_CURRENT);
		XtFree(widgetText);
		return False;
	}
	XtFree(widgetText);
	FreeProgram(prog);

	/* Test compile the modify macro */
	if (!TextWidgetIsBlank(SmartIndentDialog.modMacro)) {
		widgetText = ensureNewline(XmTextGetString(SmartIndentDialog.modMacro));
		prog = ParseMacro(widgetText, &errMsg, &stoppedAt);
		if(!prog) {
			ParseError(SmartIndentDialog.shell, widgetText, stoppedAt, "modify macro", errMsg);
			XmTextSetInsertionPosition(SmartIndentDialog.modMacro, stoppedAt - widgetText);
			XmProcessTraversal(SmartIndentDialog.modMacro, XmTRAVERSE_CURRENT);
			XtFree(widgetText);
			return False;
		}
		XtFree(widgetText);
		FreeProgram(prog);
	}
	return True;
}

static smartIndentRec *getSmartIndentDialogData(void) {

	auto is = new smartIndentRec;
	is->lmName       = XtNewStringEx(SmartIndentDialog.langModeName);
	is->initMacro    = TextWidgetIsBlank(SmartIndentDialog.initMacro) ? nullptr : ensureNewline(XmTextGetString(SmartIndentDialog.initMacro));
	is->newlineMacro = TextWidgetIsBlank(SmartIndentDialog.newlineMacro) ? nullptr : ensureNewline(XmTextGetString(SmartIndentDialog.newlineMacro));
	is->modMacro     = TextWidgetIsBlank(SmartIndentDialog.modMacro) ? nullptr : ensureNewline(XmTextGetString(SmartIndentDialog.modMacro));
	return is;
}

static void setSmartIndentDialogData(smartIndentRec *is) {
	if(!is) {
		XmTextSetStringEx(SmartIndentDialog.initMacro, "");
		XmTextSetStringEx(SmartIndentDialog.newlineMacro, "");
		XmTextSetStringEx(SmartIndentDialog.modMacro, "");
	} else {
		if (!is->initMacro)
			XmTextSetStringEx(SmartIndentDialog.initMacro, "");
		else
			XmTextSetStringEx(SmartIndentDialog.initMacro, is->initMacro);
		XmTextSetStringEx(SmartIndentDialog.newlineMacro, is->newlineMacro);
		if (!is->modMacro)
			XmTextSetStringEx(SmartIndentDialog.modMacro, "");
		else
			XmTextSetStringEx(SmartIndentDialog.modMacro, is->modMacro);
	}
}

void EditCommonSmartIndentMacro(void) {
#define VERT_BORDER 4
	Widget form, topLbl;
	Widget okBtn, applyBtn, checkBtn;
	Widget closeBtn, restoreBtn;
	XmString s1;
	Arg args[20];
	int n;

	/* if the dialog is already displayed, just pop it to the top and return */
	if (CommonDialog.shell) {
		RaiseDialogWindow(CommonDialog.shell);
		return;
	}

	/* Create a form widget in an application shell */
	n = 0;
	XtSetArg(args[n], XmNdeleteResponse, XmDO_NOTHING);
	n++;
	XtSetArg(args[n], XmNiconName, "NEdit Common Smart Indent Macros");
	n++;
	XtSetArg(args[n], XmNtitle, "Common Smart Indent Macros");
	n++;
	CommonDialog.shell = CreateWidget(TheAppShell, "smartIndent", topLevelShellWidgetClass, args, n);
	AddSmallIcon(CommonDialog.shell);
	form = XtVaCreateManagedWidget("editCommonSIMacros", xmFormWidgetClass, CommonDialog.shell, XmNautoUnmanage, False, XmNresizePolicy, XmRESIZE_NONE, nullptr);
	XtAddCallback(form, XmNdestroyCallback, comDestroyCB, nullptr);
	AddMotifCloseCallback(CommonDialog.shell, comCloseCB, nullptr);

	topLbl = XtVaCreateManagedWidget("topLbl", xmLabelGadgetClass, form, XmNlabelString, s1 = XmStringCreateSimpleEx("Common Definitions for Smart Indent Macros"), XmNmnemonic, 'C', XmNtopAttachment, XmATTACH_FORM, XmNtopOffset,
	                                 VERT_BORDER, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 1, nullptr);

	okBtn = XtVaCreateManagedWidget("ok", xmPushButtonWidgetClass, form, XmNlabelString, s1 = XmStringCreateSimpleEx("OK"), XmNmarginWidth, BUTTON_WIDTH_MARGIN, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 6, XmNrightAttachment,
	                                XmATTACH_POSITION, XmNrightPosition, 18, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, VERT_BORDER, nullptr);
	XtAddCallback(okBtn, XmNactivateCallback, comOKCB, nullptr);
	XmStringFree(s1);

	applyBtn = XtVaCreateManagedWidget("apply", xmPushButtonWidgetClass, form, XmNlabelString, s1 = XmStringCreateSimpleEx("Apply"), XmNmnemonic, 'y', XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 22, XmNrightAttachment,
	                                   XmATTACH_POSITION, XmNrightPosition, 35, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, VERT_BORDER, nullptr);
	XtAddCallback(applyBtn, XmNactivateCallback, comApplyCB, nullptr);
	XmStringFree(s1);

	checkBtn = XtVaCreateManagedWidget("check", xmPushButtonWidgetClass, form, XmNlabelString, s1 = XmStringCreateSimpleEx("Check"), XmNmnemonic, 'k', XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 39, XmNrightAttachment,
	                                   XmATTACH_POSITION, XmNrightPosition, 52, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, VERT_BORDER, nullptr);
	XtAddCallback(checkBtn, XmNactivateCallback, comCheckCB, nullptr);
	XmStringFree(s1);

	restoreBtn = XtVaCreateManagedWidget("restore", xmPushButtonWidgetClass, form, XmNlabelString, s1 = XmStringCreateSimpleEx("Restore Default"), XmNmnemonic, 'f', XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 56,
	                                     XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, 77, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, VERT_BORDER, nullptr);
	XtAddCallback(restoreBtn, XmNactivateCallback, comRestoreCB, nullptr);
	XmStringFree(s1);

	closeBtn = XtVaCreateManagedWidget("close", xmPushButtonWidgetClass, form, XmNlabelString, s1 = XmStringCreateSimpleEx("Close"), XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 81, XmNrightAttachment, XmATTACH_POSITION,
	                                   XmNrightPosition, 94, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, VERT_BORDER, nullptr);
	XtAddCallback(closeBtn, XmNactivateCallback, comCloseCB, nullptr);
	XmStringFree(s1);

	n = 0;
	XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT);
	n++;
	XtSetArg(args[n], XmNrows, 24);
	n++;
	XtSetArg(args[n], XmNcolumns, 80);
	n++;
	XtSetArg(args[n], XmNvalue, CommonMacros);
	n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET);
	n++;
	XtSetArg(args[n], XmNtopWidget, topLbl);
	n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION);
	n++;
	XtSetArg(args[n], XmNleftPosition, 1);
	n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_POSITION);
	n++;
	XtSetArg(args[n], XmNrightPosition, 99);
	n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET);
	n++;
	XtSetArg(args[n], XmNbottomWidget, okBtn);
	n++;
	XtSetArg(args[n], XmNbottomOffset, VERT_BORDER);
	n++;
	CommonDialog.text = XmCreateScrolledText(form, (String) "commonText", args, n);
	AddMouseWheelSupport(CommonDialog.text);
	XtManageChild(CommonDialog.text);
	RemapDeleteKey(CommonDialog.text);
	XtVaSetValues(topLbl, XmNuserData, CommonDialog.text, nullptr);

	/* Set initial default button */
	XtVaSetValues(form, XmNdefaultButton, okBtn, nullptr);
	XtVaSetValues(form, XmNcancelButton, closeBtn, nullptr);

	/* Handle mnemonic selection of buttons and focus to dialog */
	AddDialogMnemonicHandler(form, FALSE);

	/* Realize all of the widgets in the new dialog */
	RealizeWithoutForcingPosition(CommonDialog.shell);
}

static void comDestroyCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	CommonDialog.shell = nullptr;
}

static void comOKCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	/* change the macro */
	if (!updateSmartIndentCommonData())
		return;

	/* pop down and destroy the dialog */
	XtDestroyWidget(CommonDialog.shell);
}

static void comApplyCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	/* change the macro */
	updateSmartIndentCommonData();
}

static void comCheckCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	if (checkSmartIndentCommonDialogData()) {
		DialogF(DF_INF, CommonDialog.shell, 1, "Macro compiled", "Macros compiled without error", "OK");
	}
}

static void comRestoreCB(Widget w, XtPointer clientData, XtPointer callData) {
	(void)w;
	(void)clientData;
	(void)callData;

	if (DialogF(DF_WARN, CommonDialog.shell, 2, "Discard Changes", "Are you sure you want to discard all\n"
	                                                               "changes to common smart indent macros",
	            "Discard", "Cancel") == 2) {
		return;
	}

	/* replace common macros with default */
	XtFree(CommonMacros);
	CommonMacros = XtNewStringEx(DefaultCommonMacros);

	/* Update the dialog */
	XmTextSetStringEx(CommonDialog.text, CommonMacros);
}

static void comCloseCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	/* pop down and destroy the dialog */
	XtDestroyWidget(CommonDialog.shell);
}

/*
** Update the smart indent macros being edited in the dialog
** with the information that the dialog is currently displaying, and
** apply changes to any window which is currently using the macros.
*/
static int updateSmartIndentCommonData(void) {
	WindowInfo *window;

	/* Make sure the patterns are valid and compile */
	if (!checkSmartIndentCommonDialogData())
		return False;

	/* Get the current data */
	CommonMacros = ensureNewline(XmTextGetString(CommonDialog.text));

	/* Re-execute initialization macros (macros require a window to function,
	   since user could theoretically execute an action routine, but it
	   probably won't be referenced in a smart indent initialization) */
	if (!ReadMacroString(WindowList, CommonMacros, "common macros"))
		return False;

	/* Find windows that are currently using smart indent and
	   re-initialize the smart indent macros (in case they have initialization
	   data which depends on common data) */
	for (window = WindowList; window != nullptr; window = window->next) {
		if (window->indentStyle == SMART_INDENT && window->languageMode != PLAIN_LANGUAGE_MODE) {
			EndSmartIndent(window);
			BeginSmartIndent(window, False);
		}
	}

	/* Note that preferences have been changed */
	MarkPrefsChanged();

	return True;
}

static int checkSmartIndentCommonDialogData(void) {
	char *widgetText;
	const char *stoppedAt;

	if (!TextWidgetIsBlank(CommonDialog.text)) {
		widgetText = ensureNewline(XmTextGetString(CommonDialog.text));
		if (!CheckMacroString(CommonDialog.shell, widgetText, "macros", &stoppedAt)) {
			XmTextSetInsertionPosition(CommonDialog.text, stoppedAt - widgetText);
			XmProcessTraversal(CommonDialog.text, XmTRAVERSE_CURRENT);
			XtFree(widgetText);
			return False;
		}
		XtFree(widgetText);
	}
	return True;
}

/*
** Update the smart indent macros being edited in the dialog
** with the information that the dialog is currently displaying, and
** apply changes to any window which is currently using the macros.
*/
static int updateSmartIndentData(void) {
	smartIndentRec *newMacros;
	WindowInfo *window;
	char *lmName;
	int i;

	/* Make sure the patterns are valid and compile */
	if (!checkSmartIndentDialogData())
		return False;

	/* Get the current data */
	newMacros = getSmartIndentDialogData();

	/* Find the original macros */
	for (i = 0; i < NSmartIndentSpecs; i++)
		if (!strcmp(SmartIndentDialog.langModeName, SmartIndentSpecs[i]->lmName))
			break;

	/* If it's a new language, add it at the end, otherwise free the
	   existing macros and replace it */
	if (i == NSmartIndentSpecs) {
		SmartIndentSpecs[NSmartIndentSpecs++] = newMacros;
	} else {
		freeIndentSpec(SmartIndentSpecs[i]);
		SmartIndentSpecs[i] = newMacros;
	}

	/* Find windows that are currently using this indent specification and
	   re-do the smart indent macros */
	for (window = WindowList; window != nullptr; window = window->next) {
		lmName = LanguageModeName(window->languageMode);
		if (lmName != nullptr && !strcmp(lmName, newMacros->lmName)) {
			window->SetSensitive(window->smartIndentItem, True);
			if (window->indentStyle == SMART_INDENT && window->languageMode != PLAIN_LANGUAGE_MODE) {
				EndSmartIndent(window);
				BeginSmartIndent(window, False);
			}
		}
	}

	/* Note that preferences have been changed */
	MarkPrefsChanged();

	return True;
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

int LoadSmartIndentStringEx(view::string_view string) {
	auto buffer = new char[string.size() + 1];
	std::copy(string.begin(), string.end(), buffer);
	buffer[string.size()] = '\0';
	int r = LoadSmartIndentString(buffer);
	delete [] buffer;
	return r;

}

int LoadSmartIndentString(char *inString) {
	const char *errMsg;
	const char *inPtr = inString;
	smartIndentRec is, *isCopy;
	int i;

	for (;;) {

		/* skip over blank space */
		inPtr += strspn(inPtr, " \t\n");

		/* finished */
		if (*inPtr == '\0')
			return True;

		/* read language mode name */
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

		/* read the newline macro */
		is.newlineMacro = readSIMacro(&inPtr);
		if(!is.newlineMacro) {
			XtFree((char *)is.lmName);
			XtFree((char *)is.initMacro);
			return siParseError(inString, inPtr, "no end boundary to newline macro");
		}

		/* read the modify macro */
		is.modMacro = readSIMacro(&inPtr);
		if(!is.modMacro) {
			XtFree((char *)is.lmName);
			XtFree((char *)is.initMacro);
			XtFree((char *)is.newlineMacro);
			return siParseError(inString, inPtr, "no end boundary to modify macro");
		}

		/* if there's no mod macro, make it null so it won't be executed */
		if (is.modMacro[0] == '\0') {
			XtFree((char *)is.modMacro);
			is.modMacro = nullptr;
		}

		/* create a new data structure and add/change it in the list */
		isCopy = new smartIndentRec(is);

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

	/* If called from -import, can replace existing ones */
	XtFree(CommonMacros);

	/* skip over blank space */
	inPtr += strspn(inPtr, " \t\n");

	/* look for "Default" keyword, and if it's there, return the default
	   smart common macro */
	if (!strncmp(inPtr, "Default", 7)) {
		CommonMacros = XtNewStringEx(DefaultCommonMacros);
		return True;
	}

	/* Remove leading tabs added by writer routine */
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
	
	/* Strip leading newline */
	if (**inPtr == '\n') {
		(*inPtr)++;
	}

	/* Find the end of the macro */
	const char *macroEnd = strstr(*inPtr, MacroEndBoundary);
	if(!macroEnd) {
		return nullptr;
	}

	/* Copy the macro */
	auto macroStr = new char[macroEnd - *inPtr + 1];
	strncpy(macroStr, *inPtr, macroEnd - *inPtr);
	macroStr[macroEnd - *inPtr] = '\0';

	/* Remove leading tabs added by writer routine */
	*inPtr = macroEnd + strlen(MacroEndBoundary);
	int shiftedLen;
	char *retStr = ShiftText(macroStr, SHIFT_LEFT, True, 8, 8, &shiftedLen);
	delete [] macroStr;
	return retStr;
}

static smartIndentRec *copyIndentSpec(smartIndentRec *is) {

	auto ris = new smartIndentRec;
	ris->lmName       = XtNewStringEx(is->lmName);
	ris->initMacro    = XtNewStringEx(is->initMacro);
	ris->newlineMacro = XtNewStringEx(is->newlineMacro);
	ris->modMacro     = XtNewStringEx(is->modMacro);
	return ris;
}

static void freeIndentSpec(smartIndentRec *is) {

	XtFree((char *)is->lmName);
	XtFree((char *)is->initMacro);
	XtFree((char *)is->newlineMacro);
	XtFree((char *)is->modMacro);
	// TODO(eteran): is this a memory leak? we never free is!
	// delete is;
}

static int indentSpecsDiffer(smartIndentRec *is1, smartIndentRec *is2) {
	return AllocatedStringsDiffer(is1->initMacro, is2->initMacro) || AllocatedStringsDiffer(is1->newlineMacro, is2->newlineMacro) || AllocatedStringsDiffer(is1->modMacro, is2->modMacro);
}

static int siParseError(const char *stringStart, const char *stoppedAt, const char *message) {
	return ParseError(nullptr, stringStart, stoppedAt, "smart indent specification", message);
}

std::string WriteSmartIndentStringEx(void) {

	auto outBuf = new TextBuffer;
	for (int i = 0; i < NSmartIndentSpecs; i++) {
		smartIndentRec *sis = SmartIndentSpecs[i];
		
		outBuf->BufInsertEx(outBuf->BufGetLength(), "\t");
		outBuf->BufInsertEx(outBuf->BufGetLength(), sis->lmName);
		outBuf->BufInsertEx(outBuf->BufGetLength(), ":");
		if (isDefaultIndentSpec(sis)) {
			outBuf->BufInsertEx(outBuf->BufGetLength(), "Default\n");
		} else {
			insertShiftedMacro(outBuf, (String)sis->initMacro);
			insertShiftedMacro(outBuf, (String)sis->newlineMacro);
			insertShiftedMacro(outBuf, (String)sis->modMacro);
		}
	}

	/* Get the output string, and lop off the trailing newline */
	std::string outStr = outBuf->BufGetRangeEx(0, outBuf->BufGetLength() > 0 ? outBuf->BufGetLength() - 1 : 0);
	delete outBuf;

	/* Protect newlines and backslashes from translation by the resource
	   reader */
	return EscapeSensitiveCharsEx(outStr);
}

std::string WriteSmartIndentCommonStringEx(void) {

	if (!strcmp(CommonMacros, DefaultCommonMacros))
		return "Default";

	if(!CommonMacros)
		return "";

	/* Shift the macro over by a tab to keep .nedit file bright and clean */
	std::string outStr = ShiftTextEx(CommonMacros, SHIFT_RIGHT, True, 8, 8);

	/* Protect newlines and backslashes from translation by the resource
	   reader */
	std::string escapedStr = EscapeSensitiveCharsEx(outStr);

	/* If there's a trailing escaped newline, remove it */
	int len = escapedStr.size();
	if (len > 1 && escapedStr[len - 1] == '\n' && escapedStr[len - 2] == '\\') {
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
		buf->BufInsertEx(buf->BufGetLength(), shiftedMacro);
	}
	buf->BufInsertEx(buf->BufGetLength(), "\t");
	buf->BufInsertEx(buf->BufGetLength(), MacroEndBoundary);
	buf->BufInsertEx(buf->BufGetLength(), "\n");
}

static int isDefaultIndentSpec(smartIndentRec *indentSpec) {
	int i;

	for (i = 0; i < N_DEFAULT_INDENT_SPECS; i++)
		if (!strcmp(indentSpec->lmName, DefaultIndentSpecs[i].lmName))
			return !indentSpecsDiffer(indentSpec, &DefaultIndentSpecs[i]);
	return False;
}

static smartIndentRec *findIndentSpec(const char *modeName) {
	int i;

	if(!modeName)
		return nullptr;

	for (i = 0; i < NSmartIndentSpecs; i++)
		if (!strcmp(modeName, SmartIndentSpecs[i]->lmName))
			return SmartIndentSpecs[i];
	return nullptr;
}

/*
** If "string" is not terminated with a newline character,  return a
** reallocated string which does end in a newline (otherwise, just pass on
** string as function value).  (The macro language requires newline terminators
** for statements, but the text widget doesn't force it like the NEdit text
** buffer does, so this might avoid some confusion.)
*/
static char *ensureNewline(char *string) {
	char *newString;
	int length;

	if(!string)
		return nullptr;
	length = strlen(string);
	if (length == 0 || string[length - 1] == '\n')
		return string;
	newString = XtMalloc(length + 2);
	strcpy(newString, string);
	newString[length] = '\n';
	newString[length + 1] = '\0';
	XtFree(string);
	return newString;
}

/*
** Returns True if there are smart indent macros, or potential macros
** not yet committed in the smart indent dialog for a language mode,
*/
int LMHasSmartIndentMacros(const char *languageMode) {
	if (findIndentSpec(languageMode) != nullptr)
		return True;
	return SmartIndentDialog.shell != nullptr && !strcmp(SmartIndentDialog.langModeName, languageMode);
}

/*
** Change the language mode name of smart indent macro sets for language
** "oldName" to "newName" in both the stored macro sets, and the pattern set
** currently being edited in the dialog.
*/
void RenameSmartIndentMacros(const char *oldName, const char *newName) {
	int i;

	for (i = 0; i < NSmartIndentSpecs; i++) {
		if (!strcmp(oldName, SmartIndentSpecs[i]->lmName)) {
			XtFree((char *)SmartIndentSpecs[i]->lmName);
			SmartIndentSpecs[i]->lmName = XtNewStringEx(newName);
		}
	}
	if (SmartIndentDialog.shell) {
		if (!strcmp(SmartIndentDialog.langModeName, oldName)) {
			XtFree(SmartIndentDialog.langModeName);
			SmartIndentDialog.langModeName = XtNewStringEx(newName);
		}
	}
}

/*
** If a smart indent dialog is up, ask to have the option menu for
** chosing language mode updated (via a call to CreateLanguageModeMenu)
*/
void UpdateLangModeMenuSmartIndent(void) {
	Widget oldMenu;

	if (SmartIndentDialog.shell == nullptr)
		return;

	oldMenu = SmartIndentDialog.lmPulldown;
	SmartIndentDialog.lmPulldown = CreateLanguageModeMenu(XtParent(XtParent(oldMenu)), langModeCB, nullptr);
	XtVaSetValues(XmOptionButtonGadget(SmartIndentDialog.lmOptMenu), XmNsubMenuId, SmartIndentDialog.lmPulldown, nullptr);
	SetLangModeMenu(SmartIndentDialog.lmOptMenu, SmartIndentDialog.langModeName);

	XtDestroyWidget(oldMenu);
}
