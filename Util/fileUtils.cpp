
#include "Util/fileUtils.h"
#include "Util/FileFormats.h"
#include "Util/utils.h"
#include "Util/ClearCase.h"
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

#ifdef Q_OS_UNIX
#include <unistd.h>
#include <pwd.h>
#endif

#include <QString>
#include <QFileInfo>

namespace {

/* Parameters to algorithm used to auto-detect DOS format files.  NEdit will
   scan up to the lesser of FORMAT_SAMPLE_LINES lines and FORMAT_SAMPLE_CHARS
   characters of the beginning of the file, checking that all newlines are
   paired with carriage returns.  If even a single counterexample exists,
   the file is judged to be in Unix format. */
constexpr int FORMAT_SAMPLE_LINES = 5;
constexpr int FORMAT_SAMPLE_CHARS = 2000;

/**
 * @brief prevSlash
 * @param str
 * @param index
 * @return
 *
 * Requires that index be an offset one past a given slash and will return the
 * index of the previous slash
 */
int prevSlash(const QString &str, int index) {

    for (index -= 2; index >= 0 && str[index] != QLatin1Char('/'); index--) {
        ;
    }

    return index + 1;
}

template <class In>
In nextSlash(In first, In last) {

    while(first != last && *first != QLatin1Char('/')) {
        ++first;
    }

    return std::next(first);
}

template <class In>
bool compareThruSlash(In first, In last, const QString &str) {

    auto first2 = str.begin();
    auto last2  = str.end();

    while (true) {

        if (first2 == last2 || *first != *first2) {
            return false;
        }

        if (first == last || *first == QLatin1Char('/')) {
            return true;
        }

        ++first;
        ++first2;
    }
}

template <class Out, class In>
void copyThruSlash(Out &to, In &from, In end) {

    while (true) {
        *to = *from;

        if (from == end) {
            return;
        }

        if (*from == QLatin1Char('/')) {
            ++from;
            ++to;
            return;
        }

        ++from;
        ++to;
    }
}

}

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

    return QString(QLatin1String("%1/%2")).arg(QString::fromUtf8(passwdEntry->pw_dir), pathname.mid(end));
#else
    return pathname;
#endif
}

QString NormalizePathnameEx(const QString &pathname) {

    QString path = pathname;

    QFileInfo fi(path);

    // if this is a relative pathname, prepend current directory
    if (!fi.isAbsolute()) {

        // make a copy of pathname to work from
        QString oldPathname = path;

        // get the working directory and prepend to the path
        path = GetCurrentDirEx();

        if(!path.endsWith(QLatin1Char('/'))) {
            path.append(QLatin1Char('/'));
        }

        path.append(oldPathname);
    }

    /* compress out .. and . */
    return CompressPathnameEx(path);
}

/**
 * @brief CompressPathnameEx
 * @param pathname
 * @return
 *
 * Returns a pathname without symbolic links or redundant "." or ".." elements.
 *
 */
QString CompressPathnameEx(const QString &path) {

    // NOTE(eteran): Things like QFileInfo::canonicalFilePath return an empty
    // string if a path represents a file that doesn't exist yet. So we may not
    // be able to use those in all cases!

    /* (Added by schwarzenberg)
    ** replace multiple slashes by a single slash
    **  (added by yooden)
    **  Except for the first slash. From the Single UNIX Spec: "A pathname
    **  that begins with two successive slashes may be interpreted in an
    **  implementation-dependent manner"
    */

    QString pathname;
    pathname.reserve(path.size());
    auto out = std::back_inserter(pathname);
    auto in  = path.begin();

    *out++ = *in++;
    while(in != path.end()) {
        const QChar ch = *in++;
        *out++ = ch;
        if (ch == QLatin1Char('/')) {
            while(*in == QLatin1Char('/')) {
                ++in;
            }
        }
    }

    /* compress out . and .. */
    QString buffer;
    buffer.reserve(path.size());

    auto inPtr = pathname.begin();
    auto outPtr = std::back_inserter(buffer);

    /* copy initial / */
    copyThruSlash(outPtr, inPtr, pathname.end());

    while (inPtr != pathname.end()) {
        /* if the next component is "../", remove previous component */
        if (compareThruSlash(inPtr, pathname.end(), QLatin1String("../"))) {

            /* If the ../ is at the beginning, or if the previous component is
             * a symbolic link, preserve the ../
             * It is not valid to compress ../ when the previous component is
             * a symbolic link because ../ is relative to where the link
             * points. */

            // NOTE(eteran): in the original NEdit, this code was broken!
            // lstat/QFileInfo ALWAYS returns a non-symlink mode for paths
            // ending in '/'
            // so we need to chop that off for the test!
            QFileInfo fi(buffer.left(buffer.size() - 1));

            if (buffer == QLatin1String("/") || fi.isSymLink()) {
                copyThruSlash(outPtr, inPtr, pathname.end());
            } else {
                /* back up outPtr to remove last path name component */
                int index = prevSlash(buffer, buffer.size());
                if(index != -1) {
                    buffer = buffer.left(index);
                }

                inPtr = nextSlash(inPtr, pathname.end());
            }
        } else if (compareThruSlash(inPtr, pathname.end(), QLatin1String("./"))) {
            /* don't copy the component if it's the redundant "./" */
            inPtr = nextSlash(inPtr, pathname.end());
        } else {
            /* copy the component to outPtr */
            copyThruSlash(outPtr, inPtr, pathname.end());
        }
    }

    return buffer;
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
** In the DOS case, the length will be shorter. The routine has support for
** blockwise file to string conversion: if the fileString has a trailing '\r' and
** 'pendingCR' is not null, the '\r' is deposited in there and is not
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
