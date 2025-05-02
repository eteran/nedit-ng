
#include "DialogMacros.h"
#include "CommandRecorder.h"
#include "CommonDialog.h"
#include "Font.h"
#include "LanguageMode.h"
#include "MainWindow.h"
#include "MenuData.h"
#include "MenuItem.h"
#include "MenuItemModel.h"
#include "Preferences.h"
#include "UserCommands.h"
#include "Util/String.h"
#include "parse.h"

#include <QMessageBox>

/**
 * @brief Constructor for DialogMacros class.
 *
 * @param parent The parent widget, defaults to nullptr.
 * @param f The window flags for the dialog, defaults to Qt::WindowFlags().
 */
DialogMacros::DialogMacros(QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f) {

	ui.setupUi(this);
	connectSlots();

	const int tabStop = Preferences::GetPrefTabDist(PLAIN_LANGUAGE_MODE);
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
	ui.editMacro->setTabStopDistance(tabStop * Font::characterWidth(ui.editMacro->fontMetrics(), QLatin1Char(' ')));
#else
	ui.editMacro->setTabStopWidth(tabStop * Font::characterWidth(ui.editMacro->fontMetrics(), QLatin1Char(' ')));
#endif

	CommonDialog::SetButtonIcons(&ui);

	ui.editAccelerator->setMaximumSequenceLength(1);

	ui.buttonPasteLRMacro->setEnabled(!CommandRecorder::instance()->replayMacro().isEmpty());

	model_ = new MenuItemModel(this);
	ui.listItems->setModel(model_);

	// Copy the list of menu information to one that the user can freely edit
	for (const MenuData &menuData : MacroMenuData) {
		model_->addItem(menuData.item);
	}

	connect(ui.listItems->selectionModel(), &QItemSelectionModel::currentChanged, this, &DialogMacros::currentChanged, Qt::QueuedConnection);
	connect(this, &DialogMacros::restore, ui.listItems, &QListView::setCurrentIndex, Qt::QueuedConnection);

	// default to selecting the first item
	if (model_->rowCount() != 0) {
		const QModelIndex index = model_->index(0, 0);
		ui.listItems->setCurrentIndex(index);
	}
}

/**
 * @brief Connects the slots for the dialog's buttons and other UI elements.
 */
void DialogMacros::connectSlots() {
	connect(ui.buttonNew, &QPushButton::clicked, this, &DialogMacros::buttonNew_clicked);
	connect(ui.buttonCopy, &QPushButton::clicked, this, &DialogMacros::buttonCopy_clicked);
	connect(ui.buttonDelete, &QPushButton::clicked, this, &DialogMacros::buttonDelete_clicked);
	connect(ui.buttonUp, &QPushButton::clicked, this, &DialogMacros::buttonUp_clicked);
	connect(ui.buttonDown, &QPushButton::clicked, this, &DialogMacros::buttonDown_clicked);
	connect(ui.buttonPasteLRMacro, &QPushButton::clicked, this, &DialogMacros::buttonPasteLRMacro_clicked);
	connect(ui.buttonCheck, &QPushButton::clicked, this, &DialogMacros::buttonCheck_clicked);
	connect(ui.buttonApply, &QPushButton::clicked, this, &DialogMacros::buttonApply_clicked);
	connect(ui.buttonOK, &QPushButton::clicked, this, &DialogMacros::buttonOK_clicked);
}

/**
 * @brief Handles the "New" button click event.
 */
