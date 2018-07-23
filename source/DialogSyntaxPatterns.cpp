
#include "DialogSyntaxPatterns.h"
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
#include "SignalBlocker.h"
#include "WindowHighlightData.h"
#include "preferences.h"

#include <QMessageBox>

/**
 * @brief DialogSyntaxPatterns::DialogSyntaxPatterns
 * @param parent
 * @param f
 */
DialogSyntaxPatterns::DialogSyntaxPatterns(MainWindow *window, Qt::WindowFlags f) : Dialog(window, f), window_(window) {
	ui.setupUi(this);

    model_ = new HighlightPatternModel(this);
    ui.listItems->setModel(model_);

	ui.editContextChars->setValidator(new QIntValidator(0, INT_MAX, this));
	ui.editContextLines->setValidator(new QIntValidator(0, INT_MAX, this));

    connect(ui.radioColoring,    &QRadioButton::toggled, this, &DialogSyntaxPatterns::updateLabels);
    connect(ui.radioPass1,       &QRadioButton::toggled, this, &DialogSyntaxPatterns::updateLabels);
    connect(ui.radioPass2,       &QRadioButton::toggled, this, &DialogSyntaxPatterns::updateLabels);
    connect(ui.radioSubPattern,  &QRadioButton::toggled, this, &DialogSyntaxPatterns::updateLabels);
    connect(ui.radioSimpleRegex, &QRadioButton::toggled, this, &DialogSyntaxPatterns::updateLabels);
    connect(ui.radioRangeRegex,  &QRadioButton::toggled, this, &DialogSyntaxPatterns::updateLabels);

	// populate the highlight style combo
    if(auto blocker = no_signals(ui.comboHighlightStyle)) {
        for(const HighlightStyle &style : Highlight::HighlightStyles) {
            ui.comboHighlightStyle->addItem(style.name);
        }
    }

	// populate language mode combo
    if(auto blocker = no_signals(ui.comboLanguageMode)) {
        for(const LanguageMode &lang : Preferences::LanguageModes) {
            ui.comboLanguageMode->addItem(lang.name);
        }
    }

    connect(ui.listItems->selectionModel(), &QItemSelectionModel::currentChanged, this, &DialogSyntaxPatterns::currentChanged, Qt::QueuedConnection);
    connect(this, &DialogSyntaxPatterns::restore, this, &DialogSyntaxPatterns::restoreSlot, Qt::QueuedConnection);
}

/**
 * @brief DialogSyntaxPatterns::restoreSlot
 * @param index
 */
void DialogSyntaxPatterns::restoreSlot(const QModelIndex &index) {
    ui.listItems->setCurrentIndex(index);
}

/**
 * @brief DialogSyntaxPatterns::setLanguageName
 * @param name
 */
