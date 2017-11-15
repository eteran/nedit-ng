
#ifndef SEARCH_TYPE_H_
#define SEARCH_TYPE_H_

#include <QLatin1String>

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

inline QLatin1String to_string(SearchType style) {

    switch(style) {
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
