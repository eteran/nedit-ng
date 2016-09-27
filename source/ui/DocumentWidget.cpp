
#include <QBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QtDebug>
#include "MainWindow.h"
#include "DocumentWidget.h"
#include "DialogReplace.h"
#include "TextArea.h"
#include "preferences.h"
#include "TextBuffer.h"
#include "nedit.h"
#include "Color.h"
#include "highlight.h"
#include "LanguageMode.h"
#include "search.h"
#include "clearcase.h"
#include "dragEndCBStruct.h"
#include "smartIndentCBStruct.h"
#include "smartIndent.h"
#include "highlightData.h"
#include "MenuItem.h"
#include "tags.h"
#include "HighlightData.h"
#include "WindowHighlightData.h"

namespace {

void modifiedCB(int pos, int nInserted, int nDeleted, int nRestyled, view::string_view deletedText, void *user) {
    if(auto w = static_cast<DocumentWidget *>(user)) {
        w->modifiedCallback(pos, nInserted, nDeleted, nRestyled, deletedText);
    }
}

void smartIndentCB(TextArea *area, smartIndentCBStruct *data, void *user) {
	if(auto w = static_cast<DocumentWidget *>(user)) {
		w->smartIndentCallback(area, data);
	}
}

void movedCB(TextArea *area, void *user) {
	if(auto w = static_cast<DocumentWidget *>(user)) {
		w->movedCallback(area);
	}
}

void dragStartCB(TextArea *area, void *user) {
	if(auto w = static_cast<DocumentWidget *>(user)) {
		w->dragStartCallback(area);
	}
}

void dragEndCB(TextArea *area, dragEndCBStruct *data, void *user) {
	if(auto w = static_cast<DocumentWidget *>(user)) {
		w->dragEndCallback(area, data);
	}
}



/*
** Update a position across buffer modifications specified by
** "modPos", "nDeleted", and "nInserted".
*/
void maintainPosition(int *position, int modPos, int nInserted, int nDeleted) {
    if (modPos > *position) {
        return;
    }

    if (modPos + nDeleted <= *position) {
        *position += nInserted - nDeleted;
    } else {
        *position = modPos;
    }
}

/*
** Update a selection across buffer modifications specified by
** "pos", "nDeleted", and "nInserted".
*/
void maintainSelection(TextSelection *sel, int pos, int nInserted, int nDeleted) {
    if (!sel->selected || pos > sel->end) {
        return;
    }

    maintainPosition(&sel->start, pos, nInserted, nDeleted);
    maintainPosition(&sel->end, pos, nInserted, nDeleted);
    if (sel->end <= sel->start) {
        sel->selected = false;
    }
}


}

//------------------------------------------------------------------------------
// Name: DocumentWidget
//------------------------------------------------------------------------------
DocumentWidget::DocumentWidget(const QString &name, QWidget *parent, Qt::WindowFlags f) : QWidget(parent, f) {

	ui.setupUi(this);

	buffer_ = new TextBuffer();

	// create the text widget
	splitter_ = new QSplitter(Qt::Vertical, this);
	splitter_->setChildrenCollapsible(false);
	ui.verticalLayout->addWidget(splitter_);
	ui.verticalLayout->setContentsMargins(0, 0, 0, 0);

#if 1
	{
        auto area = createTextArea(buffer_);
        splitter_->addWidget(area);
	}
#if 0
	{
        auto area = createTextArea(buffer_);
        splitter_->addWidget(area);
    }
#endif
#endif
	
	// initialize the members
	multiFileReplSelected_ = false;
	multiFileBusy_         = false;
	writableWindows_       = nullptr;
	nWritableWindows_      = 0;
	fileChanged_           = false;
	fileMissing_           = true;
	fileMode_              = 0;
	fileUid_               = 0;
	fileGid_               = 0;
	filenameSet_           = false;
	fileFormat_            = UNIX_FILE_FORMAT;
	lastModTime_           = 0;
	filename_              = name;
	undo_                  = std::list<UndoInfo *>();
	redo_                  = std::list<UndoInfo *>();
	autoSaveCharCount_     = 0;
	autoSaveOpCount_       = 0;
	undoMemUsed_           = 0;
	
	lockReasons_.clear();
	
	indentStyle_           = GetPrefAutoIndent(PLAIN_LANGUAGE_MODE);
	autoSave_              = GetPrefAutoSave();
	saveOldVersion_        = GetPrefSaveOldVersion();
	wrapMode_              = GetPrefWrap(PLAIN_LANGUAGE_MODE);
	overstrike_            = false;
	showMatchingStyle_     = GetPrefShowMatching();
	matchSyntaxBased_      = GetPrefMatchSyntaxBased();
	highlightSyntax_       = GetPrefHighlightSyntax();
	backlightCharTypes_    = QString();
	backlightChars_        = GetPrefBacklightChars();
	
	if (backlightChars_) {
		const char *cTypes = GetPrefBacklightCharTypes();
		if (cTypes && backlightChars_) {			
			backlightCharTypes_ = QLatin1String(cTypes);
		}
	}
	
	modeMessageDisplayed_  = false;
	modeMessage_           = nullptr;
	ignoreModify_          = false;
	windowMenuValid_       = false;

    flashTimer_            = new QTimer(this);

	fileClosedAtom_        = None;
	wasSelected_           = false;
	fontName_              = GetPrefFontName();
	italicFontName_        = GetPrefItalicFontName();
	boldFontName_          = GetPrefBoldFontName();
	boldItalicFontName_    = GetPrefBoldItalicFontName();
	dialogColors_          = nullptr;
	fontList_              = GetPrefFontList();
	italicFontStruct_      = GetPrefItalicFont();
	boldFontStruct_        = GetPrefBoldFont();
	boldItalicFontStruct_  = GetPrefBoldItalicFont();
	dialogFonts_           = nullptr;
	nMarks_                = 0;
	markTimeoutID_         = 0;
	highlightData_         = nullptr;
	shellCmdData_          = nullptr;
	macroCmdData_          = nullptr;
	smartIndentData_       = nullptr;
	languageMode_          = PLAIN_LANGUAGE_MODE;
	iSearchHistIndex_      = 0;
	iSearchStartPos_       = -1;
	iSearchLastRegexCase_  = true;
	iSearchLastLiteralCase_= false;
	device_                = 0;
	inode_                 = 0;	
	
    if(auto win = toWindow()) {
		ui.statusFrame->setVisible(win->showStats_);
	}

    flashTimer_->setInterval(1500);
    flashTimer_->setSingleShot(true);
    connect(flashTimer_, SIGNAL(timeout()), this, SLOT(flashTimerTimeout()));
}


