
#ifndef SEARCH_TYPE_H_
#define SEARCH_TYPE_H_

#include <QLatin1String>
#include <QtDebug>

/*
** Schwarzenberg: added LiteralWord .. RegexNoCase
*/
enum class SearchType {
	Literal,
	CaseSense,
	Regex,
	LiteralWord,
	CaseSenseWord,
	RegexNoCase,
};

template <class T>
inline T from_integer(int value);

template <>
inline SearchType from_integer(int value) {
	switch (value) {
	case static_cast<int>(SearchType::Literal):
	case static_cast<int>(SearchType::CaseSense):
	case static_cast<int>(SearchType::Regex):
	case static_cast<int>(SearchType::LiteralWord):
	case static_cast<int>(SearchType::CaseSenseWord):
	case static_cast<int>(SearchType::RegexNoCase):
		return static_cast<SearchType>(value);
	default:
		qWarning("NEdit: Invalid value for SearchType");
		return SearchType::Literal;
	}
}

inline QLatin1String to_string(SearchType style) {

	switch (style) {
	case SearchType::Literal:
		return QLatin1String("literal");
	case SearchType::CaseSense:
		return QLatin1String("case");
	case SearchType::Regex:
		return QLatin1String("regex");
	case SearchType::LiteralWord:
		return QLatin1String("word");
	case SearchType::CaseSenseWord:
		return QLatin1String("caseWord");
	case SearchType::RegexNoCase:
		return QLatin1String("regexNoCase");
	}

	Q_UNREACHABLE();
}

#endif
