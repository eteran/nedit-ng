
#include "DialogSyntaxPatterns.h"
#include "CommonDialog.h"
#include "DialogDrawingStyles.h"
#include "DialogLanguageModes.h"
#include "DocumentWidget.h"
#include "Help.h"
#include "Highlight.h"
#include "HighlightPattern.h"
#include "HighlightPatternModel.h"
#include "HighlightStyle.h"
#include "LanguageMode.h"
#include "MainWindow.h"
#include "PatternSet.h"
#include "Preferences.h"
#include "SignalBlocker.h"
#include "Util/algorithm.h"
#include "WindowHighlightData.h"

#include <QMessageBox>

/**
 * @brief Constructor for the DialogSyntaxPatterns class.
 *
 * @param parent The parent widget for this dialog, defaults to nullptr.
 * @param f The window flags for the dialog, defaults to Qt::WindowFlags().
 */
DialogSyntaxPatterns::DialogSyntaxPatterns(QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f) {

	ui.setupUi(this);
	connectSlots();

	CommonDialog::SetButtonIcons(&ui);

	model_ = new HighlightPatternModel(this);
	ui.listItems->setModel(model_);

	ui.editContextChars->setValidator(new QIntValidator(0, INT_MAX, this));
	ui.editContextLines->setValidator(new QIntValidator(0, INT_MAX, this));

	connect(ui.radioColoring, &QRadioButton::toggled, this, &DialogSyntaxPatterns::updateLabels);
	connect(ui.radioPass1, &QRadioButton::toggled, this, &DialogSyntaxPatterns::updateLabels);
	connect(ui.radioPass2, &QRadioButton::toggled, this, &DialogSyntaxPatterns::updateLabels);
	connect(ui.radioSubPattern, &QRadioButton::toggled, this, &DialogSyntaxPatterns::updateLabels);
	connect(ui.radioSimpleRegex, &QRadioButton::toggled, this, &DialogSyntaxPatterns::updateLabels);
	connect(ui.radioRangeRegex, &QRadioButton::toggled, this, &DialogSyntaxPatterns::updateLabels);

	// populate the highlight style combo
	if (auto blocker = no_signals(ui.comboHighlightStyle)) {
		for (const HighlightStyle &style : Highlight::HighlightStyles) {
			ui.comboHighlightStyle->addItem(style.name);
		}
	}

	// populate language mode combo
	if (auto blocker = no_signals(ui.comboLanguageMode)) {
		for (const LanguageMode &lang : Preferences::LanguageModes) {
			ui.comboLanguageMode->addItem(lang.name);
		}
	}

	connect(ui.listItems->selectionModel(), &QItemSelectionModel::currentChanged, this, &DialogSyntaxPatterns::currentChanged, Qt::QueuedConnection);
	connect(this, &DialogSyntaxPatterns::restore, ui.listItems, &QListView::setCurrentIndex, Qt::QueuedConnection);
}

/**
 * @brief Connects the slots for the dialog's buttons and other UI elements.
 */
void DialogSyntaxPatterns::connectSlots() {
	connect(ui.buttonLanguageMode, &QPushButton::clicked, this, &DialogSyntaxPatterns::buttonLanguageMode_clicked);
	connect(ui.buttonHighlightStyle, &QPushButton::clicked, this, &DialogSyntaxPatterns::buttonHighlightStyle_clicked);
	connect(ui.buttonNew, &QPushButton::clicked, this, &DialogSyntaxPatterns::buttonNew_clicked);
	connect(ui.buttonDelete, &QPushButton::clicked, this, &DialogSyntaxPatterns::buttonDelete_clicked);
	connect(ui.buttonCopy, &QPushButton::clicked, this, &DialogSyntaxPatterns::buttonCopy_clicked);
	connect(ui.buttonUp, &QPushButton::clicked, this, &DialogSyntaxPatterns::buttonUp_clicked);
	connect(ui.buttonDown, &QPushButton::clicked, this, &DialogSyntaxPatterns::buttonDown_clicked);
	connect(ui.buttonOK, &QPushButton::clicked, this, &DialogSyntaxPatterns::buttonOK_clicked);
	connect(ui.buttonApply, &QPushButton::clicked, this, &DialogSyntaxPatterns::buttonApply_clicked);
	connect(ui.buttonCheck, &QPushButton::clicked, this, &DialogSyntaxPatterns::buttonCheck_clicked);
	connect(ui.buttonDeletePattern, &QPushButton::clicked, this, &DialogSyntaxPatterns::buttonDeletePattern_clicked);
	connect(ui.buttonRestore, &QPushButton::clicked, this, &DialogSyntaxPatterns::buttonRestore_clicked);
	connect(ui.buttonHelp, &QPushButton::clicked, this, &DialogSyntaxPatterns::buttonHelp_clicked);
}

