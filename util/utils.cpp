/*******************************************************************************
*                                                                              *
* utils.c -- miscellaneous non-GUI routines                                    *
*                                                                              *
* Copyright (C) 2002 Mark Edel                                                 *
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
* for more details.*                                                           *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
*******************************************************************************/

#include <QDir>
#include <QHostInfo>
#include <QStandardPaths>

#include "utils.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <pwd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>


/* return non-nullptr value for the current working directory.
   If system call fails, provide a fallback value */
QString GetCurrentDirEx(void) {
	return QDir::currentPath();
}

/* return a non-nullptr value for the user's home directory,
   without trailing slash.
   We try the  environment var and the system user database. */
QString GetHomeDirEx(void) {
    return QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
}

/*
** Return a pointer to the username of the current user in a statically
** allocated string.
*/
QString GetUserNameEx(void) {
	/* cuserid has apparently been dropped from the ansi C standard, and if
	   strict ansi compliance is turned on (on Sun anyhow, maybe others), calls
	   to cuserid fail to compile.  Older versions of nedit try to use the
	   getlogin call first, then if that fails, use getpwuid and getuid.  This
	   results in the user-name of the original terminal being used, which is
	   not correct when the user uses the su command.  Now, getpwuid only: */

	static char *userName = nullptr;

	if (userName) {
		return QLatin1String(userName);
	}

	if(const struct passwd *passwdEntry = getpwuid(getuid())) {
		// NOTE(eteran): so, this is effecively a one time memory leak.
		//               it is tollerable, but probably should be 
		//               improved in the future.
		userName = qstrdup(passwdEntry->pw_name);
		return QLatin1String(userName);
	}
	
	/* This is really serious, but sometimes username service
	   is misconfigured through no fault of the user.  Be nice
	   and let the user start nc anyway. */
	perror("nedit: getpwuid() failed - reverting to $USER");
	return QLatin1String(qgetenv("USER"));

}


/*
** Writes the hostname of the current system in string "hostname".
**
** NOTE: This function used to be called "GetHostName" but that resulted in a
** linking conflict on VMS with the standard gethostname function, because
** VMS links case-insensitively.
*/
QString GetNameOfHostEx() {
	return QHostInfo::localHostName();
}

/*
** Create a path: $HOME/filename
** Return "" if it doesn't fit into the buffer
*/
QString PrependHomeEx(const QString &filename) {	
	return QString(QLatin1String("%1/%2")).arg(GetHomeDirEx()).arg(filename);
}

