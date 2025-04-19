
#ifndef UTIL_FILESYSTEM_H_
#define UTIL_FILESYSTEM_H_

#include <QString>
#include <QtGlobal>

#include <optional>
#include <string>
#include <string_view>

enum class FileFormats : int;

struct PathInfo {
	QString pathname;
	QString filename;
};

FileFormats FormatOfFile(std::string_view text);
QString GetTrailingPathComponents(const QString &path, int components);
QString NormalizePathname(const QString &pathname);
QString ReadAnyTextFile(const QString &fileName, bool forceNL);
PathInfo parseFilename(const QString &fullname);

// std::string based conversions
void ConvertToMac(std::string &text);
void ConvertToDos(std::string &text);
void ConvertFromMac(std::string &text);
void ConvertFromDos(std::string &text);
void ConvertFromDos(std::string &text, char *pendingCR);

template <class Integer>
using IsInteger = typename std::enable_if<std::is_integral<Integer>::value>::type;

/**
 * @brief Converts a string from Mac format to Unix format in place.
 *
 * @param text The text to convert, which may represent the entire contents of the file.
 * @param length The length of the text in characters.
 */
template <class Length, class = IsInteger<Length>>
void ConvertFromMac(char *text, Length length) {

	Q_ASSERT(text);
	std::replace(text, text + length, '\r', '\n');
}

/**
 * @brief Converts a string from DOS format to Unix format in place.
 *
 * @param text The text to convert, which may represent the entire contents of the file.
 * @param length The length of the text in characters.
 * @param pendingCR An optional pointer to a character that will hold a pending '\r' if it exists.
 *                  If there is no trailing '\r', it will be set to '\0'.
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