//------------------------------------------------------------------------------
// Name: ~DocumentWidget
//------------------------------------------------------------------------------
DocumentWidget::~DocumentWidget() {
}

//------------------------------------------------------------------------------
// Name: createTextArea
//------------------------------------------------------------------------------
TextArea *DocumentWidget::createTextArea(TextBuffer *buffer) {

    int P_marginWidth  = 5;
    int P_marginHeight = 5;
    int lineNumCols    = 0;
    int marginWidth    = 0;
    int charWidth      = 0;
    int l = P_marginWidth + ((lineNumCols == 0) ? 0 : marginWidth + charWidth * lineNumCols);
    int h = P_marginHeight;

    auto area = new TextArea(this,
                             l,
                             h,
                             100,
                             100,
                             0,
                             0,
                             buffer,
                             QFont(tr("Monospace"), 10),
                             Qt::white,
                             Qt::black,
                             Qt::white,
                             Qt::blue,
                             Qt::black, //QColor highlightFGPixel,
                             Qt::black, //QColor highlightBGPixel,
                             Qt::black, //QColor cursorFGPixel,
                             Qt::black  //QColor lineNumFGPixel,
                             );

    Pixel textFgPix   = AllocColor(GetPrefColorName(TEXT_FG_COLOR));
    Pixel textBgPix   = AllocColor(GetPrefColorName(TEXT_BG_COLOR));
    Pixel selectFgPix = AllocColor(GetPrefColorName(SELECT_FG_COLOR));
    Pixel selectBgPix = AllocColor(GetPrefColorName(SELECT_BG_COLOR));
    Pixel hiliteFgPix = AllocColor(GetPrefColorName(HILITE_FG_COLOR));
    Pixel hiliteBgPix = AllocColor(GetPrefColorName(HILITE_BG_COLOR));
    Pixel lineNoFgPix = AllocColor(GetPrefColorName(LINENO_FG_COLOR));
    Pixel cursorFgPix = AllocColor(GetPrefColorName(CURSOR_FG_COLOR));

    area->TextDSetColors(
        toQColor(textFgPix),
        toQColor(textBgPix),
        toQColor(selectFgPix),
        toQColor(selectBgPix),
        toQColor(hiliteFgPix),
        toQColor(hiliteBgPix),
        toQColor(lineNoFgPix),
        toQColor(cursorFgPix));

    // add focus, drag, cursor tracking, and smart indent callbacks
    area->addCursorMovementCallback(movedCB, this);
    area->addDragStartCallback(dragStartCB, this);
    area->addDragEndCallback(dragEndCB, this);
    area->addSmartIndentCallback(smartIndentCB, this);

    connect(area, SIGNAL(focusIn(QWidget*)), this, SLOT(onFocusIn(QWidget*)));
    connect(area, SIGNAL(focusOut(QWidget*)), this, SLOT(onFocusOut(QWidget*)));

    buffer_->BufAddModifyCB(modifiedCB, this);
    return area;
}

//------------------------------------------------------------------------------
// Name: onFocusChanged
//------------------------------------------------------------------------------
void DocumentWidget::onFocusIn(QWidget *now) {
	if(auto w = qobject_cast<TextArea *>(now)) {
		if(auto window = qobject_cast<MainWindow *>(w->window())) {

			// record which window pane last had the keyboard focus
			window->lastFocus_ = w;

			// update line number statistic to reflect current focus pane
			UpdateStatsLine(w);

			// finish off the current incremental search
			EndISearch();
	#if 0
			// Check for changes to read-only status and/or file modifications
			CheckForChangesToFile(window);
	#endif
		}
	}
}

void DocumentWidget::onFocusOut(QWidget *was) {
	Q_UNUSED(was);
}

/*
** Incremental searching is anchored at the position where the cursor
** was when the user began typing the search string.  Call this routine
** to forget about this original anchor, and if the search bar is not
** permanently up, pop it down.
*/
void DocumentWidget::EndISearch() {

	/* Note: Please maintain this such that it can be freely peppered in
	   mainline code, without callers having to worry about performance
	   or visual glitches.  */

	// Forget the starting position used for the current run of searches
	iSearchStartPos_ = -1;

	// Mark the end of incremental search history overwriting
    saveSearchHistory("", nullptr, SEARCH_LITERAL, false);

	// Pop down the search line (if it's not pegged up in Preferences)
    if(auto win = toWindow()) {
        win->TempShowISearch(false);
    }
}

