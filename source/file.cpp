/*******************************************************************************
*                                                                              *
* file.c -- Nirvana Editor file i/o                                            *
*                                                                              *
* Copyright (C) 1999 Mark Edel                                                 *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute versions of this program linked to  *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License        *
* for more details.                                                            *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* May 10, 1991                                                                 *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/

#include <QApplication>
#include <QBoxLayout>
#include <QButtonGroup>
#include <QCheckBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QtDebug>
#include "ui/DialogPrint.h"
#include "IndentStyle.h"
#include "WrapStyle.h"

#include "file.h"
#include "TextBuffer.h"
#include "text.h"
#include "window.h"
#include "preferences.h"
#include "menu.h"
#include "tags.h"
#include "server.h"
#include "interpret.h"
#include "Document.h"
#include "MotifHelper.h"
#include "misc.h"
#include "fileUtils.h"
#include "getfiles.h"
#include "utils.h"

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>

#include <Xm/Xm.h>
#include <Xm/ToggleB.h>
#include <Xm/FileSB.h>
#include <Xm/RowColumn.h>
#include <Xm/Form.h>
#include <Xm/Label.h>

/* Maximum frequency in miliseconds of checking for external modifications.
   The periodic check is only performed on buffer modification, and the check
   interval is only to prevent checking on every keystroke in case of a file
   system which is slow to process stat requests (which I'm not sure exists) */
#define MOD_CHECK_INTERVAL 3000

static bool writeBckVersion(Document *window);
static bool bckError(Document *window, const char *errString, const char *file);
static int cmpWinAgainstFile(Document *window, const char *fileName);
static int doOpen(Document *window, const char *name, const char *path, int flags);
static bool doSave(Document *window);
static int fileWasModifiedExternally(Document *window);
static void addWrapCB(Widget w, XtPointer clientData, XtPointer callData);
static void addWrapNewlines(Document *window);
static std::string backupFileNameEx(Document *window);
static void forceShowLineNumbers(Document *window);
static void modifiedWindowDestroyedCB(Widget w, XtPointer clientData, XtPointer callData);
static void safeClose(Document *window);
static void setFormatCB(Widget w, XtPointer clientData, XtPointer callData);

Document *EditNewFile(Document *inWindow, char *geometry, int iconic, const char *languageMode, const char *defaultPath) {
	char name[MAXPATHLEN];
	Document *window;

	/*... test for creatability? */

	// Find a (relatively) unique name for the new file 
	UniqueUntitledName(name, sizeof(name));

	// create new window/document 
	if (inWindow) {
		window = inWindow->CreateDocument(name);
	} else {
		window = new Document(name, geometry, iconic);
	}

	window->filename_ = name;
	window->path_     = (defaultPath && *defaultPath) ? defaultPath : GetCurrentDirEx().toStdString();
	
	// do we have a "/" at the end? if not, add one 
	if (!window->path_.empty() && window->path_.back() != '/') {
		window->path_.append("/");
	}

	window->SetWindowModified(FALSE);
	CLEAR_ALL_LOCKS(window->lockReasons_);
	window->UpdateWindowReadOnly();
	window->UpdateStatsLine();
	window->UpdateWindowTitle();
	window->RefreshTabState();

	if(!languageMode)
		DetermineLanguageMode(window, True);
	else
		SetLanguageMode(window, FindLanguageMode(languageMode), True);

	window->ShowTabBar(window->GetShowTabBar());

	if (iconic && window->IsIconic()) {
		window->RaiseDocument();
	} else {
		window->RaiseDocumentWindow();
	}

	window->SortTabBar();
	return window;
}

/*
** Open an existing file specified by name and path.  Use the window inWindow
** unless inWindow is nullptr or points to a window which is already in use
** (displays a file other than Untitled, or is Untitled but modified).  Flags
** can be any of:
**
**	CREATE: 		If file is not found, (optionally) prompt the
**				user whether to create
**	SUPPRESS_CREATE_WARN	When creating a file, don't ask the user
**	PREF_READ_ONLY		Make the file read-only regardless
**
** If languageMode is passed as nullptr, it will be determined automatically
** from the file extension or file contents.
**
** If bgOpen is True, then the file will be open in background. This
** works in association with the SetLanguageMode() function that has
** the syntax highlighting deferred, in order to speed up the file-
** opening operation when multiple files are being opened in succession.
*/
Document *EditExistingFile(Document *inWindow, const char *name, const char *path, int flags, char *geometry, int iconic, const char *languageMode, int tabbed, int bgOpen) {
	Document *window;
	
	// first look to see if file is already displayed in a window 
	window = FindWindowWithFile(name, path);
	if (window) {
		if (!bgOpen) {
			if (iconic)
				window->RaiseDocument();
			else
				window->RaiseDocumentWindow();
		}
		return window;
	}

	/* If an existing window isn't specified; or the window is already
	   in use (not Untitled or Untitled and modified), or is currently
	   busy running a macro; create the window */
	if(!inWindow) {
		window = new Document(name, geometry, iconic);
	} else if (inWindow->filenameSet_ || inWindow->fileChanged_ || inWindow->macroCmdData_) {
		if (tabbed) {
			window = inWindow->CreateDocument(name);
		} else {
			window = new Document(name, geometry, iconic);
		}
	} else {
		// open file in untitled document 
		window            = inWindow;
		window->path_     = path;
		window->filename_ = name;
		
		if (!iconic && !bgOpen) {
			window->RaiseDocumentWindow();
		}
	}

	// Open the file 
	if (!doOpen(window, name, path, flags)) {
		/* The user may have destroyed the window instead of closing the
		   warning dialog; don't close it twice */
		safeClose(window);

		return nullptr;
	}
	forceShowLineNumbers(window);

	// Decide what language mode to use, trigger language specific actions 
	if(!languageMode)
		DetermineLanguageMode(window, True);
	else
		SetLanguageMode(window, FindLanguageMode(languageMode), True);

	// update tab label and tooltip 
	window->RefreshTabState();
	window->SortTabBar();
	window->ShowTabBar(window->GetShowTabBar());

	if (!bgOpen)
		window->RaiseDocument();

	/* Bring the title bar and statistics line up to date, doOpen does
	   not necessarily set the window title or read-only status */
	window->UpdateWindowTitle();
	window->UpdateWindowReadOnly();
	window->UpdateStatsLine();

	// Add the name to the convenience menu of previously opened files 
	char fullname[MAXPATHLEN];
	snprintf(fullname, sizeof(fullname), "%s%s", path, name);
	
	if (GetPrefAlwaysCheckRelTagsSpecs()) {
		AddRelTagsFile(GetPrefTagFile(), path, TAG);
	}
	
	AddToPrevOpenMenu(fullname);

	return window;
}

void RevertToSaved(Document *window) {
	char name[MAXPATHLEN], path[MAXPATHLEN];
	int i;
	int insertPositions[MAX_PANES], topLines[MAX_PANES];
	int horizOffsets[MAX_PANES];
	int openFlags = 0;
	Widget text;

	// Can't revert untitled windows 
	if (!window->filenameSet_) {
		QMessageBox::warning(nullptr /*parent*/, QLatin1String("Error"), QString(QLatin1String("Window '%1' was never saved, can't re-read")).arg(QString::fromStdString(window->filename_)));
		return;
	}

	// save insert & scroll positions of all of the panes to restore later 
	for (i = 0; i <= window->nPanes_; i++) {
		text = i == 0 ? window->textArea_ : window->textPanes_[i - 1];
		insertPositions[i] = TextGetCursorPos(text);
		TextGetScroll(text, &topLines[i], &horizOffsets[i]);
	}

	// re-read the file, update the window title if new file is different 
	strcpy(name, window->filename_.c_str());
	strcpy(path, window->path_.c_str());
	
	RemoveBackupFile(window);
	window->ClearUndoList();
	openFlags |= IS_USER_LOCKED(window->lockReasons_) ? PREF_READ_ONLY : 0;
	if (!doOpen(window, name, path, openFlags)) {
		/* This is a bit sketchy.  The only error in doOpen that irreperably
		        damages the window is "too much binary data".  It should be
		        pretty rare to be reverting something that was fine only to find
		        that now it has too much binary data. */
		if (!window->fileMissing_)
			safeClose(window);
		else {
			// Treat it like an externally modified file 
			window->lastModTime_ = 0;
			window->fileMissing_ = FALSE;
		}
		return;
	}
	forceShowLineNumbers(window);
	window->UpdateWindowTitle();
	window->UpdateWindowReadOnly();

	// restore the insert and scroll positions of each pane 
	for (i = 0; i <= window->nPanes_; i++) {
		text = i == 0 ? window->textArea_ : window->textPanes_[i - 1];
		TextSetCursorPos(text, insertPositions[i]);
		TextSetScroll(text, topLines[i], horizOffsets[i]);
	}
}

