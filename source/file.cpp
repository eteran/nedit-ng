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

#include "file.h"
#include "TextBuffer.h"
#include "text.h"
#include "window.h"
#include "preferences.h"
#include "undo.h"
#include "menu.h"
#include "tags.h"
#include "server.h"
#include "interpret.h"
#include "Document.h"
#include "MotifHelper.h"
#include "../util/misc.h"
#include "../util/DialogF.h"
#include "../util/fileUtils.h"
#include "../util/getfiles.h"
#include "../util/printUtils.h"
#include "../util/utils.h"
#include "../util/nullable_string.h"

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
static int bckError(Document *window, const char *errString, const char *file);
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
	size_t pathlen;
	char *path;

	/*... test for creatability? */

	/* Find a (relatively) unique name for the new file */
	UniqueUntitledName(name, sizeof(name));

	/* create new window/document */
	if (inWindow)
		window = inWindow->CreateDocument(name);
	else
		window = new Document(name, geometry, iconic);

	path = window->path_;
	strcpy(window->filename_, name);
	strcpy(path, (defaultPath && *defaultPath) ? defaultPath : GetCurrentDirEx().c_str());
	pathlen = strlen(window->path_);

	/* do we have a "/" at the end? if not, add one */
	if (0 < pathlen && path[pathlen - 1] != '/' && pathlen < MAXPATHLEN - 1) {
		strcpy(&path[pathlen], "/");
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

	if (iconic && window->IsIconic())
		window->RaiseDocument();
	else
		window->RaiseDocumentWindow();

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
	char fullname[MAXPATHLEN];

	/* first look to see if file is already displayed in a window */
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
		/* open file in untitled document */
		window = inWindow;
		strcpy(window->path_, path);
		strcpy(window->filename_, name);
		if (!iconic && !bgOpen) {
			window->RaiseDocumentWindow();
		}
	}

	/* Open the file */
	if (!doOpen(window, name, path, flags)) {
		/* The user may have destroyed the window instead of closing the
		   warning dialog; don't close it twice */
		safeClose(window);

		return nullptr;
	}
	forceShowLineNumbers(window);

	/* Decide what language mode to use, trigger language specific actions */
	if(!languageMode)
		DetermineLanguageMode(window, True);
	else
		SetLanguageMode(window, FindLanguageMode(languageMode), True);

	/* update tab label and tooltip */
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

	/* Add the name to the convenience menu of previously opened files */
	strcpy(fullname, path);
	strcat(fullname, name);
	if (GetPrefAlwaysCheckRelTagsSpecs())
		AddRelTagsFile(GetPrefTagFile(), path, TAG);
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

	/* Can't revert untitled windows */
	if (!window->filenameSet_) {
		DialogF(DF_WARN, window->shell_, 1, "Error", "Window '%s' was never saved, can't re-read", "OK", window->filename_);
		return;
	}

	/* save insert & scroll positions of all of the panes to restore later */
	for (i = 0; i <= window->nPanes_; i++) {
		text = i == 0 ? window->textArea_ : window->textPanes_[i - 1];
		insertPositions[i] = TextGetCursorPos(text);
		TextGetScroll(text, &topLines[i], &horizOffsets[i]);
	}

	/* re-read the file, update the window title if new file is different */
	strcpy(name, window->filename_);
	strcpy(path, window->path_);
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
			/* Treat it like an externally modified file */
			window->lastModTime_ = 0;
			window->fileMissing_ = FALSE;
		}
		return;
	}
	forceShowLineNumbers(window);
	window->UpdateWindowTitle();
	window->UpdateWindowReadOnly();

	/* restore the insert and scroll positions of each pane */
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
	Document *p = WindowList;
	while (p) {
		if (p == window) {
			window->CloseWindow();
			return;
		}
		p = p->next_;
	}
}

