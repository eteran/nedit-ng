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

#include "utils.h"

#include <QtGlobal>
#include <QDir>
#include <QHostInfo>
#include <QStandardPaths>

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <pwd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>


/* return non-nullptr value for the current working directory.
   If system call fails, provide a fallback value */
QString GetCurrentDirEx() {
	return QDir::currentPath();
}

/* return a non-nullptr value for the user's home directory,
   without trailing slash.
   We try the  environment var and the system user database. */
QString GetHomeDirEx() {
    return QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
}

/*
** Return a pointer to the username of the current user in a statically
** allocated string.
*/
QString GetUserNameEx() {

#ifdef Q_OS_UNIX
	/* cuserid has apparently been dropped from the ansi C standard, and if
	   strict ansi compliance is turned on (on Sun anyhow, maybe others), calls
	   to cuserid fail to compile.  Older versions of nedit try to use the
	   getlogin call first, then if that fails, use getpwuid and getuid.  This
	   results in the user-name of the original terminal being used, which is
	   not correct when the user uses the su command.  Now, getpwuid only: */

	static QString user_name;

	if (!user_name.isNull()) {
        return user_name;
	}

	if(const struct passwd *passwdEntry = getpwuid(getuid())) {
		user_name = QString::fromLatin1(passwdEntry->pw_name);
        return user_name;
	}
	
	/* This is really serious, but sometimes username service
	   is misconfigured through no fault of the user.  Be nice
	   and let the user start nc anyway. */
	perror("nedit: getpwuid() failed - reverting to $USER");
    return QString::fromLatin1(qgetenv("USER"));
#else
	return QString();
#endif
}

/**
 * @brief GetNameOfHostEx
 * @return the hostname of the system
 */
QString GetNameOfHostEx() {
	return QHostInfo::localHostName();
}

/**
 * @brief PrependHomeEx
 * @param filename
 * @return $HOME/filename
 */
QString PrependHomeEx(const QString &filename) {	
	return QString(QLatin1String("%1/%2")).arg(GetHomeDirEx(), filename);
}

/**
 * @brief ErrorString
 * @param error
 * @return The result of strerror(error), but as a QString
 */
QString ErrorString(int error) {
    return QString::fromLatin1(strerror(error));
}
