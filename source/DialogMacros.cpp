
#include "DialogMacros.h"
#include "CommandRecorder.h"
#include "interpret.h"
#include "macro.h"
#include "MainWindow.h"
#include "MenuData.h"
#include "MenuItem.h"
#include "preferences.h"
#include "MenuItemModel.h"
#include "userCmds.h"

#include <QMessageBox>

/**
 * @brief DialogMacros::DialogMacros
 * @param parent
 * @param f
 */
DialogMacros::DialogMacros(QWidget *parent, Qt::WindowFlags f) : Dialog(parent, f) {
	ui.setupUi(this);
    ui.editAccelerator->setMaximumSequenceLength(1);

    model_ = new MenuItemModel(this);
    ui.listItems->setModel(model_);

    // Copy the list of menu information to one that the user can freely edit
    for(MenuData &data : MacroMenuData) {
        model_->addItem(data.item);
    }

    connect(ui.listItems->selectionModel(), &QItemSelectionModel::currentChanged, this, &DialogMacros::currentChanged, Qt::QueuedConnection);
    connect(this, &DialogMacros::restore, this, &DialogMacros::restoreSlot, Qt::QueuedConnection);

    // default to selecting the first item
    if(model_->rowCount() != 0) {
        QModelIndex index = model_->index(0, 0);
        ui.listItems->setCurrentIndex(index);
    }
}

/**
 * @brief DialogMacros::restoreSlot
 * @param index
 */
void DialogMacros::restoreSlot(const QModelIndex &index) {
    ui.listItems->setCurrentIndex(index);
}

/**
 * @brief DialogMacros::updateButtonStates
 */
void DialogMacros::updateButtonStates() {
    QModelIndex index = ui.listItems->currentIndex();
    updateButtonStates(index);
}

/**
 * @brief DialogMacros::updateButtonStates
 */
