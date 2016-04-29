
#include <QMessageBox>
#include <QPushButton>
#include "DialogShellMenu.h"

#include "MenuItem.h"
#include "interpret.h"
#include "macro.h"
#include "preferences.h"
#include "shell.h"
#include "userCmds.h"

//------------------------------------------------------------------------------
// Name: DialogShellMenu
//------------------------------------------------------------------------------
DialogShellMenu::DialogShellMenu(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f), previous_(nullptr) {
	ui.setupUi(this);

	for (int i = 0; i < NShellMenuItems; i++) {
		auto ptr  = new MenuItem(*ShellMenuItems[i]);
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

	// TODO(eteran): update entry we are leaving
	
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

	// TODO(eteran): update entry we are leaving

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
		// TODO(eteran): update entry we are leaving
	}

	if(current) {
		const int i = ui.listItems->row(current);

		auto ptr = reinterpret_cast<MenuItem *>(current->data(Qt::UserRole).toULongLong());

		char buf[255];
		generateAcceleratorString(buf, ptr->modifiers, ptr->keysym);

		ui.editName->setText(ptr->name);
		ui.editAccelerator->setText(tr("%1").arg(QLatin1String(buf)));
		ui.editMnemonic->setText(tr("%1").arg(ptr->mnemonic));
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
		ui.editAccelerator->setText(QString());
		ui.editMnemonic->setText(QString());
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
MenuItem *DialogShellMenu::readDialogFields(bool silent) {

	QString nameText = ui.editName->text();
	if (nameText.isEmpty()) {
		if (!silent) {
			QMessageBox::warning(this, tr("Menu Entry"), tr("Please specify a name for the menu item"));
		}
		return nullptr;
	}

	if (nameText.indexOf(QLatin1Char(':')) != -1) {
		if (!silent) {
			QMessageBox::warning(this, tr("Menu Entry"), tr("Menu item names may not contain colon (:) characters"));
		}
		return nullptr;
	}

	QString cmdText = ui.editCommand->toPlainText();
	if (cmdText.isEmpty()) {
		if (!silent) {
			QMessageBox::warning(this, tr("Command to Execute"), tr("Please specify macro command(s) to execute"));
		}
		return nullptr;
	}

	auto f = new MenuItem;
	f->name = nameText;
	f->cmd  = cmdText;

	QString mneText = ui.editMnemonic->text();
	if(!mneText.isEmpty()) {

		f->mnemonic = mneText[0].toLatin1();

		// colons mess up string parsing
		if (f->mnemonic == ':') {
			f->mnemonic = '\0';
		}
	}

	QString accText = ui.editAccelerator->text();
	if(!accText.isEmpty()) {
		parseAcceleratorString(accText.toLatin1().data(), &f->modifiers, &f->keysym);
	}

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
	auto current = readDialogFields(false);
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
	selection->setData(Qt::UserRole, reinterpret_cast<qulonglong>(current));
	selection->setText(current->name);

	// Update the menu information
	for (int i = 0; i < NShellMenuItems; i++) {
		delete ShellMenuItems[i];
	}

	freeUserMenuInfoList(ShellMenuInfo, NShellMenuItems);
	freeSubMenuCache(&ShellSubMenus);

	int count = ui.listItems->count();
	for(int i = 0; i < count; ++i) {
		auto ptr = itemFromIndex(i);
		ShellMenuItems[i] = new MenuItem(*ptr);
	}

	NShellMenuItems = count;

	parseMenuItemList(ShellMenuItems, NShellMenuItems, ShellMenuInfo, &ShellSubMenus);

	// Update the menus themselves in all of the NEdit windows
	rebuildMenuOfAllWindows(SHELL_CMDS);

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
