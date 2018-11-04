
#include "DialogMoveDocument.h"
#include "MainWindow.h"

/**
 * @brief DialogMoveDocument::DialogMoveDocument
 * @param parent
 * @param f
 */
DialogMoveDocument::DialogMoveDocument(QWidget *parent, Qt::WindowFlags f) : Dialog(parent, f) {
	ui.setupUi(this);
}

/**
 * @brief DialogMoveDocument::addItem
 * @param window
 */
void DialogMoveDocument::addItem(MainWindow *window) {
	windows_.push_back(window);
	ui.listWindows->addItem(window->windowTitle());
}

/**
 * @brief DialogMoveDocument::resetSelection
 */
void DialogMoveDocument::resetSelection() {
	ui.listWindows->setCurrentRow(0);
}

/**
 * @brief DialogMoveDocument::setLabel
 * @param label
 */
void DialogMoveDocument::setLabel(const QString &label) {
	ui.label->setText(tr("Move %1 into window of").arg(label));
}

/**
 * @brief DialogMoveDocument::setMultipleDocuments
 * @param value
 */
void DialogMoveDocument::setMultipleDocuments(bool value) {
	ui.checkMoveAll->setVisible(value);
}

/**
 * @brief DialogMoveDocument::selectionIndex
 * @return
 */
int DialogMoveDocument::selectionIndex() const {
	return ui.listWindows->currentRow();
}

/**
 * @brief DialogMoveDocument::moveAllSelected
 * @return
 */
bool DialogMoveDocument::moveAllSelected() const {
	return ui.checkMoveAll->isChecked();
}

/**
 * @brief DialogMoveDocument::on_listWindows_itemDoubleClicked
 * @param item
 */
void DialogMoveDocument::on_listWindows_itemDoubleClicked(QListWidgetItem *item) {

	Q_UNUSED(item);
	accept();

}
