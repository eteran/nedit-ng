/*******************************************************************************
*                                                                              *
* search.c -- Nirvana Editor search and replace functions                      *
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

#include "search.h"
#include "utils.h"
#include "DialogFind.h"
#include "DialogMultiReplace.h"
#include "DialogReplace.h"
#include "DocumentWidget.h"
#include "MainWindow.h"
#include "TruncSubstitution.h"
#include "SignalBlocker.h"
#include "TextArea.h"
#include "TextBuffer.h"
#include "WrapStyle.h"
#include "highlight.h"
#include "preferences.h"
#include "regularExp.h"
#include "selection.h"
#include "userCmds.h"

#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QMimeData>
#include <QTimer>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

int NHist = 0;


// History mechanism for search and replace strings 
SearchReplaceHistoryEntry SearchReplaceHistory[MAX_SEARCH_HISTORY];
static int HistStart = 0;

static bool SearchString(view::string_view string, const QString &searchString, Direction direction, SearchType searchType, WrapMode wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters);

static bool backwardRegexSearch(view::string_view string, view::string_view searchString, WrapMode wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters, int defaultFlags);
static bool replaceUsingREEx(view::string_view searchStr, const char *replaceStr, view::string_view sourceStr, int beginPos, std::string &dest, int prevChar, const char *delimiters, int defaultFlags);
static bool replaceUsingREEx(const QString &searchStr, const QString &replaceStr, view::string_view sourceStr, int beginPos, std::string &dest, int prevChar, const QString &delimiters, int defaultFlags);

static bool forwardRegexSearch(view::string_view string, view::string_view searchString, WrapMode wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters, int defaultFlags);
static bool searchRegex(view::string_view string, view::string_view searchString, Direction direction, WrapMode wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters, int defaultFlags);
static int countWindows(void);
static bool searchLiteral(view::string_view string, view::string_view searchString, bool caseSense, Direction direction, WrapMode wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW);
static bool searchLiteralWord(view::string_view string, view::string_view searchString, bool caseSense, Direction direction, WrapMode wrap, int beginPos, int *startPos, int *endPos, const char *delimiters);
static bool searchMatchesSelectionEx(DocumentWidget *window, const QString &searchString, SearchType searchType, int *left, int *right, int *searchExtentBW, int *searchExtentFW);
static void iSearchRecordLastBeginPosEx(MainWindow *window, Direction direction, int initPos);
static void iSearchTryBeepOnWrapEx(MainWindow *window, Direction direction, int beginPos, int startPos);
static std::string upCaseStringEx(view::string_view inString);
static std::string downCaseStringEx(view::string_view inString);

#if defined(REPLACE_SCOPE)
/*
** Checks whether a selection spans multiple lines. Used to decide on the
** default scope for replace dialogs.
** This routine introduces a dependency on TextDisplay.h, which is not so nice,
** but I currently don't have a cleaner solution.
*/
static bool selectionSpansMultipleLines(DocumentWidget *document) {
	int selStart;
	int selEnd;
	int rectStart;
	int rectEnd;
	int lineStartStart;
	int lineStartEnd;
	bool isRect;
	int lineWidth;

    if (!document->buffer_->BufGetSelectionPos(&selStart, &selEnd, &isRect, &rectStart, &rectEnd)) {
        return false;
	}

	/* This is kind of tricky. The perception of a line depends on the
	   line wrap mode being used. So in theory, we should take into
	   account the layout of the text on the screen. However, the
	   routine to calculate a line number for a given character position
	   (TextDPosToLineAndCol) only works for displayed lines, so we cannot
	   use it. Therefore, we use this simple heuristic:
	    - If a newline is found between the start and end of the selection,
	  we obviously have a multi-line selection.
	- If no newline is found, but the distance between the start and the
	      end of the selection is larger than the number of characters
	  displayed on a line, and we're in continuous wrap mode,
	  we also assume a multi-line selection.
	*/

    lineStartStart = document->buffer_->BufStartOfLine(selStart);
    lineStartEnd = document->buffer_->BufStartOfLine(selEnd);
	// If the line starts differ, we have a "\n" in between. 
	if (lineStartStart != lineStartEnd) {
		return true;
	}

    if (document->wrapMode_ != Continuous) {
        return false; // Same line
	}

	// Estimate the number of characters on a line 
    TextArea *area = document->firstPane();

    int maxWidth = area->TextDMaxFontWidth(false);

    if (maxWidth > 0) {
        lineWidth = area->getRect().width() / maxWidth;
	} else {
		lineWidth = 1;
	}

	if (lineWidth < 1) {
		lineWidth = 1; // Just in case 
	}

	/* Estimate the numbers of line breaks from the start of the line to
	   the start and ending positions of the selection and compare.*/
	if ((selStart - lineStartStart) / lineWidth != (selEnd - lineStartStart) / lineWidth) {
		return true; // Spans multiple lines
	}

    return false; // Small selection; probably doesn't span lines
}
#endif

void DoFindReplaceDlogEx(MainWindow *window, DocumentWidget *document, TextArea *area, Direction direction, bool keepDialogs, SearchType searchType) {

    Q_UNUSED(area);

    // Create the dialog if it doesn't already exist
    if (!window->dialogReplace_) {
        window->dialogReplace_ = new DialogReplace(window, document, window);
    }

    auto dialog = window->getDialogReplace();

    dialog->setTextField(document);

    // If the window is already up, just pop it to the top
    if(dialog->isVisible()) {
        dialog->raise();
        dialog->activateWindow();
        return;
    }

    // Blank the Replace with field
    dialog->ui.textReplace->setText(QString());

    // Set the initial search type
    dialog->initToggleButtons(searchType);

    // Set the initial direction based on the direction argument
    dialog->ui.checkBackward->setChecked(direction == Direction::FORWARD ? false : true);

    // Set the state of the Keep Dialog Up button
    dialog->ui.checkKeep->setChecked(keepDialogs);

#if defined(REPLACE_SCOPE)

    if (window->wasSelected_) {
        // If a selection exists, the default scope depends on the preference
        // of the user.
        switch (GetPrefReplaceDefScope()) {
        case REPL_DEF_SCOPE_SELECTION:
            // The user prefers selection scope, no matter what the size of
            // the selection is.
            dialog->ui.radioSelection->setChecked(true);
            break;
        case REPL_DEF_SCOPE_SMART:
            if (selectionSpansMultipleLines(document)) {
                /* If the selection spans multiple lines, the user most
                   likely wants to perform a replacement in the selection */
                dialog->ui.radioSelection->setChecked(true);
            } else {
                /* It's unlikely that the user wants a replacement in a
                   tiny selection only. */
                dialog->ui.radioWindow->setChecked(true);
            }
            break;
        default:
            // The user always wants window scope as default.
            dialog->ui.radioWindow->setChecked(true);
            break;
        }
    } else {
        // No selection -> always choose "In Window" as default.
        dialog->ui.radioWindow->setChecked(true);
    }
#endif

    dialog->UpdateReplaceActionButtons();

    // Start the search history mechanism at the current history item
    window->rHistIndex_ = 0;

    dialog->show();
}

void DoFindDlogEx(MainWindow *window, DocumentWidget *document, Direction direction, bool keepDialogs, SearchType searchType) {

    if(!window->dialogFind_) {
        window->dialogFind_ = new DialogFind(window, document, window);
    }

    auto dialog = qobject_cast<DialogFind *>(window->dialogFind_);

    dialog->setTextField(document);

    if(dialog->isVisible()) {
        dialog->raise();
        dialog->activateWindow();
        return;
    }

    // Set the initial search type
    dialog->initToggleButtons(searchType);

    // Set the initial direction based on the direction argument
    dialog->ui.checkBackward->setChecked(direction == Direction::FORWARD ? false : true);

    // Set the state of the Keep Dialog Up button
    dialog->ui.checkKeep->setChecked(keepDialogs);

    // Set the state of the Find button
    dialog->fUpdateActionButtons();

    // start the search history mechanism at the current history item
    window->fHistIndex_ = 0;

    dialog->show();
}

