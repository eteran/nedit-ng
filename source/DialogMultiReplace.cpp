
#include "DialogMultiReplace.h"
#include "DialogReplace.h"
#include "DocumentWidget.h"
#include "MainWindow.h"
#include "nedit.h"
#include "preferences.h"
#include "search.h"
#include <QMessageBox>

DialogMultiReplace::DialogMultiReplace(MainWindow *window, DocumentWidget *document, DialogReplace *replace, QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f), window_(window), document_(document), replace_(replace) {
	ui.setupUi(this);
}

DialogMultiReplace::~DialogMultiReplace() {
}

void DialogMultiReplace::showEvent(QShowEvent *event) {
    Q_UNUSED(event);
    centerDialog(this);
}

void DialogMultiReplace::on_checkShowPaths_toggled(bool checked) {
	Q_UNUSED(checked);
	uploadFileListItems(true);
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
	SearchDirection direction;
	SearchType searchType;	

	int nSelected = ui.listFiles->selectedItems().size();

	if (!nSelected) {
		QMessageBox::information(this, tr("No Files"), tr("No files selected!"));
		return; // Give the user another chance 
	}

	// Set the initial focus of the dialog back to the search string 
	replace_->ui.textFind->setFocus();

	/*
	 * Protect the user against him/herself; Maybe this is a bit too much?
	 */
	 
	int res = QMessageBox::question(this, tr("Multi-File Replacement"), tr("Multi-file replacements are difficult to undo. Proceed with the replacement ?"), QMessageBox::Yes, QMessageBox::Cancel);
	if(res == QMessageBox::Cancel) {
		// pop down the multi-file dialog only 
		hide();
		return;
	}


	/* Fetch the find and replace strings from the dialog;
	   they should have been validated already, but since Lesstif may not
	   honor modal dialogs, it is possible that the user modified the
	   strings again, so we should verify them again too. */
    if (!replace_->getReplaceDlogInfo(&direction, &searchString, &replaceString, &searchType))
		return;


	// Set the initial focus of the dialog back to the search string 
	replace_->ui.textFind->setFocus();

	bool replaceFailed = true;
	bool noWritableLeft = true;

    // Perform the replacements and mark the selected files (history)
    for (int i = 0; i < window_->writableWindows_.size(); ++i) {
        DocumentWidget *writableWin = window_->writableWindows_[i];

		if(ui.listFiles->item(i)->isSelected()) {
		
			/* First check again whether the file is still writable. If the
			   file status has changed or the file was locked in the mean time
			   (possible due to Lesstif modal dialog bug), we just skip the
			   window. */
			if (!writableWin->lockReasons_.isAnyLocked()) {
				noWritableLeft = false;
				writableWin->multiFileReplSelected_ = true;
				writableWin->multiFileBusy_ = true; // Avoid multi-beep/dialog 
				writableWin->replaceFailed_ = false;
                writableWin->replaceAllAP(searchString, replaceString, searchType);
				writableWin->multiFileBusy_ = false;
				if (!writableWin->replaceFailed_) {
					replaceFailed = false;
				}
			}			
		} else {
			writableWin->multiFileReplSelected_ = false;
		}

	}

	if (!replace_->keepDialog()) {
		// Pop down both replace dialogs. 
		replace_->hide();
		hide();
	} else {
		// pow down only the file selection dialog
		hide();
	}


	/* We suppressed multiple beeps/dialogs. If there wasn't any file in
	   which the replacement succeeded, we should still warn the user */
	if (replaceFailed) {
		if (GetPrefSearchDlogs()) {
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


/*
 * Uploads the file items to the multi-file replament dialog list.
 * A boolean argument indicates whether the elements currently in the
 * list have to be replaced or not.
 * Depending on the state of the "Show path names" toggle button, either
 * the file names or the path names are listed.
 */
void DialogMultiReplace::uploadFileListItems(bool replace) {

	bool usePathNames = ui.checkShowPaths->isChecked();
	
    // NOTE(eteran): so replace seems to mean that we want to keep
    //               existing items and possible update them "replacing" their
    //               strings with new ones, when not in replace mode, I think the
    //               whole list is supposed to be new
	if(replace) {
		// we want to maintain selections, we are replacing the 
		// existing items with equivalent items but with (possibly)
		// updated names
		for(int i = 0; i < ui.listFiles->count(); ++i) {
	    	QListWidgetItem* item = ui.listFiles->item(i);
            auto w = reinterpret_cast<DocumentWidget *>(item->data(Qt::UserRole).toULongLong());
            if (usePathNames && w->filenameSet_) {
				item->setText(w->FullPath());
			} else {
				item->setText(w->filename_);
			}				
		}
		
	} else {
		/* Note: the windows are sorted alphabetically by _file_ name. This
	        	 order is _not_ changed when we switch to path names. That
	        	 would be confusing for the user */

        ui.listFiles->clear();

        for (int i = 0; i < window_->writableWindows_.size(); ++i) {
            DocumentWidget *w = window_->writableWindows_[i];

			QListWidgetItem *item;
            if (usePathNames && w->filenameSet_) {
				item = new QListWidgetItem(w->FullPath());
			} else {
				item = new QListWidgetItem(w->filename_);
			}
			
			item->setData(Qt::UserRole, reinterpret_cast<qulonglong>(w));
			ui.listFiles->addItem(item);
		}
		
		if(ui.listFiles->selectedItems().isEmpty()) {
			ui.listFiles->selectAll();
		}		
	}
}
