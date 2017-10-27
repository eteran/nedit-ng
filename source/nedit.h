/*******************************************************************************
*                                                                              *
* nedit.h -- Nirvana Editor Common Header File                                 *
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

#ifndef NEDIT_H_
#define NEDIT_H_

#include "ShowMatchingStyle.h"
#include "TextSelection.h"

/* Tuning parameters */
#define SEARCHMAX 5119         /* Maximum length of search/replace strings */
#define MAX_PANES 6            /* Max # of ADDITIONAL text editing panes  that can be added to a window */

#define AUTOSAVE_CHAR_LIMIT 80 /* set higher on VMS becaus saving is slower */
#define AUTOSAVE_OP_LIMIT   8  /* number of distinct editing operations user can do before NEdit gens. new backup file */
#define MIN_LINE_NUM_COLS 4 /* Min. # of columns in line number display */

/*  This enum must be kept in parallel to the array TruncSubstitutionModes[]
    in preferences.c  */
enum TruncSubstitution {
    TRUNCSUBST_SILENT,
    TRUNCSUBST_FAIL,
    TRUNCSUBST_WARN,
    TRUNCSUBST_IGNORE
};

enum virtKeyOverride {
    VIRT_KEY_OVERRIDE_NEVER,
    VIRT_KEY_OVERRIDE_AUTO,
    VIRT_KEY_OVERRIDE_ALWAYS
};

extern bool IsServer;

#endif
