
#ifndef UTIL_FILESYSTEM_H_
#define UTIL_FILESYSTEM_H_

#include <string>
#include "string_view.h"
#include <QtGlobal>

class QString;

enum class FileFormats : int;

FileFormats FormatOfFile(view::string_view text);
QString GetTrailingPathComponents(const QString &path, int noOfComponents);
QString NormalizePathname(const QString &pathname);
QString ReadAnyTextFile(const QString &fileName, bool forceNL);
bool parseFilename(const QString &fullname, QString *filename, QString *pathname);

// std::string based convesions
void ConvertToMac(std::string &text);
void ConvertToDos(std::string &text);
void ConvertFromMac(std::string &text);
void ConvertFromDos(std::string &text);
void ConvertFromDos(std::string &text, char *pendingCR);

template <class Integer>
using IsInteger = typename std::enable_if<std::is_integral<Integer>::value>::type;

/*
** Converts a string (which may represent the entire contents of the file)
** from DOS or Macintosh format to Unix format.  Conversion is done in-place.
**
** In the DOS case, the length will be shorter. The routine has support for
** blockwise file to string conversion: if the text has a trailing '\r' and
** 'pendingCR' is not null, the '\r' is deposited in there and is not
** converted. If there is no trailing '\r', a '\0' is deposited in 'pendingCR'
** It's the caller's responsability to make sure that the pending character,
** if present, is inserted at the beginning of the next block to convert.
*/
template <class Length, class = IsInteger<Length>>
void ConvertFromMac(char *text, Length length) {

	Q_ASSERT(text);
	std::replace(text, text + length, '\r', '\n');
}

/**
 * @brief ConvertFromDos
 * @param text
 * @param length
 * @param pendingCR
 */
template <class Length, class = IsInteger<Length>>
void ConvertFromDos(char *text, Length *length, char *pendingCR) {

	Q_ASSERT(text);
	char *out       = text;
	const char *in  = text;
	const char *end = text + *length;

	if (pendingCR) {
		*pendingCR = '\0';
	}

	while (in < end) {
		if (*in == '\r') {
			if (in < end - 1) {
				if (in[1] == '\n') {
					++in;
				}
			} else {
				if (pendingCR) {
					*pendingCR = *in;
					break;
				}
			}
		}
		*out++ = *in++;
	}

	*length = static_cast<Length>(out - text);
}

#endif
