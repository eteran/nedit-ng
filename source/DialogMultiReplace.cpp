
#include "DialogMultiReplace.h"
#include "DocumentModel.h"
#include "DialogReplace.h"
#include "DocumentWidget.h"
#include "MainWindow.h"
#include "preferences.h"

#include <QMessageBox>

DialogMultiReplace::DialogMultiReplace(DialogReplace *replace, Qt::WindowFlags f) : Dialog(replace, f), replace_(replace) {
	ui.setupUi(this);

    model_ = new DocumentModel(this);
    ui.listFiles->setModel(model_);
}

void DialogMultiReplace::on_checkShowPaths_toggled(bool checked) {
    model_->setShowFullPath(checked);
}

void DialogMultiReplace::on_buttonDeselectAll_clicked() {
	ui.listFiles->clearSelection();
}

void DialogMultiReplace::on_buttonSelectAll_clicked() {
	ui.listFiles->selectAll();
}

void DialogMultiReplace::on_buttonReplace_clicked() {

    QString searchString;
    QString replaceString;
    Direction direction;
    SearchType searchType;

    QModelIndexList selections = ui.listFiles->selectionModel()->selectedRows();
    const int nSelected = selections.size();

	if (!nSelected) {
		QMessageBox::information(this, tr("No Files"), tr("No files selected!"));
		return; // Give the user another chance 
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

	if(res == QMessageBox::Cancel) {
		hide();
		return;
	}

    /* Fetch the find and replace strings from the dialog;
     * they should have been validated already, but it is possible that the
     * user modified the strings again, so we should verify them again too. */
    if (!replace_->getReplaceDlogInfo(&direction, &searchString, &replaceString, &searchType)) {
		return;
    }

	// Set the initial focus of the dialog back to the search string 
	replace_->ui.textFind->setFocus();

	bool replaceFailed = true;
	bool noWritableLeft = true;

    // Perform the replacements and mark the selected files (history)
    for(QModelIndex index : selections) {
        if(DocumentWidget *writableWin = model_->itemFromIndex(index)) {

            /* First check again whether the file is still writable. If the
             * file status has changed or the file was locked in the mean time,
             * we just skip the window. */
            if (!writableWin->lockReasons().isAnyLocked()) {
                noWritableLeft = false;
                writableWin->multiFileBusy_ = true; // Avoid multi-beep/dialog
                writableWin->replaceFailed_ = false;

                if(auto win = MainWindow::fromDocument(writableWin)) {
                    win->action_Replace_All(writableWin, searchString, replaceString, searchType);
                }

                writableWin->multiFileBusy_ = false;
                if (!writableWin->replaceFailed_) {
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
        if (Preferences::GetPrefSearchDlogs()) {
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
void DialogMultiReplace::uploadFileListItems() {

    model_->clear();

    for(DocumentWidget *document : replace_->writableWindows_) {
        model_->addItem(document);
    }
}
