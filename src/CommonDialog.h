
#ifndef COMMON_DIALOG_H_
#define COMMON_DIALOG_H_

#include <QIcon>
#include <QLatin1String>
#include <QModelIndex>

namespace CommonDialog {

/**
 * @brief setButtonIcons
 * @param ui
 */
template <class Ui>
void setButtonIcons(Ui *ui) {
	ui->buttonNew->setIcon(QIcon::fromTheme(QLatin1String("document-new")));
	ui->buttonDelete->setIcon(QIcon::fromTheme(QLatin1String("edit-delete")));
	ui->buttonCopy->setIcon(QIcon::fromTheme(QLatin1String("edit-copy")));
	ui->buttonUp->setIcon(QIcon::fromTheme(QLatin1String("go-up")));
	ui->buttonDown->setIcon(QIcon::fromTheme(QLatin1String("go-down")));
}

/**
 * @brief updateButtonStates
 * @param ui
 * @param model
 * @param current
 */
template <class Ui, class Model>
void updateButtonStates(Ui *ui, Model *model, const QModelIndex &current) {
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
 * @brief updateButtonStates
 * @param ui
 * @param model
 */
template <class Ui, class Model>
void updateButtonStates(Ui *ui, Model *model) {
	QModelIndex index = ui->listItems->currentIndex();
	updateButtonStates(ui, model, index);
}

/**
 * @brief addNewItem
 * @param ui
 * @param model
 * @param func
 */
template <class Ui, class Model, class Func>
void addNewItem(Ui *ui, Model *model, Func func) {

	model->addItem(func());

	QModelIndex index = model->index(model->rowCount() - 1, 0);
	ui->listItems->setCurrentIndex(index);

	ui->listItems->scrollTo(ui->listItems->currentIndex());
	updateButtonStates(ui, model);
}

/**
 * @brief deleteItem
 * @param ui
 * @param model
 * @param deleted
 */
template <class Ui, class Model>
void deleteItem(Ui *ui, Model *model, QModelIndex *deleted) {
	QModelIndex index = ui->listItems->currentIndex();
	if (index.isValid()) {
		*deleted = index;
		model->deleteItem(index);
	}

	ui->listItems->scrollTo(ui->listItems->currentIndex());
	CommonDialog::updateButtonStates(ui, model);
}

/**
 * @brief copyItem
 * @param ui
 * @param model
 */
template <class Ui, class Model>
void copyItem(Ui *ui, Model *model) {

	QModelIndex index = ui->listItems->currentIndex();
	if (index.isValid()) {
		auto ptr = model->itemFromIndex(index);
		model->addItem(*ptr);

		QModelIndex newIndex = model->index(model->rowCount() - 1, 0);
		ui->listItems->setCurrentIndex(newIndex);
	}

	ui->listItems->scrollTo(ui->listItems->currentIndex());
	updateButtonStates(ui, model);
}

/**
 * @brief moveItemUp
 * @param ui
 * @param model
 */
template <class Ui, class Model>
void moveItemUp(Ui *ui, Model *model) {
	QModelIndex index = ui->listItems->currentIndex();
	if (index.isValid()) {
		model->moveItemUp(index);
	}

	ui->listItems->scrollTo(ui->listItems->currentIndex());
	updateButtonStates(ui, model);
}

/**
 * @brief moveItemDown
 * @param ui
 * @param model
 */
template <class Ui, class Model>
void moveItemDown(Ui *ui, Model *model) {
	QModelIndex index = ui->listItems->currentIndex();
	if (index.isValid()) {
		model->moveItemDown(index);
	}

	ui->listItems->scrollTo(ui->listItems->currentIndex());
	updateButtonStates(ui, model);
}

}

#endif