/**
 * @brief Sets the language name for the dialog and updates the UI accordingly.
 *
 * @param name The name of the language mode to set.
 */
void DialogSyntaxPatterns::setLanguageName(const QString &name) {

	static const PatternSet emptyPatternSet;

	// if there is no change, do nothing
	if (previousLanguage_ == name) {
		return;
	}

	// if we are setting the language for the first time skip this part
	// otherwise check if any uncommitted changes are present
	if (!previousLanguage_.isEmpty()) {

		// Look up the original version of the patterns being edited
		const PatternSet *activePatternSet = Highlight::FindPatternSet(previousLanguage_);
		if (!activePatternSet) {
			activePatternSet = &emptyPatternSet;
		}

		/* Get the current information displayed by the dialog.  If it's bad,
		   give the user the chance to throw it out or go back and fix it.  If
		   it has changed, give the user the chance to apply discard or cancel. */
		const std::unique_ptr<PatternSet> currentPatternSet = getDialogPatternSet();

		if (!currentPatternSet) {
			QMessageBox messageBox(this);
			messageBox.setWindowTitle(tr("Incomplete Language Mode"));
			messageBox.setIcon(QMessageBox::Warning);
			messageBox.setText(tr("Discard incomplete entry for current language mode?"));
			QPushButton *buttonKeep    = messageBox.addButton(tr("Keep"), QMessageBox::RejectRole);
			QPushButton *buttonDiscard = messageBox.addButton(QMessageBox::Discard);
			Q_UNUSED(buttonDiscard)

			messageBox.exec();
			if (messageBox.clickedButton() == buttonKeep) {

				// reselect the old item
				if (auto blocker = no_signals(ui.comboLanguageMode)) {
					SetLangModeMenu(previousLanguage_);
				}
				return;
			}
		} else if (*activePatternSet != *currentPatternSet) {

			const int resp = QMessageBox::warning(
				this,
				tr("Language Mode"),
				tr("Apply changes for language mode %1?").arg(previousLanguage_),
				QMessageBox::Apply | QMessageBox::Discard | QMessageBox::Cancel);

			switch (resp) {
			case QMessageBox::Cancel:
				// reselect the old item
				if (auto blocker = no_signals(ui.comboLanguageMode)) {
					SetLangModeMenu(previousLanguage_);
				}
				return;
			case QMessageBox::Apply:
				updatePatternSet();
				break;
			default:
				break;
			}
		}
	}

	model_->clear();

	// Find the associated pattern set (patSet) to edit
	if (const PatternSet *patSet = Highlight::FindPatternSet(name)) {

		// Copy the list of highlight style information to one that the user can freely edit
		for (const HighlightPattern &pattern : patSet->patterns) {
			model_->addItem(pattern);
		}

		// Fill in the dialog information for the selected language mode
		ui.editContextLines->setText(QString::number(patSet->lineContext));
		ui.editContextChars->setText(QString::number(patSet->charContext));
		SetLangModeMenu(name);
		updateLabels();
	} else {
		ui.editContextLines->setText(QString::number(PatternSet::DefaultLineContext));
		ui.editContextChars->setText(QString::number(PatternSet::DefaultCharContext));
	}

	// default to selecting the first item
	if (model_->rowCount() != 0) {
		const QModelIndex index = model_->index(0, 0);
		no_signals(ui.listItems)->setCurrentIndex(index);
	}

	previousLanguage_ = name;
}

/**
 * @brief Sets the language mode menu to the specified name.
 *
 * @param name The name of the language mode to set in the combo box.
 */
void DialogSyntaxPatterns::SetLangModeMenu(const QString &name) {

	const int index = ui.comboLanguageMode->findText(name, Qt::MatchFixedString);
	if (index != -1) {
		ui.comboLanguageMode->setCurrentIndex(index);
	} else {
		ui.comboLanguageMode->setCurrentIndex(0);
	}
}

