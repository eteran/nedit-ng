
#include "DialogFind.h"
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
#include <QTimer>

/**
 * @brief DialogFind::DialogFind
 * @param window
 * @param document
 * @param f
 */
DialogFind::DialogFind(MainWindow *window, DocumentWidget *document, Qt::WindowFlags f) : Dialog(window, f), window_(window), document_(document) {
    ui.setupUi(this);

    lastRegexCase_   = true;
    lastLiteralCase_ = false;
}

/**
 * @brief DialogFind::showEvent
 * @param event
 */
void DialogFind::showEvent(QShowEvent *event) {    
    Dialog::showEvent(event);
    QTimer::singleShot(0, ui.textFind, SLOT(setFocus()));
}

/**
 * @brief DialogFind::keyPressEvent
 * @param event
 */
void DialogFind::keyPressEvent(QKeyEvent *event) {

	if(ui.textFind->hasFocus()) {
		int index = window_->fHistIndex_;

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
		SearchType searchType;
		if (index == 0) {
            searchStr  = QString();
            searchType = Preferences::GetPrefSearch();
        } else {
            const Search::HistoryEntry *entry = Search::HistoryByIndex(index);
            Q_ASSERT(entry);
            searchStr  = entry->search;
            searchType = entry->type;
		}

		// Set the buttons and fields with the selected search type 
		initToggleButtons(searchType);

        ui.textFind->setText(searchStr);

		// Set the state of the Find ... button 
        updateActionButtons();

		window_->fHistIndex_ = index;
	}
	
	QDialog::keyPressEvent(event);
}

/**
 * @brief DialogFind::on_checkKeep_toggled
 * @param checked
 */
void DialogFind::on_checkKeep_toggled(bool checked) {
	if (checked) {
        setWindowTitle(tr("Find (in %1)").arg(document_->FileName()));
	} else {
		setWindowTitle(tr("Find"));
	}
}

/**
 * @brief DialogFind::updateActionButtons
 */
void DialogFind::updateActionButtons() {
	bool buttonState = !ui.textFind->text().isEmpty();
	ui.buttonFind->setEnabled(buttonState);
}

/**
 * @brief DialogFind::on_textFind_textChanged
 * @param text
 */
void DialogFind::on_textFind_textChanged(const QString &text) {
	Q_UNUSED(text);
    updateActionButtons();
}

/*
** Shared routine for replace and find dialogs and i-search bar to initialize
** the state of the regex/case/word toggle buttons, and the sticky case
** sensitivity states.
*/
void DialogFind::initToggleButtons(SearchType searchType) {
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
 * @brief DialogFind::setTextField
 * @param document
 */
void DialogFind::setTextField(DocumentWidget *document) {

    QString initialText;

    if (Preferences::GetPrefFindReplaceUsesSelection()) {
        initialText = document->GetAnySelectionEx(/*beep_on_error=*/false);
    }

    // Update the field
    ui.textFind->setText(initialText);
}

/**
 * @brief DialogFind::on_buttonFind_clicked
 */
void DialogFind::on_buttonFind_clicked() {

    Direction direction;
	SearchType searchType;
	
	// fetch find string, direction and type from the dialog 
    QString searchString;
    if (!getFindDlogInfoEx(&direction, &searchString, &searchType)) {
		return;
    }

	// Set the initial focus of the dialog back to the search string
	ui.textFind->setFocus();

	// find the text and mark it 
    window_->action_Find(document_, searchString, direction, searchType, Preferences::GetPrefSearchWraps());

	// pop down the dialog 
	if (!keepDialog()) {
		hide();
	}
}

/*
** Fetch and verify (particularly regular expression) search string,
** direction, and search type from the Find dialog.  If the search string
** is ok, save a copy in the search history, copy it to "searchString",
** return search type in "searchType", and return TRUE as the function value.
** Otherwise, return FALSE.
*/
bool DialogFind::getFindDlogInfoEx(Direction *direction, QString *searchString, SearchType *searchType) {

	// Get the search string, search type, and direction from the dialog 
	QString findText = ui.textFind->text();

	if (ui.checkRegex->isChecked()) {
		int regexDefault;
		if (ui.checkCase->isChecked()) {
            *searchType  = SearchType::Regex;
			regexDefault = REDFLT_STANDARD;
		} else {
            *searchType  = SearchType::RegexNoCase;
			regexDefault = REDFLT_CASE_INSENSITIVE;
		}
		/* If the search type is a regular expression, test compile it
		   immediately and present error messages */
		try {
            auto compiledRE = make_regex(findText, regexDefault);
		} catch(const RegexError &e) {
            QMessageBox::warning(this, tr("Regex Error"), tr("Please respecify the search string:\n%1").arg(QString::fromLatin1(e.what())));
			return false;
		}
	} else {
		if (ui.checkCase->isChecked()) {
			if (ui.checkWord->isChecked()) {
                *searchType = SearchType::CaseSenseWord;
			} else {
                *searchType = SearchType::CaseSense;
			}
		} else {
			if (ui.checkWord->isChecked()) {
                *searchType = SearchType::LiteralWord;
			} else {
                *searchType = SearchType::Literal;
			}
		}
	}

    *direction = ui.checkBackward->isChecked() ? Direction::Backward : Direction::Forward;

    if (Search::isRegexType(*searchType)) {
        // NOTE(eteran): nothing...
	}

    *searchString = findText;
    return true;
}

/**
 * @brief DialogFind::on_checkRegex_toggled
 * @param checked
 */
void DialogFind::on_checkRegex_toggled(bool checked) {

	bool searchRegex     = checked;
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
 * @brief DialogFind::on_checkCase_toggled
 * @param checked
 */
void DialogFind::on_checkCase_toggled(bool checked) {

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
 * @brief DialogFind::keepDialog
 * @return
 */
bool DialogFind::keepDialog() const {
	return ui.checkKeep->isChecked();
}

/**
 * @brief DialogFind::setDocument
 * @param document
 */
void DialogFind::setDocument(DocumentWidget *document) {
    document_ = document;
    if(keepDialog()) {
        setWindowTitle(tr("Find (in %1)").arg(document_->FileName()));
    }
}