MainWindow *DocumentWidget::toWindow() const {
    return qobject_cast<MainWindow *>(window());
}

/*
** Change the window appearance and the window data structure to show
** that the file it contains has been modified
*/
void DocumentWidget::SetWindowModified(bool modified) {
    if(auto win = toWindow()) {
		if (!fileChanged_ && modified) {
			win->ui.action_Close->setEnabled(true);
			fileChanged_ = true;
			win->UpdateWindowTitle(this);
			RefreshTabState();
		} else if (fileChanged_ && !modified) {
			fileChanged_ = false;
			win->UpdateWindowTitle(this);
			RefreshTabState();
		}
	}
}

/*
** update the tab label, etc. of a tab, per the states of it's document.
*/
void DocumentWidget::RefreshTabState() {

    if(auto w = toWindow()) {
		TabWidget *tabWidget = w->ui.tabWidget;
		int index = tabWidget->indexOf(this);

		QString labelString;

		/* Set tab label to document's filename. Position of
		   "*" (modified) will change per label alignment setting */
		QStyle *style = tabWidget->getTabBar()->style();
		int alignment = style->styleHint(QStyle::SH_TabBar_Alignment);

		if (alignment != Qt::AlignRight) {
			labelString = tr("%1%2").arg(fileChanged_ ? tr("*") : tr("")).arg(filename_);
		} else {
			labelString = tr("%1%2").arg(filename_).arg(fileChanged_ ? tr("*") : tr(""));
		}

        // NOTE(eteran): original code made the tab with focus show in bold
        // a) There is not *easy* way to do this in Qt (can be done with style sheets)
        // b) I say, by default let Qt style the tabs as it likes, user can always override with theming

		tabWidget->setTabText(index, labelString);

		QString tipString;
		if (GetPrefShowPathInWindowsMenu() && filenameSet_) {
			tipString = tr("%1  - %2").arg(labelString, path_);
		} else {
			tipString = labelString;
		}

		tabWidget->setTabToolTip(index, tipString);
	}
}

/*
** Apply language mode matching criteria and set languageMode_ to
** the appropriate mode for the current file, trigger language mode
** specific actions (turn on/off highlighting), and update the language
** mode menu item.  If forceNewDefaults is true, re-establish default
** settings for language-specific preferences regardless of whether
** they were previously set by the user.
*/
void DocumentWidget::DetermineLanguageMode(bool forceNewDefaults) {
	SetLanguageMode(matchLanguageMode(), forceNewDefaults);
}

/*
** Set the language mode for the window, update the menu and trigger language
** mode specific actions (turn on/off highlighting).  If forceNewDefaults is
** true, re-establish default settings for language-specific preferences
** regardless of whether they were previously set by the user.
*/
void DocumentWidget::SetLanguageMode(int mode, bool forceNewDefaults) {

    // Do mode-specific actions
    reapplyLanguageMode(mode, forceNewDefaults);

	// Select the correct language mode in the sub-menu
    if (IsTopDocument()) {
        if(auto win = toWindow()) {
            QList<QAction *> languages = win->ui.action_Language_Mode->menu()->actions();
            for(QAction *action : languages) {
                if(action->data().value<int>() == mode) {
                    action->setChecked(true);
                    break;
                }
            }
        }
	}
}

/*
** Find and return the name of the appropriate languange mode for
** the file in "window".  Returns a pointer to a string, which will
** remain valid until a change is made to the language modes list.
*/
int DocumentWidget::matchLanguageMode() {

	int fileNameLen;
	int beginPos;
	int endPos;

	/*... look for an explicit mode statement first */

	// Do a regular expression search on for recognition pattern
	std::string first200 = buffer_->BufGetRangeEx(0, 200);
	for (int i = 0; i < NLanguageModes; i++) {
		if (!LanguageModes[i]->recognitionExpr.isNull()) {
            if (SearchString(first200, LanguageModes[i]->recognitionExpr.toLatin1().data(), SEARCH_FORWARD, SEARCH_REGEX, false, 0, &beginPos, &endPos, nullptr, nullptr, nullptr)) {
				return i;
			}
		}
	}

	/* Look at file extension ("@@/" starts a ClearCase version extended path,
	   which gets appended after the file extension, and therefore must be
	   stripped off to recognize the extension to make ClearCase users happy) */
	fileNameLen = filename_.size();

	int versionExtendedPathIndex = GetClearCaseVersionExtendedPathIndex(filename_);
	if (versionExtendedPathIndex != -1) {
		fileNameLen = versionExtendedPathIndex;
	}

	for (int i = 0; i < NLanguageModes; i++) {
        for(const QString &ext : LanguageModes[i]->extensions) {
			// TODO(eteran): we could use QFileInfo to get the extension to make this code a little simpler
			int extLen = ext.size();
			int start = fileNameLen - extLen;

			if (start >= 0 && strncmp(&filename_.toLatin1().data()[start], ext.toLatin1().data(), extLen) == 0) {
				return i;
			}
		}
	}

	// no appropriate mode was found
	return PLAIN_LANGUAGE_MODE;
}