void DialogMacros::updateButtonStates(const QModelIndex &current) {
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
 * @brief DialogMacros::on_buttonNew_clicked
 */
void DialogMacros::on_buttonNew_clicked() {

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
 * @brief DialogMacros::on_buttonCopy_clicked
 */
void DialogMacros::on_buttonCopy_clicked() {

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
 * @brief DialogMacros::on_buttonDelete_clicked
 */
void DialogMacros::on_buttonDelete_clicked() {

    QModelIndex index = ui.listItems->currentIndex();
    if(index.isValid()) {
        deleted_ = index;
        model_->deleteItem(index);
    }

    ui.listItems->scrollTo(ui.listItems->currentIndex());
    updateButtonStates();
}

/**
 * @brief DialogMacros::on_buttonPasteLRMacro_clicked
 */
void DialogMacros::on_buttonPasteLRMacro_clicked() {

    QString replayMacro = CommandRecorder::getInstance()->replayMacro;
    if (replayMacro.isEmpty()) {
		return;
	}

    ui.editMacro->insertPlainText(replayMacro);
}

/**
 * @brief DialogMacros::on_buttonUp_clicked
 */
void DialogMacros::on_buttonUp_clicked() {

    QModelIndex index = ui.listItems->currentIndex();
    if(index.isValid()) {
        model_->moveItemUp(index);
    }

    ui.listItems->scrollTo(ui.listItems->currentIndex());
    updateButtonStates();
}

/**
 * @brief DialogMacros::on_buttonDown_clicked
 */
void DialogMacros::on_buttonDown_clicked() {

    QModelIndex index = ui.listItems->currentIndex();
    if(index.isValid()) {
        model_->moveItemDown(index);
    }

    ui.listItems->scrollTo(ui.listItems->currentIndex());
    updateButtonStates();
}

/**
 * @brief DialogMacros::currentChanged
 * @param current
 * @param previous
 */
void DialogMacros::currentChanged(const QModelIndex &current, const QModelIndex &previous) {
    static bool canceled = false;

    if (canceled) {
        canceled = false;
        return;
    }

    // if we are actually switching items, check that the previous one was valid
    // so we can optionally cancel
    if(previous.isValid() && previous != deleted_ && !checkMacro(Mode::Silent)) {
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
            checkMacro(Mode::Verbose);

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
        ui.checkRequiresSelection->setChecked(ptr->input == FROM_SELECTION);
        ui.editMacro->setPlainText(ptr->cmd);
    } else {
        ui.editName->setText(QString());
        ui.editAccelerator->clear();
        ui.checkRequiresSelection->setChecked(false);
        ui.editMacro->setPlainText(QString());
    }

    // ensure that the appropriate buttons are enabled
    updateButtonStates(current);
}

/**
 * @brief DialogMacros::on_buttonCheck_clicked
 */
void DialogMacros::on_buttonCheck_clicked() {
    if (checkMacro(Mode::Verbose)) {
        QMessageBox::information(this,
                                 tr("Macro"),
                                 tr("Macro compiled without error"));
	}
}

/**
 * @brief DialogMacros::on_buttonApply_clicked
 */
void DialogMacros::on_buttonApply_clicked() {
	applyDialogChanges();
}

/**
 * @brief DialogMacros::on_buttonOK_clicked
 */
void DialogMacros::on_buttonOK_clicked() {

	// Read the dialog fields, and update the menus
	if (!applyDialogChanges()) {
		return;
	}

	accept();
}

/**
 * @brief DialogMacros::checkMacro
 * @param mode
 * @return
 */
bool DialogMacros::checkMacro(Mode mode) {

    auto f = readDialogFields(mode);
	if(!f) {
		return false;
	}

    if (!checkMacroText(f->cmd, mode)) {
		return false;
	}

	return true;
}


/*
** Read the name, accelerator, mnemonic, and command fields from the shell or
** macro commands dialog into a newly allocated MenuItem.  Returns a
** pointer to the new MenuItem structure as the function value, or nullptr on
** failure.
*/
std::unique_ptr<MenuItem> DialogMacros::readDialogFields(Mode mode) {

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

	QString cmdText = ui.editMacro->toPlainText();
	if (cmdText.isEmpty()) {
        if (mode == Mode::Verbose) {
			QMessageBox::warning(this, tr("Command to Execute"), tr("Please specify macro command(s) to execute"));
		}
		return nullptr;
	}

	cmdText = ensureNewline(cmdText);
    if (!checkMacroText(cmdText, mode)) {
		return nullptr;
	}

    auto f = std::make_unique<MenuItem>();
	f->name = nameText;
	f->cmd  = cmdText;

    QKeySequence shortcut = ui.editAccelerator->keySequence();

	f->input     = ui.checkRequiresSelection->isChecked() ? FROM_SELECTION : FROM_NONE;
	f->output    = TO_SAME_WINDOW;
	f->repInput  = false;
	f->saveFirst = false;
	f->loadAfter = false;
    f->shortcut  = shortcut;

	return f;
}

/**
 * @brief DialogMacros::checkMacroText
 * @param macro
 * @param mode
 * @return
 */
bool DialogMacros::checkMacroText(const QString &macro, Mode mode) {

	QString errMsg;
	int stoppedAt;

    Program *prog = ParseMacroEx(macro, &errMsg, &stoppedAt);
	if(!prog) {
        if(mode == Mode::Verbose) {
			ParseErrorEx(this, macro, stoppedAt, tr("macro"), errMsg);
		}
		QTextCursor cursor = ui.editMacro->textCursor();
		cursor.setPosition(stoppedAt);
		ui.editMacro->setTextCursor(cursor);
		ui.editMacro->setFocus();
		return false;
	}
	FreeProgram(prog);

	if(stoppedAt != macro.size()) {
        if(mode == Mode::Verbose) {
			ParseErrorEx(this, macro, stoppedAt, tr("macro"), tr("syntax error"));
		}
		QTextCursor cursor = ui.editMacro->textCursor();
		cursor.setPosition(stoppedAt);
		ui.editMacro->setTextCursor(cursor);
		ui.editMacro->setFocus();
	}

	return true;
}

/*
** If "string" is not terminated with a newline character,  return a
** reallocated string which does end in a newline (otherwise, just pass on
** string as function value).  (The macro language requires newline terminators
** for statements, but the text widget doesn't force it like the NEdit text
** buffer does, so this might avoid some confusion.)
*/
QString DialogMacros::ensureNewline(const QString &string) {

	if(string.isNull()) {
		return QString();
	}

	if(string.endsWith(QLatin1Char('\n'))) {
		return string;
	}

	return string + QLatin1Char('\n');
}

/**
 * @brief DialogMacros::applyDialogChanges
 * @return
 */
bool DialogMacros::applyDialogChanges() {

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

    std::vector<MenuData> newItems;

    for(int i = 0; i < model_->rowCount(); ++i) {
        QModelIndex index = model_->index(i, 0);
        auto item = model_->itemFromIndex(index);
        newItems.push_back({ *item, nullptr });
    }

    MacroMenuData = newItems;

    parseMenuItemList(MacroMenuData);

    // Update the menus themselves in all of the NEdit windows
    for(MainWindow *window : MainWindow::allWindows()) {
        window->UpdateUserMenus();
    }
	
	// Note that preferences have been changed
	MarkPrefsChanged();
	return true;
}

void DialogMacros::setPasteReplayEnabled(bool enabled) {
	ui.buttonPasteLRMacro->setEnabled(enabled);
}

/**
 * @brief DialogMacros::updateCurrentItem
 * @param item
 * @return
 */
bool DialogMacros::updateCurrentItem(const QModelIndex &index) {
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
 * @brief DialogMacros::updateCurrentItem
 * @return
 */
bool DialogMacros::updateCurrentItem() {
    QModelIndex index = ui.listItems->currentIndex();
    if(index.isValid()) {
        return updateCurrentItem(index);
    }

    return true;
}