void DialogSyntaxPatterns::setLanguageName(const QString &name) {

    static const PatternSet emptyPatternSet;

	// if there is no change, do nothing
	if(previousLanguage_== name) {
		return;
	}

	// if we are setting the language for the first time skip this part
	// otherwise check if any uncommited changes are present
	if(!previousLanguage_.isEmpty()) {

		// Look up the original version of the patterns being edited        
        const PatternSet *activePatternSet = Highlight::FindPatternSet(previousLanguage_);
        if(!activePatternSet) {
            activePatternSet = &emptyPatternSet;
		}

		/* Get the current information displayed by the dialog.  If it's bad,
		   give the user the chance to throw it out or go back and fix it.  If
		   it has changed, give the user the chance to apply discard or cancel. */
        std::unique_ptr<PatternSet> currentPatternSet = getDialogPatternSet();

        if(!currentPatternSet) {
			QMessageBox messageBox(this);
			messageBox.setWindowTitle(tr("Incomplete Language Mode"));
			messageBox.setIcon(QMessageBox::Warning);
			messageBox.setText(tr("Discard incomplete entry for current language mode?"));
			QPushButton *buttonKeep    = messageBox.addButton(tr("Keep"), QMessageBox::RejectRole);
            QPushButton *buttonDiscard = messageBox.addButton(QMessageBox::Discard);
			Q_UNUSED(buttonDiscard);

			messageBox.exec();
			if (messageBox.clickedButton() == buttonKeep) {

				// reselect the old item
                if(auto blocker = no_signals(ui.comboLanguageMode)) {
                    SetLangModeMenu(previousLanguage_);
                }
				return;
			}
        } else if (*activePatternSet != *currentPatternSet) {

            int resp = QMessageBox::warning(
                        this,
                        tr("Language Mode"),
                        tr("Apply changes for language mode %1?").arg(previousLanguage_),
                        QMessageBox::Apply | QMessageBox::Discard | QMessageBox::Cancel);

            switch(resp) {
            case QMessageBox::Cancel:
                // reselect the old item
                if(auto blocker = no_signals(ui.comboLanguageMode)) {
                    SetLangModeMenu(previousLanguage_);
                }
                return;
            case QMessageBox::Apply:
                updatePatternSet();
                break;
            }
		}
	}

    model_->clear();

	// Find the associated pattern set (patSet) to edit
    if(const PatternSet *patSet = Highlight::FindPatternSet(name)) {

		// Copy the list of highlight style information to one that the user can freely edit
        for(const HighlightPattern &pattern: patSet->patterns) {
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
    if(model_->rowCount() != 0) {
        QModelIndex index = model_->index(0, 0);
        no_signals(ui.listItems)->setCurrentIndex(index);
    }

	previousLanguage_ = name;
}

/**
 * @brief DialogSyntaxPatterns::SetLangModeMenu
 * @param name
 */
void DialogSyntaxPatterns::SetLangModeMenu(const QString &name) {

	int index = ui.comboLanguageMode->findText(name, Qt::MatchFixedString);
	if(index != -1) {
		ui.comboLanguageMode->setCurrentIndex(index);
	} else {
		ui.comboLanguageMode->setCurrentIndex(0);
	}
}

/**
 * @brief DialogSyntaxPatterns::setStyleMenu
 * @param name
 */
void DialogSyntaxPatterns::setStyleMenu(const QString &name) {

	int index = ui.comboHighlightStyle->findText(name, Qt::MatchFixedString);
	if(index != -1) {
		ui.comboHighlightStyle->setCurrentIndex(index);
	} else {
		ui.comboHighlightStyle->setCurrentIndex(0);
	}
}

/**
 * @brief DialogSyntaxPatterns::updateLabels
 */
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
        endLbl      = tr("Ending Regular Expression");
        matchSense  = true;
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
    ui.editParentPattern ->setEnabled(parentSense);
    ui.labelRegexEnd     ->setEnabled(endSense);
    ui.editRegexEnd      ->setEnabled(endSense);
    ui.labelRegexError   ->setEnabled(errSense);
    ui.editRegexError    ->setEnabled(errSense);
    ui.radioSimpleRegex  ->setEnabled(matchSense);
    ui.radioRangeRegex   ->setEnabled(matchSense);
    ui.labelRegex        ->setText(startLbl);
    ui.labelRegexEnd     ->setText(endLbl);
    ui.groupMatching     ->setEnabled(matchSense);
}

/**
 * @brief DialogSyntaxPatterns::on_comboLanguageMode_currentIndexChanged
 * @param currentText
 */
void DialogSyntaxPatterns::on_comboLanguageMode_currentIndexChanged(const QString &text) {
    setLanguageName(text);
}

/**
 * @brief DialogSyntaxPatterns::on_buttonLanguageMode_clicked
 */
void DialogSyntaxPatterns::on_buttonLanguageMode_clicked() {
    auto dialog = std::make_unique<DialogLanguageModes>(this, this);
	dialog->exec();
}

/**
 * @brief DialogSyntaxPatterns::on_buttonHighlightStyle_clicked
 */
void DialogSyntaxPatterns::on_buttonHighlightStyle_clicked() {
	QString style = ui.comboHighlightStyle->currentText();
	if(!style.isEmpty()) {
        auto DrawingStyles = std::make_unique<DialogDrawingStyles>(this, Highlight::HighlightStyles, this);
        DrawingStyles->setStyleByName(style);
        DrawingStyles->exec();
	}
}

/**
 * @brief DialogSyntaxPatterns::updateCurrentItem
 * @param item
 * @return
 */
bool DialogSyntaxPatterns::updateCurrentItem(const QModelIndex &index) {
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
 * @brief DialogSyntaxPatterns::updateCurrentItem
 * @return
 */
bool DialogSyntaxPatterns::updateCurrentItem() {
    QModelIndex index = ui.listItems->currentIndex();
    if(index.isValid()) {
        return updateCurrentItem(index);
    }

    return true;
}

/**
 * @brief DialogSyntaxPatterns::on_buttonNew_clicked
 */
void DialogSyntaxPatterns::on_buttonNew_clicked() {

    if(!updateCurrentItem()) {
        return;
    }

    HighlightPattern style;
    // some sensible defaults...
    style.name  = tr("New Item");
    model_->addItem(style);

    QModelIndex index = model_->index(model_->rowCount() - 1, 0);
    ui.listItems->setCurrentIndex(index);

    ui.listItems->scrollTo(ui.listItems->currentIndex());
    updateButtonStates();
}

/**
 * @brief DialogSyntaxPatterns::on_buttonDelete_clicked
 */
void DialogSyntaxPatterns::on_buttonDelete_clicked() {
    QModelIndex index = ui.listItems->currentIndex();
    if(index.isValid()) {
        deleted_ = index;
        model_->deleteItem(index);
    }

    ui.listItems->scrollTo(ui.listItems->currentIndex());
    updateButtonStates();
}

/**
 * @brief DialogSyntaxPatterns::on_buttonCopy_clicked
 */
void DialogSyntaxPatterns::on_buttonCopy_clicked() {

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
 * @brief DialogSyntaxPatterns::on_buttonUp_clicked
 */
void DialogSyntaxPatterns::on_buttonUp_clicked() {
    QModelIndex index = ui.listItems->currentIndex();
    if(index.isValid()) {
        model_->moveItemUp(index);
    }

    ui.listItems->scrollTo(ui.listItems->currentIndex());
    updateButtonStates();
}

/**
 * @brief DialogSyntaxPatterns::on_buttonDown_clicked
 */
void DialogSyntaxPatterns::on_buttonDown_clicked() {
    QModelIndex index = ui.listItems->currentIndex();
    if(index.isValid()) {
        model_->moveItemDown(index);
    }

    ui.listItems->scrollTo(ui.listItems->currentIndex());
    updateButtonStates();
}

/**
 * @brief DialogSyntaxPatterns::on_buttonOK_clicked
 */
void DialogSyntaxPatterns::on_buttonOK_clicked() {
	// change the patterns
	if (!updatePatternSet()) {
		return;
	}

	accept();
}

/**
 * @brief DialogSyntaxPatterns::on_buttonApply_clicked
 */
void DialogSyntaxPatterns::on_buttonApply_clicked() {
	updatePatternSet();
}

/**
 * @brief DialogSyntaxPatterns::on_buttonCheck_clicked
 */
void DialogSyntaxPatterns::on_buttonCheck_clicked() {
	if (checkHighlightDialogData()) {
        QMessageBox::information(this,
                                 tr("Pattern compiled"),
                                 tr("Patterns compiled without error"));
	}
}

/**
 * @brief DialogSyntaxPatterns::on_buttonDeletePattern_clicked
 */
void DialogSyntaxPatterns::on_buttonDeletePattern_clicked() {

	const QString languageMode = ui.comboLanguageMode->currentText();

    int resp = QMessageBox::warning(
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

    // NOTE(eteran): notify the settings system that things have changed
    Preferences::MarkPrefsChanged();
}

/**
 * @brief DialogSyntaxPatterns::on_buttonRestore_clicked
 */
void DialogSyntaxPatterns::on_buttonRestore_clicked() {

	const QString languageMode = ui.comboLanguageMode->currentText();

    std::unique_ptr<PatternSet> defaultPatSet = Highlight::readDefaultPatternSet(languageMode);
	if(!defaultPatSet) {
        QMessageBox::warning(
                    this,
                    tr("No Default Pattern"),
                    tr("There is no default pattern set for language mode %1").arg(languageMode));
		return;
	}

    int resp = QMessageBox::warning(
                this,
                tr("Discard Changes"),
                tr("Are you sure you want to discard all changes to syntax highlighting patterns for language mode %1?").arg(languageMode),
                QMessageBox::Discard | QMessageBox::Cancel);

	if (resp == QMessageBox::Cancel) {
		return;
	}

	// if a stored version of the pattern set exists, replace it, if it doesn't, add a new one
    auto it = std::find_if(Highlight::PatternSets.begin(), Highlight::PatternSets.end(), [languageMode](const PatternSet &pattern) {
		return pattern.languageMode == languageMode;
	});

    if (it != Highlight::PatternSets.end()) {
        *it = *defaultPatSet;
	} else {
        Highlight::PatternSets.push_back(*defaultPatSet);
	}

    model_->clear();

	// Update the dialog
	for(HighlightPattern &pattern: defaultPatSet->patterns) {
        model_->addItem(pattern);
	}

	// Fill in the dialog information for the selected language mode
	ui.editContextLines->setText(QString::number(defaultPatSet->lineContext));
	ui.editContextChars->setText(QString::number(defaultPatSet->charContext));

    // default to selecting the first item
    if(model_->rowCount() != 0) {
        QModelIndex index = model_->index(0, 0);
        ui.listItems->setCurrentIndex(index);
    }
}

/**
 * @brief DialogSyntaxPatterns::on_buttonHelp_clicked
 */
void DialogSyntaxPatterns::on_buttonHelp_clicked() {
    Help::displayTopic(this, Help::Topic::Syntax);
}

/**
 * @brief DialogSyntaxPatterns::updateButtonStates
 */
void DialogSyntaxPatterns::updateButtonStates(const QModelIndex &current) {
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
 * @brief DialogSyntaxPatterns::updateButtonStates
 */
void DialogSyntaxPatterns::updateButtonStates() {
    QModelIndex index = ui.listItems->currentIndex();
    updateButtonStates(index);
}

/**
 * @brief PreferenceList::currentChanged
 * @param current
 * @param previous
 */
void DialogSyntaxPatterns::currentChanged(const QModelIndex &current, const QModelIndex &previous) {
    static bool canceled = false;

    if (canceled) {
        canceled = false;
        return;
    }

    // if we are actually switching items, check that the previous one was valid
    // so we can optionally cancel
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

	// this is only safe if we aren't moving due to a delete operation
    if(previous.isValid() && previous != deleted_) {
        if(!updateCurrentItem(previous)) {
            // reselect the old item
            canceled = true;
            Q_EMIT restore(previous);
            return;
        }
    }

    // previous was OK, so let's update the contents of the dialog
    if(const auto pattern = model_->itemFromIndex(current)) {

        bool isSubpat    = !pattern->subPatternOf.isNull();
        bool isDeferred  = pattern->flags & DEFER_PARSING;
        bool isColorOnly = pattern->flags & COLOR_ONLY;
        bool isRange     = (!pattern->endRE.isNull());

        ui.editPatternName->setText(pattern->name);
        ui.editParentPattern->setText(!pattern->subPatternOf.isNull() ? pattern->subPatternOf : QString());
        ui.editRegex->setPlainText(pattern->startRE);
        ui.editRegexEnd->setText(pattern->endRE);
        ui.editRegexError->setText(pattern->errorRE);

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
        setStyleMenu(QLatin1String("Plain"));
    }

    updateButtonStates(current);
    updateLabels();
}

/*
** If a syntax highlighting dialog is up, ask to have the option menu for
** chosing language mode updated (via a call to CreateLanguageModeMenu)
*/
void DialogSyntaxPatterns::UpdateLanguageModeMenu() {

    if(auto blocker = no_signals(ui.comboLanguageMode)) {

        const QString language = ui.comboLanguageMode->currentText();
        ui.comboLanguageMode->clear();

        for(const LanguageMode &lang : Preferences::LanguageModes) {
            ui.comboLanguageMode->addItem(lang.name);
        }

        SetLangModeMenu(language);
    }
}

/**
 * @brief DialogSyntaxPatterns::updateHighlightStyleMenu
 */
void DialogSyntaxPatterns::updateHighlightStyleMenu() {

    if(auto blocker = no_signals(ui.comboHighlightStyle)) {

        QString pattern = ui.comboHighlightStyle->currentText();
        ui.comboHighlightStyle->clear();

        for(const HighlightStyle &style : Highlight::HighlightStyles) {
            ui.comboHighlightStyle->addItem(style.name);
        }

        setStyleMenu(pattern);
    }
}

/**
 * @brief DialogSyntaxPatterns::updatePatternSet
 * @return
 */
bool DialogSyntaxPatterns::updatePatternSet() {

	// Make sure the patterns are valid and compile
	if (!checkHighlightDialogData()) {
		return false;
	}

	// Get the current data
	std::unique_ptr<PatternSet> patternSet = getDialogPatternSet();
	if(!patternSet) {
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
		*it = *patternSet;
	}

	// Find windows that are currently using this pattern set and re-do the highlighting
    for(DocumentWidget *document : DocumentWidget::allDocuments()) {
		if (!patternSet->patterns.empty()) {
			if (document->GetLanguageMode() != PLAIN_LANGUAGE_MODE && (Preferences::LanguageModeName(document->GetLanguageMode()) == patternSet->languageMode)) {
				/*  The user worked on the current document's language mode, so
				    we have to make some changes immediately. For inactive
				    modes, the changes will be activated on activation.  */
				if (oldNum == 0) {
					/*  Highlighting (including menu entry) was deactivated in
					    this function or in preferences.c::reapplyLanguageMode()
					    if the old set had no patterns, so reactivate menu entry. */
                    if (document->IsTopDocument()) {
                        MainWindow::fromDocument(document)->ui.action_Highlight_Syntax->setEnabled(true);
					}

					//  Reactivate highlighting if it's default
                    document->highlightSyntax_ = Preferences::GetPrefHighlightSyntax();
				}

                if (document->highlightSyntax_) {
                    document->StopHighlightingEx();

                    if (document->IsTopDocument()) {
                        MainWindow::fromDocument(document)->ui.action_Highlight_Syntax->setEnabled(true);
                        no_signals(MainWindow::fromDocument(document)->ui.action_Highlight_Syntax)->setChecked(true);
					}
                    document->StartHighlightingEx(/*warn=*/true);
				}
			}
		} else {
			/*  No pattern in pattern set. This will probably not happen much,
			    but you never know.  */
            document->StopHighlightingEx();
            document->highlightSyntax_ = false;

            if (document->IsTopDocument()) {
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
 * @brief DialogSyntaxPatterns::checkHighlightDialogData
 * @return
 */
bool DialogSyntaxPatterns::checkHighlightDialogData() {
	// Get the pattern information from the dialog
	std::unique_ptr<PatternSet> patternSet = getDialogPatternSet();
	if(!patternSet) {
		return false;
	}

	// Compile the patterns
	return patternSet->patterns.empty() ? true : TestHighlightPatterns(patternSet);
}

/*
** Get the current information that the user has entered in the syntax
** highlighting dialog.  Return nullptr if the data is currently invalid
*/
std::unique_ptr<PatternSet> DialogSyntaxPatterns::getDialogPatternSet() {

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

	// ensure that the list has the current item complete filled out
	updateCurrentItem();

	/* Allocate a new pattern set structure and copy the fields read from the
	   dialog, including the modified pattern list into it */
	auto patternSet = std::make_unique<PatternSet>();
	patternSet->languageMode = ui.comboLanguageMode->currentText();
	patternSet->lineContext  = lineContext;
	patternSet->charContext  = charContext;

	for (int i = 0; i < model_->rowCount(); ++i) {
		QModelIndex index = model_->index(i, 0);
		if(auto pattern = model_->itemFromIndex(index)) {
			patternSet->patterns.push_back(*pattern);
        }
	}

	return patternSet;
}

/*
** Read the pattern fields of the highlight dialog, and produce an allocated
** HighlightPattern structure reflecting the contents, or pop up dialogs
** telling the user what's wrong (Passing "silent" as true, suppresses these
** dialogs).  Returns nullptr on error.
*/
boost::optional<HighlightPattern> DialogSyntaxPatterns::readFields(Verbosity verbosity) {

	HighlightPattern pat;

	// read the type buttons
	int colorOnly = ui.radioColoring->isChecked();
	if (ui.radioPass2->isChecked()) {
		pat.flags |= DEFER_PARSING;
	} else if (colorOnly) {
		pat.flags = COLOR_ONLY;
	}

	// read the name field
	QString name = ui.editPatternName->text().simplified();
	if (name.isNull()) {
		return boost::none;
	}

	pat.name = name;
	if (pat.name.isEmpty()) {
        if (verbosity == Verbosity::Verbose) {
			QMessageBox::warning(this, tr("Pattern Name"), tr("Please specify a name for the pattern"));
		}
		return boost::none;
	}

	// read the startRE field
	pat.startRE = ui.editRegex->toPlainText();
	if (pat.startRE.isEmpty()) {
        if (verbosity == Verbosity::Verbose) {
			QMessageBox::warning(this, tr("Matching Regex"), tr("Please specify a regular expression to match"));
		}
		return boost::none;
	}


	/* Make sure coloring patterns contain only sub-expression references
	   and put it in replacement regular-expression form */
	if (colorOnly) {

		QString outStr;
		outStr.reserve(pat.startRE.size());

		auto outPtr = std::back_inserter(outStr);

		Q_FOREACH(QChar ch, pat.startRE) {
			if (ch != QLatin1Char(' ') && ch != QLatin1Char('\t')) {
				*outPtr++ = ch;
			}
		}

		pat.startRE = outStr;

		static const QRegularExpression re(QLatin1String(R"([^&\\123456789 \t])"));
		if (pat.startRE.contains(re) || (pat.startRE[0] != QLatin1Char('\\') && pat.startRE[0] != QLatin1Char('&')) || pat.startRE.contains(QLatin1String("\\\\"))) {

            if (verbosity == Verbosity::Verbose) {
                QMessageBox::warning(
                            this,
                            tr("Pattern Error"),
                            tr("The expression field in patterns which specify highlighting for a parent, must contain only sub-expression references in regular expression replacement form (&\\1\\2 etc.).  See Help -> Regular Expressions and Help -> Syntax Highlighting for more information"));
			}
			return boost::none;
		}
	}

	// read the parent field
	if (ui.radioSubPattern->isChecked() || colorOnly) {
		QString parent = ui.editParentPattern->text().simplified();
		if (parent.isEmpty()) {
            if (verbosity == Verbosity::Verbose) {
				QMessageBox::warning(this, tr("Specify Parent Pattern"), tr("Please specify a parent pattern"));
			}
			return boost::none;
		}

		if(!parent.isNull()) {
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
			return boost::none;
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
 * @brief DialogSyntaxPatterns::validateFields
 * @param mode
 * @return
 */
bool DialogSyntaxPatterns::validateFields(Verbosity verbosity) {
	if(readFields(verbosity)) {
		return true;
	}

	return false;
}

/*
** Do a test compile of patterns in "patSet" and report problems to the
** user via dialog.  Returns true if patterns are ok.
**
** This is somewhat kludgy in that it uses createHighlightData, which
** requires a window to find the fonts to use, and just uses a random
** window from the window list.  Since the window is used to get the
** dialog parent as well, these dialogs will appear in odd places on the screen.
*/
bool DialogSyntaxPatterns::TestHighlightPatterns(const std::unique_ptr<PatternSet> &patSet) {

	/* Compile the patterns (passing a random window as a source for fonts, and
	   parent for dialogs, since we really don't care what fonts are used) */
	if(PatternSet *const patternSet = patSet.get()) {
		for(DocumentWidget *document : DocumentWidget::allDocuments()) {
			if(std::shared_ptr<WindowHighlightData> highlightData = document->createHighlightDataEx(patternSet)) {
				return true;
			}
		}
	}

	return false;
}

/**
 * @brief DialogSyntaxPatterns::RenameHighlightPattern
 * @param oldName
 * @param newName
 */
void DialogSyntaxPatterns::RenameHighlightPattern(const QString &oldName, const QString &newName) {
	if (ui.comboLanguageMode->currentText() == oldName) {
        SetLangModeMenu(newName);
	}
}

/**
 * @brief DialogSyntaxPatterns::LMHasHighlightPatterns
 * @param languageMode
 * @return
 */
bool DialogSyntaxPatterns::LMHasHighlightPatterns(const QString &languageMode) {
    return languageMode == ui.comboLanguageMode->currentText() && model_->rowCount() != 0;
}