/*
** Count no. of windows
*/
static int countWindows() {
    return DocumentWidget::allDocuments().size();
}

/*
** Count no. of writable windows, but first update the status of all files.
*/
int countWritableWindows() {

	int nBefore = countWindows();
    int nWritable = 0;

    QList<DocumentWidget *> documents = DocumentWidget::allDocuments();
    auto first = documents.begin();
    auto last = documents.end();

    for(auto it = first; it != last; ++it) {
        DocumentWidget *document = *it;

		/* We must be very careful! The status check may trigger a pop-up
		   dialog when the file has changed on disk, and the user may destroy
		   arbitrary windows in response. */
        document->CheckForChangesToFileEx();
		int nAfter = countWindows();

		if (nAfter != nBefore) {
			// The user has destroyed a file; start counting all over again 
			nBefore = nAfter;

            documents = DocumentWidget::allDocuments();
            first = documents.begin();
            last = documents.end();
            it = first;
			nWritable = 0;
			continue;
		}
		
        if (!document->lockReasons_.isAnyLocked()) {
			++nWritable;
		}
	}

	return nWritable;
}

/*
** Fetch and verify (particularly regular expression) search string,
** direction, and search type from the Find dialog.  If the search string
** is ok, save a copy in the search history, copy it to "searchString",
** which is assumed to be at least SEARCHMAX in length, return search type
** in "searchType", and return TRUE as the function value.  Otherwise,
** return FALSE.
*/
bool SearchAndSelectSameEx(MainWindow *window, DocumentWidget *document, TextArea *area, Direction direction, WrapMode searchWrap) {
    if (NHist < 1) {
        QApplication::beep();
        return false;
    }

    return SearchAndSelectEx(
                window,
                document,
                area,
                direction,
                SearchReplaceHistory[historyIndex(1)].search,
                SearchReplaceHistory[historyIndex(1)].type,
                searchWrap);
}

/*
** Search for "searchString" in "window", and select the matching text in
** the window when found (or beep or put up a dialog if not found).  Also
** adds the search string to the global search history.
*/
bool SearchAndSelectEx(MainWindow *window, DocumentWidget *document, TextArea *area, Direction direction, const QString &searchString, SearchType searchType, WrapMode searchWrap) {
    int startPos;
    int endPos;
    int beginPos;
    int selStart;
    int selEnd;
    int movedFwd = 0;

    // Save a copy of searchString in the search history
    saveSearchHistory(searchString, QString(), searchType, false);

    /* set the position to start the search so we don't find the same
       string that was found on the last search	*/
    if (searchMatchesSelectionEx(document, searchString, searchType, &selStart, &selEnd, nullptr, nullptr)) {
        // selection matches search string, start before or after sel.
        if (direction == Direction::BACKWARD) {
            beginPos = selStart - 1;
        } else {
            beginPos = selStart + 1;
            movedFwd = 1;
        }
    } else {
        selStart = -1;
        selEnd = -1;
        // no selection, or no match, search relative cursor

        int cursorPos = area->TextGetCursorPos();
        if (direction == Direction::BACKWARD) {
            // use the insert position - 1 for backward searches
            beginPos = cursorPos - 1;
        } else {
            // use the insert position for forward searches
            beginPos = cursorPos;
        }
    }

    /* when the i-search bar is active and search is repeated there
       (Return), the action "find" is called (not: "find_incremental").
       "find" calls this function SearchAndSelect.
       To keep track of the iSearchLastBeginPos correctly in the
       repeated i-search case it is necessary to call the following
       function here, otherwise there are no beeps on the repeated
       incremental search wraps.  */
    iSearchRecordLastBeginPosEx(window, direction, beginPos);

    // do the search.  SearchWindow does appropriate dialogs and beeps
    if (!SearchWindowEx(window, document, direction, searchString, searchType, searchWrap, beginPos, &startPos, &endPos, nullptr, nullptr))
        return false;

    /* if the search matched an empty string (possible with regular exps)
       beginning at the start of the search, go to the next occurrence,
       otherwise repeated finds will get "stuck" at zero-length matches */
    if (direction == Direction::FORWARD && beginPos == startPos && beginPos == endPos) {
        if (!movedFwd && !SearchWindowEx(window, document, direction, searchString, searchType, searchWrap, beginPos + 1, &startPos, &endPos, nullptr, nullptr))
            return false;
    }

    // if matched text is already selected, just beep
    if (selStart == startPos && selEnd == endPos) {
        QApplication::beep();
        return false;
    }

    // select the text found string
    document->buffer_->BufSelect(startPos, endPos);
    document->MakeSelectionVisible(area);

    area->TextSetCursorPos(endPos);

    return true;
}

void SearchForSelectedEx(MainWindow *window, DocumentWidget *document, TextArea *area, Direction direction, SearchType searchType, WrapMode searchWrap) {

    // skip if we can't get the selection data or it's too long
    // should be of type text???
    const QMimeData *mimeData = QApplication::clipboard()->mimeData(QClipboard::Selection);
    if(!mimeData->hasText()) {
        if (GetPrefSearchDlogs()) {
            QMessageBox::warning(document, QLatin1String("Wrong Selection"), QLatin1String("Selection not appropriate for searching"));
        } else {
            QApplication::beep();
        }
        return;
    }

    // make the selection the current search string
    QString searchString = mimeData->text();

    if (searchString.size() > SEARCHMAX) {
        if (GetPrefSearchDlogs()) {
            QMessageBox::warning(document, QLatin1String("Selection too long"), QLatin1String("Selection too long"));
        } else {
            QApplication::beep();
        }
        return;
    }

    if (searchString.isEmpty()) {
        QApplication::beep();
        return;
    }

    /* Use the passed method for searching, unless it is regex, since this
       kind of search is by definition a literal search */
    if (searchType == SearchType::Regex) {
        searchType = SearchType::CaseSense;
    } else if (searchType == SearchType::RegexNoCase) {
        searchType = SearchType::Literal;
    }

    // search for it in the window
    SearchAndSelectEx(
                window,
                document,
                area,
                direction,
                searchString,
                searchType,
                searchWrap);
}

/*
** Reset window->iSearchLastBeginPos_ to the resulting initial
** search begin position for incremental searches.
*/
static void iSearchRecordLastBeginPosEx(MainWindow *window, Direction direction, int initPos) {
    window->iSearchLastBeginPos_ = initPos;
    if (direction == Direction::BACKWARD)
        window->iSearchLastBeginPos_--;
}

/*
** Search for "searchString" in "window", and select the matching text in
** the window when found (or beep or put up a dialog if not found).  If
** "continued" is TRUE and a prior incremental search starting position is
** recorded, search from that original position, otherwise, search from the
** current cursor position.
*/
bool SearchAndSelectIncrementalEx(MainWindow *window, DocumentWidget *document, TextArea *area, Direction direction, const QString &searchString, SearchType searchType, WrapMode searchWrap, bool continued) {
    int beginPos;
    int startPos;
    int endPos;

    /* If there's a search in progress, start the search from the original
       starting position, otherwise search from the cursor position. */
    if (!continued || window->iSearchStartPos_ == -1) {
        window->iSearchStartPos_ = area->TextGetCursorPos();
        iSearchRecordLastBeginPosEx(window, direction, window->iSearchStartPos_);
    }

    beginPos = window->iSearchStartPos_;

    /* If the search string is empty, beep eventually if text wrapped
       back to the initial position, re-init iSearchLastBeginPos,
       clear the selection, set the cursor back to what would be the
       beginning of the search, and return. */
    if (searchString.isEmpty()) {
        int beepBeginPos = (direction == Direction::BACKWARD) ? beginPos - 1 : beginPos;
        iSearchTryBeepOnWrapEx(window, direction, beepBeginPos, beepBeginPos);
        iSearchRecordLastBeginPosEx(window, direction, window->iSearchStartPos_);
        document->buffer_->BufUnselect();

        area->TextSetCursorPos(beginPos);
        return true;
    }

    /* Save the string in the search history, unless we're cycling thru
       the search history itself, which can be detected by matching the
       search string with the search string of the current history index. */
    if (!(window->iSearchHistIndex_ > 1 && (searchString == SearchReplaceHistory[historyIndex(window->iSearchHistIndex_)].search))) {
        saveSearchHistory(searchString, QString(), searchType, true);
        // Reset the incremental search history pointer to the beginning
        window->iSearchHistIndex_ = 1;
    }

    // begin at insert position - 1 for backward searches
    if (direction == Direction::BACKWARD)
        beginPos--;

    // do the search.  SearchWindow does appropriate dialogs and beeps
    if (!SearchWindowEx(window, document, direction, searchString, searchType, searchWrap, beginPos, &startPos, &endPos, nullptr, nullptr))
        return false;

    window->iSearchLastBeginPos_ = startPos;

    /* if the search matched an empty string (possible with regular exps)
       beginning at the start of the search, go to the next occurrence,
       otherwise repeated finds will get "stuck" at zero-length matches */
    if (direction == Direction::FORWARD && beginPos == startPos && beginPos == endPos)
        if (!SearchWindowEx(window, document, direction, searchString, searchType, searchWrap, beginPos + 1, &startPos, &endPos, nullptr, nullptr))
            return false;

    window->iSearchLastBeginPos_ = startPos;

    // select the text found string
    document->buffer_->BufSelect(startPos, endPos);
    document->MakeSelectionVisible(area);

    area->TextSetCursorPos(endPos);

    return true;
}

