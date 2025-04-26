
#include "DialogWindowBackgroundMenu.h"
#include "CommandRecorder.h"
#include "CommonDialog.h"
#include "MainWindow.h"
#include "MenuData.h"
#include "MenuItem.h"
#include "MenuItemModel.h"
#include "Preferences.h"
#include "SignalBlocker.h"
#include "UserCommands.h"
#include "Util/String.h"
#include "parse.h"

#include <QMessageBox>

/**
 * @brief Constructor for the DialogWindowBackgroundMenu class.
 *
 * @param parent The parent widget for this dialog, defaults to nullptr.
 * @param f The window flags for the dialog, defaults to Qt::WindowFlags().
 */
DialogWindowBackgroundMenu::DialogWindowBackgroundMenu(QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f) {
	ui.setupUi(this);
	connectSlots();

	CommonDialog::SetButtonIcons(&ui);

	ui.editAccelerator->setMaximumSequenceLength(1);

	ui.buttonPasteLRMacro->setEnabled(!CommandRecorder::instance()->replayMacro().isEmpty());

	model_ = new MenuItemModel(this);
	ui.listItems->setModel(model_);

	// Copy the list of menu information to one that the user can freely edit
	for (const MenuData &menuData : BGMenuData) {
		model_->addItem(menuData.item);
	}

	connect(ui.listItems->selectionModel(), &QItemSelectionModel::currentChanged, this, &DialogWindowBackgroundMenu::currentChanged, Qt::QueuedConnection);
	connect(this, &DialogWindowBackgroundMenu::restore, ui.listItems, &QListView::setCurrentIndex, Qt::QueuedConnection);

	// default to selecting the first item
	if (model_->rowCount() != 0) {
		const QModelIndex index = model_->index(0, 0);
		ui.listItems->setCurrentIndex(index);
	}
}

/**
 * @brief Connects the slots for the dialog's buttons and other UI elements.
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
 * @brief Handler for the "New" button click event.
 */
void DialogWindowBackgroundMenu::buttonNew_clicked() {

	if (!updateCurrentItem()) {
		return;
	}

	CommonDialog::AddNewItem(&ui, model_, []() {
		MenuItem item;
		// some sensible defaults...
		item.name = tr("New Item");
		return item;
	});
}

/**
 * @brief Handler for the "Copy" button click event.
 */
void DialogWindowBackgroundMenu::buttonCopy_clicked() {

	if (!updateCurrentItem()) {
		return;
	}

	CommonDialog::CopyItem(&ui, model_);
}

/**
 * @brief Handler for the "Delete" button click event.
 */
void DialogWindowBackgroundMenu::buttonDelete_clicked() {
	CommonDialog::DeleteItem(&ui, model_, &deleted_);
}

/**
 * @brief Handler for the "Paste LR Macro" button click event.
 */
void DialogWindowBackgroundMenu::buttonPasteLRMacro_clicked() {

	const QString replayMacro = CommandRecorder::instance()->replayMacro();
	if (replayMacro.isEmpty()) {
		return;
	}

	ui.editMacro->insertPlainText(replayMacro);
}

/**
 * @brief Moves the currently selected item up in the model and updates the UI.
 */
void DialogWindowBackgroundMenu::buttonUp_clicked() {
	CommonDialog::MoveItemUp(&ui, model_);
}

/**
 * @brief Moves the currently selected item down in the model and updates the UI.
 */
void DialogWindowBackgroundMenu::buttonDown_clicked() {
	CommonDialog::MoveItemDown(&ui, model_);
}

/**
 * @brief Handles the change of the current item in the list view.
 *
 * @param current The QModelIndex of the currently selected item.
 * @param previous The QModelIndex of the previously selected item.
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
		if (!updateItem(previous)) {
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
	CommonDialog::UpdateButtonStates(&ui, model_, current);
}

/**
 * @brief Checks the macro text for validity and displays a message box if there are errors.
 */
void DialogWindowBackgroundMenu::buttonCheck_clicked() {
	if (validateFields(Verbosity::Verbose)) {
		QMessageBox::information(this,
								 tr("Macro"),
								 tr("Macro compiled without error"));
	}
}

/**
 * @brief Handles the click event for the "Apply" button in the dialog.
 */
void DialogWindowBackgroundMenu::buttonApply_clicked() {
	applyDialogChanges();
}

/**
 * @brief Handles the click event for the "OK" button in the dialog.
 */
void DialogWindowBackgroundMenu::buttonOK_clicked() {

	// Read the dialog fields, and update the menus
	if (!applyDialogChanges()) {
		return;
	}

	accept();
}

/**
 * @brief Validates the fields in the dialog and returns whether they are valid.
 *
 * @param verbosity The verbosity level for error messages.
 * @return `true` if the fields are valid, `false` otherwise.
 */
