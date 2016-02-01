/*******************************************************************************
*                                                                              *
* window.c -- Nirvana Editor window creation/deletion                          *
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
* May 10, 1991                                                                 *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/

#include "window.h"
#include "TextBuffer.h"
#include "textSel.h"
#include "text.h"
#include "textDisp.h"
#include "Document.h"
#include "textP.h"
#include "UndoInfo.h"
#include "nedit.h"
#include "menu.h"
#include "file.h"
#include "search.h"
#include "undo.h"
#include "preferences.h"
#include "selection.h"
#include "server.h"
#include "shell.h"
#include "macro.h"
#include "highlight.h"
#include "smartIndent.h"
#include "userCmds.h"
#include "n.bm"
#include "windowTitle.h"
#include "interpret.h"
#include "RangesetTable.h"
#include "Rangeset.h"
#include "MotifHelper.h"
#include "../util/clearcase.h"
#include "../util/misc.h"
#include "../util/fileUtils.h"
#include "../util/utils.h"
#include "../util/fileUtils.h"
#include "../util/DialogF.h"
#include "../Xlt/BubbleButtonP.h"
#include "../Microline/XmL/Folder.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <vector>
#include <sys/stat.h>

#include <sys/param.h>
#include "../util/clearcase.h"

#include <climits>
#include <cmath>
#include <cctype>
#include <ctime>
#include <sys/time.h>

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/Xatom.h>
#include <Xm/Xm.h>
#include <Xm/MainW.h>
#include <Xm/PanedW.h>
#include <Xm/PanedWP.h>
#include <Xm/RowColumnP.h>
#include <Xm/Separator.h>
#include <Xm/Text.h>
#include <Xm/ToggleB.h>
#include <Xm/PushB.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/Label.h>
#include <Xm/SelectioB.h>
#include <Xm/List.h>
#include <Xm/Protocols.h>
#include <Xm/ScrolledW.h>
#include <Xm/ScrollBar.h>
#include <Xm/PrimitiveP.h>
#include <Xm/Frame.h>
#include <Xm/CascadeB.h>

#ifdef EDITRES
#include <X11/Xmu/Editres.h>
/* extern void _XEditResCheckMessages(); */
#endif

/* Initial minimum height of a pane.  Just a fallback in case setPaneMinHeight
   (which may break in a future release) is not available */
#define PANE_MIN_HEIGHT 39

/* Thickness of 3D border around statistics and/or incremental search areas
   below the main menu bar */
#define STAT_SHADOW_THICKNESS 1



extern void _XmDismissTearOff(Widget, XtPointer, XtPointer);

static void hideTooltip(Widget tab);
static Widget addTab(Widget folder, const char *string);
static Widget createTextArea(Widget parent, Document *window, int rows, int cols, int emTabDist, char *delimiters, int wrapMargin, int lineNumCols);
static void focusCB(Widget w, XtPointer clientData, XtPointer callData);
static void modifiedCB(int pos, int nInserted, int nDeleted, int nRestyled, view::string_view deletedText, void *cbArg);
static void movedCB(Widget w, XtPointer clientData, XtPointer callData);
static void dragStartCB(Widget w, XtPointer clientData, XtPointer callData);
static void dragEndCB(Widget w, XtPointer clientData, XtPointer callData);
static void saveYourselfCB(Widget w, XtPointer clientData, XtPointer callData);
static Widget containingPane(Widget w);
static void tabClickEH(Widget w, XtPointer clientData, XEvent *event);











/*
** find which document a tab belongs to
*/
Document *TabToWindow(Widget tab) {
	for (Document *win = WindowList; win; win = win->next) {
		if (win->tab == tab)
			return win;
	}

	return nullptr;
}





