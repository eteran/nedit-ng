
#include "util/fileUtils.h"
#include "util/FileFormats.h"
#include "util/utils.h"
#include "util/ClearCase.h"
#include <gsl/gsl_util>

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <algorithm>
#include <memory>
#include <iostream>
#include <fstream>
#include <string>

#include <sys/param.h>
#include <unistd.h>
#include <pwd.h>

#include <QString>
#include <QFileInfo>



/* Parameters to algorithm used to auto-detect DOS format files.  NEdit will
   scan up to the lesser of FORMAT_SAMPLE_LINES lines and FORMAT_SAMPLE_CHARS
   characters of the beginning of the file, checking that all newlines are
   paired with carriage returns.  If even a single counterexample exists,
   the file is judged to be in Unix format. */
constexpr int FORMAT_SAMPLE_LINES = 5;
constexpr int FORMAT_SAMPLE_CHARS = 2000;

static char *nextSlash(char *ptr);
static char *prevSlash(char *ptr);
static bool compareThruSlash(const char *string1, const char *string2);
static void copyThruSlash(char **toString, char **fromString);
static int ResolvePath(const char *pathIn, char *pathResolved);
static bool CompressPathname(char *pathname);
static bool NormalizePathname(char *pathname);

/*
** Decompose a Unix file name into a file name and a path.
** returns false if an error occured (currently, there is no error case).
*/
bool ParseFilenameEx(const QString &fullname, QString *filename, QString *pathname) {

    int fullLen = fullname.size();
    int scanStart = -1;

    /* For clearcase version extended paths, slash characters after the "@@/"
       should be considered part of the file name, rather than the path */
    const int viewExtendPath = ClearCase::GetVersionExtendedPathIndex(fullname);
    if (viewExtendPath != -1) {
        scanStart = viewExtendPath - 1;
    }

    /* find the last slash */
    const int i = fullname.lastIndexOf(QLatin1Char('/'), scanStart);

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

    return true;
}

/*
** Expand tilde characters which begin file names as done by the shell
** If it doesn't work just return the pathname originally passed pathname
*/
QString ExpandTildeEx(const QString &pathname) {
#ifdef Q_OS_UNIX
    struct passwd *passwdEntry;

    if (!pathname.startsWith(QLatin1Char('~'))) {
        return pathname;
    }

    int end = pathname.indexOf(QLatin1Char('/'));
    if(end == -1) {
        end = pathname.size();
    }

    QString username = pathname.mid(1, end - 1);

    /* We might consider to re-use the GetHomeDirEx() function,
       but to keep the code more similar for both cases ... */
    if (username.isEmpty()) {
        passwdEntry = getpwuid(getuid());
        if ((passwdEntry == nullptr) || (*(passwdEntry->pw_dir) == '\0')) {
            /* This is really serious, so just exit. */
            qFatal("getpwuid() failed ");
        }
    } else {
        passwdEntry = getpwnam(username.toLatin1().data());
        if ((passwdEntry == nullptr) || (*(passwdEntry->pw_dir) == '\0')) {
            /* username was just an input by the user, this is no indication
             * for some (serious) problems */
            return QString();
        }
    }

    return QString(QLatin1String("%1/%2")).arg(QString::fromLatin1(passwdEntry->pw_dir), pathname.mid(end));
#else
    return pathname;
#endif
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
    char path[MAXPATHLEN];

    if(ResolvePath(pathname.toLatin1().data(), path)) {
        return QString::fromLatin1(path);
    }

    return QString();
}