/*
** Checks whether a window is still alive, and closes it only if so.
** Intended to be used when the file could not be opened for some reason.
** Normally the window is still alive, but the user may have closed the
** window instead of the error dialog. In that case, we shouldn't close the
** window a second time.
*/
static void safeClose(Document *window) {

	Document *win = Document::find_if([window](Document *p) {
		return p == window;
	});
	
	if(win) {
		win->CloseWindow();
	}
}

static int doOpen(Document *window, const char *name, const char *path, int flags) {

	struct stat statbuf;
	FILE *fp = nullptr;
	int fd;
	int resp;

	// initialize lock reasons 
	CLEAR_ALL_LOCKS(window->lockReasons_);

	// Update the window data structure 
	window->filename_    = name;
	window->path_        = path;
	window->filenameSet_ = TRUE;
	window->fileMissing_ = TRUE;

	// Get the full name of the file 
	const std::string fullname = window->FullPath();

	// Open the file 
	/* The only advantage of this is if you use clearcase,
	   which messes up the mtime of files opened with r+,
	   even if they're never actually written.
	   To avoid requiring special builds for clearcase users,
	   this is now the default.
	*/
	{
		if ((fp = fopen(fullname.c_str(), "r"))) {
			if (access(fullname.c_str(), W_OK) != 0) {
				SET_PERM_LOCKED(window->lockReasons_, TRUE);
			}

		} else if (flags & CREATE && errno == ENOENT) {
			// Give option to create (or to exit if this is the only window) 
			if (!(flags & SUPPRESS_CREATE_WARN)) {
				/* on Solaris 2.6, and possibly other OSes, dialog won't
		           show if parent window is iconized. */
				RaiseShellWindow(window->shell_, False);
				
				QMessageBox msgbox(nullptr /*parent*/);
				QAbstractButton  *exitButton;

				// ask user for next action if file not found 
				if (window == WindowList && window->next_ == nullptr) {
					
					msgbox.setIcon(QMessageBox::Warning);
					msgbox.setWindowTitle(QLatin1String("New File"));
					msgbox.setText(QString(QLatin1String("Can't open %1:\n%2")).arg(QString::fromStdString(fullname), QLatin1String(strerror(errno))));
					msgbox.addButton(QLatin1String("New File"), QMessageBox::AcceptRole);
					msgbox.addButton(QMessageBox::Cancel);
					exitButton = msgbox.addButton(QLatin1String("Exit NEdit"), QMessageBox::RejectRole);
					resp = msgbox.exec();
					
				} else {
				
					msgbox.setIcon(QMessageBox::Warning);
					msgbox.setWindowTitle(QLatin1String("New File"));
					msgbox.setText(QString(QLatin1String("Can't open %1:\n%2")).arg(QString::fromStdString(fullname), QLatin1String(strerror(errno))));
					msgbox.addButton(QLatin1String("New File"), QMessageBox::AcceptRole);
					msgbox.addButton(QMessageBox::Cancel);
					exitButton = nullptr;
					resp = msgbox.exec();				
				}

				if (resp == QMessageBox::Cancel) {
					return FALSE;
				} else if (msgbox.clickedButton() == exitButton) {
					exit(EXIT_SUCCESS);
				}
			}

			// Test if new file can be created 
			if ((fd = creat(fullname.c_str(), 0666)) == -1) {
				QMessageBox::critical(nullptr /*parent*/, QLatin1String("Error creating File"), QString(QLatin1String("Can't create %1:\n%2")).arg(QString::fromStdString(fullname), QLatin1String(strerror(errno))));
				return FALSE;
			} else {
				close(fd);
				remove(fullname.c_str());
			}

			window->SetWindowModified(FALSE);
			if ((flags & PREF_READ_ONLY) != 0) {
				SET_USER_LOCKED(window->lockReasons_, TRUE);
			}
			window->UpdateWindowReadOnly();
			return TRUE;
		} else {
			// A true error 
			QMessageBox::critical(nullptr /*parent*/, QLatin1String("Error opening File"), QString(QLatin1String("Could not open %1%2:\n%3")).arg(QLatin1String(path), QLatin1String(name), QLatin1String(strerror(errno))));
			return FALSE;
		}
	}

	/* Get the length of the file, the protection mode, and the time of the
	   last modification to the file */
	if (fstat(fileno(fp), &statbuf) != 0) {
		fclose(fp);
		window->filenameSet_ = FALSE; // Temp. prevent check for changes. 
		QMessageBox::critical(nullptr /*parent*/, QLatin1String("Error opening File"), QString(QLatin1String("Error opening %1")).arg(QLatin1String(name)));
		window->filenameSet_ = TRUE;
		return FALSE;
	}

	if (S_ISDIR(statbuf.st_mode)) {
		fclose(fp);
		window->filenameSet_ = FALSE; // Temp. prevent check for changes. 
		QMessageBox::critical(nullptr /*parent*/, QLatin1String("Error opening File"), QString(QLatin1String("Can't open directory %1")).arg(QLatin1String(name)));
		window->filenameSet_ = TRUE;
		return FALSE;
	}

#ifdef S_ISBLK
	if (S_ISBLK(statbuf.st_mode)) {
		fclose(fp);
		window->filenameSet_ = FALSE; // Temp. prevent check for changes. 
		QMessageBox::critical(nullptr /*parent*/, QLatin1String("Error opening File"), QString(QLatin1String("Can't open block device %1")).arg(QLatin1String(name)));
		window->filenameSet_ = TRUE;
		return FALSE;
	}
#endif

	int fileLen = statbuf.st_size;

	// Allocate space for the whole contents of the file (unfortunately) 
	auto fileString = new char[fileLen + 1]; // +1 = space for null 
	if(!fileString) {
		fclose(fp);
		window->filenameSet_ = FALSE; // Temp. prevent check for changes. 
		QMessageBox::critical(nullptr /*parent*/, QLatin1String("Error while opening File"), QLatin1String("File is too large to edit"));
		window->filenameSet_ = TRUE;
		return FALSE;
	}

	// Read the file into fileString and terminate with a null 
	int readLen = fread(fileString, sizeof(char), fileLen, fp);
	if (ferror(fp)) {
		fclose(fp);
		window->filenameSet_ = FALSE; // Temp. prevent check for changes. 
		QMessageBox::critical(nullptr /*parent*/, QLatin1String("Error while opening File"), QString(QLatin1String("Error reading %1:\n%2")).arg(QLatin1String(name), QLatin1String(strerror(errno))));
		window->filenameSet_ = TRUE;
		delete [] fileString;
		return FALSE;
	}
	fileString[readLen] = '\0';

	// Close the file 
	if (fclose(fp) != 0) {
		// unlikely error 
		QMessageBox::warning(nullptr /*parent*/, QLatin1String("Error while opening File"), QLatin1String("Unable to close file"));
		// we read it successfully, so continue 
	}

	/* Any errors that happen after this point leave the window in a
	    "broken" state, and thus RevertToSaved will abandon the window if
	    window->fileMissing_ is FALSE and doOpen fails. */
	window->fileMode_    = statbuf.st_mode;
	window->fileUid_     = statbuf.st_uid;
	window->fileGid_     = statbuf.st_gid;
	window->lastModTime_ = statbuf.st_mtime;
	window->device_      = statbuf.st_dev;
	window->inode_       = statbuf.st_ino;
	window->fileMissing_ = FALSE;

	// Detect and convert DOS and Macintosh format files 
	if (GetPrefForceOSConversion()) {
		window->fileFormat_ = FormatOfFileEx(view::string_view(fileString, readLen));
		if (window->fileFormat_ == DOS_FILE_FORMAT) {
			ConvertFromDosFileString(fileString, &readLen, nullptr);
		} else if (window->fileFormat_ == MAC_FILE_FORMAT) {
			ConvertFromMacFileString(fileString, readLen);
		}
	}

	// Display the file contents in the text widget 
	window->ignoreModify_ = True;
	window->buffer_->BufSetAllEx(view::string_view(fileString, readLen));
	window->ignoreModify_ = False;

	/* Check that the length that the buffer thinks it has is the same
	   as what we gave it.  If not, there were probably nuls in the file.
	   Substitute them with another character.  If that is impossible, warn
	   the user, make the file read-only, and force a substitution */
	if (window->buffer_->BufGetLength() != readLen) {
		if (!window->buffer_->BufSubstituteNullChars(fileString, readLen)) {
		
		
				
			QMessageBox msgbox(nullptr /*parent*/);
			msgbox.setIcon(QMessageBox::Critical);
			msgbox.setWindowTitle(QLatin1String("Error while opening File"));
			msgbox.setText(QLatin1String("Too much binary data in file.  You may view\nit, but not modify or re-save its contents."));
			msgbox.addButton(QLatin1String("View"), QMessageBox::AcceptRole);
			msgbox.addButton(QMessageBox::Cancel);
			int resp = msgbox.exec();
			
			if (resp == QMessageBox::Cancel) {
				return FALSE;
			}

			SET_TMBD_LOCKED(window->lockReasons_, TRUE);
			for (char *c = fileString; c < &fileString[readLen]; c++) {
				if (*c == '\0') {
					*c = (char)0xfe;
				}
			}
			window->buffer_->nullSubsChar_ = (char)0xfe;
		}
		window->ignoreModify_ = True;
		window->buffer_->BufSetAllEx(fileString);
		window->ignoreModify_ = False;
	}

	// Release the memory that holds fileString 
	delete [] fileString;

	// Set window title and file changed flag 
	if ((flags & PREF_READ_ONLY) != 0) {
		SET_USER_LOCKED(window->lockReasons_, TRUE);
	}
	if (IS_PERM_LOCKED(window->lockReasons_)) {
		window->fileChanged_ = FALSE;
		window->UpdateWindowTitle();
	} else {
		window->SetWindowModified(FALSE);
		if (IS_ANY_LOCKED(window->lockReasons_)) {
			window->UpdateWindowTitle();
		}
	}
	window->UpdateWindowReadOnly();

	return TRUE;
}

