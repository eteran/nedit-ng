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
#include "nedit.bm"
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

/* bitmap data for the close-tab button */
#define close_width 11
#define close_height 11
static unsigned char close_bits[] = {0x00, 0x00, 0x00, 0x00, 0x8c, 0x01, 0xdc, 0x01, 0xf8, 0x00, 0x70, 0x00, 0xf8, 0x00, 0xdc, 0x01, 0x8c, 0x01, 0x00, 0x00, 0x00, 0x00};

/* bitmap data for the isearch-find button */
#define isrcFind_width 11
#define isrcFind_height 11
static unsigned char isrcFind_bits[] = {0xe0, 0x01, 0x10, 0x02, 0xc8, 0x04, 0x08, 0x04, 0x08, 0x04, 0x00, 0x04, 0x18, 0x02, 0xdc, 0x01, 0x0e, 0x00, 0x07, 0x00, 0x03, 0x00};

/* bitmap data for the isearch-clear button */
#define isrcClear_width 11
#define isrcClear_height 11
static unsigned char isrcClear_bits[] = {0x00, 0x00, 0x00, 0x00, 0x04, 0x01, 0x84, 0x01, 0xc4, 0x00, 0x64, 0x00, 0xc4, 0x00, 0x84, 0x01, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00};

extern void _XmDismissTearOff(Widget, XtPointer, XtPointer);

static void hideTooltip(Widget tab);
static Pixmap createBitmapWithDepth(Widget w, char *data, unsigned int width, unsigned int height);
static Widget addTab(Widget folder, const char *string);
static int getTabPosition(Widget tab);
static Widget manageToolBars(Widget toolBarsForm);
static void hideTearOffs(Widget menuPane);
static void CloseDocumentWindow(Widget w, XtPointer clientData, XtPointer callData);
static void closeTabCB(Widget w, XtPointer clientData, XtPointer callData);
static void raiseTabCB(Widget w, XtPointer clientData, XtPointer callData);
static Widget createTextArea(Widget parent, Document *window, int rows, int cols, int emTabDist, char *delimiters, int wrapMargin, int lineNumCols);
static void focusCB(Widget w, XtPointer clientData, XtPointer callData);
static void modifiedCB(int pos, int nInserted, int nDeleted, int nRestyled, view::string_view deletedText, void *cbArg);
static void movedCB(Widget w, XtPointer clientData, XtPointer callData);
static void dragStartCB(Widget w, XtPointer clientData, XtPointer callData);
static void dragEndCB(Widget w, XtPointer clientData, XtPointer callData);
static void closeCB(Widget w, XtPointer clientData, XtPointer callData);
static void saveYourselfCB(Widget w, XtPointer clientData, XtPointer callData);
static void setPaneMinHeight(Widget w, int min);
static void addWindowIcon(Widget shell);
static void wmSizeUpdateProc(XtPointer clientData, XtIntervalId *id);
static void getGeometryString(Document *window, char *geomString);
static void cloneTextPanes(Document *window, Document *orgWin);
static std::list<UndoInfo *> cloneUndoItems(const std::list<UndoInfo *> &orgList);
static Widget containingPane(Widget w);
static void setPaneDesiredHeight(Widget w, int height);

/* From Xt, Shell.c, "BIGSIZE" */
static const Dimension XT_IGNORE_PPOSITION = 32767;



/*
** Redisplay menu tearoffs previously hid by hideTearOffs()
*/
static void redisplayTearOffs(Widget menuPane) {
	WidgetList itemList;
	Widget subMenuID;
	Cardinal nItems;
	int n;

	/* redisplay all submenu tearoffs */
	XtVaGetValues(menuPane, XmNchildren, &itemList, XmNnumChildren, &nItems, nullptr);
	for (n = 0; n < (int)nItems; n++) {
		if (XtClass(itemList[n]) == xmCascadeButtonWidgetClass) {
			XtVaGetValues(itemList[n], XmNsubMenuId, &subMenuID, nullptr);
			redisplayTearOffs(subMenuID);
		}
	}

	/* redisplay tearoff for this menu */
	if (!XmIsMenuShell(XtParent(menuPane)))
		ShowHiddenTearOff(menuPane);
}