/*
** Check if there is already a window open for a given file
*/
Document *FindWindowWithFile(const char *name, const char *path) {
	Document *window;

	/* I don't think this algorithm will work on vms so I am
	   disabling it for now */
	if (!GetPrefHonorSymlinks()) {
		char fullname[MAXPATHLEN + 1];
		struct stat attribute;

		strncpy(fullname, path, MAXPATHLEN);
		strncat(fullname, name, MAXPATHLEN);
		fullname[MAXPATHLEN] = '\0';

		if (stat(fullname, &attribute) == 0) {
			for (window = WindowList; window != nullptr; window = window->next) {
				if (attribute.st_dev == window->device && attribute.st_ino == window->inode) {
					return window;
				}
			}
		} /*  else:  Not an error condition, just a new file. Continue to check
		      whether the filename is already in use for an unsaved document.  */
	}

	for (window = WindowList; window != nullptr; window = window->next) {
		if (!strcmp(window->filename, name) && !strcmp(window->path, path)) {
			return window;
		}
	}
	return nullptr;
}







/*
** Count the windows
*/
int NWindows(void) {
	int n = 0;

	for (Document *win = WindowList; win != nullptr; win = win->next) {
		++n;
	}
	return n;
}















/*
** Recover the window pointer from any widget in the window, by searching
** up the widget hierarcy for the top level container widget where the
** window pointer is stored in the userData field. In a tabbed window,
** this is the window pointer of the top (active) document, which is
** returned if w is 'shell-level' widget - menus, find/replace dialogs, etc.
**
** To support action routine in tabbed windows, a copy of the window
** pointer is also store in the splitPane widget.
*/
Document *WidgetToWindow(Widget w) {
	Document *window = nullptr;
	Widget parent;

	while (true) {
		/* return window pointer of document */
		if (XtClass(w) == xmPanedWindowWidgetClass)
			break;

		if (XtClass(w) == topLevelShellWidgetClass) {
			WidgetList items;

			/* there should be only 1 child for the shell -
			   the main window widget */
			XtVaGetValues(w, XmNchildren, &items, nullptr);
			w = items[0];
			break;
		}

		parent = XtParent(w);
		if(!parent)
			return nullptr;

		/* make sure it is not a dialog shell */
		if (XtClass(parent) == topLevelShellWidgetClass && XmIsMainWindow(w))
			break;

		w = parent;
	}

	XtVaGetValues(w, XmNuserData, &window, nullptr);

	return window;
}







/*
** Find the start and end of a single line selection.  Hides rectangular
** selection issues for older routines which use selections that won't
** span lines.
*/
int GetSimpleSelection(TextBuffer *buf, int *left, int *right) {
	int selStart, selEnd, rectStart, rectEnd, lineStart;
	bool isRect;

	/* get the character to match and its position from the selection, or
	   the character before the insert point if nothing is selected.
	   Give up if too many characters are selected */
	if (!buf->BufGetSelectionPos(&selStart, &selEnd, &isRect, &rectStart, &rectEnd))
		return False;
	if (isRect) {
		lineStart = buf->BufStartOfLine(selStart);
		selStart  = buf->BufCountForwardDispChars(lineStart, rectStart);
		selEnd    = buf->BufCountForwardDispChars(lineStart, rectEnd);
	}
	*left = selStart;
	*right = selEnd;
	return True;
}