/**
 * @brief Sets the highlight style menu to the specified name.
 *
 * @param name The name of the highlight style to set in the combo box.
 */
void DialogSyntaxPatterns::setStyleMenu(const QString &name) {

	const int index = ui.comboHighlightStyle->findText(name, Qt::MatchFixedString);
	if (index != -1) {
		ui.comboHighlightStyle->setCurrentIndex(index);
	} else {
		ui.comboHighlightStyle->setCurrentIndex(0);
	}
}

/**
 * @brief Updates the labels in the dialog based on the current selection of radio buttons.
 */
void DialogSyntaxPatterns::updateLabels() {

	QString startLbl;
	QString endLbl;
	bool endSense;
	bool errSense;
	bool matchSense;
	bool parentSense;

	if (ui.radioColoring->isChecked()) {
		startLbl    = tr("Sub-expressions to Highlight in Parent's Starting &Regular Expression (\\1, &, etc.)");
		endLbl      = tr("Sub-expressions to Highlight in Parent Pattern's &Ending Regular Expression");
		endSense    = true;
		errSense    = false;
		matchSense  = false;
		parentSense = true;
	} else {
		endLbl      = tr("&Ending Regular Expression");
		matchSense  = true;
		parentSense = ui.radioSubPattern->isChecked();

		if (ui.radioSimpleRegex->isChecked()) {
			startLbl = tr("&Regular Expression to Match");
			endSense = false;
			errSense = false;
		} else {
			startLbl = tr("Starting &Regular Expression");
			endSense = true;
			errSense = true;
		}
	}

	ui.labelParentPattern->setEnabled(parentSense);
	ui.editParentPattern->setEnabled(parentSense);
	ui.labelRegexEnd->setEnabled(endSense);
	ui.editRegexEnd->setEnabled(endSense);
	ui.labelRegexError->setEnabled(errSense);
	ui.editRegexError->setEnabled(errSense);
	ui.radioSimpleRegex->setEnabled(matchSense);
	ui.radioRangeRegex->setEnabled(matchSense);
	ui.labelRegex->setText(startLbl);
	ui.labelRegexEnd->setText(endLbl);
	ui.groupMatching->setEnabled(matchSense);
}

/**
 * @brief Handles the current index change in the language mode combo box.
 *
 * @param currentText The text of the currently selected language mode in the combo box.
 */
void DialogSyntaxPatterns::on_comboLanguageMode_currentIndexChanged(const QString &text) {
	setLanguageName(text);
}

/**
 * @brief Opens the dialog for selecting language modes.
 */
void DialogSyntaxPatterns::buttonLanguageMode_clicked() {

	if (!dialogLanguageModes_) {
		dialogLanguageModes_ = new DialogLanguageModes(this, this);
	}

	dialogLanguageModes_->show();
}

/**
 * @brief Opens the dialog for selecting highlight styles.
 */
void DialogSyntaxPatterns::buttonHighlightStyle_clicked() {
	const QString style = ui.comboHighlightStyle->currentText();
	if (!style.isEmpty()) {

		if (!dialogDrawingStyles_) {
			dialogDrawingStyles_ = new DialogDrawingStyles(this, Highlight::HighlightStyles, this);
		}

		dialogDrawingStyles_->setStyleByName(style);
		dialogDrawingStyles_->show();
	}
}

/**
 * @brief Updates an item in the model with the current dialog fields.
 *
 * @param item The QModelIndex of the item to update.
 * @return `true` if the item was successfully updated, `false` otherwise.
 */
bool DialogSyntaxPatterns::updateItem(const QModelIndex &index) {
	// Get the current contents of the "patterns" dialog fields
	auto dialogFields = readFields(Verbosity::Verbose);
	if (!dialogFields) {
		return false;
	}

	// Get the current contents of the dialog fields
	if (!index.isValid()) {
		return false;
	}

	model_->updateItem(index, *dialogFields);
	return true;
}

/**
 * @brief Updates the pattern set based on the current dialog fields.
 *
 * @return `true` if the pattern set was successfully updated, `false` otherwise.
 */