/*
** Search and replace using previously entered search strings (from dialog
** or selection).
*/
bool ReplaceSameEx(MainWindow *window, DocumentWidget *document, TextArea *area, Direction direction, WrapMode searchWrap) {
    if (NHist < 1) {
        QApplication::beep();
        return false;
    }

    return SearchAndReplaceEx(
                window,
                document,
                area,
                direction,
                SearchReplaceHistory[historyIndex(1)].search,
                SearchReplaceHistory[historyIndex(1)].replace,
                SearchReplaceHistory[historyIndex(1)].type,
                searchWrap);
}

/*
** Search and replace using previously entered search strings (from dialog
** or selection).
*/
bool ReplaceFindSameEx(MainWindow *window, DocumentWidget *document, TextArea *area, Direction direction, WrapMode searchWrap) {
    if (NHist < 1) {
        QApplication::beep();
        return false;
    }

    return ReplaceAndSearchEx(
                window,
                document,
                area,
                direction,
                SearchReplaceHistory[historyIndex(1)].search,
                SearchReplaceHistory[historyIndex(1)].replace,
                SearchReplaceHistory[historyIndex(1)].type,
                searchWrap);
}

/*
** Replace selection with "replaceString" and search for string "searchString" in window "window",
** using algorithm "searchType" and direction "direction"
*/
bool ReplaceAndSearchEx(MainWindow *window, DocumentWidget *document, TextArea *area, Direction direction, const QString &searchString, const QString &replaceString, SearchType searchType, WrapMode searchWrap) {
    int startPos = 0;
    int endPos = 0;    
    int searchExtentBW;
    int searchExtentFW;

    // Save a copy of search and replace strings in the search history
    saveSearchHistory(searchString, replaceString, searchType, false);

    bool replaced = false;

    // Replace the selected text only if it matches the search string
    if (searchMatchesSelectionEx(document, searchString, searchType, &startPos, &endPos, &searchExtentBW, &searchExtentFW)) {

        int replaceLen = 0;

        // replace the text
        if (isRegexType(searchType)) {
            std::string replaceResult;
            const std::string foundString = document->buffer_->BufGetRangeEx(searchExtentBW, searchExtentFW + 1);

            replaceUsingREEx(
                searchString,
                replaceString,
                foundString,
                startPos - searchExtentBW,
                replaceResult,
                startPos == 0 ? '\0' : document->buffer_->BufGetCharacter(startPos - 1),
                GetWindowDelimitersEx(document),
                defaultRegexFlags(searchType));

            document->buffer_->BufReplaceEx(startPos, endPos, replaceResult);
            replaceLen = replaceResult.size();
        } else {
            document->buffer_->BufReplaceEx(startPos, endPos, replaceString.toLatin1().data());
            replaceLen = replaceString.size();
        }

        // Position the cursor so the next search will work correctly based
        // on the direction of the search
        area->TextSetCursorPos(startPos + ((direction == Direction::FORWARD) ? replaceLen : 0));
        replaced = true;
    }

    // do the search; beeps/dialogs are taken care of
    SearchAndSelectEx(window, document, area, direction, searchString, searchType, searchWrap);
    return replaced;
}

/*
** Search for string "searchString" in window "window", using algorithm
** "searchType" and direction "direction", and replace it with "replaceString"
** Also adds the search and replace strings to the global search history.
*/
bool SearchAndReplaceEx(MainWindow *window, DocumentWidget *document, TextArea *area, Direction direction, const QString &searchString, const QString &replaceString, SearchType searchType, WrapMode searchWrap) {
    int startPos;
    int endPos;
    int replaceLen;
    int searchExtentBW;
    int searchExtentFW;

    // Save a copy of search and replace strings in the search history
    saveSearchHistory(searchString, replaceString, searchType, false);

    // If the text selected in the window matches the search string,
    // the user is probably using search then replace method, so
    // replace the selected text regardless of where the cursor is.
    // Otherwise, search for the string.
    if (!searchMatchesSelectionEx(document, searchString, searchType, &startPos, &endPos, &searchExtentBW, &searchExtentFW)) {
        // get the position to start the search

        int beginPos;
        int cursorPos = area->TextGetCursorPos();
        if (direction == Direction::BACKWARD) {
            // use the insert position - 1 for backward searches
            beginPos = cursorPos - 1;
        } else {
            // use the insert position for forward searches
            beginPos = cursorPos;
        }

        // do the search
        bool found = SearchWindowEx(window, document, direction, searchString, searchType, searchWrap, beginPos, &startPos, &endPos, &searchExtentBW, &searchExtentFW);
        if (!found)
            return false;
    }

    // replace the text
    if (isRegexType(searchType)) {
        std::string replaceResult;
        const std::string foundString = document->buffer_->BufGetRangeEx(searchExtentBW, searchExtentFW + 1);

        QString delimieters = GetWindowDelimitersEx(document);

        replaceUsingREEx(
            searchString,
            replaceString,
            foundString,
            startPos - searchExtentBW,
            replaceResult,
            startPos == 0 ? '\0' : document->buffer_->BufGetCharacter(startPos - 1),
            delimieters,
            defaultRegexFlags(searchType));

        document->buffer_->BufReplaceEx(startPos, endPos, replaceResult);
        replaceLen = replaceResult.size();
    } else {
        document->buffer_->BufReplaceEx(startPos, endPos, replaceString.toLatin1().data());
        replaceLen = replaceString.size();
    }

    /* after successfully completing a replace, selected text attracts
       attention away from the area of the replacement, particularly
       when the selection represents a previous search. so deselect */
    document->buffer_->BufUnselect();

    /* temporarily shut off autoShowInsertPos before setting the cursor
       position so MakeSelectionVisible gets a chance to place the replaced
       string at a pleasing position on the screen (otherwise, the cursor would
       be automatically scrolled on screen and MakeSelectionVisible would do
       nothing) */
    area->setAutoShowInsertPos(false);

    area->TextSetCursorPos(startPos + ((direction == Direction::FORWARD) ? replaceLen : 0));
    document->MakeSelectionVisible(area);
    area->setAutoShowInsertPos(true);

    return true;
}

