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

#define DEFAULT_NEDIT_HOME ".nedit"

namespace {

const char *hiddenFileNames[N_FILE_TYPES] = {
	".nedit",
	".neditmacro",
	".neditdb"
};

const char *plainFileNames[N_FILE_TYPES] = {
	"nedit.rc",
	"autoload.nm",
	"nedit.history"
};

/*
**  Builds a file path from 'dir' and 'file', watching for buffer overruns.
**
**  Preconditions:
**      - MAXPATHLEN is set to the max. allowed path length
**      - 'fullPath' points to a buffer of at least MAXPATHLEN
**      - 'dir' and 'file' are valid strings
**
**  Postcondition:
**      - 'fullpath' will contain 'dir/file'
**      - Exits when the result would be greater than MAXPATHLEN
*/
void buildFilePath(char *fullPath, const char *dir, const char *file) {
	const int len = snprintf(fullPath, MAXPATHLEN, "%s/%s", dir, file);
	if(len >= MAXPATHLEN) {
		/*  We have no way to build the path. */
		fprintf(stderr, "nedit: rc file path too long for %s.\n", file);
		exit(EXIT_FAILURE);	
	}
}

/*
**  Returns true if 'file' is a directory, false otherwise.
**  Links are followed.
**
**  Preconditions:
**      - None
**
**  Returns:
**      - True for directories, false otherwise
*/
bool isDir(const char *file) {
	struct stat attribute;
	return ((stat(file, &attribute) == 0) && S_ISDIR(attribute.st_mode));
}

/*
**  Returns true if 'file' is a regular file, false otherwise.
**  Links are followed.
**
**  Preconditions:
**      - None
**
**  Returns:
**      - True for regular files, false otherwise
*/
bool isRegFile(const char *file) {
	struct stat attribute;
	return ((stat(file, &attribute) == 0) && S_ISREG(attribute.st_mode));
}

}

/* return non-nullptr value for the current working directory.
   If system call fails, provide a fallback value */
QString GetCurrentDirEx(void) {
	return QDir::currentPath();
}

/* return a non-nullptr value for the user's home directory,
   without trailing slash.
   We try the  environment var and the system user database. */
QString GetHomeDirEx(void) {
	return QDir::homePath();
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
QString GetNameOfHostEx(void) {
	return QHostInfo::localHostName();
}

/*
** Create a path: $HOME/filename
** Return "" if it doesn't fit into the buffer
*/
QString PrependHomeEx(const QString &filename) {	
	return QString(QLatin1String("%1/%2")).arg(GetHomeDirEx()).arg(filename);
}

/*
**  Returns a pointer to the name of an rc file of the requested type.
**
**  Preconditions:
**      - MAXPATHLEN is set to the max. allowed path length
**      - fullPath points to a buffer of at least MAXPATHLEN
**
**  Returns:
**      - nullptr if an error occurs while creating a directory
**      - Pointer to a static array containing the file name
**
*/
QString GetRCFileNameEx(int type) {
	
	static char rcFiles[N_FILE_TYPES][MAXPATHLEN + 1];
	static bool namesDetermined = false;

	if (!namesDetermined) {
		char *const nedit_home = getenv("NEDIT_HOME");

		if (nedit_home == nullptr) {
			/*  No NEDIT_HOME */

			/* Let's try if ~/.nedit is a regular file or not. */
			char legacyFile[MAXPATHLEN + 1];
			buildFilePath(legacyFile, GetHomeDirEx().toLatin1().data(), hiddenFileNames[NEDIT_RC]);
			if (isRegFile(legacyFile)) {
				/* This is a legacy setup with rc files in $HOME */
				for (int i = 0; i < N_FILE_TYPES; i++) {
					buildFilePath(rcFiles[i], GetHomeDirEx().toLatin1().data(), hiddenFileNames[i]);
				}
			} else {
				/* ${HOME}/.nedit does not exist as a regular file. */
				/* FIXME: Devices, sockets and fifos are ignored for now. */
				char defaultNEditHome[MAXPATHLEN + 1];
				buildFilePath(defaultNEditHome, GetHomeDirEx().toLatin1().data(), DEFAULT_NEDIT_HOME);
				if (!isDir(defaultNEditHome)) {
					/* Create DEFAULT_NEDIT_HOME */
					if (mkdir(defaultNEditHome, 0777) != 0) {
						perror("nedit: Error while creating rc file directory"
						       " $HOME/" DEFAULT_NEDIT_HOME "\n"
						       " (Make sure all parent directories exist.)");
						return QString();
					}
				}

				/* All set for DEFAULT_NEDIT_HOME, let's copy the names */
				for (int i = 0; i < N_FILE_TYPES; i++) {
					buildFilePath(rcFiles[i], defaultNEditHome, plainFileNames[i]);
				}
			}
		} else {
			/*  $NEDIT_HOME is set. */
			/* FIXME: Is this required? Does VMS know stat(), mkdir()? */
			if (!isDir(nedit_home)) {
				/* Create $NEDIT_HOME */
				if (mkdir(nedit_home, 0777) != 0) {
					perror("nedit: Error while creating rc file directory $NEDIT_HOME\n"
					       "nedit: (Make sure all parent directories exist.)");
					return QString();
				}
			}

			/* All set for NEDIT_HOME, let's copy the names */
			for (int i = 0; i < N_FILE_TYPES; i++) {
				buildFilePath(rcFiles[i], nedit_home, plainFileNames[i]);
			}
		}

		namesDetermined = true;
	}

	return QLatin1String(rcFiles[type]);
}