bool DialogSyntaxPatterns::updateCurrentItem() {
	const QModelIndex index = ui.listItems->currentIndex();
	if (index.isValid()) {
		return updateItem(index);
	}

	return true;
}

/**
 * @brief Handles the click event for the "New" button in the dialog.
 */
void DialogSyntaxPatterns::buttonNew_clicked() {

	if (!updateCurrentItem()) {
		return;
	}

	CommonDialog::AddNewItem(&ui, model_, []() {
		HighlightPattern style;
		// some sensible defaults...
		style.name = tr("New Item");
		return style;
	});
}

/**
 * @brief Handles the click event for the "Delete" button in the dialog.
 */
void DialogSyntaxPatterns::buttonDelete_clicked() {
	CommonDialog::DeleteItem(&ui, model_, &deleted_);
}

/**
 * @brief Handles the click event for the "Copy" button in the dialog.
 */
void DialogSyntaxPatterns::buttonCopy_clicked() {

	if (!updateCurrentItem()) {
		return;
	}

	CommonDialog::CopyItem(&ui, model_);
}

/**
 * @brief Moves the currently selected item up in the model and updates the UI.
 */
void DialogSyntaxPatterns::buttonUp_clicked() {
	CommonDialog::MoveItemUp(&ui, model_);
}

/**
 * @brief Moves the currently selected item down in the model and updates the UI.
 */
void DialogSyntaxPatterns::buttonDown_clicked() {
	CommonDialog::MoveItemDown(&ui, model_);
}

/**
 * @brief Handles the click event for the "OK" button in the dialog.
 */
void DialogSyntaxPatterns::buttonOK_clicked() {
	// change the patterns
	if (!updatePatternSet()) {
		return;
	}

	accept();
}

/**
 * @brief Handles the click event for the "Apply" button in the dialog.
 */
void DialogSyntaxPatterns::buttonApply_clicked() {
	updatePatternSet();
}

/**
 * @brief Checks the highlight dialog data for errors and displays a message.
 */
void DialogSyntaxPatterns::buttonCheck_clicked() {
	if (checkHighlightDialogData()) {
		QMessageBox::information(this,
								 tr("Pattern compiled"),
								 tr("Patterns compiled without error"));
	}
}

/**
 * @brief Deletes the currently selected syntax highlighting patterns for the specified language mode.
 */
void DialogSyntaxPatterns::buttonDeletePattern_clicked() {

	const QString languageMode = ui.comboLanguageMode->currentText();

	const int resp = QMessageBox::warning(
		this,
		tr("Delete Pattern"),
		tr("Are you sure you want to delete syntax highlighting patterns for language mode %1?").arg(languageMode),
		QMessageBox::Yes | QMessageBox::Cancel);

	if (resp == QMessageBox::Cancel) {
		return;
	}

	// if a stored version of the pattern set exists, delete it from the list
	auto it = std::find_if(Highlight::PatternSets.begin(), Highlight::PatternSets.end(), [&languageMode](const PatternSet &patternSet) {
		return patternSet.languageMode == languageMode;
	});

	if (it != Highlight::PatternSets.end()) {
		Highlight::PatternSets.erase(it);
	}

	model_->clear();

	// Clear out the dialog
	ui.editContextLines->setText(QString::number(PatternSet::DefaultLineContext));
	ui.editContextChars->setText(QString::number(PatternSet::DefaultCharContext));

	ui.editParentPattern->setText(QString());
	ui.editPatternName->setText(QString());
	ui.editRegex->setPlainText(QString());
	ui.editRegexEnd->setText(QString());
	ui.editRegexError->setText(QString());

	// notify the settings system that things have changed
	Preferences::MarkPrefsChanged();
}

/**
 * @brief Restores the default syntax highlighting patterns for the current language mode.
 */