/*
** Update the optional statistics line.
*/
void DocumentWidget::UpdateStatsLine(TextArea *area) {

	if (!IsTopDocument()) {
		return;
	}

    if(auto win = toWindow()) {

        /* This routine is called for each character typed, so its performance
           affects overall editor perfomance.  Only update if the line is on. */
        if (!win->showStats_) {
            return;
        }

        if(!area) {
            area = win->lastFocus_;
            if(!area) {
                area = qobject_cast<TextArea *>(splitter_->widget(0));
            }
		}

		// Compose the string to display. If line # isn't available, leave it off
		int pos = area->TextGetCursorPos();

		QString format;
		switch(fileFormat_) {
		case DOS_FILE_FORMAT:
			format = tr(" DOS");
			break;
		case MAC_FILE_FORMAT:
			format = tr(" Mac");
			break;
		default:
			format = tr("");
			break;
		}

		QString string;
		QString slinecol;
		int line;
		int colNum;
		if (!area->TextDPosToLineAndCol(pos, &line, &colNum)) {
			string   = tr("%1%2%3 %4 bytes").arg(path_, filename_, format).arg(buffer_->BufGetLength());
			slinecol = tr("L: ---  C: ---");
		} else {
			slinecol = tr("L: %1  C: %2").arg(line).arg(colNum);
			if (win->showLineNumbers_) {
				string = tr("%1%2%3 byte %4 of %5").arg(path_, filename_, format).arg(pos).arg(buffer_->BufGetLength());
			} else {
				string = tr("%1%2%3 %4 bytes").arg(path_, filename_, format).arg(buffer_->BufGetLength());
			}
		}

		// Update the line/column number
		ui.labelStats->setText(slinecol);

		// Don't clobber the line if there's a special message being displayed
		if(!modeMessageDisplayed_) {
			ui.labelFileAndSize->setText(string);
		}
	}
}

void DocumentWidget::movedCallback(TextArea *area) {

	if (ignoreModify_) {
		return;
	}

	// update line and column nubers in statistics line
	UpdateStatsLine(area);

	// Check the character before the cursor for matchable characters
    FlashMatchingEx(this, area);

#if 0
	// Check for changes to read-only status and/or file modifications
	CheckForChangesToFile(window);
#endif
	/*  This callback is not only called for focussed panes, but for newly
		created panes as well. So make sure that the cursor is left alone
		for unfocussed panes.
		TextWidget have no state per se about focus, so we use the related
		ID for the blink procedure.  */
    if(QTimer *const blinkTimer = area->cursorBlinkTimer()) {
        if(!blinkTimer->isActive()) {
            //  Start blinking the caret again.
            blinkTimer->start();
        }
    }
}

void DocumentWidget::dragStartCallback(TextArea *area) {
	Q_UNUSED(area);
	// don't record all of the intermediate drag steps for undo
	ignoreModify_ = true;
}

/*
** Keep the marks in the windows book-mark table up to date across
** changes to the underlying buffer
*/
void DocumentWidget::UpdateMarkTable(int pos, int nInserted, int nDeleted) {

    for (int i = 0; i < nMarks_; i++) {
        maintainSelection(&markTable_[i].sel, pos, nInserted, nDeleted);
        maintainPosition(&markTable_[i].cursorPos, pos, nInserted, nDeleted);
    }
}


/*
**
*/
void DocumentWidget::modifiedCallback(int pos, int nInserted, int nDeleted, int nRestyled, view::string_view deletedText) {

    Q_UNUSED(nRestyled);

    bool selected = buffer_->primary_.selected;

    // update the table of bookmarks
    if (!ignoreModify_) {
        UpdateMarkTable(pos, nInserted, nDeleted);
    }

    if(auto win = toWindow()) {
        // Check and dim/undim selection related menu items
        if ((wasSelected_ && !selected) || (!wasSelected_ && selected)) {
            wasSelected_ = selected;

            /* do not refresh shell-level items (window, menu-bar etc)
               when motifying non-top document */
            if (IsTopDocument()) {
                win->ui.action_Print_Selection->setEnabled(selected);
                win->ui.action_Cut->setEnabled(selected);
                win->ui.action_Copy->setEnabled(selected);
                win->ui.action_Delete->setEnabled(selected);

                /* Note we don't change the selection for items like
                   "Open Selected" and "Find Selected".  That's because
                   it works on selections in external applications.
                   Desensitizing it if there's no NEdit selection
                   disables this feature. */

                win->ui.action_Filter_Selection->setEnabled(selected);

    #if 0
                DimSelectionDepUserMenuItems(window, selected);
    #endif
                if(auto dialog = win->getDialogReplace()) {
                    dialog->UpdateReplaceActionButtons();
                }
            }

        }

        /* When the program needs to make a change to a text area without without
           recording it for undo or marking file as changed it sets ignoreModify */
        if (ignoreModify_ || (nDeleted == 0 && nInserted == 0)) {
            return;
        }

        // Make sure line number display is sufficient for new data
        win->updateLineNumDisp();
#if 0
        /* Save information for undoing this operation (this call also counts
           characters and editing operations for triggering autosave */
        window->SaveUndoInformation(pos, nInserted, nDeleted, deletedText);
#endif

        // Trigger automatic backup if operation or character limits reached
        if (autoSave_ && (autoSaveCharCount_ > AUTOSAVE_CHAR_LIMIT || autoSaveOpCount_ > AUTOSAVE_OP_LIMIT)) {
#if 0
            WriteBackupFile(window);
#endif
            autoSaveCharCount_ = 0;
            autoSaveOpCount_ = 0;
        }

        // Indicate that the window has now been modified
        SetWindowModified(true);

        // Update # of bytes, and line and col statistics
        UpdateStatsLine(nullptr);

#if 0
        // Check if external changes have been made to file and warn user
        CheckForChangesToFile(window);
#endif
    }
}

