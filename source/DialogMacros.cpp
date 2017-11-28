
#include "DialogMacros.h"
#include "CommandRecorder.h"
#include "interpret.h"
#include "macro.h"
#include "MainWindow.h"
#include "MenuData.h"
#include "MenuItem.h"
#include "preferences.h"
#include "SignalBlocker.h"
#include "userCmds.h"

#include <QMessageBox>

/**
 * @brief DialogMacros::DialogMacros
 * @param parent
 * @param f
 */
DialogMacros::DialogMacros(QWidget *parent, Qt::WindowFlags f) : Dialog(parent, f), previous_(nullptr) {
	ui.setupUi(this);
    ui.editAccelerator->setMaximumSequenceLength(1);

	for(MenuData &data : MacroMenuData) {
        auto ptr  = new MenuItem(*data.item.get());
		auto item = new QListWidgetItem(ptr->name);
		item->setData(Qt::UserRole, reinterpret_cast<qulonglong>(ptr));
		ui.listItems->addItem(item);
	}

	if(ui.listItems->count() != 0) {
		ui.listItems->setCurrentRow(0);
	}
}

/**
 * @brief DialogMacros::~DialogMacros
 */
DialogMacros::~DialogMacros() noexcept {
	for(int i = 0; i < ui.listItems->count(); ++i) {
	    delete itemFromIndex(i);
	}
}

/**
 * @brief DialogMacros::itemFromIndex
 * @param i
 * @return
 */
MenuItem *DialogMacros::itemFromIndex(int i) const {
	if(i < ui.listItems->count()) {
	    QListWidgetItem* item = ui.listItems->item(i);
		auto ptr = reinterpret_cast<MenuItem *>(item->data(Qt::UserRole).toULongLong());
		return ptr;
	}
	
	return nullptr;
}

/**
 * @brief DialogMacros::on_buttonNew_clicked
 */
void DialogMacros::on_buttonNew_clicked() {

    // if the list isn't empty, then make sure we've updated the current one
    // before moving the focus away
    if(ui.listItems->count() != 0) {
        if(!updateCurrentItem()) {
            return;
        }
    }

	auto ptr  = new MenuItem;
	ptr->name = tr("New Item");

	auto item = new QListWidgetItem(ptr->name);
	item->setData(Qt::UserRole, reinterpret_cast<qulonglong>(ptr));
	ui.listItems->addItem(item);
	ui.listItems->setCurrentItem(item);
}

/**
 * @brief DialogMacros::on_buttonCopy_clicked
 */
void DialogMacros::on_buttonCopy_clicked() {

	if(!updateCurrentItem()) {
		return;
	}

	QList<QListWidgetItem *> selections = ui.listItems->selectedItems();
	if(selections.size() != 1) {
		return;
	}

	QListWidgetItem *const selection = selections[0];
	auto ptr = reinterpret_cast<MenuItem *>(selection->data(Qt::UserRole).toULongLong());
	auto newPtr = new MenuItem(*ptr);
	auto newItem = new QListWidgetItem(newPtr->name);
	newItem->setData(Qt::UserRole, reinterpret_cast<qulonglong>(newPtr));

	const int i = ui.listItems->row(selection);
	ui.listItems->insertItem(i + 1, newItem);
	ui.listItems->setCurrentItem(newItem);
}

/**
 * @brief DialogMacros::on_buttonDelete_clicked
 */
