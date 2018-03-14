
#include "DialogLanguageModes.h"
#include "DialogSyntaxPatterns.h"
#include "DocumentWidget.h"
#include "Highlight.h"
#include "LanguageMode.h"
#include "LanguageModeModel.h"
#include "MainWindow.h"
#include "preferences.h"
#include "Regex.h"
#include "Util/regex.h"
#include "Search.h"
#include "SmartIndent.h"
#include "TextArea.h"
#include "userCmds.h"

#include <QMessageBox>

/**
 * @brief DialogLanguageModes::DialogLanguageModes
 * @param parent
 * @param f
 */
DialogLanguageModes::DialogLanguageModes(DialogSyntaxPatterns *dialogSyntaxPatterns, QWidget *parent, Qt::WindowFlags f) : Dialog(parent, f), dialogSyntaxPatterns_(dialogSyntaxPatterns) {
	ui.setupUi(this);

    model_ = new LanguageModeModel(this);
    ui.listItems->setModel(model_);

    // Copy the list of menu information to one that the user can freely edit
    for(const LanguageMode &lang : Preferences::LanguageModes) {
        model_->addItem(lang);
    }

    connect(ui.listItems->selectionModel(), &QItemSelectionModel::currentChanged, this, &DialogLanguageModes::currentChanged, Qt::QueuedConnection);
    connect(this, &DialogLanguageModes::restore, this, &DialogLanguageModes::restoreSlot, Qt::QueuedConnection);

    // default to selecting the first item
    if(model_->rowCount() != 0) {
        QModelIndex index = model_->index(0, 0);
        ui.listItems->setCurrentIndex(index);
    }

	// Valid characters are letters, numbers, _, -, +, $, #, and internal whitespace.
	ui.editName->setValidator(new QRegExpValidator(QRegExp(QLatin1String("[\\sA-Za-z0-9_+$#-]+")), this));

	// 0-100
	ui.editTabSpacing->setValidator(new QIntValidator(0, 100, this));
	ui.editEmulatedTabSpacing->setValidator(new QIntValidator(-1, 100, this));
}

/**
 * @brief DialogLanguageModes::updateButtonStates
 */
void DialogLanguageModes::updateButtonStates() {
    QModelIndex index = ui.listItems->currentIndex();
    updateButtonStates(index);
}

/**
 * @brief DialogLanguageModes::updateButtonStates
 */
