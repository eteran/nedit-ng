
#include "DialogShellMenu.h"
#include "MainWindow.h"
#include "MenuItem.h"
#include "MenuItemModel.h"
#include "preferences.h"
#include "MenuData.h"
#include "userCmds.h"

#include <QMessageBox>

/**
 * @brief DialogShellMenu::DialogShellMenu
 * @param parent
 * @param f
 */
DialogShellMenu::DialogShellMenu(QWidget *parent, Qt::WindowFlags f) : Dialog(parent, f) {
	ui.setupUi(this);
    ui.editAccelerator->setMaximumSequenceLength(1);

    model_ = new MenuItemModel(this);
    ui.listItems->setModel(model_);

    // Copy the list of menu information to one that the user can freely edit
    for(MenuData &data : ShellMenuData) {
        model_->addItem(*data.item);
    }

    connect(ui.listItems->selectionModel(), &QItemSelectionModel::currentChanged, this, &DialogShellMenu::currentChanged, Qt::QueuedConnection);
    connect(this, &DialogShellMenu::restore, this, &DialogShellMenu::restoreSlot, Qt::QueuedConnection);

    // default to selecting the first item
    if(model_->rowCount() != 0) {
        QModelIndex index = model_->index(0, 0);
        ui.listItems->setCurrentIndex(index);
    }
}

/**
 * @brief DialogWindowBackgroundMenu::restoreSlot
 * @param index
 */
void DialogShellMenu::restoreSlot(const QModelIndex &index) {
    ui.listItems->setCurrentIndex(index);
}

/**
 * @brief DialogShellMenu::updateButtonStates
 */
void DialogShellMenu::updateButtonStates() {
    QModelIndex index = ui.listItems->currentIndex();
    updateButtonStates(index);
}

/**
 * @brief DialogShellMenu::updateButtonStates
 */
