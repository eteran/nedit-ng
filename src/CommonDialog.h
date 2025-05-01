
#ifndef COMMON_DIALOG_H_
#define COMMON_DIALOG_H_

#include <QIcon>
#include <QLatin1String>
#include <QModelIndex>

namespace CommonDialog {

/**
 * @brief Sets the icons for the buttons in a dialog.
 *
 * @param ui The UI object containing the buttons.
 */
template <class Ui>
void SetButtonIcons(Ui *ui) {
	ui->buttonNew->setIcon(QIcon::fromTheme(QLatin1String("document-new")));
	ui->buttonDelete->setIcon(QIcon::fromTheme(QLatin1String("edit-delete")));
	ui->buttonCopy->setIcon(QIcon::fromTheme(QLatin1String("edit-copy")));
	ui->buttonUp->setIcon(QIcon::fromTheme(QLatin1String("go-up")));
	ui->buttonDown->setIcon(QIcon::fromTheme(QLatin1String("go-down")));
}

/**
 * @brief Updates the states of the buttons based on the current selection in a model.
 * If the index is valid, the buttons will be enabled or disabled based on the position of the selected item.
 * If the index is invalid, all buttons will be disabled.
 *
 * @param ui The UI object containing the buttons.
 * @param model The model containing the items.
 * @param current The currently selected index in the model.
 */
template <class Ui, class Model>
void UpdateButtonStates(Ui *ui, Model *model, const QModelIndex &current) {
	if (current.isValid()) {
		if (current.row() == 0) {
			ui->buttonUp->setEnabled(false);
			ui->buttonDown->setEnabled(model->rowCount() > 1);
			ui->buttonDelete->setEnabled(true);
			ui->buttonCopy->setEnabled(true);
		} else if (current.row() == model->rowCount() - 1) {
			ui->buttonUp->setEnabled(true);
			ui->buttonDown->setEnabled(false);
			ui->buttonDelete->setEnabled(true);
			ui->buttonCopy->setEnabled(true);
		} else {
			ui->buttonUp->setEnabled(true);
			ui->buttonDown->setEnabled(true);
			ui->buttonDelete->setEnabled(true);
			ui->buttonCopy->setEnabled(true);
		}
	} else {
		ui->buttonUp->setEnabled(false);
		ui->buttonDown->setEnabled(false);
		ui->buttonDelete->setEnabled(false);
		ui->buttonCopy->setEnabled(false);
	}
}

/**
 * @brief Updates the button states based on the current selection in the model.
 *
 * @param ui The UI object containing the buttons.
 * @param model The model containing the items.
 */
template <class Ui, class Model>
void UpdateButtonStates(Ui *ui, Model *model) {
	QModelIndex index = ui->listItems->currentIndex();
	UpdateButtonStates(ui, model, index);
}

/**
 * @brief Adds a new item to the model and updates the UI.
 *
 * @param ui The UI object containing the model.
 * @param model The model where the item will be added.
 * @param func A function that returns a new item to be added to the model.
 */
template <class Ui, class Model, class Func>
void AddNewItem(Ui *ui, Model *model, Func func) {

	model->addItem(func());

	QModelIndex index = model->index(model->rowCount() - 1, 0);
	ui->listItems->setCurrentIndex(index);

	ui->listItems->scrollTo(ui->listItems->currentIndex());
	UpdateButtonStates(ui, model);
}

/**
 * @brief Deletes the currently selected item from the model and updates the UI.
 *
 * @param ui The UI object containing the model.
 * @param model The model from which the item will be deleted.
 * @param deleted The QModelIndex where the deleted item index will be stored.
 */
template <class Ui, class Model>
void DeleteItem(Ui *ui, Model *model, QModelIndex *deleted) {
	QModelIndex index = ui->listItems->currentIndex();
	if (index.isValid()) {

		const int deletedRow = index.row();

		*deleted = index;
		model->deleteItem(index);

		ui->listItems->setCurrentIndex(QModelIndex());

		QModelIndex newIndex = model->index(deletedRow, 0);
		ui->listItems->setCurrentIndex(newIndex);
	}

	ui->listItems->scrollTo(ui->listItems->currentIndex());
	UpdateButtonStates(ui, model);
}

/**
 * @brief Copies the currently selected item in the model and adds it as a new item.
 *
 * @param ui The UI object containing the model.
 * @param model The model from which the item will be copied.
 */
template <class Ui, class Model>
void CopyItem(Ui *ui, Model *model) {

	QModelIndex index = ui->listItems->currentIndex();
	if (index.isValid()) {
		auto ptr = model->itemFromIndex(index);
		model->addItem(*ptr);

		QModelIndex newIndex = model->index(model->rowCount() - 1, 0);
		ui->listItems->setCurrentIndex(newIndex);
	}

	ui->listItems->scrollTo(ui->listItems->currentIndex());
	UpdateButtonStates(ui, model);
}

/**
 * @brief Moves the currently selected item up in the model and updates the UI.
 *
 * @param ui The UI object containing the model.
 * @param model The model where the item will be moved.
 */
template <class Ui, class Model>
void MoveItemUp(Ui *ui, Model *model) {
	QModelIndex index = ui->listItems->currentIndex();
	if (index.isValid()) {
		model->moveItemUp(index);
	}

	ui->listItems->scrollTo(ui->listItems->currentIndex());
	UpdateButtonStates(ui, model);
}

/**
 * @brief Moves the currently selected item down in the model and updates the UI.
 *
 * @param ui The UI object containing the model.
 * @param model The model where the item will be moved.
 */
template <class Ui, class Model>
void MoveItemDown(Ui *ui, Model *model) {
	QModelIndex index = ui->listItems->currentIndex();
	if (index.isValid()) {
		model->moveItemDown(index);
	}

	ui->listItems->scrollTo(ui->listItems->currentIndex());
	UpdateButtonStates(ui, model);
}

}

#endif