/*
**  Uses the resource nedit.truncSubstitution to determine how to deal with
**  regex failures. This function only knows about the resource (via the usual
**  setting getter) and asks or warns the user depending on that.
**
**  One could argue that the dialoging should be determined by the setting
**  'searchDlogs'. However, the incomplete substitution is not just a question
**  of verbosity, but of data loss. The search is successful, only the
**  replacement fails due to an internal limitation of NEdit.
**
**  The parameters 'parent' and 'display' are only used to put dialogs and
**  beeps at the right place.
**
**  The result is either predetermined by the resource or the user's choice.
*/
static bool prefOrUserCancelsSubstEx(MainWindow *window, DocumentWidget *document) {

    Q_UNUSED(window);

    bool cancel = true;

    switch (GetPrefTruncSubstitution()) {
    case TruncSubstitution::Silent:
        //  silently fail the operation
        cancel = true;
        break;

    case TruncSubstitution::Fail:
        //  fail the operation and pop up a dialog informing the user
        QApplication::beep();

        QMessageBox::information(document, QLatin1String("Substitution Failed"), QLatin1String("The result length of the substitution exceeded an internal limit.\n"
                                                                                                           "The substitution is canceled."));

        cancel = true;
        break;

    case TruncSubstitution::Warn:
        //  pop up dialog and ask for confirmation
        QApplication::beep();

        {
            QMessageBox messageBox(document);
            messageBox.setWindowTitle(QLatin1String("Substitution Failed"));
            messageBox.setIcon(QMessageBox::Warning);
            messageBox.setText(QLatin1String("The result length of the substitution exceeded an internal limit.\nExecuting the substitution will result in loss of data."));
            QPushButton *buttonLose   = messageBox.addButton(QLatin1String("Lose Data"), QMessageBox::AcceptRole);
            QPushButton *buttonCancel = messageBox.addButton(QMessageBox::Cancel);
            Q_UNUSED(buttonLose);

            messageBox.exec();
            cancel = (messageBox.clickedButton() == buttonCancel);
        }
        break;

    case TruncSubstitution::Ignore:
        //  silently conclude the operation; THIS WILL DESTROY DATA.
        cancel = false;
        break;
    }

    return cancel;
}

/*
** Replace all occurences of "searchString" in "window" with "replaceString"
** within the current primary selection in "window". Also adds the search and
** replace strings to the global search history.
*/
void ReplaceInSelectionEx(MainWindow *window, DocumentWidget *document, TextArea *area, const QString &searchString, const QString &replaceString, SearchType searchType) {

    int selStart;
    int selEnd;
    int beginPos;
    int startPos;
    int endPos;
    int realOffset;
    int replaceLen;
    bool found;
    bool isRect;
    int rectStart;
    int rectEnd;
    int lineStart;
    int cursorPos;
    int extentBW;
    int extentFW;
    std::string fileString;
    bool substSuccess = false;
    bool anyFound     = false;
    bool cancelSubst  = true;

    // save a copy of search and replace strings in the search history
    saveSearchHistory(searchString, replaceString, searchType, false);

    // find out where the selection is
    if (!document->buffer_->BufGetSelectionPos(&selStart, &selEnd, &isRect, &rectStart, &rectEnd)) {
        return;
    }

    // get the selected text
    if (isRect) {
        selStart = document->buffer_->BufStartOfLine(selStart);
        selEnd = document->buffer_->BufEndOfLine( selEnd);
        fileString = document->buffer_->BufGetRangeEx(selStart, selEnd);
    } else {
        fileString = document->buffer_->BufGetSelectionTextEx();
    }

    /* create a temporary buffer in which to do the replacements to hide the
       intermediate steps from the display routines, and so everything can
       be undone in a single operation */
    TextBuffer tempBuf;
    tempBuf.BufSetAllEx(fileString);

    // search the string and do the replacements in the temporary buffer

    replaceLen = replaceString.size();
    found      = true;
    beginPos   = 0;
    cursorPos  = 0;
    realOffset = 0;

    while (found) {
        found = SearchString(
                    fileString,
                    searchString,
                    Direction::FORWARD,
                    searchType,
                    WrapMode::NoWrap,
                    beginPos,
                    &startPos,
                    &endPos,
                    &extentBW,
                    &extentFW,
                    GetWindowDelimitersEx(document));

        if (!found) {
            break;
        }

        anyFound = true;
        /* if the selection is rectangular, verify that the found
           string is in the rectangle */
        if (isRect) {
            lineStart = document->buffer_->BufStartOfLine(selStart + startPos);
            if (document->buffer_->BufCountDispChars(lineStart, selStart + startPos) < rectStart || document->buffer_->BufCountDispChars(lineStart, selStart + endPos) > rectEnd) {

                if(static_cast<size_t>(endPos) == fileString.size()) {
                    break;
                }

                /* If the match starts before the left boundary of the
                   selection, and extends past it, we should not continue
                   search after the end of the (false) match, because we
                   could miss a valid match starting between the left boundary
                   and the end of the false match. */
                if (document->buffer_->BufCountDispChars(lineStart, selStart + startPos) < rectStart && document->buffer_->BufCountDispChars(lineStart, selStart + endPos) > rectStart) {
                    beginPos += 1;
                } else {
                    beginPos = (startPos == endPos) ? endPos + 1 : endPos;
                }
                continue;
            }
        }

        /* Make sure the match did not start past the end (regular expressions
           can consider the artificial end of the range as the end of a line,
           and match a fictional whole line beginning there) */
        if (startPos == (selEnd - selStart)) {
            found = false;
            break;
        }

        // replace the string and compensate for length change
        if (isRegexType(searchType)) {
            std::string replaceResult;
            const std::string foundString = tempBuf.BufGetRangeEx(extentBW + realOffset, extentFW + realOffset + 1);

            substSuccess = replaceUsingREEx(
                            searchString,
                            replaceString,
                            foundString,
                            startPos - extentBW,
                            replaceResult,
                            (startPos + realOffset) == 0 ? '\0' : tempBuf.BufGetCharacter(startPos + realOffset - 1),
                            GetWindowDelimitersEx(document),
                            defaultRegexFlags(searchType));

            if (!substSuccess) {
                /*  The substitution failed. Primary reason for this would be
                    a result string exceeding SEARCHMAX. */

                cancelSubst = prefOrUserCancelsSubstEx(window, document);

                if (cancelSubst) {
                    //  No point in trying other substitutions.
                    break;
                }
            }

            tempBuf.BufReplaceEx(startPos + realOffset, endPos + realOffset, replaceResult);
            replaceLen = replaceResult.size();
        } else {
            // at this point plain substitutions (should) always work
            tempBuf.BufReplaceEx(startPos + realOffset, endPos + realOffset, replaceString.toLatin1().data());
            substSuccess = true;
        }

        realOffset += replaceLen - (endPos - startPos);
        // start again after match unless match was empty, then endPos+1
        beginPos = (startPos == endPos) ? endPos + 1 : endPos;
        cursorPos = endPos;
        if (fileString[endPos] == '\0')
            break;
    }

    if (anyFound) {
        if (substSuccess || !cancelSubst) {
            /*  Either the substitution was successful (the common case) or the
                user does not care and wants to have a faulty replacement.  */

            // replace the selected range in the real buffer
            document->buffer_->BufReplaceEx(selStart, selEnd, tempBuf.BufAsStringEx());

            // set the insert point at the end of the last replacement
            area->TextSetCursorPos(selStart + cursorPos + realOffset);

            /* leave non-rectangular selections selected (rect. ones after replacement
               are less useful since left/right positions are randomly adjusted) */
            if (!isRect) {
                document->buffer_->BufSelect(selStart, selEnd + realOffset);
            }
        }
    } else {
        //  Nothing found, tell the user about it
        if (GetPrefSearchDlogs()) {

            // Avoid bug in Motif 1.1 by putting away search dialog before Dialogs
            if (auto dialog = qobject_cast<DialogFind *>(window->dialogFind_)) {
                if(!dialog->keepDialog()) {
                    dialog->hide();
                }
            }

            auto dialog = window->getDialogReplace();
            if (dialog && !dialog->keepDialog()) {
                dialog->hide();
            }

            QMessageBox::information(document, QLatin1String("String not found"), QLatin1String("String was not found"));
        } else {
            QApplication::beep();
        }
    }
}


