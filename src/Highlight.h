
#ifndef HIGHLIGHT_H_
#define HIGHLIGHT_H_

#include "TextBufferFwd.h"
#include "TextCursor.h"
#include "Util/QtHelper.h"
#include <string_view>

#include <memory>
#include <optional>
#include <vector>

#include <QCoreApplication>

class HighlightPattern;
class PatternSet;
struct HighlightData;
struct HighlightStyle;
struct ReparseContext;

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

constexpr char ASCII_A = 'A';

// Meanings of style buffer characters (styles)
constexpr uint8_t UNFINISHED_STYLE = (ASCII_A + 0);
constexpr uint8_t PLAIN_STYLE      = (ASCII_A + 1);

constexpr auto PATTERN_NOT_FOUND = static_cast<size_t>(-1);

namespace Highlight {
Q_DECLARE_NAMESPACE_TR(Highlight)

struct ParseContext {
	int *prev_char = nullptr;
	QString delimiters;
	std::string_view text;
};

bool FontOfNamedStyleIsBold(const QString &styleName);
bool FontOfNamedStyleIsItalic(const QString &styleName);
void LoadHighlightString(const QString &string);
bool NamedStyleExists(const QString &styleName);
bool parseString(const HighlightData *pattern, const char *&string_ptr, uint8_t *&style_ptr, int64_t length, const ParseContext *ctx, const char *look_behind_to, const char *match_to);
HighlightData *patternOfStyle(const std::unique_ptr<HighlightData[]> &patterns, uint8_t style);
size_t findTopLevelParentIndex(const std::vector<HighlightPattern> &patterns, size_t index);
size_t indexOfNamedPattern(const std::vector<HighlightPattern> &patterns, const QString &name);
int getPrevChar(TextBuffer *buf, TextCursor pos);
PatternSet *FindPatternSet(const QString &languageMode);
QString BgColorOfNamedStyle(const QString &styleName);
QString FgColorOfNamedStyle(const QString &styleName);
QString WriteHighlightString();
size_t IndexOfNamedStyle(const QString &styleName);
std::optional<PatternSet> readDefaultPatternSet(const QString &langModeName);
TextCursor backwardOneContext(TextBuffer *buf, const ReparseContext &context, TextCursor fromPos);
TextCursor forwardOneContext(TextBuffer *buf, const ReparseContext &context, TextCursor fromPos);
void RenameHighlightPattern(const QString &oldName, const QString &newName);
void SyntaxHighlightModifyCB(TextCursor pos, int64_t nInserted, int64_t nDeleted, int64_t nRestyled, std::string_view deletedText, void *user);

extern std::vector<HighlightStyle> HighlightStyles;
extern std::vector<PatternSet> PatternSets;
}

#endif