int IncludeFile(Document *window, const char *name) {
	struct stat statbuf;

	// Open the file 
	FILE *fp = fopen(name, "rb");
	if(!fp) {
		QMessageBox::critical(nullptr /*parent*/, QLatin1String("Error opening File"), QString(QLatin1String("Could not open %1:\n%2")).arg(QLatin1String(name), QLatin1String(strerror(errno))));
		return FALSE;
	}

	// Get the length of the file 
	if (fstat(fileno(fp), &statbuf) != 0) {
		QMessageBox::critical(nullptr /*parent*/, QLatin1String("Error opening File"), QString(QLatin1String("Error opening %1")).arg(QLatin1String(name)));
		fclose(fp);
		return FALSE;
	}

	if (S_ISDIR(statbuf.st_mode)) {
		QMessageBox::critical(nullptr /*parent*/, QLatin1String("Error opening File"), QString(QLatin1String("Can't open directory %1")).arg(QLatin1String(name)));
		fclose(fp);
		return FALSE;
	}
	int fileLen = statbuf.st_size;

	// allocate space for the whole contents of the file 
	auto fileString = new char[fileLen + 1]; // +1 = space for null 
	if(!fileString) {
		QMessageBox::critical(nullptr /*parent*/, QLatin1String("Error opening File"), QLatin1String("File is too large to include"));
		fclose(fp);
		return FALSE;
	}

	// read the file into fileString and terminate with a null 
	int readLen = fread(fileString, sizeof(char), fileLen, fp);
	if (ferror(fp)) {
		QMessageBox::critical(nullptr /*parent*/, QLatin1String("Error opening File"), QString(QLatin1String("Error reading %1:\n%2")).arg(QLatin1String(name), QLatin1String(strerror(errno))));
		fclose(fp);
		delete [] fileString;
		return FALSE;
	}
	fileString[readLen] = '\0';

	// Detect and convert DOS and Macintosh format files 
	switch (FormatOfFileEx(view::string_view(fileString, readLen))) {
	case DOS_FILE_FORMAT:
		ConvertFromDosFileString(fileString, &readLen, nullptr);
		break;
	case MAC_FILE_FORMAT:
		ConvertFromMacFileString(fileString, readLen);
		break;
	default:
		//  Default is Unix, no conversion necessary.  
		break;
	}

	// If the file contained ascii nulls, re-map them 
	if (!window->buffer_->BufSubstituteNullChars(fileString, readLen)) {
		QMessageBox::critical(nullptr /*parent*/, QLatin1String("Error opening File"), QLatin1String("Too much binary data in file"));
	}

	// close the file 
	if (fclose(fp) != 0) {
		// unlikely error 
		QMessageBox::warning(nullptr /*parent*/, QLatin1String("Error opening File"), QLatin1String("Unable to close file"));
		// we read it successfully, so continue 
	}

	/* insert the contents of the file in the selection or at the insert
	   position in the window if no selection exists */
	if (window->buffer_->primary_.selected) {
		window->buffer_->BufReplaceSelectedEx(view::string_view(fileString, readLen));
	} else {
		window->buffer_->BufInsertEx(TextGetCursorPos(window->lastFocus_), view::string_view(fileString, readLen));
	}

	// release the memory that holds fileString 
	delete [] fileString;

	return TRUE;
}

/*
** Close all files and windows, leaving one untitled window
*/
int CloseAllFilesAndWindows(void) {

	// while the size of the list is > 1 ...
	while (WindowList->next_ != nullptr || WindowList->filenameSet_ || WindowList->fileChanged_) {
		/*
		 * When we're exiting through a macro, the document running the
		 * macro does not disappear from the list, so we could get stuck
		 * in an endless loop if we try to close it. Therefore, we close
		 * other documents first. (Note that the document running the macro
		 * may get closed because it is in the same window as another
		 * document that gets closed, but it won't disappear; it becomes
		 * Untitled.)
		 */
		if (MacroRunWindow() == WindowList && WindowList->next_) {
			if (!WindowList->next_->CloseAllDocumentInWindow()) {
				return False;
			}
		} else {
			if (!WindowList->CloseAllDocumentInWindow()) {
				return False;
			}
		}
	}

	return TRUE;
}

