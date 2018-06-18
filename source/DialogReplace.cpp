
#include "DialogReplace.h"
#include "DialogMultiReplace.h"
#include "DocumentWidget.h"
#include "MainWindow.h"
#include "preferences.h"
#include "Regex.h"
#include "Util/regex.h"
#include "Search.h"

#include <QClipboard>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMimeData>

namespace {

/*
** Count no. of writable windows, but first update the status of all files.
*/
int countWritableWindows() {

    int nWritable = 0;

    std::vector<DocumentWidget *> documents = DocumentWidget::allDocuments();
    size_t nBefore = documents.size();

    auto first = documents.begin();
    auto last  = documents.end();

    for(auto it = first; it != last; ++it) {
        DocumentWidget *document = *it;

        /* We must be very careful! The status check may trigger a pop-up
           dialog when the file has changed on disk, and the user may destroy
           arbitrary windows in response. */
        document->CheckForChangesToFileEx();

        std::vector<DocumentWidget *> afterDocuments = DocumentWidget::allDocuments();
        size_t nAfter = afterDocuments.size();

        if (nAfter != nBefore) {
            // The user has destroyed a file; start counting all over again
            nBefore = nAfter;

            documents = afterDocuments;
            first     = documents.begin();
            last      = documents.end();
            it        = first;
            nWritable = 0;
            continue;
        }

        if (!document->lockReasons_.isAnyLocked()) {
            ++nWritable;
        }
    }

    return nWritable;
}

}

/**
 * @brief DialogReplace::DialogReplace
 * @param window
 * @param document
 * @param f
 */
DialogReplace::DialogReplace(MainWindow *window, DocumentWidget *document, Qt::WindowFlags f) : Dialog(window, f), window_(window), document_(document) {
	
	ui.setupUi(this);
	
	lastRegexCase_   = true;
	lastLiteralCase_ = false;
}

/**
 * @brief DialogReplace::showEvent
 * @param event
 */
void DialogReplace::showEvent(QShowEvent *event) {
    Dialog::showEvent(event);
	ui.textFind->setFocus();    
}

/**
 * @brief DialogReplace::keyPressEvent
 * @param event
 */
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
        if (index != 0 && Search::historyIndex(index) == -1) {
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
            searchType = Preferences::GetPrefSearch();
        } else {
            const Search::HistoryEntry *entry = Search::HistoryByIndex(index);
            Q_ASSERT(entry);

            searchStr  = entry->search;
            replaceStr = entry->replace;
            searchType = entry->type;
		}

		// Set the buttons and fields with the selected search type 
		initToggleButtons(searchType);

		ui.textFind->setText(searchStr);
        ui.textReplace->setText(replaceStr);

		// Set the state of the Find ... button 
		updateActionButtons();

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
        if (index != 0 && Search::historyIndex(index) == -1) {
			QApplication::beep();
			return;
		}

		// change only the replace field information 
		if (index == 0) {
			ui.textReplace->setText(QString());
		} else {
            ui.textReplace->setText(Search::HistoryByIndex(index)->replace);
		}

		window_->rHistIndex_ = index;
	}
	
	QDialog::keyPressEvent(event);
}

/**
 * @brief DialogReplace::on_checkBackward_toggled
 * @param checked
 */
void DialogReplace::on_checkBackward_toggled(bool checked) {
	Q_UNUSED(checked);
}

/**
 * @brief DialogReplace::on_checkKeep_toggled
 * @param checked
 */
void DialogReplace::on_checkKeep_toggled(bool checked) {
	if (checked) {
        setWindowTitle(tr("Find/Replace (in %1)").arg(document_->filename_));
	} else {
		setWindowTitle(tr("Find/Replace"));
	}
}

/**
 * @brief DialogReplace::on_textFind_textChanged
 * @param text
 */
void DialogReplace::on_textFind_textChanged(const QString &text) {
	Q_UNUSED(text);
	UpdateReplaceActionButtons();
}

/**
 * @brief DialogReplace::on_buttonFind_clicked
 */
void DialogReplace::on_buttonFind_clicked() {
	
    QString searchString;
    QString replaceString;
    Direction direction;
	SearchType searchType;
	
	// Validate and fetch the find and replace strings from the dialog 
    if (!getReplaceDlogInfo(&direction, &searchString, &replaceString, &searchType))
		return;

	// Set the initial focus of the dialog back to the search string	
	ui.textFind->setFocus();

	// Find the text and mark it 
    window_->action_Find(document_, searchString, direction, searchType, Preferences::GetPrefSearchWraps());

	/* Doctor the search history generated by the action to include the
	   replace string (if any), so the replace string can be used on
	   subsequent replaces, even though no actual replacement was done. */
    if (Search::historyIndex(1) != -1 && Search::HistoryByIndex(1)->search == searchString) {
        Search::HistoryByIndex(1)->replace = replaceString;
	}

	if (!keepDialog()) {
		hide();
	}
}

/**
 * @brief DialogReplace::on_buttonReplace_clicked
 */