/*
** Replace all occurences of "searchString" in "window" with "replaceString".
** Also adds the search and replace strings to the global search history.
*/
bool ReplaceAllEx(MainWindow *window, DocumentWidget *document, TextArea *area, const QString &searchString, const QString &replaceString, SearchType searchType) {

    int copyStart;
    int copyEnd;

    // reject empty string
    if (searchString.isEmpty()) {
        return false;
    }

    // save a copy of search and replace strings in the search history
    saveSearchHistory(searchString, replaceString, searchType, false);

    // view the entire text buffer from the text area widget as a string
    view::string_view fileString = document->buffer_->BufAsStringEx();

    QString delimieters = GetWindowDelimitersEx(document);

    bool ok;
    std::string newFileString = ReplaceAllInStringEx(
                fileString,
                searchString,
                replaceString,
                searchType,
                &copyStart,
                &copyEnd,
                delimieters,
                &ok);

    if(!ok) {
        if (document->multiFileBusy_) {
            // only needed during multi-file replacements
            document->replaceFailed_ = true;
        } else if (GetPrefSearchDlogs()) {

            if (auto dialog = qobject_cast<DialogFind *>(window->dialogFind_)) {
                if(!dialog->keepDialog()) {
                    dialog->hide();
                }
            }

            auto dialog = window->getDialogReplace();
            if (dialog && !dialog->keepDialog()) {
                dialog->hide();
            }

            QMessageBox::information(document, QLatin1String("String not found"), QLatin1String("String was not found"));
        } else {
            QApplication::beep();
        }

        return false;
    }

    // replace the contents of the text widget with the substituted text
    document->buffer_->BufReplaceEx(copyStart, copyEnd, newFileString);

    // Move the cursor to the end of the last replacement
    area->TextSetCursorPos(copyStart + newFileString.size());

    return true;
}

/*
** Replace all occurences of "searchString" in "inString" with "replaceString"
** and return an allocated string covering the range between the start of the
** first replacement (returned in "copyStart", and the end of the last
** replacement (returned in "copyEnd")
*/
std::string ReplaceAllInStringEx(view::string_view inString, const QString &searchString, const QString &replaceString, SearchType searchType, int *copyStart, int *copyEnd, const QString &delimiters, bool *ok) {
    int startPos;
    int endPos;
    int lastEndPos;
    int copyLen;
    int searchExtentBW;
    int searchExtentFW;

	// reject empty string 
    if (searchString.isNull()) {
        *ok = false;
        return std::string();
    }

	/* rehearse the search first to determine the size of the buffer needed
	   to hold the substituted text.  No substitution done here yet */
    int replaceLen = replaceString.size();
    bool found     = true;
    int nFound     = 0;
    int removeLen  = 0;
    int addLen     = 0;
    int beginPos   = 0;

	*copyStart = -1;

	while (found) {
        found = SearchString(
                    inString,
                    searchString,
                    Direction::FORWARD,
                    searchType,
                    WrapMode::NoWrap,
                    beginPos,
                    &startPos,
                    &endPos,
                    &searchExtentBW,
                    &searchExtentFW,
                    delimiters);

		if (found) {
            if (*copyStart < 0) {
				*copyStart = startPos;
            }

			*copyEnd = endPos;
			// start next after match unless match was empty, then endPos+1 
			beginPos = (startPos == endPos) ? endPos + 1 : endPos;
			nFound++;
			removeLen += endPos - startPos;
			if (isRegexType(searchType)) {
                std::string replaceResult;

				replaceUsingREEx(
                    searchString,
					replaceString,
                    &inString[searchExtentBW],
					startPos - searchExtentBW,
					replaceResult,
					startPos == 0 ? '\0' : inString[startPos - 1],
					delimiters,
					defaultRegexFlags(searchType));

                addLen += replaceResult.size();
            } else {
				addLen += replaceLen;
            }

            if (inString[endPos] == '\0') {
				break;
            }
		}
	}

    if (nFound == 0) {
        *ok = false;
        return std::string();
    }

	/* Allocate a new buffer to hold all of the new text between the first
	   and last substitutions */
	copyLen = *copyEnd - *copyStart;


    std::string outString;
    outString.reserve(copyLen - removeLen + addLen);

	/* Scan through the text buffer again, substituting the replace string
	   and copying the part between replaced text to the new buffer  */
    found = true;
	beginPos = 0;
	lastEndPos = 0;

    while (found) {
        found = SearchString(inString, searchString, Direction::FORWARD, searchType, WrapMode::NoWrap, beginPos, &startPos, &endPos, &searchExtentBW, &searchExtentFW, delimiters);
		if (found) {
			if (beginPos != 0) {

                outString.append(&inString[lastEndPos], &inString[lastEndPos + startPos - lastEndPos]);
			}

			if (isRegexType(searchType)) {
                std::string replaceResult;

				replaceUsingREEx(
                    searchString,
					replaceString,
					&inString[searchExtentBW],
					startPos - searchExtentBW,
					replaceResult,
					startPos == 0 ? '\0' : inString[startPos - 1],
					delimiters,
					defaultRegexFlags(searchType));

                outString.append(replaceResult);
			} else {
                outString.append(replaceString.toStdString());
			}

			lastEndPos = endPos;

			// start next after match unless match was empty, then endPos+1 
			beginPos = (startPos == endPos) ? endPos + 1 : endPos;
            if (inString[endPos] == '\0') {
				break;
            }
		}
	}

    *ok = true;
	return outString;
}

/*
** If this is an incremental search and BeepOnSearchWrap is on:
** Emit a beep if the search wrapped over BOF/EOF compared to
** the last startPos of the current incremental search.
*/
static void iSearchTryBeepOnWrapEx(MainWindow *window, Direction direction, int beginPos, int startPos) {
    if (GetPrefBeepOnSearchWrap()) {
        if (direction == Direction::FORWARD) {
            if ((startPos >= beginPos && window->iSearchLastBeginPos_ < beginPos) || (startPos < beginPos && window->iSearchLastBeginPos_ >= beginPos)) {
                QApplication::beep();
            }
        } else {
            if ((startPos <= beginPos && window->iSearchLastBeginPos_ > beginPos) || (startPos > beginPos && window->iSearchLastBeginPos_ <= beginPos)) {
                QApplication::beep();
            }
        }
    }
}