void DialogSyntaxPatterns::buttonRestore_clicked() {

	const QString languageMode = ui.comboLanguageMode->currentText();

	std::optional<PatternSet> patternSet = Highlight::ReadDefaultPatternSet(languageMode);
	if (!patternSet) {
		QMessageBox::warning(
			this,
			tr("No Default Pattern"),
			tr("There is no default pattern set for language mode %1").arg(languageMode));
		return;
	}

	const int resp = QMessageBox::warning(
		this,
		tr("Discard Changes"),
		tr("Are you sure you want to discard all changes to syntax highlighting patterns for language mode %1?").arg(languageMode),
		QMessageBox::Discard | QMessageBox::Cancel);

	if (resp == QMessageBox::Cancel) {
		return;
	}

	// if a stored version of the pattern set exists, replace it, if it doesn't, add a new one
	Upsert(Highlight::PatternSets, *patternSet, [languageMode](const PatternSet &pattern) {
		return pattern.languageMode == languageMode;
	});

	model_->clear();

	// Update the dialog
	for (const HighlightPattern &pattern : patternSet->patterns) {
		model_->addItem(pattern);
	}

	// Fill in the dialog information for the selected language mode
	ui.editContextLines->setText(QString::number(patternSet->lineContext));
	ui.editContextChars->setText(QString::number(patternSet->charContext));

	// default to selecting the first item
	if (model_->rowCount() != 0) {
		const QModelIndex index = model_->index(0, 0);
		ui.listItems->setCurrentIndex(index);
	}
}

/**
 * @brief Displays the help topic for syntax patterns.
 */
void DialogSyntaxPatterns::buttonHelp_clicked() {
	Help::displayTopic(Help::Topic::Syntax);
}

/**
 * @brief Handles the change of the current item in the model.
 *
 * @param current The QModelIndex of the currently selected item.
 * @param previous The QModelIndex of the previously selected item.
 */
void DialogSyntaxPatterns::currentChanged(const QModelIndex &current, const QModelIndex &previous) {
	static bool canceled = false;
	bool skip_check      = false;

	if (canceled) {
		canceled = false;
		return;
	}

	// if we are actually switching items, check that the previous one was valid
	// so we can optionally cancel
	if (previous.isValid() && previous != deleted_ && !validateFields(Verbosity::Silent)) {
		QMessageBox messageBox(this);
		messageBox.setWindowTitle(tr("Discard Entry"));
		messageBox.setIcon(QMessageBox::Warning);
		messageBox.setText(tr("Discard incomplete entry for current menu item?"));
		QPushButton *buttonKeep    = messageBox.addButton(tr("Keep"), QMessageBox::RejectRole);
		QPushButton *buttonDiscard = messageBox.addButton(QMessageBox::Discard);

		messageBox.exec();
		if (messageBox.clickedButton() == buttonKeep) {

			// again to cause message box to pop up
			validateFields(Verbosity::Verbose);

			// reselect the old item
			canceled = true;
			Q_EMIT restore(previous);
			return;
		}

		if (messageBox.clickedButton() == buttonDiscard) {
			model_->deleteItem(previous);
			skip_check = true;
		}
	}

	// this is only safe if we aren't moving due to a delete operation
	if (previous.isValid() && previous != deleted_ && !skip_check) {
		if (!updateItem(previous)) {
			// reselect the old item
			canceled = true;
			Q_EMIT restore(previous);
			return;
		}
	}

	// previous was OK, so let's update the contents of the dialog
	if (const auto pattern = model_->itemFromIndex(current)) {

		const bool isSubpat    = !pattern->subPatternOf.isNull();
		const bool isDeferred  = pattern->flags & DEFER_PARSING;
		const bool isColorOnly = pattern->flags & COLOR_ONLY;
		const bool isRange     = (!pattern->endRE.isNull());

		ui.editPatternName->setText(pattern->name);
		ui.editParentPattern->setText(!pattern->subPatternOf.isNull() ? pattern->subPatternOf : QString());
		ui.editRegex->setPlainText(pattern->startRE);
		ui.editRegexEnd->setText(pattern->endRE);
		ui.editRegexError->setText(pattern->errorRE);

		if (!isSubpat && !isDeferred) {
			ui.radioPass1->setChecked(true);
		} else if (!isSubpat && isDeferred) {
			ui.radioPass2->setChecked(true);
		} else if (isSubpat && !isColorOnly) {
			ui.radioSubPattern->setChecked(true);
		} else if (isSubpat && isColorOnly) {
			ui.radioColoring->setChecked(true);
		}

		if (isRange) {
			ui.radioRangeRegex->setChecked(true);
		} else {
			ui.radioSimpleRegex->setChecked(true);
		}

		setStyleMenu(pattern->style);
	} else {
		// if nothing is selected, reset to defaults
		ui.editPatternName->setText(QString());
		ui.editParentPattern->setText(QString());
		ui.editRegex->setPlainText(QString());
		ui.editRegexEnd->setText(QString());
		ui.editRegexError->setText(QString());
		ui.radioPass1->setChecked(true);
		ui.radioSimpleRegex->setChecked(true);
		setStyleMenu(QStringLiteral("Plain"));
	}

	CommonDialog::UpdateButtonStates(&ui, model_, current);
	updateLabels();
}

