/*******************************************************************************
*                                                                              *
* Getfiles.c -- File Interface Routines                                        *
*                                                                              *
* Copyright (C) 1999 Mark Edel                                                 *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute version of this program linked to   *
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
* May 23, 1991                                                                 *
*                                                                              *
* Written by Donna Reid                                                        *
*                                                                              *
* modified 11/5/91 by JMK: integrated changes made by M. Edel; updated for     *
*                          destroy widget problem (took out ManageModalDialog  *
*                          call; added comments.                               *
*          10/1/92 by MWE: Added help dialog and fixed a few bugs              *
*           4/7/93 by DR:  Port to VMS                                         *
*           6/1/93 by JMK: Integrate Port and changes by MWE to make           *
*                          directories "sticky" and a fix to prevent opening   *
*                          a directory when no filename was specified          *
*          6/24/92 by MWE: Made filename list and directory list typeable,     *
*                          set initial focus to filename list                  *
*          6/25/93 by JMK: Fix memory leaks found by Purify.                   *
*                                                                              *
* Included are two routines written using Motif for accessing files:           *
*                                                                              *
* GetExistingFilename  presents a FileSelectionBox dialog where users can      *
*                      choose an existing file to open.                        *
*                                                                              *
*******************************************************************************/

#include "getfiles.h"


namespace {

#if 0
/* Text for help button help display */
/* ... needs variant for VMS */
const char *HelpExist = "The file open dialog shows a list of directories on the left, and a list \
of files on the right.  Double clicking on a file name in the list on the \
right, or selecting it and pressing the OK button, will open that file.  \
Double clicking on a directory name, or selecting \
it and pressing \"Filter\", will move into that directory.  To move upwards in \
the directory tree, double click on the directory entry ending in \"..\".  \
You can also begin typing a file name to select from the file list, or \
directly type in directory and file specifications in the \
field labeled \"Filter\".\n\
\n\
If you use the filter field, remember to include \
either a file name, \"*\" is acceptable, or a trailing \"/\".  If \
you don't, the name after the last \"/\" is interpreted as the file name to \
match.  When you leave off the file name or trailing \"/\", you won't see \
any files to open in the list \
because the filter specification matched the directory file itself, rather \
than the files in the directory.";

const char *HelpNew = "This dialog allows you to create a new file, or to save the current file \
under a new name.  To specify a file \
name in the current directory, complete the name displayed in the \"Save File \
As:\" field near the bottom of the dialog.  If you delete or change \
the path shown in the field, the file will be saved using whatever path \
you type, provided that it is a valid Unix file specification.\n\
\n\
To replace an existing file, select it from the Files list \
and press \"OK\", or simply double click on the name.\n\
\n\
To save a file in another directory, use the Directories list \
to move around in the file system hierarchy.  Double clicking on \
directory names in the list, or selecting them and pressing the \
\"Filter\" button will select that directory.  To move upwards \
in the directory tree, double \
click on the directory entry ending in \"..\".  You can also move directly \
to a directory by typing the file specification of the path in the \"Filter\" \
field and pressing the \"Filter\" button.";
#endif

}