/*
** Search the text in "window", attempting to match "searchString"
*/
bool SearchWindowEx(MainWindow *window, DocumentWidget *document, Direction direction, const QString &searchString, SearchType searchType, WrapMode searchWrap, int beginPos, int *startPos, int *endPos, int *extentBW, int *extentFW) {
    bool found;
    int fileEnd = document->buffer_->BufGetLength() - 1;
    bool outsideBounds;

    // reject empty string
    if (searchString.isEmpty()) {
        return false;
    }

    // get the entire text buffer from the text area widget
    view::string_view fileString = document->buffer_->BufAsStringEx();

    /* If we're already outside the boundaries, we must consider wrapping
       immediately (Note: fileEnd+1 is a valid starting position. Consider
       searching for $ at the end of a file ending with \n.) */
    if ((direction == Direction::FORWARD && beginPos > fileEnd + 1) || (direction == Direction::BACKWARD && beginPos < 0)) {
        outsideBounds = true;
    } else {
        outsideBounds = false;
    }

    /* search the string copied from the text area widget, and present
       dialogs, or just beep.  iSearchStartPos is not a perfect indicator that
       an incremental search is in progress.  A parameter would be better. */
    if (window->iSearchStartPos_ == -1) { // normal search

        found = !outsideBounds && SearchString(
                    fileString,
                    searchString,
                    direction,
                    searchType,
                    WrapMode::NoWrap,
                    beginPos,
                    startPos,
                    endPos,
                    extentBW,
                    extentFW,
                    GetWindowDelimitersEx(document));

        // Avoid Motif 1.1 bug by putting away search dialog before Dialogs
        if (auto dialog = qobject_cast<DialogFind *>(window->dialogFind_)) {
            if(!dialog->keepDialog()) {
                dialog->hide();
            }
        }

        auto dialog = window->getDialogReplace();
        if (dialog && !dialog->keepDialog()) {
            dialog->hide();
        }

        if (!found) {
            if (searchWrap == WrapMode::Wrap) {
                if (direction == Direction::FORWARD && beginPos != 0) {
                    if (GetPrefBeepOnSearchWrap()) {
                        QApplication::beep();
                    } else if (GetPrefSearchDlogs()) {

                        QMessageBox messageBox(nullptr /*window->shell_*/);
                        messageBox.setWindowTitle(QLatin1String("Wrap Search"));
                        messageBox.setIcon(QMessageBox::Question);
                        messageBox.setText(QLatin1String("Continue search from\nbeginning of file?"));
                        QPushButton *buttonContinue = messageBox.addButton(QLatin1String("Continue"), QMessageBox::AcceptRole);
                        QPushButton *buttonCancel   = messageBox.addButton(QMessageBox::Cancel);
                        Q_UNUSED(buttonContinue);

                        messageBox.exec();
                        if(messageBox.clickedButton() == buttonCancel) {
                            return false;
                        }
                    }

                    found = SearchString(
                                fileString,
                                searchString,
                                direction,
                                searchType,
                                WrapMode::NoWrap,
                                0,
                                startPos,
                                endPos,
                                extentBW,
                                extentFW,
                                GetWindowDelimitersEx(document));

                } else if (direction == Direction::BACKWARD && beginPos != fileEnd) {
                    if (GetPrefBeepOnSearchWrap()) {
                        QApplication::beep();
                    } else if (GetPrefSearchDlogs()) {

                        QMessageBox messageBox(nullptr /*window->shell_*/);
                        messageBox.setWindowTitle(QLatin1String("Wrap Search"));
                        messageBox.setIcon(QMessageBox::Question);
                        messageBox.setText(QLatin1String("Continue search\nfrom end of file?"));
                        QPushButton *buttonContinue = messageBox.addButton(QLatin1String("Continue"), QMessageBox::AcceptRole);
                        QPushButton *buttonCancel   = messageBox.addButton(QMessageBox::Cancel);
                        Q_UNUSED(buttonContinue);

                        messageBox.exec();
                        if(messageBox.clickedButton() == buttonCancel) {
                            return false;
                        }
                    }
                    found = SearchString(
                                fileString,
                                searchString,
                                direction,
                                searchType,
                                WrapMode::NoWrap,
                                fileEnd + 1,
                                startPos,
                                endPos,
                                extentBW,
                                extentFW,
                                GetWindowDelimitersEx(document));
                }
            }
            if (!found) {
                if (GetPrefSearchDlogs()) {
                    QMessageBox::information(document, QLatin1String("String not found"), QLatin1String("String was not found"));
                } else {
                    QApplication::beep();
                }
            }
        }
    } else { // incremental search
        if (outsideBounds && searchWrap == WrapMode::Wrap) {
            if (direction == Direction::FORWARD) {
                beginPos = 0;
            } else {
                beginPos = fileEnd + 1;
            }
            outsideBounds = false;
        }

        found = !outsideBounds && SearchString(
                    fileString,
                    searchString,
                    direction,
                    searchType,
                    searchWrap,
                    beginPos,
                    startPos,
                    endPos,
                    extentBW,
                    extentFW,
                    GetWindowDelimitersEx(document));

        if (found) {
            iSearchTryBeepOnWrapEx(window, direction, beginPos, *startPos);
        } else
            QApplication::beep();
    }

    return found;
}

/*
** Search the null terminated string "string" for "searchString", beginning at
** "beginPos".  Returns the boundaries of the match in "startPos" and "endPos".
** searchExtentBW and searchExtentFW return the backwardmost and forwardmost
** positions used to make the match, which are usually startPos and endPos,
** but may extend further if positive lookahead or lookbehind was used in
** a regular expression match.  "delimiters" may be used to provide an
** alternative set of word delimiters for regular expression "<" and ">"
** characters, or simply passed as null for the default delimiter set.
*/
bool SearchString(view::string_view string, const QString &searchString, Direction direction, SearchType searchType, WrapMode wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const QString &delimiters) {

    return SearchString(
                string,
                searchString,
                direction,
                searchType,
                wrap,
                beginPos,
                startPos,
                endPos,
                searchExtentBW,
                searchExtentFW,
                delimiters.isNull() ? nullptr : delimiters.toLatin1().data());

}

static bool SearchString(view::string_view string, const QString &searchString, Direction direction, SearchType searchType, WrapMode wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters) {
	switch (searchType) {
    case SearchType::CaseSenseWord:
        return searchLiteralWord(string, searchString.toLatin1().data(), true, direction, wrap, beginPos, startPos, endPos, delimiters);
    case SearchType::LiteralWord:
        return searchLiteralWord(string, searchString.toLatin1().data(), false, direction, wrap, beginPos, startPos, endPos, delimiters);
    case SearchType::CaseSense:
        return searchLiteral(string, searchString.toLatin1().data(), true, direction, wrap, beginPos, startPos, endPos, searchExtentBW, searchExtentFW);
    case SearchType::Literal:
        return searchLiteral(string, searchString.toLatin1().data(), false, direction, wrap, beginPos, startPos, endPos, searchExtentBW, searchExtentFW);
    case SearchType::Regex:
        return searchRegex(string, searchString.toLatin1().data(), direction, wrap, beginPos, startPos, endPos, searchExtentBW, searchExtentFW, delimiters, REDFLT_STANDARD);
    case SearchType::RegexNoCase:
        return searchRegex(string, searchString.toLatin1().data(), direction, wrap, beginPos, startPos, endPos, searchExtentBW, searchExtentFW, delimiters, REDFLT_CASE_INSENSITIVE);
	}

    Q_UNREACHABLE();
}

/*
**  Searches for whole words (Markus Schwarzenberg).
**
**  If the first/last character of `searchString' is a "normal
**  word character" (not contained in `delimiters', not a whitespace)
**  then limit search to strings, who's next left/next right character
**  is contained in `delimiters' or is a whitespace or text begin or end.
**
**  If the first/last character of `searchString' itself is contained
**  in delimiters or is a white space, then the neighbour character of the
**  first/last character will not be checked, just a simple match
**  will suffice in that case.
**
*/
static bool searchLiteralWord(view::string_view string, view::string_view searchString, bool caseSense, Direction direction, WrapMode wrap, int beginPos, int *startPos, int *endPos, const char *delimiters) {

	std::string lcString;
	std::string ucString;
    bool cignore_L = false;
    bool cignore_R = false;

    auto do_search_word2 = [&](const char *filePtr) {
		if (*filePtr == ucString[0] || *filePtr == lcString[0]) {
			// matched first character 
			auto ucPtr = ucString.begin();
			auto lcPtr = lcString.begin();
			const char *tempPtr = filePtr;

			while (*tempPtr == *ucPtr || *tempPtr == *lcPtr) {
				tempPtr++;
				ucPtr++;
				lcPtr++;

                if (ucPtr == ucString.end() &&                                                                   // matched whole string
                    (cignore_R || safe_ctype<isspace>(*tempPtr) || strchr(delimiters, *tempPtr)) &&    // next char right delimits word ?
                    (cignore_L || filePtr == &string[0] ||                                                       // border case
                     safe_ctype<isspace>(filePtr[-1]) || strchr(delimiters, filePtr[-1]))) {           // next char left delimits word ?

                    *startPos = filePtr - &string[0];
                    *endPos   = tempPtr - &string[0];
					return true;
				}
			}
		}
		
        return false;
	};


	// If there is no language mode, we use the default list of delimiters 
	QByteArray delimiterString = GetPrefDelimiters().toLatin1();
	if(!delimiters) {
		delimiters = delimiterString.data();
    }

    if (safe_ctype<isspace>(searchString[0]) || strchr(delimiters, searchString[0])) {
		cignore_L = true;
	}

    if (safe_ctype<isspace>(searchString[searchString.size() - 1]) || strchr(delimiters, searchString[searchString.size() - 1])) {
		cignore_R = true;
	}

	if (caseSense) {
		ucString = searchString.to_string();
		lcString = searchString.to_string();
	} else {
		ucString = upCaseStringEx(searchString);
		lcString = downCaseStringEx(searchString);
	}

    if (direction == Direction::FORWARD) {
		// search from beginPos to end of string 
		for (auto filePtr = string.begin() + beginPos; filePtr != string.end(); filePtr++) {
            if(do_search_word2(filePtr)) {
				return true;
			}
		}
        if (wrap == WrapMode::NoWrap)
            return false;

		// search from start of file to beginPos 
		for (auto filePtr = string.begin(); filePtr <= string.begin() + beginPos; filePtr++) {
            if(do_search_word2(filePtr)) {
				return true;
			}
		}
        return false;
	} else {
        // Direction::BACKWARD
		// search from beginPos to start of file. A negative begin pos 
		// says begin searching from the far end of the file 
		if (beginPos >= 0) {
			for (auto filePtr = string.begin() + beginPos; filePtr >= string.begin(); filePtr--) {
                if(do_search_word2(filePtr)) {
					return true;
				}
			}
		}
        if (wrap == WrapMode::NoWrap)
            return false;

		// search from end of file to beginPos 
		for (auto filePtr = string.begin() + string.size(); filePtr >= string.begin() + beginPos; filePtr--) {
            if(do_search_word2(filePtr)) {
				return true;
			}
		}
        return false;
	}
}