void DocumentWidget::dragEndCallback(TextArea *area, dragEndCBStruct *data) {

    // NOTE(eteran): so, it's a minor shame here that we don't use the area
    // variable. The **buffer** is what reports the modifications, and that is
    // lower level than a TextArea. So it doesn't pass that along to the other
    // versions of the modified callback. The code will eventually call
    // "UpdateStatsLine(nullptr)", which will force it to fall back on the
    // "lastFocus_" variable. Which **probably** points to the correct widget.
    // TODO(eteran): improve this somehow!
    Q_UNUSED(area);

	// restore recording of undo information
	ignoreModify_ = false;

	// Do nothing if drag operation was canceled
    if (data->nCharsInserted == 0) {
		return;
    }

	/* Save information for undoing this operation not saved while
	   undo recording was off */
    modifiedCallback(data->startPos, data->nCharsInserted, data->nCharsDeleted, 0, data->deletedText);
}

void DocumentWidget::smartIndentCallback(TextArea *area, smartIndentCBStruct *data) {

    Q_UNUSED(area);

	if (!smartIndentData_) {
		return;
	}

	switch(data->reason) {
	case CHAR_TYPED:
#if 0
		executeModMacro(window, data);
#endif
		break;
	case NEWLINE_INDENT_NEEDED:
#if 0
		executeNewlineMacro(window, data);
#endif
		break;
	}
}

void DocumentWidget::RaiseDocument() {
#if 0
    // TODO(eteran): implement
#endif
}

/*
** Change the language mode to the one indexed by "mode", reseting word
** delimiters, syntax highlighting and other mode specific parameters
*/
void DocumentWidget::reapplyLanguageMode(int mode, bool forceDefaults) {

    if(auto win = toWindow()) {
        const int oldMode = languageMode_;

        /* If the mode is the same, and changes aren't being forced (as might
           happen with Save As...), don't mess with already correct settings */
        if (languageMode_ == mode && !forceDefaults) {
            return;
        }

        // Change the mode name stored in the window
        languageMode_ = mode;

        // Decref oldMode's default calltips file if needed
        if (oldMode != PLAIN_LANGUAGE_MODE && !LanguageModes[oldMode]->defTipsFile.isNull()) {
            DeleteTagsFileEx(LanguageModes[oldMode]->defTipsFile, TIP, False);
        }

        // Set delimiters for all text widgets
        QString delimiters;
        if (mode == PLAIN_LANGUAGE_MODE || LanguageModes[mode]->delimiters.isNull()) {
            delimiters = GetPrefDelimiters();
        } else {
            delimiters = LanguageModes[mode]->delimiters;
        }

        const QList<TextArea *> textAreas = textPanes();
        for(TextArea *area : textAreas) {
            area->setWordDelimiters(delimiters);
        }

        /* Decide on desired values for language-specific parameters.  If a
           parameter was set to its default value, set it to the new default,
           otherwise, leave it alone */
        bool wrapModeIsDef = wrapMode_ == GetPrefWrap(oldMode);
        bool tabDistIsDef = buffer_->BufGetTabDistance() == GetPrefTabDist(oldMode);

        int oldEmTabDist = textAreas[0]->getEmulateTabs();
        QString oldlanguageModeName = LanguageModeName(oldMode);

        bool emTabDistIsDef   = oldEmTabDist == GetPrefEmTabDist(oldMode);
        bool indentStyleIsDef = indentStyle_ == GetPrefAutoIndent(oldMode)   || (GetPrefAutoIndent(oldMode) == SMART_INDENT && indentStyle_ == AUTO_INDENT && !SmartIndentMacrosAvailable(LanguageModeName(oldMode).toLatin1().data()));
        bool highlightIsDef   = highlightSyntax_ == GetPrefHighlightSyntax() || (GetPrefHighlightSyntax() && FindPatternSet(!oldlanguageModeName.isNull() ? oldlanguageModeName : QLatin1String("")) == nullptr);
        int wrapMode          = wrapModeIsDef                                || forceDefaults ? GetPrefWrap(mode)        : wrapMode_;
        int tabDist           = tabDistIsDef                                 || forceDefaults ? GetPrefTabDist(mode)     : buffer_->BufGetTabDistance();
        int emTabDist         = emTabDistIsDef                               || forceDefaults ? GetPrefEmTabDist(mode)   : oldEmTabDist;
        int indentStyle       = indentStyleIsDef                             || forceDefaults ? GetPrefAutoIndent(mode)  : indentStyle_;
        int highlight         = highlightIsDef                               || forceDefaults ? GetPrefHighlightSyntax() : highlightSyntax_;

        /* Dim/undim smart-indent and highlighting menu items depending on
           whether patterns/macros are available */
        QString languageModeName = LanguageModeName(mode);
        bool haveHighlightPatterns = FindPatternSet(!languageModeName.isNull() ? languageModeName : QLatin1String("")) != nullptr;
        bool haveSmartIndentMacros = SmartIndentMacrosAvailable(LanguageModeName(mode).toLatin1().data());

        if (IsTopDocument()) {
            win->ui.action_Highlight_Syntax->setEnabled(haveHighlightPatterns);
            win->ui.action_Indent_Smart    ->setEnabled(haveSmartIndentMacros);
        }

        // Turn off requested options which are not available
        highlight = haveHighlightPatterns && highlight;
        if (indentStyle == SMART_INDENT && !haveSmartIndentMacros) {
            indentStyle = AUTO_INDENT;
        }


        // Change highlighting
        highlightSyntax_ = highlight;

        win->ui.action_Highlight_Syntax->blockSignals(true);
        win->ui.action_Highlight_Syntax->setChecked(highlight);
        win->ui.action_Highlight_Syntax->blockSignals(false);

        StopHighlighting();
    #if 0
        // we defer highlighting to RaiseDocument() if doc is hidden
        if (IsTopDocument() && highlight) {
            StartHighlighting(window, false);
        }
    #endif

        // Force a change of smart indent macros (SetAutoIndent will re-start)
        if (indentStyle_ == SMART_INDENT) {
    #if 0
            EndSmartIndent(window);
    #endif
            indentStyle_ = AUTO_INDENT;
        }


        // set requested wrap, indent, and tabs
        SetAutoWrap(wrapMode);
        SetAutoIndent(indentStyle);
        SetTabDist(tabDist);
        SetEmTabDist(emTabDist);


        // Load calltips files for new mode
        if (mode != PLAIN_LANGUAGE_MODE && !LanguageModes[mode]->defTipsFile.isNull()) {
            AddTagsFileEx(LanguageModes[mode]->defTipsFile, TIP);
        }

        // Add/remove language specific menu items
        UpdateUserMenus();
    }
}

