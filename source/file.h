/*******************************************************************************
*                                                                              *
* file.h -- Nirvana Editor File Header File                                    *
*                                                                              *
* Copyright 2004 The NEdit Developers                                          *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute versions of this program linked to  *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for    *
* more details.                                                                *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* July 31, 2001                                                                *
*                                                                              *
*******************************************************************************/

#ifndef FILE_H_
#define FILE_H_

#include "nedit.h"
#include "fileUtils.h"
#include "string_view.h"

/* flags for EditExistingFile */
enum EditFlags {
	CREATE               = 1,
	SUPPRESS_CREATE_WARN = 2,
	PREF_READ_ONLY       = 4
};

#define PROMPT_SBC_DIALOG_RESPONSE 0
#define YES_SBC_DIALOG_RESPONSE    1
#define NO_SBC_DIALOG_RESPONSE     2

int CheckReadOnly(Document *window);
int CloseAllFilesAndWindows();
int CloseFileAndWindow(Document *window, int preResponse);
int IncludeFile(Document *window, const char *name);
QString PromptForExistingFile(Document *window, const QString &prompt);
int PromptForNewFile(Document *window, const char *prompt, char *fullname, FileFormats *fileFormat, bool *addWrap);
int SaveWindowAs(Document *window, const char *newName, bool addWrap);
int SaveWindow(Document *window);
int WriteBackupFile(Document *window);
void CheckForChangesToFile(Document *window);
void PrintString(const std::string &string, const std::string &jobName);
void PrintWindow(Document *window, bool selectedOnly);
void RemoveBackupFile(Document *window);
void RevertToSaved(Document *window);
QString UniqueUntitledName();
Document *EditExistingFile(Document *inWindow, const QString &name, const QString &path, int flags, char *geometry, int iconic, const char *languageMode, int tabbed, int bgOpen);
Document *EditNewFile(Document *inWindow, char *geometry, int iconic, const char *languageMode, const char *defaultPath);

#endif