static bool searchLiteral(view::string_view string, view::string_view searchString, bool caseSense, Direction direction, WrapMode wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW) {


	std::string lcString;
	std::string ucString;

    if (caseSense) {
        lcString = searchString.to_string();
        ucString = searchString.to_string();
    } else {
        ucString = upCaseStringEx(searchString);
        lcString = downCaseStringEx(searchString);
    }

    auto do_search2 = [&](const char *filePtr) {
		if (*filePtr == ucString[0] || *filePtr == lcString[0]) {
			// matched first character 
			auto ucPtr   = ucString.begin();
			auto lcPtr   = lcString.begin();
			const char *tempPtr = filePtr;

			while (*tempPtr == *ucPtr || *tempPtr == *lcPtr) {
				tempPtr++;
				ucPtr++;
				lcPtr++;
				if (ucPtr == ucString.end()) {
					// matched whole string 
					*startPos = filePtr - &string[0];
					*endPos   = tempPtr - &string[0];
					if(searchExtentBW) {
						*searchExtentBW = *startPos;
					}

					if(searchExtentFW) {
						*searchExtentFW = *endPos;
					}
					return true;
				}
			}
		}

        return false;
	};



    if (direction == Direction::FORWARD) {

		auto first = string.begin();
		auto mid   = first + beginPos;
		auto last  = string.end();

		// search from beginPos to end of string 
		for (auto filePtr = mid; filePtr != last; ++filePtr) {
            if(do_search2(filePtr)) {
				return true;
			}
		}

        if (wrap == WrapMode::NoWrap) {
            return false;
		}

		// search from start of file to beginPos 
        // NOTE(eteran): this used to include "mid", but that seems redundant given that we already looked there
		//               in the first loop
		for (auto filePtr = first; filePtr != mid; ++filePtr) {
            if(do_search2(filePtr)) {
				return true;
			}
		}

        return false;
	} else {
        // Direction::BACKWARD
		// search from beginPos to start of file.  A negative begin pos	
		// says begin searching from the far end of the file 

		auto first = string.begin();
		auto mid   = first + beginPos;
		auto last  = string.end();

		if (beginPos >= 0) {
			for (auto filePtr = mid; filePtr >= first; --filePtr) {
                if(do_search2(filePtr)) {
					return true;
				}
			}
		}

        if (wrap == WrapMode::NoWrap) {
            return false;
		}

		// search from end of file to beginPos 
		// how to get the text string length from the text widget (under 1.1)
		for (auto filePtr = last; filePtr >= mid; --filePtr) {
            if(do_search2(filePtr)) {
				return true;
			}
		}

        return false;
	}
}

static bool searchRegex(view::string_view string, view::string_view searchString, Direction direction, WrapMode wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters, int defaultFlags) {
    if (direction == Direction::FORWARD)
		return forwardRegexSearch(string, searchString, wrap, beginPos, startPos, endPos, searchExtentBW, searchExtentFW, delimiters, defaultFlags);
	else
		return backwardRegexSearch(string, searchString, wrap, beginPos, startPos, endPos, searchExtentBW, searchExtentFW, delimiters, defaultFlags);
}

static bool forwardRegexSearch(view::string_view string, view::string_view searchString, WrapMode wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters, int defaultFlags) {

	try {
        regexp compiledRE(searchString, defaultFlags);

		// search from beginPos to end of string 
		if (compiledRE.execute(string, beginPos, delimiters, false)) {

			*startPos = compiledRE.startp[0] - &string[0];
			*endPos   = compiledRE.endp[0]   - &string[0];

			if(searchExtentFW) {
				*searchExtentFW = compiledRE.extentpFW - &string[0];
			}

			if(searchExtentBW) {
				*searchExtentBW = compiledRE.extentpBW - &string[0];
			}

			return true;
		}

		// if wrap turned off, we're done 
        if (wrap == WrapMode::NoWrap) {
            return false;
		}

		// search from the beginning of the string to beginPos 
		if (compiledRE.execute(string, 0, beginPos, delimiters, false)) {

			*startPos = compiledRE.startp[0] - &string[0];
			*endPos = compiledRE.endp[0]     - &string[0];

			if(searchExtentFW) {
				*searchExtentFW = compiledRE.extentpFW - &string[0];
			}

			if(searchExtentBW) {
				*searchExtentBW = compiledRE.extentpBW - &string[0];
			}
			return true;
		}

        return false;
	} catch(const regex_error &e) {
		/* Note that this does not process errors from compiling the expression.
		 * It assumes that the expression was checked earlier.
		 */
        return false;
	}
}

static bool backwardRegexSearch(view::string_view string, view::string_view searchString, WrapMode wrap, int beginPos, int *startPos, int *endPos, int *searchExtentBW, int *searchExtentFW, const char *delimiters, int defaultFlags) {

	try {
		regexp compiledRE(searchString, defaultFlags);

		// search from beginPos to start of file.  A negative begin pos	
		// says begin searching from the far end of the file.		
		if (beginPos >= 0) {

            // NOTE(eteran): why do we use '\0' as the previous char, and not string[beginPos - 1] (assuming that beginPos > 0)?
			if (compiledRE.execute(string, 0, beginPos, '\0', '\0', delimiters, true)) {

				*startPos = compiledRE.startp[0] - &string[0];
				*endPos   = compiledRE.endp[0]   - &string[0];

				if(searchExtentFW) {
					*searchExtentFW = compiledRE.extentpFW - &string[0];
				}

				if(searchExtentBW) {
					*searchExtentBW = compiledRE.extentpBW - &string[0];
				}

				return true;
			}
		}

		// if wrap turned off, we're done 
        if (wrap == WrapMode::NoWrap) {
            return false;
		}

		// search from the end of the string to beginPos 
		if (beginPos < 0) {
			beginPos = 0;
		}

		if (compiledRE.execute(string, beginPos, delimiters, true)) {

			*startPos = compiledRE.startp[0] - &string[0];
			*endPos   = compiledRE.endp[0]   - &string[0];

			if(searchExtentFW) {
				*searchExtentFW = compiledRE.extentpFW - &string[0];
			}

			if(searchExtentBW) {
				*searchExtentBW = compiledRE.extentpBW - &string[0];
			}

			return true;
		}

        return false;
	} catch(const regex_error &e) {
        Q_UNUSED(e);
        return false;
	}
}

static std::string upCaseStringEx(view::string_view inString) {

	std::string str;
	str.reserve(inString.size());
	std::transform(inString.begin(), inString.end(), std::back_inserter(str), [](char ch) {
        return safe_ctype<toupper>(ch);
	});
	return str;
}

static std::string downCaseStringEx(view::string_view inString) {

    std::string str;
	str.reserve(inString.size());
	std::transform(inString.begin(), inString.end(), std::back_inserter(str), [](char ch) {
        return safe_ctype<tolower>(ch);
	});
	return str;
}