int CloseFileAndWindow(Document *window, int preResponse) {
	int response, stat;

	// Make sure that the window is not in iconified state 
	if (window->fileChanged_) {
		window->RaiseDocumentWindow();
	}

	/* If the window is a normal & unmodified file or an empty new file,
	   or if the user wants to ignore external modifications then
	   just close it.  Otherwise ask for confirmation first. */
	if (!window->fileChanged_ &&
	    // Normal File 
	    ((!window->fileMissing_ && window->lastModTime_ > 0) ||
	     // New File
	     (window->fileMissing_ && window->lastModTime_ == 0) ||
	     // File deleted/modified externally, ignored by user. 
	     !GetPrefWarnFileMods())) {
		window->CloseWindow();
		// up-to-date windows don't have outstanding backup files to close 
	} else {
		if (preResponse == PROMPT_SBC_DIALOG_RESPONSE) {
		
			int resp = QMessageBox::warning(nullptr /*window->shell_*/, QLatin1String("Save File"), QString(QLatin1String("Save %1 before closing?")).arg(QString::fromStdString(window->filename_)), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
			
			// TODO(eteran): factor out the need for this mapping...
			switch(resp) {
			case QMessageBox::Yes:
				response = 1;
				break;
			case QMessageBox::No:
				response = 2;
				break;
			case QMessageBox::Cancel:
			default:			
				response = 3;
				break;
			}

		} else {
			response = preResponse;
		}
		
		switch(response) {
		case YES_SBC_DIALOG_RESPONSE:
			// Save 
			stat = SaveWindow(window);
			if (stat) {
				window->CloseWindow();
			} else {
				return FALSE;
			}
			break;
			
		case NO_SBC_DIALOG_RESPONSE:
			// Don't Save 
			RemoveBackupFile(window);
			window->CloseWindow();
			break;
		default:
			return FALSE;			
		}
	}
	return TRUE;
}

int SaveWindow(Document *window) {

	// Try to ensure our information is up-to-date 
	CheckForChangesToFile(window);

	/* Return success if the file is normal & unchanged or is a
	    read-only file. */
	if ((!window->fileChanged_ && !window->fileMissing_ && window->lastModTime_ > 0) || IS_ANY_LOCKED_IGNORING_PERM(window->lockReasons_))
		return TRUE;
	// Prompt for a filename if this is an Untitled window 
	if (!window->filenameSet_)
		return SaveWindowAs(window, nullptr, False);

	// Check for external modifications and warn the user 
	if (GetPrefWarnFileMods() && fileWasModifiedExternally(window)) {
	
		QMessageBox messageBox(nullptr /*window->shell_*/);
		messageBox.setWindowTitle(QLatin1String("Save File"));
		messageBox.setIcon(QMessageBox::Warning);
		messageBox.setText(QString(QLatin1String("%1 has been modified by another program.\n\n"
		                                         "Continuing this operation will overwrite any external\n"
		                                         "modifications to the file since it was opened in NEdit,\n"
		                                         "and your work or someone else's may potentially be lost.\n\n"
		                                         "To preserve the modified file, cancel this operation and\n"
		                                         "use Save As... to save this file under a different name,\n"
		                                         "or Revert to Saved to revert to the modified version.")).arg(QString::fromStdString(window->filename_)));

		QPushButton *buttonContinue = messageBox.addButton(QLatin1String("Continue"), QMessageBox::AcceptRole);
		QPushButton *buttonCancel   = messageBox.addButton(QMessageBox::Cancel);
		Q_UNUSED(buttonContinue);

		messageBox.exec();
		if(messageBox.clickedButton() == buttonCancel) {
			// Cancel and mark file as externally modified 
			window->lastModTime_ = 0;
			window->fileMissing_ = FALSE;
			return FALSE;
		}
	}

	if (writeBckVersion(window))
		return FALSE;
	
	bool stat = doSave(window);
	if (stat)
		RemoveBackupFile(window);

	return stat;
}

int SaveWindowAs(Document *window, const char *newName, bool addWrap) {
	int  retVal;
	char fullname[MAXPATHLEN];
	char filename[MAXPATHLEN];
	char pathname[MAXPATHLEN];
	Document *otherWindow;

	if(!newName) {
		QFileDialog dialog(nullptr /*parent*/, QLatin1String("Save File As"));
		dialog.setFileMode(QFileDialog::AnyFile);
		dialog.setAcceptMode(QFileDialog::AcceptSave);
		dialog.setDirectory((!window->path_.empty()) ? QString::fromStdString(window->path_) : QString());
		dialog.setOptions(QFileDialog::DontUseNativeDialog);
		
		if(QGridLayout* const layout = qobject_cast<QGridLayout*>(dialog.layout())) {
			if(layout->rowCount() == 4 && layout->columnCount() == 3) {
				auto boxLayout = new QBoxLayout(QBoxLayout::LeftToRight);

				auto unixCheck = new QRadioButton(QLatin1String("&Unix"));
				auto dosCheck  = new QRadioButton(QLatin1String("D&OS"));
				auto macCheck  = new QRadioButton(QLatin1String("&Macintosh"));

				switch(window->fileFormat_) {
				case DOS_FILE_FORMAT:
					dosCheck->setChecked(true);
					break;			
				case MAC_FILE_FORMAT:
					macCheck->setChecked(true);
					break;
				case UNIX_FILE_FORMAT:
					unixCheck->setChecked(true);
					break;
				}

				auto group = new QButtonGroup();
				group->addButton(unixCheck);
				group->addButton(dosCheck);
				group->addButton(macCheck);

				boxLayout->addWidget(unixCheck);
				boxLayout->addWidget(dosCheck);
				boxLayout->addWidget(macCheck);

				int row = layout->rowCount();

				layout->addWidget(new QLabel(QLatin1String("Format: ")), row, 0, 1, 1);
				layout->addLayout(boxLayout, row, 1, 1, 1, Qt::AlignLeft);
				
				++row;

				auto wrapCheck = new QCheckBox(QLatin1String("&Add line breaks where wrapped"));
#if 0	
				// TODO(eteran): implement this once this is hoisted into a QObject
				//               since Qt4 doesn't support lambda based connections							
				QObject::connect(wrapCheck, &QCheckBox::toggled, [&](bool checked) {
					if(checked) {
						int ret = QMessageBox::information(nullptr, QLatin1String("Add Wrap"), 
							QLatin1String("This operation adds permanent line breaks to\n"
							"match the automatic wrapping done by the\n"
							"Continuous Wrap mode Preferences Option.\n\n"
							"*** This Option is Irreversable ***\n\n"
							"Once newlines are inserted, continuous wrapping\n"
							"will no longer work automatically on these lines"),
							QMessageBox::Ok, QMessageBox::Cancel);
							
						if(ret != QMessageBox::Ok) {
							wrapCheck->setChecked(false);
						}
					}
				});
#endif
				
				if (window->wrapMode_ == CONTINUOUS_WRAP) {			
					layout->addWidget(wrapCheck, row, 1, 1, 1);
				}

				if(dialog.exec()) {
					if(dosCheck->isChecked()) {
						window->fileFormat_ = DOS_FILE_FORMAT;
					} else if(macCheck->isChecked()) {
						window->fileFormat_ = MAC_FILE_FORMAT;
					} else if(unixCheck->isChecked()) {
						window->fileFormat_ = UNIX_FILE_FORMAT;
					}
					
					addWrap = wrapCheck->isChecked();					
					strcpy(fullname, dialog.selectedFiles()[0].toLocal8Bit().data());
				} else {
					return FALSE;
				}

			}
		}
	} else {
		strcpy(fullname, newName);
	}

	// Add newlines if requested 
	if (addWrap)
		addWrapNewlines(window);

	if (ParseFilename(fullname, filename, pathname) != 0) {
		return FALSE;
	}

	// If the requested file is this file, just save it and return 
	if (window->filename_ == filename && window->path_ == pathname) {
		if (writeBckVersion(window))
			return FALSE;
		return doSave(window);
	}

	/* If the file is open in another window, make user close it.  Note that
	   it is possible for user to close the window by hand while the dialog
	   is still up, because the dialog is not application modal, so after
	   doing the dialog, check again whether the window still exists. */
	otherWindow = FindWindowWithFile(filename, pathname);
	if (otherWindow) {
	
		QMessageBox messageBox(nullptr /*window->shell_*/);
		messageBox.setWindowTitle(QLatin1String("File open"));
		messageBox.setIcon(QMessageBox::Warning);
		messageBox.setText(QString(QLatin1String("%1 is open in another NEdit window")).arg(QLatin1String(filename)));
		QPushButton *buttonCloseOther = messageBox.addButton(QLatin1String("Close Other Window"), QMessageBox::AcceptRole);
		QPushButton *buttonCancel     = messageBox.addButton(QMessageBox::Cancel);
		Q_UNUSED(buttonCloseOther);

		messageBox.exec();
		if(messageBox.clickedButton() == buttonCancel) {
			return FALSE;
		}	
	
		if (otherWindow == FindWindowWithFile(filename, pathname)) {
			if (!CloseFileAndWindow(otherWindow, PROMPT_SBC_DIALOG_RESPONSE)) {
				return FALSE;
			}
		}
	}

	// Destroy the file closed property for the original file 
	DeleteFileClosedProperty(window);

	// Change the name of the file and save it under the new name 
	RemoveBackupFile(window);
	window->filename_ = filename;
	window->path_     = pathname;
	window->fileMode_ = 0;
	window->fileUid_  = 0;
	window->fileGid_  = 0;
	
	CLEAR_ALL_LOCKS(window->lockReasons_);
	retVal = doSave(window);
	window->UpdateWindowReadOnly();
	window->RefreshTabState();

	// Add the name to the convenience menu of previously opened files 
	AddToPrevOpenMenu(fullname);

	/*  If name has changed, language mode may have changed as well, unless
	    it's an Untitled window for which the user already set a language
	    mode; it's probably the right one.  */
	if (window->languageMode_ == PLAIN_LANGUAGE_MODE || window->filenameSet_) {
		DetermineLanguageMode(window, False);
	}
	window->filenameSet_ = True;

	// Update the stats line and window title with the new filename 
	window->UpdateWindowTitle();
	window->UpdateStatsLine();

	window->SortTabBar();
	return retVal;

}

static bool doSave(Document *window) {
	struct stat statbuf;
	FILE *fp;

	// Get the full name of the file 
	
	std::string fullname = window->FullPath();

	/*  Check for root and warn him if he wants to write to a file with
	    none of the write bits set.  */
	if ((getuid() == 0) && (stat(fullname.c_str(), &statbuf) == 0) && !(statbuf.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH))) {
	
		int result = QMessageBox::warning(nullptr /*window->shell_*/, QLatin1String("Writing Read-only File"), QString(QLatin1String("File '%1' is marked as read-only.\nDo you want to save anyway?")).arg(QString::fromStdString(window->filename_)), QMessageBox::Save | QMessageBox::Cancel);
		if (result != QMessageBox::Save) {
			return true;
		}
	}

	/* add a terminating newline if the file doesn't already have one for
	   Unix utilities which get confused otherwise
	   NOTE: this must be done _before_ we create/open the file, because the
	         (potential) buffer modification can trigger a check for file
	         changes. If the file is created for the first time, it has
	         zero size on disk, and the check would falsely conclude that the
	         file has changed on disk, and would pop up a warning dialog */
	// TODO(eteran): this seems to be broken for an empty buffer
	//               it is going to read the buffer at index -1 :-/
	if (window->buffer_->BufGetCharacter(window->buffer_->BufGetLength() - 1) != '\n' && !window->buffer_->BufIsEmpty() && GetPrefAppendLF()) {
		window->buffer_->BufAppendEx("\n");
	}

	// open the file 
	fp = fopen(fullname.c_str(), "wb");
	if(!fp) {
	
		QMessageBox messageBox(nullptr /*window->shell_*/);
		messageBox.setWindowTitle(QLatin1String("Error saving File"));
		messageBox.setIcon(QMessageBox::Warning);
		messageBox.setText(QString(QLatin1String("Unable to save %1:\n%1\n\nSave as a new file?")).arg(QString::fromStdString(window->filename_)).arg(QLatin1String(strerror(errno))));

		QPushButton *buttonSaveAs = messageBox.addButton(QLatin1String("Save As..."), QMessageBox::AcceptRole);
		QPushButton *buttonCancel = messageBox.addButton(QMessageBox::Cancel);
		Q_UNUSED(buttonCancel);

		messageBox.exec();
		if(messageBox.clickedButton() == buttonSaveAs) {
			return SaveWindowAs(window, nullptr, 0);
		}	

		return FALSE;
	}

	// get the text buffer contents and its length 
	std::string fileString = window->buffer_->BufGetAllEx();

	// If null characters are substituted for, put them back 
	window->buffer_->BufUnsubstituteNullCharsEx(fileString);

	// If the file is to be saved in DOS or Macintosh format, reconvert 
	if (window->fileFormat_ == DOS_FILE_FORMAT) {
		if (!ConvertToDosFileStringEx(fileString)) {
			QMessageBox::critical(nullptr /*parent*/, QLatin1String("Out of Memory"), QLatin1String("Out of memory!  Try\nsaving in Unix format"));

			// NOTE(eteran): fixes resource leak error
			fclose(fp);
			return FALSE;
		}
	} else if (window->fileFormat_ == MAC_FILE_FORMAT) {
		ConvertToMacFileStringEx(fileString);
	}

	// write to the file 
	fwrite(fileString.data(), sizeof(char), fileString.size(), fp);
	
	
	if (ferror(fp)) {
		QMessageBox::critical(nullptr /*window->shell_*/, QLatin1String("Error saving File"), QString(QLatin1String("%2 not saved:\n%2")).arg(QString::fromStdString(window->filename_)).arg(QLatin1String(strerror(errno))));
		fclose(fp);
		remove(fullname.c_str());
		return FALSE;
	}

	// close the file 
	if (fclose(fp) != 0) {
		QMessageBox::critical(nullptr /*window->shell_*/, QLatin1String("Error closing File"), QString(QLatin1String("Error closing file:\n%1")).arg(QLatin1String(strerror(errno))));
		return FALSE;
	}

	// success, file was written 
	window->SetWindowModified(FALSE);

	// update the modification time 
	if (stat(fullname.c_str(), &statbuf) == 0) {
		window->lastModTime_ = statbuf.st_mtime;
		window->fileMissing_ = FALSE;
		window->device_ = statbuf.st_dev;
		window->inode_ = statbuf.st_ino;
	} else {
		// This needs to produce an error message -- the file can't be accessed! 
		window->lastModTime_ = 0;
		window->fileMissing_ = TRUE;
		window->device_ = 0;
		window->inode_ = 0;
	}

	return TRUE;
}

