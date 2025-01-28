
#include "Util/FileSystem.h"
#include "Util/ClearCase.h"
#include "Util/FileFormats.h"

#include <fstream>
#include <iostream>
#include <iterator>

#include <QDir>
#include <QFileInfo>

namespace {

/* Parameters to algorithm used to auto-detect DOS format files.  NEdit will
   scan up to the lesser of FORMAT_SAMPLE_LINES lines and FORMAT_SAMPLE_CHARS
   characters of the beginning of the file, checking that all newlines are
   paired with carriage returns.  If even a single counterexample exists,
   the file is judged to be in Unix format. */
constexpr int FORMAT_SAMPLE_LINES = 5;
constexpr int FORMAT_SAMPLE_CHARS = 2000;

}

/**
 * Decompose a Unix file name into a file name and a path.
 *
 * @brief parseFilename
 * @param fullname
 * @return
 */
PathInfo parseFilename(const QString &fullname) {

	PathInfo fileInfo;

	const QString cleanedPath = QDir::cleanPath(fullname);

	const int fullLen = cleanedPath.size();
	int scanStart     = -1;

	/* For clearcase version extended paths, slash characters after the "@@/"
	   should be considered part of the file name, rather than the path */
	const int viewExtendPath = ClearCase::GetVersionExtendedPathIndex(cleanedPath);
	if (viewExtendPath != -1) {
		scanStart = viewExtendPath - 1;
	}

	// find the last slash
	const int i = cleanedPath.lastIndexOf(QLatin1Char('/'), scanStart);

	// move chars before / (or ] or :) into pathname,& after into filename
	const int pathLen = i + 1;
	const int fileLen = fullLen - pathLen;

	fileInfo.pathname = cleanedPath.left(pathLen);
	fileInfo.filename = cleanedPath.mid(pathLen, fileLen);
	fileInfo.pathname = NormalizePathname(fileInfo.pathname);

	return fileInfo;
}

/**
 * @brief NormalizePathname
 * @param pathname
 * @return
 */
QString NormalizePathname(const QString &pathname) {

	QString path = QDir::cleanPath(pathname);
	const QFileInfo fi(path);

	// if this is a relative pathname, prepend current directory
	if (fi.isRelative()) {

		const QString oldPathname = std::exchange(path, QDir::currentPath());

		if (!path.endsWith(QLatin1Char('/'))) {
			path.append(QLatin1Char('/'));
		}

		path.append(oldPathname);
	}

	QString cleanedPath = QDir::cleanPath(path);
	const QFileInfo cleanedFi(cleanedPath);

	// IFF it is a directory, insist that it ends in a slash
	if (cleanedFi.isDir()) {
		if (!cleanedPath.endsWith(QLatin1Char('/'))) {
			cleanedPath.append(QLatin1Char('/'));
		}
	}

	return cleanedPath;
}

/*
** Return the trailing 'n' no. of path components
*/
QString GetTrailingPathComponents(const QString &path, int components) {

	return path.section(
		QLatin1Char('/'),
		-1 - components,
		-1,
		QString::SectionIncludeLeadingSep);
}

/*
** Samples up to a maximum of FORMAT_SAMPLE_LINES lines and FORMAT_SAMPLE_CHARS
** characters, to determine whether text represents a MS DOS or Macintosh
** format file.  If there's ANY ambiguity (a newline in the sample not paired
** with a return in an otherwise DOS looking file, or a newline appearing in
** the sampled portion of a Macintosh looking file), the file is judged to be
** Unix format.
*/
FileFormats FormatOfFile(std::string_view text) {

	size_t nNewlines = 0;
	size_t nReturns  = 0;

	const size_t sampleSize = std::min<size_t>(text.size(), FORMAT_SAMPLE_CHARS);
	const auto end          = text.begin() + sampleSize;

	for (auto it = text.begin(); it != end; ++it) {
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
	for (const char ch : text) {
		if (ch == '\n') {
			outLength++;
		}
		outLength++;
	}

	std::string outString;
	outString.reserve(outLength);
	auto outPtr = std::back_inserter(outString);

	for (const char ch : text) {
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
	std::replace(text.begin(), text.end(), '\n', '\r');
}

/**
 * @brief ConvertFromMac
 * @param text
 */
void ConvertFromMac(std::string &text) {
	std::replace(text.begin(), text.end(), '\r', '\n');
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
	if (!file) {
		return {};
	}

	try {
		auto contents = std::string(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});

		switch (FormatOfFile(contents)) {
		case FileFormats::Dos:
			ConvertFromDos(contents);
			break;
		case FileFormats::Mac:
			ConvertFromMac(contents);
			break;
		case FileFormats::Unix:
			break;
		}

		if (contents.empty()) {
			return {};
		}

		// now, that the string is in Unix format, check for terminating \n
		if (forceNL && contents.back() != '\n') {
			contents.push_back('\n');
		}

		return QString::fromStdString(contents);
	} catch (const std::ios_base::failure &ex) {
		qWarning("NEdit: Error while reading file. %s", ex.what());
		return {};
	}
}
