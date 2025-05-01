
#include "DialogShellMenu.h"
#include "CommonDialog.h"
#include "MainWindow.h"
#include "MenuData.h"
#include "MenuItem.h"
#include "MenuItemModel.h"
#include "Preferences.h"
#include "UserCommands.h"

#include <QMessageBox>

/**
 * @brief Constructor for the DialogShellMenu class.
 *
 * @param parent The parent widget for this dialog, defaults to nullptr.
 * @param f The window flags for the dialog, defaults to Qt::WindowFlags().
 */
DialogShellMenu::DialogShellMenu(QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f) {

	ui.setupUi(this);
	connectSlots();

	CommonDialog::SetButtonIcons(&ui);

	ui.editAccelerator->setMaximumSequenceLength(1);

	model_ = new MenuItemModel(this);
	ui.listItems->setModel(model_);

	// Copy the list of menu information to one that the user can freely edit
	for (const MenuData &menuData : ShellMenuData) {
		model_->addItem(menuData.item);
	}

	connect(ui.listItems->selectionModel(), &QItemSelectionModel::currentChanged, this, &DialogShellMenu::currentChanged, Qt::QueuedConnection);
	connect(this, &DialogShellMenu::restore, ui.listItems, &QListView::setCurrentIndex, Qt::QueuedConnection);

	// default to selecting the first item
	if (model_->rowCount() != 0) {
		const QModelIndex index = model_->index(0, 0);
		ui.listItems->setCurrentIndex(index);
	}
}

/**
 * @brief Connects the slots for the dialog's buttons and other UI elements.
 */
void DialogShellMenu::connectSlots() {
	connect(ui.buttonNew, &QPushButton::clicked, this, &DialogShellMenu::buttonNew_clicked);
	connect(ui.buttonCopy, &QPushButton::clicked, this, &DialogShellMenu::buttonCopy_clicked);
	connect(ui.buttonDelete, &QPushButton::clicked, this, &DialogShellMenu::buttonDelete_clicked);
	connect(ui.buttonUp, &QPushButton::clicked, this, &DialogShellMenu::buttonUp_clicked);
	connect(ui.buttonDown, &QPushButton::clicked, this, &DialogShellMenu::buttonDown_clicked);
	connect(ui.buttonBox, &QDialogButtonBox::accepted, this, &DialogShellMenu::buttonBox_accepted);
	connect(ui.buttonBox, &QDialogButtonBox::clicked, this, &DialogShellMenu::buttonBox_clicked);
	connect(ui.radioToSameDocument, &QRadioButton::toggled, this, &DialogShellMenu::radioToSameDocument_toggled);
}

/**
 * @brief Handles the toggling of the "New" radio button.
 */
