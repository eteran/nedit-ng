
#include "DialogMoveDocument.h"
#include "MainWindow.h"

/**
 * @brief Constructor for DialogMoveDocument class.
 *
 * @param parent The parent widget, defaults to nullptr.
 * @param f The window flags, defaults to Qt::WindowFlags().
 */
DialogMoveDocument::DialogMoveDocument(QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f) {
	ui.setupUi(this);

	connect(ui.listWindows, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
		Q_UNUSED(item)
		accept();
	});
}

/**
 * @brief Adds a MainWindow item to the dialog's list.
 *
 * @param window The MainWindow instance to add.
 */
void DialogMoveDocument::addItem(MainWindow *window) {
	windows_.push_back(window);
	ui.listWindows->addItem(window->windowTitle());
}

/**
 * @brief Resets the selection in the dialog's list to the first item.
 */
void DialogMoveDocument::resetSelection() {
	ui.listWindows->setCurrentRow(0);
}

/**
 * @brief Sets the label text in the dialog.
 *
 * @param label The label text to set in the dialog.
 */
void DialogMoveDocument::setLabel(const QString &label) {
	ui.label->setText(tr("Move %1 into window of").arg(label));
}

/**
 * @brief Sets whether the dialog should allow moving multiple documents.
 *
 * @param value If `true`, the dialog will show the checkbox to move all selected documents;
 *              if `false`, it will hide that checkbox.
 */
void DialogMoveDocument::setMultipleDocuments(bool value) {
	ui.checkMoveAll->setVisible(value);
}

/**
 * @brief Returns the index of the currently selected item in the dialog's list.
 *
 * @return The index of the currently selected item, or -1 if no item is selected.
 */
int DialogMoveDocument::selectionIndex() const {
	return ui.listWindows->currentRow();
}

/**
 * @brief Returns whether the dialog should move all selected documents.
 *
 * @return `true` if the checkbox to move all selected documents is checked, `false` otherwise.
 */
bool DialogMoveDocument::moveAllSelected() const {
	return ui.checkMoveAll->isChecked();
}
