
#include <QMessageBox>
#include "DialogMultiReplace.h"
#include "DialogReplace.h"
#include "search.h"
#include "Document.h"
#include "preferences.h"

DialogMultiReplace::DialogMultiReplace(Document *window, DialogReplace *replace, QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f), window_(window), replace_(replace) {
	ui.setupUi(this);
}

DialogMultiReplace::~DialogMultiReplace() {
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

	char searchString[SEARCHMAX];
	char replaceString[SEARCHMAX];
	SearchDirection direction;
	int searchType;	

	int nSelected = ui.listFiles->selectedItems().size();

	if (!nSelected) {
		QMessageBox::information(this, tr("No Files"), tr("No files selected!"));
		return; // Give the user another chance 
	}

	// Set the initial focus of the dialog back to the search string 
	resetReplaceTabGroup(window_);

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
	if (!replace_->getReplaceDlogInfo(&direction, searchString, replaceString, &searchType))
		return;


	// Set the initial focus of the dialog back to the search string 
	resetReplaceTabGroup(window_);

	const char *params[4];
	params[0] = searchString;
	params[1] = replaceString;
	params[2] = searchTypeArg(searchType);

	bool replaceFailed = true;
	bool noWritableLeft = true;

	// Perform the replacements and mark the selected files (history) 
	for (int i = 0; i < window_->nWritableWindows_; ++i) {
		Document *writableWin = window_->writableWindows_[i];
		
		if(ui.listFiles->item(i)->isSelected()) {
		
			/* First check again whether the file is still writable. If the
			   file status has changed or the file was locked in the mean time
			   (possible due to Lesstif modal dialog bug), we just skip the
			   window. */
			if (!IS_ANY_LOCKED(writableWin->lockReasons_)) {
				noWritableLeft = false;
				writableWin->multiFileReplSelected_ = true;
				writableWin->multiFileBusy_ = true; // Avoid multi-beep/dialog 
				writableWin->replaceFailed_ = false;
				XtCallActionProc(writableWin->lastFocus_, "replace_all", nullptr /*callData->event*/, const_cast<char **>(params), 3);
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

	int nWritable = window_->nWritableWindows_;

	QStringList names;

	bool usePathNames = ui.checkShowPaths->isChecked();
	
	
	if(replace) {
		// we want to maintain selections, we are replacing the 
		// existing items with equivalent items but with (possibly)
		// updated names
		for(int i = 0; i < ui.listFiles->count(); ++i) {
	    	QListWidgetItem* item = ui.listFiles->item(i);
			auto w = reinterpret_cast<Document *>(item->data(Qt::UserRole).toULongLong());
			if (usePathNames && window_->filenameSet_) {
				item->setText(w->FullPath());
			} else {
				item->setText(w->filename_);
			}				
		}
		
	} else {
		/* Note: the windows are sorted alphabetically by _file_ name. This
	        	 order is _not_ changed when we switch to path names. That
	        	 would be confusing for the user */

		for (int i = 0; i < nWritable; ++i) {
			Document *w = window_->writableWindows_[i];

			QListWidgetItem *item;
			if (usePathNames && window_->filenameSet_) {
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