/*
** Create either the variable Shell menu, Macro menu or Background menu
** items of "window" (driven by value of "menuType")
*/
QMenu *DocumentWidget::createUserMenu(const QVector<MenuData> &data) {

    auto rootMenu = new QMenu(this);
    for(int i = 0; i < data.size(); ++i) {
        const MenuData &menuData = data[i];

        bool found = menuData.info->umiNbrOfLanguageModes == 0;
        for(int language = 0; language < menuData.info->umiNbrOfLanguageModes; ++language) {
            if(menuData.info->umiLanguageMode[language] == languageMode_) {
                found = true;
            }
        }

        if(!found) {
            continue;
        }

        QMenu *parentMenu = rootMenu;
        QString name = QLatin1String(menuData.info->umiName);

        int index = 0;
        for (;;) {
            int subSep = name.indexOf(QLatin1Char('>'), index);
            if(subSep == -1) {
                name = name.mid(index);

                // add the mnemonic to the string in the appropriate place
                int pos = name.indexOf(QLatin1Char(menuData.item->mnemonic));
                if(pos != -1) {
                    name.insert(pos, QLatin1String("&"));
                }

                // create the actual action or, if it represents one of our
                // *very* common entries make it equivalent to the global
                // QAction representing that task
                if(menuData.item->cmd.trimmed() == QLatin1String("cut_clipboard()")) {
                    parentMenu->addAction(toWindow()->ui.action_Cut);
                } else if(menuData.item->cmd.trimmed() == QLatin1String("copy_clipboard()")) {
                    parentMenu->addAction(toWindow()->ui.action_Copy);
                } else if(menuData.item->cmd.trimmed() == QLatin1String("paste_clipboard()")) {
                    parentMenu->addAction(toWindow()->ui.action_Paste);
                } else if(menuData.item->cmd.trimmed() == QLatin1String("undo()")) {
                    parentMenu->addAction(toWindow()->ui.action_Undo);
                } else if(menuData.item->cmd.trimmed() == QLatin1String("redo()")) {
                    parentMenu->addAction(toWindow()->ui.action_Redo);
                } else {
                    QAction *action = parentMenu->addAction(name);
                    action->setData(i);
                }

                break;
            }

            QString parentName = name.mid(index, subSep);
            int subSubSep = parentName.indexOf(QLatin1Char('>'));
            if(subSubSep != -1) {
                parentName = parentName.mid(0, subSubSep);
            }

            QList<QAction*> actions = parentMenu->actions();
            QAction *parentAction = nullptr;
            for(QAction *action : actions) {
                if(action->text() == parentName) {
                    parentAction = action;
                    break;
                }
            }

            if(!parentAction) {
                auto newMenu = new QMenu(parentName, this);
                parentMenu->addMenu(newMenu);
                parentMenu = newMenu;
            } else {
                parentMenu = parentAction->menu();
            }

            index = subSep + 1;
        }
    }
    return rootMenu;
}

/*
** Update the Shell, Macro, and Window Background menus of window
** "window" from the currently loaded command descriptions.
*/
void DocumentWidget::UpdateUserMenus() {

    if (!IsTopDocument()) {
        return;
    }

    // TODO(eteran): the old code used to only do this if the language mode changed
    //               we should probably restore that behavior
    if(auto win = toWindow()) {
        /* update user menus, which are shared over all documents, only
           if language mode was changed */
        auto shellMenu = createUserMenu(ShellMenuData);
        win->ui.menu_Shell->clear();
        win->ui.menu_Shell->addAction(win->ui.action_Execute_Command);
        win->ui.menu_Shell->addAction(win->ui.action_Execute_Command_Line);
        win->ui.menu_Shell->addAction(win->ui.action_Filter_Selection);
        win->ui.menu_Shell->addAction(win->ui.action_Cancel_Shell_Command);
        win->ui.menu_Shell->addSeparator();
        win->ui.menu_Shell->addActions(shellMenu->actions());

        auto macroMenu = createUserMenu(MacroMenuData);
        win->ui.menu_Macro->clear();
        win->ui.menu_Macro->addAction(win->ui.action_Learn_Keystrokes);
        win->ui.menu_Macro->addAction(win->ui.action_Finish_Learn);
        win->ui.menu_Macro->addAction(win->ui.action_Cancel_Learn);
        win->ui.menu_Macro->addAction(win->ui.action_Replay_Keystrokes);
        win->ui.menu_Macro->addAction(win->ui.action_Repeat);
        win->ui.menu_Macro->addSeparator();
        win->ui.menu_Macro->addActions(macroMenu->actions());

        /* update background menu, which is owned by a single document, only
           if language mode was changed */
        auto bgMenu = createUserMenu(BGMenuData);
        const QList<TextArea *> textAreas = textPanes();
        for(TextArea *area : textAreas) {
            area->setContextMenu(bgMenu);
        }
    }
}

