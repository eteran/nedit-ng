
#include "DialogReplace.h"
#include "DialogMultiReplace.h"
#include "DocumentWidget.h"
#include "MainWindow.h"
#include "Preferences.h"
#include "Regex.h"
#include "Search.h"
#include "Util/regex.h"

#include <QClipboard>
#include <QKeyEvent>
#include <QMessageBox>

namespace {

/*
** Count no. of writable windows, but first update the status of all files.
*/
int countWritableWindows() {

	int nWritable = 0;

	std::vector<DocumentWidget *> documents = DocumentWidget::allDocuments();
	size_t nBefore                          = documents.size();

	auto first = documents.begin();
	auto last  = documents.end();

	for (auto it = first; it != last; ++it) {
		DocumentWidget *document = *it;

		/* We must be very careful! The status check may trigger a pop-up
		   dialog when the file has changed on disk, and the user may destroy
		   arbitrary windows in response. */
		document->checkForChangesToFile();

		const std::vector<DocumentWidget *> afterDocuments = DocumentWidget::allDocuments();
		const size_t nAfter                                = afterDocuments.size();

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

		if (!document->lockReasons().isAnyLocked()) {
			++nWritable;
		}
	}

	return nWritable;
}

/*
** Collects a list of writable windows (sorted by file name).
**/
std::vector<DocumentWidget *> collectWritableWindows() {

	std::vector<DocumentWidget *> documents;
	std::vector<DocumentWidget *> allDocuments = DocumentWidget::allDocuments();

	std::copy_if(allDocuments.begin(), allDocuments.end(), std::back_inserter(documents), [](DocumentWidget *document) {
		return (!document->lockReasons().isAnyLocked());
	});

	std::sort(documents.begin(), documents.end(), [](const DocumentWidget *lhs, const DocumentWidget *rhs) {
		return lhs->filename() < rhs->filename();
	});

	return documents;
}

}

/**
 * @brief DialogReplace::DialogReplace
 * @param window
 * @param document
 * @param f
 */
DialogReplace::DialogReplace(MainWindow *window, DocumentWidget *document, Qt::WindowFlags f)
	: Dialog(window, f), window_(window), document_(document) {
	ui.setupUi(this);
	connectSlots();

	ui.textFind->installEventFilter(this);
	ui.textReplace->installEventFilter(this);

	Dialog::shrinkToFit(this);
}

/**
 * @brief DialogReplace::connectSlots
 */
