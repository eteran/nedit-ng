
#include <QtDebug>
#include <QMessageBox>
#include "DialogSyntaxPatterns.h"
#include "DialogLanguageModes.h"
#include "PatternSet.h"
#include "Document.h"
#include "LanguageMode.h"
#include "HighlightStyle.h"
#include "preferences.h"
#include "highlightData.h"

//------------------------------------------------------------------------------
// Name: DialogSyntaxPatterns
//------------------------------------------------------------------------------
DialogSyntaxPatterns::DialogSyntaxPatterns(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f), previous_(nullptr) {
	ui.setupUi(this);
	
	connect(ui.radioColoring,    SIGNAL(toggled(bool)), this, SLOT(updateLabels()));
	connect(ui.radioPass1,       SIGNAL(toggled(bool)), this, SLOT(updateLabels()));
	connect(ui.radioPass2,       SIGNAL(toggled(bool)), this, SLOT(updateLabels()));
	connect(ui.radioSubPattern,  SIGNAL(toggled(bool)), this, SLOT(updateLabels()));
	connect(ui.radioSimpleRegex, SIGNAL(toggled(bool)), this, SLOT(updateLabels()));
	connect(ui.radioRangeRegex,  SIGNAL(toggled(bool)), this, SLOT(updateLabels()));	
	
	// populate language mode combo
	for (int i = 0; i < NLanguageModes; i++) {	
		ui.comboLanguageMode->addItem(LanguageModes[i]->name);
	}
	
	// populate the highlight style combo
	for(HighlightStyle *style : HighlightStyles) {
		ui.comboHighlightStyle->addItem(style->name);
	}
}

//------------------------------------------------------------------------------
// Name: ~DialogSyntaxPatterns
//------------------------------------------------------------------------------
DialogSyntaxPatterns::~DialogSyntaxPatterns() {
	for(int i = 0; i < ui.listItems->count(); ++i) {
	    delete itemFromIndex(i);
	}
}

//------------------------------------------------------------------------------
// Name: itemFromIndex
//------------------------------------------------------------------------------
HighlightPattern *DialogSyntaxPatterns::itemFromIndex(int i) const {
	if(i < ui.listItems->count()) {
	    QListWidgetItem* item = ui.listItems->item(i);
		auto ptr = reinterpret_cast<HighlightPattern *>(item->data(Qt::UserRole).toULongLong());
		return ptr;
	}
	
	return nullptr;
}

//------------------------------------------------------------------------------
// Name: setLanguageName
//------------------------------------------------------------------------------
void DialogSyntaxPatterns::setLanguageName(const QString &name) {

	for(int i = 0; i < ui.listItems->count(); ++i) {
	    delete itemFromIndex(i);
	}
	ui.listItems->clear();
	
	// Find the associated pattern set (patSet) to edit 
	if(PatternSet *patSet = FindPatternSet(name)) {

		// Copy the list of highlight style information to one that the user can freely edit
		int nPatterns = patSet->nPatterns;
		for (int i = 0; i < nPatterns; i++) {
			auto ptr  = new HighlightPattern(patSet->patterns[i]);
			auto item = new QListWidgetItem(QString::fromStdString(ptr->name));
			item->setData(Qt::UserRole, reinterpret_cast<qulonglong>(ptr));
			ui.listItems->addItem(item);
		}
		
		// Fill in the dialog information for the selected language mode 
		ui.editContextLines->setText(tr("%1").arg(patSet->lineContext));
		ui.editContextChars->setText(tr("%1").arg(patSet->charContext));
		SetLangModeMenu(name);
		updateLabels();
	} else {
		ui.editContextLines->setText(tr("%1").arg(1));
		ui.editContextChars->setText(tr("%1").arg(1));    
	}
	
	if(ui.listItems->count() != 0) {
		ui.listItems->setCurrentRow(0);
	}
}

//------------------------------------------------------------------------------
// Name: SetLangModeMenu
//------------------------------------------------------------------------------
void DialogSyntaxPatterns::SetLangModeMenu(const QString &name) {
	QList<QListWidgetItem *> matches = ui.listItems->findItems(name, Qt::MatchFixedString);
	if(!matches.isEmpty()) {
		ui.listItems->setCurrentItem(matches[0]);
	}
}