/*
** Create a backup file for the current window.  The name for the backup file
** is generated using the name and path stored in the window and adding a
** tilde (~) on UNIX and underscore (_) on VMS to the beginning of the name.
*/
int WriteBackupFile(Document *window) {
	FILE *fp;
	int fd;

	// Generate a name for the autoSave file 
	std::string name = backupFileNameEx(window);

	/* remove the old backup file.
	   Well, this might fail - we'll notice later however. */
	remove(name.c_str());

	/* open the file, set more restrictive permissions (using default
	    permissions was somewhat of a security hole, because permissions were
	    independent of those of the original file being edited */
	if ((fd = open(name.c_str(), O_CREAT | O_EXCL | O_WRONLY, S_IRUSR | S_IWUSR)) < 0 || (fp = fdopen(fd, "w")) == nullptr) {
	
		QMessageBox::warning(nullptr /*window->shell_*/, QLatin1String("Error writing Backup"), QString(QLatin1String("Unable to save backup for %1:\n%2\nAutomatic backup is now off")).arg(QString::fromStdString(window->filename_)).arg(QLatin1String(strerror(errno))));		
		window->autoSave_ = FALSE;
		window->SetToggleButtonState(window->autoSaveItem_, FALSE, FALSE);
		return FALSE;
	}

	// get the text buffer contents and its length 
	std::string fileString = window->buffer_->BufGetAllEx();

	// If null characters are substituted for, put them back 
	window->buffer_->BufUnsubstituteNullCharsEx(fileString);

	// add a terminating newline if the file doesn't already have one 
	if (!fileString.empty() && fileString.back() != '\n') {
		fileString.append("\n"); // null terminator no longer needed 
	}

	// write out the file 
	fwrite(fileString.data(), sizeof(char), fileString.size(), fp);
	if (ferror(fp)) {	
		QMessageBox::critical(nullptr /*window->shell_*/, QLatin1String("Error saving Backup"), QString(QLatin1String("Error while saving backup for %1:\n%2\nAutomatic backup is now off")).arg(QString::fromStdString(window->filename_)).arg(QLatin1String(strerror(errno))));
		fclose(fp);
		remove(name.c_str());
		window->autoSave_ = FALSE;
		return FALSE;
	}

	// close the backup file 
	if (fclose(fp) != 0) {
		return FALSE;
	}

	return TRUE;
}

/*
** Remove the backup file associated with this window
*/
void RemoveBackupFile(Document *window) {

	// Don't delete backup files when backups aren't activated. 
	if (window->autoSave_ == FALSE)
		return;

	std::string name = backupFileNameEx(window);
	remove(name.c_str());
}

/*
** Generate the name of the backup file for this window from the filename
** and path in the window data structure & write into name
*/
static std::string backupFileNameEx(Document *window) {
	
	char buf[MAXPATHLEN];
	if (window->filenameSet_) {
		snprintf(buf, sizeof(buf), "%s~%s", window->path_.c_str(), window->filename_.c_str());
		return buf;
	} else {
		snprintf(buf, sizeof(buf), "~%s", window->filename_.c_str());
		return PrependHomeEx(buf).toStdString();
	}
}

