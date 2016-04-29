
#include <QtDebug>
#include <QStringList>
#include <QString>
#include <QRegExpValidator>
#include <QRegExp>
#include <QMessageBox>
#include "DialogLanguageModes.h"
#include "LanguageMode.h"
#include "preferences.h"
#include "Document.h"
#include "MotifHelper.h"
#include "regularExp.h"
#include "tags.h"
#include "text.h" // for textNwordDelimiters
#include "highlightData.h"
#include "smartIndent.h"
#include "userCmds.h"

/* suplement wrap and indent styles w/ a value meaning "use default" for
   the override fields in the language modes dialog */
namespace {
const int DEFAULT_TAB_DIST    = -1;
const int DEFAULT_EM_TAB_DIST = -1;
}

//------------------------------------------------------------------------------
// Name: DialogLanguageModes
//------------------------------------------------------------------------------
DialogLanguageModes::DialogLanguageModes(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f) {
	ui.setupUi(this);

	for (int i = 0; i < NLanguageModes; i++) {
		languageModes_.push_back(copyLanguageModeRec(LanguageModes[i]));
	}

	ui.listLanguages->addItem(tr("New"));
	for (LanguageMode *language : languageModes_) {
		ui.listLanguages->addItem(QLatin1String(language->name));
	}
	ui.listLanguages->setCurrentRow(0);
	
	// Valid characters are letters, numbers, _, -, +, $, #, and internal whitespace.
	QValidator *validator = new QRegExpValidator(QRegExp(QLatin1String("[\\sA-Za-z0-9_+$#-]+")), this);
	
	ui.editName->setValidator(validator);
}

//------------------------------------------------------------------------------
// Name: ~DialogLanguageModes
//------------------------------------------------------------------------------
DialogLanguageModes::~DialogLanguageModes() {
	qDeleteAll(languageModes_);
}