int ResolvePath(const char *pathIn, char *pathResolved) {
	char resolveBuf[MAXPATHLEN], pathBuf[MAXPATHLEN];
	char *pathEnd;
	int loops;

	/* !! readlink does NOT recognize loops, i.e. links like file -> ./file */
	for (loops = 0; loops < MAXSYMLINKS; loops++) {
        auto rlResult = gsl::narrow<int>(::readlink(pathIn, resolveBuf, MAXPATHLEN - 1));
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
** Return false if everything's fine. In fact it always return false...
** (No it doesn't) Capable to handle arbitrary path length (>MAXPATHLEN)!
*/
bool NormalizePathname(char *pathname) {

	/* if this is a relative pathname, prepend current directory */
	if (pathname[0] != '/') {

		/* make a copy of pathname to work from */
        auto oldPathname = std::make_unique<char[]>(strlen(pathname) + 1);
        strcpy(&oldPathname[0], pathname);
		
		/* get the working directory and prepend to the path */
		strcpy(pathname, GetCurrentDirEx().toLatin1().data());

		/* check for trailing slash, or pathname being root dir "/":
		   don't add a second '/' character as this may break things
		   on non-un*x systems */
        const size_t len = strlen(pathname);

        /*  Apart from the fact that people putting conditional expressions in
         * ifs should be caned: How should len ever become 0 if
         * GetCurrentDirEx() always returns a useful value?
         */
		if ((len == 0) ? 1 : pathname[len - 1] != '/') {
			strcat(pathname, "/");
		}

        strcat(pathname, &oldPathname[0]);
	}

	/* compress out .. and . */
	return CompressPathname(pathname);
}

/**
 * @brief CompressPathnameEx
 * @param pathname
 * @return
 *
 * Returns a pathname without symbolic links or redundant "." or ".." elements.
 *
 */
QString CompressPathnameEx(const QString &pathname) {
    char path[PATH_MAX];
    strncpy(path, pathname.toLatin1().data(), sizeof(path));
    path[PATH_MAX - 1] = '\0';

    if(!CompressPathname(path)) {
        return QString::fromLatin1(path);
    }

    return QString();
}

/**
 * @brief CompressPathnameEx
 * @param pathname
 * @return
 *
 * Returns a pathname without symbolic links or redundant "." or ".." elements.
 *
 */
QString CompressPathnameEx(const std::string &pathname) {
    char path[PATH_MAX];
    strncpy(path, pathname.c_str(), sizeof(path));
    path[PATH_MAX - 1] = '\0';

    if(!CompressPathname(path)) {
        return QString::fromLatin1(path);
    }

    return QString();
}


/**
 * @brief CompressPathname
 * @param pathname
 * @return
 *
 * Returns true upon error, pathname context is replaced with compressed path
 *
 */
bool CompressPathname(char *pathname) {

    // TODO(eteran): replace this function with QFileInfo::canonicalFilePath
    // once we figure out the best way to test and make sure that they are
    // comparable (I beleive that they are).

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
    auto buf = std::make_unique<char[]>(strlen(pathname) + 2);
	char *inPtr = pathname;
    char *outPtr = &buf[0];
	
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
            if (outPtr - 1 == &buf[0] || (::lstat(&buf[0], &statbuf) == 0 && S_ISLNK(statbuf.st_mode))) {
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
    if (strlen(&buf[0]) > MAXPATHLEN) {
        qWarning("NEdit: CompressPathname(): file name too long %s", pathname);
        return true;
	} else {
        strcpy(pathname, &buf[0]);
        return false;
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
    for (ptr -= 2; *ptr != '/'; ptr--) {
		;
    }
	return ptr + 1;
}

static bool compareThruSlash(const char *string1, const char *string2) {
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

    size_t nNewlines = 0;
    size_t nReturns = 0;

    for (auto it = fileString.begin(); it != fileString.end() && it < fileString.begin() + FORMAT_SAMPLE_CHARS; ++it) {
        if (*it == '\n') {
			nNewlines++;
            if (it == fileString.begin() || *std::prev(it) != '\r') {
                return FileFormats::Unix;
            }

            if (nNewlines >= FORMAT_SAMPLE_LINES) {
                return FileFormats::Dos;
            }
        } else if (*it == '\r') {
			nReturns++;
        }
    }

    if (nNewlines > 0) {
        return FileFormats::Dos;
    }

    if (nReturns > 0) {
        return FileFormats::Mac;
    }

    return FileFormats::Unix;

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
void ConvertFromDosFileStringEx(std::string *fileString, char *pendingCR) {
	Q_ASSERT(fileString);

	if (pendingCR) {
		*pendingCR = '\0';
	}

	auto out = fileString->begin();
	auto it  = fileString->begin();

	while (it != fileString->end()) {
		if (*it == '\r') {
			auto next = std::next(it);
			if (next != fileString->end()) {
				if (*next == '\n') {
					++it;
				}
			} else {
				if (pendingCR) {
					*pendingCR = *it;
					break; // Don't copy this trailing '\r'
				}
			}
		}
		*out++ = *it++;
	}

	// remove the junk left at the end of the string
	fileString->erase(out, fileString->end());
}

/**
 * @brief ConvertFromDosFileString
 * @param fileString
 * @param length
 * @param pendingCR
 */
void ConvertFromDosFileString(char *fileString, size_t *length, char *pendingCR) {
    Q_ASSERT(fileString);
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

/**
 * @brief ConvertFromDosFileString
 * @param fileString
 * @param length
 * @param pendingCR
 */
void ConvertFromDosFileString(char *fileString, int *length, char *pendingCR) {
	Q_ASSERT(fileString);
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
    *length = gsl::narrow<int>(outPtr - fileString);
}

/**
 * @brief ConvertFromMacFileString
 * @param fileString
 * @param length
 */
void ConvertFromMacFileString(char *fileString, size_t length) {
    Q_ASSERT(fileString);
    std::transform(fileString, fileString + length, fileString, [](char ch) {
        if(ch == '\r') {
            return '\n';
        }

        return ch;
    });
}

/**
 * @brief ConvertFromMacFileString
 * @param fileString
 * @param length
 */
void ConvertFromMacFileString(char *fileString, int length) {
	Q_ASSERT(fileString);
    std::transform(fileString, fileString + length, fileString, [](char ch) {
        if(ch == '\r') {
            return '\n';
        }

        return ch;
    });
}

/**
 * @brief ConvertFromMacFileStringEx
 * @param fileString
 */
void ConvertFromMacFileStringEx(std::string *fileString) {
    Q_ASSERT(fileString);
    std::transform(fileString->begin(), fileString->end(), fileString->begin(), [](char ch) {
		if(ch == '\r') {
			return '\n';
		}

		return ch;
	});
}

/*
** Converts a string (which may represent the entire contents of the file) from
** Unix to DOS format.
**
** This could be done more efficiently by asking doSave to allocate some
** extra memory for this, and only re-allocating if it wasn't enough.  If
** anyone cares about the performance or the potential for running out of
** memory on a save, it should probably be redone.
*/
void ConvertToDosFileStringEx(std::string &fileString) {

    // How long a string will we need?
	size_t outLength = 0;
	for (char ch : fileString) {
		if (ch == '\n') {
			outLength++;
		}
		outLength++;
	}

	std::string outString;
	outString.reserve(outLength);

	auto outPtr = std::back_inserter(outString);

    // Do the conversion, free the old string
	for (char ch : fileString) {
		if (ch == '\n') {
			*outPtr++ = '\r';
		}
		*outPtr++ = ch;
	}

	fileString = outString;
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
QString ReadAnyTextFileEx(const QString &fileName, bool forceNL) {

    std::ifstream file(fileName.toStdString());
	if(!file) {
		return QString();
	}

	auto contents = std::string(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});

	/* Convert linebreaks? */
    switch(FormatOfFileEx(contents)) {
    case FileFormats::Dos:
		ConvertFromDosFileStringEx(&contents, nullptr);
		break;
    case FileFormats::Mac:
		ConvertFromMacFileStringEx(&contents);
		break;
    case FileFormats::Unix:
		break;
	}

	if(contents.empty()) {
		return QString();
	}

	/* now, that the fileString is in Unix format, check for terminating \n */
	if (forceNL && contents.back() != '\n') {
		contents.push_back('\n');
	}
	
	return QString::fromStdString(contents);
}
