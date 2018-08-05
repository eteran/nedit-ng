
#include "Util/FileSystem.h"
#include "Util/ClearCase.h"
#include "Util/FileFormats.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include <QDir>
#include <QFileInfo>
#include <QString>

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
	auto it = std::find(first, last, QLatin1Char('/'));
	return std::next(it);
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
void copyThruSlash(In &first, In last, Out &dest) {

	while (true) {
		*dest = *first;

		if (first == last) {
			return;
		}

		if (*first == QLatin1Char('/')) {
			++first;
			++dest;
			return;
		}

		++first;
		++dest;
	}
}

}

/*
** Decompose a Unix file name into a file name and a path.
** returns false if an error occured (currently, there is no error case).
*/
bool ParseFilenameEx(const QString &fullname, QString *filename, QString *pathname) {

	const int fullLen = fullname.size();
	int scanStart = -1;

	/* For clearcase version extended paths, slash characters after the "@@/"
	   should be considered part of the file name, rather than the path */
	const int viewExtendPath = ClearCase::GetVersionExtendedPathIndex(fullname);
	if (viewExtendPath != -1) {
		scanStart = viewExtendPath - 1;
	}

	// find the last slash
	const int i = fullname.lastIndexOf(QLatin1Char('/'), scanStart);

	// move chars before / (or ] or :) into pathname,& after into filename
	int pathLen = i + 1;
	int fileLen = fullLen - pathLen;

	if (pathname) {
		*pathname = fullname.left(pathLen);
	}

	if (filename) {
		*filename = fullname.mid(pathLen, fileLen);
	}

	if (pathname) {
		*pathname = NormalizePathname(*pathname);
	}

	return true;
}



QString NormalizePathname(const QString &pathname) {

	QString path = pathname;

	QFileInfo fi(path);

	// TODO(eteran): investigate if we can just use QFileInfo::makeAbsolute
	// here...

	// if this is a relative pathname, prepend current directory
	if (fi.isRelative()) {

		// make a copy of pathname to work from and get the working directory
		// and prepend to the path
		QString oldPathname = std::exchange(path, QDir::currentPath());

		if(!path.endsWith(QLatin1Char('/'))) {
			path.append(QLatin1Char('/'));
		}

		path.append(oldPathname);
	}

	/* compress out .. and . */
	return CompressPathname(path);
}

/**
 * @brief CompressPathnameEx
 * @param pathname
 * @return
 *
 * Returns a pathname without symbolic links or redundant "." or ".." elements.
 *
 */
QString CompressPathname(const QString &path) {

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

	// compress out . and ..
	QString buffer;
	buffer.reserve(path.size());

	auto inPtr = pathname.begin();
	auto outPtr = std::back_inserter(buffer);

	// copy initial "/"
	copyThruSlash(inPtr, pathname.end(), outPtr);

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

			// NOTE(eteran): UNIX assumption here...
			if (buffer == QLatin1String("/") || fi.isSymLink()) {
				copyThruSlash(inPtr, pathname.end(), outPtr);
			} else {
				/* back up to remove last path name component */
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
			copyThruSlash(inPtr, pathname.end(), outPtr);
		}
	}

	return buffer;
}

/*
** Return the trailing 'n' no. of path components
*/
QString GetTrailingPathComponents(const QString &path, int noOfComponents) {

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
** characters, to determine whether text represents a MS DOS or Macintosh
** format file.  If there's ANY ambiguity (a newline in the sample not paired
** with a return in an otherwise DOS looking file, or a newline appearing in
** the sampled portion of a Macintosh looking file), the file is judged to be
** Unix format.
*/
FileFormats FormatOfFile(view::string_view text) {

	size_t nNewlines = 0;
	size_t nReturns = 0;

	for (auto it = text.begin(); it != text.end() && it < text.begin() + FORMAT_SAMPLE_CHARS; ++it) {
		if (*it == '\n') {
			nNewlines++;
			if (it == text.begin() || *std::prev(it) != '\r') {
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
** Converts a string (which may represent the entire contents of the file) from
** Unix to DOS format.
*/
void ConvertToDos(std::string &text) {

	// How long a string will we need?
	size_t outLength = 0;
	for (char ch : text) {
		if (ch == '\n') {
			outLength++;
		}
		outLength++;
	}

	std::string outString;
	outString.reserve(outLength);
	auto outPtr = std::back_inserter(outString);

	for (char ch : text) {
		if (ch == '\n') {
			*outPtr++ = '\r';
		}
		*outPtr++ = ch;
	}

	text = std::move(outString);
}

/*
** Converts a string (which may represent the entire contents of the file)
** from Unix to Macintosh format.
*/
void ConvertToMac(std::string &text) {

	std::transform(text.begin(), text.end(), text.begin(), [](char ch) {
		if (ch == '\n') {
			return '\r';
		}

		return ch;
	});
}

/**
 * @brief ConvertFromMac
 * @param text
 */
void ConvertFromMac(std::string &text) {

	std::transform(text.begin(), text.end(), text.begin(), [](char ch) {
		if(ch == '\r') {
			return '\n';
		}

		return ch;
	});
}

/**
 * @brief ConvertFromDos
 * @param text
 */
void ConvertFromDos(std::string &text) {
	ConvertFromDos(text, nullptr);
}

/**
 * @brief ConvertFromDos
 * @param text
 * @param pendingCR
 */
void ConvertFromDos(std::string &text, char *pendingCR) {

	if (pendingCR) {
		*pendingCR = '\0';
	}

	auto out = text.begin();
	auto it  = text.begin();

	while (it != text.end()) {
		if (*it == '\r') {
			auto next = std::next(it);
			if (next != text.end()) {
				if (*next == '\n') {
					++it;
				}
			} else {
				if (pendingCR) {
					*pendingCR = *it;
					break;
				}
			}
		}
		*out++ = *it++;
	}

	text.erase(out, text.end());
}

/*
** Reads a text file into a string buffer, converting line breaks to
** unix-style if appropriate.
**
** Force a terminating \n, if this is requested
*/
QString ReadAnyTextFile(const QString &fileName, bool forceNL) {

	std::ifstream file(fileName.toStdString());
	if(!file) {
		return {};
	}

	auto contents = std::string(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});

	switch(FormatOfFile(contents)) {
	case FileFormats::Dos:
		ConvertFromDos(contents);
		break;
	case FileFormats::Mac:
		ConvertFromMac(contents);
		break;
	case FileFormats::Unix:
		break;
	}

	if(contents.empty()) {
		return {};
	}

	// now, that the string is in Unix format, check for terminating \n
	if (forceNL && contents.back() != '\n') {
		contents.push_back('\n');
	}

	return QString::fromStdString(contents);
}