/*
** If saveOldVersion is on, copies the existing version of the file to
** <filename>.bck in anticipation of a new version being saved.  Returns
** True if backup fails and user requests that the new file not be written.
*/
static bool writeBckVersion(Document *window) {

	char bckname[MAXPATHLEN];
	struct stat statbuf;
	int in_fd;
	int out_fd;
	
	static const size_t IO_BUFFER_SIZE = (1024 * 1024);

	// Do only if version backups are turned on 
	if (!window->saveOldVersion_) {
		return false;
	}

	// Get the full name of the file 
	std::string fullname = window->FullPath();

	// Generate name for old version 
	if (fullname.size() >= MAXPATHLEN) {
		return bckError(window, "file name too long", window->filename_.c_str());
	}
	snprintf(bckname, sizeof(bckname), "%s.bck", fullname.c_str());

	// Delete the old backup file 
	// Errors are ignored; we'll notice them later. 
	remove(bckname);

	/* open the file being edited.  If there are problems with the
	   old file, don't bother the user, just skip the backup */
	in_fd = open(fullname.c_str(), O_RDONLY);
	if (in_fd < 0) {
		return false;
	}

	/* Get permissions of the file.
	   We preserve the normal permissions but not ownership, extended
	   attributes, et cetera. */
	if (fstat(in_fd, &statbuf) != 0) {
		return false;
	}

	// open the destination file exclusive and with restrictive permissions. 
	out_fd = open(bckname, O_CREAT | O_EXCL | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
	if (out_fd < 0) {
		return bckError(window, "Error open backup file", bckname);
	}

	// Set permissions on new file 
	if (fchmod(out_fd, statbuf.st_mode) != 0) {
		close(in_fd);
		close(out_fd);
		remove(bckname);
		return bckError(window, "fchmod() failed", bckname);
	}

	// Allocate I/O buffer 
	auto io_buffer = new char[IO_BUFFER_SIZE];

	// copy loop 
	for (;;) {
		ssize_t bytes_read;
		ssize_t bytes_written;
		bytes_read = read(in_fd, io_buffer, IO_BUFFER_SIZE);

		if (bytes_read < 0) {
			close(in_fd);
			close(out_fd);
			remove(bckname);
			delete [] io_buffer;
			return bckError(window, "read() error", window->filename_.c_str());
		}

		if (bytes_read == 0) {
			break; // EOF 
		}

		// write to the file 
		bytes_written = write(out_fd, io_buffer, (size_t)bytes_read);
		if (bytes_written != bytes_read) {
			close(in_fd);
			close(out_fd);
			remove(bckname);
			delete [] io_buffer;
			return bckError(window, strerror(errno), bckname);
		}
	}

	// close the input and output files 
	close(in_fd);
	close(out_fd);

	delete [] io_buffer;

	return false;
}

/*
** Error processing for writeBckVersion, gives the user option to cancel
** the subsequent save, or continue and optionally turn off versioning
*/
static bool bckError(Document *window, const char *errString, const char *file) {

	QMessageBox messageBox(nullptr /*window->shell_*/);
	messageBox.setWindowTitle(QLatin1String("Error writing Backup"));
	messageBox.setIcon(QMessageBox::Critical);
	messageBox.setText(QString(QLatin1String("Couldn't write .bck (last version) file.\n%1: %2")).arg(QLatin1String(file)).arg(QLatin1String(errString)));

	QPushButton *buttonCancelSave = messageBox.addButton(QLatin1String("Cancel Save"),      QMessageBox::RejectRole);
	QPushButton *buttonTurnOff    = messageBox.addButton(QLatin1String("Turn off Backups"), QMessageBox::AcceptRole);
	QPushButton *buttonContinue   = messageBox.addButton(QLatin1String("Continue"),         QMessageBox::AcceptRole);

	Q_UNUSED(buttonContinue);

	messageBox.exec();
	if(messageBox.clickedButton() == buttonCancelSave) {
		return true;
	}

	if(messageBox.clickedButton() == buttonTurnOff) {
		window->saveOldVersion_ = FALSE;
		window->SetToggleButtonState(window->saveLastItem_, FALSE, FALSE);
	}

	return false;
}

void PrintWindow(Document *window, bool selectedOnly) {
	TextBuffer *buf = window->buffer_;
	TextSelection *sel = &buf->primary_;
	std::string fileString;

	/* get the contents of the text buffer from the text area widget.  Add
	   wrapping newlines if necessary to make it match the displayed text */
	if (selectedOnly) {
		if (!sel->selected) {
			QApplication::beep();
			return;
		}
		if (sel->rectangular) {
			fileString = buf->BufGetSelectionTextEx();
		} else {
			fileString = TextGetWrappedEx(window->textArea_, sel->start, sel->end);
		}
	} else {
		fileString = TextGetWrappedEx(window->textArea_, 0, buf->BufGetLength());
	}

	// If null characters are substituted for, put them back 
	buf->BufUnsubstituteNullCharsEx(fileString);

	// add a terminating newline if the file doesn't already have one 
	if (!fileString.empty() && fileString.back() != '\n') {
		fileString.push_back('\n'); // null terminator no longer needed 
	}

	// Print the string 
	PrintString(fileString, window->filename_);
}

/*
** Print a string (length is required).  parent is the dialog parent, for
** error dialogs, and jobName is the print title.
*/
void PrintString(const std::string &string, const std::string &jobName) {

	QString tempDir = QDesktopServices::storageLocation(QDesktopServices::TempLocation);

	// L_tmpnam defined in stdio.h 
	char tmpFileName[L_tmpnam];
	snprintf(tmpFileName, sizeof(tmpFileName), "%s/nedit-XXXXXX", qPrintable(tempDir));
	
	int fd = mkstemp(tmpFileName);
	if (fd < 0) {
		QMessageBox::warning(nullptr /*parent*/, QLatin1String("Error while Printing"), QString(QLatin1String("Unable to write file for printing:\n%1")).arg(QLatin1String(strerror(errno))));
		return;
	}

	FILE *const fp = fdopen(fd, "w");

	// open the temporary file 
	if (fp == nullptr) {
		QMessageBox::warning(nullptr /*parent*/, QLatin1String("Error while Printing"), QString(QLatin1String("Unable to write file for printing:\n%1")).arg(QLatin1String(strerror(errno))));
		return;
	}

	// write to the file 
	fwrite(string.data(), sizeof(char), string.size(), fp);
	if (ferror(fp)) {
		QMessageBox::critical(nullptr /*parent*/, QLatin1String("Error while Printing"), QString(QLatin1String("%1 not printed:\n%2")).arg(QString::fromStdString(jobName)).arg(QLatin1String(strerror(errno))));
		fclose(fp); // should call close(fd) in turn! 
		remove(tmpFileName);
		return;
	}

	// close the temporary file 
	if (fclose(fp) != 0) {
		QMessageBox::warning(nullptr /*parent*/, QLatin1String("Error while Printing"), QString(QLatin1String("Error closing temp. print file:\n%1")).arg(QLatin1String(strerror(errno))));
		remove(tmpFileName);
		return;
	}

	// Print the temporary file, then delete it and return success 
	
	auto dialog = new DialogPrint(QLatin1String(tmpFileName), QString::fromStdString(jobName), nullptr);
	dialog->exec();
	delete dialog;
	
//	PrintFile(parent, tmpFileName, jobName);
	remove(tmpFileName);
	return;
}

/*
** Wrapper for GetExistingFilename which uses the current window's path
** (if set) as the default directory.
*/
int PromptForExistingFile(Document *window, const char *prompt, char *fullname) {

	/* Temporarily set default directory to window->path_, prompt for file,
	   then, if the call was unsuccessful, restore the original default
	   directory */
	auto savedDefaultDir = GetFileDialogDefaultDirectoryEx();
	if (!window->path_.empty()) {
		SetFileDialogDefaultDirectory(QString::fromStdString(window->path_));
	}
	
#if 1
	QFileDialog dialog(nullptr /*parent*/, QLatin1String(prompt));
	dialog.setOptions(QFileDialog::DontUseNativeDialog);
	dialog.setFileMode(QFileDialog::ExistingFile);
	dialog.setDirectory((!window->path_.empty()) ? QString::fromStdString(window->path_) : QString());
	if(dialog.exec()) {
		strcpy(fullname, dialog.selectedFiles()[0].toLocal8Bit().data());
		return GFN_OK;
	}

	SetFileDialogDefaultDirectory(savedDefaultDir);
	return GFN_CANCEL;
#else
	int retVal = GetExistingFilename(window->shell_, prompt, fullname);
	if (retVal != GFN_OK) {
		SetFileDialogDefaultDirectory(savedDefaultDir);
	}

	return retVal;
#endif
}

/*
** Wrapper for HandleCustomNewFileSB which uses the current window's path
** (if set) as the default directory, and asks about embedding newlines
** to make wrapping permanent.
*/
int PromptForNewFile(Document *window, const char *prompt, char *fullname, FileFormats *fileFormat, bool *addWrap) {
	int n, retVal;
	Arg args[20];
	XmString s1, s2;
	Widget fileSB, wrapToggle;
	Widget formatForm, formatBtns, unixFormat, dosFormat, macFormat;

	*fileFormat = window->fileFormat_;

	/* Temporarily set default directory to window->path_, prompt for file,
	   then, if the call was unsuccessful, restore the original default
	   directory */
	auto savedDefaultDir = GetFileDialogDefaultDirectoryEx();

	if (!window->path_.empty()) {
		SetFileDialogDefaultDirectory(QString::fromStdString(window->path_));
	}

	/* Present a file selection dialog with an added field for requesting
	   long line wrapping to become permanent via inserted newlines */
	n = 0;
	XtSetArg(args[n], XmNselectionLabelString, s1 = XmStringCreateLocalizedEx("New File Name:"));
	n++;
	XtSetArg(args[n], XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL);
	n++;
	XtSetArg(args[n], XmNdialogTitle, s2 = XmStringCreateSimpleEx(prompt));
	n++;
	fileSB = CreateFileSelectionDialog(window->shell_, "FileSelect", args, n);
	XmStringFree(s1);
	XmStringFree(s2);
	formatForm = XtVaCreateManagedWidget("formatForm", xmFormWidgetClass, fileSB, nullptr);
	formatBtns = XtVaCreateManagedWidget("formatBtns", xmRowColumnWidgetClass, formatForm, XmNradioBehavior, XmONE_OF_MANY, XmNorientation, XmHORIZONTAL, XmNpacking, XmPACK_TIGHT, XmNtopAttachment, XmATTACH_FORM, XmNleftAttachment,
	                                     XmATTACH_FORM, nullptr);
	XtVaCreateManagedWidget("formatBtns", xmLabelWidgetClass, formatBtns, XmNlabelString, s1 = XmStringCreateSimpleEx("Format:"), nullptr);
	XmStringFree(s1);
	unixFormat = XtVaCreateManagedWidget("unixFormat", xmToggleButtonWidgetClass, formatBtns, XmNlabelString, s1 = XmStringCreateSimpleEx("Unix"), XmNset, *fileFormat == UNIX_FILE_FORMAT, XmNuserData, (XtPointer)UNIX_FILE_FORMAT,
	                                     XmNmarginHeight, 0, XmNalignment, XmALIGNMENT_BEGINNING, XmNmnemonic, 'U', nullptr);
	XmStringFree(s1);
	XtAddCallback(unixFormat, XmNvalueChangedCallback, setFormatCB, fileFormat);
	dosFormat = XtVaCreateManagedWidget("dosFormat", xmToggleButtonWidgetClass, formatBtns, XmNlabelString, s1 = XmStringCreateSimpleEx("DOS"), XmNset, *fileFormat == DOS_FILE_FORMAT, XmNuserData, (XtPointer)DOS_FILE_FORMAT,
	                                    XmNmarginHeight, 0, XmNalignment, XmALIGNMENT_BEGINNING, XmNmnemonic, 'O', nullptr);
	XmStringFree(s1);
	XtAddCallback(dosFormat, XmNvalueChangedCallback, setFormatCB, fileFormat);
	macFormat = XtVaCreateManagedWidget("macFormat", xmToggleButtonWidgetClass, formatBtns, XmNlabelString, s1 = XmStringCreateSimpleEx("Macintosh"), XmNset, *fileFormat == MAC_FILE_FORMAT, XmNuserData, (XtPointer)MAC_FILE_FORMAT,
	                                    XmNmarginHeight, 0, XmNalignment, XmALIGNMENT_BEGINNING, XmNmnemonic, 'M', nullptr);
	XmStringFree(s1);
	XtAddCallback(macFormat, XmNvalueChangedCallback, setFormatCB, fileFormat);
	if (window->wrapMode_ == CONTINUOUS_WRAP) {
		wrapToggle = XtVaCreateManagedWidget("addWrap", xmToggleButtonWidgetClass, formatForm, XmNlabelString, s1 = XmStringCreateSimpleEx("Add line breaks where wrapped"), XmNalignment, XmALIGNMENT_BEGINNING, XmNmnemonic, 'A',
		                                     XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, formatBtns, XmNleftAttachment, XmATTACH_FORM, nullptr);
		XtAddCallback(wrapToggle, XmNvalueChangedCallback, addWrapCB, addWrap);
		XmStringFree(s1);
	}
	*addWrap = False;
	XtVaSetValues(XmFileSelectionBoxGetChild(fileSB, XmDIALOG_FILTER_LABEL), XmNmnemonic, 'l', XmNuserData, XmFileSelectionBoxGetChild(fileSB, XmDIALOG_FILTER_TEXT), nullptr);
	XtVaSetValues(XmFileSelectionBoxGetChild(fileSB, XmDIALOG_DIR_LIST_LABEL), XmNmnemonic, 'D', XmNuserData, XmFileSelectionBoxGetChild(fileSB, XmDIALOG_DIR_LIST), nullptr);
	XtVaSetValues(XmFileSelectionBoxGetChild(fileSB, XmDIALOG_LIST_LABEL), XmNmnemonic, 'F', XmNuserData, XmFileSelectionBoxGetChild(fileSB, XmDIALOG_LIST), nullptr);
	XtVaSetValues(XmFileSelectionBoxGetChild(fileSB, XmDIALOG_SELECTION_LABEL), XmNmnemonic, prompt[strspn(prompt, "lFD")], XmNuserData, XmFileSelectionBoxGetChild(fileSB, XmDIALOG_TEXT), nullptr);
	AddDialogMnemonicHandler(fileSB, FALSE);
	RemapDeleteKey(XmFileSelectionBoxGetChild(fileSB, XmDIALOG_FILTER_TEXT));
	RemapDeleteKey(XmFileSelectionBoxGetChild(fileSB, XmDIALOG_TEXT));
	retVal = HandleCustomNewFileSB(fileSB, fullname, window->filenameSet_ ? window->filename_.c_str() : nullptr);

	if (retVal != GFN_OK)
		SetFileDialogDefaultDirectory(savedDefaultDir);

	return retVal;
}

/*
** Find a name for an untitled file, unique in the name space of in the opened
** files in this session, i.e. Untitled or Untitled_nn, and write it into
** the string "name".
*/
void UniqueUntitledName(char *name, size_t size) {
	
	for (int i = 0; i < INT_MAX; i++) {
	
		if (i == 0) {
			snprintf(name, size, "Untitled");
		} else {
			snprintf(name, size, "Untitled_%d", i);
		}
		
		Document *w = Document::find_if([name](Document *window) {
			return window->filename_ == name;
		});
		
		if(!w) {
			break;
		}
	}
}

/*
** Callback that guards us from trying to access a window after it has
** been destroyed while a modal dialog is up.
*/
static void modifiedWindowDestroyedCB(Widget w, XtPointer clientData, XtPointer callData) {
	(void)w;
	(void)callData;

	*static_cast<Bool *>(clientData) = TRUE;
}

/*
** Check if the file in the window was changed by an external source.
** and put up a warning dialog if it has.
*/
void CheckForChangesToFile(Document *window) {
	static Document *lastCheckWindow = nullptr;
	static Time lastCheckTime = 0;
	struct stat statbuf;
	Time timestamp;
	FILE *fp;
	int silent = 0;
	XWindowAttributes winAttr;
	Boolean windowIsDestroyed = False;

	if (!window->filenameSet_) {
		return;
	}

	// If last check was very recent, don't impact performance 
	timestamp = XtLastTimestampProcessed(XtDisplay(window->shell_));
	if (window == lastCheckWindow && timestamp - lastCheckTime < MOD_CHECK_INTERVAL) {
		return;
	}
	
	lastCheckWindow = window;
	lastCheckTime = timestamp;

	/* Update the status, but don't pop up a dialog if we're called
	   from a place where the window might be iconic (e.g., from the
	   replace dialog) or on another desktop.

	   This works, but I bet it costs a round-trip to the server.
	   Might be better to capture MapNotify/Unmap events instead.

	   For tabs that are not on top, we don't want the dialog either,
	   and we don't even need to contact the server to find out. By
	   performing this check first, we avoid a server round-trip for
	   most files in practice. */
	if (!window->IsTopDocument())
		silent = 1;
	else {
		XGetWindowAttributes(XtDisplay(window->shell_), XtWindow(window->shell_), &winAttr);

		if (winAttr.map_state != IsViewable)
			silent = 1;
	}

	// Get the file mode and modification time
	std::string fullname = window->FullPath();
	
	if (stat(fullname.c_str(), &statbuf) != 0) {
		// Return if we've already warned the user or we can't warn him now 
		if (window->fileMissing_ || silent) {
			return;
		}

		/* Can't stat the file -- maybe it's been deleted.
		   The filename is now invalid */
		window->fileMissing_ = TRUE;
		window->lastModTime_ = 1;
		window->device_ = 0;
		window->inode_ = 0;

		/* Warn the user, if they like to be warned (Maybe this should be its
		    own preference setting: GetPrefWarnFileDeleted()) */
		if (GetPrefWarnFileMods()) {
			bool save = false;

			// See note below about pop-up timing and XUngrabPointer 
			XUngrabPointer(XtDisplay(window->shell_), timestamp);

			/* If the window (and the dialog) are destroyed while the dialog
			   is up (typically closed via the window manager), we should
			   avoid accessing the window afterwards. */
			XtAddCallback(window->shell_, XmNdestroyCallback, modifiedWindowDestroyedCB, &windowIsDestroyed);

			//  Set title, message body and button to match stat()'s error.  
			switch (errno) {
			case ENOENT:
				{
					// A component of the path file_name does not exist. 
					int resp = QMessageBox::critical(nullptr /*window->shell_*/, QLatin1String("File not Found"), QString(QLatin1String("File '%1' (or directory in its path)\nno longer exists.\nAnother program may have deleted or moved it.")).arg(QString::fromStdString(window->filename_)), QMessageBox::Save | QMessageBox::Cancel);
					save = (resp == QMessageBox::Save);
				}
				break;
			case EACCES:
				{
					// Search permission denied for a path component. We add one to the response because Re-Save wouldn't really make sense here.
					int resp = QMessageBox::critical(nullptr /*window->shell_*/, QLatin1String("Permission Denied"), QString(QLatin1String("You no longer have access to file '%1'.\nAnother program may have changed the permissions of\none of its parent directories.")).arg(QString::fromStdString(window->filename_)), QMessageBox::Save | QMessageBox::Cancel);
					save = (resp == QMessageBox::Save);
				}
				break;
			default:
				{
					// Everything else. This hints at an internal error (eg. ENOTDIR) or at some bad state at the host.
					int resp = QMessageBox::critical(nullptr /*window->shell_*/, QLatin1String("File not Accessible"), QString(QLatin1String("Error while checking the status of file '%1':\n    '%2'\nPlease make sure that no data is lost before closing\nthis window.")).arg(QString::fromStdString(window->filename_)).arg(QLatin1String(strerror(errno))), QMessageBox::Save | QMessageBox::Cancel);
					save = (resp == QMessageBox::Save);
				}
				break;
			}

			if (!windowIsDestroyed) {
				XtRemoveCallback(window->shell_, XmNdestroyCallback, modifiedWindowDestroyedCB, &windowIsDestroyed);
			}

			if(save) {
				SaveWindow(window);
			}
		}

		// A missing or (re-)saved file can't be read-only. 
		//  TODO: A document without a file can be locked though.  
		// Make sure that the window was not destroyed behind our back! 
		if (!windowIsDestroyed) {
			SET_PERM_LOCKED(window->lockReasons_, False);
			window->UpdateWindowTitle();
			window->UpdateWindowReadOnly();
		}
		return;
	}

	/* Check that the file's read-only status is still correct (but
	   only if the file can still be opened successfully in read mode) */
	if (window->fileMode_ != statbuf.st_mode || window->fileUid_ != statbuf.st_uid || window->fileGid_ != statbuf.st_gid) {
		window->fileMode_ = statbuf.st_mode;
		window->fileUid_ = statbuf.st_uid;
		window->fileGid_ = statbuf.st_gid;
		if ((fp = fopen(fullname.c_str(), "r"))) {
			int readOnly;
			fclose(fp);
#ifndef DONT_USE_ACCESS
			readOnly = access(fullname.c_str(), W_OK) != 0;
#else
			if (((fp = fopen(fullname.c_str(), "r+")) != nullptr)) {
				readOnly = FALSE;
				fclose(fp);
			} else
				readOnly = TRUE;
#endif
			if (IS_PERM_LOCKED(window->lockReasons_) != readOnly) {
				SET_PERM_LOCKED(window->lockReasons_, readOnly);
				window->UpdateWindowTitle();
				window->UpdateWindowReadOnly();
			}
		}
	}

	/* Warn the user if the file has been modified, unless checking is
	   turned off or the user has already been warned.  Popping up a dialog
	   from a focus callback (which is how this routine is usually called)
	   seems to catch Motif off guard, and if the timing is just right, the
	   dialog can be left with a still active pointer grab from a Motif menu
	   which is still in the process of popping down.  The workaround, below,
	   of calling XUngrabPointer is inelegant but seems to fix the problem. */
	if (!silent && ((window->lastModTime_ != 0 && window->lastModTime_ != statbuf.st_mtime) || window->fileMissing_)) {
		window->lastModTime_ = 0; // Inhibit further warnings 
		window->fileMissing_ = FALSE;
		if (!GetPrefWarnFileMods())
			return;
		if (GetPrefWarnRealFileMods() && !cmpWinAgainstFile(window, fullname.c_str())) {
			// Contents hasn't changed. Update the modification time. 
			window->lastModTime_ = statbuf.st_mtime;
			return;
		}

		XUngrabPointer(XtDisplay(window->shell_), timestamp);

		QMessageBox messageBox(nullptr /*window->shell_*/);
		messageBox.setIcon(QMessageBox::Warning);
		messageBox.setWindowTitle(QLatin1String("File modified externally"));
		QPushButton *buttonReload = messageBox.addButton(QLatin1String("Reload"), QMessageBox::AcceptRole);
		QPushButton *buttonCancel = messageBox.addButton(QMessageBox::Cancel);

		Q_UNUSED(buttonCancel);

		if (window->fileChanged_) {			
			messageBox.setText(QString(QLatin1String("%1 has been modified by another program.  Reload?\n\nWARNING: Reloading will discard changes made in this\nediting session!")).arg(QString::fromStdString(window->filename_)));
		} else {
			messageBox.setText(QString(QLatin1String("%1 has been modified by another\nprogram.  Reload?")).arg(QString::fromStdString(window->filename_)));
		}
		
		messageBox.exec();
		if(messageBox.clickedButton() == buttonReload) {
			RevertToSaved(window);
		}
	}
}

/*
** Return true if the file displayed in window has been modified externally
** to nedit.  This should return FALSE if the file has been deleted or is
** unavailable.
*/
static int fileWasModifiedExternally(Document *window) {	
	struct stat statbuf;

	if (!window->filenameSet_) {
		return FALSE;
	}
	
	std::string fullname = window->FullPath();
	
	if (stat(fullname.c_str(), &statbuf) != 0) {
		return FALSE;
	}
	
	if (window->lastModTime_ == statbuf.st_mtime) {
		return FALSE;
	}
	
	if (GetPrefWarnRealFileMods() && !cmpWinAgainstFile(window, fullname.c_str())) {
		return FALSE;
	}
	
	return TRUE;
}

/*
** Check the read-only or locked status of the window and beep and return
** false if the window should not be written in.
*/
int CheckReadOnly(Document *window) {
	if (IS_ANY_LOCKED(window->lockReasons_)) {
		QApplication::beep();
		return True;
	}
	return False;
}

/*
** Callback procedure for File Format toggle buttons.  Format is stored
** in userData field of widget button
*/
static void setFormatCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)callData;

	if (XmToggleButtonGetState(w)) {
		XtPointer userData;
		XtVaGetValues(w, XmNuserData, &userData, nullptr);
		*(int *)clientData = (long)userData;
	}
}