void DialogReplace::on_buttonReplace_clicked() {

    QString searchString;
    QString replaceString;
    Direction direction;
	SearchType searchType;
	
	// Validate and fetch the find and replace strings from the dialog 
    if (!getReplaceDlogInfo(&direction, &searchString, &replaceString, &searchType)) {
		return;
    }

	// Set the initial focus of the dialog back to the search string 
	ui.textFind->setFocus();

	// Find the text and replace it 
    window_->action_Replace(document_, searchString, replaceString, direction, searchType, Preferences::GetPrefSearchWraps());

	// Pop down the dialog 
	if (!keepDialog()) {
		hide();
	}
}

/**
 * @brief DialogReplace::on_buttonReplaceFind_clicked
 */
void DialogReplace::on_buttonReplaceFind_clicked() {

    QString searchString;
    QString replaceString;
    Direction direction;
	SearchType searchType;
	
	// Validate and fetch the find and replace strings from the dialog 
    if (!getReplaceDlogInfo(&direction, &searchString, &replaceString, &searchType))
		return;

	// Set the initial focus of the dialog back to the search string 
	ui.textFind->setFocus();

	// Find the text and replace it 
    window_->action_Replace_Find(document_, searchString, replaceString, direction, searchType, Preferences::GetPrefSearchWraps());

	// Pop down the dialog 
	if (!keepDialog()) {
		hide();
	}
}

/**
 * @brief DialogReplace::on_buttonWindow_clicked
 */
void DialogReplace::on_buttonWindow_clicked() {

    QString searchString;
    QString replaceString;
    Direction direction;
	SearchType searchType;
	

	// Validate and fetch the find and replace strings from the dialog 
    if (!getReplaceDlogInfo(&direction, &searchString, &replaceString, &searchType))
		return;

	// Set the initial focus of the dialog back to the search string	
	ui.textFind->setFocus();

	// do replacement 
    window_->action_Replace_All(document_, searchString, replaceString, searchType);

	// Pop down the dialog 
	if (!keepDialog()) {
		hide();
	}
}

/**
 * @brief DialogReplace::on_buttonSelection_clicked
 */
void DialogReplace::on_buttonSelection_clicked() {

    QString searchString;
    QString replaceString;
    Direction direction;
	SearchType searchType;
	

	// Validate and fetch the find and replace strings from the dialog 
    if (!getReplaceDlogInfo(&direction, &searchString, &replaceString, &searchType))
		return;

	// Set the initial focus of the dialog back to the search string 
	ui.textFind->setFocus();

	// do replacement 
    window_->action_Replace_In_Selection(document_, searchString, replaceString, searchType);

	// Pop down the dialog 
	if (!keepDialog()) {
		hide();
	}
}

/**
 * @brief DialogReplace::on_buttonMulti_clicked
 */
void DialogReplace::on_buttonMulti_clicked() {

    QString searchString;
    QString replaceString;
    Direction direction;
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
        dialogMultiReplace_ = new DialogMultiReplace(this);
	}

	// Prepare a list of writable windows 
	collectWritableWindows();

	// Initialize/update the list of files. 
    dialogMultiReplace_->uploadFileListItems();

	// Display the dialog 
    dialogMultiReplace_->exec();
}



/**
 * @brief DialogReplace::on_checkRegex_toggled
 * @param checked
 */
