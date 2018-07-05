
#ifndef HIGHLIGHT_H_
#define HIGHLIGHT_H_

#include "Util/string_view.h"
#include "TextBufferFwd.h"
#include "TextCursor.h"

#include <gsl/span>
#include <boost/optional.hpp>
#include <vector>

#include <QCoreApplication>

class HighlightData;
class HighlightPattern;
class PatternSet;
class TextArea;
class WindowHighlightData;
struct HighlightStyle;
struct ReparseContext;
class Style;
class Input;
class Regex;

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

class Highlight {
    Q_DECLARE_TR_FUNCTIONS(Highlight)
public:
    Highlight() = delete;

public:
    static void loadTheme();
    static void saveTheme();
    static bool FontOfNamedStyleIsBold(const QString &styleName);
    static bool FontOfNamedStyleIsItalic(const QString &styleName);
    static bool isDefaultPatternSet(const PatternSet &patternSet);
    static bool LoadHighlightString(const QString &string);
    static bool NamedStyleExists(const QString &styleName);
    static bool parseString(const HighlightData *pattern, const char *first, const char *last, const char **string, char **styleString, int64_t length, int *prevChar, bool anchored, const QString &delimiters, const char *look_behind_to, const char *match_to);
    static bool patternIsParsable(HighlightData *pattern);
    static bool readHighlightPattern(Input &in, QString *errMsg, HighlightPattern *pattern);
    static HighlightData *patternOfStyle(HighlightData *patterns, int style);
    static int findSafeParseRestartPos(TextBuffer *buf, const std::unique_ptr<WindowHighlightData> &highlightData, TextCursor *pos);
    static int findTopLevelParentIndex(const gsl::span<HighlightPattern> &patterns, int index);
    static int getPrevChar(TextBuffer *buf, TextCursor pos);
    static int indexOfNamedPattern(const gsl::span<HighlightPattern> &patterns, const QString &name);
    static PatternSet *FindPatternSet(const QString &languageMode);
    static QString BgColorOfNamedStyleEx(const QString &styleName);
    static QString createPatternsString(const PatternSet *patternSet, const QString &indentString);
    static QString FgColorOfNamedStyleEx(const QString &styleName);
    static QString WriteHighlightString();
    static size_t IndexOfNamedStyle(const QString &styleName);
    static std::unique_ptr<PatternSet> readDefaultPatternSet(const QString &langModeName);
    static std::unique_ptr<PatternSet> readDefaultPatternSet(QByteArray &patternData, const QString &langModeName);
    static std::unique_ptr<PatternSet> readPatternSet(Input &in);
    static boost::optional<std::vector<HighlightPattern>> readHighlightPatterns(Input &in, QString *errMsg);
    static TextCursor backwardOneContext(TextBuffer *buf, const ReparseContext &context, TextCursor fromPos);
    static TextCursor forwardOneContext(TextBuffer *buf, const ReparseContext &context, TextCursor fromPos);
    static TextCursor lastModified(const std::shared_ptr<TextBuffer> &styleBuf);
    static TextCursor parseBufferRange(const HighlightData *pass1Patterns, const HighlightData *pass2Patterns, TextBuffer *buf, const std::shared_ptr<TextBuffer> &styleBuf, const ReparseContext &contextRequirements, TextCursor beginParse, TextCursor endParse, const QString &delimiters);
    static void fillStyleString(const char **stringPtr, char **stylePtr, const char *toPtr, uint8_t style, int *prevChar);
    static void incrementalReparse(const std::unique_ptr<WindowHighlightData> &highlightData, TextBuffer *buf, TextCursor pos, int64_t nInserted, const QString &delimiters);
    static void modifyStyleBuf(const std::shared_ptr<TextBuffer> &styleBuf, char *styleString, TextCursor startPos, TextCursor endPos, int firstPass2Style);
    static void passTwoParseString(const HighlightData *pattern, const char *first, const char *last, const char *string, char *styleString, int64_t length, int *prevChar, const QString &delimiters, const char *lookBehindTo, const char *match_till);
    static void recolorSubexpr(const std::shared_ptr<Regex> &re, size_t subexpr, int style, const char *string, char *styleString);
    static void RenameHighlightPattern(const QString &oldName, const QString &newName);
	
public:
    static std::vector<HighlightStyle> HighlightStyles;
    static std::vector<PatternSet>     PatternSets;
};

void SyntaxHighlightModifyCBEx(TextCursor pos, int64_t nInserted, int64_t nDeleted, int64_t nRestyled, view::string_view deletedText, void *user);


#endif
