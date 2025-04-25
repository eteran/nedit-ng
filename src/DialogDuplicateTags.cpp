
#include "DialogDuplicateTags.h"
#include "DocumentWidget.h"

#include <QAbstractButton>
#include <QFontDatabase>

/**
 * @brief Constructor for DialogDuplicateTags class.
 *
 * @param document The DocumentWidget where the tags are defined.
 * @param area The TextArea where the tags are being used.
 * @param f The window flags for the dialog, defaults to Qt::WindowFlags().
 */
DialogDuplicateTags::DialogDuplicateTags(DocumentWidget *document, TextArea *area, Qt::WindowFlags f)
	: Dialog(document, f), document_(document), area_(area) {

	ui.setupUi(this);
	connectSlots();
}

/**
 * @brief Connects the slots for the dialog's buttons and other UI elements.
 */
void DialogDuplicateTags::connectSlots() {
	connect(ui.buttonBox, &QDialogButtonBox::clicked, this, &DialogDuplicateTags::buttonBox_clicked);
	connect(ui.buttonBox, &QDialogButtonBox::accepted, this, &DialogDuplicateTags::buttonBox_accepted);
}

/**
 * @brief Sets the tag for the dialog.
 *
 * @param tag The tag to be displayed in the dialog.
 */
void DialogDuplicateTags::setTag(const QString &tag) {
	ui.label->setText(tr("Select File With TAG: %1").arg(tag));
}

/**
 * @brief Adds an item to the list widget in the dialog.
 *
 * @param text The text to be displayed in the list item.
 * @param id The unique identifier for the item, stored in the item's user data.
 */
void DialogDuplicateTags::addListItem(const QString &text, int id) {
	auto item = new QListWidgetItem(text, ui.listWidget);
	item->setData(Qt::UserRole, id);
}

/**
 * @brief Handles the acceptance of the dialog.
 */
void DialogDuplicateTags::buttonBox_accepted() {
	if (applySelection()) {
		accept();
	}
}

/**
 * @brief Handles the button box click event.
 * Handles the Apply button click to apply changes without closing the dialog.
 *
 * @param button The button that was clicked in the button box.
 */
void DialogDuplicateTags::buttonBox_clicked(QAbstractButton *button) {
	if (ui.buttonBox->standardButton(button) == QDialogButtonBox::Apply) {
		applySelection();
	}
}

/**
 * @brief Applies the selection made in the dialog.
 *
 * @return `true` if the selection was successfully applied, `false` otherwise.
 */
bool DialogDuplicateTags::applySelection() {
	QListWidgetItem *item = ui.listWidget->currentItem();
	if (!item) {
		QApplication::beep();
		return false;
	}

	if (!document_) {
		QApplication::beep();
		return false;
	}

	const int id = item->data(Qt::UserRole).toInt();

	if (Tags::searchMode == Tags::SearchMode::TAG) {
		document_->editTaggedLocation(area_, id); // Open the file with the definition
	} else {
		Tags::ShowMatchingCalltip(this, area_, id);
	}

	return true;
}
