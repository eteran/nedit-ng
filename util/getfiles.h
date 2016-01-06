/*******************************************************************************
*                                                                              *
* getfiles.h -- Nirvana Editor File Handling Header File                       *
*                                                                              *
* Copyright 2003 The NEdit Developers                                          *
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

#ifndef GETFILES_H_
#define GETFILES_H_

#include <X11/Intrinsic.h>
#include <string>
#include <boost/optional.hpp>

#define GFN_OK 1     /* Get Filename OK constant     */
#define GFN_CANCEL 2 /* Get Filename Cancel constant */

int GetExistingFilename(Widget parent, const char *promptString, char *filename);
int GetNewFilename(Widget parent, const char *promptString, char *filename, const char *defaultName);
int HandleCustomExistFileSB(Widget existFileSB, char *filename);
int HandleCustomNewFileSB(Widget newFileSB, char *filename, const char *defaultName);
boost::optional<std::string> GetFileDialogDefaultDirectoryEx(void);
boost::optional<std::string> GetFileDialogDefaultPatternEx(void);
void SetFileDialogDefaultDirectory(boost::optional<std::string> dir);
void SetFileDialogDefaultPattern(boost::optional<std::string> pattern);
void SetGetEFTextFieldRemoval(int state);

#endif