#ifndef NO_SESSION_RESTART
static void saveYourselfCB(Widget w, XtPointer clientData, XtPointer callData) {

	auto appShell = static_cast<Widget>(clientData);
	
	(void)w;
	(void)callData;

	Document *win, *topWin;
	char geometry[MAX_GEOM_STRING_LEN];
	int i;
	int wasIconic = False;
	int nItems;
	WidgetList children;

	/* Allocate memory for an argument list and for a reversed list of
	   windows.  The window list is reversed for IRIX 4DWM and any other
	   window/session manager combination which uses window creation
	   order for re-associating stored geometry information with
	   new windows created by a restored application */
	int nWindows = 0;
	for (Document *win = WindowList; win != nullptr; win = win->next) {
		nWindows++;
	}
	
	auto revWindowList = new Document *[nWindows];
	for (win = WindowList, i = nWindows - 1; win != nullptr; win = win->next, i--) {
		revWindowList[i] = win;
	}

	/* Create command line arguments for restoring each window in the list */
	std::vector<char *> argv;
	argv.push_back(XtNewStringEx(ArgV0));
	
	if (IsServer) {
		argv.push_back(XtNewStringEx("-server"));
		
		if (GetPrefServerName()[0] != '\0') {
			argv.push_back(XtNewStringEx("-svrname"));
			argv.push_back(XtNewStringEx(GetPrefServerName()));
		}
	}

	/* editor windows are popup-shell children of top-level appShell */
	XtVaGetValues(appShell, XmNchildren, &children, XmNnumChildren, &nItems, nullptr);

	for (int n = nItems - 1; n >= 0; n--) {
		WidgetList tabs;
		int tabCount;

		if (strcmp(XtName(children[n]), "textShell") || ((topWin = WidgetToWindow(children[n])) == nullptr)) {
			continue; /* skip non-editor windows */
		}

		/* create a group for each window */
		topWin->getGeometryString(geometry);
		argv.push_back(XtNewStringEx("-group"));
		argv.push_back(XtNewStringEx("-geometry"));
		argv.push_back(XtNewStringEx(geometry));
		
		if (topWin->IsIconic()) {
			argv.push_back(XtNewStringEx("-iconic"));
			wasIconic = True;
		} else if (wasIconic) {
			argv.push_back(XtNewStringEx("-noiconic"));
			wasIconic = False;
		}

		/* add filename of each tab in window... */
		XtVaGetValues(topWin->tabBar, XmNtabWidgetList, &tabs, XmNtabCount, &tabCount, nullptr);

		for (int i = 0; i < tabCount; i++) {
			win = TabToWindow(tabs[i]);
			if (win->filenameSet) {
				/* add filename */
				auto p = new char[strlen(win->path) + strlen(win->filename) + 1];
				sprintf(p, "%s%s", win->path, win->filename);
				argv.push_back(p);
			}
		}
	}

	delete [] revWindowList;

	/* Set the window's WM_COMMAND property to the created command line */
	XSetCommand(TheDisplay, XtWindow(appShell), &argv[0], argv.size());
	for (char *p : argv) {
		delete [] p;
	}
}

void AttachSessionMgrHandler(Widget appShell) {
	static Atom wmpAtom, syAtom = 0;

	/* Add wm protocol callback for making nedit restartable by session
	   managers.  Doesn't yet handle multiple-desktops or iconifying right. */
	if (syAtom == 0) {
		wmpAtom = XmInternAtom(TheDisplay, (String) "WM_PROTOCOLS", FALSE);
		syAtom = XmInternAtom(TheDisplay, (String) "WM_SAVE_YOURSELF", FALSE);
	}
	
	XmAddProtocolCallback(appShell, wmpAtom, syAtom, saveYourselfCB, (XtPointer)appShell);
}
#endif /* NO_SESSION_RESTART */













static bool currentlyBusy = false;
static long busyStartTime = 0;
static bool modeMessageSet = false;

/*
 * Auxiliary function for measuring elapsed time during busy waits.
 */
static long getRelTimeInTenthsOfSeconds() {
	struct timeval current;
	gettimeofday(&current, nullptr);
	return (current.tv_sec * 10 + current.tv_usec / 100000) & 0xFFFFFFFL;
}

void AllWindowsBusy(const char *message) {
	Document *w;

	if (!currentlyBusy) {
		busyStartTime = getRelTimeInTenthsOfSeconds();
		modeMessageSet = False;

		for (w = WindowList; w; w = w->next) {
			/* We don't the display message here yet, but defer it for
			   a while. If the wait is short, we don't want
			   to have it flash on and off the screen.  However,
			   we can't use a time since in generally we are in
			   a tight loop and only processing exposure events, so it's
			   up to the caller to make sure that this routine is called
			   at regular intervals.
			*/
			BeginWait(w->shell);
		}
	} else if (!modeMessageSet && message && getRelTimeInTenthsOfSeconds() - busyStartTime > 10) {
		/* Show the mode message when we've been busy for more than a second */
		for (w = WindowList; w; w = w->next) {
			w->SetModeMessage(message);
		}
		modeMessageSet = True;
	}
	BusyWait(WindowList->shell);

	currentlyBusy = True;
}

void AllWindowsUnbusy(void) {

	for (Document *w = WindowList; w; w = w->next) {
		w->ClearModeMessage();
		EndWait(w->shell);
	}

	currentlyBusy = False;
	modeMessageSet = False;
	busyStartTime = 0;
}









