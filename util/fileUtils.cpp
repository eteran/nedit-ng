/*******************************************************************************
*                                                                              *
* fileUtils.c -- File utilities for Nirvana applications                       *
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
* July 28, 1992                                                                *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
* Modified by:        DMR - Ported to VMS (1st stage for Histo-Scope)          *
*                                                                              *
*******************************************************************************/

#include <QString>
#include "fileUtils.h"
#include "utils.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <algorithm>
#include <memory>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

static int ExpandTilde(char *pathname);
static int ResolvePath(const char *pathIn, char *pathResolved);

#ifndef MAXSYMLINKS /* should be defined in <sys/param.h> */
#define MAXSYMLINKS 20
#endif

/* Parameters to algorithm used to auto-detect DOS format files.  NEdit will
   scan up to the lesser of FORMAT_SAMPLE_LINES lines and FORMAT_SAMPLE_CHARS
   characters of the beginning of the file, checking that all newlines are
   paired with carriage returns.  If even a single counterexample exists,
   the file is judged to be in Unix format. */
#define FORMAT_SAMPLE_LINES 5
#define FORMAT_SAMPLE_CHARS 2000

static char *nextSlash(char *ptr);
static char *prevSlash(char *ptr);
static int compareThruSlash(const char *string1, const char *string2);
static void copyThruSlash(char **toString, char **fromString);

/*
** Decompose a Unix file name into a file name and a path.
** Return non-zero value if it fails, zero else.
** For now we assume that filename and pathname are at
** least MAXPATHLEN chars long.
** To skip setting filename or pathname pass nullptr for that argument.
*/
int ParseFilenameEx(const QString &fullname, QString *filename, QString *pathname) {
    int fullLen = fullname.size();
    int i;
    int viewExtendPath;
    int scanStart;

    /* For clearcase version extended paths, slash characters after the "@@/"
       should be considered part of the file name, rather than the path */
    if ((viewExtendPath = fullname.indexOf(QLatin1String("@@/"))) != -1) {
        scanStart = viewExtendPath - 1;
    } else {
        scanStart = fullLen - 1;
    }

    /* find the last slash */
    for (i = scanStart; i >= 0; i--) {
        if (fullname[i] == QLatin1Char('/')) {
            break;
        }
    }

    /* move chars before / (or ] or :) into pathname,& after into filename */
    int pathLen = i + 1;
    int fileLen = fullLen - pathLen;

    if (pathname) {
        *pathname = fullname.left(pathLen);
    }

    if (filename) {
        *filename = fullname.mid(pathLen, fileLen);
    }

    if (pathname) {
        *pathname = NormalizePathnameEx(*pathname);
    }

    return 0;
}

/*
** Expand tilde characters which begin file names as done by the shell
** If it doesn't work just out leave pathname unmodified.
** This implementation is neither fast, nor elegant, nor ...
*/
QString ExpandTildeEx(const QString &pathname) {
#ifdef Q_OS_UNIX
    char path[PATH_MAX];
    strncpy(path, pathname.toLatin1().data(), sizeof(path));
    path[PATH_MAX - 1] = '\0';

    if(ExpandTilde(path)) {
        return QString::fromLatin1(path);
    }

    return QString();
#else
    return pathname;
#endif
}

int ExpandTilde(char *pathname) {
	struct passwd *passwdEntry;
    char username[MAXPATHLEN];
    char temp[MAXPATHLEN];
	unsigned len_left;

    if (pathname[0] != '~') {
        return true;
    }

    char *nameEnd = strchr(&pathname[1], '/');
	if(!nameEnd) {
		nameEnd = pathname + strlen(pathname);
	}

	strncpy(username, &pathname[1], nameEnd - &pathname[1]);
	username[nameEnd - &pathname[1]] = '\0';

	/* We might consider to re-use the GetHomeDirEx() function,
	   but to keep the code more similar for both cases ... */
	if (username[0] == '\0') {
		passwdEntry = getpwuid(getuid());
		if ((passwdEntry == nullptr) || (*(passwdEntry->pw_dir) == '\0')) {
			/* This is really serious, so just exit. */
			perror("NEdit/nc: getpwuid() failed ");
			exit(EXIT_FAILURE);
		}
	} else {
		passwdEntry = getpwnam(username);
		if ((passwdEntry == nullptr) || (*(passwdEntry->pw_dir) == '\0')) {
			/* username was just an input by the user, this is no
		   indication for some (serious) problems */
            return false;
		}
	}

	strcpy(temp, passwdEntry->pw_dir);
	strcat(temp, "/");

	len_left = sizeof(temp) - strlen(temp) - 1;
	if (len_left < strlen(nameEnd)) {
		/* It won't work out */
        return false;
	}

	strcat(temp, nameEnd);
	strcpy(pathname, temp);
    return true;
}

