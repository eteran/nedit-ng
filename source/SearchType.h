
#ifndef SEARCH_TYPE_H_
#define SEARCH_TYPE_H_

#include <QMetaType>

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

Q_DECLARE_METATYPE(SearchType)

#endif
