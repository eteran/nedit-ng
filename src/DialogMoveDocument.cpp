
#include "DialogMoveDocument.h"
#include "MainWindow.h"

/**
 * @brief
 *
 * @param parent
 * @param f
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
 * @brief
 *
 * @param window
 */
void DialogMoveDocument::addItem(MainWindow *window) {
	windows_.push_back(window);
	ui.listWindows->addItem(window->windowTitle());
}

/**
 * @brief
 */
void DialogMoveDocument::resetSelection() {
	ui.listWindows->setCurrentRow(0);
}

/**
 * @brief
 *
 * @param label
 */
void DialogMoveDocument::setLabel(const QString &label) {
	ui.label->setText(tr("Move %1 into window of").arg(label));
}

/**
 * @brief
 *
 * @param value
 */
void DialogMoveDocument::setMultipleDocuments(bool value) {
	ui.checkMoveAll->setVisible(value);
}

/**
 * @brief
 *
 * @return
 */
int DialogMoveDocument::selectionIndex() const {
	return ui.listWindows->currentRow();
}

/**
 * @brief
 *
 * @return
 */
bool DialogMoveDocument::moveAllSelected() const {
	return ui.checkMoveAll->isChecked();
}