/**
 * @brief Updates the language mode menu in the dialog.
 */
void DialogSyntaxPatterns::UpdateLanguageModeMenu() {

	if (auto blocker = no_signals(ui.comboLanguageMode)) {

		const QString language = ui.comboLanguageMode->currentText();
		ui.comboLanguageMode->clear();

		for (const LanguageMode &lang : Preferences::LanguageModes) {
			ui.comboLanguageMode->addItem(lang.name);
		}

		SetLangModeMenu(language);
	}
}

/**
 * @brief Updates the highlight style menu in the dialog.
 */
void DialogSyntaxPatterns::updateHighlightStyleMenu() {

	if (auto blocker = no_signals(ui.comboHighlightStyle)) {

		const QString pattern = ui.comboHighlightStyle->currentText();
		ui.comboHighlightStyle->clear();

		for (const HighlightStyle &style : Highlight::HighlightStyles) {
			ui.comboHighlightStyle->addItem(style.name);
		}

		setStyleMenu(pattern);
	}
}

/**
 * @brief Updates the pattern set based on the current dialog fields and applies it to all documents.
 *
 * @return `true` if the pattern set was successfully updated, `false` otherwise.
 */
bool DialogSyntaxPatterns::updatePatternSet() {

	// Make sure the patterns are valid and compile
	if (!checkHighlightDialogData()) {
		return false;
	}

	// Get the current data
	std::unique_ptr<PatternSet> patternSet = getDialogPatternSet();
	if (!patternSet) {
		return false;
	}

	// Find the pattern being modified
	auto it = std::find_if(Highlight::PatternSets.begin(), Highlight::PatternSets.end(), [this](const PatternSet &pattern) {
		return pattern.languageMode == ui.comboLanguageMode->currentText();
	});

	// If it's a new pattern, add it at the end, otherwise free the existing pattern set and replace it
	size_t oldNum;
	if (it == Highlight::PatternSets.end()) {
		Highlight::PatternSets.push_back(*patternSet);
		oldNum = 0;
	} else {
		oldNum = it->patterns.size();
		*it    = *patternSet;
	}

	// Find windows that are currently using this pattern set and re-do the highlighting
	for (DocumentWidget *document : DocumentWidget::allDocuments()) {
		if (!patternSet->patterns.empty()) {
			if (document->getLanguageMode() != PLAIN_LANGUAGE_MODE && (Preferences::LanguageModeName(document->getLanguageMode()) == patternSet->languageMode)) {
				/*  The user worked on the current document's language mode, so
					we have to make some changes immediately. For inactive
					modes, the changes will be activated on activation.  */
				if (oldNum == 0) {
					/*  Highlighting (including menu entry) was deactivated in
						this function or in preferences.c::reapplyLanguageMode()
						if the old set had no patterns, so reactivate menu entry. */
					if (document->isTopDocument()) {
						MainWindow::fromDocument(document)->ui.action_Highlight_Syntax->setEnabled(true);
					}

					//  Reactivate highlighting if it's default
					document->highlightSyntax_ = Preferences::GetPrefHighlightSyntax();
				}

				if (document->highlightSyntax_) {
					document->stopHighlighting();

					if (document->isTopDocument()) {
						MainWindow::fromDocument(document)->ui.action_Highlight_Syntax->setEnabled(true);
						no_signals(MainWindow::fromDocument(document)->ui.action_Highlight_Syntax)->setChecked(true);
					}
					document->startHighlighting(Verbosity::Verbose);
				}
			}
		} else {
			/*  No pattern in pattern set. This will probably not happen much,
				but you never know.  */
			document->stopHighlighting();
			document->highlightSyntax_ = false;

			if (document->isTopDocument()) {
				MainWindow::fromDocument(document)->ui.action_Highlight_Syntax->setEnabled(false);
				no_signals(MainWindow::fromDocument(document)->ui.action_Highlight_Syntax)->setChecked(false);
			}
		}
	}

	// Note that preferences have been changed
	Preferences::MarkPrefsChanged();
	return true;
}

