
#include "DialogWindowBackgroundMenu.h"
#include "CommandRecorder.h"
#include "CommonDialog.h"
#include "MainWindow.h"
#include "MenuData.h"
#include "MenuItem.h"
#include "MenuItemModel.h"
#include "Preferences.h"
#include "SignalBlocker.h"
#include "Util/String.h"
#include "parse.h"
#include "userCmds.h"

#include <QMessageBox>

/**
 * @brief DialogWindowBackgroundMenu::DialogWindowBackgroundMenu
 * @param parent
 * @param f
 */
DialogWindowBackgroundMenu::DialogWindowBackgroundMenu(QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f) {
	ui.setupUi(this);
	connectSlots();

	CommonDialog::setButtonIcons(&ui);

	ui.editAccelerator->setMaximumSequenceLength(1);

	ui.buttonPasteLRMacro->setEnabled(!CommandRecorder::instance()->replayMacro().isEmpty());

	model_ = new MenuItemModel(this);
	ui.listItems->setModel(model_);

	// Copy the list of menu information to one that the user can freely edit
	for (MenuData &menuData : BGMenuData) {
		model_->addItem(menuData.item);
	}

	connect(ui.listItems->selectionModel(), &QItemSelectionModel::currentChanged, this, &DialogWindowBackgroundMenu::currentChanged, Qt::QueuedConnection);
	connect(this, &DialogWindowBackgroundMenu::restore, ui.listItems, &QListView::setCurrentIndex, Qt::QueuedConnection);

	// default to selecting the first item
	if (model_->rowCount() != 0) {
		QModelIndex index = model_->index(0, 0);
		ui.listItems->setCurrentIndex(index);
	}
}

/**
 * @brief DialogWindowBackgroundMenu::connectSlots
 */
void DialogWindowBackgroundMenu::connectSlots() {
	connect(ui.buttonNew, &QPushButton::clicked, this, &DialogWindowBackgroundMenu::buttonNew_clicked);
	connect(ui.buttonCopy, &QPushButton::clicked, this, &DialogWindowBackgroundMenu::buttonCopy_clicked);
	connect(ui.buttonDelete, &QPushButton::clicked, this, &DialogWindowBackgroundMenu::buttonDelete_clicked);
	connect(ui.buttonUp, &QPushButton::clicked, this, &DialogWindowBackgroundMenu::buttonUp_clicked);
	connect(ui.buttonDown, &QPushButton::clicked, this, &DialogWindowBackgroundMenu::buttonDown_clicked);
	connect(ui.buttonPasteLRMacro, &QPushButton::clicked, this, &DialogWindowBackgroundMenu::buttonPasteLRMacro_clicked);
	connect(ui.buttonCheck, &QPushButton::clicked, this, &DialogWindowBackgroundMenu::buttonCheck_clicked);
	connect(ui.buttonApply, &QPushButton::clicked, this, &DialogWindowBackgroundMenu::buttonApply_clicked);
	connect(ui.buttonOK, &QPushButton::clicked, this, &DialogWindowBackgroundMenu::buttonOK_clicked);
}

/**
 * @brief DialogWindowBackgroundMenu::buttonNew_clicked
 */
void DialogWindowBackgroundMenu::buttonNew_clicked() {

	if (!updateCurrentItem()) {
		return;
	}

	CommonDialog::addNewItem(&ui, model_, []() {
		MenuItem item;
		// some sensible defaults...
		item.name = tr("New Item");
		return item;
	});
}

/**
 * @brief DialogWindowBackgroundMenu::buttonCopy_clicked
 */
void DialogWindowBackgroundMenu::buttonCopy_clicked() {

	if (!updateCurrentItem()) {
		return;
	}

	CommonDialog::copyItem(&ui, model_);
}

/**
 * @brief DialogWindowBackgroundMenu::buttonDelete_clicked
 */
void DialogWindowBackgroundMenu::buttonDelete_clicked() {
	CommonDialog::deleteItem(&ui, model_, &deleted_);
}

/**
 * @brief DialogWindowBackgroundMenu::buttonPasteLRMacro_clicked
 */
void DialogWindowBackgroundMenu::buttonPasteLRMacro_clicked() {

	QString replayMacro = CommandRecorder::instance()->replayMacro();
	if (replayMacro.isEmpty()) {
		return;
	}

	ui.editMacro->insertPlainText(replayMacro);
}

/**
 * @brief DialogWindowBackgroundMenu::buttonUp_clicked
 */
void DialogWindowBackgroundMenu::buttonUp_clicked() {
	CommonDialog::moveItemUp(&ui, model_);
}

/**
 * @brief DialogWindowBackgroundMenu::buttonDown_clicked
 */
void DialogWindowBackgroundMenu::buttonDown_clicked() {
	CommonDialog::moveItemDown(&ui, model_);
}

/**
 * @brief DialogWindowBackgroundMenu::currentChanged
 * @param current
 * @param previous
 */