void DialogReplace::on_checkRegex_toggled(bool checked) {


	bool searchRegex = checked;
	bool searchCaseSense = ui.checkCase->isChecked();

	// In sticky mode, restore the state of the Case Sensitive button 
    if (Preferences::GetPrefStickyCaseSenseBtn()) {
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

/**
 * @brief DialogReplace::on_checkCase_toggled
 * @param checked
 */
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

/**
 * @brief DialogReplace::setTextField
 * @param document
 */
void DialogReplace::setTextField(DocumentWidget *document) {

    QString initialText;

    if (Preferences::GetPrefFindReplaceUsesSelection()) {
        initialText = document->GetAnySelectionEx(/*beep_on_error=*/false);
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
    case SearchType::Literal:
		lastLiteralCase_ = false;
		lastRegexCase_   = true;
		ui.checkRegex->setChecked(false);
		ui.checkCase->setChecked(false);
		if (true) {
			ui.checkWord->setChecked(false);
			ui.checkWord->setEnabled(true);
		}
		break;
    case SearchType::CaseSense:
		lastLiteralCase_ = true;
		lastRegexCase_   = true;
		ui.checkRegex->setChecked(false);
		ui.checkCase->setChecked(true);
		if (true) {
			ui.checkWord->setChecked(false);
			ui.checkWord->setEnabled(true);
		}
		break;
    case SearchType::LiteralWord:
		lastLiteralCase_ = false;
		lastRegexCase_   = true;
		ui.checkRegex->setChecked(false);
		ui.checkCase->setChecked(false);
		if (true) {
			ui.checkWord->setChecked(true);
			ui.checkWord->setEnabled(true);
		}
		break;
    case SearchType::CaseSenseWord:
		lastLiteralCase_ = true;
		lastRegexCase_   = true;
		ui.checkRegex->setChecked(false);
		ui.checkCase->setChecked(true);
		if (true) {
			ui.checkWord->setChecked(true);
			ui.checkWord->setEnabled(true);
		}
		break;
    case SearchType::Regex:
		lastLiteralCase_ = false;
		lastRegexCase_   = true;
		ui.checkRegex->setChecked(true);
		ui.checkCase->setChecked(true);
		if (true) {
			ui.checkWord->setChecked(false);
			ui.checkWord->setEnabled(false);
		}
		break;
    case SearchType::RegexNoCase:
		lastLiteralCase_ = false;
		lastRegexCase_   = false;
		ui.checkRegex->setChecked(true);
		ui.checkCase->setChecked(false);
		if (true) {
			ui.checkWord->setChecked(false);
			ui.checkWord->setEnabled(false);
		}
		break;
	}
}

/**
 * @brief DialogReplace::updateActionButtons
 */
void DialogReplace::updateActionButtons() {
	bool buttonState = !ui.textFind->text().isEmpty();
	ui.buttonFind->setEnabled(buttonState);
}

/**
 * @brief DialogReplace::UpdateReplaceActionButtons
 */
void DialogReplace::UpdateReplaceActionButtons() {

	// Is there any text in the search for field 
	bool searchText = !ui.textFind->text().isEmpty();
	rSetActionButtons(searchText, searchText, searchText, searchText, searchText && window_->wasSelected_, searchText && (countWritableWindows() > 1));
}

/**
 * @brief DialogReplace::rSetActionButtons
 * @param replaceBtn
 * @param replaceFindBtn
 * @param replaceAndFindBtn
 * @param replaceInWinBtn
 * @param replaceInSelBtn
 * @param replaceAllBtn
 */
void DialogReplace::rSetActionButtons(bool replaceBtn, bool replaceFindBtn, bool replaceAndFindBtn, bool replaceInWinBtn, bool replaceInSelBtn, bool replaceAllBtn) {

	ui.buttonReplace->setEnabled(replaceBtn);
	ui.buttonFind->setEnabled(replaceFindBtn);
	ui.buttonReplaceFind->setEnabled(replaceAndFindBtn);
	ui.buttonWindow->setEnabled(replaceInWinBtn);
	ui.buttonSelection->setEnabled(replaceInSelBtn);
	ui.buttonMulti->setEnabled(replaceAllBtn); // all is multi here
}


/*
** Fetch and verify (particularly regular expression) search and replace
** strings and search type from the Replace dialog.  If the strings are ok,
** save a copy in the search history, copy them in to "searchString",
** "replaceString'. return search type in "searchType", and return TRUE as
** the function value.  Otherwise, return FALSE.
*/
bool DialogReplace::getReplaceDlogInfo(Direction *direction, QString *searchString, QString *replaceString, SearchType *searchType) {

	/* Get the search and replace strings, search type, and direction
	   from the dialog */
	QString replaceText     = ui.textFind->text();
	QString replaceWithText = ui.textReplace->text();

	if (ui.checkRegex->isChecked()) {
		int regexDefault;
		if (ui.checkCase->isChecked()) {
            *searchType = SearchType::Regex;
			regexDefault = REDFLT_STANDARD;
		} else {
            *searchType = SearchType::RegexNoCase;
			regexDefault = REDFLT_CASE_INSENSITIVE;
		}
		/* If the search type is a regular expression, test compile it
		   immediately and present error messages */
		try {
            auto compiledRE = make_regex(replaceText, regexDefault);
		} catch(const RegexError &e) {
            QMessageBox::warning(this, tr("Search String"), tr("Please respecify the search string:\n%1").arg(QString::fromLatin1(e.what())));
            return false;
		}
	} else {
		if (ui.checkCase->isChecked()) {
			if (ui.checkWord->isChecked())
                *searchType = SearchType::CaseSenseWord;
			else
                *searchType = SearchType::CaseSense;
		} else {
			if (ui.checkWord->isChecked())
                *searchType = SearchType::LiteralWord;
			else
                *searchType = SearchType::Literal;
		}
	}

    *direction = ui.checkBackward->isChecked() ? Direction::Backward : Direction::Forward;

    *searchString  = replaceText;
    *replaceString = replaceWithText;
    return true;
}


/*
** Collects a list of writable windows (sorted by file name).
**/
void DialogReplace::collectWritableWindows() {

    std::vector<DocumentWidget *> documents;

    for(DocumentWidget *document : DocumentWidget::allDocuments()) {
        if (!document->lockReasons().isAnyLocked()) {
            documents.push_back(document);
        }
    }

    std::sort(documents.begin(), documents.end(), [](const DocumentWidget *lhs, const DocumentWidget *rhs) {
        return lhs->FileName() < rhs->FileName();
	});

    writableWindows_ = documents;
}


/**
 * @brief DialogReplace::keepDialog
 * @return
 */
bool DialogReplace::keepDialog() const {
	return ui.checkKeep->isChecked();
}

/**
 * @brief DialogReplace::setDocument
 * @param document
 */
void DialogReplace::setDocument(DocumentWidget *document) {
    document_ = document;
    if(keepDialog()) {
        setWindowTitle(tr("Replace (in %1)").arg(document_->filename_));
    }
}