/**
 * @brief Checks the highlight dialog data for validity and compiles the patterns.
 *
 * @return `true` if the patterns are valid and compiled successfully, `false` otherwise.
 */
bool DialogSyntaxPatterns::checkHighlightDialogData() {
	// Get the pattern information from the dialog
	std::unique_ptr<PatternSet> patternSet = getDialogPatternSet();
	if (!patternSet) {
		return false;
	}

	// Compile the patterns
	return patternSet->patterns.empty() ? true : TestHighlightPatterns(patternSet);
}

/**
 * @brief Get the current information that the user has entered in the syntax
 * highlighting dialog.
 *
 * @return A PatternSet object containing the dialog's current state, or
 * nullptr if there was an error.
 */
std::unique_ptr<PatternSet> DialogSyntaxPatterns::getDialogPatternSet() {

	// Get the line and character context values
	if (ui.editContextLines->text().isEmpty()) {
		QMessageBox::critical(this, tr("Warning"), tr("Please supply a value for context lines"));
		return nullptr;
	}

	bool ok;
	const int lineContext = ui.editContextLines->text().toInt(&ok);
	if (!ok) {
		QMessageBox::critical(this, tr("Warning"), tr("Can't read integer value \"%1\" in context lines").arg(ui.editContextLines->text()));
		return nullptr;
	}

	// Get the line and character context values
	if (ui.editContextChars->text().isEmpty()) {
		QMessageBox::critical(this, tr("Warning"), tr("Please supply a value for context chars"));
		return nullptr;
	}

	const int charContext = ui.editContextChars->text().toInt(&ok);
	if (!ok) {
		QMessageBox::critical(this, tr("Warning"), tr("Can't read integer value \"%1\" in context chars").arg(ui.editContextChars->text()));
		return nullptr;
	}

	// ensure that the list has the current item complete filled out
	if (!updateCurrentItem()) {
		return nullptr;
	}

	/* Allocate a new pattern set structure and copy the fields read from the
	   dialog, including the modified pattern list into it */
	auto patternSet          = std::make_unique<PatternSet>();
	patternSet->languageMode = ui.comboLanguageMode->currentText();
	patternSet->lineContext  = lineContext;
	patternSet->charContext  = charContext;

	for (int i = 0; i < model_->rowCount(); ++i) {
		const QModelIndex index = model_->index(i, 0);
		if (auto pattern = model_->itemFromIndex(index)) {
			patternSet->patterns.push_back(*pattern);
		}
	}

	return patternSet;
}

/**
 * @brief Reads the fields from the dialog and returns a HighlightPattern object.
 *
 * @param verbosity The verbosity level for error messages.
 * @return A HighlightPattern object if the fields are valid, or an empty optional if there are errors.
 */
