
#ifndef UTIL_FILESYSTEM_H_
#define UTIL_FILESYSTEM_H_

#include <string>
#include "string_view.h"
#include <QtGlobal>

class QString;

enum class FileFormats : int;

FileFormats FormatOfFile(view::string_view fileString);
QString CompressPathname(const QString &pathname);
QString GetTrailingPathComponents(const QString &path, int noOfComponents);
QString NormalizePathname(const QString &pathname);
QString ReadAnyTextFileEx(const QString &fileName, bool forceNL);
bool ParseFilenameEx(const QString &fullname, QString *filename, QString *pathname);

void ConvertToDos(std::string &text);
void ConvertToMac(std::string &text);

template <class Integer>
using IsInteger = typename std::enable_if<std::is_integral<Integer>::value>::type;

template <class Ch, class Length, class = IsInteger<Length>>
void ConvertFromMac(Ch *text, Length length) {

	Q_ASSERT(text);
	std::transform(text, text + length, text, [](Ch ch) {
		if(ch == Ch('\r')) {
			return Ch('\n');
		}

		return ch;
	});
}

/**
 * @brief ConvertFromMac
 * @param text
 */
template <class String>
void ConvertFromMac(String *text) {

	using Ch = typename String::value_type;

	Q_ASSERT(text);
	std::transform(text->begin(), text->end(), text->begin(), [](Ch ch) {
		if(ch == Ch('\r')) {
			return Ch('\n');
		}

		return ch;
	});
}

template <class Ch, class Length, class = IsInteger<Length>>
void ConvertFromDos(Ch *text, Length *length, Ch *pendingCR) {

	Q_ASSERT(text);
	char *out       = text;
	const char *in  = text;

	if (pendingCR) {
		*pendingCR = Ch('\0');
	}

	while (in < text + *length) {
		if (*in == Ch('\r')) {
			if (in < text + *length - 1) {
				if (in[1] == Ch('\n')) {
					++in;
				}
			} else {
				if (pendingCR) {
					*pendingCR = *in;
					break; // Don't copy this trailing '\r'
				}
			}
		}
		*out++ = *in++;
	}

	*length = static_cast<Length>(out - text);
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
template <class Ch, class String, class = typename std::enable_if<std::is_same<Ch, typename String::value_type>::value>::type>
void ConvertFromDos(String *text, Ch *pendingCR) {

	Q_ASSERT(text);

	if (pendingCR) {
		*pendingCR = Ch('\0');
	}

	auto out = text->begin();
	auto it  = text->begin();

	while (it != text->end()) {
		if (*it == Ch('\r')) {
			auto next = std::next(it);
			if (next != text->end()) {
				if (*next == Ch('\n')) {
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
	text->erase(out, text->end());
}

template <class String>
void ConvertFromDos(String *text) {
	using Ch = typename String::value_type;
	ConvertFromDos(text, static_cast<Ch *>(nullptr));
}

template <class Ch, class Length, class = IsInteger<Length>>
void ConvertFromDos(Ch *text, Length *length) {
	ConvertFromDos(text, length, static_cast<Ch *>(nullptr));
}

#endif