void DialogShellMenu::updateButtonStates(const QModelIndex &current) {
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
 * @brief DialogShellMenu::on_buttonNew_clicked
 */
void DialogShellMenu::on_buttonNew_clicked() {

    if(!updateCurrentItem()) {
        return;
    }

    MenuItem item;
    // some sensible defaults...
    item.name  = tr("New Item");
    model_->addItem(item);

    QModelIndex index = model_->index(model_->rowCount() - 1, 0);
    ui.listItems->setCurrentIndex(index);

    ui.listItems->scrollTo(ui.listItems->currentIndex());
    updateButtonStates();
}

/**
 * @brief DialogShellMenu::on_buttonCopy_clicked
 */
void DialogShellMenu::on_buttonCopy_clicked() {

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
 * @brief DialogShellMenu::on_buttonDelete_clicked
 */
void DialogShellMenu::on_buttonDelete_clicked() {

    QModelIndex index = ui.listItems->currentIndex();
    if(index.isValid()) {
        deleted_ = index;
        model_->deleteItem(index);
    }

    ui.listItems->scrollTo(ui.listItems->currentIndex());
    updateButtonStates();
}

/**
 * @brief DialogShellMenu::on_buttonUp_clicked
 */
void DialogShellMenu::on_buttonUp_clicked() {

    QModelIndex index = ui.listItems->currentIndex();
    if(index.isValid()) {
        model_->moveItemUp(index);
    }

    ui.listItems->scrollTo(ui.listItems->currentIndex());
    updateButtonStates();
}

/**
 * @brief DialogShellMenu::on_buttonDown_clicked
 */
void DialogShellMenu::on_buttonDown_clicked() {

    QModelIndex index = ui.listItems->currentIndex();
    if(index.isValid()) {
        model_->moveItemDown(index);
    }

    ui.listItems->scrollTo(ui.listItems->currentIndex());
    updateButtonStates();
}

/**
 * @brief DialogShellMenu::currentChanged
 * @param current
 * @param previous
 */
void DialogShellMenu::currentChanged(const QModelIndex &current, const QModelIndex &previous) {
    static bool canceled = false;

    if (canceled) {
        canceled = false;
        return;
    }

    // if we are actually switching items, check that the previous one was valid
    // so we can optionally cancel
    if(previous.isValid() && previous != deleted_ && !checkCurrent(Mode::Silent)) {
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
            checkCurrent(Mode::Verbose);

            // reselect the old item
            canceled = true;
            Q_EMIT restore(previous);
            return;
        }
    }

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
    if(const auto ptr = model_->itemFromIndex(current)) {

        ui.editName->setText(ptr->name);
        ui.editAccelerator->setKeySequence(ptr->shortcut);
        ui.editCommand->setPlainText(ptr->cmd);

        switch(ptr->input) {
        case FROM_SELECTION:
            ui.radioFromSelection->setChecked(true);
            break;
        case FROM_WINDOW:
            ui.radioFromDocument->setChecked(true);
            break;
        case FROM_EITHER:
            ui.radioFromEither->setChecked(true);
            break;
        case FROM_NONE:
            ui.radioFromNone->setChecked(true);
            break;
        }

        switch(ptr->output) {
        case TO_SAME_WINDOW:
            ui.radioToSameDocument->setChecked(true);
            break;
        case TO_DIALOG:
            ui.radioToDialog->setChecked(true);
            break;
        case TO_NEW_WINDOW:
            ui.radioToNewDocument->setChecked(true);
            break;
        }

        ui.checkReplaceInput->setChecked(ptr->repInput);
        ui.checkSaveBeforeExec->setChecked(ptr->saveFirst);
        ui.checkReloadAfterExec->setChecked(ptr->loadAfter);

    } else {
        ui.editName->setText(QString());
        ui.editAccelerator->clear();
        ui.editCommand->setPlainText(QString());

        ui.radioFromSelection->setChecked(true);
        ui.radioToSameDocument->setChecked(true);

        ui.checkReplaceInput->setChecked(false);
        ui.checkSaveBeforeExec->setChecked(false);
        ui.checkReloadAfterExec->setChecked(false);
    }

    // ensure that the appropriate buttons are enabled
    updateButtonStates(current);
}

/**
 * @brief DialogShellMenu::on_buttonBox_clicked
 * @param button
 */
void DialogShellMenu::on_buttonBox_clicked(QAbstractButton *button) {
	if(ui.buttonBox->standardButton(button) == QDialogButtonBox::Apply) {
		applyDialogChanges();
	}
}

/**
 * @brief DialogShellMenu::on_buttonBox_accepted
 */
void DialogShellMenu::on_buttonBox_accepted() {
	// Read the dialog fields, and update the menus
	if (!applyDialogChanges()) {
		return;
	}

	accept();
}

/*
** Read the name, accelerator, mnemonic, and command fields from the shell or
** macro commands dialog into a newly allocated MenuItem.  Returns a
** pointer to the new MenuItem structure as the function value, or nullptr on
** failure.
*/
std::unique_ptr<MenuItem> DialogShellMenu::readDialogFields(Mode mode) {

	QString nameText = ui.editName->text();
	if (nameText.isEmpty()) {
        if (mode == Mode::Verbose) {
			QMessageBox::warning(this, tr("Menu Entry"), tr("Please specify a name for the menu item"));
		}
		return nullptr;
	}

	if (nameText.indexOf(QLatin1Char(':')) != -1) {
        if (mode == Mode::Verbose) {
			QMessageBox::warning(this, tr("Menu Entry"), tr("Menu item names may not contain colon (:) characters"));
		}
		return nullptr;
	}

	QString cmdText = ui.editCommand->toPlainText();
	if (cmdText.isEmpty()) {
        if (mode == Mode::Verbose) {
			QMessageBox::warning(this, tr("Command to Execute"), tr("Please specify macro command(s) to execute"));
		}
		return nullptr;
	}

    auto f = std::make_unique<MenuItem>();
	f->name = nameText;
	f->cmd  = cmdText;

    QKeySequence shortcut = ui.editAccelerator->keySequence();

    f->shortcut = shortcut;

	if(ui.radioFromSelection->isChecked()) {
		f->input = FROM_SELECTION;
	} else if(ui.radioFromDocument->isChecked()) {
		f->input = FROM_WINDOW;
	} else if(ui.radioFromEither->isChecked()) {
		f->input = FROM_EITHER;
	} else if(ui.radioFromNone->isChecked()) {
		f->input = FROM_NONE;
	}
	
	if(ui.radioToSameDocument->isChecked()) {
		f->output = TO_SAME_WINDOW;
	} else if(ui.radioToDialog->isChecked()) {
		f->output = TO_DIALOG;
	} else if(ui.radioToNewDocument->isChecked()) {
		f->output = TO_NEW_WINDOW;
	}			

	f->repInput  = ui.checkReplaceInput->isChecked();
	f->saveFirst = ui.checkSaveBeforeExec->isChecked();
	f->loadAfter = ui.checkReloadAfterExec->isChecked();
	
	return f;
}


/*
** If "string" is not terminated with a newline character,  return a
** reallocated string which does end in a newline (otherwise, just pass on
** string as function value).  (The macro language requires newline terminators
** for statements, but the text widget doesn't force it like the NEdit text
** buffer does, so this might avoid some confusion.)
*/
QString DialogShellMenu::ensureNewline(const QString &string) {

	if(string.isNull()) {
		return QString();
	}

	if(string.endsWith(QLatin1Char('\n'))) {
		return string;
	}

	return string + QLatin1Char('\n');
}

/**
 * @brief DialogShellMenu::applyDialogChanges
 * @return
 */
bool DialogShellMenu::applyDialogChanges() {

    if(model_->rowCount() != 0) {
        auto dialogFields = readDialogFields(Mode::Verbose);
        if(!dialogFields) {
            return false;
        }

        // Get the current selected item
        QModelIndex index = ui.listItems->currentIndex();
        if(!index.isValid()) {
            return false;
        }

        // update the currently selected item's associated data
        // and make sure it has the text updated as well
        auto ptr = model_->itemFromIndex(index);
        *ptr = *dialogFields;
    }

    ShellMenuData.clear();

    for(int i = 0; i < model_->rowCount(); ++i) {
        QModelIndex index = model_->index(i, 0);
        auto item = model_->itemFromIndex(index);
        ShellMenuData.push_back({ std::make_unique<MenuItem>(*item), nullptr });
    }

    parseMenuItemList(ShellMenuData);

	// Update the menus themselves in all of the NEdit windows
    for(MainWindow *window : MainWindow::allWindows()) {
        window->UpdateUserMenus();
    }

	// Note that preferences have been changed
	MarkPrefsChanged();

	return true;
}

/**
 * @brief DialogShellMenu::on_radioToSameDocument_toggled
 * @param checked
 */
void DialogShellMenu::on_radioToSameDocument_toggled(bool checked) {
	ui.checkReplaceInput->setEnabled(checked);
}

/**
 * @brief DialogShellMenu::updateCurrentItem
 * @param item
 * @return
 */
bool DialogShellMenu::updateCurrentItem(const QModelIndex &index) {
    // Get the current contents of the "patterns" dialog fields
    auto dialogFields = readDialogFields(Mode::Verbose);
    if(!dialogFields) {
        return false;
    }

    // Get the current contents of the dialog fields
    if(!index.isValid()) {
        return false;
    }

    auto ptr = model_->itemFromIndex(index);
    *ptr = *dialogFields;
    return true;
}

/**
 * @brief DialogShellMenu::updateCurrentItem
 * @return
 */
bool DialogShellMenu::updateCurrentItem() {
    QModelIndex index = ui.listItems->currentIndex();
    if(index.isValid()) {
        return updateCurrentItem(index);
    }

    return true;
}


/**
 * @brief DialogShellMenu::checkCurrent
 * @param mode
 * @return
 */
bool DialogShellMenu::checkCurrent(Mode mode) {
    if(auto ptr = readDialogFields(mode)) {
		return true;
	}
	
	return false;
}
