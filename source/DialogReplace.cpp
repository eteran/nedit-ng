
#include "DialogReplace.h"
#include "DialogMultiReplace.h"
#include "DocumentWidget.h"
#include "MainWindow.h"
#include "nedit.h"
#include "preferences.h"
#include "regularExp.h"
#include "search.h" // for the search type enum
#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMimeData>
#include <QtDebug>
#include <memory>

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
DialogReplace::DialogReplace(MainWindow *window, DocumentWidget *document, QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f), window_(window), document_(document) {
	
	ui.setupUi(this);
	
	lastRegexCase_   = true;
	lastLiteralCase_ = false;
	
#if defined(REPLACE_SCOPE)
	replaceScope_ = REPL_SCOPE_WIN;
#endif
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::showEvent(QShowEvent *event) {
	Q_UNUSED(event);
	ui.textFind->setFocus();

    centerDialog(this);
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::keyPressEvent(QKeyEvent *event) {

	if(ui.textFind->hasFocus()) {
        int index = window_->rHistIndex_;

		// only process up and down arrow keys 
		if (event->key() != Qt::Key_Up && event->key() != Qt::Key_Down) {
            QDialog::keyPressEvent(event);
			return;
		}

		// increment or decrement the index depending on which arrow was pressed 
		index += (event->key() == Qt::Key_Up) ? 1 : -1;

		// if the index is out of range, beep and return 
		if (index != 0 && historyIndex(index) == -1) {
			QApplication::beep();
			return;
		}

		// determine the strings and button settings to use 
		QString searchStr;
        QString replaceStr;
		SearchType searchType;
		if (index == 0) {
            searchStr  = QString();
            replaceStr = QString();
			searchType = GetPrefSearch();
		} else {
            searchStr  = SearchHistory[historyIndex(index)];
            replaceStr = ReplaceHistory[historyIndex(index)];
			searchType = SearchTypeHistory[historyIndex(index)];
		}

		// Set the buttons and fields with the selected search type 
		initToggleButtons(searchType);

		ui.textFind->setText(searchStr);
        ui.textReplace->setText(replaceStr);

		// Set the state of the Find ... button 
		fUpdateActionButtons();

        window_->rHistIndex_ = index;
	}

	if(ui.textReplace->hasFocus()) {
		int index = window_->rHistIndex_;

		// only process up and down arrow keys 
		if (event->key() != Qt::Key_Up && event->key() != Qt::Key_Down) {
            QDialog::keyPressEvent(event);
			return;
		}

		// increment or decrement the index depending on which arrow was pressed 
		index += (event->key() == Qt::Key_Up) ? 1 : -1;

		// if the index is out of range, beep and return 
		if (index != 0 && historyIndex(index) == -1) {
			QApplication::beep();
			return;
		}

		// change only the replace field information 
		if (index == 0) {
			ui.textReplace->setText(QString());
		} else {
            ui.textReplace->setText(ReplaceHistory[historyIndex(index)]);
		}

		window_->rHistIndex_ = index;
	}
	
	QDialog::keyPressEvent(event);
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_checkBackward_toggled(bool checked) {
	Q_UNUSED(checked);
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_checkKeep_toggled(bool checked) {
	if (checked) {
        setWindowTitle(tr("Find/Replace (in %1)").arg(document_->filename_));
	} else {
		setWindowTitle(tr("Find/Replace"));
	}
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_textFind_textChanged(const QString &text) {
	Q_UNUSED(text);
	UpdateReplaceActionButtons();
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_buttonFind_clicked() {
	
    QString searchString;
    QString replaceString;
	SearchDirection direction;
	SearchType searchType;
	
	// Validate and fetch the find and replace strings from the dialog 
    if (!getReplaceDlogInfo(&direction, &searchString, &replaceString, &searchType))
		return;

	// Set the initial focus of the dialog back to the search string	
	ui.textFind->setFocus();

	// Find the text and mark it 
    document_->findAP(searchString, direction, searchType, GetPrefSearchWraps());

	/* Doctor the search history generated by the action to include the
	   replace string (if any), so the replace string can be used on
	   subsequent replaces, even though no actual replacement was done. */
    if (historyIndex(1) != -1 && SearchHistory[historyIndex(1)] == searchString) {
        ReplaceHistory[historyIndex(1)] = replaceString;
	}

	// Pop down the dialog 
	if (!keepDialog()) {
		hide();
	}
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_buttonReplace_clicked() {

    QString searchString;
    QString replaceString;
	SearchDirection direction;
	SearchType searchType;
	
	// Validate and fetch the find and replace strings from the dialog 
    if (!getReplaceDlogInfo(&direction, &searchString, &replaceString, &searchType)) {
		return;
    }

	// Set the initial focus of the dialog back to the search string 
	ui.textFind->setFocus();

	// Find the text and replace it 
    document_->replaceAP(searchString, replaceString, direction, searchType, GetPrefSearchWraps());

	// Pop down the dialog 
	if (!keepDialog()) {
		hide();
	}
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_buttonReplaceFind_clicked() {

    QString searchString;
    QString replaceString;
	SearchDirection direction;
	SearchType searchType;
	
	// Validate and fetch the find and replace strings from the dialog 
    if (!getReplaceDlogInfo(&direction, &searchString, &replaceString, &searchType))
		return;

	// Set the initial focus of the dialog back to the search string 
	ui.textFind->setFocus();

	// Find the text and replace it 
    document_->replaceFindAP(searchString, replaceString, direction, searchType, GetPrefSearchWraps());

	// Pop down the dialog 
	if (!keepDialog()) {
		hide();
	}
}

#if defined(REPLACE_SCOPE)

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_radioWindow_toggled(bool checked) {
	if (checked) {
		replaceScope_ = REPL_SCOPE_WIN;
		UpdateReplaceActionButtons();
	}
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_radioSelection_toggled(bool checked) {
	if (checked) {
		replaceScope_ = REPL_SCOPE_SEL;
		UpdateReplaceActionButtons();
	}
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_radioMulti_toggled(bool checked) {

	if (checked) {
		replaceScope_ = REPL_SCOPE_MULTI;
		UpdateReplaceActionButtons();
	}
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_buttonAll_clicked() {
	
#if defined(REPLACE_SCOPE)

    QString searchString;
    QString replaceString;
    SearchDirection direction;
    SearchType searchType;

    // Validate and fetch the find and replace strings from the dialog
    if (!getReplaceDlogInfo(&direction, &searchString, &replaceString, &searchType)) {
        return;
    }

    switch (replaceScope_) {
	case REPL_SCOPE_WIN:
        document_->replaceAllAP(searchString, replaceString, searchType);

        // pop down the replace dialog
        if(!keepDialog()) {
            hide();
        }

		break;
	case REPL_SCOPE_SEL:
        document_->replaceInSelAP(searchString, replaceString, searchType);

        // pop down the replace dialog
        if(!keepDialog()) {
            hide();
        }

		break;
	case REPL_SCOPE_MULTI:
        // Don't let the user select files when no replacement can be made
        if (searchString.isEmpty()) {
            // Set the initial focus of the dialog back to the search string
            ui.textFind->setFocus();

            // pop down the replace dialog
            if(!keepDialog()) {
                hide();
            }
            return;
        }

        // Create the dialog if it doesn't already exist
        if (!dialogMultiReplace_) {
            dialogMultiReplace_ = new DialogMultiReplace(window_, document_, this, this);
        }

        // Raising the window doesn't make sense. It is modal, so we can't get here unless it is unmanaged
        // Prepare a list of writable windows
        collectWritableWindows();

        // Initialize/update the list of files.
        dialogMultiReplace_->uploadFileListItems(false);

        // Display the dialog
        dialogMultiReplace_->exec();
		break;
	}
#endif
}

#else

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_buttonWindow_clicked() {

    QString searchString;
    QString replaceString;
	SearchDirection direction;
	SearchType searchType;
	

	// Validate and fetch the find and replace strings from the dialog 
    if (!getReplaceDlogInfo(&direction, &searchString, &replaceString, &searchType))
		return;

	// Set the initial focus of the dialog back to the search string	
	ui.textFind->setFocus();

	// do replacement 
    document_->replaceAllAP(searchString, replaceString, searchType);

	// Pop down the dialog 
	if (!keepDialog()) {
		hide();
	}
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_buttonSelection_clicked() {

    QString searchString;
    QString replaceString;
	SearchDirection direction;
	SearchType searchType;
	

	// Validate and fetch the find and replace strings from the dialog 
    if (!getReplaceDlogInfo(&direction, &searchString, &replaceString, &searchType))
		return;

	// Set the initial focus of the dialog back to the search string 
	ui.textFind->setFocus();

	// do replacement 
    document_->replaceInSelAP(searchString, replaceString, searchType);

	// Pop down the dialog 
	if (!keepDialog()) {
		hide();
	}
}

//------------------------------------------------------------------------------
// name: on_buttonMulti_clicked
//------------------------------------------------------------------------------
void DialogReplace::on_buttonMulti_clicked() {

    QString searchString;
    QString replaceString;
	SearchDirection direction;
	SearchType searchType;

	// Validate and fetch the find and replace strings from the dialog 
    if (!getReplaceDlogInfo(&direction, &searchString, &replaceString, &searchType)) {
		return;
    }

	// Don't let the user select files when no replacement can be made 
    if (searchString.isEmpty()) {
		// Set the initial focus of the dialog back to the search string 
		ui.textFind->setFocus();
		
		// pop down the replace dialog 		
		if(!keepDialog()) {
            hide();
		}
		return;
	}

	// Create the dialog if it doesn't already exist 
	if (!dialogMultiReplace_) {
        dialogMultiReplace_ = new DialogMultiReplace(window_, document_, this, this);
	}

	// Raising the window doesn't make sense. It is modal, so we can't get here unless it is unmanaged
	// Prepare a list of writable windows 
	collectWritableWindows();

	// Initialize/update the list of files. 
	dialogMultiReplace_->uploadFileListItems(false);

	// Display the dialog 
    dialogMultiReplace_->exec();
}

#endif

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_checkRegex_toggled(bool checked) {


	bool searchRegex = checked;
	bool searchCaseSense = ui.checkCase->isChecked();

	// In sticky mode, restore the state of the Case Sensitive button 
	if (GetPrefStickyCaseSenseBtn()) {
		if (searchRegex) {
			lastLiteralCase_ = searchCaseSense;
			ui.checkCase->setChecked(lastRegexCase_);
		} else {
			lastRegexCase_ = searchCaseSense;
			ui.checkCase->setChecked(lastLiteralCase_);
		}
	}
	// make the Whole Word button insensitive for regex searches 
	ui.checkWord->setEnabled(!searchRegex);
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::on_checkCase_toggled(bool checked) {

	bool searchCaseSense = checked;

	/* Save the state of the Case Sensitive button
	   depending on the state of the Regex button*/
	if (ui.checkRegex->isChecked()) {
		lastRegexCase_ = searchCaseSense;
	} else {
		lastLiteralCase_ = searchCaseSense;
	}
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::setTextField(DocumentWidget *document) {

    Q_UNUSED(document);
	
    QString initialText;

	if (GetPrefFindReplaceUsesSelection()) {
        const QMimeData *mimeData = QApplication::clipboard()->mimeData(QClipboard::Selection);
        if(mimeData->hasText()) {
            initialText = mimeData->text();
        }
	}

	// Update the field 
    ui.textFind->setText(initialText);
}

/*
** Shared routine for replace and find dialogs and i-search bar to initialize
** the state of the regex/case/word toggle buttons, and the sticky case
** sensitivity states.
*/
void DialogReplace::initToggleButtons(SearchType searchType) {
	/* Set the initial search type and remember the corresponding case
	   sensitivity states in case sticky case sensitivity is required. */
	switch (searchType) {
	case SEARCH_LITERAL:
		lastLiteralCase_ = false;
		lastRegexCase_   = true;
		ui.checkRegex->setChecked(false);
		ui.checkCase->setChecked(false);
		if (true) {
			ui.checkWord->setChecked(false);
			ui.checkWord->setEnabled(true);
		}
		break;
	case SEARCH_CASE_SENSE:
		lastLiteralCase_ = true;
		lastRegexCase_   = true;
		ui.checkRegex->setChecked(false);
		ui.checkCase->setChecked(true);
		if (true) {
			ui.checkWord->setChecked(false);
			ui.checkWord->setEnabled(true);
		}
		break;
	case SEARCH_LITERAL_WORD:
		lastLiteralCase_ = false;
		lastRegexCase_   = true;
		ui.checkRegex->setChecked(false);
		ui.checkCase->setChecked(false);
		if (true) {
			ui.checkWord->setChecked(true);
			ui.checkWord->setEnabled(true);
		}
		break;
	case SEARCH_CASE_SENSE_WORD:
		lastLiteralCase_ = true;
		lastRegexCase_   = true;
		ui.checkRegex->setChecked(false);
		ui.checkCase->setChecked(true);
		if (true) {
			ui.checkWord->setChecked(true);
			ui.checkWord->setEnabled(true);
		}
		break;
	case SEARCH_REGEX:
		lastLiteralCase_ = false;
		lastRegexCase_   = true;
		ui.checkRegex->setChecked(true);
		ui.checkCase->setChecked(true);
		if (true) {
			ui.checkWord->setChecked(false);
			ui.checkWord->setEnabled(false);
		}
		break;
	case SEARCH_REGEX_NOCASE:
		lastLiteralCase_ = false;
		lastRegexCase_   = false;
		ui.checkRegex->setChecked(true);
		ui.checkCase->setChecked(false);
		if (true) {
			ui.checkWord->setChecked(false);
			ui.checkWord->setEnabled(false);
		}
		break;
	default:
		Q_ASSERT(0);
	}
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::fUpdateActionButtons() {
	bool buttonState = !ui.textFind->text().isEmpty();
	ui.buttonFind->setEnabled(buttonState);
}

//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
void DialogReplace::UpdateReplaceActionButtons() {

#if defined(REPLACE_SCOPE)
	// Is there any text in the search for field 
	bool searchText = !ui.textFind->text().isEmpty();

	switch (replaceScope_) {
	case REPL_SCOPE_WIN:
		// Enable all buttons, if there is any text in the search field. 
		rSetActionButtons(searchText, searchText, searchText, searchText);
		break;

	case REPL_SCOPE_SEL:
		// Only enable Replace All, if a selection exists and text in search field. 
		rSetActionButtons(false, false, false, searchText && window_->wasSelected_);
		break;

	case REPL_SCOPE_MULTI:
		// Only enable Replace All, if text in search field. 
		rSetActionButtons(false, false, false, searchText);
		break;
	}
#else
	// Is there any text in the search for field 
	bool searchText = !ui.textFind->text().isEmpty();
	rSetActionButtons(searchText, searchText, searchText, searchText, searchText && window_->wasSelected_, searchText && (countWritableWindows() > 1));
#endif
}

#if defined(REPLACE_SCOPE)
void DialogReplace::rSetActionButtons(bool replaceBtn, bool replaceFindBtn, bool replaceAndFindBtn, bool replaceAllBtn) {
	
	ui.buttonReplace->setEnabled(replaceBtn);
	ui.buttonFind->setEnabled(replaceFindBtn);
	ui.buttonReplaceFind->setEnabled(replaceAndFindBtn);
	ui.buttonAll->setEnabled(replaceAllBtn);
}
#else
void DialogReplace::rSetActionButtons(bool replaceBtn, bool replaceFindBtn, bool replaceAndFindBtn, bool replaceInWinBtn, bool replaceInSelBtn, bool replaceAllBtn) {

	ui.buttonReplace->setEnabled(replaceBtn);
	ui.buttonFind->setEnabled(replaceFindBtn);
	ui.buttonReplaceFind->setEnabled(replaceAndFindBtn);
	ui.buttonWindow->setEnabled(replaceInWinBtn);
	ui.buttonSelection->setEnabled(replaceInSelBtn);
	ui.buttonMulti->setEnabled(replaceAllBtn); // all is multi here
}
#endif

/*
** Fetch and verify (particularly regular expression) search and replace
** strings and search type from the Replace dialog.  If the strings are ok,
** save a copy in the search history, copy them in to "searchString",
** "replaceString', which are assumed to be at least SEARCHMAX in length,
** return search type in "searchType", and return TRUE as the function
** value.  Otherwise, return FALSE.
*/
bool DialogReplace::getReplaceDlogInfo(SearchDirection *direction, QString *searchString, QString *replaceString, SearchType *searchType) {

	/* Get the search and replace strings, search type, and direction
	   from the dialog */
	QString replaceText     = ui.textFind->text();
	QString replaceWithText = ui.textReplace->text();

	if (ui.checkRegex->isChecked()) {
		int regexDefault;
		if (ui.checkCase->isChecked()) {
			*searchType = SEARCH_REGEX;
			regexDefault = REDFLT_STANDARD;
		} else {
			*searchType = SEARCH_REGEX_NOCASE;
			regexDefault = REDFLT_CASE_INSENSITIVE;
		}
		/* If the search type is a regular expression, test compile it
		   immediately and present error messages */
		try {
			auto compiledRE = std::make_unique<regexp>(replaceText.toLatin1().data(), regexDefault);
		} catch(const regex_error &e) {
            QMessageBox::warning(this, tr("Search String"), tr("Please respecify the search string:\n%1").arg(QString::fromLatin1(e.what())));
            return false;
		}
	} else {
		if (ui.checkCase->isChecked()) {
			if (ui.checkWord->isChecked())
				*searchType = SEARCH_CASE_SENSE_WORD;
			else
				*searchType = SEARCH_CASE_SENSE;
		} else {
			if (ui.checkWord->isChecked())
				*searchType = SEARCH_LITERAL_WORD;
			else
				*searchType = SEARCH_LITERAL;
		}
	}

	*direction = ui.checkBackward->isChecked() ? SEARCH_BACKWARD : SEARCH_FORWARD;

	// Return strings 
	if (replaceText.size() >= SEARCHMAX) {
		QMessageBox::warning(this, tr("String too long"), tr("Search string too long."));
        return false;
	}
	if (replaceWithText.size() >= SEARCHMAX) {
		QMessageBox::warning(this, tr("String too long"), tr("Replace string too long."));
        return false;
	}

    *searchString  = replaceText;
    *replaceString = replaceWithText;
    return true;
}


/*
** Collects a list of writable windows (sorted by file name).
** The previous list, if any is freed first.
**/
void DialogReplace::collectWritableWindows() {

    QVector<DocumentWidget *> windows;

    for(DocumentWidget *document : DocumentWidget::allDocuments()) {
        if (!document->lockReasons_.isAnyLocked()) {
            windows.push_back(document);
        }
    }

    std::sort(windows.begin(), windows.end(), [](const DocumentWidget *lhs, const DocumentWidget *rhs) {
		return lhs->filename_ < rhs->filename_;
	});

	window_->writableWindows_  = windows;
}


//------------------------------------------------------------------------------
// name: 
//------------------------------------------------------------------------------
bool DialogReplace::keepDialog() const {
	return ui.checkKeep->isChecked();
}

//------------------------------------------------------------------------------
// name:
//------------------------------------------------------------------------------
void DialogReplace::setDocument(DocumentWidget *document) {
    document_ = document;
    if(keepDialog()) {
        setWindowTitle(tr("Replace (in %1)").arg(document_->filename_));
    }
}