std::optional<HighlightPattern> DialogSyntaxPatterns::readFields(Verbosity verbosity) {

	HighlightPattern pat;

	// read the type buttons
	const int colorOnly = ui.radioColoring->isChecked();
	if (ui.radioPass2->isChecked()) {
		pat.flags |= DEFER_PARSING;
	} else if (colorOnly) {
		pat.flags = COLOR_ONLY;
	}

	// read the name field
	QString name = ui.editPatternName->text().simplified();
	if (name.isNull()) {
		return {};
	}

	pat.name = std::move(name);
	if (pat.name.isEmpty()) {
		if (verbosity == Verbosity::Verbose) {
			QMessageBox::warning(this, tr("Pattern Name"), tr("Please specify a name for the pattern"));
		}
		return {};
	}

	// read the startRE field
	pat.startRE = ui.editRegex->toPlainText();
	if (pat.startRE.isEmpty()) {
		if (verbosity == Verbosity::Verbose) {
			QMessageBox::warning(this, tr("Matching Regex"), tr("Please specify a regular expression to match"));
		}
		return {};
	}

	/* Make sure coloring patterns contain only sub-expression references
	   and put it in replacement regular-expression form */
	if (colorOnly) {

		QString outStr;
		outStr.reserve(pat.startRE.size());

		auto outPtr = std::back_inserter(outStr);

		Q_FOREACH (const QChar ch, pat.startRE) {
			if (ch != QLatin1Char(' ') && ch != QLatin1Char('\t')) {
				*outPtr++ = ch;
			}
		}

		pat.startRE = outStr;

		static const QRegularExpression re(QStringLiteral(R"([^&\\123456789 \t])"));
		if (pat.startRE.contains(re) || (pat.startRE[0] != QLatin1Char('\\') && pat.startRE[0] != QLatin1Char('&')) || pat.startRE.contains(QStringLiteral("\\\\"))) {

			if (verbosity == Verbosity::Verbose) {
				QMessageBox::warning(
					this,
					tr("Pattern Error"),
					tr("The expression field in patterns which specify highlighting for a parent, must contain only sub-expression references in regular expression replacement form (&\\1\\2 etc.).  See Help -> Regular Expressions and Help -> Syntax Highlighting for more information"));
			}
			return {};
		}
	}

	// read the parent field
	if (ui.radioSubPattern->isChecked() || colorOnly) {
		const QString parent = ui.editParentPattern->text().simplified();
		if (parent.isEmpty()) {
			if (verbosity == Verbosity::Verbose) {
				QMessageBox::warning(this, tr("Specify Parent Pattern"), tr("Please specify a parent pattern"));
			}
			return {};
		}

		if (!parent.isNull()) {
			pat.subPatternOf = parent;
		}
	}

	// read the styles option menu
	pat.style = ui.comboHighlightStyle->currentText();

	// read the endRE field
	if (colorOnly || ui.radioRangeRegex->isChecked()) {
		pat.endRE = ui.editRegexEnd->text();
		if (!colorOnly && pat.endRE.isEmpty()) {
			if (verbosity == Verbosity::Verbose) {
				QMessageBox::warning(this, tr("Specify Regex"), tr("Please specify an ending regular expression"));
			}
			return {};
		}
	}

	// read the errorRE field
	if (ui.radioRangeRegex->isChecked()) {
		pat.errorRE = ui.editRegexError->text();
		if (pat.errorRE.isEmpty()) {
			pat.errorRE = QString();
		}
	}

	return pat;
}

/**
 * @brief Validates the fields in the dialog and returns `true` if they are valid.
 *
 * @param mode The verbosity level for error messages.
 * @return `true` if the fields are valid, `false` otherwise.
 */
bool DialogSyntaxPatterns::validateFields(Verbosity verbosity) {
	if (readFields(verbosity)) {
		return true;
	}

	return false;
}

/**
 * @brief Do a test compile of patterns in `patSet` and report problems to the user via dialog.
 *
 * @param patSet The pattern set to test for validity.
 * @return `true` if the patterns compiled successfully, `false` otherwise.
 */
bool DialogSyntaxPatterns::TestHighlightPatterns(const std::unique_ptr<PatternSet> &patSet) {

	try {
		/* Compile the patterns (passing a random window as a source for fonts, and
		   parent for dialogs, since we really don't care what fonts are used) */
		if (PatternSet *const patternSet = patSet.get()) {
			for (DocumentWidget *document : DocumentWidget::allDocuments()) {
				if (document->createHighlightData(patternSet, Verbosity::Verbose)) {
					return true;
				}
			}
		}
	} catch (const RegexError &e) {
		QMessageBox::critical(this, tr("Error"), tr("Error compiling syntax highlight patterns:\n%1").arg(QString::fromLatin1(e.what())));
	}

	return false;
}

/**
 * @brief Renames a highlight pattern in the dialog.
 *
 * @param oldName The old name of the highlight pattern to rename.
 * @param newName The new name to set for the highlight pattern.
 */
void DialogSyntaxPatterns::RenameHighlightPattern(const QString &oldName, const QString &newName) {
	if (ui.comboLanguageMode->currentText() == oldName) {
		SetLangModeMenu(newName);
	}
}

/**
 * @brief Checks if the current language mode has highlight patterns defined.
 *
 * @param languageMode The language mode to check for highlight patterns.
 * @return `true` if the language mode has highlight patterns, `false` otherwise.
 */
bool DialogSyntaxPatterns::LMHasHighlightPatterns(const QString &languageMode) {
	return languageMode == ui.comboLanguageMode->currentText() && model_->rowCount() != 0;
}
