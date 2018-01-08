
#ifndef HIGHLIGHT_H_
#define HIGHLIGHT_H_

#include "Util/string_view.h"
#include "TextBufferFwd.h"

#include <gsl/span>
#include <vector>

class HighlightData;
class HighlightPattern;
class PatternSet;
class TextArea;
class WindowHighlightData;
struct HighlightStyle;
struct ReparseContext;
class Style;

class QColor;
class QString;
class QWidget;

// Pattern flags for modifying pattern matching behavior
enum {
	PARSE_SUBPATS_FROM_START = 1,
	DEFER_PARSING            = 2,
	COLOR_ONLY               = 4
};

// How much re-parsing to do when an unfinished style is encountered
constexpr int PASS_2_REPARSE_CHUNK_SIZE = 1000;

// Don't use plain 'A' or 'B' for style indices, it causes problems
// with EBCDIC coding (possibly negative offsets when subtracting 'A').
constexpr auto ASCII_A = static_cast<char>(65);

/* Meanings of style buffer characters (styles). Don't use plain 'A' or 'B';
   it causes problems with EBCDIC coding (possibly negative offsets when
   subtracting 'A'). */
constexpr uint8_t UNFINISHED_STYLE = ASCII_A;

constexpr uint8_t PLAIN_STYLE = (ASCII_A + 1);

constexpr auto STYLE_NOT_FOUND = static_cast<size_t>(-1);

void LoadTheme();
void SaveTheme();
bool LoadHighlightStringEx(const QString &string);
bool NamedStyleExists(const QString &styleName);
bool parseString(HighlightData *pattern, const char **string, char **styleString, int64_t length, char *prevChar, bool anchored, const QString &delimiters, const char *look_behind_to, const char *match_till);
char getPrevChar(TextBuffer *buf, int64_t pos);
HighlightData *patternOfStyle(HighlightData *patterns, int style);
int64_t backwardOneContext(TextBuffer *buf, ReparseContext *context, int64_t fromPos);
int findTopLevelParentIndex(const gsl::span<HighlightPattern> &patList, int index);
int FontOfNamedStyleIsBold(const QString &styleName);
int FontOfNamedStyleIsItalic(const QString &styleName);
int64_t forwardOneContext(TextBuffer *buf, ReparseContext *context, int64_t fromPos);
int indexOfNamedPattern(const gsl::span<HighlightPattern> &patList, const QString &patName);
PatternSet *FindPatternSet(const QString &langModeName);
QString BgColorOfNamedStyleEx(const QString &styleName);
QString ColorOfNamedStyleEx(const QString &styleName);
QString WriteHighlightStringEx();
size_t IndexOfNamedStyle(const QString &styleName);
std::unique_ptr<PatternSet> readDefaultPatternSet(const QString &langModeName);
void handleUnparsedRegionCBEx(const TextArea *area, int64_t pos, const void *user);
void RemoveWidgetHighlightEx(TextArea *area);
void RenameHighlightPattern(const QString &oldName, const QString &newName);
void SyntaxHighlightModifyCBEx(int64_t pos, int64_t nInserted, int64_t nDeleted, int64_t nRestyled, view::string_view deletedText, void *user);


// list of available highlight styles
extern std::vector<HighlightStyle> HighlightStyles;
extern std::vector<PatternSet> PatternSets;
#endif