void AddSmallIcon(Widget shell) {
	static Pixmap iconPixmap = 0, maskPixmap = 0;

	if (iconPixmap == 0) {
		iconPixmap = XCreateBitmapFromData(TheDisplay, RootWindowOfScreen(XtScreen(shell)), (char *)n_bits, n_width, n_height);
		maskPixmap = XCreateBitmapFromData(TheDisplay, RootWindowOfScreen(XtScreen(shell)), (char *)n_mask, n_width, n_height);
	}
	XtVaSetValues(shell, XmNiconPixmap, iconPixmap, XmNiconMask, maskPixmap, nullptr);
}











/*
** Create a new document in the shell window.
** Document are created in 'background' so that the user
** menus, ie. the Macro/Shell/BG menus, will not be updated
** unnecessarily; hence speeding up the process of opening
** multiple files.
*/
Document *Document::CreateDocument(const char *name) {
	Widget pane, text;
	int nCols, nRows;

	/* Allocate some memory for the new window data structure */
	auto window = new Document(*this);

#if 0
    /* share these dialog items with parent shell */
    window->replaceDlog = nullptr;
    window->replaceText = nullptr;
    window->replaceWithText = nullptr;
    window->replaceWordToggle = nullptr;
    window->replaceCaseToggle = nullptr;
    window->replaceRegexToggle = nullptr;
    window->findDlog = nullptr;
    window->findText = nullptr;
    window->findWordToggle = nullptr;
    window->findCaseToggle = nullptr;
    window->findRegexToggle = nullptr;
    window->replaceMultiFileDlog = nullptr;
    window->replaceMultiFilePathBtn = nullptr;
    window->replaceMultiFileList = nullptr;
    window->showLineNumbers = GetPrefLineNums();
    window->showStats = GetPrefStatsLine();
    window->showISearchLine = GetPrefISearchLine();
#endif

	window->multiFileReplSelected = FALSE;
	window->multiFileBusy = FALSE;
	window->writableWindows = nullptr;
	window->nWritableWindows = 0;
	window->fileChanged = FALSE;
	window->fileMissing = True;
	window->fileMode = 0;
	window->fileUid = 0;
	window->fileGid = 0;
	window->filenameSet = FALSE;
	window->fileFormat = UNIX_FILE_FORMAT;
	window->lastModTime = 0;
	strcpy(window->filename, name);
	window->undo = std::list<UndoInfo *>();
	window->redo = std::list<UndoInfo *>();
	window->nPanes = 0;
	window->autoSaveCharCount = 0;
	window->autoSaveOpCount = 0;
	window->undoMemUsed = 0;
	CLEAR_ALL_LOCKS(window->lockReasons);
	window->indentStyle = GetPrefAutoIndent(PLAIN_LANGUAGE_MODE);
	window->autoSave = GetPrefAutoSave();
	window->saveOldVersion = GetPrefSaveOldVersion();
	window->wrapMode = GetPrefWrap(PLAIN_LANGUAGE_MODE);
	window->overstrike = False;
	window->showMatchingStyle = GetPrefShowMatching();
	window->matchSyntaxBased = GetPrefMatchSyntaxBased();
	window->highlightSyntax = GetPrefHighlightSyntax();
	window->backlightCharTypes = nullptr;
	window->backlightChars = GetPrefBacklightChars();
	if (window->backlightChars) {
		const char *cTypes = GetPrefBacklightCharTypes();
		if (cTypes && window->backlightChars) {			
			window->backlightCharTypes = XtStringDup(cTypes);
		}
	}
	window->modeMessageDisplayed = FALSE;
	window->modeMessage = nullptr;
	window->ignoreModify = FALSE;
	window->windowMenuValid = FALSE;
	window->flashTimeoutID = 0;
	window->fileClosedAtom = None;
	window->wasSelected = FALSE;
	strcpy(window->fontName, GetPrefFontName());
	strcpy(window->italicFontName, GetPrefItalicFontName());
	strcpy(window->boldFontName, GetPrefBoldFontName());
	strcpy(window->boldItalicFontName, GetPrefBoldItalicFontName());
	window->colorDialog = nullptr;
	window->fontList = GetPrefFontList();
	window->italicFontStruct = GetPrefItalicFont();
	window->boldFontStruct = GetPrefBoldFont();
	window->boldItalicFontStruct = GetPrefBoldItalicFont();
	window->fontDialog = nullptr;
	window->nMarks = 0;
	window->markTimeoutID = 0;
	window->highlightData = nullptr;
	window->shellCmdData = nullptr;
	window->macroCmdData = nullptr;
	window->smartIndentData = nullptr;
	window->languageMode = PLAIN_LANGUAGE_MODE;
	window->iSearchHistIndex = 0;
	window->iSearchStartPos = -1;
	window->replaceLastRegexCase = TRUE;
	window->replaceLastLiteralCase = FALSE;
	window->iSearchLastRegexCase = TRUE;
	window->iSearchLastLiteralCase = FALSE;
	window->findLastRegexCase = TRUE;
	window->findLastLiteralCase = FALSE;
	window->tab = nullptr;
	window->bgMenuUndoItem = nullptr;
	window->bgMenuRedoItem = nullptr;
	window->device = 0;
	window->inode = 0;

	if (!window->fontList)
		XtVaGetValues(this->statsLine, XmNfontList, &window->fontList, nullptr);

	this->getTextPaneDimension(&nRows, &nCols);

	/* Create pane that actaully holds the new document. As
	   document is created in 'background', we need to hide
	   it. If we leave it unmanaged without setting it to
	   the XmNworkWindow of the mainWin, due to a unknown
	   bug in Motif where splitpane's scrollWindow child
	   somehow came up with a height taller than the splitpane,
	   the bottom part of the text editing widget is obstructed
	   when later brought up by  RaiseDocument(). So we first
	   manage it hidden, then unmanage it and reset XmNworkWindow,
	   then let RaiseDocument() show it later. */
	pane = XtVaCreateWidget("pane", xmPanedWindowWidgetClass, window->mainWin, XmNmarginWidth, 0, XmNmarginHeight, 0, XmNseparatorOn, False, XmNspacing, 3, XmNsashIndent, -2, XmNmappedWhenManaged, False, nullptr);
	XtVaSetValues(window->mainWin, XmNworkWindow, pane, nullptr);
	XtManageChild(pane);
	window->splitPane = pane;

	/* Store a copy of document/window pointer in text pane to support
	   action procedures. See also WidgetToWindow() for info. */
	XtVaSetValues(pane, XmNuserData, window, nullptr);

	/* Patch around Motif's most idiotic "feature", that its menu accelerators
	   recognize Caps Lock and Num Lock as modifiers, and don't trigger if
	   they are engaged */
	AccelLockBugPatch(pane, window->menuBar);

	/* Create the first, and most permanent text area (other panes may
	   be added & removed, but this one will never be removed */
	text = createTextArea(pane, window, nRows, nCols, GetPrefEmTabDist(PLAIN_LANGUAGE_MODE), GetPrefDelimiters(), GetPrefWrapMargin(), window->showLineNumbers ? MIN_LINE_NUM_COLS : 0);
	XtManageChild(text);
	window->textArea = text;
	window->lastFocus = text;

	/* Set the initial colors from the globals. */
	window->SetColors(GetPrefColorName(TEXT_FG_COLOR), GetPrefColorName(TEXT_BG_COLOR), GetPrefColorName(SELECT_FG_COLOR), GetPrefColorName(SELECT_BG_COLOR), GetPrefColorName(HILITE_FG_COLOR), GetPrefColorName(HILITE_BG_COLOR),
	          GetPrefColorName(LINENO_FG_COLOR), GetPrefColorName(CURSOR_FG_COLOR));

	/* Create the right button popup menu (note: order is important here,
	   since the translation for popping up this menu was probably already
	   added in createTextArea, but CreateBGMenu requires window->textArea
	   to be set so it can attach the menu to it (because menu shells are
	   finicky about the kinds of widgets they are attached to)) */
	window->bgMenuPane = CreateBGMenu(window);

	/* cache user menus: init. user background menu cache */
	InitUserBGMenuCache(&window->userBGMenuCache);

	/* Create the text buffer rather than using the one created automatically
	   with the text area widget.  This is done so the syntax highlighting
	   modify callback can be called to synchronize the style buffer BEFORE
	   the text display's callback is called upon to display a modification */
	window->buffer = new TextBuffer;
	window->buffer->BufAddModifyCB(SyntaxHighlightModifyCB, window);

	/* Attach the buffer to the text widget, and add callbacks for modify */
	TextSetBuffer(text, window->buffer);
	window->buffer->BufAddModifyCB(modifiedCB, window);

	/* Designate the permanent text area as the owner for selections */
	HandleXSelections(text);

	/* Set the requested hardware tab distance and useTabs in the text buffer */
	window->buffer->BufSetTabDistance(GetPrefTabDist(PLAIN_LANGUAGE_MODE));
	window->buffer->useTabs_ = GetPrefInsertTabs();
	window->tab = addTab(window->tabBar, name);

	/* add the window to the global window list, update the Windows menus */
	InvalidateWindowMenus();
	window->addToWindowList();

	/* return the shell ownership to previous tabbed doc */
	XtVaSetValues(window->mainWin, XmNworkWindow, this->splitPane, nullptr);
	XLowerWindow(TheDisplay, XtWindow(window->splitPane));
	XtUnmanageChild(window->splitPane);
	XtVaSetValues(window->splitPane, XmNmappedWhenManaged, True, nullptr);

	return window;
}



















