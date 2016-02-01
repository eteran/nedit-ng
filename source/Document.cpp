
#include "Document.h"
#include "window.h" // There are a few global functions found here... for now
#include "preferences.h"
#include "userCmds.h"
#include "textDisp.h"
#include "text.h"
#include "textP.h"
#include "smartIndent.h"
#include "menu.h"
#include "windowTitle.h"
#include "highlight.h"
#include "file.h"

#include "../util/misc.h"
#include "../util/fileUtils.h"
#include "../Microline/XmL/Folder.h"
#include "../Xlt/BubbleButton.h"
#include "../util/clearcase.h"

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/Xatom.h>
#include <Xm/CascadeB.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/Label.h>
#include <Xm/List.h>
#include <Xm/MainW.h>
#include <Xm/PanedW.h>
#include <Xm/PanedWP.h>
#include <Xm/PrimitiveP.h>
#include <Xm/Protocols.h>
#include <Xm/PushB.h>
#include <Xm/RowColumnP.h>
#include <Xm/ScrollBar.h>
#include <Xm/ScrolledW.h>
#include <Xm/SelectioB.h>
#include <Xm/Separator.h>
#include <Xm/Text.h>
#include <Xm/ToggleB.h>
#include <Xm/Xm.h>



namespace {

/* Thickness of 3D border around statistics and/or incremental search areas
   below the main menu bar */
const int STAT_SHADOW_THICKNESS = 1;

Document *inFocusDocument  = nullptr;   /* where we are now */
Document *lastFocusDocument = nullptr; /* where we came from */


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
Widget manageToolBars(Widget toolBarsForm) {
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

/*
** Paned windows are impossible to adjust after they are created, which makes
** them nearly useless for NEdit (or any application which needs to dynamically
** adjust the panes) unless you tweek some private data to overwrite the
** desired and minimum pane heights which were set at creation time.  These
** will probably break in a future release of Motif because of dependence on
** private data.
*/
void setPaneDesiredHeight(Widget w, int height) {
	reinterpret_cast<XmPanedWindowConstraintPtr>(w->core.constraints)->panedw.dheight = height;
}

void setPaneMinHeight(Widget w, int min) {
	reinterpret_cast<XmPanedWindowConstraintPtr>(w->core.constraints)->panedw.min = min;
}

/*
**
*/
Widget containingPane(Widget w) {
	/* The containing pane used to simply be the first parent, but with
	   the introduction of an XmFrame, it's the grandparent. */
	return XtParent(XtParent(w));
}

}

/*
** set/clear menu sensitivity if the calling document is on top.
*/
void Document::SetSensitive(Widget w, Boolean sensitive) {
	if (this->IsTopDocument()) {
		XtSetSensitive(w, sensitive);
	}
}

/*
** set/clear toggle menu state if the calling document is on top.
*/
void Document::SetToggleButtonState(Widget w, Boolean state, Boolean notify) {
	if (this->IsTopDocument()) {
		XmToggleButtonSetState(w, state, notify);
	}
}

/*
** remember the last document.
*/
Document *Document::MarkLastDocument() {
	Document *prev = lastFocusDocument;

	if (this)
		lastFocusDocument = this;

	return prev;
}

/*
** remember the active (top) document.
*/
Document *Document::MarkActiveDocument() {
	Document *prev = inFocusDocument;

	if (this)
		inFocusDocument = this;

	return prev;
}

/*
** Bring up the last active window
*/
void Document::LastDocument() {
	Document *win;

	for (win = WindowList; win; win = win->next) {
		if (lastFocusDocument == win) {
			break;
		}
	}

	if (!win) {
		return;
	}

	if (this->shell == win->shell) {
		win->RaiseDocument();
	} else {
		win->RaiseFocusDocumentWindow(True);
	}
}

/*
** raise the document and its shell window and optionally focus.
*/
void Document::RaiseFocusDocumentWindow(Boolean focus) {
	if (!this)
		return;

	this->RaiseDocument();
	RaiseShellWindow(this->shell, focus);
}

/*
** 
*/
bool Document::IsTopDocument() const {
	return this == GetTopDocument(this->shell);
}

/*
** 
*/
Widget Document::GetPaneByIndex(int paneIndex) const {
	Widget text = nullptr;
	if (paneIndex >= 0 && paneIndex <= this->nPanes) {
		text = (paneIndex == 0) ? this->textArea : this->textPanes[paneIndex - 1];
	}
	return text;
}

/*
** make sure window is alive is kicking
*/
int Document::IsValidWindow() {

	for (Document *win = WindowList; win; win = win->next) {
		if (this == win) {
			return true;
		}
	}

	return false;
}

/*
** check if tab bar is to be shown on this window
*/
int Document::GetShowTabBar() {
	if (!GetPrefTabBar())
		return False;
	else if (this->NDocuments() == 1)
		return !GetPrefTabBarHideOne();
	else
		return True;
}

/*
** Returns true if window is iconic (as determined by the WM_STATE property
** on the shell window.  I think this is the most reliable way to tell,
** but if someone has a better idea please send me a note).
*/
int Document::IsIconic() {
	unsigned long *property = nullptr;
	unsigned long nItems;
	unsigned long leftover;
	static Atom wmStateAtom = 0;
	Atom actualType;
	int actualFormat;
	
	if (wmStateAtom == 0) {
		wmStateAtom = XInternAtom(XtDisplay(this->shell), "WM_STATE", False);
	}
		
	if (XGetWindowProperty(XtDisplay(this->shell), XtWindow(this->shell), wmStateAtom, 0L, 1L, False, wmStateAtom, &actualType, &actualFormat, &nItems, &leftover, (unsigned char **)&property) != Success || nItems != 1 || property == nullptr) {
		return FALSE;
	}

	int result = (*property == IconicState);
	XtFree((char *)property);
	return result;
}

/*
** return the next/previous docment on the tab list.
**
** If <wrap> is true then the next tab of the rightmost tab will be the
** second tab from the right, and the the previous tab of the leftmost
** tab will be the second from the left.  This is useful for getting
** the next tab after a tab detaches/closes and you don't want to wrap around.
*/
Document *Document::getNextTabWindow(int direction, int crossWin, int wrap) {
	WidgetList tabList;
	Document *win;
	int tabCount, tabTotalCount;
	int tabPos, nextPos;
	int i, n;
	int nBuf = crossWin ? NWindows() : this->NDocuments();

	if (nBuf <= 1)
		return nullptr;

	/* get the list of tabs */
	auto tabs = new Widget[nBuf];
	tabTotalCount = 0;
	if (crossWin) {
		int n, nItems;
		WidgetList children;

		XtVaGetValues(TheAppShell, XmNchildren, &children, XmNnumChildren, &nItems, nullptr);

		/* get list of tabs in all windows */
		for (n = 0; n < nItems; n++) {
			if (strcmp(XtName(children[n]), "textShell") || ((win = WidgetToWindow(children[n])) == nullptr))
				continue; /* skip non-text-editor windows */

			XtVaGetValues(win->tabBar, XmNtabWidgetList, &tabList, XmNtabCount, &tabCount, nullptr);

			for (i = 0; i < tabCount; i++) {
				tabs[tabTotalCount++] = tabList[i];
			}
		}
	} else {
		/* get list of tabs in this window */
		XtVaGetValues(this->tabBar, XmNtabWidgetList, &tabList, XmNtabCount, &tabCount, nullptr);

		for (i = 0; i < tabCount; i++) {
			if (TabToWindow(tabList[i])) /* make sure tab is valid */
				tabs[tabTotalCount++] = tabList[i];
		}
	}

	/* find the position of the tab in the tablist */
	tabPos = 0;
	for (n = 0; n < tabTotalCount; n++) {
		if (tabs[n] == this->tab) {
			tabPos = n;
			break;
		}
	}

	/* calculate index position of next tab */
	nextPos = tabPos + direction;
	if (nextPos >= nBuf) {
		if (wrap)
			nextPos = 0;
		else
			nextPos = nBuf - 2;
	} else if (nextPos < 0) {
		if (wrap)
			nextPos = nBuf - 1;
		else
			nextPos = 1;
	}

	/* return the document where the next tab belongs to */
	win = TabToWindow(tabs[nextPos]);
	delete [] tabs;
	return win;
}

/*
** Displays and undisplays the statistics line (regardless of settings of
** this->showStats or this->modeMessageDisplayed)
*/
void Document::showStatistics(int state) {
	if (state) {
		XtManageChild(this->statsLineForm);
		this->showStatsForm();
	} else {
		XtUnmanageChild(this->statsLineForm);
		this->showStatsForm();
	}

	/* Tell WM that the non-expandable part of the this has changed size */
	/* Already done in showStatsForm */
	/* this->UpdateWMSizeHints(); */
}

/*
** Put up or pop-down the incremental search line regardless of settings
** of showISearchLine or TempShowISearch
*/
void Document::showISearch(int state) {
	if (state) {
		XtManageChild(this->iSearchForm);
		this->showStatsForm();
	} else {
		XtUnmanageChild(this->iSearchForm);
		this->showStatsForm();
	}

	/* Tell WM that the non-expandable part of the this has changed size */
	/* This is already done in showStatsForm */
	/* this->UpdateWMSizeHints(); */
}

/*
** Show or hide the extra display area under the main menu bar which
** optionally contains the status line and the incremental search bar
*/
void Document::showStatsForm() {
	Widget statsAreaForm = XtParent(this->statsLineForm);
	Widget mainW = XtParent(statsAreaForm);

	/* The very silly use of XmNcommandWindowLocation and XmNshowSeparator
	   below are to kick the main this widget to position and remove the
	   status line when it is managed and unmanaged.  At some Motif version
	   level, the showSeparator trick backfires and leaves the separator
	   shown, but fortunately the dynamic behavior is fixed, too so the
	   workaround is no longer necessary, either.  (... the version where
	   this occurs may be earlier than 2.1.  If the stats line shows
	   double thickness shadows in earlier Motif versions, the #if XmVersion
	   directive should be moved back to that earlier version) */
	if (manageToolBars(statsAreaForm)) {
		XtUnmanageChild(statsAreaForm); /*... will this fix Solaris 7??? */
		XtVaSetValues(mainW, XmNcommandWindowLocation, XmCOMMAND_ABOVE_WORKSPACE, nullptr);
#if XmVersion < 2001
		XtVaSetValues(mainW, XmNshowSeparator, True, nullptr);
#endif
		XtManageChild(statsAreaForm);
		XtVaSetValues(mainW, XmNshowSeparator, False, nullptr);
		this->UpdateStatsLine();
	} else {
		XtUnmanageChild(statsAreaForm);
		XtVaSetValues(mainW, XmNcommandWindowLocation, XmCOMMAND_BELOW_WORKSPACE, nullptr);
	}

	/* Tell WM that the non-expandable part of the this has changed size */
	this->UpdateWMSizeHints();
}

/*
** Add a this to the the this list.
*/
void Document::addToWindowList() {

	Document *temp = WindowList;
	WindowList = this;
	this->next = temp;
}

/*
** Remove a this from the list of windows
*/
void Document::removeFromWindowList() {
	Document *temp;

	if (WindowList == this) {
		WindowList = this->next;
	} else {
		for (temp = WindowList; temp != nullptr; temp = temp->next) {
			if (temp->next == this) {
				temp->next = this->next;
				break;
			}
		}
	}
}

/*
** Remove redundant expose events on tab bar.
*/
void Document::CleanUpTabBarExposeQueue() {
	XEvent event;
	XExposeEvent ev;
	int count;

	if(!this)
		return;

	/* remove redundant expose events on tab bar */
	count = 0;
	while (XCheckTypedWindowEvent(TheDisplay, XtWindow(this->tabBar), Expose, &event))
		count++;

	/* now we can update tabbar */
	if (count) {
		ev.type = Expose;
		ev.display = TheDisplay;
		ev.window = XtWindow(this->tabBar);
		ev.x = 0;
		ev.y = 0;
		ev.width = XtWidth(this->tabBar);
		ev.height = XtHeight(this->tabBar);
		ev.count = 0;
		XSendEvent(TheDisplay, XtWindow(this->tabBar), False, ExposureMask, (XEvent *)&ev);
	}
}

/*
** refresh window state for this document
*/
void Document::RefreshWindowStates() {
	if (!this->IsTopDocument())
		return;

	if (this->modeMessageDisplayed)
		XmTextSetStringEx(this->statsLine, this->modeMessage);
	else
		this->UpdateStatsLine();
		
	this->UpdateWindowReadOnly();
	this->UpdateWindowTitle();

	/* show/hide statsline as needed */
	if (this->modeMessageDisplayed && !XtIsManaged(this->statsLineForm)) {
		/* turn on statline to display mode message */
		this->showStatistics(True);
	} else if (this->showStats && !XtIsManaged(this->statsLineForm)) {
		/* turn on statsline since it is enabled */
		this->showStatistics(True);
	} else if (!this->showStats && !this->modeMessageDisplayed && XtIsManaged(this->statsLineForm)) {
		/* turn off statsline since there's nothing to show */
		this->showStatistics(False);
	}

	/* signal if macro/shell is running */
	if (this->shellCmdData || this->macroCmdData)
		BeginWait(this->shell);
	else
		EndWait(this->shell);

	/* we need to force the statsline to reveal itself */
	if (XtIsManaged(this->statsLineForm)) {
		XmTextSetCursorPosition(this->statsLine, 0);    /* start of line */
		XmTextSetCursorPosition(this->statsLine, 9000); /* end of line */
	}

	XmUpdateDisplay(this->statsLine);
	this->refreshMenuBar();

	this->updateLineNumDisp();
}

/*
** Refresh the various settings/state of the shell window per the
** settings of the top document.
*/
void Document::refreshMenuBar() {
	this->RefreshMenuToggleStates();

	/* Add/remove language specific menu items */
	UpdateUserMenus(this);

	/* refresh selection-sensitive menus */
	DimSelectionDepUserMenuItems(this, this->wasSelected);
}

/*
**  If necessary, enlarges the window and line number display area to make
**  room for numbers.
*/
int Document::updateLineNumDisp() {
	if (!this->showLineNumbers) {
		return 0;
	}

	/* Decide how wide the line number field has to be to display all
	   possible line numbers */
	return this->updateGutterWidth();
}

/*
**  Set the new gutter width in the window. Sadly, the only way to do this is
**  to set it on every single document, so we have to iterate over them.
**
**  (Iteration taken from NDocuments(); is there a better way to do it?)
*/
int Document::updateGutterWidth() {
	Document *document;
	int reqCols = MIN_LINE_NUM_COLS;
	int newColsDiff = 0;
	int maxCols = 0;

	for (document = WindowList; nullptr != document; document = document->next) {
		if (document->shell == this->shell) {
			/*  We found ourselves a document from this this.  */
			int lineNumCols, tmpReqCols;
			textDisp *textD = ((TextWidget)document->textArea)->text.textD;

			XtVaGetValues(document->textArea, textNlineNumCols, &lineNumCols, nullptr);

			/* Is the width of the line number area sufficient to display all the
			   line numbers in the file?  If not, expand line number field, and the
			   this width. */

			if (lineNumCols > maxCols) {
				maxCols = lineNumCols;
			}

			tmpReqCols = textD->nBufferLines < 1 ? 1 : (int)log10((double)textD->nBufferLines + 1) + 1;

			if (tmpReqCols > reqCols) {
				reqCols = tmpReqCols;
			}
		}
	}

	if (reqCols != maxCols) {
		XFontStruct *fs;
		Dimension windowWidth;
		short fontWidth;

		newColsDiff = reqCols - maxCols;

		XtVaGetValues(this->textArea, textNfont, &fs, nullptr);
		fontWidth = fs->max_bounds.width;

		XtVaGetValues(this->shell, XmNwidth, &windowWidth, nullptr);
		XtVaSetValues(this->shell, XmNwidth, (Dimension)windowWidth + (newColsDiff * fontWidth), nullptr);

		this->UpdateWMSizeHints();
	}

	for (document = WindowList; nullptr != document; document = document->next) {
		if (document->shell == this->shell) {
			Widget text;
			int i;
			int lineNumCols;

			XtVaGetValues(document->textArea, textNlineNumCols, &lineNumCols, nullptr);

			if (lineNumCols == reqCols) {
				continue;
			}

			/*  Update all panes of this document.  */
			for (i = 0; i <= document->nPanes; i++) {
				text = (i == 0) ? document->textArea : document->textPanes[i - 1];
				XtVaSetValues(text, textNlineNumCols, reqCols, nullptr);
			}
		}
	}

	return reqCols;
}

/*
** Refresh the menu entries per the settings of the
** top document.
*/
void Document::RefreshMenuToggleStates() {

	if (!this->IsTopDocument())
		return;

	/* File menu */
	XtSetSensitive(this->printSelItem, this->wasSelected);

	/* Edit menu */
	XtSetSensitive(this->undoItem, !this->undo.empty());
	XtSetSensitive(this->redoItem, !this->redo.empty());
	XtSetSensitive(this->printSelItem, this->wasSelected);
	XtSetSensitive(this->cutItem, this->wasSelected);
	XtSetSensitive(this->copyItem, this->wasSelected);
	XtSetSensitive(this->delItem, this->wasSelected);

	/* Preferences menu */
	XmToggleButtonSetState(this->statsLineItem, this->showStats, False);
	XmToggleButtonSetState(this->iSearchLineItem, this->showISearchLine, False);
	XmToggleButtonSetState(this->lineNumsItem, this->showLineNumbers, False);
	XmToggleButtonSetState(this->highlightItem, this->highlightSyntax, False);
	XtSetSensitive(this->highlightItem, this->languageMode != PLAIN_LANGUAGE_MODE);
	XmToggleButtonSetState(this->backlightCharsItem, this->backlightChars, False);
	XmToggleButtonSetState(this->saveLastItem, this->saveOldVersion, False);
	XmToggleButtonSetState(this->autoSaveItem, this->autoSave, False);
	XmToggleButtonSetState(this->overtypeModeItem, this->overstrike, False);
	XmToggleButtonSetState(this->matchSyntaxBasedItem, this->matchSyntaxBased, False);
	XmToggleButtonSetState(this->readOnlyItem, IS_USER_LOCKED(this->lockReasons), False);

	XtSetSensitive(this->smartIndentItem, SmartIndentMacrosAvailable(LanguageModeName(this->languageMode)));

	this->SetAutoIndent(this->indentStyle);
	this->SetAutoWrap(this->wrapMode);
	this->SetShowMatching(this->showMatchingStyle);
	SetLanguageMode(this, this->languageMode, FALSE);

	/* Windows Menu */
	XtSetSensitive(this->splitPaneItem, this->nPanes < MAX_PANES);
	XtSetSensitive(this->closePaneItem, this->nPanes > 0);
	XtSetSensitive(this->detachDocumentItem, this->NDocuments() > 1);
	XtSetSensitive(this->contextDetachDocumentItem, this->NDocuments() > 1);

	Document *win;
	for (win = WindowList; win; win = win->next)
		if (win->shell != this->shell)
			break;
	XtSetSensitive(this->moveDocumentItem, win != nullptr);
}

/*
** update the tab label, etc. of a tab, per the states of it's document.
*/
void Document::RefreshTabState() {
	XmString s1, tipString;
	char labelString[MAXPATHLEN];
	char *tag = XmFONTLIST_DEFAULT_TAG;
	unsigned char alignment;

	/* Set tab label to document's filename. Position of
	   "*" (modified) will change per label alignment setting */
	XtVaGetValues(this->tab, XmNalignment, &alignment, nullptr);
	if (alignment != XmALIGNMENT_END) {
		sprintf(labelString, "%s%s", this->fileChanged ? "*" : "", this->filename);
	} else {
		sprintf(labelString, "%s%s", this->filename, this->fileChanged ? "*" : "");
	}

	/* Make the top document stand out a little more */
	if (this->IsTopDocument())
		tag = (String) "BOLD";

	s1 = XmStringCreateLtoREx(labelString, tag);

	if (GetPrefShowPathInWindowsMenu() && this->filenameSet) {
		strcat(labelString, " - ");
		strcat(labelString, this->path);
	}
	tipString = XmStringCreateSimpleEx(labelString);

	XtVaSetValues(this->tab, XltNbubbleString, tipString, XmNlabelString, s1, nullptr);
	XmStringFree(s1);
	XmStringFree(tipString);
}

/*
** return the number of documents owned by this shell window
*/
int Document::NDocuments() {
	Document *win;
	int nDocument = 0;

	for (win = WindowList; win; win = win->next) {
		if (win->shell == this->shell)
			nDocument++;
	}

	return nDocument;
}

/*
** raise the document and its shell window and focus depending on pref.
*/
void Document::RaiseDocumentWindow() {
	if (!this)
		return;

	this->RaiseDocument();
	RaiseShellWindow(this->shell, GetPrefFocusOnRaise());
}

/*
** Update the optional statistics line.
*/
void Document::UpdateStatsLine() {
	int line;
	int colNum;	
	Widget statW = this->statsLine;
	XmString xmslinecol;

	if (!this->IsTopDocument()) {
		return;
	}

	/* This routine is called for each character typed, so its performance
	   affects overall editor perfomance.  Only update if the line is on. */
	if (!this->showStats) {
		return;
	}

	/* Compose the string to display. If line # isn't available, leave it off */
	int pos            = TextGetCursorPos(this->lastFocus);
	size_t string_size = strlen(this->filename) + strlen(this->path) + 45;
	auto string        = new char[string_size];
	const char *format = (this->fileFormat == DOS_FILE_FORMAT) ? " DOS" : (this->fileFormat == MAC_FILE_FORMAT ? " Mac" : "");
	char slinecol[32];
	
	if (!TextPosToLineAndCol(this->lastFocus, pos, &line, &colNum)) {
		snprintf(string, string_size, "%s%s%s %d bytes", this->path, this->filename, format, this->buffer->BufGetLength());
		snprintf(slinecol, sizeof(slinecol), "L: ---  C: ---");
	} else {
		snprintf(slinecol, sizeof(slinecol), "L: %d  C: %d", line, colNum);
		if (this->showLineNumbers) {
			snprintf(string, string_size, "%s%s%s byte %d of %d", this->path, this->filename, format, pos, this->buffer->BufGetLength());
		} else {
			snprintf(string, string_size, "%s%s%s %d bytes", this->path, this->filename, format, this->buffer->BufGetLength());
		}
	}

	/* Update the line/column number */
	xmslinecol = XmStringCreateSimpleEx(slinecol);
	XtVaSetValues(this->statsLineColNo, XmNlabelString, xmslinecol, nullptr);
	XmStringFree(xmslinecol);

	/* Don't clobber the line if there's a special message being displayed */
	if (!this->modeMessageDisplayed) {
		/* Change the text in the stats line */
		XmTextReplace(statW, 0, XmTextGetLastPosition(statW), string);
	}
	delete [] string;

	/* Update the line/col display */
	xmslinecol = XmStringCreateSimpleEx(slinecol);
	XtVaSetValues(this->statsLineColNo, XmNlabelString, xmslinecol, nullptr);
	XmStringFree(xmslinecol);
}

/*
** Update the window manager's size hints.  These tell it the increments in
** which it is allowed to resize the window.  While this isn't particularly
** important for NEdit (since it can tolerate any window size), setting these
** hints also makes the resize indicator show the window size in characters
** rather than pixels, which is very helpful to users.
*/
void Document::UpdateWMSizeHints() {
	Dimension shellWidth, shellHeight, textHeight, hScrollBarHeight;
	int marginHeight, marginWidth, totalHeight, nCols, nRows;
	XFontStruct *fs;
	int i, baseWidth, baseHeight, fontHeight, fontWidth;
	Widget hScrollBar;
	textDisp *textD = ((TextWidget)this->textArea)->text.textD;

	/* Find the dimensions of a single character of the text font */
	XtVaGetValues(this->textArea, textNfont, &fs, nullptr);
	fontHeight = textD->ascent + textD->descent;
	fontWidth = fs->max_bounds.width;

	/* Find the base (non-expandable) width and height of the editor this.

	   FIXME:
	   To workaround the shrinking-this bug on some WM such as Metacity,
	   which caused the this to shrink as we switch between documents
	   using different font sizes on the documents in the same this, the
	   base width, and similarly the base height, is ajusted such that:
	        shellWidth = baseWidth + cols * textWidth
	   There are two issues with this workaround:
	   1. the right most characters may appear partially obsure
	   2. the Col x Row info reported by the WM will be based on the fully
	      display text.
	*/
	XtVaGetValues(this->textArea, XmNheight, &textHeight, textNmarginHeight, &marginHeight, textNmarginWidth, &marginWidth, nullptr);
	totalHeight = textHeight - 2 * marginHeight;
	for (i = 0; i < this->nPanes; i++) {
		XtVaGetValues(this->textPanes[i], XmNheight, &textHeight, textNhScrollBar, &hScrollBar, nullptr);
		totalHeight += textHeight - 2 * marginHeight;
		if (!XtIsManaged(hScrollBar)) {
			XtVaGetValues(hScrollBar, XmNheight, &hScrollBarHeight, nullptr);
			totalHeight -= hScrollBarHeight;
		}
	}

	XtVaGetValues(this->shell, XmNwidth, &shellWidth, XmNheight, &shellHeight, nullptr);
	nCols = textD->width / fontWidth;
	nRows = totalHeight / fontHeight;
	baseWidth = shellWidth - nCols * fontWidth;
	baseHeight = shellHeight - nRows * fontHeight;

	/* Set the size hints in the shell widget */
	XtVaSetValues(this->shell, XmNwidthInc, fs->max_bounds.width, XmNheightInc, fontHeight, XmNbaseWidth, baseWidth, XmNbaseHeight, baseHeight, XmNminWidth, baseWidth + fontWidth, XmNminHeight,
	              baseHeight + (1 + this->nPanes) * fontHeight, nullptr);

	/* Motif will keep placing this on the shell every time we change it,
	   so it needs to be undone every single time.  This only seems to
	   happen on mult-head dispalys on screens 1 and higher. */

	RemovePPositionHint(this->shell);
}

/*
**
*/
int Document::WidgetToPaneIndex(Widget w) {
	int i;
	Widget text;
	int paneIndex = 0;

	for (i = 0; i <= this->nPanes; ++i) {
		text = (i == 0) ? this->textArea : this->textPanes[i - 1];
		if (text == w) {
			paneIndex = i;
		}
	}
	return (paneIndex);
}

/*
**
*/
void Document::SetTabDist(int tabDist) {
	if (this->buffer->tabDist_ != tabDist) {
		int saveCursorPositions[MAX_PANES + 1];
		int saveVScrollPositions[MAX_PANES + 1];
		int saveHScrollPositions[MAX_PANES + 1];
		int paneIndex;

		this->ignoreModify = True;

		for (paneIndex = 0; paneIndex <= this->nPanes; ++paneIndex) {
			Widget w = this->GetPaneByIndex(paneIndex);
			textDisp *textD = reinterpret_cast<TextWidget>(w)->text.textD;

			TextGetScroll(w, &saveVScrollPositions[paneIndex], &saveHScrollPositions[paneIndex]);
			saveCursorPositions[paneIndex] = TextGetCursorPos(w);
			textD->modifyingTabDist = 1;
		}

		this->buffer->BufSetTabDistance(tabDist);

		for (paneIndex = 0; paneIndex <= this->nPanes; ++paneIndex) {
			Widget w = this->GetPaneByIndex(paneIndex);
			textDisp *textD = reinterpret_cast<TextWidget>(w)->text.textD;

			textD->modifyingTabDist = 0;
			TextSetCursorPos(w, saveCursorPositions[paneIndex]);
			TextSetScroll(w, saveVScrollPositions[paneIndex], saveHScrollPositions[paneIndex]);
		}

		this->ignoreModify = False;
	}
}

/*
**
*/
void Document::SetEmTabDist(int emTabDist) {

	XtVaSetValues(this->textArea, textNemulateTabs, emTabDist, nullptr);
	for (int i = 0; i < this->nPanes; ++i) {
		XtVaSetValues(this->textPanes[i], textNemulateTabs, emTabDist, nullptr);
	}
}

/*
**
*/
static void showTabBar(Document *window, int state) {
	if (state) {
		XtManageChild(XtParent(window->tabBar));
		window->showStatsForm();
	} else {
		XtUnmanageChild(XtParent(window->tabBar));
		window->showStatsForm();
	}
}

/*
**
*/
void Document::ShowTabBar(int state) {
	if (XtIsManaged(XtParent(this->tabBar)) == state)
		return;
	showTabBar(this, state);
}

/*
** Turn on and off the continuing display of the incremental search line
** (when off, it is popped up and down as needed via TempShowISearch)
*/
void Document::ShowISearchLine(int state) {
	Document *win;

	if (this->showISearchLine == state)
		return;
	this->showISearchLine = state;
	this->showISearch(state);

	/* i-search line is shell-level, hence other tabbed
	   documents in the this should synch */
	for (win = WindowList; win; win = win->next) {
		if (win->shell != this->shell || win == this)
			continue;
		win->showISearchLine = state;
	}
}

/*
** Temporarily show and hide the incremental search line if the line is not
** already up.
*/
void Document::TempShowISearch(int state) {
	if (this->showISearchLine)
		return;
	if (XtIsManaged(this->iSearchForm) != state)
		this->showISearch(state);
}

/*
** Display a special message in the stats line (show the stats line if it
** is not currently shown).
*/
void Document::SetModeMessage(const char *message) {
	/* this document may be hidden (not on top) or later made hidden,
	   so we save a copy of the mode message, so we can restore the
	   statsline when the document is raised to top again */
	this->modeMessageDisplayed = True;
	XtFree(this->modeMessage);
	this->modeMessage = XtNewStringEx(message);

	if (!this->IsTopDocument())
		return;

	XmTextSetStringEx(this->statsLine, message);
	/*
	 * Don't invoke the stats line again, if stats line is already displayed.
	 */
	if (!this->showStats)
		this->showStatistics(True);
}

/*
** Clear special statistics line message set in SetModeMessage, returns
** the statistics line to its original state as set in window->showStats
*/
void Document::ClearModeMessage() {
	if (!this->modeMessageDisplayed)
		return;

	this->modeMessageDisplayed = False;
	XtFree(this->modeMessage);
	this->modeMessage = nullptr;

	if (!this->IsTopDocument())
		return;

	/*
	 * Remove the stats line only if indicated by it's window state.
	 */
	if (!this->showStats) {
		this->showStatistics(False);
	}
	
	this->UpdateStatsLine();
}

/*
** Set the backlight character class string
*/
void Document::SetBacklightChars(char *applyBacklightTypes) {
	int i;
	int is_applied = XmToggleButtonGetState(this->backlightCharsItem) ? 1 : 0;
	int do_apply = applyBacklightTypes ? 1 : 0;

	this->backlightChars = do_apply;

	XtFree(this->backlightCharTypes);
	
	
	if(this->backlightChars) {
		this->backlightCharTypes = XtStringDup(applyBacklightTypes);
	} else {
		this->backlightCharTypes = nullptr;
	}

	XtVaSetValues(this->textArea, textNbacklightCharTypes, this->backlightCharTypes, nullptr);
	for (i = 0; i < this->nPanes; i++)
		XtVaSetValues(this->textPanes[i], textNbacklightCharTypes, this->backlightCharTypes, nullptr);
	if (is_applied != do_apply)
		this->SetToggleButtonState(this->backlightCharsItem, do_apply, False);
}

/*
** Update the read-only state of the text area(s) in the window, and
** the ReadOnly toggle button in the File menu to agree with the state in
** the window data structure.
*/
void Document::UpdateWindowReadOnly() {
	int i, state;

	if (!this->IsTopDocument())
		return;

	state = IS_ANY_LOCKED(this->lockReasons);
	XtVaSetValues(this->textArea, textNreadOnly, state, nullptr);
	for (i = 0; i < this->nPanes; i++)
		XtVaSetValues(this->textPanes[i], textNreadOnly, state, nullptr);
	XmToggleButtonSetState(this->readOnlyItem, state, FALSE);
	XtSetSensitive(this->readOnlyItem, !IS_ANY_LOCKED_IGNORING_USER(this->lockReasons));
}

/*
** Bring up the previous window by tab order
*/
void Document::PreviousDocument() {

	if (!WindowList->next) {
		return;
	}

	Document *win = this->getNextTabWindow(-1, GetPrefGlobalTabNavigate(), 1);
	if(!win)
		return;

	if (this->shell == win->shell)
		win->RaiseDocument();
	else
		win->RaiseFocusDocumentWindow(True);
}

/*
** Bring up the next window by tab order
*/
void Document::NextDocument() {
	Document *win;

	if (!WindowList->next)
		return;

	win = this->getNextTabWindow(1, GetPrefGlobalTabNavigate(), 1);
	if(!win)
		return;

	if (this->shell == win->shell)
		win->RaiseDocument();
	else
		win->RaiseFocusDocumentWindow(True);
}

/*
** Change the window appearance and the window data structure to show
** that the file it contains has been modified
*/
void Document::SetWindowModified(int modified) {
	if (this->fileChanged == FALSE && modified == TRUE) {
		this->SetSensitive(this->closeItem, TRUE);
		this->fileChanged = TRUE;
		this->UpdateWindowTitle();
		this->RefreshTabState();
	} else if (this->fileChanged == TRUE && modified == FALSE) {
		this->fileChanged = FALSE;
		this->UpdateWindowTitle();
		this->RefreshTabState();
	}
}

/*
** Update the window title to reflect the filename, read-only, and modified
** status of the window data structure
*/
void Document::UpdateWindowTitle() {

	if (!this->IsTopDocument()) {
		return;
	}

	char *title = FormatWindowTitle(this->filename, this->path, GetClearCaseViewTag(), GetPrefServerName(), IsServer, this->filenameSet, this->lockReasons, this->fileChanged, GetPrefTitleFormat());
	char *iconTitle = new char[strlen(this->filename) + 2]; /* strlen("*")+1 */

	strcpy(iconTitle, this->filename);
	if (this->fileChanged) {
		strcat(iconTitle, "*");
	}
	
	XtVaSetValues(this->shell, XmNtitle, title, XmNiconName, iconTitle, nullptr);

	/* If there's a find or replace dialog up in "Keep Up" mode, with a
	   file name in the title, update it too */
	if (this->findDlog && XmToggleButtonGetState(this->findKeepBtn)) {
		sprintf(title, "Find (in %s)", this->filename);
		XtVaSetValues(XtParent(this->findDlog), XmNtitle, title, nullptr);
	}
	if (this->replaceDlog && XmToggleButtonGetState(this->replaceKeepBtn)) {
		sprintf(title, "Replace (in %s)", this->filename);
		XtVaSetValues(XtParent(this->replaceDlog), XmNtitle, title, nullptr);
	}
	delete [] iconTitle;

	/* Update the Windows menus with the new name */
	InvalidateWindowMenus();
}

/*
** Sort tabs in the tab bar alphabetically, if demanded so.
*/
void Document::SortTabBar() {

	if (!GetPrefSortTabs())
		return;

	/* need more than one tab to sort */
	const int nDoc = this->NDocuments();
	if (nDoc < 2) {
		return;
	}

	/* first sort the documents */
	std::vector<Document *> windows;
	windows.reserve(nDoc);
	
	for (Document *w = WindowList; w != nullptr; w = w->next) {
		if (this->shell == w->shell)
			windows.push_back(w);
	}
	
	std::sort(windows.begin(), windows.end(), [](const Document *a, const Document *b) {
		if(strcmp(a->filename, b->filename) < 0) {
			return true;
		}
		return strcmp(a->path, b->path) < 0;
	});
	
	/* assign tabs to documents in sorted order */
	WidgetList tabList;
	int tabCount;	
	XtVaGetValues(this->tabBar, XmNtabWidgetList, &tabList, XmNtabCount, &tabCount, nullptr);

	for (int i = 0, j = 0; i < tabCount && j < nDoc; i++) {
		if (tabList[i]->core.being_destroyed)
			continue;

		/* set tab as active */
		if (windows[j]->IsTopDocument())
			XmLFolderSetActiveTab(this->tabBar, i, False);

		windows[j]->tab = tabList[i];
		windows[j]->RefreshTabState();
		j++;
	}
}

/*
** Update the minimum allowable height for a split pane after a change
** to font or margin height.
*/
void Document::UpdateMinPaneHeights() {
	textDisp *textD = ((TextWidget)this->textArea)->text.textD;
	Dimension hsbHeight, swMarginHeight, frameShadowHeight;
	int i, marginHeight, minPaneHeight;
	Widget hScrollBar;

	/* find the minimum allowable size for a pane */
	XtVaGetValues(this->textArea, textNhScrollBar, &hScrollBar, nullptr);
	XtVaGetValues(containingPane(this->textArea), XmNscrolledWindowMarginHeight, &swMarginHeight, nullptr);
	XtVaGetValues(XtParent(this->textArea), XmNshadowThickness, &frameShadowHeight, nullptr);
	XtVaGetValues(this->textArea, textNmarginHeight, &marginHeight, nullptr);
	XtVaGetValues(hScrollBar, XmNheight, &hsbHeight, nullptr);
	minPaneHeight = textD->ascent + textD->descent + marginHeight * 2 + swMarginHeight * 2 + hsbHeight + 2 * frameShadowHeight;

	/* Set it in all of the widgets in the this */
	setPaneMinHeight(containingPane(this->textArea), minPaneHeight);
	for (i = 0; i < this->nPanes; i++)
		setPaneMinHeight(containingPane(this->textPanes[i]), minPaneHeight);
}

/*
** Update the "New (in X)" menu item to reflect the preferences
*/
void Document::UpdateNewOppositeMenu(int openInTab) {
	XmString lbl;
	if (openInTab)
		XtVaSetValues(this->newOppositeItem, XmNlabelString, lbl = XmStringCreateSimpleEx("New Window"), XmNmnemonic, 'W', nullptr);
	else
		XtVaSetValues(this->newOppositeItem, XmNlabelString, lbl = XmStringCreateSimpleEx("New Tab"), XmNmnemonic, 'T', nullptr);
	XmStringFree(lbl);
}

/*
** Set insert/overstrike mode
*/
void Document::SetOverstrike(int overstrike) {

	XtVaSetValues(this->textArea, textNoverstrike, overstrike, nullptr);
	for (int i = 0; i < this->nPanes; i++)
		XtVaSetValues(this->textPanes[i], textNoverstrike, overstrike, nullptr);
	this->overstrike = overstrike;
}

/*
** Select auto-wrap mode, one of NO_WRAP, NEWLINE_WRAP, or CONTINUOUS_WRAP
*/
void Document::SetAutoWrap(int state) {
	int autoWrap = state == NEWLINE_WRAP, contWrap = state == CONTINUOUS_WRAP;

	XtVaSetValues(this->textArea, textNautoWrap, autoWrap, textNcontinuousWrap, contWrap, nullptr);
	for (int i = 0; i < this->nPanes; i++)
		XtVaSetValues(this->textPanes[i], textNautoWrap, autoWrap, textNcontinuousWrap, contWrap, nullptr);
	this->wrapMode = state;

	if (this->IsTopDocument()) {
		XmToggleButtonSetState(this->newlineWrapItem, autoWrap, False);
		XmToggleButtonSetState(this->continuousWrapItem, contWrap, False);
		XmToggleButtonSetState(this->noWrapItem, state == NO_WRAP, False);
	}
}

/*
** Set the auto-scroll margin
*/
void Document::SetAutoScroll(int margin) {

	XtVaSetValues(this->textArea, textNcursorVPadding, margin, nullptr);
	for (int i = 0; i < this->nPanes; i++)
		XtVaSetValues(this->textPanes[i], textNcursorVPadding, margin, nullptr);
}

/*
**
*/
void Document::SetColors(const char *textFg, const char *textBg, const char *selectFg, const char *selectBg, const char *hiliteFg, const char *hiliteBg, const char *lineNoFg, const char *cursorFg) {
	
	int i, dummy;
	
	Pixel textFgPix = AllocColor(this->textArea, textFg, &dummy, &dummy, &dummy), textBgPix = AllocColor(this->textArea, textBg, &dummy, &dummy, &dummy), selectFgPix = AllocColor(this->textArea, selectFg, &dummy, &dummy, &dummy);
	Pixel selectBgPix = AllocColor(this->textArea, selectBg, &dummy, &dummy, &dummy), hiliteFgPix = AllocColor(this->textArea, hiliteFg, &dummy, &dummy, &dummy);
	Pixel hiliteBgPix = AllocColor(this->textArea, hiliteBg, &dummy, &dummy, &dummy), lineNoFgPix = AllocColor(this->textArea, lineNoFg, &dummy, &dummy, &dummy);
	Pixel cursorFgPix = AllocColor(this->textArea, cursorFg, &dummy, &dummy, &dummy);
	
	textDisp *textD;

	/* Update the main pane */
	XtVaSetValues(this->textArea, XmNforeground, textFgPix, XmNbackground, textBgPix, nullptr);
	textD = ((TextWidget)this->textArea)->text.textD;
	textD->	TextDSetColors(textFgPix, textBgPix, selectFgPix, selectBgPix, hiliteFgPix, hiliteBgPix, lineNoFgPix, cursorFgPix);
	/* Update any additional panes */
	for (i = 0; i < this->nPanes; i++) {
		XtVaSetValues(this->textPanes[i], XmNforeground, textFgPix, XmNbackground, textBgPix, nullptr);
		textD = ((TextWidget)this->textPanes[i])->text.textD;
		textD->TextDSetColors(textFgPix, textBgPix, selectFgPix, selectBgPix, hiliteFgPix, hiliteBgPix, lineNoFgPix, cursorFgPix);
	}

	/* Redo any syntax highlighting */
	if (this->highlightData)
		UpdateHighlightStyles(this);
}

/*
** close all the documents in a window
*/
int Document::CloseAllDocumentInWindow() {
	Document *win;

	if (this->NDocuments() == 1) {
		/* only one document in the window */
		return CloseFileAndWindow(this, PROMPT_SBC_DIALOG_RESPONSE);
	} else {
		Widget winShell = this->shell;
		Document *topDocument;

		/* close all _modified_ documents belong to this window */
		for (win = WindowList; win;) {
			if (win->shell == winShell && win->fileChanged) {
				Document *next = win->next;
				if (!CloseFileAndWindow(win, PROMPT_SBC_DIALOG_RESPONSE))
					return False;
				win = next;
			} else
				win = win->next;
		}

		/* see there's still documents left in the window */
		for (win = WindowList; win; win = win->next)
			if (win->shell == winShell)
				break;

		if (win) {
			topDocument = GetTopDocument(winShell);

			/* close all non-top documents belong to this window */
			for (win = WindowList; win;) {
				if (win->shell == winShell && win != topDocument) {
					Document *next = win->next;
					if (!CloseFileAndWindow(win, PROMPT_SBC_DIALOG_RESPONSE))
						return False;
					win = next;
				} else
					win = win->next;
			}

			/* close the last document and its window */
			if (!CloseFileAndWindow(topDocument, PROMPT_SBC_DIALOG_RESPONSE))
				return False;
		}
	}

	return True;
}