/*
** Create a new editor window
*/
Document::Document(const char *name, char *geometry, bool iconic) {
	
	Widget winShell;
	Widget mainWin;
	Widget menuBar;
	Widget pane;
	Widget text;
	Widget stats;
	Widget statsAreaForm;
	Widget closeTabBtn;
	Widget tabForm;
	Pixel bgpix, fgpix;
	Arg al[20];
	int ac;
	XmString s1;
	XmFontList statsFontList;
	Document *win;
	char newGeometry[MAX_GEOM_STRING_LEN];
	unsigned int rows, cols;
	int x = 0, y = 0, bitmask, showTabBar, state;

	static Pixmap isrcFind = 0;
	static Pixmap isrcClear = 0;
	static Pixmap closeTabPixmap = 0;

	/* initialize window structure */
	/* + Schwarzenberg: should a
	  memset(window, 0, sizeof(Document));
	     be added here ?
	*/
	this->replaceDlog = nullptr;
	this->replaceText = nullptr;
	this->replaceWithText = nullptr;
	this->replaceWordToggle = nullptr;
	this->replaceCaseToggle = nullptr;
	this->replaceRegexToggle = nullptr;
	this->findDlog = nullptr;
	this->findText = nullptr;
	this->findWordToggle = nullptr;
	this->findCaseToggle = nullptr;
	this->findRegexToggle = nullptr;
	this->replaceMultiFileDlog = nullptr;
	this->replaceMultiFilePathBtn = nullptr;
	this->replaceMultiFileList = nullptr;
	this->multiFileReplSelected = FALSE;
	this->multiFileBusy = FALSE;
	this->writableWindows = nullptr;
	this->nWritableWindows = 0;
	this->fileChanged = FALSE;
	this->fileMode = 0;
	this->fileUid = 0;
	this->fileGid = 0;
	this->filenameSet = FALSE;
	this->fileFormat = UNIX_FILE_FORMAT;
	this->lastModTime = 0;
	this->fileMissing = True;
	strcpy(this->filename, name);
	this->undo = std::list<UndoInfo *>();
	this->redo = std::list<UndoInfo *>();
	this->nPanes = 0;
	this->autoSaveCharCount = 0;
	this->autoSaveOpCount = 0;
	this->undoMemUsed = 0;
	CLEAR_ALL_LOCKS(this->lockReasons);
	this->indentStyle = GetPrefAutoIndent(PLAIN_LANGUAGE_MODE);
	this->autoSave = GetPrefAutoSave();
	this->saveOldVersion = GetPrefSaveOldVersion();
	this->wrapMode = GetPrefWrap(PLAIN_LANGUAGE_MODE);
	this->overstrike = False;
	this->showMatchingStyle = GetPrefShowMatching();
	this->matchSyntaxBased = GetPrefMatchSyntaxBased();
	this->showStats = GetPrefStatsLine();
	this->showISearchLine = GetPrefISearchLine();
	this->showLineNumbers = GetPrefLineNums();
	this->highlightSyntax = GetPrefHighlightSyntax();
	this->backlightCharTypes = nullptr;
	this->backlightChars = GetPrefBacklightChars();
	if (this->backlightChars) {
		const char *cTypes = GetPrefBacklightCharTypes();
		if (cTypes && this->backlightChars) {
			this->backlightCharTypes = XtStringDup(cTypes);
		}
	}
	this->modeMessageDisplayed = FALSE;
	this->modeMessage = nullptr;
	this->ignoreModify = FALSE;
	this->windowMenuValid = FALSE;
	this->flashTimeoutID = 0;
	this->fileClosedAtom = None;
	this->wasSelected = FALSE;

	strcpy(this->fontName, GetPrefFontName());
	strcpy(this->italicFontName, GetPrefItalicFontName());
	strcpy(this->boldFontName, GetPrefBoldFontName());
	strcpy(this->boldItalicFontName, GetPrefBoldItalicFontName());
	this->colorDialog = nullptr;
	this->fontList = GetPrefFontList();
	this->italicFontStruct = GetPrefItalicFont();
	this->boldFontStruct = GetPrefBoldFont();
	this->boldItalicFontStruct = GetPrefBoldItalicFont();
	this->fontDialog = nullptr;
	this->nMarks = 0;
	this->markTimeoutID = 0;
	this->highlightData = nullptr;
	this->shellCmdData = nullptr;
	this->macroCmdData = nullptr;
	this->smartIndentData = nullptr;
	this->languageMode = PLAIN_LANGUAGE_MODE;
	this->iSearchHistIndex = 0;
	this->iSearchStartPos = -1;
	this->replaceLastRegexCase = TRUE;
	this->replaceLastLiteralCase = FALSE;
	this->iSearchLastRegexCase = TRUE;
	this->iSearchLastLiteralCase = FALSE;
	this->findLastRegexCase = TRUE;
	this->findLastLiteralCase = FALSE;
	this->tab = nullptr;
	this->device = 0;
	this->inode = 0;

	/* If window geometry was specified, split it apart into a window position
	   component and a window size component.  Create a new geometry string
	   containing the position component only.  Rows and cols are stripped off
	   because we can't easily calculate the size in pixels from them until the
	   whole window is put together.  Note that the preference resource is only
	   for clueless users who decide to specify the standard X geometry
	   application resource, which is pretty useless because width and height
	   are the same as the rows and cols preferences, and specifying a window
	   location will force all the windows to pile on top of one another */
	if (geometry == nullptr || geometry[0] == '\0')
		geometry = GetPrefGeometry();
	if (geometry == nullptr || geometry[0] == '\0') {
		rows = GetPrefRows();
		cols = GetPrefCols();
		newGeometry[0] = '\0';
	} else {
		bitmask = XParseGeometry(geometry, &x, &y, &cols, &rows);
		if (bitmask == 0)
			fprintf(stderr, "Bad window geometry specified: %s\n", geometry);
		else {
			if (!(bitmask & WidthValue))
				cols = GetPrefCols();
			if (!(bitmask & HeightValue))
				rows = GetPrefRows();
		}
		CreateGeometryString(newGeometry, x, y, 0, 0, bitmask & ~(WidthValue | HeightValue));
	}

	/* Create a new toplevel shell to hold the window */
	ac = 0;
	XtSetArg(al[ac], XmNtitle, name);
	ac++;
	XtSetArg(al[ac], XmNdeleteResponse, XmDO_NOTHING);
	ac++;
	XtSetArg(al[ac], XmNiconName, name);
	ac++;
	XtSetArg(al[ac], XmNgeometry, newGeometry[0] == '\0' ? nullptr : newGeometry);
	ac++;
	XtSetArg(al[ac], XmNinitialState, iconic ? IconicState : NormalState);
	ac++;

	if (newGeometry[0] == '\0') {
		/* Workaround to make Xt ignore Motif's bad PPosition size changes. Even
		   though we try to remove the PPosition in RealizeWithoutForcingPosition,
		   it is not sufficient.  Motif will recompute the size hints some point
		   later and put PPosition back! If the window is mapped after that time,
		   then the window will again wind up at 0, 0.  So, XEmacs does this, and
		   now we do.

		   Alternate approach, relying on ShellP.h:

		   ((WMShellWidget)winShell)->shell.client_specified &= ~_XtShellPPositionOK;
		 */

		XtSetArg(al[ac], XtNx, XT_IGNORE_PPOSITION);
		ac++;
		XtSetArg(al[ac], XtNy, XT_IGNORE_PPOSITION);
		ac++;
	}

	winShell = CreateWidget(TheAppShell, "textShell", topLevelShellWidgetClass, al, ac);
	this->shell = winShell;

#ifdef EDITRES
	XtAddEventHandler(winShell, (EventMask)0, True, (XtEventHandler)_XEditResCheckMessages, nullptr);
#endif /* EDITRES */

	addWindowIcon(winShell);

	/* Create a MainWindow to manage the menubar and text area, set the
	   userData resource to be used by WidgetToWindow to recover the
	   window pointer from the widget id of any of the window's widgets */
	XtSetArg(al[ac], XmNuserData, this);
	ac++;
	mainWin = XmCreateMainWindow(winShell, (String) "main", al, ac);
	this->mainWin = mainWin;
	XtManageChild(mainWin);

	/* The statsAreaForm holds the stats line and the I-Search line. */
	statsAreaForm = XtVaCreateWidget("statsAreaForm", xmFormWidgetClass, mainWin, XmNmarginWidth, STAT_SHADOW_THICKNESS, XmNmarginHeight, STAT_SHADOW_THICKNESS,
	                                 /* XmNautoUnmanage, False, */
	                                 nullptr);

	/* NOTE: due to a bug in openmotif 2.1.30, NEdit used to crash when
	   the i-search bar was active, and the i-search text widget was focussed,
	   and the window's width was resized to nearly zero.
	   In theory, it is possible to avoid this by imposing a minimum
	   width constraint on the nedit windows, but that width would have to
	   be at least 30 characters, which is probably unacceptable.
	   Amazingly, adding a top offset of 1 pixel to the toggle buttons of
	   the i-search bar, while keeping the the top offset of the text widget
	   to 0 seems to avoid avoid the crash. */

	this->iSearchForm = XtVaCreateWidget("iSearchForm", xmFormWidgetClass, statsAreaForm, XmNshadowThickness, 0, XmNleftAttachment, XmATTACH_FORM, XmNleftOffset, STAT_SHADOW_THICKNESS, XmNtopAttachment, XmATTACH_FORM, XmNtopOffset,
	                                       STAT_SHADOW_THICKNESS, XmNrightAttachment, XmATTACH_FORM, XmNrightOffset, STAT_SHADOW_THICKNESS, XmNbottomOffset, STAT_SHADOW_THICKNESS, nullptr);
	if (this->showISearchLine)
		XtManageChild(this->iSearchForm);

	/* Disable keyboard traversal of the find, clear and toggle buttons.  We
	   were doing this previously by forcing the keyboard focus back to the
	   text widget whenever a toggle changed.  That causes an ugly focus flash
	   on screen.  It's better just not to go there in the first place.
	   Plus, if the user really wants traversal, it's an X resource so it
	   can be enabled without too much pain and suffering. */

	if (isrcFind == 0) {
		isrcFind = createBitmapWithDepth(this->iSearchForm, (char *)isrcFind_bits, isrcFind_width, isrcFind_height);
	}
	this->iSearchFindButton = XtVaCreateManagedWidget("iSearchFindButton", xmPushButtonWidgetClass, this->iSearchForm, XmNlabelString, s1 = XmStringCreateSimpleEx("Find"), XmNlabelType, XmPIXMAP, XmNlabelPixmap, isrcFind,
	                                                    XmNtraversalOn, False, XmNmarginHeight, 1, XmNmarginWidth, 1, XmNleftAttachment, XmATTACH_FORM,
	                                                    /* XmNleftOffset, 3, */
	                                                    XmNleftOffset, 0, XmNtopAttachment, XmATTACH_FORM, XmNtopOffset, 1, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, 1, nullptr);
	XmStringFree(s1);

	this->iSearchCaseToggle = XtVaCreateManagedWidget("iSearchCaseToggle", xmToggleButtonWidgetClass, this->iSearchForm, XmNlabelString, s1 = XmStringCreateSimpleEx("Case"), XmNset,
	                                                    GetPrefSearch() == SEARCH_CASE_SENSE || GetPrefSearch() == SEARCH_REGEX || GetPrefSearch() == SEARCH_CASE_SENSE_WORD, XmNtopAttachment, XmATTACH_FORM, XmNbottomAttachment,
	                                                    XmATTACH_FORM, XmNtopOffset, 1, /* see openmotif note above */
	                                                    XmNrightAttachment, XmATTACH_FORM, XmNmarginHeight, 0, XmNtraversalOn, False, nullptr);
	XmStringFree(s1);

	this->iSearchRegexToggle =
	    XtVaCreateManagedWidget("iSearchREToggle", xmToggleButtonWidgetClass, this->iSearchForm, XmNlabelString, s1 = XmStringCreateSimpleEx("RegExp"), XmNset, GetPrefSearch() == SEARCH_REGEX_NOCASE || GetPrefSearch() == SEARCH_REGEX,
	                            XmNtopAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, XmNtopOffset, 1, /* see openmotif note above */
	                            XmNrightAttachment, XmATTACH_WIDGET, XmNrightWidget, this->iSearchCaseToggle, XmNmarginHeight, 0, XmNtraversalOn, False, nullptr);
	XmStringFree(s1);

	this->iSearchRevToggle = XtVaCreateManagedWidget("iSearchRevToggle", xmToggleButtonWidgetClass, this->iSearchForm, XmNlabelString, s1 = XmStringCreateSimpleEx("Rev"), XmNset, False, XmNtopAttachment, XmATTACH_FORM,
	                                                   XmNbottomAttachment, XmATTACH_FORM, XmNtopOffset, 1, /* see openmotif note above */
	                                                   XmNrightAttachment, XmATTACH_WIDGET, XmNrightWidget, this->iSearchRegexToggle, XmNmarginHeight, 0, XmNtraversalOn, False, nullptr);
	XmStringFree(s1);

	if (isrcClear == 0) {
		isrcClear = createBitmapWithDepth(this->iSearchForm, (char *)isrcClear_bits, isrcClear_width, isrcClear_height);
	}
	this->iSearchClearButton = XtVaCreateManagedWidget("iSearchClearButton", xmPushButtonWidgetClass, this->iSearchForm, XmNlabelString, s1 = XmStringCreateSimpleEx("<x"), XmNlabelType, XmPIXMAP, XmNlabelPixmap, isrcClear,
	                                                     XmNtraversalOn, False, XmNmarginHeight, 1, XmNmarginWidth, 1, XmNrightAttachment, XmATTACH_WIDGET, XmNrightWidget, this->iSearchRevToggle, XmNrightOffset, 2, XmNtopAttachment,
	                                                     XmATTACH_FORM, XmNtopOffset, 1, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, 1, nullptr);
	XmStringFree(s1);

	this->iSearchText = XtVaCreateManagedWidget("iSearchText", xmTextWidgetClass, this->iSearchForm, XmNmarginHeight, 1, XmNnavigationType, XmEXCLUSIVE_TAB_GROUP, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget,
	                                              this->iSearchFindButton, XmNrightAttachment, XmATTACH_WIDGET, XmNrightWidget, this->iSearchClearButton,
	                                              /* XmNrightOffset, 5, */
	                                              XmNtopAttachment, XmATTACH_FORM, XmNtopOffset, 0, /* see openmotif note above */
	                                              XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, 0, nullptr);
	RemapDeleteKey(this->iSearchText);

	SetISearchTextCallbacks(this);

	/* create the a form to house the tab bar and close-tab button */
	tabForm = XtVaCreateWidget("tabForm", xmFormWidgetClass, statsAreaForm, XmNmarginHeight, 0, XmNmarginWidth, 0, XmNspacing, 0, XmNresizable, False, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, XmNshadowThickness,
	                           0, nullptr);

	/* button to close top document */
	if (closeTabPixmap == 0) {
		closeTabPixmap = createBitmapWithDepth(tabForm, (char *)close_bits, close_width, close_height);
	}
	closeTabBtn = XtVaCreateManagedWidget("closeTabBtn", xmPushButtonWidgetClass, tabForm, XmNmarginHeight, 0, XmNmarginWidth, 0, XmNhighlightThickness, 0, XmNlabelType, XmPIXMAP, XmNlabelPixmap, closeTabPixmap, XmNshadowThickness, 1,
	                                      XmNtraversalOn, False, XmNrightAttachment, XmATTACH_FORM, XmNrightOffset, 3, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, 3, nullptr);
	XtAddCallback(closeTabBtn, XmNactivateCallback, closeTabCB, mainWin);

	/* create the tab bar */
	this->tabBar = XtVaCreateManagedWidget("tabBar", xmlFolderWidgetClass, tabForm, XmNresizePolicy, XmRESIZE_PACK, XmNleftAttachment, XmATTACH_FORM, XmNleftOffset, 0, XmNrightAttachment, XmATTACH_WIDGET, XmNrightWidget, closeTabBtn,
	                                         XmNrightOffset, 5, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, 0, XmNtopAttachment, XmATTACH_FORM, nullptr);

	this->tabMenuPane = CreateTabContextMenu(this->tabBar, this);
	AddTabContextMenuAction(this->tabBar);

	/* create an unmanaged composite widget to get the folder
	   widget to hide the 3D shadow for the manager area.
	   Note: this works only on the patched XmLFolder widget */
	Widget form = XtVaCreateWidget("form", xmFormWidgetClass, this->tabBar, XmNheight, 1, XmNresizable, False, nullptr);

	(void)form;

	XtAddCallback(this->tabBar, XmNactivateCallback, raiseTabCB, nullptr);

	this->tab = addTab(this->tabBar, name);

	/* A form to hold the stats line text and line/col widgets */
	this->statsLineForm = XtVaCreateWidget("statsLineForm", xmFormWidgetClass, statsAreaForm, XmNshadowThickness, 0, XmNtopAttachment, this->showISearchLine ? XmATTACH_WIDGET : XmATTACH_FORM, XmNtopWidget, this->iSearchForm,
	                                         XmNrightAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, XmNresizable, False, /*  */
	                                         nullptr);

	/* A separate display of the line/column number */
	this->statsLineColNo = XtVaCreateManagedWidget("statsLineColNo", xmLabelWidgetClass, this->statsLineForm, XmNlabelString, s1 = XmStringCreateSimpleEx("L: ---  C: ---"), XmNshadowThickness, 0, XmNmarginHeight, 2, XmNtraversalOn,
	                                                 False, XmNtopAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, /*  */
	                                                 nullptr);
	XmStringFree(s1);

	/* Create file statistics display area.  Using a text widget rather than
	   a label solves a layout problem with the main window, which messes up
	   if the label is too long (we would need a resize callback to control
	   the length when the window changed size), and allows users to select
	   file names and line numbers.  Colors are copied from parent
	   widget, because many users and some system defaults color text
	   backgrounds differently from other widgets. */
	XtVaGetValues(this->statsLineForm, XmNbackground, &bgpix, nullptr);
	XtVaGetValues(this->statsLineForm, XmNforeground, &fgpix, nullptr);
	stats = XtVaCreateManagedWidget("statsLine", xmTextWidgetClass, this->statsLineForm, XmNbackground, bgpix, XmNforeground, fgpix, XmNshadowThickness, 0, XmNhighlightColor, bgpix, XmNhighlightThickness,
	                                0,                  /* must be zero, for OM (2.1.30) to
	                                                   aligns tatsLineColNo & statsLine */
	                                XmNmarginHeight, 1, /* == statsLineColNo.marginHeight - 1,
	                                                   to align with statsLineColNo */
	                                XmNscrollHorizontal, False, XmNeditMode, XmSINGLE_LINE_EDIT, XmNeditable, False, XmNtraversalOn, False, XmNcursorPositionVisible, False, XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,                /*  */
	                                XmNtopWidget, this->statsLineColNo, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_WIDGET, XmNrightWidget, this->statsLineColNo, XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET, /*  */
	                                XmNbottomWidget, this->statsLineColNo, XmNrightOffset, 3, nullptr);
	this->statsLine = stats;

	/* Give the statsLine the same font as the statsLineColNo */
	XtVaGetValues(this->statsLineColNo, XmNfontList, &statsFontList, nullptr);
	XtVaSetValues(this->statsLine, XmNfontList, statsFontList, nullptr);

	/* Manage the statsLineForm */
	if (this->showStats)
		XtManageChild(this->statsLineForm);

	/* If the fontList was nullptr, use the magical default provided by Motif,
	   since it must have worked if we've gotten this far */
	if (!this->fontList)
		XtVaGetValues(stats, XmNfontList, &this->fontList, nullptr);

	/* Create the menu bar */
	menuBar = CreateMenuBar(mainWin, this);
	this->menuBar = menuBar;
	XtManageChild(menuBar);

	/* Create paned window to manage split pane behavior */
	pane = XtVaCreateManagedWidget("pane", xmPanedWindowWidgetClass, mainWin, XmNseparatorOn, False, XmNspacing, 3, XmNsashIndent, -2, nullptr);
	this->splitPane = pane;
	XmMainWindowSetAreas(mainWin, menuBar, statsAreaForm, nullptr, nullptr, pane);

	/* Store a copy of document/window pointer in text pane to support
	   action procedures. See also WidgetToWindow() for info. */
	XtVaSetValues(pane, XmNuserData, this, nullptr);

	/* Patch around Motif's most idiotic "feature", that its menu accelerators
	   recognize Caps Lock and Num Lock as modifiers, and don't trigger if
	   they are engaged */
	AccelLockBugPatch(pane, this->menuBar);

	/* Create the first, and most permanent text area (other panes may
	   be added & removed, but this one will never be removed */
	text = createTextArea(pane, this, rows, cols, GetPrefEmTabDist(PLAIN_LANGUAGE_MODE), GetPrefDelimiters(), GetPrefWrapMargin(), this->showLineNumbers ? MIN_LINE_NUM_COLS : 0);
	XtManageChild(text);
	this->textArea = text;
	this->lastFocus = text;

	/* Set the initial colors from the globals. */
	this->SetColors(GetPrefColorName(TEXT_FG_COLOR), GetPrefColorName(TEXT_BG_COLOR), GetPrefColorName(SELECT_FG_COLOR), GetPrefColorName(SELECT_BG_COLOR), GetPrefColorName(HILITE_FG_COLOR), GetPrefColorName(HILITE_BG_COLOR),
	          GetPrefColorName(LINENO_FG_COLOR), GetPrefColorName(CURSOR_FG_COLOR));

	/* Create the right button popup menu (note: order is important here,
	   since the translation for popping up this menu was probably already
	   added in createTextArea, but CreateBGMenu requires this->textArea
	   to be set so it can attach the menu to it (because menu shells are
	   finicky about the kinds of widgets they are attached to)) */
	this->bgMenuPane = CreateBGMenu(this);

	/* cache user menus: init. user background menu cache */
	InitUserBGMenuCache(&this->userBGMenuCache);

	/* Create the text buffer rather than using the one created automatically
	   with the text area widget.  This is done so the syntax highlighting
	   modify callback can be called to synchronize the style buffer BEFORE
	   the text display's callback is called upon to display a modification */
	this->buffer = new TextBuffer;
	this->buffer->BufAddModifyCB(SyntaxHighlightModifyCB, this);

	/* Attach the buffer to the text widget, and add callbacks for modify */
	TextSetBuffer(text, this->buffer);
	this->buffer->BufAddModifyCB(modifiedCB, this);

	/* Designate the permanent text area as the owner for selections */
	HandleXSelections(text);

	/* Set the requested hardware tab distance and useTabs in the text buffer */
	this->buffer->BufSetTabDistance(GetPrefTabDist(PLAIN_LANGUAGE_MODE));
	this->buffer->useTabs_ = GetPrefInsertTabs();

	/* add the window to the global window list, update the Windows menus */
	this->addToWindowList();
	InvalidateWindowMenus();

	showTabBar = this->GetShowTabBar();
	if (showTabBar)
		XtManageChild(tabForm);

	manageToolBars(statsAreaForm);

	if (showTabBar || this->showISearchLine || this->showStats)
		XtManageChild(statsAreaForm);

	/* realize all of the widgets in the new window */
	RealizeWithoutForcingPosition(winShell);
	XmProcessTraversal(text, XmTRAVERSE_CURRENT);

	/* Make close command in window menu gracefully prompt for close */
	AddMotifCloseCallback(winShell, closeCB, this);

	/* Make window resizing work in nice character heights */
	this->UpdateWMSizeHints();

	/* Set the minimum pane height for the initial text pane */
	this->UpdateMinPaneHeights();

	/* create dialogs shared by all documents in a window */
	CreateFindDlog(this->shell, this);
	CreateReplaceDlog(this->shell, this);
	CreateReplaceMultiFileDlog(this);

	/* dim/undim Attach_Tab menu items */
	state = this->NDocuments() < NWindows();
	for (win = WindowList; win; win = win->next) {
		if (win->IsTopDocument()) {
			XtSetSensitive(win->moveDocumentItem, state);
			XtSetSensitive(win->contextMoveDocumentItem, state);
		}
	}
}