Document *GetTopDocument(Widget w) {
	Document *window = WidgetToWindow(w);

	return WidgetToWindow(window->shell);
}


// TODO(eteran): temporary duplicate
//----------------------------------------------------------------------------------------


Widget containingPane(Widget w) {
	/* The containing pane used to simply be the first parent, but with
	   the introduction of an XmFrame, it's the grandparent. */
	return XtParent(XtParent(w));
}

static void modifiedCB(int pos, int nInserted, int nDeleted, int nRestyled, view::string_view deletedText, void *cbArg) {

	(void)nRestyled;

	Document *window = (Document *)cbArg;
	int selected = window->buffer->primary_.selected;

	/* update the table of bookmarks */
	if (!window->ignoreModify) {
		UpdateMarkTable(window, pos, nInserted, nDeleted);
	}

	/* Check and dim/undim selection related menu items */
	if ((window->wasSelected && !selected) || (!window->wasSelected && selected)) {
		window->wasSelected = selected;

		/* do not refresh shell-level items (window, menu-bar etc)
		   when motifying non-top document */
		if (window->IsTopDocument()) {
			XtSetSensitive(window->printSelItem, selected);
			XtSetSensitive(window->cutItem, selected);
			XtSetSensitive(window->copyItem, selected);
			XtSetSensitive(window->delItem, selected);
			/* Note we don't change the selection for items like
			   "Open Selected" and "Find Selected".  That's because
			   it works on selections in external applications.
			   Desensitizing it if there's no NEdit selection
			   disables this feature. */
			XtSetSensitive(window->filterItem, selected);

			DimSelectionDepUserMenuItems(window, selected);
			if (window->replaceDlog != nullptr && XtIsManaged(window->replaceDlog)) {
				UpdateReplaceActionButtons(window);
			}
		}
	}

	/* When the program needs to make a change to a text area without without
	   recording it for undo or marking file as changed it sets ignoreModify */
	if (window->ignoreModify || (nDeleted == 0 && nInserted == 0))
		return;

	/* Make sure line number display is sufficient for new data */
	window->updateLineNumDisp();

	/* Save information for undoing this operation (this call also counts
	   characters and editing operations for triggering autosave */
	window->SaveUndoInformation(pos, nInserted, nDeleted, deletedText);

	/* Trigger automatic backup if operation or character limits reached */
	if (window->autoSave && (window->autoSaveCharCount > AUTOSAVE_CHAR_LIMIT || window->autoSaveOpCount > AUTOSAVE_OP_LIMIT)) {
		WriteBackupFile(window);
		window->autoSaveCharCount = 0;
		window->autoSaveOpCount = 0;
	}

	/* Indicate that the window has now been modified */
	window->SetWindowModified(TRUE);

	/* Update # of bytes, and line and col statistics */
	window->UpdateStatsLine();

	/* Check if external changes have been made to file and warn user */
	CheckForChangesToFile(window);
}