bool DialogWindowBackgroundMenu::validateFields(Verbosity verbosity) {

	if (auto f = readFields(verbosity)) {
		return checkMacroText(f->cmd, verbosity);
	}

	return false;
}

/**
 * @brief Read the name, accelerator, mnemonic, and command fields.
 *
 * @param verbosity The verbosity level for error messages.
 * @return A MenuItem structure containing the dialog fields if valid, or an empty optional if the fields are invalid.
 */
std::optional<MenuItem> DialogWindowBackgroundMenu::readFields(Verbosity verbosity) {

	const QString nameText = ui.editName->text();

	if (nameText.isEmpty()) {
		if (verbosity == Verbosity::Verbose) {
			QMessageBox::warning(this, tr("Menu Entry"), tr("Please specify a name for the menu item"));
		}
		return {};
	}

	if (nameText.indexOf(QLatin1Char(':')) != -1) {
		if (verbosity == Verbosity::Verbose) {
			QMessageBox::warning(this, tr("Menu Entry"), tr("Menu item names may not contain colon (:) characters"));
		}
		return {};
	}

	QString cmdText = ui.editMacro->toPlainText();
	if (cmdText.isEmpty()) {
		if (verbosity == Verbosity::Verbose) {
			QMessageBox::warning(this, tr("Command to Execute"), tr("Please specify macro command(s) to execute"));
		}
		return {};
	}

	cmdText = ensure_newline(cmdText);
	if (!checkMacroText(cmdText, verbosity)) {
		return {};
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
 * @brief Checks the macro text for validity and reports errors if any.
 *
 * @param macro The macro text to check for validity.
 * @param verbosity The verbosity level for error messages.
 * @return `true` if the macro text is valid, `false` otherwise.
 */
bool DialogWindowBackgroundMenu::checkMacroText(const QString &macro, Verbosity verbosity) {

	QString errMsg;
	int stoppedAt;
	if (!isMacroValid(macro, &errMsg, &stoppedAt)) {
		if (verbosity == Verbosity::Verbose) {
			Preferences::ReportError(this, macro, stoppedAt, tr("macro"), errMsg);
		}
		QTextCursor cursor = ui.editMacro->textCursor();
		cursor.setPosition(stoppedAt);
		ui.editMacro->setTextCursor(cursor);
		ui.editMacro->setFocus();
		return false;
	}

	if (stoppedAt != macro.size()) {
		if (verbosity == Verbosity::Verbose) {
			Preferences::ReportError(this, macro, stoppedAt, tr("macro"), tr("syntax error"));
		}

		QTextCursor cursor = ui.editMacro->textCursor();
		cursor.setPosition(stoppedAt);
		ui.editMacro->setTextCursor(cursor);
		ui.editMacro->setFocus();
	}

	return true;
}

/**
 * @brief Applies the changes made in the dialog to the model and updates the menus.
 *
 * @return `true` if the changes were successfully applied, `false` otherwise.
 */
bool DialogWindowBackgroundMenu::applyDialogChanges() {

	if (model_->rowCount() != 0) {
		auto dialogFields = readFields(Verbosity::Verbose);
		if (!dialogFields) {
			return false;
		}

		// Get the current selected item
		const QModelIndex index = ui.listItems->currentIndex();
		if (!index.isValid()) {
			return false;
		}

		// update the currently selected item's associated data
		// and make sure it has the text updated as well
		model_->updateItem(index, *dialogFields);
	}

	std::vector<MenuData> newItems;

	for (int i = 0; i < model_->rowCount(); ++i) {
		const QModelIndex index = model_->index(i, 0);
		auto item               = model_->itemFromIndex(index);
		newItems.push_back({*item, nullptr});
	}

	BGMenuData = std::move(newItems);

	ParseMenuItemList(BGMenuData);

	// Update the menus themselves in all of the NEdit windows
	for (MainWindow *window : MainWindow::allWindows()) {
		window->updateUserMenus();
	}

	// Note that preferences have been changed
	Preferences::MarkPrefsChanged();
	return true;
}

/**
 * @brief Sets whether the "Paste LR Macro" button is enabled or disabled.
 *
 * @param enabled If `true`, the button will be enabled; if `false`, it will be disabled.
 */
void DialogWindowBackgroundMenu::setPasteReplayEnabled(bool enabled) {
	ui.buttonPasteLRMacro->setEnabled(enabled);
}

/**
 * @brief Updates an item in the model with the current dialog fields.
 *
 * @param item The item to update in the model.
 * @return `true` if the item was successfully updated, `false` otherwise.
 */
bool DialogWindowBackgroundMenu::updateItem(const QModelIndex &index) {
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
 * @brief Updates the currently selected item in the model with the current dialog fields.
 *
 * @return `true` if the item was successfully updated, `false` otherwise.
 */
bool DialogWindowBackgroundMenu::updateCurrentItem() {
	const QModelIndex index = ui.listItems->currentIndex();
	if (index.isValid()) {
		return updateItem(index);
	}

	return true;
}