/*
 * Resolve symbolic links (if any) for the absolute path given in pathIn
 * and place the resolved absolute path in pathResolved.
 * -  pathIn must contain an absolute path spec.
 * -  pathResolved must point to a buffer of minimum size MAXPATHLEN.
 *
 * Returns:
 *   TRUE  if pathResolved contains a valid resolved path
 *         OR pathIn is not a symlink (pathResolved will have the same
 *	      contents like pathIn)
 *
 *   FALSE an error occured while trying to resolve the symlink, i.e.
 *         pathIn was no absolute path or the link is a loop.
 */
QString ResolvePathEx(const QString &pathname) {
    char pathResolved[MAXPATHLEN];

    if(!ResolvePath(pathname.toLatin1().data(), pathResolved)) {
        return QString::fromLatin1(pathResolved);
    }

    return QString();
}

int ResolvePath(const char *pathIn, char *pathResolved) {
	char resolveBuf[MAXPATHLEN], pathBuf[MAXPATHLEN];
	char *pathEnd;
	int loops;

	/* !! readlink does NOT recognize loops, i.e. links like file -> ./file */
	for (loops = 0; loops < MAXSYMLINKS; loops++) {
		int rlResult = readlink(pathIn, resolveBuf, MAXPATHLEN - 1);
		if (rlResult < 0) {

			if (errno == EINVAL)
			{
				/* It's not a symlink - we are done */
				strncpy(pathResolved, pathIn, MAXPATHLEN);
				pathResolved[MAXPATHLEN - 1] = '\0';
                return true;
			} else {
                return false;
			}
		} else if (rlResult == 0) {
            return false;
		}

		resolveBuf[rlResult] = 0;

		if (resolveBuf[0] != '/') {
			strncpy(pathBuf, pathIn, MAXPATHLEN);
			pathBuf[MAXPATHLEN - 1] = '\0';
			pathEnd = strrchr(pathBuf, '/');
			if (!pathEnd) {
                return false;
			}
			strcpy(pathEnd + 1, resolveBuf);
		} else {
			strcpy(pathBuf, resolveBuf);
		}
		NormalizePathname(pathBuf);
		pathIn = pathBuf;
	}

    return false;
}

QString NormalizePathnameEx(const std::string &pathname) {

    char path[PATH_MAX];
    strncpy(path, pathname.c_str(), sizeof(path));
    path[PATH_MAX - 1] = '\0';

    if(!NormalizePathname(path)) {
        return QString::fromLatin1(path);
    }

    return QString();
}

QString NormalizePathnameEx(const QString &pathname) {

    char path[PATH_MAX];
    strncpy(path, pathname.toLatin1().data(), sizeof(path));
    path[PATH_MAX - 1] = '\0';

    if(!NormalizePathname(path)) {
        return QString::fromLatin1(path);
    }

    return QString();
}

/*
** Return 0 if everything's fine. In fact it always return 0... (No it doesn't)
** Capable to handle arbitrary path length (>MAXPATHLEN)!
**
**  FIXME: Documentation
**  FIXME: Change return value to False and True.
*/
int NormalizePathname(char *pathname) {

	/* if this is a relative pathname, prepend current directory */
	if (pathname[0] != '/') {

		/* make a copy of pathname to work from */
        char *oldPathname = new char[strlen(pathname) + 1];
		strcpy(oldPathname, pathname);
		
		/* get the working directory and prepend to the path */
		strcpy(pathname, GetCurrentDirEx().toLatin1().data());

		/* check for trailing slash, or pathname being root dir "/":
		   don't add a second '/' character as this may break things
		   on non-un*x systems */
        const size_t len = strlen(pathname); /* GetCurrentDirEx() returns non-nullptr value */

		/*  Apart from the fact that people putting conditional expressions in
		    ifs should be caned: How should len ever become 0 if GetCurrentDirEx()
		    always returns a useful value?
		    FIXME: Check and document GetCurrentDirEx() return behaviour.  */
		if ((len == 0) ? 1 : pathname[len - 1] != '/') {
			strcat(pathname, "/");
		}

		strcat(pathname, oldPathname);
        delete [] oldPathname;
	}

	/* compress out .. and . */
	return CompressPathname(pathname);
}