//------------------------------------------------------------------------------
// Name: updateLabels
//------------------------------------------------------------------------------
void DialogSyntaxPatterns::updateLabels() {

	QString startLbl;
	QString endLbl;
	bool endSense;
	bool errSense;
	bool matchSense;
	bool parentSense;

	if (ui.radioColoring->isChecked()) {
		startLbl    = tr("Sub-expressions to Highlight in Parent's Starting Regular Expression (\\1, &, etc.)");
		endLbl      = tr("Sub-expressions to Highlight in Parent Pattern's Ending Regular Expression");
		endSense    = true;
		errSense    = false;
		matchSense  = false;
		parentSense = true;
	} else {
		endLbl = tr("Ending Regular Expression");
		matchSense = true;
		parentSense = ui.radioSubPattern->isChecked();
		
		if (ui.radioSimpleRegex->isChecked()) {
			startLbl = tr("Regular Expression to Match");
			endSense = false;
			errSense = false;
		} else {
			startLbl = tr("Starting Regular Expression");
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

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSyntaxPatterns::on_comboLanguageMode_currentIndexChanged(const QString &currentText) {
	setLanguageName(currentText);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSyntaxPatterns::on_buttonLanguageMode_clicked() {
	auto dialog = new DialogLanguageModes(this);
	dialog->exec();
	delete dialog;
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSyntaxPatterns::on_buttonHighlightStyle_clicked() {
	QString style = ui.comboHighlightStyle->currentText();
	if(!style.isEmpty()) {
		EditHighlightStyles(style.toLatin1().data());
	}
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
bool DialogSyntaxPatterns::updateCurrentItem(QListWidgetItem *item) {
	// Get the current contents of the "patterns" dialog fields 
	auto ptr = readDialogFields(false);
	if(!ptr) {
		return false;
	}
	
	// delete the current pattern in this slot
	auto old = reinterpret_cast<HighlightPattern *>(item->data(Qt::UserRole).toULongLong());
	delete old;
	
	item->setData(Qt::UserRole, reinterpret_cast<qulonglong>(ptr));
	item->setText(QString::fromStdString(ptr->name));
	return true;
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
bool DialogSyntaxPatterns::updateCurrentItem() {
	QList<QListWidgetItem *> selections = ui.listItems->selectedItems();
	if(selections.size() != 1) {
		return false;
	}

	QListWidgetItem *const selection = selections[0];
	return updateCurrentItem(selection);	
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSyntaxPatterns::on_buttonNew_clicked() {

	if(!updateCurrentItem()) {
		return;
	}

	auto ptr  = new HighlightPattern;
	ptr->name = tr("New Item").toStdString();

	auto item = new QListWidgetItem(QString::fromStdString(ptr->name));
	item->setData(Qt::UserRole, reinterpret_cast<qulonglong>(ptr));
	ui.listItems->addItem(item);
	ui.listItems->setCurrentItem(item);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSyntaxPatterns::on_buttonDelete_clicked() {
	QList<QListWidgetItem *> selections = ui.listItems->selectedItems();
	if(selections.size() != 1) {
		return;
	}

	QListWidgetItem *const selection = selections[0];
	auto ptr = reinterpret_cast<HighlightPattern *>(selection->data(Qt::UserRole).toULongLong());

	delete ptr;
	delete selection;
	
	// force an update of the display
	Q_EMIT on_listItems_itemSelectionChanged();
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSyntaxPatterns::on_buttonCopy_clicked() {

	if(!updateCurrentItem()) {
		return;
	}

	QList<QListWidgetItem *> selections = ui.listItems->selectedItems();
	if(selections.size() != 1) {
		return;
	}

	QListWidgetItem *const selection = selections[0];
	auto ptr = reinterpret_cast<HighlightPattern *>(selection->data(Qt::UserRole).toULongLong());
	auto newPtr = new HighlightPattern(*ptr);
	auto newItem = new QListWidgetItem(QString::fromStdString(newPtr->name));
	newItem->setData(Qt::UserRole, reinterpret_cast<qulonglong>(newPtr));

	const int i = ui.listItems->row(selection);
	ui.listItems->insertItem(i + 1, newItem);
	ui.listItems->setCurrentItem(newItem);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSyntaxPatterns::on_buttonUp_clicked() {
	QList<QListWidgetItem *> selections = ui.listItems->selectedItems();
	if(selections.size() != 1) {
		return;
	}

	QListWidgetItem *const selection = selections[0];
	const int i = ui.listItems->row(selection);

	if(i != 0) {
		QListWidgetItem *item = ui.listItems->takeItem(i);
		ui.listItems->insertItem(i - 1, item);
		ui.listItems->setCurrentItem(item);
	}
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSyntaxPatterns::on_buttonDown_clicked() {
	QList<QListWidgetItem *> selections = ui.listItems->selectedItems();
	if(selections.size() != 1) {
		return;
	}

	QListWidgetItem *const selection = selections[0];
	const int i = ui.listItems->row(selection);

	if(i != ui.listItems->count() - 1) {
		QListWidgetItem *item = ui.listItems->takeItem(i);
		ui.listItems->insertItem(i + 1, item);
		ui.listItems->setCurrentItem(item);
	}
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSyntaxPatterns::on_buttonOK_clicked() {
	// change the patterns 
	if (!updatePatternSet()) {
		return;
	}
		
	accept();
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSyntaxPatterns::on_buttonApply_clicked() {
	updatePatternSet();
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSyntaxPatterns::on_buttonCheck_clicked() {
	if (checkHighlightDialogData()) {
		QMessageBox::information(this, tr("Pattern compiled"), tr("Patterns compiled without error"));
	}
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSyntaxPatterns::on_buttonDeletePattern_clicked() {

	const QString languageMode = ui.comboLanguageMode->currentText();

	QMessageBox messageBox(this);
	messageBox.setWindowTitle(tr("Delete Pattern"));
	messageBox.setIcon(QMessageBox::Warning);
	messageBox.setText(tr("Are you sure you want to delete syntax highlighting patterns for language mode %1?").arg(languageMode));
	QPushButton *buttonYes     = messageBox.addButton(QLatin1String("Yes, Delete"), QMessageBox::AcceptRole);
	QPushButton *buttonCancel  = messageBox.addButton(QMessageBox::Cancel);
	Q_UNUSED(buttonYes);

	messageBox.exec();
	if (messageBox.clickedButton() == buttonCancel) {
		return;
	}


	// if a stored version of the pattern set exists, delete it from the list 
	int psn;
	for (psn = 0; psn < NPatternSets; psn++) {
		if (languageMode == PatternSets[psn]->languageMode) {
			break;
		}
	}

	if (psn < NPatternSets) {
		delete PatternSets[psn];
		std::copy_n(&PatternSets[psn + 1], (NPatternSets - 1 - psn), &PatternSets[psn]);
		NPatternSets--;
	}

	// Free the old dialog information 
	for(int i = 0; i < ui.listItems->count(); ++i) {
	    delete itemFromIndex(i);
	}
	ui.listItems->clear();	

	// Clear out the dialog 
	ui.editContextLines->setText(tr("%1").arg(1));
	ui.editContextChars->setText(tr("%1").arg(0));
	
	Q_EMIT on_listItems_itemSelectionChanged();
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSyntaxPatterns::on_buttonRestore_clicked() {

	const QString languageMode = ui.comboLanguageMode->currentText();
	
	PatternSet *defaultPatSet = readDefaultPatternSet(languageMode.toLatin1().data());
	if(!defaultPatSet) {
		QMessageBox::warning(this, tr("No Default Pattern"), tr("There is no default pattern set for language mode %1").arg(languageMode));
		return;
	}
	
	int resp = QMessageBox::warning(this, tr("Discard Changes"), tr("Are you sure you want to discard all changes to syntax highlighting patterns for language mode %1?").arg(languageMode), QMessageBox::Discard | QMessageBox::Cancel);
	if (resp == QMessageBox::Cancel) {
		return;
	}

	// if a stored version of the pattern set exists, replace it, if it doesn't, add a new one
	int psn;
	for (psn = 0; psn < NPatternSets; psn++) {
		if (languageMode == PatternSets[psn]->languageMode) {
			break;
		}
	}
			
	if (psn < NPatternSets) {
		delete PatternSets[psn];
		PatternSets[psn] = defaultPatSet;
	} else {
		PatternSets[NPatternSets++] = defaultPatSet;
	}

	// Free the old dialog information 
	for(int i = 0; i < ui.listItems->count(); ++i) {
	    delete itemFromIndex(i);
	}
	ui.listItems->clear();

	// Update the dialog 
	int nPatterns = defaultPatSet->nPatterns;
	for (int i = 0; i < nPatterns; i++) {
		auto ptr  = new HighlightPattern(defaultPatSet->patterns[i]);
		auto item = new QListWidgetItem(QString::fromStdString(ptr->name));
		item->setData(Qt::UserRole, reinterpret_cast<qulonglong>(ptr));
		ui.listItems->addItem(item);
	}

	// Fill in the dialog information for the selected language mode 
	ui.editContextLines->setText(tr("%1").arg(defaultPatSet->lineContext));
	ui.editContextChars->setText(tr("%1").arg(defaultPatSet->charContext));
	
	if(ui.listItems->count() != 0) {
		ui.listItems->setCurrentRow(0);
	}
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSyntaxPatterns::on_buttonHelp_clicked() {
#if 0
	Help(HELP_PATTERNS);
#endif
}

//------------------------------------------------------------------------------
// Name: on_listItems_itemSelectionChanged
//------------------------------------------------------------------------------
void DialogSyntaxPatterns::on_listItems_itemSelectionChanged() {

	QList<QListWidgetItem *> selections = ui.listItems->selectedItems();
	if(selections.size() != 1) {
		previous_ = nullptr;
		
		// if nothing is selected, reset to defaults
		ui.editPatternName->setText(QString());
		ui.editParentPattern->setText(QString());
		ui.editRegex->setPlainText(QString());
		ui.editRegexEnd->setText(QString());
		ui.editRegexError->setText(QString());
		ui.radioPass1->setChecked(true);
		ui.radioSimpleRegex->setChecked(true);
		setStyleMenu(tr("Plain"));
			
		ui.buttonUp    ->setEnabled(false);
		ui.buttonDown  ->setEnabled(false);
		ui.buttonDelete->setEnabled(false);
		ui.buttonCopy  ->setEnabled(false);		
		
		return;
	}

	QListWidgetItem *const current = selections[0];

	if(previous_ != nullptr && current != nullptr && current != previous_) {

		// we want to try to save it (but not apply it yet)
		// and then move on
		if(!checkCurrentPattern(true)) {

			QMessageBox messageBox(this);
			messageBox.setWindowTitle(tr("Discard Entry"));
			messageBox.setIcon(QMessageBox::Warning);
			messageBox.setText(tr("Discard incomplete entry for current menu item?"));
			QPushButton *buttonKeep    = messageBox.addButton(tr("Keep"), QMessageBox::RejectRole);
			QPushButton *buttonDiscard = messageBox.addButton(tr("Discard"), QMessageBox::AcceptRole);
			Q_UNUSED(buttonDiscard);

			messageBox.exec();
			if (messageBox.clickedButton() == buttonKeep) {
			
				// again to cause messagebox to pop up
				checkCurrentPattern(false);
				
				// reselect the old item
				ui.listItems->blockSignals(true);
				ui.listItems->setCurrentItem(previous_);
				ui.listItems->blockSignals(false);
				return;
			}

			// if we get here, we are ditching changes
		} else {
			updateCurrentItem(previous_);
		}
	}

	if(current) {
		const int i = ui.listItems->row(current);

		auto pat = reinterpret_cast<HighlightPattern *>(current->data(Qt::UserRole).toULongLong());

		bool isSubpat;
		bool isDeferred;
		bool isColorOnly;
		bool isRange;


		if(!pat) {		
			ui.editPatternName->setText(QString());
			ui.editParentPattern->setText(QString());
			ui.editRegex->setPlainText(QString());
			ui.editRegexEnd->setText(QString());
			ui.editRegexError->setText(QString());
			ui.radioPass1->setChecked(true);
			ui.radioSimpleRegex->setChecked(true);
			setStyleMenu(tr("Plain"));
		} else {
			isSubpat    = !pat->subPatternOf.isNull();
			isDeferred  = pat->flags & DEFER_PARSING;
			isColorOnly = pat->flags & COLOR_ONLY;
			isRange = (!pat->endRE.isNull());
			
			ui.editPatternName->setText(QString::fromStdString(pat->name));
			ui.editParentPattern->setText(!pat->subPatternOf.isNull() ? pat->subPatternOf : QString());
			ui.editRegex->setPlainText(pat->startRE);
			ui.editRegexEnd->setText(pat->endRE);
			ui.editRegexError->setText(pat->errorRE);
			
			if(!isSubpat && !isDeferred) {
				ui.radioPass1->setChecked(true);
			} else if(!isSubpat && isDeferred) {
				ui.radioPass2->setChecked(true);
			} else if(isSubpat && !isColorOnly) {
				ui.radioSubPattern->setChecked(true);
			} else if(isSubpat && isColorOnly) {
				ui.radioColoring->setChecked(true);
			}
			
			if(isRange) {
				ui.radioRangeRegex->setChecked(true);
			} else {
				ui.radioSimpleRegex->setChecked(true);
			}

			setStyleMenu(pat->style);
		}
		
		updateLabels();

		if(i == 0) {
			ui.buttonUp    ->setEnabled(false);
			ui.buttonDown  ->setEnabled(ui.listItems->count() > 1);
			ui.buttonDelete->setEnabled(true);
			ui.buttonCopy  ->setEnabled(true);
		} else if(i == (ui.listItems->count() - 1)) {
			ui.buttonUp    ->setEnabled(true);
			ui.buttonDown  ->setEnabled(false);
			ui.buttonDelete->setEnabled(true);
			ui.buttonCopy  ->setEnabled(true);
		} else {
			ui.buttonUp    ->setEnabled(true);
			ui.buttonDown  ->setEnabled(true);
			ui.buttonDelete->setEnabled(true);
			ui.buttonCopy  ->setEnabled(true);
		}
	} else {
		ui.editPatternName->setText(QString());
		ui.editParentPattern->setText(QString());
		ui.editRegex->setPlainText(QString());
		ui.editRegexEnd->setText(QString());
		ui.editRegexError->setText(QString());
		ui.radioPass1->setChecked(true);
		ui.radioSimpleRegex->setChecked(true);
		setStyleMenu(tr("Plain"));
			
		ui.buttonUp    ->setEnabled(false);
		ui.buttonDown  ->setEnabled(false);
		ui.buttonDelete->setEnabled(false);
		ui.buttonCopy  ->setEnabled(false);
	}
	
	previous_ = current;
}

/*
** If a syntax highlighting dialog is up, ask to have the option menu for
** chosing language mode updated (via a call to CreateLanguageModeMenu)
*/
void DialogSyntaxPatterns::UpdateLanguageModeMenu() {
	// TODO(eteran): implement this
}


/*
** If a syntax highlighting dialog is up, ask to have the option menu for
** chosing highlight styles updated (via a call to createHighlightStylesMenu)
*/
void DialogSyntaxPatterns::updateHighlightStyleMenu() {
	// TODO(eteran): implement this
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
bool DialogSyntaxPatterns::updatePatternSet() {
	// TODO(eteran): implement this
	return true;
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
bool DialogSyntaxPatterns::checkHighlightDialogData() {
	// Get the pattern information from the dialog 
	PatternSet *patSet = getDialogPatternSet();
	if(!patSet) {
		return false;
	}

	// Compile the patterns  
	bool result = (patSet->nPatterns == 0) ? true : TestHighlightPatterns(patSet);
	delete patSet;
	return result;
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogSyntaxPatterns::setStyleMenu(const QString &name) {

	int index = ui.comboHighlightStyle->findText(name, Qt::MatchFixedString);
	if(index != -1) {
		ui.comboHighlightStyle->setCurrentIndex(index);
	} else {
		ui.comboHighlightStyle->setCurrentIndex(0);
	}
	
}

/*
** Get the current information that the user has entered in the syntax
** highlighting dialog.  Return nullptr if the data is currently invalid
*/
PatternSet *DialogSyntaxPatterns::getDialogPatternSet() {

	// Get the line and character context values 
	if(ui.editContextLines->text().isEmpty()) {
		QMessageBox::critical(this, tr("Warning"), tr("Please supply a value for context lines"));
	}	

	bool ok;
	int lineContext = ui.editContextLines->text().toInt(&ok);
	if (!ok) {
		QMessageBox::critical(this, tr("Warning"), tr("Can't read integer value \"%1\" in context lines").arg(ui.editContextLines->text()));
		return nullptr;
	}
	

	// Get the line and character context values 
	if(ui.editContextChars->text().isEmpty()) {
		QMessageBox::critical(this, tr("Warning"), tr("Please supply a value for context chars"));
	}	

	int charContext = ui.editContextChars->text().toInt(&ok);
	if (!ok) {
		QMessageBox::critical(this, tr("Warning"), tr("Can't read integer value \"%1\" in context chars").arg(ui.editContextChars->text()));
		return nullptr;
	}


	/* Allocate a new pattern set structure and copy the fields read from the
	   dialog, including the modified pattern list into it */
	auto patSet = new PatternSet(ui.listItems->count());
	patSet->languageMode = ui.comboLanguageMode->currentText();
	patSet->lineContext  = lineContext;
	patSet->charContext  = charContext;
	
	for (int i = 0; i < ui.listItems->count(); i++) {		
		patSet->patterns[i] = *itemFromIndex(i);
	}
	
	return patSet;
}

/*
** Read the pattern fields of the highlight dialog, and produce an allocated
** HighlightPattern structure reflecting the contents, or pop up dialogs
** telling the user what's wrong (Passing "silent" as True, suppresses these
** dialogs).  Returns nullptr on error.
*/
HighlightPattern *DialogSyntaxPatterns::readDialogFields(bool silent) {

	auto pat = new HighlightPattern;

	// read the type buttons 
	int colorOnly = ui.radioColoring->isChecked();
	if (ui.radioPass2->isChecked()) {
		pat->flags |= DEFER_PARSING;
	} else if (colorOnly) {
		pat->flags = COLOR_ONLY;
	}

	// read the name field 
	QString name = ui.editPatternName->text().simplified();
	if (name.isNull()) {
		delete pat;
		return nullptr;
	}
	
	pat->name = name.toStdString();
	if (pat->name.empty()) {
		if (!silent) {
			QMessageBox::warning(this, tr("Pattern Name"), tr("Please specify a name for the pattern"));
		}
		delete pat;
		return nullptr;
	}

	// read the startRE field 
	pat->startRE = ui.editRegex->toPlainText();
	if (pat->startRE.isEmpty()) {
		if (!silent) {
			QMessageBox::warning(this, tr("Matching Regex"), tr("Please specify a regular expression to match"));
		}
		delete pat;
		return nullptr;
	}


	/* Make sure coloring patterns contain only sub-expression references
	   and put it in replacement regular-expression form */
	if (colorOnly) {
	
		QString outStr;
		outStr.reserve(pat->startRE.size());
	
		auto outPtr = std::back_inserter(outStr);
		
		for(QChar ch : pat->startRE) {
			if (ch != QLatin1Char(' ') && ch != QLatin1Char('\t')) {
				*outPtr++ = ch;
			}
		}
			
		pat->startRE = outStr;		
		QByteArray regexString = pat->startRE.toLatin1();
				
		if (strspn(regexString.data(), "&\\123456789 \t") != static_cast<size_t>(pat->startRE.size()) || (pat->startRE[0] != QLatin1Char('\\') && pat->startRE[0] != QLatin1Char('&')) || strstr(regexString.data(), "\\\\")) {
			if (!silent) {
				QMessageBox::warning(this, tr("Pattern Error"), tr("The expression field in patterns which specify highlighting for a parent, must contain only sub-expression references in regular expression replacement form (&\\1\\2 etc.).  See Help -> Regular Expressions and Help -> Syntax Highlighting for more information"));
			}
			delete pat;
			return nullptr;
		}
	}

	// read the parent field 
	if (ui.radioSubPattern->isChecked() || colorOnly) {
		QString parent = ui.editParentPattern->text().simplified();
		if (parent.isEmpty()) {
			if (!silent) {
				QMessageBox::warning(this, tr("Specify Parent Pattern"), tr("Please specify a parent pattern"));
			}
			delete pat;
			return nullptr;
		}

		if(!parent.isNull()) {
			pat->subPatternOf = parent;
		}
	}

	// read the styles option menu 
	pat->style = ui.comboHighlightStyle->currentText();

	// read the endRE field 
	if (colorOnly || ui.radioRangeRegex->isChecked()) {
		pat->endRE = ui.editRegexEnd->text();
		if (!colorOnly && pat->endRE.isEmpty()) {
			if (!silent) {
				QMessageBox::warning(this, tr("Specify Regex"), tr("Please specify an ending regular expression"));
			}
			delete pat;
			return nullptr;
		}
	}

	// read the errorRE field 
	if (ui.radioRangeRegex->isChecked()) {
		pat->errorRE = ui.editRegexError->text();
		if (pat->errorRE.isEmpty()) {
			pat->errorRE = QString();
		}
	}

	return pat;
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
bool DialogSyntaxPatterns::checkCurrentPattern(bool silent) {
	if(auto ptr = readDialogFields(silent)) {
		delete ptr;
		return true;
	}
	
	return false;
}
