
#include "DialogShellMenu.h"
#include "MainWindow.h"
#include "MenuItem.h"
#include "SignalBlocker.h"
#include "interpret.h"
#include "macro.h"
#include "preferences.h"
#include "userCmds.h"
#include <QMessageBox>
#include <QPushButton>

//------------------------------------------------------------------------------
// Name: DialogShellMenu
//------------------------------------------------------------------------------
DialogShellMenu::DialogShellMenu(QWidget *parent, Qt::WindowFlags f) : Dialog(parent, f), previous_(nullptr) {
	ui.setupUi(this);
    ui.editAccelerator->setMaximumSequenceLength(1);

	for(MenuData &data : ShellMenuData) {
		auto ptr  = new MenuItem(*data.item);
		auto item = new QListWidgetItem(ptr->name);
		item->setData(Qt::UserRole, reinterpret_cast<qulonglong>(ptr));
		ui.listItems->addItem(item);
	}

	if(ui.listItems->count() != 0) {
		ui.listItems->setCurrentRow(0);
	}
}

//------------------------------------------------------------------------------
// Name: ~DialogShellMenu
//------------------------------------------------------------------------------
DialogShellMenu::~DialogShellMenu() {
	for(int i = 0; i < ui.listItems->count(); ++i) {
	    delete itemFromIndex(i);
	}
}

//------------------------------------------------------------------------------
// Name: itemFromIndex
//------------------------------------------------------------------------------
MenuItem *DialogShellMenu::itemFromIndex(int i) const {
	if(i < ui.listItems->count()) {
	    QListWidgetItem* item = ui.listItems->item(i);
		auto ptr = reinterpret_cast<MenuItem *>(item->data(Qt::UserRole).toULongLong());
		return ptr;
	}
	
	return nullptr;
}

//------------------------------------------------------------------------------
// Name: on_buttonNew_clicked
//------------------------------------------------------------------------------
void DialogShellMenu::on_buttonNew_clicked() {

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

//------------------------------------------------------------------------------
// Name: on_buttonCopy_clicked
//------------------------------------------------------------------------------
void DialogShellMenu::on_buttonCopy_clicked() {

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

//------------------------------------------------------------------------------
// Name: on_buttonDelete_clicked
//------------------------------------------------------------------------------
void DialogShellMenu::on_buttonDelete_clicked() {

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
	Q_EMIT on_listItems_itemSelectionChanged();
}

//------------------------------------------------------------------------------
// Name: on_buttonUp_clicked
//------------------------------------------------------------------------------
void DialogShellMenu::on_buttonUp_clicked() {

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
// Name: on_buttonDown_clicked
//------------------------------------------------------------------------------
void DialogShellMenu::on_buttonDown_clicked() {

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
// Name: on_listItems_itemSelectionChanged
//------------------------------------------------------------------------------
void DialogShellMenu::on_listItems_itemSelectionChanged() {

	QList<QListWidgetItem *> selections = ui.listItems->selectedItems();
	if(selections.size() != 1) {
		previous_ = nullptr;
		return;
	}

	QListWidgetItem *const current = selections[0];

	if(previous_ != nullptr && current != nullptr && current != previous_) {
	
		// we want to try to save it (but not apply it yet)
		// and then move on
        if(!checkCurrent(Mode::Silent)) {

			QMessageBox messageBox(this);
			messageBox.setWindowTitle(tr("Discard Entry"));
			messageBox.setIcon(QMessageBox::Warning);
			messageBox.setText(tr("Discard incomplete entry for current highlight style?"));
			QPushButton *buttonKeep    = messageBox.addButton(tr("Keep"), QMessageBox::RejectRole);
			QPushButton *buttonDiscard = messageBox.addButton(tr("Discard"), QMessageBox::AcceptRole);
			Q_UNUSED(buttonDiscard);

			messageBox.exec();
			if (messageBox.clickedButton() == buttonKeep) {
			
				// again to cause messagebox to pop up
                checkCurrent(Mode::Verbose);
				
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
		ui.editCommand->setPlainText(ptr->cmd);
				
		switch(ptr->input) {
		case FROM_SELECTION:
		default:
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
		default:
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
		ui.editCommand->setPlainText(QString());

		ui.buttonUp    ->setEnabled(false);
		ui.buttonDown  ->setEnabled(false);
		ui.buttonDelete->setEnabled(false);
		ui.buttonCopy  ->setEnabled(false);
		
		ui.radioFromSelection->setChecked(true);
		ui.radioToSameDocument->setChecked(true);

		
		ui.checkReplaceInput->setChecked(false);	
		ui.checkSaveBeforeExec->setChecked(false);
		ui.checkReloadAfterExec->setChecked(false);
				
	}
	
	previous_ = current;
}



//------------------------------------------------------------------------------
// Name: on_buttonBox_clicked
//------------------------------------------------------------------------------
void DialogShellMenu::on_buttonBox_clicked(QAbstractButton *button) {
	if(ui.buttonBox->standardButton(button) == QDialogButtonBox::Apply) {
		applyDialogChanges();
	}
}

//------------------------------------------------------------------------------
// Name: on_buttonBox_accepted
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Name: applyDialogChanges
//------------------------------------------------------------------------------
bool DialogShellMenu::applyDialogChanges() {

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
	freeUserMenuInfoList(ShellMenuData);

	int count = ui.listItems->count();
	for(int i = 0; i < count; ++i) {
		auto ptr = itemFromIndex(i);		
		ShellMenuData.push_back({ new MenuItem(*ptr), nullptr });
	}

    parseMenuItemList(ShellMenuData);

	// Update the menus themselves in all of the NEdit windows
    for(MainWindow *window : MainWindow::allWindows()) {
        window->UpdateUserMenus(window->currentDocument());
    }

	// Note that preferences have been changed
	MarkPrefsChanged();

	return true;
}

//------------------------------------------------------------------------------
// Name: on_radioToSameDocument_toggled
//------------------------------------------------------------------------------
void DialogShellMenu::on_radioToSameDocument_toggled(bool checked) {
	ui.checkReplaceInput->setEnabled(checked);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
bool DialogShellMenu::updateCurrentItem(QListWidgetItem *item) {
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

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
bool DialogShellMenu::updateCurrentItem() {
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
bool DialogShellMenu::checkCurrent(Mode mode) {
    if(auto ptr = readDialogFields(mode)) {
		return true;
	}
	
	return false;
}