/*
** Return 0 if everything's fine, 1 else.
**
**  FIXME: Documentation
**  FIXME: Change return value to False and True.
*/
int CompressPathname(char *pathname) {

	/* (Added by schwarzenberg)
	** replace multiple slashes by a single slash
	**  (added by yooden)
	**  Except for the first slash. From the Single UNIX Spec: "A pathname
	**  that begins with two successive slashes may be interpreted in an
	**  implementation-dependent manner"
	*/
	char *in  = pathname;
	char *out = pathname;
	
	*out++ = *in++;	
	while(*in != '\0') {
		const char ch = *in++;
		*out++ = ch;
		if (ch == '/') {
			while(*in == '/') {
				++in;
			}
		}
	}	
	*out = '\0';


	/* compress out . and .. */
	auto buf = new char[strlen(pathname) + 2];
	char *inPtr = pathname;
	char *outPtr = buf;
	
	/* copy initial / */
	copyThruSlash(&outPtr, &inPtr);
	while (inPtr) {
		/* if the next component is "../", remove previous component */
		if (compareThruSlash(inPtr, "../")) {
			*outPtr = 0;
		
			/* If the ../ is at the beginning, or if the previous component
			   is a symbolic link, preserve the ../.  It is not valid to
			   compress ../ when the previous component is a symbolic link
			   because ../ is relative to where the link points.  If there's
			   no S_ISLNK macro, assume system does not do symbolic links. */
#ifdef S_ISLNK
			struct stat statbuf;
			if (outPtr - 1 == buf || (lstat(buf, &statbuf) == 0 && S_ISLNK(statbuf.st_mode))) {
				copyThruSlash(&outPtr, &inPtr);
			} else
#endif
			{
				/* back up outPtr to remove last path name component */
				outPtr = prevSlash(outPtr);
				inPtr = nextSlash(inPtr);
			}
		} else if (compareThruSlash(inPtr, "./")) {
			/* don't copy the component if it's the redundant "./" */
			inPtr = nextSlash(inPtr);
		} else {
			/* copy the component to outPtr */
			copyThruSlash(&outPtr, &inPtr);
		}
	}

	/* updated pathname with the new value */
	if (strlen(buf) > MAXPATHLEN) {
		fprintf(stderr, "nedit: CompressPathname(): file name too long %s\n", pathname);
		delete [] buf;
		return 1;
	} else {
		strcpy(pathname, buf);
		delete [] buf;
		return 0;
	}
}

static char *nextSlash(char *ptr) {
	for (; *ptr != '/'; ptr++) {
		if (*ptr == '\0')
			return nullptr;
	}
	return ptr + 1;
}

static char *prevSlash(char *ptr) {
	for (ptr -= 2; *ptr != '/'; ptr--)
		;
	return ptr + 1;
}

static int compareThruSlash(const char *string1, const char *string2) {
    while (true) {
		if (*string1 != *string2)
            return false;
		if (*string1 == '\0' || *string1 == '/')
            return true;
		string1++;
		string2++;
	}
}

static void copyThruSlash(char **toString, char **fromString) {
	char *to = *toString;
	char *from = *fromString;

    while (true) {
		*to = *from;
		if (*from == '\0') {
			*fromString = nullptr;
			return;
		}
		if (*from == '/') {
			*toString = to + 1;
			*fromString = from + 1;
			return;
		}
		from++;
		to++;
	}
}

/*
** Return the trailing 'n' no. of path components
*/
QString GetTrailingPathComponentsEx(const QString &path, int noOfComponents) {
	
	/* Start from the rear */
	int index = path.size();
	int count = 0;

	while (--index > 0) {
		if (path[index] == QLatin1Char('/')) {
			if (count++ == noOfComponents) {
				break;
			}
		}
	}
	return path.mid(index);
}

/*
** Samples up to a maximum of FORMAT_SAMPLE_LINES lines and FORMAT_SAMPLE_CHARS
** characters, to determine whether fileString represents a MS DOS or Macintosh
** format file.  If there's ANY ambiguity (a newline in the sample not paired
** with a return in an otherwise DOS looking file, or a newline appearing in
** the sampled portion of a Macintosh looking file), the file is judged to be
** Unix format.
*/
FileFormats FormatOfFileEx(view::string_view fileString) {


	const char *p;
	int nNewlines = 0;
	int nReturns = 0;
	int length = fileString.size();

	for (p = fileString.begin(); length-- != 0 && p < fileString.begin() + FORMAT_SAMPLE_CHARS; p++) {
		if (*p == '\n') {
			nNewlines++;
			if (p == fileString.begin() || *(p - 1) != '\r')
				return UNIX_FILE_FORMAT;
			if (nNewlines >= FORMAT_SAMPLE_LINES)
				return DOS_FILE_FORMAT;
		} else if (*p == '\r')
			nReturns++;
	}
	if (nNewlines > 0)
		return DOS_FILE_FORMAT;
	if (nReturns > 0)
		return MAC_FILE_FORMAT;
	return UNIX_FILE_FORMAT;

}