/*
** Callback procedure for toggle button requesting newlines to be inserted
** to emulate continuous wrapping.
*/
static void addWrapCB(Widget w, XtPointer clientData, XtPointer callData) {
	(void)callData;

	auto addWrap = static_cast<int *>(clientData);

	if (XmToggleButtonGetState(w)) {
	
		int resp = QMessageBox::warning(nullptr /*w*/, QLatin1String("Add Wrap"), 
			QLatin1String("This operation adds permanent line breaks to\n"
						  "match the automatic wrapping done by the\n"
						  "Continuous Wrap mode Preferences Option.\n\n"
						  "*** This Option is Irreversable ***\n\n"
						  "Once newlines are inserted, continuous wrapping\n"
						  "will no longer work automatically on these lines"), 
			QMessageBox::Ok | QMessageBox::Cancel);
	
		if (resp == QMessageBox::Cancel) {
			XmToggleButtonSetState(w, False, False);
			*addWrap = False;
		} else {
			*addWrap = True;
		}
	} else {
		*addWrap = False;
	}
}

/*
** Change a window created in NEdit's continuous wrap mode to the more
** conventional Unix format of embedded newlines.  Indicate to the user
** by turning off Continuous Wrap mode.
*/
static void addWrapNewlines(Document *window) {
	int i, insertPositions[MAX_PANES], topLines[MAX_PANES];
	int horizOffset;
	Widget text;

	// save the insert and scroll positions of each pane 
	for (i = 0; i <= window->nPanes_; i++) {
		text = i == 0 ? window->textArea_ : window->textPanes_[i - 1];
		insertPositions[i] = TextGetCursorPos(text);
		TextGetScroll(text, &topLines[i], &horizOffset);
	}

	// Modify the buffer to add wrapping 
	std::string fileString = TextGetWrappedEx(window->textArea_, 0, window->buffer_->BufGetLength());
	window->buffer_->BufSetAllEx(fileString);

	// restore the insert and scroll positions of each pane 
	for (i = 0; i <= window->nPanes_; i++) {
		text = i == 0 ? window->textArea_ : window->textPanes_[i - 1];
		TextSetCursorPos(text, insertPositions[i]);
		TextSetScroll(text, topLines[i], 0);
	}

	/* Show the user that something has happened by turning off
	   Continuous Wrap mode */
	window->SetToggleButtonState(window->continuousWrapItem_, False, True);
}