/*
**
*/
void DocumentWidget::SetTabDist(int tabDist) {
    if (buffer_->tabDist_ != tabDist) {
        int saveCursorPositions[MAX_PANES + 1];
        int saveVScrollPositions[MAX_PANES + 1];
        int saveHScrollPositions[MAX_PANES + 1];

        ignoreModify_ = true;

        const QList<TextArea *> textAreas = textPanes();
        for(int paneIndex = 0; paneIndex < textAreas.size(); ++paneIndex) {
            TextArea *area = textAreas[paneIndex];

            area->TextDGetScroll(&saveVScrollPositions[paneIndex], &saveHScrollPositions[paneIndex]);
            saveCursorPositions[paneIndex] = area->TextGetCursorPos();
            area->setModifyingTabDist(1);
        }

        buffer_->BufSetTabDistance(tabDist);

        for(int paneIndex = 0; paneIndex < textAreas.size(); ++paneIndex) {
            TextArea *area = textAreas[paneIndex];

            area->setModifyingTabDist(0);
            area->TextSetCursorPos(saveCursorPositions[paneIndex]);
            area->TextDSetScroll(saveVScrollPositions[paneIndex], saveHScrollPositions[paneIndex]);
        }

        ignoreModify_ = false;
    }
}

/*
**
*/
void DocumentWidget::SetEmTabDist(int emTabDist) {

    const QList<TextArea *> textAreas = textPanes();
    for(TextArea *area : textAreas) {
        area->setEmulateTabs(emTabDist);
    }
}

/*
** Set autoindent state to one of  NO_AUTO_INDENT, AUTO_INDENT, or SMART_INDENT.
*/
// TODO(eteran): make state an enum
void DocumentWidget::SetAutoIndent(int state) {
    bool autoIndent  = (state == AUTO_INDENT);
    bool smartIndent = (state == SMART_INDENT);

    if (indentStyle_ == SMART_INDENT && !smartIndent) {
#if 0
        EndSmartIndent(this);
#endif
    } else if (smartIndent && indentStyle_ != SMART_INDENT) {
#if 0
        BeginSmartIndent(this, true);
#endif
    }

    indentStyle_ = state;

    const QList<TextArea *> textAreas = textPanes();
    for(TextArea *area : textAreas) {
        area->setAutoIndent(smartIndent);
        area->setSmartIndent(autoIndent);
    }

    if (IsTopDocument()) {
        if(auto win = toWindow()) {
            win->ui.action_Indent_Smart->blockSignals(true);
            win->ui.action_Indent_Smart->setChecked(smartIndent);
            win->ui.action_Indent_Smart->blockSignals(false);

            win->ui.action_Indent_On->blockSignals(true);
            win->ui.action_Indent_On->setChecked(autoIndent);
            win->ui.action_Indent_On->blockSignals(false);

            win->ui.action_Indent_Off->blockSignals(true);
            win->ui.action_Indent_Off->setChecked(state == NO_AUTO_INDENT);
            win->ui.action_Indent_Off->blockSignals(false);
        }
    }
}

/*
** Select auto-wrap mode, one of NO_WRAP, NEWLINE_WRAP, or CONTINUOUS_WRAP
*/
void DocumentWidget::SetAutoWrap(int state) {
    const bool autoWrap = (state == NEWLINE_WRAP);
    const bool contWrap = (state == CONTINUOUS_WRAP);

    const QList<TextArea *> textAreas = textPanes();
    for(TextArea *area : textAreas) {
        area->setAutoWrap(autoWrap);
        area->setContinuousWrap(contWrap);
    }

    wrapMode_ = state;

    if (IsTopDocument()) {
        if(auto win = toWindow()) {
            win->ui.action_Wrap_Auto_Newline->blockSignals(true);
            win->ui.action_Wrap_Auto_Newline->setChecked(autoWrap);
            win->ui.action_Wrap_Auto_Newline->blockSignals(false);

            win->ui.action_Wrap_Continuous->blockSignals(true);
            win->ui.action_Wrap_Continuous->setChecked(contWrap);
            win->ui.action_Wrap_Continuous->blockSignals(false);

            win->ui.action_Wrap_None->blockSignals(true);
            win->ui.action_Wrap_None->setChecked(state == NO_WRAP);
            win->ui.action_Wrap_None->blockSignals(false);
        }
    }
}

QList<TextArea *> DocumentWidget::textPanes() const {

    QList<TextArea *> list;
    list.reserve(splitter_->count());

    for(int i = 0; i < splitter_->count(); ++i) {
        if(auto area = qobject_cast<TextArea *>(splitter_->widget(i))) {
            list.push_back(area);
        }
    }

    return list;
}