void DialogMacros::buttonNew_clicked() {

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
 * @brief Handles the "Copy" button click event.
 */
void DialogMacros::buttonCopy_clicked() {

	if (!updateCurrentItem()) {
		return;
	}

	CommonDialog::CopyItem(&ui, model_);
}

/**
 * @brief Handles the "Delete" button click event.
 */
void DialogMacros::buttonDelete_clicked() {
	CommonDialog::DeleteItem(&ui, model_, &deleted_);
}

/**
 * @brief Handles the "Paste LR Macro" button click event.
 */
void DialogMacros::buttonPasteLRMacro_clicked() {

	const QString replayMacro = CommandRecorder::instance()->replayMacro();
	if (replayMacro.isEmpty()) {
		return;
	}

	ui.editMacro->insertPlainText(replayMacro);
}

/**
 * @brief Moves the currently selected item up in the model and updates the UI.
 */
void DialogMacros::buttonUp_clicked() {
	CommonDialog::MoveItemUp(&ui, model_);
}

/**
 * @brief Moves the currently selected item down in the model and updates the UI.
 */
void DialogMacros::buttonDown_clicked() {
	CommonDialog::MoveItemDown(&ui, model_);
}

/**
 * @brief Handles the change of the current item in the list view.
 *
 * @param current The currently selected item index.
 * @param previous The previously selected item index, used for validation.
 */
void DialogMacros::currentChanged(const QModelIndex &current, const QModelIndex &previous) {
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
		ui.editAccelerator->clear();
		ui.checkRequiresSelection->setChecked(false);
		ui.editMacro->setPlainText(QString());
	}

	// ensure that the appropriate buttons are enabled
	CommonDialog::UpdateButtonStates(&ui, model_, current);
}

/**
 * @brief handles the "Check" button click event.
 */
void DialogMacros::buttonCheck_clicked() {
	if (validateFields(Verbosity::Verbose)) {
		QMessageBox::information(this,
								 tr("Macro"),
								 tr("Macro compiled without error"));
	}
}

/**
 * @brief Handles the "Apply" button click event.
 */
void DialogMacros::buttonApply_clicked() {
	applyDialogChanges();
}

/**
 * @brief Handles the "OK" button click event.
 */
void DialogMacros::buttonOK_clicked() {

	// Read the dialog fields, and update the menus
	if (!applyDialogChanges()) {
		return;
	}

	accept();
}

/**
 * @brief Validates the fields in the dialog.
 *
 * @param verbosity The verbosity level for error messages.
 * @return `true` if the fields are valid, `false` otherwise.
 */
bool DialogMacros::validateFields(Verbosity verbosity) {

	auto dialogFields = readFields(verbosity);
	if (!dialogFields) {
		return false;
	}

	if (!checkMacroText(dialogFields->cmd, verbosity)) {
		return false;
	}

	return true;
}

/**
 * @brief Read the name, accelerator, mnemonic, and command fields from the shell or
 * macro commands dialog into a newly allocated MenuItem.
 *
 * @param verbosity The verbosity level for error messages.
 * @return A MenuItem object if the fields are valid, or an empty optional if validation fails.
 */
std::optional<MenuItem> DialogMacros::readFields(Verbosity verbosity) {

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
 * @brief Checks if the macro text is valid.
 *
 * @param macro The macro text to check.
 * @param verbosity The verbosity level for error messages.
 * @return `true` if the macro text is valid, `false` otherwise.
 */
bool DialogMacros::checkMacroText(const QString &macro, Verbosity verbosity) {

	QString errMsg;
	int stoppedAt;
	if (!IsMacroValid(macro, &errMsg, &stoppedAt)) {
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
		return false;
	}

	return true;
}

/**
 * @brief Applies the changes made in the dialog to the macro menu data.
 *
 * @return `true` if the changes were successfully applied, `false` otherwise.
 */
bool DialogMacros::applyDialogChanges() {

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

	MacroMenuData = std::move(newItems);

	ParseMenuItemList(MacroMenuData);

	// Update the menus themselves in all of the NEdit windows
	for (MainWindow *window : MainWindow::allWindows()) {
		window->updateUserMenus();
	}

	// Note that preferences have been changed
	Preferences::MarkPrefsChanged();
	return true;
}

/**
 * @brief Updates a given item in the dialog based on the provided index.
 *
 * @param item The index of the item to update in the model.
 * @return `true` if the item was successfully updated, `false` otherwise.
 */
bool DialogMacros::updateItem(const QModelIndex &index) {

	auto dialogFields = readFields(Verbosity::Verbose);
	if (!dialogFields) {
		return false;
	}

	if (!index.isValid()) {
		return false;
	}

	model_->updateItem(index, *dialogFields);
	return true;
}

/**
 * @brief Updates the currently selected item in the dialog with the current field values.
 *
 * @return `true` if the item was successfully updated, `false` otherwise.
 */
bool DialogMacros::updateCurrentItem() {
	const QModelIndex index = ui.listItems->currentIndex();
	if (index.isValid()) {
		return updateItem(index);
	}

	return true;
}