/*
 * Number of bytes read at once by cmpWinAgainstFile
 */
#define PREFERRED_CMPBUF_LEN 32768

/*
 * Check if the contens of the TextBuffer *buf is equal
 * the contens of the file named fileName. The format of
 * the file (UNIX/DOS/MAC) is handled properly.
 *
 * Return values
 *   0: no difference found
 *  !0: difference found or could not compare contents.
 */
static int cmpWinAgainstFile(Document *window, const char *fileName) {
	char fileString[PREFERRED_CMPBUF_LEN + 2];
	struct stat statbuf;
	int fileLen, restLen, nRead, bufPos, rv, offset, filePos;
	char pendingCR = 0;
	int fileFormat = window->fileFormat_;
	char message[MAXPATHLEN + 50];
	TextBuffer *buf = window->buffer_;
	FILE *fp;

	fp = fopen(fileName, "r");
	if (!fp)
		return (1);
	if (fstat(fileno(fp), &statbuf) != 0) {
		fclose(fp);
		return (1);
	}

	fileLen = statbuf.st_size;
	// For DOS files, we can't simply check the length 
	if (fileFormat != DOS_FILE_FORMAT) {
		if (fileLen != buf->BufGetLength()) {
			fclose(fp);
			return (1);
		}
	} else {
		// If a DOS file is smaller on disk, it's certainly different 
		if (fileLen < buf->BufGetLength()) {
			fclose(fp);
			return (1);
		}
	}

	/* For large files, the comparison can take a while. If it takes too long,
	   the user should be given a clue about what is happening. */
	sprintf(message, "Comparing externally modified %s ...", window->filename_.c_str());
	restLen = std::min<int>(PREFERRED_CMPBUF_LEN, fileLen);
	bufPos = 0;
	filePos = 0;
	while (restLen > 0) {
		AllWindowsBusy(message);
		if (pendingCR) {
			fileString[0] = pendingCR;
			offset = 1;
		} else {
			offset = 0;
		}

		nRead = fread(fileString + offset, sizeof(char), restLen, fp);
		if (nRead != restLen) {
			fclose(fp);
			AllWindowsUnbusy();
			return (1);
		}
		filePos += nRead;

		nRead += offset;

		// check for on-disk file format changes, but only for the first hunk 
		if (bufPos == 0 && fileFormat != FormatOfFileEx(view::string_view(fileString, nRead))) {
			fclose(fp);
			AllWindowsUnbusy();
			return (1);
		}

		if (fileFormat == MAC_FILE_FORMAT)
			ConvertFromMacFileString(fileString, nRead);
		else if (fileFormat == DOS_FILE_FORMAT)
			ConvertFromDosFileString(fileString, &nRead, &pendingCR);

		// Beware of 0 chars ! 
		buf->BufSubstituteNullChars(fileString, nRead);
		rv = buf->BufCmpEx(bufPos, nRead, fileString);
		if (rv) {
			fclose(fp);
			AllWindowsUnbusy();
			return (rv);
		}
		bufPos += nRead;
		restLen = std::min<int>(fileLen - filePos, PREFERRED_CMPBUF_LEN);
	}
	AllWindowsUnbusy();
	fclose(fp);
	if (pendingCR) {
		rv = buf->BufCmpEx(bufPos, 1, &pendingCR);
		if (rv) {
			return (rv);
		}
		bufPos += 1;
	}
	if (bufPos != buf->BufGetLength()) {
		return (1);
	}
	return (0);
}

/*
** Force ShowLineNumbers() to re-evaluate line counts for the window if line
** counts are required.
*/
static void forceShowLineNumbers(Document *window) {
	Boolean showLineNum = window->showLineNumbers_;
	if (showLineNum) {
		window->showLineNumbers_ = False;
		window->ShowLineNumbers(showLineNum);
	}
}
