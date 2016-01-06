/*******************************************************************************
*                                                                              *
* utils.h -- Nirvana Editor Utilities Header File                              *
*                                                                              *
* Copyright 2002 The NEdit Developers                                          *
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

#ifndef UTILS_H_
#define UTILS_H_

#include <sys/param.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <string>
#include "string_view.h"
#include "path_error.h"

std::string GetCurrentDirEx();
std::string GetHomeDirEx();
std::string GetNameOfHostEx();
std::string GetRCFileNameEx(int type);
std::string GetUserNameEx();
std::string PrependHomeEx(view::string_view filename);

/* N_FILE_TYPES must be the last entry!! This saves us from counting. */
enum {
	NEDIT_RC,
	AUTOLOAD_NM,
	NEDIT_HISTORY,
	N_FILE_TYPES
};

/* If anyone knows where to get this from system include files (in a machine
   independent way), please change this (L_cuserid is apparently not ANSI) */
#define MAXUSERNAMELEN 32

/* Ditto for the maximum length for a node name.  SYS_NMLN is not available
   on most systems, and I don't know what the portable alternative is. */
#define MAXNODENAMELEN (MAXPATHLEN + 2)

#endif