static Widget createTextArea(Widget parent, Document *window, int rows, int cols, int emTabDist, char *delimiters, int wrapMargin, int lineNumCols) {

	/* Create a text widget inside of a scrolled window widget */
	Widget sw         = XtVaCreateManagedWidget("scrolledW", xmScrolledWindowWidgetClass, parent, XmNpaneMaximum, SHRT_MAX, XmNpaneMinimum, PANE_MIN_HEIGHT, XmNhighlightThickness, 0, nullptr);
	Widget hScrollBar = XtVaCreateManagedWidget("textHorScrollBar", xmScrollBarWidgetClass, sw, XmNorientation, XmHORIZONTAL, XmNrepeatDelay, 10, nullptr);
	Widget vScrollBar = XtVaCreateManagedWidget("textVertScrollBar", xmScrollBarWidgetClass, sw, XmNorientation, XmVERTICAL, XmNrepeatDelay, 10, nullptr);
	Widget frame      = XtVaCreateManagedWidget("textFrame", xmFrameWidgetClass, sw, XmNshadowType, XmSHADOW_IN, nullptr);
	
	Widget text = XtVaCreateManagedWidget(
		"text", 
		textWidgetClass, 
		frame, 
		textNbacklightCharTypes, 
		window->backlightCharTypes, 
		textNrows, 
		rows, 
		textNcolumns, 
		cols, 
		textNlineNumCols, 
		lineNumCols, 
		textNemulateTabs, 
		emTabDist, 
		textNfont,
	    GetDefaultFontStruct(window->fontList), 
		textNhScrollBar, 
		hScrollBar, 
		textNvScrollBar, 
		vScrollBar, 
		textNreadOnly, 
		IS_ANY_LOCKED(window->lockReasons), 
		textNwordDelimiters, 
		delimiters, 
		textNwrapMargin,
	    wrapMargin, 
		textNautoIndent, 
		window->indentStyle == AUTO_INDENT, 
		textNsmartIndent, 
		window->indentStyle == SMART_INDENT, 
		textNautoWrap, 
		window->wrapMode == NEWLINE_WRAP, 
		textNcontinuousWrap,
	    window->wrapMode == CONTINUOUS_WRAP, 
		textNoverstrike, 
		window->overstrike, 
		textNhidePointer, 
		(Boolean)GetPrefTypingHidesPointer(), 
		textNcursorVPadding, 
		GetVerticalAutoScroll(), 
		nullptr);

	XtVaSetValues(sw, XmNworkWindow, frame, XmNhorizontalScrollBar, hScrollBar, XmNverticalScrollBar, vScrollBar, nullptr);

	/* add focus, drag, cursor tracking, and smart indent callbacks */
	XtAddCallback(text, textNfocusCallback, focusCB, window);
	XtAddCallback(text, textNcursorMovementCallback, movedCB, window);
	XtAddCallback(text, textNdragStartCallback, dragStartCB, window);
	XtAddCallback(text, textNdragEndCallback, dragEndCB, window);
	XtAddCallback(text, textNsmartIndentCallback, SmartIndentCB, window);

	/* This makes sure the text area initially has a the insert point shown
	   ... (check if still true with the nedit text widget, probably not) */
	XmAddTabGroup(containingPane(text));

	/* compensate for Motif delete/backspace problem */
	RemapDeleteKey(text);

	/* Augment translation table for right button popup menu */
	AddBGMenuAction(text);

	/* If absolute line numbers will be needed for display in the statistics
	   line, tell the widget to maintain them (otherwise, it's a costly
	   operation and performance will be better without it) */
	reinterpret_cast<TextWidget>(text)->text.textD->TextDMaintainAbsLineNum(window->showStats);

	return text;
}