//------------------------------------------------------------------------------
// Name: on_listLanguages_itemSelectionChanged
//------------------------------------------------------------------------------
void DialogLanguageModes::on_listLanguages_itemSelectionChanged() {
	QList<QListWidgetItem *> selections = ui.listLanguages->selectedItems();
	if(selections.size() != 1) {
		return;
	}

	QListWidgetItem *const selection = selections[0];
	QString languageName = selection->text();

	if(languageName == tr("New")) {
		ui.editName              ->setText(QString());
		ui.editExtensions        ->setText(QString());
		ui.editRegex             ->setText(QString());
		ui.editCallTips          ->setText(QString());
		ui.editDelimiters        ->setText(QString());
		ui.editTabSpacing        ->setText(QString());
		ui.editEmulatedTabSpacing->setText(QString());

		ui.radioIndentDefault->setChecked(true);
		ui.radioWrapDefault  ->setChecked(true);
		
		ui.buttonUp    ->setEnabled(false);
		ui.buttonDown  ->setEnabled(false);
		ui.buttonDelete->setEnabled(false);
		ui.buttonCopy  ->setEnabled(false);
	} else {
		
		const int i = ui.listLanguages->row(selection) - 1;

		LanguageMode *language = languageModes_[i];

		QStringList extensions;
		for(int i = 0; i < language->nExtensions; ++i) {
			extensions.push_back(QLatin1String(language->extensions[i]));
		}

		ui.editName      ->setText(QLatin1String(language->name));
		ui.editExtensions->setText(extensions.join(tr(" ")));
		ui.editRegex     ->setText(QLatin1String(language->recognitionExpr));
		ui.editCallTips  ->setText(QLatin1String(language->defTipsFile));
		ui.editDelimiters->setText(QLatin1String(language->delimiters));

		if(language->tabDist != -1) {
			ui.editTabSpacing->setText(tr("%1").arg(language->tabDist));
		} else {
			ui.editTabSpacing->setText(QString());
		}

		if(language->emTabDist != -1) {
			ui.editEmulatedTabSpacing->setText(tr("%1").arg(language->emTabDist));
		} else {
			ui.editEmulatedTabSpacing->setText(QString());
		}

		switch(language->indentStyle) {
		case 0:
			ui.radioIndentNone->setChecked(true);
			break;
		case 1:
			ui.radioIndentAuto->setChecked(true);
			break;
		case 2:
			ui.radioIndentSmart->setChecked(true);
			break;
		default:
			ui.radioIndentDefault->setChecked(true);
			break;
		}

		switch(language->wrapStyle) {
		case 0:
			ui.radioWrapNone->setChecked(true);
			break;
		case 1:
			ui.radioWrapAuto->setChecked(true);
			break;
		case 2:
			ui.radioWrapContinuous->setChecked(true);
			break;
		default:
			ui.radioWrapDefault->setChecked(true);
			break;
		}

		if(i == 0) {
			ui.buttonUp    ->setEnabled(false);
			ui.buttonDown  ->setEnabled(languageModes_.size() > 2);
			ui.buttonDelete->setEnabled(true);
			ui.buttonCopy  ->setEnabled(true);
		} else if(i == languageModes_.size() - 1) {
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
	}
}

//------------------------------------------------------------------------------
// Name: on_buttonBox_accepted
//------------------------------------------------------------------------------
void DialogLanguageModes::on_buttonBox_accepted() {
	if (!updateLMList(false)) {
		return;
	}
	
	accept();
}

//------------------------------------------------------------------------------
// Name: on_buttonBox_clicked
//------------------------------------------------------------------------------
void DialogLanguageModes::on_buttonBox_clicked(QAbstractButton *button) {
	if(ui.buttonBox->standardButton(button) == QDialogButtonBox::Apply) {
		updateLMList(false);
	}
}

/*
** Read the fields in the language modes dialog and create a LanguageMode data
** structure reflecting the current state of the selected language mode in the dialog.
** If any of the information is incorrect or missing, display a warning dialog and
** return nullptr.  Passing "silent" as True, suppresses the warning dialogs.
*/
LanguageMode *DialogLanguageModes::readLMDialogFields(bool silent) {

	/* Allocate a language mode structure to return, set unread fields to
	   empty so everything can be freed on errors by freeLanguageModeRec */
	auto lm = new LanguageMode;
	
	lm->name            = nullptr;
	lm->nExtensions     = 0;
	lm->extensions      = nullptr;
	lm->recognitionExpr = nullptr;	
	lm->defTipsFile     = nullptr;
	lm->delimiters      = nullptr;
	lm->wrapStyle       = 0;
	lm->indentStyle     = 0;
	lm->tabDist         = 0;
	lm->emTabDist       = 0;

	// read the name field 
	QString name = ui.editName->text().simplified();	
	if (name.isEmpty()) {
		if (!silent) {
			QMessageBox::warning(this, tr("Language Mode Name"), tr("Please specify a name for the language mode"));
		}
		freeLanguageModeRec(lm);
		return nullptr;
	}
	
	lm->name = XtStringDup(name);
	
	// read the extension list field 
	QString extStr      = ui.editExtensions->text().simplified();
	QStringList extList = extStr.split(QLatin1Char(' '), QString::SkipEmptyParts);
	lm->extensions  = new char*[extList.size()];
	lm->nExtensions = extList.size();
	int i = 0;
	for(QString ext : extList) {
		lm->extensions[i] = XtStringDup(ext);
		++i;
	}
	
	// read recognition expression 
	QString recognitionExpr = ui.editRegex->text();
	if(!recognitionExpr.isEmpty()) {
		regexp *compiledRE;
		try {
			std::string expression = recognitionExpr.toStdString();
			compiledRE = new regexp(expression, REDFLT_STANDARD);

		} catch(const regex_error &e) {
			if (!silent) {
				QMessageBox::warning(this, tr("Regex"), tr("Recognition expression:\n%1").arg(QLatin1String(e.what())));
			}
			freeLanguageModeRec(lm);
			return nullptr;
		}

		delete compiledRE;	
	}
	
	// Read the default calltips file for the language mode 
	QString tipsFile = ui.editCallTips->text();
	if(!tipsFile.isEmpty()) {
		// Ensure that AddTagsFile will work 
		if (AddTagsFile(lm->defTipsFile, TIP) == FALSE) {
			if (!silent) {
				QMessageBox::warning(this, tr("Error reading Calltips"), tr("Can't read default calltips file(s):\n  \"%1\"\n").arg(QLatin1String(lm->defTipsFile)));
			}
			freeLanguageModeRec(lm);
			return nullptr;
		} else if (DeleteTagsFile(lm->defTipsFile, TIP, False) == FALSE) {
			fprintf(stderr, "nedit: Internal error: Trouble deleting calltips file(s):\n  \"%s\"\n", lm->defTipsFile);
		}	
	}
	
	// read tab spacing field 	
	QString tabsSpacing = ui.editTabSpacing->text();
	if(tabsSpacing.isEmpty()) {
		lm->tabDist = DEFAULT_TAB_DIST;
	} else {
		bool ok;
		int tabsSpacingValue = tabsSpacing.toInt(&ok);
		if (!ok) {
			QMessageBox::warning(this, tr("Warning"), tr("Can't read integer value \"%1\" in tab spacing").arg(tabsSpacing));
			freeLanguageModeRec(lm);
			return nullptr;
		}
		
		lm->tabDist = tabsSpacingValue;

		if (lm->tabDist <= 0 || lm->tabDist > 100) {
			if (!silent) {
				QMessageBox::warning(this, tr("Invalid Tab Spacing"), tr("Invalid tab spacing: %1").arg(lm->tabDist));
			}
			freeLanguageModeRec(lm);
			return nullptr;
		}	
	}
	
	// read emulated tab field 
	QString emulatedTabSpacing = ui.editEmulatedTabSpacing->text();
	if(emulatedTabSpacing.isEmpty()) {
		lm->emTabDist = DEFAULT_EM_TAB_DIST;
	} else {
		bool ok;
		int emulatedTabSpacingValue = emulatedTabSpacing.toInt(&ok);
		if (!ok) {
			QMessageBox::warning(this, tr("Warning"), tr("Can't read integer value \"%1\" in emulated tab spacing").arg(tabsSpacing));
			freeLanguageModeRec(lm);
			return nullptr;
		}
		
		lm->emTabDist = emulatedTabSpacingValue;

		if (lm->emTabDist < 0 || lm->emTabDist > 100) {
			if (!silent) {
				QMessageBox::warning(this, tr("Invalid Tab Spacing"), tr("Invalid emulated tab spacing: %1").arg(lm->emTabDist));
			}
			freeLanguageModeRec(lm);
			return nullptr;
		}	
	}
	
	// read delimiters string 
	QString delimiters = ui.editDelimiters->text();
	if(!delimiters.isEmpty()) {
		lm->delimiters = XtStringDup(delimiters);
	}
	
	// read indent style 
	if(ui.radioIndentNone->isChecked()) {
		lm->indentStyle = NO_AUTO_INDENT;
	} else if(ui.radioIndentAuto->isChecked()) {
		lm->indentStyle = AUTO_INDENT;
	} else if(ui.radioIndentSmart->isChecked()) {
		lm->indentStyle = SMART_INDENT;
	} else if(ui.radioIndentDefault->isChecked()) {
		lm->indentStyle = DEFAULT_INDENT;
	}
	
	// read wrap style 
	if(ui.radioWrapNone->isChecked()) {
		lm->wrapStyle = NO_WRAP;
	} else if(ui.radioWrapAuto->isChecked()) {
		lm->wrapStyle = NEWLINE_WRAP;
	} else if(ui.radioWrapContinuous->isChecked()) {
		lm->wrapStyle = CONTINUOUS_WRAP;
	} else if(ui.radioWrapDefault->isChecked()) {
		lm->wrapStyle = DEFAULT_WRAP;
	}

	return lm;
}

//------------------------------------------------------------------------------
// Name: on_buttonCopy_clicked
//------------------------------------------------------------------------------
void DialogLanguageModes::on_buttonCopy_clicked() {

	// TODO(eteran): update entry we are leaving

	QList<QListWidgetItem *> selections = ui.listLanguages->selectedItems();
	if(selections.size() != 1) {
		return;
	}

	QListWidgetItem *const selection = selections[0];
	QString languageName = selection->text();

	const int i = ui.listLanguages->row(selection) - 1;
	
	LanguageMode *newLM = copyLanguageModeRec(languageModes_[i]);
	languageModes_.insert(i, newLM);
	ui.listLanguages->insertItem(i + 1, QLatin1String(newLM->name));
	
}

//------------------------------------------------------------------------------
// Name: updateLanguageList
//------------------------------------------------------------------------------
bool DialogLanguageModes::updateLanguageList(bool silent) {
	QList<QListWidgetItem *> selections = ui.listLanguages->selectedItems();
	if(selections.size() != 1) {
		return false;
	}

	QListWidgetItem *const selection = selections[0];
	QString languageName = selection->text();
	
	if(languageName == tr("New")) {

		if(LanguageMode *newLM = readLMDialogFields(silent)) {	
			languageModes_.push_back(newLM);
			ui.listLanguages->addItem(QLatin1String(newLM->name));
			ui.listLanguages->setCurrentRow(ui.listLanguages->count() - 1);
			return true;
		}

	} else {
		const int i = ui.listLanguages->row(selection) - 1;
		LanguageMode *oldLM = languageModes_[i];

		if(LanguageMode *newLM = readLMDialogFields(silent)) {

			/* If there was a name change of a non-duplicate language mode, modify the
			   name to the weird format of: ":old name:new name".  This signals that a
			   name change is necessary in lm dependent data such as highlight
			   patterns.  Duplicate language modes may be re-named at will, since no
			   data will be lost due to the name change. */
			if (oldLM != nullptr && strcmp(oldLM->name, newLM->name)) {
				int nCopies = 0;

				for (int i = 0; i < languageModes_.size(); i++) {
					if (strcmp(oldLM->name, languageModes_[i]->name) == 0) {
						nCopies++;
					}
				}

				if (nCopies <= 1) {
					int oldLen = strchr(oldLM->name, ':') == nullptr ? strlen(oldLM->name) : strchr(oldLM->name, ':') - oldLM->name;
					char *tempName = XtMalloc(oldLen + strlen(newLM->name) + 2);

					strncpy(tempName, oldLM->name, oldLen);
					sprintf(&tempName[oldLen], ":%s", newLM->name);

					XtFree(newLM->name);
					newLM->name = tempName;
				}			
			}

			languageModes_[i] = newLM;			
			delete oldLM;
			return true;
		}
	}
	
	return false;
}

//------------------------------------------------------------------------------
// Name: updateLMList
//------------------------------------------------------------------------------
bool DialogLanguageModes::updateLMList(bool silent) {

	int oldLanguageMode;
	char *oldModeName;
	char *newDelimiters;

	// Get the current contents of the dialog fields 
	if(!updateLanguageList(silent)) {
		return false;
	}

	/* Fix up language mode indices in all open windows (which may change
	   if the currently selected mode is deleted or has changed position),
	   and update word delimiters */
	for(Document *window: WindowList) {
		if (window->languageMode_ != PLAIN_LANGUAGE_MODE) {
			oldLanguageMode = window->languageMode_;
			oldModeName = LanguageModes[window->languageMode_]->name;
			window->languageMode_ = PLAIN_LANGUAGE_MODE;

			for (int i = 0; i < languageModes_.size(); i++) {

				if (!strcmp(oldModeName, languageModes_[i]->name)) {
					newDelimiters = languageModes_[i]->delimiters;
					if(!newDelimiters) {
						newDelimiters = GetPrefDelimiters();
					}
					
					XtVaSetValues(window->textArea_, textNwordDelimiters, newDelimiters, nullptr);
					for (int j = 0; j < window->nPanes_; j++) {
						XtVaSetValues(window->textPanes_[j], textNwordDelimiters, newDelimiters, nullptr);
					}
					
					// don't forget to adapt the LM stored within the user menu cache 
					if (window->userMenuCache_->umcLanguageMode == oldLanguageMode) {
						window->userMenuCache_->umcLanguageMode = i;
					}
					
					if (window->userBGMenuCache_.ubmcLanguageMode == oldLanguageMode) {
						window->userBGMenuCache_.ubmcLanguageMode = i;
					}
					
					// update the language mode of this window (document) 
					window->languageMode_ = i;
					break;
				}
			}
		}
	}


	/* If there were any name changes, re-name dependent highlight patterns
	   and smart-indent macros and fix up the weird rename-format names */	   
	// naming causes the name to be set to the format of "Old:New"
	// update names appropriately
	for (int i = 0; i < languageModes_.size(); i++) {
		if (strchr(languageModes_[i]->name, ':')) {
			char *newName = strrchr(languageModes_[i]->name, ':') + 1;
			*strchr(languageModes_[i]->name, ':') = '\0';
			RenameHighlightPattern(languageModes_[i]->name, newName);
			RenameSmartIndentMacros(languageModes_[i]->name, newName);
			memmove(languageModes_[i]->name, newName, strlen(newName) + 1);
		}
	}

	// Replace the old language mode list with the new one from the dialog 
	for (int i = 0; i < NLanguageModes; i++) {
		freeLanguageModeRec(LanguageModes[i]);
	}
	
	for (int i = 0; i < languageModes_.size(); i++) {
		LanguageModes[i] = copyLanguageModeRec(languageModes_[i]);
	}
	
	NLanguageModes = languageModes_.size();
	

	/* Update user menu info to update language mode dependencies of
	   user menu items */
	UpdateUserMenuInfo();

	/* Update the menus in the window menu bars and load any needed
	    calltips files */
	for(Document *window: WindowList) {
		updateLanguageModeSubmenu(window);
		if (window->languageMode_ != PLAIN_LANGUAGE_MODE && LanguageModes[window->languageMode_]->defTipsFile != nullptr)
			AddTagsFile(LanguageModes[window->languageMode_]->defTipsFile, TIP);
		// cache user menus: Rebuild all user menus of this window 
		RebuildAllMenus(window);
	}

	// If a syntax highlighting dialog is up, update its menu 
	UpdateLanguageModeMenu();
	// The same for the smart indent macro dialog 
	UpdateLangModeMenuSmartIndent();
	// Note that preferences have been changed 
	MarkPrefsChanged();

	return true;
}

//------------------------------------------------------------------------------
// Name: on_buttonDelete_clicked
//------------------------------------------------------------------------------
void DialogLanguageModes::on_buttonDelete_clicked() {

	QList<QListWidgetItem *> selections = ui.listLanguages->selectedItems();
	if(selections.size() != 1) {
		return; // false
	}

	QListWidgetItem *const selection = selections[0];
	QString languageName = selection->text();

	const int itemIndex = ui.listLanguages->row(selection) - 1;

	// Allow duplicate names to be deleted regardless of dependencies 
	for (int i = 0; i < languageModes_.size(); i++) {
		if (i != itemIndex && !strcmp(languageModes_[i]->name, languageModes_[itemIndex]->name)) {
			languageModes_.removeAt(itemIndex);
			delete selection;
			// force an update of the display
			Q_EMIT on_listLanguages_itemSelectionChanged();
			return; // True;
		}
	}

	// don't allow deletion if data will be lost 
	if (LMHasHighlightPatterns(QLatin1String(languageModes_[itemIndex]->name))) {
		QMessageBox::warning(this, tr("Patterns exist"), tr("This language mode has syntax highlighting patterns defined.  Please delete the patterns first, in Preferences -> Default Settings -> Syntax Highlighting, before proceeding here."));
		return; // False;
	}

	// don't allow deletion if data will be lost 
	if (LMHasSmartIndentMacros(languageModes_[itemIndex]->name)) {
		QMessageBox::warning(this, tr("Smart Indent Macros exist"), tr("This language mode has smart indent macros defined.  Please delete the macros first, in Preferences -> Default Settings -> Auto Indent -> Program Smart Indent, before proceeding here."));
		return; // False;
	}

	languageModes_.removeAt(itemIndex);
	delete selection;
	Q_EMIT on_listLanguages_itemSelectionChanged();
	return;// True;
}

//------------------------------------------------------------------------------
// Name: on_buttonUp_clicked
//------------------------------------------------------------------------------
void DialogLanguageModes::on_buttonUp_clicked() {

	QList<QListWidgetItem *> selections = ui.listLanguages->selectedItems();
	if(selections.size() != 1) {
		return;
	}

	QListWidgetItem *const selection = selections[0];

	const int i = ui.listLanguages->row(selection) - 1;
	if(i != 0) {
		languageModes_.move(i, i - 1);
		
		// offset by 1 because of "New" at the top
		QListWidgetItem *item = ui.listLanguages->takeItem(i + 1);
		ui.listLanguages->insertItem(i, item);
		ui.listLanguages->scrollToItem(item);
		item->setSelected(true);
	}
}

//------------------------------------------------------------------------------
// Name: on_buttonDown_clicked
//------------------------------------------------------------------------------
void DialogLanguageModes::on_buttonDown_clicked() {

	QList<QListWidgetItem *> selections = ui.listLanguages->selectedItems();
	if(selections.size() != 1) {
		return;
	}

	QListWidgetItem *const selection = selections[0];

	const int i = ui.listLanguages->row(selection) - 1;
	if(i != languageModes_.size() - 1) {
		languageModes_.move(i, i + 1);
		
		// offset by 1 because of "New" at the top
		QListWidgetItem *item = ui.listLanguages->takeItem(i + 1);
		ui.listLanguages->insertItem(i + 2, item);
		ui.listLanguages->scrollToItem(item);
		item->setSelected(true);
	}
}

// TODO(eteran): what code path actually leads to this code being called
// see lmGetDisplayedCB @ preferences.c
#if 0
	/* If there are problems, and the user didn't ask for the fields to be
	   read, give more warning */
	if (!explicitRequest) {
	
		QMessageBox messageBox(nullptr /*LMDialog.shell*/);
		messageBox.setWindowTitle(tr("Discard Language Mode"));
		messageBox.setIcon(QMessageBox::Warning);
		messageBox.setText(tr("Discard incomplete entry for current language mode?"));
		
		QPushButton *buttonKeep    = messageBox.addButton(tr("Keep"), QMessageBox::RejectRole);
		QPushButton *buttonDiscard = messageBox.addButton(tr("Discard"), QMessageBox::AcceptRole);
		Q_UNUSED(buttonKeep);

		messageBox.exec();
		if(messageBox.clickedButton() == buttonDiscard) {
			return oldItem == nullptr ? nullptr : copyLanguageModeRec((LanguageMode *)oldItem);
		}
	}
#endif
