
#include <QMessageBox>
#include "DialogMacros.h"

#include "MenuItem.h"
#include "interpret.h"
#include "macro.h"
#include "preferences.h"
#include "shell.h"
#include "userCmds.h"

//------------------------------------------------------------------------------
// Name: DialogMacros
//------------------------------------------------------------------------------
DialogMacros::DialogMacros(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f), previous_(nullptr) {
	ui.setupUi(this);

	for (int i = 0; i < NMacroMenuItems; i++) {
		auto ptr  = new MenuItem(*MacroMenuItems[i]);
		auto item = new QListWidgetItem(ptr->name);
		item->setData(Qt::UserRole, reinterpret_cast<qulonglong>(ptr));
		ui.listItems->addItem(item);
	}

	if(ui.listItems->count() != 0) {
		ui.listItems->setCurrentRow(0);
	}
}

//------------------------------------------------------------------------------
// Name: ~DialogMacros
//------------------------------------------------------------------------------
DialogMacros::~DialogMacros() {
	for(int i = 0; i < ui.listItems->count(); ++i) {
	    QListWidgetItem* item = ui.listItems->item(i);
		auto ptr = reinterpret_cast<MenuItem *>(item->data(Qt::UserRole).toULongLong());
		delete ptr;
	}
}

//------------------------------------------------------------------------------
// Name: on_buttonNew_clicked
//------------------------------------------------------------------------------
void DialogMacros::on_buttonNew_clicked() {
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
void DialogMacros::on_buttonCopy_clicked() {

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
void DialogMacros::on_buttonDelete_clicked() {

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
// Name: on_buttonPasteLRMacro_clicked
//------------------------------------------------------------------------------
void DialogMacros::on_buttonPasteLRMacro_clicked() {

	if (GetReplayMacro().empty()) {
		return;
	}

	ui.editMacro->insertPlainText(QString::fromStdString(GetReplayMacro()));
}

//------------------------------------------------------------------------------
// Name: on_buttonUp_clicked
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Name: on_buttonDown_clicked
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Name: on_listItems_itemSelectionChanged
//------------------------------------------------------------------------------
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
		if(!checkMacro(true)) {

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
				checkMacro(false);
				
				// reselect the old item
				ui.listItems->blockSignals(true);
				ui.listItems->setCurrentItem(previous_);
				ui.listItems->blockSignals(false);
				return;
			}

			// if we get here, we are ditching changes
		} else {
			// TODO(eteran): update entry we are leaving
		}
	}

	if(current) {
		const int i = ui.listItems->row(current);

		auto ptr = reinterpret_cast<MenuItem *>(current->data(Qt::UserRole).toULongLong());

		char buf[255];
		generateAcceleratorString(buf, ptr->modifiers, ptr->keysym);

		ui.editName->setText(ptr->name);
		ui.editAccelerator->setText(tr("%1").arg(QLatin1String(buf)));
		ui.editMnemonic->setText(tr("%1").arg(ptr->mnemonic));
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
		ui.editAccelerator->setText(QString());
		ui.editMnemonic->setText(QString());
		ui.checkRequiresSelection->setChecked(false);
		ui.editMacro->setPlainText(QString());

		ui.buttonUp    ->setEnabled(false);
		ui.buttonDown  ->setEnabled(false);
		ui.buttonDelete->setEnabled(false);
		ui.buttonCopy  ->setEnabled(false);
	}
	
	previous_ = current;
}

//------------------------------------------------------------------------------
// Name: on_buttonCheck_clicked
//------------------------------------------------------------------------------
void DialogMacros::on_buttonCheck_clicked() {
	if (checkMacro(false)) {
		QMessageBox::information(this, tr("Macro"), tr("Macro compiled without error"));
	}
}

//------------------------------------------------------------------------------
// Name: on_buttonApply_clicked
//------------------------------------------------------------------------------
void DialogMacros::on_buttonApply_clicked() {
	applyDialogChanges();
}

//------------------------------------------------------------------------------
// Name: on_buttonOK_clicked
//------------------------------------------------------------------------------
void DialogMacros::on_buttonOK_clicked() {

	// Read the dialog fields, and update the menus
	if (!applyDialogChanges()) {
		return;
	}

	accept();
}

//------------------------------------------------------------------------------
// Name: checkMacro
//------------------------------------------------------------------------------
bool DialogMacros::checkMacro(bool silent) {

	MenuItem *f = readDialogFields(silent);
	if(!f) {
		return false;
	}

	if (!checkMacroText(f->cmd, silent)) {
		delete f;
		return false;
	}

	delete f;
	return true;
}


/*
** Read the name, accelerator, mnemonic, and command fields from the shell or
** macro commands dialog into a newly allocated MenuItem.  Returns a
** pointer to the new MenuItem structure as the function value, or nullptr on
** failure.
*/
MenuItem *DialogMacros::readDialogFields(bool silent) {

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

	QString cmdText = ui.editMacro->toPlainText();
	if (cmdText.isEmpty()) {
		if (!silent) {
			QMessageBox::warning(this, tr("Command to Execute"), tr("Please specify macro command(s) to execute"));
		}
		return nullptr;
	}

	cmdText = ensureNewline(cmdText);
	if (!checkMacroText(cmdText, silent)) {
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

	f->input     = ui.checkRequiresSelection->isChecked() ? FROM_SELECTION : FROM_NONE;
	f->output    = TO_SAME_WINDOW;
	f->repInput  = false;
	f->saveFirst = false;
	f->loadAfter = false;

	return f;
}

//------------------------------------------------------------------------------
// Name: checkMacroText
//------------------------------------------------------------------------------
bool DialogMacros::checkMacroText(const QString &macro, bool silent) {

	QString errMsg;
	int stoppedAt;

	Program *prog = ParseMacroEx(macro, 0, &errMsg, &stoppedAt);
	if(!prog) {
		if(!silent) {
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
		if(!silent) {
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

//------------------------------------------------------------------------------
// Name: applyDialogChanges
//------------------------------------------------------------------------------
bool DialogMacros::applyDialogChanges() {

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
	for (int i = 0; i < NMacroMenuItems; i++) {
		delete MacroMenuItems[i];
	}

	freeUserMenuInfoList(MacroMenuInfo, NMacroMenuItems);
	freeSubMenuCache(&MacroSubMenus);

	int count = ui.listItems->count();
	for(int i = 0; i < count; ++i) {
	    QListWidgetItem* item = ui.listItems->item(i);
		auto ptr = reinterpret_cast<MenuItem *>(item->data(Qt::UserRole).toULongLong());
		MacroMenuItems[i] = new MenuItem(*ptr);
	}

	NMacroMenuItems = count;

	parseMenuItemList(MacroMenuItems, NMacroMenuItems, MacroMenuInfo, &MacroSubMenus);
	

	// Update the menus themselves in all of the NEdit windows
	rebuildMenuOfAllWindows(MACRO_CMDS);
	
	// Note that preferences have been changed
	MarkPrefsChanged();

	return true;
}

void DialogMacros::setPasteReplayEnabled(bool enabled) {
	ui.buttonPasteLRMacro->setEnabled(enabled);
}
