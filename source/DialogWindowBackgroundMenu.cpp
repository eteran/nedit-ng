
#include "DialogWindowBackgroundMenu.h"
#include "CommandRecorder.h"
#include "interpret.h"
#include "parse.h"
#include "MainWindow.h"
#include "MenuData.h"
#include "MenuItem.h"
#include "MenuItemModel.h"
#include "preferences.h"
#include "SignalBlocker.h"
#include "userCmds.h"

#include <QMessageBox>

namespace {

/*
** If "string" is not terminated with a newline character,  return a
** reallocated string which does end in a newline (otherwise, just pass on
** string as function value).  (The macro language requires newline terminators
** for statements, but the text widget doesn't force it like the NEdit text
** buffer does, so this might avoid some confusion.)
*/
QString ensureNewline(const QString &string) {

    if(string.isNull()) {
        return QString();
    }

    if(string.endsWith(QLatin1Char('\n'))) {
        return string;
    }

    return string + QLatin1Char('\n');
}

}

/**
 * @brief DialogWindowBackgroundMenu::DialogWindowBackgroundMenu
 * @param parent
 * @param f
 */
DialogWindowBackgroundMenu::DialogWindowBackgroundMenu(QWidget *parent, Qt::WindowFlags f) : Dialog(parent, f) {
	ui.setupUi(this);
    ui.editAccelerator->setMaximumSequenceLength(1);

    ui.buttonPasteLRMacro->setEnabled(!CommandRecorder::getInstance().replayMacro.isEmpty());

    model_ = new MenuItemModel(this);
    ui.listItems->setModel(model_);

    // Copy the list of menu information to one that the user can freely edit
    for(MenuData &data : BGMenuData) {
        model_->addItem(data.item);
    }

    connect(ui.listItems->selectionModel(), &QItemSelectionModel::currentChanged, this, &DialogWindowBackgroundMenu::currentChanged, Qt::QueuedConnection);
    connect(this, &DialogWindowBackgroundMenu::restore, this, &DialogWindowBackgroundMenu::restoreSlot, Qt::QueuedConnection);

    // default to selecting the first item
    if(model_->rowCount() != 0) {
        QModelIndex index = model_->index(0, 0);
        ui.listItems->setCurrentIndex(index);
    }
}

/**
 * @brief DialogWindowBackgroundMenu::on_buttonNew_clicked
 */