void DialogReplace::connectSlots() {
	connect(ui.buttonFind, &QPushButton::clicked, this, &DialogReplace::buttonFind_clicked);
	connect(ui.buttonReplace, &QPushButton::clicked, this, &DialogReplace::buttonReplace_clicked);
	connect(ui.buttonReplaceFind, &QPushButton::clicked, this, &DialogReplace::buttonReplaceFind_clicked);
	connect(ui.buttonWindow, &QPushButton::clicked, this, &DialogReplace::buttonWindow_clicked);
	connect(ui.buttonSelection, &QPushButton::clicked, this, &DialogReplace::buttonSelection_clicked);
	connect(ui.buttonMulti, &QPushButton::clicked, this, &DialogReplace::buttonMulti_clicked);
	connect(ui.checkRegex, &QCheckBox::toggled, this, &DialogReplace::checkRegex_toggled);
	connect(ui.checkCase, &QCheckBox::toggled, this, &DialogReplace::checkCase_toggled);
	connect(ui.checkKeep, &QCheckBox::toggled, this, &DialogReplace::checkKeep_toggled);
	connect(ui.textFind, &QLineEdit::textChanged, this, &DialogReplace::textFind_textChanged);
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
 * @brief DialogReplace::eventFilter
 * @param obj
 * @param ev
 * @return
 */
bool DialogReplace::eventFilter(QObject *obj, QEvent *ev) {

	if (obj == ui.textFind && ev->type() == QEvent::KeyPress) {
		auto event = static_cast<QKeyEvent *>(ev);

		int index = window_->rHistIndex_;

		// only process up and down arrow keys
		if (event->key() != Qt::Key_Up && event->key() != Qt::Key_Down) {
			return false;
		}

		// increment or decrement the index depending on which arrow was pressed
		index += (event->key() == Qt::Key_Up) ? 1 : -1;

		// if the index is out of range, beep and return
		if (index != 0 && Search::historyIndex(index) == -1) {
			QApplication::beep();
			return true;
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
		updateFindButton();

		window_->rHistIndex_ = index;

		return true;
	}

	if (obj == ui.textReplace && ev->type() == QEvent::KeyPress) {
		auto event = static_cast<QKeyEvent *>(ev);
		int index  = window_->rHistIndex_;

		// only process up and down arrow keys
		if (event->key() != Qt::Key_Up && event->key() != Qt::Key_Down) {
			return false;
		}

		// increment or decrement the index depending on which arrow was pressed
		index += (event->key() == Qt::Key_Up) ? 1 : -1;

		// if the index is out of range, beep and return
		if (index != 0 && Search::historyIndex(index) == -1) {
			QApplication::beep();
			return true;
		}

		// change only the replace field information
		if (index == 0) {
			ui.textReplace->setText(QString());
		} else {
			ui.textReplace->setText(Search::HistoryByIndex(index)->replace);
		}

		window_->rHistIndex_ = index;
		return true;
	}

	return false;
}

/**
 * @brief DialogReplace::checkKeep_toggled
 * @param checked
 */
void DialogReplace::checkKeep_toggled(bool checked) {
	if (checked && document_) {
		setWindowTitle(tr("Find/Replace (in %1)").arg(document_->filename()));
	} else {
		setWindowTitle(tr("Find/Replace"));
	}
}

/**
 * @brief DialogReplace::textFind_textChanged
 * @param text
 */
void DialogReplace::textFind_textChanged(const QString &text) {
	Q_UNUSED(text)
	UpdateReplaceActionButtons();
}

/**
 * @brief DialogReplace::buttonFind_clicked
 */
void DialogReplace::buttonFind_clicked() {

	// Validate and fetch the find and replace strings from the dialog
	std::optional<Fields> fields = readFields();
	if (!fields) {
		return;
	}

	if (!document_) {
		return;
	}

	// Set the initial focus of the dialog back to the search string
	ui.textFind->setFocus();

	// Find the text and mark it
	window_->action_Find(
		document_,
		fields->searchString,
		fields->direction,
		fields->searchType,
		Preferences::GetPrefSearchWraps());

	/* Doctor the search history generated by the action to include the
	   replace string (if any), so the replace string can be used on
	   subsequent replaces, even though no actual replacement was done. */
	if (Search::historyIndex(1) != -1 && Search::HistoryByIndex(1)->search == fields->searchString) {
		Search::HistoryByIndex(1)->replace = fields->replaceString;
	}

	if (!keepDialog()) {
		hide();
	}
}

/**
 * @brief DialogReplace::buttonReplace_clicked
 */
void DialogReplace::buttonReplace_clicked() {

	// Validate and fetch the find and replace strings from the dialog
	std::optional<Fields> fields = readFields();
	if (!fields) {
		return;
	}

	if (!document_) {
		return;
	}

	// Set the initial focus of the dialog back to the search string
	ui.textFind->setFocus();

	// Find the text and replace it
	window_->action_Replace(
		document_,
		fields->searchString,
		fields->replaceString,
		fields->direction,
		fields->searchType,
		Preferences::GetPrefSearchWraps());

	if (!keepDialog()) {
		hide();
	}
}

/**
 * @brief DialogReplace::buttonReplaceFind_clicked
 */
void DialogReplace::buttonReplaceFind_clicked() {

	// Validate and fetch the find and replace strings from the dialog
	std::optional<Fields> fields = readFields();
	if (!fields) {
		return;
	}

	if (!document_) {
		return;
	}

	// Set the initial focus of the dialog back to the search string
	ui.textFind->setFocus();

	// Find the text and replace it
	window_->action_Replace_Find(
		document_,
		fields->searchString,
		fields->replaceString,
		fields->direction,
		fields->searchType,
		Preferences::GetPrefSearchWraps());

	if (!keepDialog()) {
		hide();
	}
}

/**
 * @brief DialogReplace::buttonWindow_clicked
 */
void DialogReplace::buttonWindow_clicked() {

	// Validate and fetch the find and replace strings from the dialog
	std::optional<Fields> fields = readFields();
	if (!fields) {
		return;
	}

	if (!document_) {
		return;
	}

	// Set the initial focus of the dialog back to the search string
	ui.textFind->setFocus();

	// do replacement
	window_->action_Replace_All(
		document_,
		fields->searchString,
		fields->replaceString,
		fields->searchType);

	if (!keepDialog()) {
		hide();
	}
}

/**
 * @brief DialogReplace::buttonSelection_clicked
 */
void DialogReplace::buttonSelection_clicked() {

	// Validate and fetch the find and replace strings from the dialog
	std::optional<Fields> fields = readFields();
	if (!fields) {
		return;
	}

	if (!document_) {
		return;
	}

	// Set the initial focus of the dialog back to the search string
	ui.textFind->setFocus();

	// do replacement
	window_->action_Replace_In_Selection(
		document_,
		fields->searchString,
		fields->replaceString,
		fields->searchType);

	if (!keepDialog()) {
		hide();
	}
}

/**
 * @brief DialogReplace::buttonMulti_clicked
 */
void DialogReplace::buttonMulti_clicked() {

	// Validate and fetch the find and replace strings from the dialog
	std::optional<Fields> fields = readFields();
	if (!fields) {
		return;
	}

	// Don't let the user select files when no replacement can be made
	if (fields->searchString.isEmpty()) {
		// Set the initial focus of the dialog back to the search string
		ui.textFind->setFocus();

		if (!keepDialog()) {
			hide();
		}
		return;
	}

	static QPointer<DialogMultiReplace> dialogMultiReplace;

	// Create the dialog if it doesn't already exist
	if (!dialogMultiReplace) {
		dialogMultiReplace = new DialogMultiReplace(this);
	}

	// temporary list of writable documents, used during multi-file replacements
	const std::vector<DocumentWidget *> writeableDocuments = collectWritableWindows();

	// Initialize/update the list of files.
	dialogMultiReplace->uploadFileListItems(writeableDocuments);

	// Display the dialog
	dialogMultiReplace->exec();
}

/**
 * @brief DialogReplace::checkRegex_toggled
 * @param checked
 */
void DialogReplace::checkRegex_toggled(bool checked) {

	const bool searchRegex     = checked;
	const bool searchCaseSense = ui.checkCase->isChecked();

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
 * @brief DialogReplace::checkCase_toggled
 * @param checked
 */
void DialogReplace::checkCase_toggled(bool checked) {

	const bool searchCaseSense = checked;

	/* Save the state of the Case Sensitive button
	   depending on the state of the Regex button*/
	if (ui.checkRegex->isChecked()) {
		lastRegexCase_ = searchCaseSense;
	} else {
		lastLiteralCase_ = searchCaseSense;
	}
}

/**
 * @brief DialogReplace::setTextFieldFromDocument
 * @param document
 */
void DialogReplace::setTextFieldFromDocument(DocumentWidget *document) {

	QString initialText;

	if (Preferences::GetPrefFindReplaceUsesSelection()) {
		initialText = document->getAnySelection();
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
		ui.checkWord->setChecked(false);
		ui.checkWord->setEnabled(true);
		break;
	case SearchType::CaseSense:
		lastLiteralCase_ = true;
		lastRegexCase_   = true;
		ui.checkRegex->setChecked(false);
		ui.checkCase->setChecked(true);
		ui.checkWord->setChecked(false);
		ui.checkWord->setEnabled(true);
		break;
	case SearchType::LiteralWord:
		lastLiteralCase_ = false;
		lastRegexCase_   = true;
		ui.checkRegex->setChecked(false);
		ui.checkCase->setChecked(false);
		ui.checkWord->setChecked(true);
		ui.checkWord->setEnabled(true);
		break;
	case SearchType::CaseSenseWord:
		lastLiteralCase_ = true;
		lastRegexCase_   = true;
		ui.checkRegex->setChecked(false);
		ui.checkCase->setChecked(true);
		ui.checkWord->setChecked(true);
		ui.checkWord->setEnabled(true);
		break;
	case SearchType::Regex:
		lastLiteralCase_ = false;
		lastRegexCase_   = true;
		ui.checkRegex->setChecked(true);
		ui.checkCase->setChecked(true);
		ui.checkWord->setChecked(false);
		ui.checkWord->setEnabled(false);
		break;
	case SearchType::RegexNoCase:
		lastLiteralCase_ = false;
		lastRegexCase_   = false;
		ui.checkRegex->setChecked(true);
		ui.checkCase->setChecked(false);
		ui.checkWord->setChecked(false);
		ui.checkWord->setEnabled(false);
		break;
	}
}

/**
 * @brief DialogReplace::updateFindButton
 */
void DialogReplace::updateFindButton() {
	const bool buttonState = !ui.textFind->text().isEmpty();
	ui.buttonFind->setEnabled(buttonState);
}

/**
 * @brief DialogReplace::UpdateReplaceActionButtons
 */
void DialogReplace::UpdateReplaceActionButtons() {

	// Is there any text in the search for field
	const bool searchText = !ui.textFind->text().isEmpty();
	setActionButtons(searchText, searchText, searchText, searchText, searchText && document_->info_->wasSelected, searchText && (countWritableWindows() > 1));
}

/**
 * @brief DialogReplace::setActionButtons
 * @param replaceBtn
 * @param replaceFindBtn
 * @param replaceAndFindBtn
 * @param replaceInWinBtn
 * @param replaceInSelBtn
 * @param replaceAllBtn
 */
void DialogReplace::setActionButtons(bool replaceBtn, bool replaceFindBtn, bool replaceAndFindBtn, bool replaceInWinBtn, bool replaceInSelBtn, bool replaceAllBtn) {

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
** save a copy in the search history, and return the fields
*/
std::optional<DialogReplace::Fields> DialogReplace::readFields() {

	Fields fields;

	/* Get the search and replace strings, search type, and direction
	   from the dialog */
	const QString replaceText     = ui.textFind->text();
	const QString replaceWithText = ui.textReplace->text();

	if (ui.checkRegex->isChecked()) {
		int regexDefault;
		if (ui.checkCase->isChecked()) {
			fields.searchType = SearchType::Regex;
			regexDefault      = RE_DEFAULT_STANDARD;
		} else {
			fields.searchType = SearchType::RegexNoCase;
			regexDefault      = RE_DEFAULT_CASE_INSENSITIVE;
		}
		/* If the search type is a regular expression, test compile it
		   immediately and present error messages */
		try {
			auto compiledRE = make_regex(replaceText, regexDefault);
		} catch (const RegexError &e) {
			QMessageBox::warning(this, tr("Search String"), tr("Please re-specify the search string:\n%1").arg(QString::fromLatin1(e.what())));
			return {};
		}
	} else {
		if (ui.checkCase->isChecked()) {
			if (ui.checkWord->isChecked()) {
				fields.searchType = SearchType::CaseSenseWord;
			} else {
				fields.searchType = SearchType::CaseSense;
			}
		} else {
			if (ui.checkWord->isChecked()) {
				fields.searchType = SearchType::LiteralWord;
			} else {
				fields.searchType = SearchType::Literal;
			}
		}
	}

	fields.direction = ui.checkBackward->isChecked() ? Direction::Backward : Direction::Forward;

	fields.searchString  = replaceText;
	fields.replaceString = replaceWithText;
	return fields;
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
	Q_ASSERT(document);

	document_ = document;
	if (keepDialog()) {
		setWindowTitle(tr("Replace (in %1)").arg(document_->filename()));
	}
}

/**
 * @brief DialogReplace::setDirection
 * @param direction
 */
void DialogReplace::setDirection(Direction direction) {
	ui.checkBackward->setChecked(direction == Direction::Backward);
}

/**
 * @brief DialogReplace::setKeepDialog
 * @param keep
 */
void DialogReplace::setKeepDialog(bool keep) {
	ui.checkKeep->setChecked(keep);
}

/**
 * @brief DialogReplace::setReplaceText
 * @param text
 */
void DialogReplace::setReplaceText(const QString &text) {
	ui.textReplace->setText(text);
}