/*
** Return TRUE if "searchString" exactly matches the text in the window's
** current primary selection using search algorithm "searchType".  If true,
** also return the position of the selection in "left" and "right".
*/
static bool searchMatchesSelectionEx(DocumentWidget *window, const QString &searchString, SearchType searchType, int *left, int *right, int *searchExtentBW, int *searchExtentFW) {
    int selLen, selStart, selEnd, startPos, endPos, extentBW, extentFW, beginPos;
    int regexLookContext = isRegexType(searchType) ? 1000 : 0;
    std::string string;
    int rectStart, rectEnd, lineStart = 0;
    bool isRect;

    // find length of selection, give up on no selection or too long
    if (!window->buffer_->BufGetEmptySelectionPos(&selStart, &selEnd, &isRect, &rectStart, &rectEnd)) {
        return false;
    }

    if (selEnd - selStart > SEARCHMAX) {
        return false;
    }

    // if the selection is rectangular, don't match if it spans lines
    if (isRect) {
        lineStart = window->buffer_->BufStartOfLine(selStart);
        if (lineStart != window->buffer_->BufStartOfLine(selEnd)) {
            return false;
        }
    }

    /* get the selected text plus some additional context for regular
       expression lookahead */
    if (isRect) {
        int stringStart = lineStart + rectStart - regexLookContext;
        if (stringStart < 0) {
            stringStart = 0;
        }

        string = window->buffer_->BufGetRangeEx(stringStart, lineStart + rectEnd + regexLookContext);
        selLen = rectEnd - rectStart;
        beginPos = lineStart + rectStart - stringStart;
    } else {
        int stringStart = selStart - regexLookContext;
        if (stringStart < 0) {
            stringStart = 0;
        }

        string = window->buffer_->BufGetRangeEx(stringStart, selEnd + regexLookContext);
        selLen = selEnd - selStart;
        beginPos = selStart - stringStart;
    }
    if (string.empty()) {
        return false;
    }

    // search for the string in the selection (we are only interested
    // in an exact match, but the procedure SearchString does important
    // stuff like applying the correct matching algorithm)
    bool found = SearchString(
                string,
                searchString,
                Direction::FORWARD,
                searchType,
                WrapMode::NoWrap,
                beginPos,
                &startPos,
                &endPos,
                &extentBW,
                &extentFW,
                GetWindowDelimitersEx(window));

    // decide if it is an exact match
    if (!found) {
        return false;
    }

    if (startPos != beginPos || endPos - beginPos != selLen) {
        return false;
    }

    // return the start and end of the selection
    if (isRect) {
        window->buffer_->GetSimpleSelection(left, right);
    } else {
        *left  = selStart;
        *right = selEnd;
    }

    if(searchExtentBW) {
        *searchExtentBW = *left - (startPos - extentBW);
    }

    if(searchExtentFW) {
        *searchExtentFW = *right + extentFW - endPos;
    }

    return true;
}

/*
** Substitutes a replace string for a string that was matched using a
** regular expression.  This was added later and is rather ineficient
** because instead of using the compiled regular expression that was used
** to make the match in the first place, it re-compiles the expression
** and redoes the search on the already-matched string.  This allows the
** code to continue using strings to represent the search and replace
** items.
*/
static bool replaceUsingREEx(view::string_view searchStr, const char *replaceStr, view::string_view sourceStr, int beginPos, std::string &dest, int prevChar, const char *delimiters, int defaultFlags) {
    try {
        regexp compiledRE(searchStr, defaultFlags);
        compiledRE.execute(sourceStr, beginPos, sourceStr.size(), prevChar, '\0', delimiters, false);
        return compiledRE.SubstituteRE(replaceStr, dest);
    } catch(const regex_error &e) {
        Q_UNUSED(e);
        return false;
    }
}


static bool replaceUsingREEx(const QString &searchStr, const QString &replaceStr, view::string_view sourceStr, int beginPos, std::string &dest, int prevChar, const QString &delimiters, int defaultFlags) {
    return replaceUsingREEx(
                searchStr.toLatin1().data(),
                replaceStr.toLatin1().data(),
                sourceStr,
                beginPos,
                dest,
                prevChar,
                delimiters.isNull() ? nullptr : delimiters.toLatin1().data(),
                defaultFlags);
}

/*
** Store the search and replace strings, and search type for later recall.
** If replaceString is nullptr, duplicate the last replaceString used.
** Contiguous incremental searches share the same history entry (each new
** search modifies the current search string, until a non-incremental search
** is made.  To mark the end of an incremental search, call saveSearchHistory
** again with an empty search string and isIncremental==False.
*/
void saveSearchHistory(const QString &searchString, QString replaceString, SearchType searchType, bool isIncremental) {

    static bool currentItemIsIncremental = false;

	/* Cancel accumulation of contiguous incremental searches (even if the
	   information is not worthy of saving) if search is not incremental */
    if (!isIncremental) {
        currentItemIsIncremental = false;
    }

	// Don't save empty search strings 
    if (searchString.isEmpty()) {
		return;
    }

	// If replaceString is nullptr, duplicate the last one (if any) 
    if(replaceString.isNull()) {
        replaceString = (NHist >= 1) ? SearchReplaceHistory[historyIndex(1)].replace : QLatin1String("");
    }

	/* Compare the current search and replace strings against the saved ones.
	   If they are identical, don't bother saving */
    if (NHist >= 1 && searchType == SearchReplaceHistory[historyIndex(1)].type && SearchReplaceHistory[historyIndex(1)].search == searchString && SearchReplaceHistory[historyIndex(1)].replace == replaceString) {
		return;
	}

	/* If the current history item came from an incremental search, and the
	   new one is also incremental, just update the entry */
	if (currentItemIsIncremental && isIncremental) {

        SearchReplaceHistory[historyIndex(1)].search = searchString;
        SearchReplaceHistory[historyIndex(1)].type   = searchType;
		return;
	}
	currentItemIsIncremental = isIncremental;

    if (NHist == 0) {
        for(MainWindow *window : MainWindow::allWindows()) {
            window->ui.action_Find_Again->setEnabled(true);
            window->ui.action_Replace_Find_Again->setEnabled(true);
            window->ui.action_Replace_Again->setEnabled(true);
		}
	}

    /* If there are more than MAX_SEARCH_HISTORY strings saved, recycle
       some space, free the entry that's about to be overwritten */
    if (NHist != MAX_SEARCH_HISTORY) {
        NHist++;
    }

    SearchReplaceHistory[HistStart].search  = searchString;
    SearchReplaceHistory[HistStart].replace = replaceString;
    SearchReplaceHistory[HistStart].type    = searchType;

    HistStart++;

    if (HistStart >= MAX_SEARCH_HISTORY) {
        HistStart = 0;
    }
}

/*
** return an index into the circular buffer arrays of history information
** for search strings, given the number of saveSearchHistory cycles back from
** the current time.
*/
int historyIndex(int nCycles) {

    if (nCycles > NHist || nCycles <= 0) {
		return -1;
    }

    int index = HistStart - nCycles;
    if (index < 0) {
        index = MAX_SEARCH_HISTORY + index;
    }

    return index;
}


/*
** Checks whether a search mode in one of the regular expression modes.
*/
bool isRegexType(SearchType searchType) {
    return searchType == SearchType::Regex || searchType == SearchType::RegexNoCase;
}

/*
** Returns the default flags for regular expression matching, given a
** regular expression search mode.
*/
int defaultRegexFlags(SearchType searchType) {
	switch (searchType) {
    case SearchType::Regex:
		return REDFLT_STANDARD;
    case SearchType::RegexNoCase:
		return REDFLT_CASE_INSENSITIVE;
	default:
		// We should never get here, but just in case ... 
		return REDFLT_STANDARD;
	}
}

/**
 * @brief make_regex
 * @param re
 * @param flags
 * @return
 */
std::unique_ptr<regexp> make_regex(const QString &re, int flags) {
    return std::make_unique<regexp>(re.toStdString(), flags);
}