void DialogShellMenu::buttonNew_clicked() {

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
void DialogShellMenu::buttonCopy_clicked() {

	if (!updateCurrentItem()) {
		return;
	}

	CommonDialog::CopyItem(&ui, model_);
}

/**
 * @brief Handles the "Delete" button click event.
 */
void DialogShellMenu::buttonDelete_clicked() {
	CommonDialog::DeleteItem(&ui, model_, &deleted_);
}

/**
 * @brief Moves the currently selected item up in the model and updates the UI.
 */
void DialogShellMenu::buttonUp_clicked() {
	CommonDialog::MoveItemUp(&ui, model_);
}

/**
 * @brief Moves the currently selected item down in the model and updates the UI.
 */
void DialogShellMenu::buttonDown_clicked() {
	CommonDialog::MoveItemDown(&ui, model_);
}

/**
 * @brief Handles the change of the current item in the list view.
 *
 * @param current The current index in the list view.
 * @param previous The previous index in the list view.
 */
void DialogShellMenu::currentChanged(const QModelIndex &current, const QModelIndex &previous) {
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

			// again to cause   to pop up
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
		ui.editCommand->setPlainText(ptr->cmd);

		switch (ptr->input) {
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

		switch (ptr->output) {
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
	CommonDialog::UpdateButtonStates(&ui, model_, current);
}

/**
 * @brief Handles the click event for the button box in the dialog.
 *
 * @param button The button that was clicked in the button box.
 */
void DialogShellMenu::buttonBox_clicked(QAbstractButton *button) {
	if (ui.buttonBox->standardButton(button) == QDialogButtonBox::Apply) {
		applyDialogChanges();
	}
}

/**
 * @brief Handles the "Accept" button click event in the dialog.
 */
void DialogShellMenu::buttonBox_accepted() {
	// Read the dialog fields, and update the menus
	if (!applyDialogChanges()) {
		return;
	}

	accept();
}

/*

*/

/**
 * @brief Read the name, accelerator, mnemonic, and command fields from the shell or
 * macro commands dialog into a MenuItem.
 *
 * @param verbosity The verbosity level for error messages.
 * @return the new MenuItem structure as the function value, or an empty optional if the fields are invalid.
 */
std::optional<MenuItem> DialogShellMenu::readFields(Verbosity verbosity) {

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

	const QString cmdText = ui.editCommand->toPlainText();
	if (cmdText.isEmpty()) {
		if (verbosity == Verbosity::Verbose) {
			QMessageBox::warning(this, tr("Command to Execute"), tr("Please specify macro command(s) to execute"));
		}
		return {};
	}

	MenuItem menuItem;
	menuItem.name     = nameText;
	menuItem.cmd      = cmdText;
	menuItem.shortcut = ui.editAccelerator->keySequence();

	if (ui.radioFromSelection->isChecked()) {
		menuItem.input = FROM_SELECTION;
	} else if (ui.radioFromDocument->isChecked()) {
		menuItem.input = FROM_WINDOW;
	} else if (ui.radioFromEither->isChecked()) {
		menuItem.input = FROM_EITHER;
	} else if (ui.radioFromNone->isChecked()) {
		menuItem.input = FROM_NONE;
	}

	if (ui.radioToSameDocument->isChecked()) {
		menuItem.output = TO_SAME_WINDOW;
	} else if (ui.radioToDialog->isChecked()) {
		menuItem.output = TO_DIALOG;
	} else if (ui.radioToNewDocument->isChecked()) {
		menuItem.output = TO_NEW_WINDOW;
	}

	menuItem.repInput  = ui.checkReplaceInput->isChecked();
	menuItem.saveFirst = ui.checkSaveBeforeExec->isChecked();
	menuItem.loadAfter = ui.checkReloadAfterExec->isChecked();

	return menuItem;
}

/**
 * @brief Applies the changes made in the dialog to the model and updates the menus.
 *
 * @return `true` if the changes were successfully applied, `false` otherwise.
 */
bool DialogShellMenu::applyDialogChanges() {

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

	ShellMenuData = std::move(newItems);

	ParseMenuItemList(ShellMenuData);

	// Update the menus themselves in all of the NEdit windows
	for (MainWindow *window : MainWindow::allWindows()) {
		window->updateUserMenus();
	}

	// Note that preferences have been changed
	Preferences::MarkPrefsChanged();
	return true;
}

/**
 * @brief Handles the toggling of the "To Same Document" radio button.
 *
 * @param checked Indicates whether the radio button is checked or not.
 */
void DialogShellMenu::radioToSameDocument_toggled(bool checked) {
	ui.checkReplaceInput->setEnabled(checked);
}

/**
 * @brief Updates an item in the model with the current dialog fields.
 *
 * @param index The QModelIndex of the item to update.
 * @return `true` if the item was successfully updated, `false` otherwise.
 */
bool DialogShellMenu::updateItem(const QModelIndex &index) {
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
 * @brief Updates the currently selected item in the dialog.
 *
 * @return `true` if the item was successfully updated, `false` otherwise.
 */
bool DialogShellMenu::updateCurrentItem() {
	const QModelIndex index = ui.listItems->currentIndex();
	if (index.isValid()) {
		return updateItem(index);
	}

	return true;
}

/**
 * @brief Validates the fields in the dialog and returns whether they are valid.
 *
 * @param verbosity The verbosity level for error messages.
 * @return `true` if the fields are valid, `false` otherwise.
 */
bool DialogShellMenu::validateFields(Verbosity verbosity) {
	if (readFields(verbosity)) {
		return true;
	}

	return false;
}