bool DocumentWidget::IsTopDocument() const {
    if(auto win = toWindow()) {
        return win->ui.tabWidget->currentWidget() == this;
    }

    return false;
}

/*
** Update the Window menu of a single window to reflect the current state of
** all NEdit windows as determined by the global WindowList.
*/
void DocumentWidget::updateWindowMenu() {

    if (!IsTopDocument()) {
        return;
    }

    if(auto win = toWindow()) {

        // Make a sorted list of windows
        QList<DocumentWidget *> documents = win->allDocuments();

        std::sort(documents.begin(), documents.end(), [](const DocumentWidget *a, const DocumentWidget *b) {

            // Untitled first
            int rc = a->filenameSet_ == b->filenameSet_ ? 0 : a->filenameSet_ && !b->filenameSet_ ? 1 : -1;
            if (rc != 0) {
                return rc < 0;
            }

            if(a->filename_ < b->filename_) {
                return true;
            }

            return a->path_ < b->path_;
        });

        win->ui.menu_Windows->clear();
        win->ui.menu_Windows->addAction(win->ui.action_Split_Pane);
        win->ui.menu_Windows->addAction(win->ui.action_Close_Pane);
        win->ui.menu_Windows->addSeparator();
        win->ui.menu_Windows->addAction(win->ui.action_Detach_Tab);
        win->ui.menu_Windows->addAction(win->ui.action_Move_Tab_To);
        win->ui.menu_Windows->addSeparator();

        for(DocumentWidget *document : documents) {
            QString title = document->getWindowsMenuEntry();
            QAction *action = win->ui.menu_Windows->addAction(title, win, SLOT(raiseCB()));
            action->setData(reinterpret_cast<qulonglong>(document));
        }
    }
}

QString DocumentWidget::getWindowsMenuEntry() {

    QString fullTitle = tr("%1%2").arg(filename_).arg(fileChanged_ ? tr("*") : tr(""));

    if (GetPrefShowPathInWindowsMenu() && filenameSet_) {
        fullTitle.append(tr(" - "));
        fullTitle.append(path_);
    }

    return fullTitle;
}

void DocumentWidget::setLanguageMode(const QString &mode) {

    // TODO(eteran): this is inefficient, we started with the mode number
    //               converted it to a string, and now we look up the number
    //               again to pass to this function. We should just pass the
    //               number and skip the round trip
    SetLanguageMode(FindLanguageMode(mode.toLatin1().data()), false);
}


/*
** Turn off syntax highlighting and free style buffer, compiled patterns, and
** related data.
*/
void DocumentWidget::StopHighlighting() {
    if (!highlightData_) {
        return;
    }

    if(TextArea *area = firstPane()) {

        /* Get the line height being used by the highlight fonts in the window,
           to be used after highlighting is turned off to resize the window
           back to the line height of the primary font */
        int oldFontHeight = area->getFontHeight();

        // Free and remove the highlight data from the window
        freeHighlightData(static_cast<WindowHighlightData *>(highlightData_));
        highlightData_ = nullptr;

        /* Remove and detach style buffer and style table from all text
           display(s) of window, and redisplay without highlighting */
        const QList<TextArea *> textAreas = textPanes();
        for(TextArea *area : textAreas) {
            RemoveWidgetHighlightEx(area);
        }

#if 0
        /* Re-size the window to fit the primary font properly & tell the window
           manager about the potential line-height change as well */
        updateWindowHeight(window, oldFontHeight);
        window->UpdateWMSizeHints();
        window->UpdateMinPaneHeights();
    #endif
    }
}

TextArea *DocumentWidget::firstPane() const {
    const QList<TextArea *> textAreas = textPanes();
    if(!textAreas.isEmpty()) {
        return textAreas[0];
    }

    return nullptr;
}

/*
** Free allocated memory associated with highlight data, including compiled
** regular expressions, style buffer and style table.  Note: be sure to
** nullptr out the widget references to the objects in this structure before
** calling this.  Because of the slow, multi-phase destruction of
** widgets, this data can be referenced even AFTER destroying the widget.
*/
void DocumentWidget::freeHighlightData(WindowHighlightData *hd) {

    if(hd) {
        freePatterns(hd->pass1Patterns);
        freePatterns(hd->pass2Patterns);
        delete hd->styleBuffer;
        delete[] hd->styleTable;
        delete hd;
    }
}

/*
** Free a pattern list and all of its allocated components
*/
void DocumentWidget::freePatterns(HighlightData *patterns) {

    if(patterns) {
        for (int i = 0; patterns[i].style != 0; i++) {
            delete patterns[i].startRE;
            delete patterns[i].endRE;
            delete patterns[i].errorRE;
            delete patterns[i].subPatternRE;
        }

        for (int i = 0; patterns[i].style != 0; i++) {
            delete [] patterns[i].subPatterns;
        }

        delete [] patterns;
    }
}

/*
** Get the set of word delimiters for the language mode set in the current
** window.  Returns nullptr when no language mode is set (it would be easy to
** return the default delimiter set when the current language mode is "Plain",
** or the mode doesn't have its own delimiters, but this is usually used
** to supply delimiters for RE searching, and ExecRE can skip compiling a
** delimiter table when delimiters is nullptr).
*/
QString DocumentWidget::GetWindowDelimiters() const {
    if (languageMode_ == PLAIN_LANGUAGE_MODE)
        return QString();
    else
        return LanguageModes[languageMode_]->delimiters;
}

void DocumentWidget::flashTimerTimeout() {
    eraseFlashEx(this);
}