void DialogWindowBackgroundMenu::currentChanged(const QModelIndex &current, const QModelIndex &previous) {
	static bool canceled = false;
	bool skip_check      = false;

	if (canceled) {
		canceled = false;
		return;
	}

	// if we are actually switching items, check that the previous one was valid
	// so we can optionally cancel
	if (previous.isValid() && previous != deleted_ && !validateFields(Verbosity::Silent)) {
		QMessageBox messageBox(this);
		messageBox.setWindowTitle(tr("Discard Entry"));
		messageBox.setIcon(QMessageBox::Warning);
		messageBox.setText(tr("Discard incomplete entry for current menu item?"));
		QPushButton *buttonKeep    = messageBox.addButton(tr("Keep"), QMessageBox::RejectRole);
		QPushButton *buttonDiscard = messageBox.addButton(QMessageBox::Discard);

		messageBox.exec();
		if (messageBox.clickedButton() == buttonKeep) {

			// again to cause message box to pop up
			validateFields(Verbosity::Verbose);

			// reselect the old item
			canceled = true;
			Q_EMIT restore(previous);
			return;
		}

		if (messageBox.clickedButton() == buttonDiscard) {
			model_->deleteItem(previous);
			skip_check = true;
		}
	}

	// this is only safe if we aren't moving due to a delete operation
	if (previous.isValid() && previous != deleted_ && !skip_check) {
		if (!updateCurrentItem(previous)) {
			// reselect the old item
			canceled = true;
			Q_EMIT restore(previous);
			return;
		}
	}

	// previous was OK, so let's update the contents of the dialog
	if (const auto ptr = model_->itemFromIndex(current)) {
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
	CommonDialog::updateButtonStates(&ui, model_, current);
}

/**
 * @brief DialogWindowBackgroundMenu::buttonCheck_clicked
 */
void DialogWindowBackgroundMenu::buttonCheck_clicked() {
	if (validateFields(Verbosity::Verbose)) {
		QMessageBox::information(this,
								 tr("Macro"),
								 tr("Macro compiled without error"));
	}
}

/**
 * @brief DialogWindowBackgroundMenu::buttonApply_clicked
 */
void DialogWindowBackgroundMenu::buttonApply_clicked() {
	applyDialogChanges();
}

/**
 * @brief DialogWindowBackgroundMenu::buttonOK_clicked
 */
void DialogWindowBackgroundMenu::buttonOK_clicked() {

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

	if (auto f = readFields(verbosity)) {
		return checkMacroText(f->cmd, verbosity);
	}

	return false;
}

/*
** Read the name, accelerator, mnemonic, and command fields.
*/
boost::optional<MenuItem> DialogWindowBackgroundMenu::readFields(Verbosity verbosity) {

	QString nameText = ui.editName->text();

	if (nameText.isEmpty()) {
		if (verbosity == Verbosity::Verbose) {
			QMessageBox::warning(this, tr("Menu Entry"), tr("Please specify a name for the menu item"));
		}
		return boost::none;
	}

	if (nameText.indexOf(QLatin1Char(':')) != -1) {
		if (verbosity == Verbosity::Verbose) {
			QMessageBox::warning(this, tr("Menu Entry"), tr("Menu item names may not contain colon (:) characters"));
		}
		return boost::none;
	}

	QString cmdText = ui.editMacro->toPlainText();
	if (cmdText.isEmpty()) {
		if (verbosity == Verbosity::Verbose) {
			QMessageBox::warning(this, tr("Command to Execute"), tr("Please specify macro command(s) to execute"));
		}
		return boost::none;
	}

	cmdText = ensure_newline(cmdText);
	if (!checkMacroText(cmdText, verbosity)) {
		return boost::none;
	}

	MenuItem menuItem;
	menuItem.name      = nameText;
	menuItem.cmd       = cmdText;
	menuItem.input     = ui.checkRequiresSelection->isChecked() ? FROM_SELECTION : FROM_NONE;
	menuItem.output    = TO_SAME_WINDOW;
	menuItem.repInput  = false;
	menuItem.saveFirst = false;
	menuItem.loadAfter = false;
	menuItem.shortcut  = ui.editAccelerator->keySequence();

	return menuItem;
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
	if (!isMacroValid(macro, &errMsg, &stoppedAt)) {
		if (verbosity == Verbosity::Verbose) {
			Preferences::reportError(this, macro, stoppedAt, tr("macro"), errMsg);
		}
		QTextCursor cursor = ui.editMacro->textCursor();
		cursor.setPosition(stoppedAt);
		ui.editMacro->setTextCursor(cursor);
		ui.editMacro->setFocus();
		return false;
	}

	if (stoppedAt != macro.size()) {
		if (verbosity == Verbosity::Verbose) {
			Preferences::reportError(this, macro, stoppedAt, tr("macro"), tr("syntax error"));
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

	if (model_->rowCount() != 0) {
		auto dialogFields = readFields(Verbosity::Verbose);
		if (!dialogFields) {
			return false;
		}

		// Get the current selected item
		QModelIndex index = ui.listItems->currentIndex();
		if (!index.isValid()) {
			return false;
		}

		// update the currently selected item's associated data
		// and make sure it has the text updated as well
		model_->updateItem(index, *dialogFields);
	}

	std::vector<MenuData> newItems;

	for (int i = 0; i < model_->rowCount(); ++i) {
		QModelIndex index = model_->index(i, 0);
		auto item         = model_->itemFromIndex(index);
		newItems.push_back({*item, nullptr});
	}

	BGMenuData = std::move(newItems);

	parse_menu_item_list(BGMenuData);

	// Update the menus themselves in all of the NEdit windows
	for (MainWindow *window : MainWindow::allWindows()) {
		window->updateUserMenus();
	}

	// Note that preferences have been changed
	Preferences::MarkPrefsChanged();
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
	if (!dialogFields) {
		return false;
	}

	// Get the current contents of the dialog fields
	if (!index.isValid()) {
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
	if (index.isValid()) {
		return updateCurrentItem(index);
	}

	return true;
}