/*
** Raise a tabbed document within its shell window.
**
** NB: use RaiseDocumentWindow() to raise the doc and
**     its shell window.
*/
void Document::RaiseDocument() {
	Document *win, *lastwin;

	if (!this || !WindowList)
		return;

	lastwin = this->MarkActiveDocument();
	if (lastwin != this && lastwin->IsValidWindow())
		lastwin->MarkLastDocument();

	/* document already on top? */
	XtVaGetValues(this->mainWin, XmNuserData, &win, nullptr);
	if (win == this)
		return;

	/* set the document as top document */
	XtVaSetValues(this->mainWin, XmNuserData, this, nullptr);

	/* show the new top document */
	XtVaSetValues(this->mainWin, XmNworkWindow, this->splitPane, nullptr);
	XtManageChild(this->splitPane);
	XRaiseWindow(TheDisplay, XtWindow(this->splitPane));

	/* Turn on syntax highlight that might have been deferred.
	   NB: this must be done after setting the document as
	       XmNworkWindow and managed, else the parent shell
	   this may shrink on some this-managers such as
	   metacity, due to changes made in UpdateWMSizeHints().*/
	if (this->highlightSyntax && this->highlightData == nullptr)
		StartHighlighting(this, False);

	/* put away the bg menu tearoffs of last active document */
	hideTearOffs(win->bgMenuPane);

	/* restore the bg menu tearoffs of active document */
	redisplayTearOffs(this->bgMenuPane);

	/* set tab as active */
	XmLFolderSetActiveTab(this->tabBar, getTabPosition(this->tab), False);

	/* set keyboard focus. Must be done before unmanaging previous
	   top document, else lastFocus will be reset to textArea */
	XmProcessTraversal(this->lastFocus, XmTRAVERSE_CURRENT);

	/* we only manage the top document, else the next time a document
	   is raised again, it's textpane might not resize properly.
	   Also, somehow (bug?) XtUnmanageChild() doesn't hide the
	   splitPane, which obscure lower part of the statsform when
	   we toggle its components, so we need to put the document at
	   the back */
	XLowerWindow(TheDisplay, XtWindow(win->splitPane));
	XtUnmanageChild(win->splitPane);
	win->RefreshTabState();

	/* now refresh this state/info. RefreshWindowStates()
	   has a lot of work to do, so we update the screen first so
	   the document appears to switch swiftly. */
	XmUpdateDisplay(this->splitPane);
	this->RefreshWindowStates();
	this->RefreshTabState();

	/* put away the bg menu tearoffs of last active document */
	hideTearOffs(win->bgMenuPane);

	/* restore the bg menu tearoffs of active document */
	redisplayTearOffs(this->bgMenuPane);

	/* Make sure that the "In Selection" button tracks the presence of a
	   selection and that the this inherits the proper search scope. */
	if (this->replaceDlog != nullptr && XtIsManaged(this->replaceDlog)) {
#ifdef REPLACE_SCOPE
		this->replaceScope = win->replaceScope;
#endif
		UpdateReplaceActionButtons(this);
	}

	this->UpdateWMSizeHints();
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
** Add another independently scrollable pane to the current document,
** splitting the pane which currently has keyboard focus.
*/
void Document::SplitPane() {
	short paneHeights[MAX_PANES + 1];
	int insertPositions[MAX_PANES + 1], topLines[MAX_PANES + 1];
	int horizOffsets[MAX_PANES + 1];
	int i, focusPane, emTabDist, wrapMargin, lineNumCols, totalHeight = 0;
	char *delimiters;
	Widget text = nullptr;
	textDisp *textD, *newTextD;

	/* Don't create new panes if we're already at the limit */
	if (this->nPanes >= MAX_PANES)
		return;

	/* Record the current heights, scroll positions, and insert positions
	   of the existing panes, keyboard focus */
	focusPane = 0;
	for (i = 0; i <= this->nPanes; i++) {
		text = i == 0 ? this->textArea : this->textPanes[i - 1];
		insertPositions[i] = TextGetCursorPos(text);
		XtVaGetValues(containingPane(text), XmNheight, &paneHeights[i], nullptr);
		totalHeight += paneHeights[i];
		TextGetScroll(text, &topLines[i], &horizOffsets[i]);
		if (text == this->lastFocus)
			focusPane = i;
	}

	/* Unmanage & remanage the panedWindow so it recalculates pane heights */
	XtUnmanageChild(this->splitPane);

	/* Create a text widget to add to the pane and set its buffer and
	   highlight data to be the same as the other panes in the document */
	XtVaGetValues(this->textArea, textNemulateTabs, &emTabDist, textNwordDelimiters, &delimiters, textNwrapMargin, &wrapMargin, textNlineNumCols, &lineNumCols, nullptr);
	text = createTextArea(this->splitPane, this, 1, 1, emTabDist, delimiters, wrapMargin, lineNumCols);

	TextSetBuffer(text, this->buffer);
	if (this->highlightData)
		AttachHighlightToWidget(text, this);
	if (this->backlightChars) {
		XtVaSetValues(text, textNbacklightCharTypes, this->backlightCharTypes, nullptr);
	}
	XtManageChild(text);
	this->textPanes[this->nPanes++] = text;

	/* Fix up the colors */
	textD = ((TextWidget)this->textArea)->text.textD;
	newTextD = reinterpret_cast<TextWidget>(text)->text.textD;
	XtVaSetValues(text, XmNforeground, textD->fgPixel, XmNbackground, textD->bgPixel, nullptr);
	newTextD->TextDSetColors(textD->fgPixel, textD->bgPixel, textD->selectFGPixel, textD->selectBGPixel, textD->highlightFGPixel, textD->highlightBGPixel, textD->lineNumFGPixel, textD->cursorFGPixel);

	/* Set the minimum pane height in the new pane */
	this->UpdateMinPaneHeights();

	/* adjust the heights, scroll positions, etc., to split the focus pane */
	for (i = this->nPanes; i > focusPane; i--) {
		insertPositions[i] = insertPositions[i - 1];
		paneHeights[i] = paneHeights[i - 1];
		topLines[i] = topLines[i - 1];
		horizOffsets[i] = horizOffsets[i - 1];
	}
	paneHeights[focusPane] = paneHeights[focusPane] / 2;
	paneHeights[focusPane + 1] = paneHeights[focusPane];

	for (i = 0; i <= this->nPanes; i++) {
		text = i == 0 ? this->textArea : this->textPanes[i - 1];
		setPaneDesiredHeight(containingPane(text), paneHeights[i]);
	}

	/* Re-manage panedWindow to recalculate pane heights & reset selection */
	if (this->IsTopDocument())
		XtManageChild(this->splitPane);

	/* Reset all of the heights, scroll positions, etc. */
	for (i = 0; i <= this->nPanes; i++) {
		text = i == 0 ? this->textArea : this->textPanes[i - 1];
		TextSetCursorPos(text, insertPositions[i]);
		TextSetScroll(text, topLines[i], horizOffsets[i]);
		setPaneDesiredHeight(containingPane(text), totalHeight / (this->nPanes + 1));
	}
	XmProcessTraversal(this->lastFocus, XmTRAVERSE_CURRENT);

	/* Update the this manager size hints after the sizes of the panes have
	   been set (the widget heights are not yet readable here, but they will
	   be by the time the event loop gets around to running this timer proc) */
	XtAppAddTimeOut(XtWidgetToApplicationContext(this->shell), 0, wmSizeUpdateProc, this);
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

static void closeCB(Widget w, XtPointer clientData, XtPointer callData) {

	auto window = static_cast<Document *>(clientData);
	
	window = WidgetToWindow(w);
	if (!WindowCanBeClosed(window)) {
		return;
	}

	CloseDocumentWindow(w, window, callData);
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
		getGeometryString(topWin, geometry);
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







/* Add an icon to an applicaction shell widget.  addWindowIcon adds a large
** (primary window) icon, AddSmallIcon adds a small (secondary window) icon.
**
** Note: I would prefer that these were not hardwired, but yhere is something
** weird about the  XmNiconPixmap resource that prevents it from being set
** from the defaults in the application resource database.
*/
static void addWindowIcon(Widget shell) {
	static Pixmap iconPixmap = 0, maskPixmap = 0;

	if (iconPixmap == 0) {
		iconPixmap = XCreateBitmapFromData(TheDisplay, RootWindowOfScreen(XtScreen(shell)), (char *)iconBits, iconBitmapWidth, iconBitmapHeight);
		maskPixmap = XCreateBitmapFromData(TheDisplay, RootWindowOfScreen(XtScreen(shell)), (char *)maskBits, iconBitmapWidth, iconBitmapHeight);
	}
	XtVaSetValues(shell, XmNiconPixmap, iconPixmap, XmNiconMask, maskPixmap, nullptr);
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
** Create pixmap per the widget's color depth setting.
**
** This fixes a BadMatch (X_CopyArea) error due to mismatching of
** color depth between the bitmap (depth of 1) and the screen,
** specifically on when linked to LessTif v1.2 (release 0.93.18
** & 0.93.94 tested).  LessTif v2.x showed no such problem.
*/
static Pixmap createBitmapWithDepth(Widget w, char *data, unsigned int width, unsigned int height) {
	Pixmap pixmap;
	Pixel fg, bg;
	int depth;

	XtVaGetValues(w, XmNforeground, &fg, XmNbackground, &bg, XmNdepth, &depth, nullptr);
	pixmap = XCreatePixmapFromBitmapData(XtDisplay(w), RootWindowOfScreen(XtScreen(w)), (char *)data, width, height, fg, bg, depth);

	return pixmap;
}

/*
** Save the position and size of a window as an X standard geometry string.
** A string of at least MAX_GEOMETRY_STRING_LEN characters should be
** provided in the argument "geomString" to receive the result.
*/
static void getGeometryString(Document *window, char *geomString) {
	int x, y, fontWidth, fontHeight, baseWidth, baseHeight;
	unsigned int width, height, dummyW, dummyH, bw, depth, nChild;
	Window parent, root, *child, w = XtWindow(window->shell);
	Display *dpy = XtDisplay(window->shell);

	/* Find the width and height from the window of the shell */
	XGetGeometry(dpy, w, &root, &x, &y, &width, &height, &bw, &depth);

	/* Find the top left corner (x and y) of the window decorations.  (This
	   is what's required in the geometry string to restore the window to it's
	   original position, since the window manager re-parents the window to
	   add it's title bar and menus, and moves the requested window down and
	   to the left.)  The position is found by traversing the window hier-
	   archy back to the window to the last parent before the root window */
	for (;;) {
		XQueryTree(dpy, w, &root, &parent, &child, &nChild);
		XFree((char *)child);
		if (parent == root)
			break;
		w = parent;
	}
	XGetGeometry(dpy, w, &root, &x, &y, &dummyW, &dummyH, &bw, &depth);

	/* Use window manager size hints (set by UpdateWMSizeHints) to
	   translate the width and height into characters, as opposed to pixels */
	XtVaGetValues(window->shell, XmNwidthInc, &fontWidth, XmNheightInc, &fontHeight, XmNbaseWidth, &baseWidth, XmNbaseHeight, &baseHeight, nullptr);
	width = (width - baseWidth) / fontWidth;
	height = (height - baseHeight) / fontHeight;

	/* Write the string */
	CreateGeometryString(geomString, x, y, width, height, XValue | YValue | WidthValue | HeightValue);
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



/*
** return the integer position of a tab in the tabbar it
** belongs to, or -1 if there's an error, somehow.
*/
static int getTabPosition(Widget tab) {
	WidgetList tabList;
	int tabCount;
	Widget tabBar = XtParent(tab);

	XtVaGetValues(tabBar, XmNtabWidgetList, &tabList, XmNtabCount, &tabCount, nullptr);

	for (int i = 0; i < tabCount; i++) {
		if (tab == tabList[i])
			return i;
	}

	return -1; /* something is wrong! */
}



static void CloseDocumentWindow(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	
	auto window = static_cast<Document *>(clientData);

	int nDocuments = window->NDocuments();

	if (nDocuments == NWindows()) {
		/* this is only window, then exit */
		XtCallActionProc(WindowList->lastFocus, "exit", ((XmAnyCallbackStruct *)callData)->event, nullptr, 0);
	} else {
		if (nDocuments == 1) {
			CloseFileAndWindow(window, PROMPT_SBC_DIALOG_RESPONSE);
		} else {
			int resp = 1;
			if (GetPrefWarnExit())
				resp = DialogF(DF_QUES, window->shell, 2, "Close Window", "Close ALL documents in this window?", "Close", "Cancel");

			if (resp == 1)
				window->CloseAllDocumentInWindow();
		}
	}
}














/*
** hide all the tearoffs spawned from this menu.
** It works recursively to close the tearoffs of the submenus
*/
static void hideTearOffs(Widget menuPane) {
	WidgetList itemList;
	Widget subMenuID;
	Cardinal nItems;
	int n;

	/* hide all submenu tearoffs */
	XtVaGetValues(menuPane, XmNchildren, &itemList, XmNnumChildren, &nItems, nullptr);
	for (n = 0; n < (int)nItems; n++) {
		if (XtClass(itemList[n]) == xmCascadeButtonWidgetClass) {
			XtVaGetValues(itemList[n], XmNsubMenuId, &subMenuID, nullptr);
			hideTearOffs(subMenuID);
		}
	}

	/* hide tearoff for this menu */
	if (!XmIsMenuShell(XtParent(menuPane)))
		XtUnmapWidget(XtParent(menuPane));
}

Document *GetTopDocument(Widget w) {
	Document *window = WidgetToWindow(w);

	return WidgetToWindow(window->shell);
}





static void cloneTextPanes(Document *window, Document *orgWin) {
	short paneHeights[MAX_PANES + 1];
	int insertPositions[MAX_PANES + 1], topLines[MAX_PANES + 1];
	int horizOffsets[MAX_PANES + 1];
	int i, focusPane, emTabDist, wrapMargin, lineNumCols, totalHeight = 0;
	char *delimiters;
	Widget text;
	TextSelection sel;
	textDisp *textD, *newTextD;

	/* transfer the primary selection */
	memcpy(&sel, &orgWin->buffer->primary_, sizeof(TextSelection));

	if (sel.selected) {
		if (sel.rectangular)
			window->buffer->BufRectSelect(sel.start, sel.end, sel.rectStart, sel.rectEnd);
		else
			window->buffer->BufSelect(sel.start, sel.end);
	} else
		window->buffer->BufUnselect();

	/* Record the current heights, scroll positions, and insert positions
	   of the existing panes, keyboard focus */
	focusPane = 0;
	for (i = 0; i <= orgWin->nPanes; i++) {
		text = i == 0 ? orgWin->textArea : orgWin->textPanes[i - 1];
		insertPositions[i] = TextGetCursorPos(text);
		XtVaGetValues(containingPane(text), XmNheight, &paneHeights[i], nullptr);
		totalHeight += paneHeights[i];
		TextGetScroll(text, &topLines[i], &horizOffsets[i]);
		if (text == orgWin->lastFocus)
			focusPane = i;
	}

	window->nPanes = orgWin->nPanes;

	/* Copy some parameters */
	XtVaGetValues(orgWin->textArea, textNemulateTabs, &emTabDist, textNwordDelimiters, &delimiters, textNwrapMargin, &wrapMargin, nullptr);
	lineNumCols = orgWin->showLineNumbers ? MIN_LINE_NUM_COLS : 0;
	XtVaSetValues(window->textArea, textNemulateTabs, emTabDist, textNwordDelimiters, delimiters, textNwrapMargin, wrapMargin, textNlineNumCols, lineNumCols, nullptr);

	/* clone split panes, if any */
	textD = ((TextWidget)window->textArea)->text.textD;
	if (window->nPanes) {
		/* Unmanage & remanage the panedWindow so it recalculates pane heights */
		XtUnmanageChild(window->splitPane);

		/* Create a text widget to add to the pane and set its buffer and
		   highlight data to be the same as the other panes in the orgWin */

		for (i = 0; i < orgWin->nPanes; i++) {
			text = createTextArea(window->splitPane, window, 1, 1, emTabDist, delimiters, wrapMargin, lineNumCols);
			TextSetBuffer(text, window->buffer);

			if (window->highlightData)
				AttachHighlightToWidget(text, window);
			XtManageChild(text);
			window->textPanes[i] = text;

			/* Fix up the colors */
			newTextD = reinterpret_cast<TextWidget>(text)->text.textD;
			XtVaSetValues(text, XmNforeground, textD->fgPixel, XmNbackground, textD->bgPixel, nullptr);
			newTextD->TextDSetColors(textD->fgPixel, textD->bgPixel, textD->selectFGPixel, textD->selectBGPixel, textD->highlightFGPixel, textD->highlightBGPixel, textD->lineNumFGPixel, textD->cursorFGPixel);
		}

		/* Set the minimum pane height in the new pane */
		window->UpdateMinPaneHeights();

		for (i = 0; i <= window->nPanes; i++) {
			text = i == 0 ? window->textArea : window->textPanes[i - 1];
			setPaneDesiredHeight(containingPane(text), paneHeights[i]);
		}

		/* Re-manage panedWindow to recalculate pane heights & reset selection */
		XtManageChild(window->splitPane);
	}

	/* Reset all of the heights, scroll positions, etc. */
	for (i = 0; i <= window->nPanes; i++) {
		text = (i == 0) ? window->textArea : window->textPanes[i - 1];
		TextSetCursorPos(text, insertPositions[i]);
		TextSetScroll(text, topLines[i], horizOffsets[i]);

		/* dim the cursor */
		auto textD = reinterpret_cast<TextWidget>(text)->text.textD;
		textD->TextDSetCursorStyle(DIM_CURSOR);
		textD->TextDUnblankCursor();
	}

	/* set the focus pane */
	// NOTE(eteran): are we sure we want "<=" here? It's of course possible that
	//               it's correct, but it is certainly unconventional.
	//               Notice that is is used in the above loops as well
	for (i = 0; i <= window->nPanes; i++) {
		text = i == 0 ? window->textArea : window->textPanes[i - 1];
		if (i == focusPane) {
			window->lastFocus = text;
			XmProcessTraversal(text, XmTRAVERSE_CURRENT);
			break;
		}
	}

	/* Update the window manager size hints after the sizes of the panes have
	   been set (the widget heights are not yet readable here, but they will
	   be by the time the event loop gets around to running this timer proc) */
	XtAppAddTimeOut(XtWidgetToApplicationContext(window->shell), 0, wmSizeUpdateProc, window);
}

/*
** clone a document's states and settings into the other.
*/
void Document::cloneDocument(Document *window) {
	char *params[4];
	int emTabDist;

	strcpy(window->path,     this->path);
	strcpy(window->filename, this->filename);

	window->ShowLineNumbers(this->showLineNumbers);

	window->ignoreModify = True;

	/* copy the text buffer */
	auto orgDocument = this->buffer->BufAsStringEx();
	window->buffer->BufSetAllEx(orgDocument);

	/* copy the tab preferences (here!) */
	window->buffer->BufSetTabDistance(this->buffer->tabDist_);
	window->buffer->useTabs_ = this->buffer->useTabs_;
	XtVaGetValues(this->textArea, textNemulateTabs, &emTabDist, nullptr);
	window->SetEmTabDist(emTabDist);

	window->ignoreModify = False;

	/* transfer text fonts */
	params[0] = this->fontName;
	params[1] = this->italicFontName;
	params[2] = this->boldFontName;
	params[3] = this->boldItalicFontName;
	XtCallActionProc(window->textArea, "set_fonts", nullptr, params, 4);

	window->SetBacklightChars(this->backlightCharTypes);

	/* Clone rangeset info.

	   FIXME:
	   Cloning of rangesets must be done before syntax highlighting,
	   else the rangesets do not be highlighted (colored) properly
	   if syntax highlighting is on.
	*/
	if(this->buffer->rangesetTable_) {
		window->buffer->rangesetTable_ = new RangesetTable(window->buffer, *this->buffer->rangesetTable_);
	} else {
		window->buffer->rangesetTable_ = nullptr;
	}

	/* Syntax highlighting */
	window->languageMode = this->languageMode;
	window->highlightSyntax = this->highlightSyntax;
	if (window->highlightSyntax) {
		StartHighlighting(window, False);
	}

	/* copy states of original document */
	window->filenameSet = this->filenameSet;
	window->fileFormat = this->fileFormat;
	window->lastModTime = this->lastModTime;
	window->fileChanged = this->fileChanged;
	window->fileMissing = this->fileMissing;
	window->lockReasons = this->lockReasons;
	window->autoSaveCharCount = this->autoSaveCharCount;
	window->autoSaveOpCount = this->autoSaveOpCount;
	window->undoMemUsed = this->undoMemUsed;
	window->lockReasons = this->lockReasons;
	window->autoSave = this->autoSave;
	window->saveOldVersion = this->saveOldVersion;
	window->wrapMode = this->wrapMode;
	window->SetOverstrike(this->overstrike);
	window->showMatchingStyle = this->showMatchingStyle;
	window->matchSyntaxBased = this->matchSyntaxBased;
#if 0    
    window->showStats = this->showStats;
    window->showISearchLine = this->showISearchLine;
    window->showLineNumbers = this->showLineNumbers;
    window->modeMessageDisplayed = this->modeMessageDisplayed;
    window->ignoreModify = this->ignoreModify;
    window->windowMenuValid = this->windowMenuValid;
    window->flashTimeoutID = this->flashTimeoutID;
    window->wasSelected = this->wasSelected;
    strcpy(window->fontName, this->fontName);
    strcpy(window->italicFontName, this->italicFontName);
    strcpy(window->boldFontName, this->boldFontName);
    strcpy(window->boldItalicFontName, this->boldItalicFontName);
    window->fontList = this->fontList;
    window->italicFontStruct = this->italicFontStruct;
    window->boldFontStruct = this->boldFontStruct;
    window->boldItalicFontStruct = this->boldItalicFontStruct;
    window->markTimeoutID = this->markTimeoutID;
    window->highlightData = this->highlightData;
    window->shellCmdData = this->shellCmdData;
    window->macroCmdData = this->macroCmdData;
    window->smartIndentData = this->smartIndentData;
#endif
	window->iSearchHistIndex = this->iSearchHistIndex;
	window->iSearchStartPos = this->iSearchStartPos;
	window->replaceLastRegexCase = this->replaceLastRegexCase;
	window->replaceLastLiteralCase = this->replaceLastLiteralCase;
	window->iSearchLastRegexCase = this->iSearchLastRegexCase;
	window->iSearchLastLiteralCase = this->iSearchLastLiteralCase;
	window->findLastRegexCase = this->findLastRegexCase;
	window->findLastLiteralCase = this->findLastLiteralCase;
	window->device = this->device;
	window->inode = this->inode;
	window->fileClosedAtom = this->fileClosedAtom;
	this->fileClosedAtom = None;

	/* copy the text/split panes settings, cursor pos & selection */
	cloneTextPanes(window, this);

	/* copy undo & redo list */
	window->undo = cloneUndoItems(this->undo);
	window->redo = cloneUndoItems(this->redo);

	/* copy bookmarks */
	window->nMarks = this->nMarks;
	memcpy(&window->markTable, &this->markTable, sizeof(Bookmark) * window->nMarks);

	/* kick start the auto-indent engine */
	window->indentStyle = NO_AUTO_INDENT;
	window->SetAutoIndent(this->indentStyle);

	/* synchronize window state to this document */
	window->RefreshWindowStates();
}

static std::list<UndoInfo *> cloneUndoItems(const std::list<UndoInfo *> &orgList) {

	std::list<UndoInfo *> list;
	for(UndoInfo *undo : orgList) {
		list.push_back(new UndoInfo(*undo));
	}
	return list;
}





static void hideTooltip(Widget tab) {
	Widget tooltip = XtNameToWidget(tab, "*BubbleShell");

	if (tooltip)
		XtPopdown(tooltip);
}

static void closeTabProc(XtPointer clientData, XtIntervalId *id) {
	(void)id;
	CloseFileAndWindow(static_cast<Document *>(clientData), PROMPT_SBC_DIALOG_RESPONSE);
}

/*
** callback to close-tab button.
*/
static void closeTabCB(Widget w, XtPointer clientData, XtPointer callData) {

	auto mainWin = static_cast<Widget>(clientData);

	(void)callData;

	/* FIXME: XtRemoveActionHook() related coredump

	   An unknown bug seems to be associated with the XtRemoveActionHook()
	   call in FinishLearn(), which resulted in coredump if a tab was
	   closed, in the middle of keystrokes learning, by clicking on the
	   close-tab button.

	   As evident to our accusation, the coredump may be surpressed by
	   simply commenting out the XtRemoveActionHook() call. The bug was
	   consistent on both Motif and Lesstif on various platforms.

	   Closing the tab through either the "Close" menu or its accel key,
	   however, was without any trouble.

	   While its actual mechanism is not well understood, we somehow
	   managed to workaround the bug by delaying the action of closing
	   the tab. For now. */
	XtAppAddTimeOut(XtWidgetToApplicationContext(w), 0, closeTabProc, GetTopDocument(mainWin));
}

/*
** callback to clicks on a tab to raise it's document.
*/
static void raiseTabCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)clientData;

	XmLFolderCallbackStruct *cbs = (XmLFolderCallbackStruct *)callData;
	WidgetList tabList;
	Widget tab;

	XtVaGetValues(w, XmNtabWidgetList, &tabList, nullptr);
	tab = tabList[cbs->pos];
	TabToWindow(tab)->RaiseDocument();
}












/*
** perform generic management on the children (toolbars) of toolBarsForm,
** a.k.a. statsForm, by setting the form attachment of the managed child
** widgets per their position/order.
**
** You can optionally create separator after a toolbar widget with it's
** widget name set to "TOOLBAR_SEP", which will appear below the toolbar
** widget. These seperators will then be managed automatically by this
** routine along with the toolbars they 'attached' to.
**
** It also takes care of the attachment offset settings of the child
** widgets to keep the border lines of the parent form displayed, so
** you don't have set them before hand.
**
** Note: XtManage/XtUnmange the target child (toolbar) before calling this
**       function.
**
** Returns the last toolbar widget managed.
**
*/

// TODO(eteran): temporary duplicate
//----------------------------------------------------------------------------------------
static Widget manageToolBars(Widget toolBarsForm) {
	Widget topWidget = nullptr;
	WidgetList children;
	int n, nItems = 0;

	XtVaGetValues(toolBarsForm, XmNchildren, &children, XmNnumChildren, &nItems, nullptr);

	for (n = 0; n < nItems; n++) {
		Widget tbar = children[n];

		if (XtIsManaged(tbar)) {
			if (topWidget) {
				XtVaSetValues(tbar, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, topWidget, XmNbottomAttachment, XmATTACH_NONE, XmNleftOffset, STAT_SHADOW_THICKNESS, XmNrightOffset, STAT_SHADOW_THICKNESS, nullptr);
			} else {
				/* the very first toolbar on top */
				XtVaSetValues(tbar, XmNtopAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_NONE, XmNleftOffset, STAT_SHADOW_THICKNESS, XmNtopOffset, STAT_SHADOW_THICKNESS, XmNrightOffset, STAT_SHADOW_THICKNESS, nullptr);
			}

			topWidget = tbar;

			/* if the next widget is a separator, turn it on */
			if (n + 1 < nItems && !strcmp(XtName(children[n + 1]), "TOOLBAR_SEP")) {
				XtManageChild(children[n + 1]);
			}
		} else {
			/* Remove top attachment to widget to avoid circular dependency.
			   Attach bottom to form so that when the widget is redisplayed
			   later, it will trigger the parent form to resize properly as
			   if the widget is being inserted */
			XtVaSetValues(tbar, XmNtopAttachment, XmATTACH_NONE, XmNbottomAttachment, XmATTACH_FORM, nullptr);

			/* if the next widget is a separator, turn it off */
			if (n + 1 < nItems && !strcmp(XtName(children[n + 1]), "TOOLBAR_SEP")) {
				XtUnmanageChild(children[n + 1]);
			}
		}
	}

	if (topWidget) {
		if (strcmp(XtName(topWidget), "TOOLBAR_SEP")) {
			XtVaSetValues(topWidget, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, STAT_SHADOW_THICKNESS, nullptr);
		} else {
			/* is a separator */
			Widget wgt;
			XtVaGetValues(topWidget, XmNtopWidget, &wgt, nullptr);

			/* don't need sep below bottom-most toolbar */
			XtUnmanageChild(topWidget);
			XtVaSetValues(wgt, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, STAT_SHADOW_THICKNESS, nullptr);
		}
	}

	return topWidget;
}

Widget containingPane(Widget w) {
	/* The containing pane used to simply be the first parent, but with
	   the introduction of an XmFrame, it's the grandparent. */
	return XtParent(XtParent(w));
}

void setPaneDesiredHeight(Widget w, int height) {
	reinterpret_cast<XmPanedWindowConstraintPtr>(w->core.constraints)->panedw.dheight = height;
}

/*
** Xt timer procedure for updating size hints.  The new sizes of objects in
** the window are not ready immediately after adding or removing panes.  This
** is a timer routine to be invoked with a timeout of 0 to give the event
** loop a chance to finish processing the size changes before reading them
** out for setting the window manager size hints.
*/
static void wmSizeUpdateProc(XtPointer clientData, XtIntervalId *id) {

	(void)id;

	static_cast<Document *>(clientData)->UpdateWMSizeHints();
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