static int doOpen(Document *window, const char *name, const char *path, int flags) {
	char fullname[MAXPATHLEN];
	struct stat statbuf;
	char *c;
	FILE *fp = nullptr;
	int fd;
	int resp;

	/* initialize lock reasons */
	CLEAR_ALL_LOCKS(window->lockReasons_);

	/* Update the window data structure */
	strcpy(window->filename_, name);
	strcpy(window->path_, path);
	window->filenameSet_ = TRUE;
	window->fileMissing_ = TRUE;

	/* Get the full name of the file */
	snprintf(fullname, sizeof(fullname), "%s%s", path, name);

	/* Open the file */
	/* The only advantage of this is if you use clearcase,
	   which messes up the mtime of files opened with r+,
	   even if they're never actually written.
	   To avoid requiring special builds for clearcase users,
	   this is now the default.
	*/
	{
		if ((fp = fopen(fullname, "r"))) {
			if (access(fullname, W_OK) != 0) {
				SET_PERM_LOCKED(window->lockReasons_, TRUE);
			}

		} else if (flags & CREATE && errno == ENOENT) {
			/* Give option to create (or to exit if this is the only window) */
			if (!(flags & SUPPRESS_CREATE_WARN)) {
				/* on Solaris 2.6, and possibly other OSes, dialog won't
		           show if parent window is iconized. */
				RaiseShellWindow(window->shell_, False);

				/* ask user for next action if file not found */
				if (WindowList == window && window->next_ == nullptr) {
					resp = DialogF(DF_WARN, window->shell_, 3, "New File", "Can't open %s:\n%s", "New File", "Cancel", "Exit NEdit", fullname, strerror(errno));
				} else {
					resp = DialogF(DF_WARN, window->shell_, 2, "New File", "Can't open %s:\n%s", "New File", "Cancel", fullname, strerror(errno));
				}

				if (resp == 2) {
					return FALSE;
				} else if (resp == 3) {
					exit(EXIT_SUCCESS);
				}
			}

			/* Test if new file can be created */
			if ((fd = creat(fullname, 0666)) == -1) {
				DialogF(DF_ERR, window->shell_, 1, "Error creating File", "Can't create %s:\n%s", "OK", fullname, strerror(errno));
				return FALSE;
			} else {
				close(fd);
				remove(fullname);
			}

			window->SetWindowModified(FALSE);
			if ((flags & PREF_READ_ONLY) != 0) {
				SET_USER_LOCKED(window->lockReasons_, TRUE);
			}
			window->UpdateWindowReadOnly();
			return TRUE;
		} else {
			/* A true error */
			DialogF(DF_ERR, window->shell_, 1, "Error opening File", "Could not open %s%s:\n%s", "OK", path, name, strerror(errno));
			return FALSE;
		}
	}

	/* Get the length of the file, the protection mode, and the time of the
	   last modification to the file */
	if (fstat(fileno(fp), &statbuf) != 0) {
		fclose(fp);
		window->filenameSet_ = FALSE; /* Temp. prevent check for changes. */
		DialogF(DF_ERR, window->shell_, 1, "Error opening File", "Error opening %s", "OK", name);
		window->filenameSet_ = TRUE;
		return FALSE;
	}

	if (S_ISDIR(statbuf.st_mode)) {
		fclose(fp);
		window->filenameSet_ = FALSE; /* Temp. prevent check for changes. */
		DialogF(DF_ERR, window->shell_, 1, "Error opening File", "Can't open directory %s", "OK", name);
		window->filenameSet_ = TRUE;
		return FALSE;
	}

#ifdef S_ISBLK
	if (S_ISBLK(statbuf.st_mode)) {
		fclose(fp);
		window->filenameSet_ = FALSE; /* Temp. prevent check for changes. */
		DialogF(DF_ERR, window->shell_, 1, "Error opening File", "Can't open block device %s", "OK", name);
		window->filenameSet_ = TRUE;
		return FALSE;
	}
#endif

	int fileLen = statbuf.st_size;

	/* Allocate space for the whole contents of the file (unfortunately) */
	auto fileString = new char[fileLen + 1]; /* +1 = space for null */
	if(!fileString) {
		fclose(fp);
		window->filenameSet_ = FALSE; /* Temp. prevent check for changes. */
		DialogF(DF_ERR, window->shell_, 1, "Error while opening File", "File is too large to edit", "OK");
		window->filenameSet_ = TRUE;
		return FALSE;
	}

	/* Read the file into fileString and terminate with a null */
	int readLen = fread(fileString, sizeof(char), fileLen, fp);
	if (ferror(fp)) {
		fclose(fp);
		window->filenameSet_ = FALSE; /* Temp. prevent check for changes. */
		DialogF(DF_ERR, window->shell_, 1, "Error while opening File", "Error reading %s:\n%s", "OK", name, strerror(errno));
		window->filenameSet_ = TRUE;
		delete [] fileString;
		return FALSE;
	}
	fileString[readLen] = '\0';

	/* Close the file */
	if (fclose(fp) != 0) {
		/* unlikely error */
		DialogF(DF_WARN, window->shell_, 1, "Error while opening File", "Unable to close file", "OK");
		/* we read it successfully, so continue */
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

	/* Detect and convert DOS and Macintosh format files */
	if (GetPrefForceOSConversion()) {
		window->fileFormat_ = FormatOfFile(fileString, readLen);
		if (window->fileFormat_ == DOS_FILE_FORMAT) {
			ConvertFromDosFileString(fileString, &readLen, nullptr);
		} else if (window->fileFormat_ == MAC_FILE_FORMAT) {
			ConvertFromMacFileString(fileString, readLen);
		}
	}

	/* Display the file contents in the text widget */
	window->ignoreModify_ = True;
	window->buffer_->BufSetAllEx(view::string_view(fileString, readLen));
	window->ignoreModify_ = False;

	/* Check that the length that the buffer thinks it has is the same
	   as what we gave it.  If not, there were probably nuls in the file.
	   Substitute them with another character.  If that is impossible, warn
	   the user, make the file read-only, and force a substitution */
	if (window->buffer_->BufGetLength() != readLen) {
		if (!window->buffer_->BufSubstituteNullChars(fileString, readLen)) {
			resp = DialogF(DF_ERR, window->shell_, 2, "Error while opening File", "Too much binary data in file.  You may view\nit, but not modify or re-save its contents.",
			               "View", "Cancel");
			if (resp == 2) {
				return FALSE;
			}

			SET_TMBD_LOCKED(window->lockReasons_, TRUE);
			for (c = fileString; c < &fileString[readLen]; c++) {
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

	/* Release the memory that holds fileString */
	delete [] fileString;

	/* Set window title and file changed flag */
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
	int fileLen, readLen;
	char *fileString;
	FILE *fp = nullptr;

	/* Open the file */
	fp = fopen(name, "rb");
	if(!fp) {
		DialogF(DF_ERR, window->shell_, 1, "Error opening File", "Could not open %s:\n%s", "OK", name, strerror(errno));
		return FALSE;
	}

	/* Get the length of the file */
	if (fstat(fileno(fp), &statbuf) != 0) {
		DialogF(DF_ERR, window->shell_, 1, "Error opening File", "Error opening %s", "OK", name);
		fclose(fp);
		return FALSE;
	}

	if (S_ISDIR(statbuf.st_mode)) {
		DialogF(DF_ERR, window->shell_, 1, "Error opening File", "Can't open directory %s", "OK", name);
		fclose(fp);
		return FALSE;
	}
	fileLen = statbuf.st_size;

	/* allocate space for the whole contents of the file */
	fileString = new char[fileLen + 1]; /* +1 = space for null */
	if(!fileString) {
		DialogF(DF_ERR, window->shell_, 1, "Error opening File", "File is too large to include", "OK");
		fclose(fp);
		return FALSE;
	}

	/* read the file into fileString and terminate with a null */
	readLen = fread(fileString, sizeof(char), fileLen, fp);
	if (ferror(fp)) {
		DialogF(DF_ERR, window->shell_, 1, "Error opening File", "Error reading %s:\n%s", "OK", name, strerror(errno));
		fclose(fp);
		delete [] fileString;
		return FALSE;
	}
	fileString[readLen] = '\0';

	/* Detect and convert DOS and Macintosh format files */
	switch (FormatOfFile(fileString, readLen)) {
	case DOS_FILE_FORMAT:
		ConvertFromDosFileString(fileString, &readLen, nullptr);
		break;
	case MAC_FILE_FORMAT:
		ConvertFromMacFileString(fileString, readLen);
		break;
	default:
		/*  Default is Unix, no conversion necessary.  */
		break;
	}

	/* If the file contained ascii nulls, re-map them */
	if (!window->buffer_->BufSubstituteNullChars(fileString, readLen)) {
		DialogF(DF_ERR, window->shell_, 1, "Error opening File", "Too much binary data in file", "OK");
	}

	/* close the file */
	if (fclose(fp) != 0) {
		/* unlikely error */
		DialogF(DF_WARN, window->shell_, 1, "Error opening File", "Unable to close file", "OK");
		/* we read it successfully, so continue */
	}

	/* insert the contents of the file in the selection or at the insert
	   position in the window if no selection exists */
	if (window->buffer_->primary_.selected) {
		window->buffer_->BufReplaceSelectedEx(fileString);
	} else {
		window->buffer_->BufInsertEx(TextGetCursorPos(window->lastFocus_), fileString);
	}

	/* release the memory that holds fileString */
	delete [] fileString;

	return TRUE;
}

/*
** Close all files and windows, leaving one untitled window
*/
int CloseAllFilesAndWindows(void) {
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
		if (WindowList == MacroRunWindow() && WindowList->next_) {
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

	/* Make sure that the window is not in iconified state */
	if (window->fileChanged_)
		window->RaiseDocumentWindow();

	/* If the window is a normal & unmodified file or an empty new file,
	   or if the user wants to ignore external modifications then
	   just close it.  Otherwise ask for confirmation first. */
	if (!window->fileChanged_ &&
	    /* Normal File */
	    ((!window->fileMissing_ && window->lastModTime_ > 0) ||
	     /* New File*/
	     (window->fileMissing_ && window->lastModTime_ == 0) ||
	     /* File deleted/modified externally, ignored by user. */
	     !GetPrefWarnFileMods())) {
		window->CloseWindow();
		/* up-to-date windows don't have outstanding backup files to close */
	} else {
		if (preResponse == PROMPT_SBC_DIALOG_RESPONSE) {
			response = DialogF(DF_WARN, window->shell_, 3, "Save File", "Save %s before closing?", "Yes", "No", "Cancel", window->filename_);
		} else {
			response = preResponse;
		}

		if (response == YES_SBC_DIALOG_RESPONSE) {
			/* Save */
			stat = SaveWindow(window);
			if (stat) {
				window->CloseWindow();
			} else {
				return FALSE;
			}
		} else if (response == NO_SBC_DIALOG_RESPONSE) {
			/* Don't Save */
			RemoveBackupFile(window);
			window->CloseWindow();
		} else /* 3 == Cancel */
		{
			return FALSE;
		}
	}
	return TRUE;
}

int SaveWindow(Document *window) {

	/* Try to ensure our information is up-to-date */
	CheckForChangesToFile(window);

	/* Return success if the file is normal & unchanged or is a
	    read-only file. */
	if ((!window->fileChanged_ && !window->fileMissing_ && window->lastModTime_ > 0) || IS_ANY_LOCKED_IGNORING_PERM(window->lockReasons_))
		return TRUE;
	/* Prompt for a filename if this is an Untitled window */
	if (!window->filenameSet_)
		return SaveWindowAs(window, nullptr, False);

	/* Check for external modifications and warn the user */
	if (GetPrefWarnFileMods() && fileWasModifiedExternally(window)) {
		int stat = DialogF(DF_WARN, window->shell_, 2, "Save File", "%s has been modified by another program.\n\n"
		                                                       "Continuing this operation will overwrite any external\n"
		                                                       "modifications to the file since it was opened in NEdit,\n"
		                                                       "and your work or someone else's may potentially be lost.\n\n"
		                                                       "To preserve the modified file, cancel this operation and\n"
		                                                       "use Save As... to save this file under a different name,\n"
		                                                       "or Revert to Saved to revert to the modified version.",
		               "Continue", "Cancel", window->filename_);
		if (stat == 2) {
			/* Cancel and mark file as externally modified */
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

int SaveWindowAs(Document *window, const char *newName, int addWrap) {
	int response, retVal, fileFormat;
	char fullname[MAXPATHLEN], filename[MAXPATHLEN], pathname[MAXPATHLEN];
	Document *otherWindow;

	/* Get the new name for the file */
	if(!newName) {
		response = PromptForNewFile(window, "Save File As", fullname, &fileFormat, &addWrap);
		if (response != GFN_OK)
			return FALSE;
		window->fileFormat_ = fileFormat;
	} else {
		strcpy(fullname, newName);
	}

	if (NormalizePathname(fullname) == 1) {
		return False;
	}

	/* Add newlines if requested */
	if (addWrap)
		addWrapNewlines(window);

	if (ParseFilename(fullname, filename, pathname) != 0) {
		return FALSE;
	}

	/* If the requested file is this file, just save it and return */
	if (!strcmp(window->filename_, filename) && !strcmp(window->path_, pathname)) {
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
		response = DialogF(DF_WARN, window->shell_, 2, "File open", "%s is open in another NEdit window", "Cancel", "Close Other Window", filename);

		if (response == 1) {
			return FALSE;
		}
		if (otherWindow == FindWindowWithFile(filename, pathname)) {
			if (!CloseFileAndWindow(otherWindow, PROMPT_SBC_DIALOG_RESPONSE)) {
				return FALSE;
			}
		}
	}

	/* Destroy the file closed property for the original file */
	DeleteFileClosedProperty(window);

	/* Change the name of the file and save it under the new name */
	RemoveBackupFile(window);
	strcpy(window->filename_, filename);
	strcpy(window->path_, pathname);
	window->fileMode_ = 0;
	window->fileUid_ = 0;
	window->fileGid_ = 0;
	CLEAR_ALL_LOCKS(window->lockReasons_);
	retVal = doSave(window);
	window->UpdateWindowReadOnly();
	window->RefreshTabState();

	/* Add the name to the convenience menu of previously opened files */
	AddToPrevOpenMenu(fullname);

	/*  If name has changed, language mode may have changed as well, unless
	    it's an Untitled window for which the user already set a language
	    mode; it's probably the right one.  */
	if (PLAIN_LANGUAGE_MODE == window->languageMode_ || window->filenameSet_) {
		DetermineLanguageMode(window, False);
	}
	window->filenameSet_ = True;

	/* Update the stats line and window title with the new filename */
	window->UpdateWindowTitle();
	window->UpdateStatsLine();

	window->SortTabBar();
	return retVal;
}

static bool doSave(Document *window) {
	char fullname[MAXPATHLEN];
	struct stat statbuf;
	FILE *fp;
	int result;

	/* Get the full name of the file */
	strcpy(fullname, window->path_);
	strcat(fullname, window->filename_);

	/*  Check for root and warn him if he wants to write to a file with
	    none of the write bits set.  */
	if ((getuid() == 0) && (stat(fullname, &statbuf) == 0) && !(statbuf.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH))) {
		result = DialogF(DF_WARN, window->shell_, 2, "Writing Read-only File", "File '%s' is marked as read-only.\n"
		                                                                      "Do you want to save anyway?",
		                 "Save", "Cancel", window->filename_);
		if (result != 1) {
			return True;
		}
	}

	/* add a terminating newline if the file doesn't already have one for
	   Unix utilities which get confused otherwise
	   NOTE: this must be done _before_ we create/open the file, because the
	         (potential) buffer modification can trigger a check for file
	         changes. If the file is created for the first time, it has
	         zero size on disk, and the check would falsely conclude that the
	         file has changed on disk, and would pop up a warning dialog */
	if (window->buffer_->BufGetCharacter(window->buffer_->BufGetLength() - 1) != '\n' && window->buffer_->BufGetLength() != 0 && GetPrefAppendLF()) {
		window->buffer_->BufInsertEx(window->buffer_->BufGetLength(), "\n");
	}

	/* open the file */
	fp = fopen(fullname, "wb");
	if(!fp) {
		result = DialogF(DF_WARN, window->shell_, 2, "Error saving File", "Unable to save %s:\n%s\n\nSave as a new file?", "Save As...", "Cancel", window->filename_, strerror(errno));

		if (result == 1) {
			return SaveWindowAs(window, nullptr, 0);
		}
		return FALSE;
	}

	/* get the text buffer contents and its length */
	std::string fileString = window->buffer_->BufGetAllEx();

	/* If null characters are substituted for, put them back */
	window->buffer_->BufUnsubstituteNullCharsEx(fileString);

	/* If the file is to be saved in DOS or Macintosh format, reconvert */
	if (window->fileFormat_ == DOS_FILE_FORMAT) {
		if (!ConvertToDosFileStringEx(fileString)) {
			DialogF(DF_ERR, window->shell_, 1, "Out of Memory", "Out of memory!  Try\nsaving in Unix format", "OK");

			// NOTE(eteran): fixes resource leak error
			fclose(fp);
			return FALSE;
		}
	} else if (window->fileFormat_ == MAC_FILE_FORMAT) {
		ConvertToMacFileStringEx(fileString);
	}

	/* write to the file */
	fwrite(fileString.data(), sizeof(char), fileString.size(), fp);
	
	
	if (ferror(fp)) {
		DialogF(DF_ERR, window->shell_, 1, "Error saving File", "%s not saved:\n%s", "OK", window->filename_, strerror(errno));
		fclose(fp);
		remove(fullname);
		return FALSE;
	}

	/* close the file */
	if (fclose(fp) != 0) {
		DialogF(DF_ERR, window->shell_, 1, "Error closing File", "Error closing file:\n%s", "OK", strerror(errno));
		return FALSE;
	}

	/* success, file was written */
	window->SetWindowModified(FALSE);

	/* update the modification time */
	if (stat(fullname, &statbuf) == 0) {
		window->lastModTime_ = statbuf.st_mtime;
		window->fileMissing_ = FALSE;
		window->device_ = statbuf.st_dev;
		window->inode_ = statbuf.st_ino;
	} else {
		/* This needs to produce an error message -- the file can't be accessed! */
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

	/* Generate a name for the autoSave file */
	std::string name = backupFileNameEx(window);

	/* remove the old backup file.
	   Well, this might fail - we'll notice later however. */
	remove(name.c_str());

	/* open the file, set more restrictive permissions (using default
	    permissions was somewhat of a security hole, because permissions were
	    independent of those of the original file being edited */
	if ((fd = open(name.c_str(), O_CREAT | O_EXCL | O_WRONLY, S_IRUSR | S_IWUSR)) < 0 || (fp = fdopen(fd, "w")) == nullptr) {
		DialogF(DF_WARN, window->shell_, 1, "Error writing Backup", "Unable to save backup for %s:\n%s\n"
		                                                           "Automatic backup is now off",
		        "OK", window->filename_, strerror(errno));
		window->autoSave_ = FALSE;
		window->SetToggleButtonState(window->autoSaveItem_, FALSE, FALSE);
		return FALSE;
	}

	/* get the text buffer contents and its length */
	std::string fileString = window->buffer_->BufGetAllEx();

	/* If null characters are substituted for, put them back */
	window->buffer_->BufUnsubstituteNullCharsEx(fileString);

	/* add a terminating newline if the file doesn't already have one */
	if (!fileString.empty() && fileString.back() != '\n') {
		fileString.append("\n"); /* null terminator no longer needed */
	}

	/* write out the file */
	fwrite(fileString.data(), sizeof(char), fileString.size(), fp);
	if (ferror(fp)) {
		DialogF(DF_ERR, window->shell_, 1, "Error saving Backup", "Error while saving backup for %s:\n%s\n"
		                                                         "Automatic backup is now off",
		        "OK", window->filename_, strerror(errno));
		fclose(fp);
		remove(name.c_str());
		window->autoSave_ = FALSE;
		return FALSE;
	}

	/* close the backup file */
	if (fclose(fp) != 0) {
		return FALSE;
	}

	return TRUE;
}

/*
** Remove the backup file associated with this window
*/
void RemoveBackupFile(Document *window) {

	/* Don't delete backup files when backups aren't activated. */
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
		snprintf(buf, sizeof(buf), "%s~%s", window->path_, window->filename_);
		return buf;
	} else {
		snprintf(buf, sizeof(buf), "~%s", window->filename_);
		return PrependHomeEx(buf);
	}
}

/*
** If saveOldVersion is on, copies the existing version of the file to
** <filename>.bck in anticipation of a new version being saved.  Returns
** True if backup fails and user requests that the new file not be written.
*/
static bool writeBckVersion(Document *window) {
	char fullname[MAXPATHLEN];
	char bckname[MAXPATHLEN];
	struct stat statbuf;
	int in_fd;
	int out_fd;
	
	static const size_t IO_BUFFER_SIZE = (1024 * 1024);

	/* Do only if version backups are turned on */
	if (!window->saveOldVersion_) {
		return false;
	}

	/* Get the full name of the file */
	int r = snprintf(fullname, sizeof(fullname), "%s%s", window->path_, window->filename_);

	/* Generate name for old version */
	if (r >= MAXPATHLEN) {
		return bckError(window, "file name too long", window->filename_);
	}
	snprintf(bckname, sizeof(bckname), "%s.bck", fullname);

	/* Delete the old backup file */
	/* Errors are ignored; we'll notice them later. */
	remove(bckname);

	/* open the file being edited.  If there are problems with the
	   old file, don't bother the user, just skip the backup */
	in_fd = open(fullname, O_RDONLY);
	if (in_fd < 0) {
		return false;
	}

	/* Get permissions of the file.
	   We preserve the normal permissions but not ownership, extended
	   attributes, et cetera. */
	if (fstat(in_fd, &statbuf) != 0) {
		return false;
	}

	/* open the destination file exclusive and with restrictive permissions. */
	out_fd = open(bckname, O_CREAT | O_EXCL | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
	if (out_fd < 0) {
		return bckError(window, "Error open backup file", bckname);
	}

	/* Set permissions on new file */
	if (fchmod(out_fd, statbuf.st_mode) != 0) {
		close(in_fd);
		close(out_fd);
		remove(bckname);
		return bckError(window, "fchmod() failed", bckname);
	}

	/* Allocate I/O buffer */
	auto io_buffer = new char[IO_BUFFER_SIZE];

	/* copy loop */
	for (;;) {
		ssize_t bytes_read;
		ssize_t bytes_written;
		bytes_read = read(in_fd, io_buffer, IO_BUFFER_SIZE);

		if (bytes_read < 0) {
			close(in_fd);
			close(out_fd);
			remove(bckname);
			delete [] io_buffer;
			return bckError(window, "read() error", window->filename_);
		}

		if (bytes_read == 0) {
			break; /* EOF */
		}

		/* write to the file */
		bytes_written = write(out_fd, io_buffer, (size_t)bytes_read);
		if (bytes_written != bytes_read) {
			close(in_fd);
			close(out_fd);
			remove(bckname);
			delete [] io_buffer;
			return bckError(window, strerror(errno), bckname);
		}
	}

	/* close the input and output files */
	close(in_fd);
	close(out_fd);

	delete [] io_buffer;

	return false;
}

/*
** Error processing for writeBckVersion, gives the user option to cancel
** the subsequent save, or continue and optionally turn off versioning
*/
static int bckError(Document *window, const char *errString, const char *file) {
	int resp;

	resp = DialogF(DF_ERR, window->shell_, 3, "Error writing Backup", "Couldn't write .bck (last version) file.\n%s: %s", "Cancel Save", "Turn off Backups", "Continue", file, errString);
	if (resp == 1)
		return TRUE;
	if (resp == 2) {
		window->saveOldVersion_ = FALSE;
		window->SetToggleButtonState(window->saveLastItem_, FALSE, FALSE);
	}
	return FALSE;
}

void PrintWindow(Document *window, int selectedOnly) {
	TextBuffer *buf = window->buffer_;
	TextSelection *sel = &buf->primary_;
	std::string fileString;

	/* get the contents of the text buffer from the text area widget.  Add
	   wrapping newlines if necessary to make it match the displayed text */
	if (selectedOnly) {
		if (!sel->selected) {
			XBell(TheDisplay, 0);
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

	/* If null characters are substituted for, put them back */
	buf->BufUnsubstituteNullCharsEx(fileString);

	/* add a terminating newline if the file doesn't already have one */
	if (!fileString.empty() && fileString.back() != '\n') {
		fileString.push_back('\n'); /* null terminator no longer needed */
	}

	/* Print the string */
	PrintString(fileString, fileString.size(), window->shell_, window->filename_);
}

/*
** Print a string (length is required).  parent is the dialog parent, for
** error dialogs, and jobName is the print title.
*/
void PrintString(const std::string &string, int length, Widget parent, const std::string &jobName) {
	// TODO(eteran): get the temp directory dynamically
	char tmpFileName[L_tmpnam] = "/tmp/nedit-XXXXXX"; /* L_tmpnam defined in stdio.h */
	int fd = mkstemp(tmpFileName);
	if (fd < 0) {
		DialogF(DF_WARN, parent, 1, "Error while Printing", "Unable to write file for printing:\n%s", "OK", strerror(errno));
		return;
	}

	FILE *fp;

	/* open the temporary file */
	if ((fp = fdopen(fd, "w")) == nullptr) {
		DialogF(DF_WARN, parent, 1, "Error while Printing", "Unable to write file for printing:\n%s", "OK", strerror(errno));
		return;
	}

	/* write to the file */
	fwrite(string.data(), sizeof(char), length, fp);
	if (ferror(fp)) {
		DialogF(DF_ERR, parent, 1, "Error while Printing", "%s not printed:\n%s", "OK", jobName.c_str(), strerror(errno));
		fclose(fp); /* should call close(fd) in turn! */
		remove(tmpFileName);
		return;
	}

	/* close the temporary file */
	if (fclose(fp) != 0) {
		DialogF(DF_ERR, parent, 1, "Error while Printing", "Error closing temp. print file:\n%s", "OK", strerror(errno));
		remove(tmpFileName);
		return;
	}

	/* Print the temporary file, then delete it and return success */
	PrintFile(parent, tmpFileName, jobName);
	remove(tmpFileName);
	return;
}

/*
** Wrapper for GetExistingFilename which uses the current window's path
** (if set) as the default directory.
*/
int PromptForExistingFile(Document *window, const char *prompt, char *fullname) {
	int retVal;

	/* Temporarily set default directory to window->path_, prompt for file,
	   then, if the call was unsuccessful, restore the original default
	   directory */
	auto savedDefaultDir = GetFileDialogDefaultDirectoryEx();
	if (*window->path_ != '\0')
		SetFileDialogDefaultDirectory(nullable_string(window->path_));
	retVal = GetExistingFilename(window->shell_, prompt, fullname);
	if (retVal != GFN_OK)
		SetFileDialogDefaultDirectory(savedDefaultDir);

	return retVal;
}

/*
** Wrapper for HandleCustomNewFileSB which uses the current window's path
** (if set) as the default directory, and asks about embedding newlines
** to make wrapping permanent.
*/
int PromptForNewFile(Document *window, const char *prompt, char *fullname, int *fileFormat, int *addWrap) {
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
	if (*window->path_ != '\0') {
		
		SetFileDialogDefaultDirectory(nullable_string(window->path_));
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
	retVal = HandleCustomNewFileSB(fileSB, fullname, window->filenameSet_ ? window->filename_ : nullptr);

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
		
		Document *w;
		for (w = WindowList; w != nullptr; w = w->next_) {
			if (!strcmp(w->filename_, name)) {
				break;
			}
		}
			
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

	*(Bool *)clientData = TRUE;
}

/*
** Check if the file in the window was changed by an external source.
** and put up a warning dialog if it has.
*/
void CheckForChangesToFile(Document *window) {
	static Document *lastCheckWindow = nullptr;
	static Time lastCheckTime = 0;
	char fullname[MAXPATHLEN];
	struct stat statbuf;
	Time timestamp;
	FILE *fp;
	int resp, silent = 0;
	XWindowAttributes winAttr;
	Boolean windowIsDestroyed = False;

	if (!window->filenameSet_)
		return;

	/* If last check was very recent, don't impact performance */
	timestamp = XtLastTimestampProcessed(XtDisplay(window->shell_));
	if (window == lastCheckWindow && timestamp - lastCheckTime < MOD_CHECK_INTERVAL)
		return;
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

	/* Get the file mode and modification time */
	strcpy(fullname, window->path_);
	strcat(fullname, window->filename_);
	if (stat(fullname, &statbuf) != 0) {
		/* Return if we've already warned the user or we can't warn him now */
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
			const char *title;
			const char *body;

			/* See note below about pop-up timing and XUngrabPointer */
			XUngrabPointer(XtDisplay(window->shell_), timestamp);

			/* If the window (and the dialog) are destroyed while the dialog
			   is up (typically closed via the window manager), we should
			   avoid accessing the window afterwards. */
			XtAddCallback(window->shell_, XmNdestroyCallback, modifiedWindowDestroyedCB, &windowIsDestroyed);

			/*  Set title, message body and button to match stat()'s error.  */
			switch (errno) {
			case ENOENT:
				/*  A component of the path file_name does not exist.  */
				title = "File not Found";
				body = "File '%s' (or directory in its path)\n"
				       "no longer exists.\n"
				       "Another program may have deleted or moved it.";
				resp = DialogF(DF_ERR, window->shell_, 2, title, body, "Save", "Cancel", window->filename_);
				break;
			case EACCES:
				/*  Search permission denied for a path component. We add
				    one to the response because Re-Save wouldn't really
				    make sense here.  */
				title = "Permission Denied";
				body = "You no longer have access to file '%s'.\n"
				       "Another program may have changed the permissions of\n"
				       "one of its parent directories.";
				resp = 1 + DialogF(DF_ERR, window->shell_, 1, title, body, "Cancel", window->filename_);
				break;
			default:
				/*  Everything else. This hints at an internal error (eg.
				    ENOTDIR) or at some bad state at the host.  */
				title = "File not Accessible";
				body = "Error while checking the status of file '%s':\n"
				       "    '%s'\n"
				       "Please make sure that no data is lost before closing\n"
				       "this window.";
				resp = DialogF(DF_ERR, window->shell_, 2, title, body, "Save", "Cancel", window->filename_, strerror(errno));
				break;
			}

			if (!windowIsDestroyed) {
				XtRemoveCallback(window->shell_, XmNdestroyCallback, modifiedWindowDestroyedCB, &windowIsDestroyed);
			}

			switch (resp) {
			case 1:
				SaveWindow(window);
				break;
				/*  Good idea, but this leads to frequent crashes, see
				    SF#1578869. Reinsert this if circumstances change by
				    uncommenting this part and inserting a "Close" button
				    before each Cancel button above.
				case 2:
				    CloseWindow(window);
				    return;
				*/
			}
		}

		/* A missing or (re-)saved file can't be read-only. */
		/*  TODO: A document without a file can be locked though.  */
		/* Make sure that the window was not destroyed behind our back! */
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
		if ((fp = fopen(fullname, "r"))) {
			int readOnly;
			fclose(fp);
#ifndef DONT_USE_ACCESS
			readOnly = access(fullname, W_OK) != 0;
#else
			if (((fp = fopen(fullname, "r+")) != nullptr)) {
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
		window->lastModTime_ = 0; /* Inhibit further warnings */
		window->fileMissing_ = FALSE;
		if (!GetPrefWarnFileMods())
			return;
		if (GetPrefWarnRealFileMods() && !cmpWinAgainstFile(window, fullname)) {
			/* Contents hasn't changed. Update the modification time. */
			window->lastModTime_ = statbuf.st_mtime;
			return;
		}
		XUngrabPointer(XtDisplay(window->shell_), timestamp);
		if (window->fileChanged_)
			resp = DialogF(DF_WARN, window->shell_, 2, "File modified externally", "%s has been modified by another program.  Reload?\n\n"
			                                                                      "WARNING: Reloading will discard changes made in this\n"
			                                                                      "editing session!",
			               "Reload", "Cancel", window->filename_);
		else
			resp = DialogF(DF_WARN, window->shell_, 2, "File modified externally", "%s has been modified by another\nprogram.  Reload?", "Reload", "Cancel", window->filename_);
		if (resp == 1)
			RevertToSaved(window);
	}
}

/*
** Return true if the file displayed in window has been modified externally
** to nedit.  This should return FALSE if the file has been deleted or is
** unavailable.
*/
static int fileWasModifiedExternally(Document *window) {
	char fullname[MAXPATHLEN];
	struct stat statbuf;

	if (!window->filenameSet_)
		return FALSE;
	/* if (window->lastModTime_ == 0)
	return FALSE; */
	strcpy(fullname, window->path_);
	strcat(fullname, window->filename_);
	if (stat(fullname, &statbuf) != 0)
		return FALSE;
	if (window->lastModTime_ == statbuf.st_mtime)
		return FALSE;
	if (GetPrefWarnRealFileMods() && !cmpWinAgainstFile(window, fullname)) {
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
		XBell(TheDisplay, 0);
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

	int resp;
	int *addWrap = (int *)clientData;

	if (XmToggleButtonGetState(w)) {
		resp = DialogF(DF_WARN, w, 2, "Add Wrap", "This operation adds permanent line breaks to\n"
		                                          "match the automatic wrapping done by the\n"
		                                          "Continuous Wrap mode Preferences Option.\n\n"
		                                          "*** This Option is Irreversable ***\n\n"
		                                          "Once newlines are inserted, continuous wrapping\n"
		                                          "will no longer work automatically on these lines",
		               "OK", "Cancel");
		if (resp == 2) {
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

	/* save the insert and scroll positions of each pane */
	for (i = 0; i <= window->nPanes_; i++) {
		text = i == 0 ? window->textArea_ : window->textPanes_[i - 1];
		insertPositions[i] = TextGetCursorPos(text);
		TextGetScroll(text, &topLines[i], &horizOffset);
	}

	/* Modify the buffer to add wrapping */
	std::string fileString = TextGetWrappedEx(window->textArea_, 0, window->buffer_->BufGetLength());
	window->buffer_->BufSetAllEx(fileString);

	/* restore the insert and scroll positions of each pane */
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
	/* For DOS files, we can't simply check the length */
	if (fileFormat != DOS_FILE_FORMAT) {
		if (fileLen != buf->BufGetLength()) {
			fclose(fp);
			return (1);
		}
	} else {
		/* If a DOS file is smaller on disk, it's certainly different */
		if (fileLen < buf->BufGetLength()) {
			fclose(fp);
			return (1);
		}
	}

	/* For large files, the comparison can take a while. If it takes too long,
	   the user should be given a clue about what is happening. */
	sprintf(message, "Comparing externally modified %s ...", window->filename_);
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

		/* check for on-disk file format changes, but only for the first hunk */
		if (bufPos == 0 && fileFormat != FormatOfFile(fileString, nRead)) {
			fclose(fp);
			AllWindowsUnbusy();
			return (1);
		}

		if (fileFormat == MAC_FILE_FORMAT)
			ConvertFromMacFileString(fileString, nRead);
		else if (fileFormat == DOS_FILE_FORMAT)
			ConvertFromDosFileString(fileString, &nRead, &pendingCR);

		/* Beware of 0 chars ! */
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