static void focusCB(Widget w, XtPointer clientData, XtPointer callData) {

	auto window = static_cast<Document *>(clientData);

	(void)callData;

	/* record which window pane last had the keyboard focus */
	window->lastFocus = w;

	/* update line number statistic to reflect current focus pane */
	window->UpdateStatsLine();

	/* finish off the current incremental search */
	EndISearch(window);

	/* Check for changes to read-only status and/or file modifications */
	CheckForChangesToFile(window);
}

static void movedCB(Widget w, XtPointer clientData, XtPointer callData) {

	auto window = static_cast<Document *>(clientData);

	(void)callData;

	TextWidget textWidget = (TextWidget)w;

	if (window->ignoreModify)
		return;

	/* update line and column nubers in statistics line */
	window->UpdateStatsLine();

	/* Check the character before the cursor for matchable characters */
	FlashMatching(window, w);

	/* Check for changes to read-only status and/or file modifications */
	CheckForChangesToFile(window);

	/*  This callback is not only called for focussed panes, but for newly
	    created panes as well. So make sure that the cursor is left alone
	    for unfocussed panes.
	    TextWidget have no state per se about focus, so we use the related
	    ID for the blink procedure.  */
	if (textWidget->text.cursorBlinkProcID != 0) {
		/*  Start blinking the caret again.  */
		ResetCursorBlink(textWidget, False);
	}
}

