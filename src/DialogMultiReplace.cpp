
#include "DialogMultiReplace.h"
#include "DialogReplace.h"
#include "DocumentModel.h"
#include "DocumentWidget.h"
#include "MainWindow.h"
#include "Preferences.h"
#include <QMessageBox>

/**
 * @brief DialogMultiReplace::DialogMultiReplace
 * @param replace
 * @param f
 */
DialogMultiReplace::DialogMultiReplace(DialogReplace *replace, Qt::WindowFlags f)
	: Dialog(replace, f), replace_(replace) {
	ui.setupUi(this);
	connectSlots();

	model_ = new DocumentModel(this);
	ui.listFiles->setModel(model_);
}

/**
 * @brief DialogMultiReplace::connectSlots
 */
void DialogMultiReplace::connectSlots() {
	connect(ui.buttonDeselectAll, &QPushButton::clicked, this, &DialogMultiReplace::buttonDeselectAll_clicked);
	connect(ui.buttonSelectAll, &QPushButton::clicked, this, &DialogMultiReplace::buttonSelectAll_clicked);
	connect(ui.buttonReplace, &QPushButton::clicked, this, &DialogMultiReplace::buttonReplace_clicked);
	connect(ui.checkShowPaths, &QCheckBox::toggled, this, &DialogMultiReplace::checkShowPaths_toggled);
}

/**
 * @brief DialogMultiReplace::checkShowPaths_toggled
 * @param checked
 */
void DialogMultiReplace::checkShowPaths_toggled(bool checked) {
	model_->setShowFullPath(checked);
}

/**
 * @brief DialogMultiReplace::buttonDeselectAll_clicked
 */
void DialogMultiReplace::buttonDeselectAll_clicked() {
	ui.listFiles->clearSelection();
}

/**
 * @brief DialogMultiReplace::buttonSelectAll_clicked
 */
void DialogMultiReplace::buttonSelectAll_clicked() {
	ui.listFiles->selectAll();
}

/**
 * @brief DialogMultiReplace::buttonReplace_clicked
 */
void DialogMultiReplace::buttonReplace_clicked() {

	const QModelIndexList selections = ui.listFiles->selectionModel()->selectedRows();
	const int nSelected              = selections.size();

	if (!nSelected) {
		QMessageBox::information(this, tr("No Files"), tr("No files selected!"));
		return;
	}

	// Set the initial focus of the dialog back to the search string
	replace_->ui.textFind->setFocus();

	/*
	 * Protect the user against him/herself; Maybe this is a bit too much?
	 */
	int res = QMessageBox::question(
		this,
		tr("Multi-File Replacement"),
		tr("Multi-file replacements are difficult to undo. Proceed with the replacement?"),
		QMessageBox::Yes | QMessageBox::Cancel);

	if (res == QMessageBox::Cancel) {
		hide();
		return;
	}

	/* Fetch the find and replace strings from the dialog;
	 * they should have been validated already, but it is possible that the
	 * user modified the strings again, so we should verify them again too. */
	std::optional<DialogReplace::Fields> fields = replace_->readFields();
	if (!fields) {
		return;
	}

	// Set the initial focus of the dialog back to the search string
	replace_->ui.textFind->setFocus();

	bool replaceFailed  = true;
	bool noWritableLeft = true;

	// Perform the replacements and mark the selected files (history)
	for (QModelIndex index : selections) {
		if (DocumentWidget *writeableDocument = model_->itemFromIndex(index)) {

			/* First check again whether the file is still writable. If the
			 * file status has changed or the file was locked in the mean time,
			 * we just skip the window. */
			if (!writeableDocument->lockReasons().isAnyLocked()) {
				noWritableLeft                    = false;
				writeableDocument->multiFileBusy_ = true; // Avoid multi-beep/dialog
				writeableDocument->replaceFailed_ = false;

				if (MainWindow *win = MainWindow::fromDocument(writeableDocument)) {
					win->action_Replace_All(
						writeableDocument,
						fields->searchString,
						fields->replaceString,
						fields->searchType);
				}

				writeableDocument->multiFileBusy_ = false;
				if (!writeableDocument->replaceFailed_) {
					replaceFailed = false;
				}
			}
		}
	}

	if (!replace_->keepDialog()) {
		replace_->hide();
	}

	hide();

	/* We suppressed multiple beeps/dialogs. If there wasn't any file in
	   which the replacement succeeded, we should still warn the user */
	if (replaceFailed) {
		if (Preferences::GetPrefSearchDialogs()) {
			if (noWritableLeft) {
				QMessageBox::information(this, tr("Read-only Files"), tr("All selected files have become read-only."));
			} else {
				QMessageBox::information(this, tr("String not found"), tr("String was not found"));
			}
		} else {
			QApplication::beep();
		}
	}
}

/**
 * @brief DialogMultiReplace::uploadFileListItems
 */
void DialogMultiReplace::uploadFileListItems(const std::vector<DocumentWidget *> &writeableDocuments) const {

	model_->clear();

	for (DocumentWidget *document : writeableDocuments) {
		model_->addItem(document);
	}
}