void DialogMacros::on_buttonDelete_clicked() {

	QList<QListWidgetItem *> selections = ui.listItems->selectedItems();
	if(selections.size() != 1) {
		return;
	}
	
	// prevent usage of this item going forward
	previous_ = nullptr;

	QListWidgetItem *const selection = selections[0];
	auto ptr = reinterpret_cast<MenuItem *>(selection->data(Qt::UserRole).toULongLong());

	delete ptr;
	delete selection;
	
	// force an update of the display
	on_listItems_itemSelectionChanged();

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

/**
 * @brief DialogMacros::on_buttonDown_clicked
 */
void DialogMacros::on_buttonDown_clicked() {

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

/**
 * @brief DialogMacros::on_listItems_itemSelectionChanged
 */
void DialogMacros::on_listItems_itemSelectionChanged() {

	QList<QListWidgetItem *> selections = ui.listItems->selectedItems();
	if(selections.size() != 1) {
		previous_ = nullptr;
		return;
	}

	QListWidgetItem *const current = selections[0];

	if(previous_ != nullptr && current != nullptr && current != previous_) {
		// we want to try to save it (but not apply it yet)
		// and then move on
        if(!checkMacro(Mode::Silent)) {

			QMessageBox messageBox(this);
			messageBox.setWindowTitle(tr("Discard Entry"));
			messageBox.setIcon(QMessageBox::Warning);
			messageBox.setText(tr("Discard incomplete entry for current menu item?"));
			QPushButton *buttonKeep    = messageBox.addButton(tr("Keep"), QMessageBox::RejectRole);
            QPushButton *buttonDiscard = messageBox.addButton(tr("Discard"), QMessageBox::DestructiveRole);
			Q_UNUSED(buttonDiscard);

			messageBox.exec();
			if (messageBox.clickedButton() == buttonKeep) {
			
				// again to cause messagebox to pop up
                checkMacro(Mode::Verbose);
				
				// reselect the old item
                no_signals(ui.listItems)->setCurrentItem(previous_);
				return;
			}

			// if we get here, we are ditching changes
		} else {
			if(!updateCurrentItem(previous_)) {
				return;
			}
		}
	}

	if(current) {
		const int i = ui.listItems->row(current);

		auto ptr = reinterpret_cast<MenuItem *>(current->data(Qt::UserRole).toULongLong());

        ui.editName->setText(ptr->name);
        ui.editAccelerator->setKeySequence(ptr->shortcut);
		ui.checkRequiresSelection->setChecked(ptr->input == FROM_SELECTION);
		ui.editMacro->setPlainText(ptr->cmd);

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
		ui.editName->setText(QString());
		ui.editAccelerator->clear();
		ui.checkRequiresSelection->setChecked(false);
		ui.editMacro->setPlainText(QString());

		ui.buttonUp    ->setEnabled(false);
		ui.buttonDown  ->setEnabled(false);
		ui.buttonDelete->setEnabled(false);
		ui.buttonCopy  ->setEnabled(false);
	}
	
	previous_ = current;
}

/**
 * @brief DialogMacros::on_buttonCheck_clicked
 */
void DialogMacros::on_buttonCheck_clicked() {
    if (checkMacro(Mode::Verbose)) {
		QMessageBox::information(this, tr("Macro"), tr("Macro compiled without error"));
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

	// Test compile the macro
    auto current = readDialogFields(Mode::Verbose);
	if(!current) {
		return false;
	}

	// Get the current contents of the dialog fields
	QList<QListWidgetItem *> selections = ui.listItems->selectedItems();
	if(selections.size() != 1) {
		return false;
	}

	// update the currently selected item's associated data
	// and make sure it has the text updated as well
	QListWidgetItem *const selection = selections[0];
	auto ptr = reinterpret_cast<MenuItem *>(selection->data(Qt::UserRole).toULongLong());
	delete ptr;

    selection->setText(current->name);
    selection->setData(Qt::UserRole, reinterpret_cast<qulonglong>(current.release()));

	// Update the menu information
    MacroMenuData.clear();

	int count = ui.listItems->count();
	for(int i = 0; i < count; ++i) {
		auto ptr = itemFromIndex(i);
        MacroMenuData.push_back({ std::make_unique<MenuItem>(*ptr), nullptr });
	}

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
bool DialogMacros::updateCurrentItem(QListWidgetItem *item) {
	// Get the current contents of the "patterns" dialog fields 
    auto ptr = readDialogFields(Mode::Verbose);
	if(!ptr) {
		return false;
	}
	
	// delete the current pattern in this slot
	auto old = reinterpret_cast<MenuItem *>(item->data(Qt::UserRole).toULongLong());
	delete old;
	
    item->setText(ptr->name);
    item->setData(Qt::UserRole, reinterpret_cast<qulonglong>(ptr.release()));
	return true;
}

/**
 * @brief DialogMacros::updateCurrentItem
 * @return
 */
bool DialogMacros::updateCurrentItem() {
	QList<QListWidgetItem *> selections = ui.listItems->selectedItems();
	if(selections.size() != 1) {
		return false;
	}

	QListWidgetItem *const selection = selections[0];
	return updateCurrentItem(selection);	
}