void DialogLanguageModes::updateButtonStates(const QModelIndex &current) {
    if(current.isValid()) {
        if(current.row() == 0) {
            ui.buttonUp    ->setEnabled(false);
            ui.buttonDown  ->setEnabled(model_->rowCount() > 1);
            ui.buttonDelete->setEnabled(true);
            ui.buttonCopy  ->setEnabled(true);
        } else if(current.row() == model_->rowCount() - 1) {
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
        ui.buttonUp    ->setEnabled(false);
        ui.buttonDown  ->setEnabled(false);
        ui.buttonDelete->setEnabled(false);
        ui.buttonCopy  ->setEnabled(false);
    }
}

/**
 * @brief DialogLanguageModes::restoreSlot
 * @param index
 */
void DialogLanguageModes::restoreSlot(const QModelIndex &index) {
    ui.listItems->setCurrentIndex(index);
}

void DialogLanguageModes::currentChanged(const QModelIndex &current, const QModelIndex &previous) {
    static bool canceled = false;

    if (canceled) {
        canceled = false;
        return;
    }

    // if we are actually switching items, check that the previous one was valid
    // so we can optionally cancel

    // NOTE(eteran): in the original nedit 5.6, this code is broken!
    //               it results in a crash when leaving invalid entries :-(
    //               we can do better of course
#if 1
    if(previous.isValid() && previous != deleted_ && !validateFields(Verbosity::Silent)) {
        QMessageBox messageBox(this);
        messageBox.setWindowTitle(tr("Discard Entry"));
        messageBox.setIcon(QMessageBox::Warning);
        messageBox.setText(tr("Discard incomplete entry for current menu item?"));
        QPushButton *buttonKeep    = messageBox.addButton(tr("Keep"), QMessageBox::RejectRole);
        QPushButton *buttonDiscard = messageBox.addButton(QMessageBox::Discard);
        Q_UNUSED(buttonDiscard);

        messageBox.exec();
        if (messageBox.clickedButton() == buttonKeep) {

            // again to cause messagebox to pop up
            validateFields(Verbosity::Verbose);

            // reselect the old item
            canceled = true;
            Q_EMIT restore(previous);
            return;
        }
    }
#endif

    // NOTE(eteran): this is only safe if we aren't moving due to a delete operation
    if(previous.isValid() && previous != deleted_) {
        if(!updateCurrentItem(previous)) {
            // reselect the old item
            canceled = true;
            Q_EMIT restore(previous);
            return;
        }
    }

    // previous was OK, so let's update the contents of the dialog
    if(auto ptr = model_->itemFromIndex(current)) {
        QStringList extensions;
        for(const QString &extension : ptr->extensions) {
            extensions.push_back(extension);
        }

        ui.editName      ->setText(ptr->name);
        ui.editExtensions->setText(extensions.join(QLatin1Char(' ')));
        ui.editRegex     ->setText(ptr->recognitionExpr);
        ui.editCallTips  ->setText(ptr->defTipsFile);
        ui.editDelimiters->setText(ptr->delimiters);

        if(ptr->tabDist != LanguageMode::DEFAULT_TAB_DIST) {
            ui.editTabSpacing->setText(QString::number(ptr->tabDist));
        } else {
            ui.editTabSpacing->setText(QString());
        }

        if(ptr->emTabDist != LanguageMode::DEFAULT_EM_TAB_DIST) {
            ui.editEmulatedTabSpacing->setText(QString::number(ptr->emTabDist));
        } else {
            ui.editEmulatedTabSpacing->setText(QString());
        }

        switch(ptr->indentStyle) {
        case IndentStyle::None:
            ui.radioIndentNone->setChecked(true);
            break;
        case IndentStyle::Auto:
            ui.radioIndentAuto->setChecked(true);
            break;
        case IndentStyle::Smart:
            ui.radioIndentSmart->setChecked(true);
            break;
        default:
            ui.radioIndentDefault->setChecked(true);
            break;
        }

        switch(ptr->wrapStyle) {
        case WrapStyle::None:
            ui.radioWrapNone->setChecked(true);
            break;
        case WrapStyle::Newline:
            ui.radioWrapAuto->setChecked(true);
            break;
        case WrapStyle::Continuous:
            ui.radioWrapContinuous->setChecked(true);
            break;
        default:
            ui.radioWrapDefault->setChecked(true);
            break;
        }
    } else {
        ui.editName      ->setText(QString());
        ui.editExtensions->setText(QString());
        ui.editRegex     ->setText(QString());
        ui.editCallTips  ->setText(QString());
        ui.editDelimiters->setText(QString());
        ui.editTabSpacing->setText(QString());
        ui.editEmulatedTabSpacing->setText(QString());
        ui.radioWrapNone->setChecked(true);
        ui.radioIndentNone->setChecked(true);
    }

    // ensure that the appropriate buttons are enabled
    updateButtonStates(current);
}

/**
 * @brief DialogLanguageModes::on_buttonBox_accepted
 */
void DialogLanguageModes::on_buttonBox_accepted() {
    if (!updateLMList(Verbosity::Verbose)) {
		return;
	}

	accept();
}

/**
 * @brief DialogLanguageModes::on_buttonBox_clicked
 * @param button
 */
void DialogLanguageModes::on_buttonBox_clicked(QAbstractButton *button) {
	if(ui.buttonBox->standardButton(button) == QDialogButtonBox::Apply) {
        updateLMList(Verbosity::Verbose);
	}
}

/*
** Read the fields in the language modes dialog and create a LanguageMode data
** structure reflecting the current state of the selected language mode in the dialog.
** If any of the information is incorrect or missing, display a warning dialog and
** return nullptr.
*/
std::unique_ptr<LanguageMode> DialogLanguageModes::readFields(Verbosity verbosity) {

	/* Allocate a language mode structure to return, set unread fields to
	   empty so everything can be freed on errors by freeLanguageModeRec */
    auto lm = std::make_unique<LanguageMode>();

	// read the name field
	QString name = ui.editName->text().simplified();
	if (name.isEmpty()) {
        if (verbosity == Verbosity::Verbose) {
			QMessageBox::warning(this, tr("Language Mode Name"), tr("Please specify a name for the language mode"));
		}
		return nullptr;
	}

	lm->name = name;

	// read the extension list field
	QString extStr      = ui.editExtensions->text().simplified();
	QStringList extList = extStr.split(QLatin1Char(' '), QString::SkipEmptyParts);
	lm->extensions = extList;

	// read recognition expression
	QString recognitionExpr = ui.editRegex->text();
	if(!recognitionExpr.isEmpty()) {
		try {
            auto compiledRE = make_regex(recognitionExpr, REDFLT_STANDARD);
        } catch(const RegexError &e) {
            if (verbosity == Verbosity::Verbose) {
                QMessageBox::warning(this, tr("Regex"), tr("Recognition expression:\n%1").arg(QString::fromLatin1(e.what())));
			}
			return nullptr;
		}
	}
    lm->recognitionExpr = !recognitionExpr.isEmpty() ? recognitionExpr : QString();

	// Read the default calltips file for the language mode
	QString tipsFile = ui.editCallTips->text();
	if(!tipsFile.isEmpty()) {
		// Ensure that AddTagsFile will work
        if (!Tags::AddTagsFileEx(tipsFile, Tags::SearchMode::TIP)) {
            if (verbosity == Verbosity::Verbose) {
				QMessageBox::warning(this, tr("Error reading Calltips"), tr("Can't read default calltips file(s):\n  \"%1\"\n").arg(tipsFile));
			}
			return nullptr;
        } else if (!Tags::DeleteTagsFileEx(tipsFile, Tags::SearchMode::TIP, false)) {
            qCritical("NEdit: Internal error: Trouble deleting calltips file(s):\n  \"%s\"", qPrintable(tipsFile));
		}
	}
    lm->defTipsFile = !tipsFile.isEmpty() ? tipsFile : QString();

	// read tab spacing field
	QString tabsSpacing = ui.editTabSpacing->text();
	if(tabsSpacing.isEmpty()) {
		lm->tabDist = LanguageMode::DEFAULT_TAB_DIST;
	} else {
		bool ok;
		int tabsSpacingValue = tabsSpacing.toInt(&ok);
		if (!ok) {
			QMessageBox::warning(this, tr("Warning"), tr("Can't read integer value \"%1\" in tab spacing").arg(tabsSpacing));
			return nullptr;
		}

		lm->tabDist = tabsSpacingValue;
	}

	// read emulated tab field
	QString emulatedTabSpacing = ui.editEmulatedTabSpacing->text();
	if(emulatedTabSpacing.isEmpty()) {
		lm->emTabDist = LanguageMode::DEFAULT_EM_TAB_DIST;
	} else {
		bool ok;
		int emulatedTabSpacingValue = emulatedTabSpacing.toInt(&ok);
		if (!ok) {
			QMessageBox::warning(this, tr("Warning"), tr("Can't read integer value \"%1\" in emulated tab spacing").arg(tabsSpacing));
			return nullptr;
		}

		lm->emTabDist = emulatedTabSpacingValue;
	}

	// read delimiters string
	QString delimiters = ui.editDelimiters->text();
	if(!delimiters.isEmpty()) {
		lm->delimiters = delimiters;
	}

	// read indent style
	if(ui.radioIndentNone->isChecked()) {
        lm->indentStyle = IndentStyle::None;
	} else if(ui.radioIndentAuto->isChecked()) {
        lm->indentStyle = IndentStyle::Auto;
	} else if(ui.radioIndentSmart->isChecked()) {
        lm->indentStyle = IndentStyle::Smart;
	} else if(ui.radioIndentDefault->isChecked()) {
        lm->indentStyle = IndentStyle::Default;
	}

	// read wrap style
	if(ui.radioWrapNone->isChecked()) {
        lm->wrapStyle = WrapStyle::None;
	} else if(ui.radioWrapAuto->isChecked()) {
        lm->wrapStyle = WrapStyle::Newline;
	} else if(ui.radioWrapContinuous->isChecked()) {
        lm->wrapStyle = WrapStyle::Continuous;
	} else if(ui.radioWrapDefault->isChecked()) {
        lm->wrapStyle = WrapStyle::Default;
	}

    return lm;
}

/**
 * @brief DialogLanguageModes::updateLanguageList
 * @param mode
 * @return
 */
bool DialogLanguageModes::updateLanguageList(Verbosity verbosity) {

    QModelIndex index = ui.listItems->currentIndex();
    if(!index.isValid()) {
        return false;
    }

    if(const LanguageMode *oldLM = model_->itemFromIndex(index)) {

        if(auto newLM = readFields(verbosity)) {

            /* If there was a name change of a non-duplicate language mode, modify the
               name to the weird format of: "old name:new name".  This signals that a
               name change is necessary in lm dependent data such as highlight
               patterns.  Duplicate language modes may be re-named at will, since no
               data will be lost due to the name change. */
            if (oldLM->name != newLM->name) {
                int count = countLanguageModes(oldLM->name);
                if (count <= 1) {
                    int index = oldLM->name.indexOf(QLatin1Char(':'));
                    int oldLen = (index == -1) ? oldLM->name.size() : index;

                    auto tempName = tr("%1:%2").arg(oldLM->name.mid(0, oldLen), newLM->name);
                    newLM->name = tempName;
                }
            }

            model_->updateItem(index, *newLM);
            return true;
        }
    }

	return false;
}

/**
 * @brief DialogLanguageModes::updateLMList
 * @param mode
 * @return
 */
bool DialogLanguageModes::updateLMList(Verbosity verbosity) {

	// Get the current contents of the dialog fields
    if(!updateLanguageList(verbosity)) {
		return false;
	}

    /* Fix up language mode indices in all open documents (which may change
       if the currently selected mode is deleted or has changed position),
       and update word delimiters */
    for(DocumentWidget *document : DocumentWidget::allDocuments()) {

        const size_t languageMode = document->GetLanguageMode();

        if (languageMode != PLAIN_LANGUAGE_MODE) {

            const QString documentLanguage = Preferences::LanguageModes[languageMode].name;

            // assume plain as default
            document->languageMode_ = PLAIN_LANGUAGE_MODE;

            // but let's try to match it to an entry in the new language list...
            for (int i = 0; i < model_->rowCount(); i++) {

                QModelIndex index = model_->index(i, 0);
                const LanguageMode *lang = model_->itemFromIndex(index);

                QStringList parts = lang->name.split(QLatin1Char(':'));
                QString name = (parts.size() == 2) ? parts[0] : lang->name;

                // OK, a language matched this document, update it!
                if (name == documentLanguage) {
                    QString newDelimiters = lang->delimiters;

                    if(newDelimiters.isNull()) {
                        newDelimiters = Preferences::GetPrefDelimiters();
                    }

                    for(TextArea *area : document->textPanes()) {
                        area->setWordDelimiters(newDelimiters.toStdString());
                    }

                    document->languageMode_ = static_cast<size_t>(i);
                    break;
                }
            }
        }

        /* If there were any name changes, re-name dependent highlight patterns
           and smart-indent macros and fix up the weird rename-format names */

        // naming causes the name to be set to the format of "Old:New"
        // update names appropriately
        for (int i = 0; i < model_->rowCount(); i++) {

            QModelIndex index = model_->index(i, 0);
            const LanguageMode *lang = model_->itemFromIndex(index);

            QStringList parts = lang->name.split(QLatin1Char(':'));

            if (parts.size() == 2) {
                QString oldName = parts[0];
                QString newName = parts[1];

                MainWindow::RenameHighlightPattern(oldName, newName);
                if(dialogSyntaxPatterns_) {
                    dialogSyntaxPatterns_->RenameHighlightPattern(oldName, newName);
                }

                SmartIndent::RenameSmartIndentMacros(oldName, newName);

                // make a copy of the language mode, and set the new name
                LanguageMode newLanguageMode = *lang;
                newLanguageMode.name = newName;

                model_->updateItem(index, newLanguageMode);
            }
        }

        // Replace the old language mode list with the new one from the dialog
        Preferences::LanguageModes.clear();

        for (int i = 0; i < model_->rowCount(); i++) {
            QModelIndex index = model_->index(i, 0);
            auto item = model_->itemFromIndex(index);
            Preferences::LanguageModes.push_back(*item);
        }

        /* Update user menu info to update language mode dependencies of
           user menu items */
        UpdateUserMenuInfo();

        // Update the menus in the window menu bars ...
        for(MainWindow *win : MainWindow::allWindows()) {
            win->updateLanguageModeSubmenu();
        }

        // and load any needed calltips files ...
        for(DocumentWidget *document : DocumentWidget::allDocuments()) {
            const size_t languageMode = document->GetLanguageMode();
            if (languageMode != PLAIN_LANGUAGE_MODE && !Preferences::LanguageModes[languageMode].defTipsFile.isNull()) {
                Tags::AddTagsFileEx(Preferences::LanguageModes[languageMode].defTipsFile, Tags::SearchMode::TIP);
            }
        }

        // If a syntax highlighting dialog is up, update its menu
        if(dialogSyntaxPatterns_) {
            dialogSyntaxPatterns_->UpdateLanguageModeMenu();
        }

		// The same for the smart indent macro dialog
        SmartIndent::UpdateLangModeMenuSmartIndent();

        // Note that preferences have been changed
        Preferences::MarkPrefsChanged();
    }

	return true;
}

/**
 * @brief DialogLanguageModes::on_buttonNew_clicked
 */
void DialogLanguageModes::on_buttonNew_clicked() {

    if(!updateCurrentItem()) {
        return;
    }

    LanguageMode item;
    // some sensible defaults...
    item.name  = tr("New Item");
    model_->addItem(item);

    QModelIndex index = model_->index(model_->rowCount() - 1, 0);
    ui.listItems->setCurrentIndex(index);

    ui.listItems->scrollTo(ui.listItems->currentIndex());
    updateButtonStates();
}

/**
 * @brief DialogLanguageModes::on_buttonCopy_clicked
 */
void DialogLanguageModes::on_buttonCopy_clicked() {

    if(!updateCurrentItem()) {
        return;
    }

    QModelIndex index = ui.listItems->currentIndex();
    if(index.isValid()) {
        auto ptr = model_->itemFromIndex(index);
        model_->addItem(*ptr);

        QModelIndex newIndex = model_->index(model_->rowCount() - 1, 0);
        ui.listItems->setCurrentIndex(newIndex);
    }

    ui.listItems->scrollTo(ui.listItems->currentIndex());
    updateButtonStates();
}

/**
 * @brief DialogLanguageModes::on_buttonUp_clicked
 */
void DialogLanguageModes::on_buttonUp_clicked() {

    QModelIndex index = ui.listItems->currentIndex();
    if(index.isValid()) {
        model_->moveItemUp(index);
    }

    ui.listItems->scrollTo(ui.listItems->currentIndex());
    updateButtonStates();
}

/**
 * @brief DialogLanguageModes::on_buttonDown_clicked
 */
void DialogLanguageModes::on_buttonDown_clicked() {

    QModelIndex index = ui.listItems->currentIndex();
    if(index.isValid()) {
        model_->moveItemDown(index);
    }

    ui.listItems->scrollTo(ui.listItems->currentIndex());
    updateButtonStates();
}

int DialogLanguageModes::countLanguageModes(const QString &name) const {
    int count = 0;
    for(int i = 0; i < model_->rowCount(); ++i) {
        QModelIndex index = model_->index(i, 0);
        auto style = model_->itemFromIndex(index);
        if(style->name == name) {
            ++count;
        }
    }
    return count;
}

/**
 * @brief DialogLanguageModes::LMHasHighlightPatterns
 * @param name
 * @return
 */
bool DialogLanguageModes::LMHasHighlightPatterns(const QString &name) const {
    if (Highlight::FindPatternSet(name) != nullptr) {
        return true;
    }

    return dialogSyntaxPatterns_ && dialogSyntaxPatterns_->LMHasHighlightPatterns(name);
}

/**
 * @brief DialogLanguageModes::on_buttonDelete_clicked
 */
void DialogLanguageModes::on_buttonDelete_clicked() {

    QModelIndex index = ui.listItems->currentIndex();
    if(index.isValid()) {
        if(auto current = model_->itemFromIndex(index)) {

            const int count = countLanguageModes(current->name);

            // Allow duplicate names to be deleted regardless of dependencies
            if(count <= 1) {
                // don't allow deletion if data will be lost
                if (LMHasHighlightPatterns(current->name)) {
                    QMessageBox::warning(this,
                                         tr("Patterns exist"),
                                         tr("This language mode has syntax highlighting patterns defined. Please delete the patterns first, in Preferences -> Default Settings -> Syntax Highlighting, before proceeding here."));
                    return;
                }

                // don't allow deletion if data will be lost
                if (SmartIndent::LMHasSmartIndentMacros(current->name)) {
                    QMessageBox::warning(this,
                                         tr("Smart Indent Macros exist"),
                                         tr("This language mode has smart indent macros defined. Please delete the macros first, in Preferences -> Default Settings -> Auto Indent -> Program Smart Indent, before proceeding here."));
                    return;
                }
            }

            // OK, passed all of the checks, do the delete
            deleted_ = index;
            model_->deleteItem(index);
            ui.listItems->scrollTo(ui.listItems->currentIndex());
            updateButtonStates();
        }
    }
}


/**
 * @brief DialogLanguageModes::updateCurrentItem
 * @param item
 * @return
 */
bool DialogLanguageModes::updateCurrentItem(const QModelIndex &index) {
    // Get the current contents of the "patterns" dialog fields
    auto dialogFields = readFields(Verbosity::Verbose);
    if(!dialogFields) {
        return false;
    }

    // Get the current contents of the dialog fields
    if(!index.isValid()) {
        return false;
    }

    model_->updateItem(index, *dialogFields);
    return true;
}

/**
 * @brief DialogLanguageModes::updateCurrentItem
 * @return
 */
bool DialogLanguageModes::updateCurrentItem() {
    QModelIndex index = ui.listItems->currentIndex();
    if(index.isValid()) {
        return updateCurrentItem(index);
    }

    return true;
}

/**
 * @brief DialogLanguageModes::validateFields
 * @param mode
 * @return
 */
bool DialogLanguageModes::validateFields(Verbosity verbosity) {
    if(auto ptr = readFields(verbosity)) {
        return true;
    }

    return false;
}
