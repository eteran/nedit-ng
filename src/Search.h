
#ifndef SEARCH_H_
#define SEARCH_H_

#include "Direction.h"
#include "SearchType.h"
#include "Util/string_view.h"
#include "WrapMode.h"

#include <QString>
#include <optional>

class DocumentWidget;
class MainWindow;
class TextArea;
class Regex;

namespace Search {

struct HistoryEntry {
	QString search;
	QString replace;
	SearchType type;
};

struct Result {
	int64_t start    = 0;
	int64_t end      = 0;
	int64_t extentBW = 0;
	int64_t extentFW = 0;
};

bool isRegexType(SearchType searchType);
bool replaceUsingRE(const QString &searchStr, const QString &replaceStr, view::string_view sourceStr, int64_t beginPos, std::string &dest, int prevChar, const QString &delimiters, int defaultFlags);
bool SearchString(view::string_view string, const QString &searchString, Direction direction, SearchType searchType, WrapMode wrap, int64_t beginPos, Result *result, const QString &delimiters);
std::optional<Result> SearchString(view::string_view string, const QString &searchString, Direction direction, SearchType searchType, WrapMode wrap, int64_t beginPos, const QString &delimiters);
int defaultRegexFlags(SearchType searchType);
int historyIndex(int nCycles);
std::optional<std::string> ReplaceAllInString(view::string_view inString, const QString &searchString, const QString &replaceString, SearchType searchType, int64_t *copyStart, int64_t *copyEnd, const QString &delimiters);
void saveSearchHistory(const QString &searchString, QString replaceString, SearchType searchType, bool isIncremental);
HistoryEntry *HistoryByIndex(int index);

}

#endif