static void dragStartCB(Widget w, XtPointer clientData, XtPointer callData) {

	auto window = static_cast<Document *>(clientData);

	(void)callData;
	(void)w;

	/* don't record all of the intermediate drag steps for undo */
	window->ignoreModify = True;
}

static void dragEndCB(Widget w, XtPointer clientData, XtPointer call_data) {

	(void)w;
	
	auto window   = static_cast<Document *>(clientData);
	auto callData = static_cast<dragEndCBStruct *>(call_data);

	/* restore recording of undo information */
	window->ignoreModify = False;

	/* Do nothing if drag operation was canceled */
	if (callData->nCharsInserted == 0)
		return;

	/* Save information for undoing this operation not saved while
	   undo recording was off */
	modifiedCB(callData->startPos, callData->nCharsInserted, callData->nCharsDeleted, 0, callData->deletedText, window);
}

/*
** add a tab to the tab bar for the new document.
*/
static Widget addTab(Widget folder, const char *string) {
	Widget tooltipLabel, tab;
	XmString s1;

	s1 = XmStringCreateSimpleEx(string);
	tab = XtVaCreateManagedWidget("tab", xrwsBubbleButtonWidgetClass, folder,
	                              /* XmNmarginWidth, <default@nedit.c>, */
	                              /* XmNmarginHeight, <default@nedit.c>, */
	                              /* XmNalignment, <default@nedit.c>, */
	                              XmNlabelString, s1, XltNbubbleString, s1, XltNshowBubble, GetPrefToolTips(), XltNautoParkBubble, True, XltNslidingBubble, False,
	                              /* XltNdelay, 800,*/
	                              /* XltNbubbleDuration, 8000,*/
	                              nullptr);
	XmStringFree(s1);

	/* there's things to do as user click on the tab */
	XtAddEventHandler(tab, ButtonPressMask, False, (XtEventHandler)tabClickEH, nullptr);

	/* BubbleButton simply use reversed video for tooltips,
	   we try to use the 'standard' color */
	tooltipLabel = XtNameToWidget(tab, "*BubbleLabel");
	XtVaSetValues(tooltipLabel, XmNbackground, AllocateColor(tab, GetPrefTooltipBgColor()), XmNforeground, AllocateColor(tab, NEDIT_DEFAULT_FG), nullptr);

	/* put borders around tooltip. BubbleButton use
	   transientShellWidgetClass as tooltip shell, which
	   came without borders */
	XtVaSetValues(XtParent(tooltipLabel), XmNborderWidth, 1, nullptr);

	return tab;
}

static void hideTooltip(Widget tab) {
	if (Widget tooltip = XtNameToWidget(tab, "*BubbleShell"))
		XtPopdown(tooltip);
}

/*
** ButtonPress event handler for tabs.
*/
static void tabClickEH(Widget w, XtPointer clientData, XEvent *event) {

	(void)clientData;
	(void)event;

	/* hide the tooltip when user clicks with any button. */
	if (BubbleButton_Timer(w)) {
		XtRemoveTimeOut(BubbleButton_Timer(w));
		BubbleButton_Timer(w) = (XtIntervalId) nullptr;
	} else {
		hideTooltip(w);
	}
}
