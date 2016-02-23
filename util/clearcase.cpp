/*******************************************************************************
*                                                                              *
* clearcase.c -- Nirvana Editor ClearCase support utilities                    *
*                                                                              *
* Copyright (C) 2001 Mark Edel                                                 *
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
* March, 2001                                                                  *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/

#include "clearcase.h"
#include "MotifHelper.h"

#include <cstring>
#include <cstdlib>


namespace {

bool ClearCaseViewTagFound    = false;
const char *ClearCaseViewRoot = nullptr;
const char *ClearCaseViewTag  = nullptr;

}

const char *GetClearCaseVersionExtendedPath(const char *fullname) {
	return (strstr(fullname, "@@/"));
}

/*
** Return a string showing the ClearCase view tag.  If ClearCase is not in
** use, or a view is not set, nullptr is returned.
**
** If user has ClearCase and is in a view, CLEARCASE_ROOT will be set and
** the view tag can be extracted.  This check is safe and efficient enough
** that it doesn't impact non-clearcase users, so it is not conditionally
** compiled. (Thanks to Max Vohlken)
*/
const char *GetClearCaseViewTag(void) {
	if (!ClearCaseViewTagFound) {
		/* Extract the view name from the CLEARCASE_ROOT environment variable */
		if (const char *envPtr = getenv("CLEARCASE_ROOT")) {

			ClearCaseViewRoot = XtStringDup(envPtr);

			const char *tagPtr = strrchr(ClearCaseViewRoot, '/');
			if (tagPtr) {
				ClearCaseViewTag = ++tagPtr;
			}
		}
	}
	/* If we don't find it first time, we will never find it, so may just as
	 * well say that we have found it.
	 */
	ClearCaseViewTagFound = true;

	return ClearCaseViewTag;
}