/*
** Converts a string (which may represent the entire contents of the file)
** from DOS or Macintosh format to Unix format.  Conversion is done in-place.
** In the DOS case, the length will be shorter, and passed length will be
** modified to reflect the new length. The routine has support for blockwise
** file to string conversion: if the fileString has a trailing '\r' and
** 'pendingCR' is not zero, the '\r' is deposited in there and is not
** converted. If there is no trailing '\r', a 0 is deposited in 'pendingCR'
** It's the caller's responsability to make sure that the pending character,
** if present, is inserted at the beginning of the next block to convert.
*/
void ConvertFromDosFileString(char *fileString, int *length, char *pendingCR) {
	char *outPtr = fileString;
	char *inPtr = fileString;
	if (pendingCR)
		*pendingCR = 0;
	while (inPtr < fileString + *length) {
		if (*inPtr == '\r') {
			if (inPtr < fileString + *length - 1) {
				if (*(inPtr + 1) == '\n')
					inPtr++;
			} else {
				if (pendingCR) {
					*pendingCR = *inPtr;
					break; /* Don't copy this trailing '\r' */
				}
			}
		}
		*outPtr++ = *inPtr++;
	}
	*outPtr = '\0';
	*length = outPtr - fileString;
}

void ConvertFromMacFileString(char *fileString, int length) {
    std::transform(fileString, fileString + length, fileString, [](char ch) {
        if(ch == '\r') {
            return '\n';
        }

        return ch;
    });
}

/*
** Converts a string (which may represent the entire contents of the file) from
** Unix to DOS format.  String is re-allocated (with malloc), and length is
** modified.  If allocation fails, which it may, because this can potentially
** be a huge hunk of memory, returns FALSE and no conversion is done.
**
** This could be done more efficiently by asking doSave to allocate some
** extra memory for this, and only re-allocating if it wasn't enough.  If
** anyone cares about the performance or the potential for running out of
** memory on a save, it should probably be redone.
*/
bool ConvertToDosFileStringEx(std::string &fileString) {

	/* How long a string will we need? */
	int outLength = 0;
	for (char ch : fileString) {
		if (ch == '\n') {
			outLength++;
		}
		outLength++;
	}

	/* Allocate the new string */
	std::string outString;
	outString.reserve(outLength);

	auto outPtr = std::back_inserter(outString);

	/* Do the conversion, free the old string */
	for (char ch : fileString) {
		if (ch == '\n') {
			*outPtr++ = '\r';
		}
		*outPtr++ = ch;
	}

	fileString = outString;
    return true;
}

/*
** Converts a string (which may represent the entire contents of the file)
** from Unix to Macintosh format.
*/
void ConvertToMacFileStringEx(std::string &fileString) {

	std::transform(fileString.begin(), fileString.end(), fileString.begin(), [](char ch) {
		if (ch == '\n') {
			return '\r';
		}

		return ch;
	});
}

/*
** Reads a text file into a string buffer, converting line breaks to
** unix-style if appropriate.
**
** Force a terminating \n, if this is requested
*/
QString ReadAnyTextFileEx(const std::string &fileName, int forceNL) {
	struct stat statbuf;
	FILE *fp;
	int readLen;

	/* Read the whole file into fileString */
	if ((fp = fopen(fileName.c_str(), "r")) == nullptr) {
		return QString();
	}
	
	if (fstat(fileno(fp), &statbuf) != 0) {
		fclose(fp);
		return QString();
	}
	
	int fileLen = statbuf.st_size;
	/* +1 = space for null
	** +1 = possible additional \n
	*/
    auto fileString = std::make_unique<char[]>(fileLen + 2);
    readLen = fread(&fileString[0], 1, fileLen, fp);
	if (ferror(fp)) {
		fclose(fp);
		return QString();
	}
	fclose(fp);
	fileString[readLen] = '\0';

	/* Convert linebreaks? */
    FileFormats format = FormatOfFileEx(view::string_view(&fileString[0], readLen));
	if (format == DOS_FILE_FORMAT) {
		char pendingCR;
        ConvertFromDosFileString(&fileString[0], &readLen, &pendingCR);
	} else if (format == MAC_FILE_FORMAT) {
        ConvertFromMacFileString(&fileString[0], readLen);
	}

	/* now, that the fileString is in Unix format, check for terminating \n */
	if (forceNL && fileString[readLen - 1] != '\n') {
		fileString[readLen] = '\n';
		fileString[readLen + 1] = '\0';
	}
	
    return QString::fromLatin1(&fileString[0], readLen);
}
