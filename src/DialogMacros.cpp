
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
#include "Util/String.h"
#include "parse.h"
#include "userCmds.h"

#include <QMessageBox>

/**
 * @brief DialogMacros::DialogMacros
 * @param parent
 * @param f
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

	CommonDialog::setButtonIcons(&ui);

	ui.editAccelerator->setMaximumSequenceLength(1);

	ui.buttonPasteLRMacro->setEnabled(!CommandRecorder::instance()->replayMacro().isEmpty());

	model_ = new MenuItemModel(this);
	ui.listItems->setModel(model_);

	// Copy the list of menu information to one that the user can freely edit
	for (MenuData &menuData : MacroMenuData) {
		model_->addItem(menuData.item);
	}

	connect(ui.listItems->selectionModel(), &QItemSelectionModel::currentChanged, this, &DialogMacros::currentChanged, Qt::QueuedConnection);
	connect(this, &DialogMacros::restore, ui.listItems, &QListView::setCurrentIndex, Qt::QueuedConnection);

	// default to selecting the first item
	if (model_->rowCount() != 0) {
		QModelIndex index = model_->index(0, 0);
		ui.listItems->setCurrentIndex(index);
	}
}

/**
 * @brief DialogMacros::connectSlots
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
 * @brief DialogMacros::buttonNew_clicked
 */
void DialogMacros::buttonNew_clicked() {

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
 * @brief DialogMacros::buttonCopy_clicked
 */
void DialogMacros::buttonCopy_clicked() {

	if (!updateCurrentItem()) {
		return;
	}

	CommonDialog::copyItem(&ui, model_);
}

/**
 * @brief DialogMacros::buttonDelete_clicked
 */
void DialogMacros::buttonDelete_clicked() {
	CommonDialog::deleteItem(&ui, model_, &deleted_);
}

/**
 * @brief DialogMacros::buttonPasteLRMacro_clicked
 */
void DialogMacros::buttonPasteLRMacro_clicked() {

	QString replayMacro = CommandRecorder::instance()->replayMacro();
	if (replayMacro.isEmpty()) {
		return;
	}

	ui.editMacro->insertPlainText(replayMacro);
}

/**
 * @brief DialogMacros::buttonUp_clicked
 */
void DialogMacros::buttonUp_clicked() {
	CommonDialog::moveItemUp(&ui, model_);
}

/**
 * @brief DialogMacros::buttonDown_clicked
 */
void DialogMacros::buttonDown_clicked() {
	CommonDialog::moveItemDown(&ui, model_);
}

/**
 * @brief DialogMacros::currentChanged
 * @param current
 * @param previous
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
		} else if (messageBox.clickedButton() == buttonDiscard) {
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
		ui.editAccelerator->clear();
		ui.checkRequiresSelection->setChecked(false);
		ui.editMacro->setPlainText(QString());
	}

	// ensure that the appropriate buttons are enabled
	CommonDialog::updateButtonStates(&ui, model_, current);
}

/**
 * @brief DialogMacros::buttonCheck_clicked
 */
void DialogMacros::buttonCheck_clicked() {
	if (validateFields(Verbosity::Verbose)) {
		QMessageBox::information(this,
								 tr("Macro"),
								 tr("Macro compiled without error"));
	}
}

/**
 * @brief DialogMacros::buttonApply_clicked
 */
void DialogMacros::buttonApply_clicked() {
	applyDialogChanges();
}

/**
 * @brief DialogMacros::buttonOK_clicked
 */
void DialogMacros::buttonOK_clicked() {

	// Read the dialog fields, and update the menus
	if (!applyDialogChanges()) {
		return;
	}

	accept();
}

/**
 * @brief DialogMacros::validateFields
 * @param mode
 * @return
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

/*
** Read the name, accelerator, mnemonic, and command fields from the shell or
** macro commands dialog into a newly allocated MenuItem.  Returns a
** pointer to the new MenuItem structure as the function value, or nullptr on
** failure.
*/
boost::optional<MenuItem> DialogMacros::readFields(Verbosity verbosity) {

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
 * @brief DialogMacros::checkMacroText
 * @param macro
 * @param mode
 * @return
 */
bool DialogMacros::checkMacroText(const QString &macro, Verbosity verbosity) {

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
		return false;
	}

	return true;
}

/**
 * @brief DialogMacros::applyDialogChanges
 * @return
 */
bool DialogMacros::applyDialogChanges() {

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

	MacroMenuData = std::move(newItems);

	parse_menu_item_list(MacroMenuData);

	// Update the menus themselves in all of the NEdit windows
	for (MainWindow *window : MainWindow::allWindows()) {
		window->updateUserMenus();
	}

	// Note that preferences have been changed
	Preferences::MarkPrefsChanged();
	return true;
}

/**
 * @brief DialogMacros::updateCurrentItem
 * @param item
 * @return
 */
bool DialogMacros::updateCurrentItem(const QModelIndex &index) {

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
 * @brief DialogMacros::updateCurrentItem
 * @return
 */
bool DialogMacros::updateCurrentItem() {
	QModelIndex index = ui.listItems->currentIndex();
	if (index.isValid()) {
		return updateCurrentItem(index);
	}

	return true;
}