void DialogWindowBackgroundMenu::on_buttonNew_clicked() {

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
 * @brief DialogWindowBackgroundMenu::on_buttonCopy_clicked
 */
void DialogWindowBackgroundMenu::on_buttonCopy_clicked() {

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
 * @brief DialogWindowBackgroundMenu::on_buttonDelete_clicked
 */
void DialogWindowBackgroundMenu::on_buttonDelete_clicked() {

    QModelIndex index = ui.listItems->currentIndex();
    if(index.isValid()) {
        deleted_ = index;
        model_->deleteItem(index);
    }

    ui.listItems->scrollTo(ui.listItems->currentIndex());
    updateButtonStates();
}

/**
 * @brief DialogWindowBackgroundMenu::on_buttonPasteLRMacro_clicked
 */
void DialogWindowBackgroundMenu::on_buttonPasteLRMacro_clicked() {

    QString replayMacro = CommandRecorder::getInstance().replayMacro;

    if (replayMacro.isEmpty()) {
		return;
	}

    ui.editMacro->insertPlainText(replayMacro);
}

/**
 * @brief DialogWindowBackgroundMenu::on_buttonUp_clicked
 */
void DialogWindowBackgroundMenu::on_buttonUp_clicked() {

    QModelIndex index = ui.listItems->currentIndex();
    if(index.isValid()) {
        model_->moveItemUp(index);
    }

    ui.listItems->scrollTo(ui.listItems->currentIndex());
    updateButtonStates();
}

/**
 * @brief DialogWindowBackgroundMenu::on_buttonDown_clicked
 */
void DialogWindowBackgroundMenu::on_buttonDown_clicked() {

    QModelIndex index = ui.listItems->currentIndex();
    if(index.isValid()) {
        model_->moveItemDown(index);
    }

    ui.listItems->scrollTo(ui.listItems->currentIndex());
    updateButtonStates();
}

/**
 * @brief DialogWindowBackgroundMenu::updateButtonStates
 */
void DialogWindowBackgroundMenu::updateButtonStates() {
    QModelIndex index = ui.listItems->currentIndex();
    updateButtonStates(index);
}

/**
 * @brief DialogWindowBackgroundMenu::updateButtonStates
 */
void DialogWindowBackgroundMenu::updateButtonStates(const QModelIndex &current) {
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
 * @brief DialogWindowBackgroundMenu::restoreSlot
 * @param index
 */
void DialogWindowBackgroundMenu::restoreSlot(const QModelIndex &index) {
    ui.listItems->setCurrentIndex(index);
}

/**
 * @brief DialogWindowBackgroundMenu::currentChanged
 * @param current
 * @param previous
 */
void DialogWindowBackgroundMenu::currentChanged(const QModelIndex &current, const QModelIndex &previous) {
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
        ui.editAccelerator->setKeySequence(QKeySequence());
        ui.checkRequiresSelection->setChecked(false);
        ui.editMacro->setPlainText(QString());
    }

    // ensure that the appropriate buttons are enabled
    updateButtonStates(current);
}

/**
 * @brief DialogWindowBackgroundMenu::on_buttonCheck_clicked
 */
void DialogWindowBackgroundMenu::on_buttonCheck_clicked() {
    if (validateFields(Verbosity::Verbose)) {
        QMessageBox::information(this,
                                 tr("Macro"),
                                 tr("Macro compiled without error"));
	}
}

/**
 * @brief DialogWindowBackgroundMenu::on_buttonApply_clicked
 */
void DialogWindowBackgroundMenu::on_buttonApply_clicked() {
	applyDialogChanges();
}

/**
 * @brief DialogWindowBackgroundMenu::on_buttonOK_clicked
 */
void DialogWindowBackgroundMenu::on_buttonOK_clicked() {

	// Read the dialog fields, and update the menus
	if (!applyDialogChanges()) {
		return;
	}

	accept();
}

/**
 * @brief DialogWindowBackgroundMenu::validateFields
 * @param silent
 * @return
 */
bool DialogWindowBackgroundMenu::validateFields(Verbosity verbosity) {

    if(auto f = readFields(verbosity)) {
        return checkMacroText(f->cmd, verbosity);
    }

    return false;
}

/*
** Read the name, accelerator, mnemonic, and command fields from the shell or
** macro commands dialog into a newly allocated MenuItem.  Returns a
** pointer to the new MenuItem structure as the function value, or nullptr on
** failure.
*/
std::unique_ptr<MenuItem> DialogWindowBackgroundMenu::readFields(Verbosity verbosity) {

	QString nameText = ui.editName->text();

	if (nameText.isEmpty()) {
        if (verbosity == Verbosity::Verbose) {
			QMessageBox::warning(this, tr("Menu Entry"), tr("Please specify a name for the menu item"));
		}
		return nullptr;
	}


	if (nameText.indexOf(QLatin1Char(':')) != -1) {
        if (verbosity == Verbosity::Verbose) {
			QMessageBox::warning(this, tr("Menu Entry"), tr("Menu item names may not contain colon (:) characters"));
		}
		return nullptr;
	}

	QString cmdText = ui.editMacro->toPlainText();
	if (cmdText.isEmpty()) {
        if (verbosity == Verbosity::Verbose) {
			QMessageBox::warning(this, tr("Command to Execute"), tr("Please specify macro command(s) to execute"));
		}
		return nullptr;
	}

	cmdText = ensureNewline(cmdText);
    if (!checkMacroText(cmdText, verbosity)) {
		return nullptr;
	}

    auto f = std::make_unique<MenuItem>();
    f->name      = nameText;
    f->cmd       = cmdText;
	f->input     = ui.checkRequiresSelection->isChecked() ? FROM_SELECTION : FROM_NONE;
	f->output    = TO_SAME_WINDOW;
	f->repInput  = false;
	f->saveFirst = false;
	f->loadAfter = false;
    f->shortcut  = ui.editAccelerator->keySequence();

	return f;
}

/**
 * @brief DialogWindowBackgroundMenu::checkMacroText
 * @param macro
 * @param silent
 * @return
 */
bool DialogWindowBackgroundMenu::checkMacroText(const QString &macro, Verbosity verbosity) {

	QString errMsg;
	int stoppedAt;

    Program *prog = ParseMacroEx(macro, &errMsg, &stoppedAt);
	if(!prog) {
        if(verbosity == Verbosity::Verbose) {
			ParseErrorEx(this, macro, stoppedAt, tr("macro"), errMsg);
		}
		QTextCursor cursor = ui.editMacro->textCursor();
		cursor.setPosition(stoppedAt);
		ui.editMacro->setTextCursor(cursor);
		ui.editMacro->setFocus();
		return false;
	}
    delete prog;

	if(stoppedAt != macro.size()) {
        if(verbosity == Verbosity::Verbose) {
			ParseErrorEx(this, macro, stoppedAt, tr("macro"), tr("syntax error"));
		}
		QTextCursor cursor = ui.editMacro->textCursor();
		cursor.setPosition(stoppedAt);
		ui.editMacro->setTextCursor(cursor);
		ui.editMacro->setFocus();
	}

	return true;
}

/**
 * @brief DialogWindowBackgroundMenu::applyDialogChanges
 * @return
 */
bool DialogWindowBackgroundMenu::applyDialogChanges() {

    if(model_->rowCount() != 0) {
        auto dialogFields = readFields(Verbosity::Verbose);
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
        model_->updateItem(index, *dialogFields);
    }

    std::vector<MenuData> newItems;

    for(int i = 0; i < model_->rowCount(); ++i) {
        QModelIndex index = model_->index(i, 0);
        auto item = model_->itemFromIndex(index);
        newItems.push_back({ *item, nullptr });
    }

    BGMenuData = newItems;

    parseMenuItemList(BGMenuData);

    // Update the menus themselves in all of the NEdit windows
    for(MainWindow *window : MainWindow::allWindows()) {
        window->UpdateUserMenus();
    }

    // Note that preferences have been changed
    MarkPrefsChanged();
    return true;
}

/**
 * @brief DialogWindowBackgroundMenu::setPasteReplayEnabled
 * @param enabled
 */
void DialogWindowBackgroundMenu::setPasteReplayEnabled(bool enabled) {
	ui.buttonPasteLRMacro->setEnabled(enabled);
}

/**
 * @brief DialogWindowBackgroundMenu::updateCurrentItem
 * @param item
 * @return
 */
bool DialogWindowBackgroundMenu::updateCurrentItem(const QModelIndex &index) {
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
 * @brief DialogWindowBackgroundMenu::updateCurrentItem
 * @return
 */
bool DialogWindowBackgroundMenu::updateCurrentItem() {
    QModelIndex index = ui.listItems->currentIndex();
    if(index.isValid()) {
        return updateCurrentItem(index);
    }

    return true;
}
