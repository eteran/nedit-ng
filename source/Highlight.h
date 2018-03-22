
#ifndef HIGHLIGHT_H_
#define HIGHLIGHT_H_

#include "Util/string_view.h"
#include "TextBufferFwd.h"

#include <gsl/span>
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
    static bool LoadHighlightStringEx(const QString &string);
    static bool NamedStyleExists(const QString &styleName);
    static bool parseString(HighlightData *pattern, const char *first, const char *last, const char **string, char **styleString, int64_t length, int *prevChar, bool anchored, const QString &delimiters, const char *look_behind_to, const char *match_to);
    static int getPrevChar(TextBuffer *buf, int64_t pos);
    static HighlightData *patternOfStyle(HighlightData *patterns, int style);
    static int64_t backwardOneContext(TextBuffer *buf, const ReparseContext &context, int64_t fromPos);
    static int findTopLevelParentIndex(const gsl::span<HighlightPattern> &patList, int index);
    static bool FontOfNamedStyleIsBold(const QString &styleName);
    static bool FontOfNamedStyleIsItalic(const QString &styleName);
    static int64_t forwardOneContext(TextBuffer *buf, const ReparseContext &context, int64_t fromPos);
    static int indexOfNamedPattern(const gsl::span<HighlightPattern> &patList, const QString &patName);
    static PatternSet *FindPatternSet(const QString &langModeName);
    static QString BgColorOfNamedStyleEx(const QString &styleName);
    static QString FgColorOfNamedStyleEx(const QString &styleName);
    static QString WriteHighlightStringEx();
    static size_t IndexOfNamedStyle(const QString &styleName);
    static std::unique_ptr<PatternSet> readDefaultPatternSet(const QString &langModeName);
    static void RenameHighlightPattern(const QString &oldName, const QString &newName);

public:
    static void incrementalReparse(const std::unique_ptr<WindowHighlightData> &highlightData, TextBuffer *buf, int64_t pos, int64_t nInserted, const QString &delimiters);

private:
    static std::unique_ptr<PatternSet> readPatternSetEx(Input &in);
    static int64_t parseBufferRange(HighlightData *pass1Patterns, HighlightData *pass2Patterns, TextBuffer *buf, const std::shared_ptr<TextBuffer> &styleBuf, const ReparseContext &contextRequirements, int64_t beginParse, int64_t endParse, const QString &delimiters);
    static int findSafeParseRestartPos(TextBuffer *buf, const std::unique_ptr<WindowHighlightData> &highlightData, int64_t *pos);
    static void passTwoParseString(HighlightData *pattern, const char *first, const char *last, const char *string, char *styleString, int64_t length, int *prevChar, const QString &delimiters, const char *lookBehindTo, const char *match_till);
    static std::unique_ptr<PatternSet> readDefaultPatternSet(QByteArray &patternData, const QString &langModeName);
    static std::unique_ptr<PatternSet> highlightErrorEx(const Input &in, const QString &message);
    static bool isDefaultPatternSet(const PatternSet *patSet);
    static void fillStyleString(const char **stringPtr, char **stylePtr, const char *toPtr, uint8_t style, int *prevChar);
    static void modifyStyleBuf(const std::shared_ptr<TextBuffer> &styleBuf, char *styleString, int64_t startPos, int64_t endPos, int firstPass2Style);
    static int64_t lastModified(const std::shared_ptr<TextBuffer> &styleBuf);
    static int parentStyleOf(const std::vector<uint8_t> &parentStyles, int style);
    static bool isParentStyle(const std::vector<uint8_t> &parentStyles, int style1, int style2);
    static bool patternIsParsable(HighlightData *pattern);
    static void recolorSubexpr(const std::shared_ptr<Regex> &re, int subexpr, int style, const char *string, char *styleString);
    static int readHighlightPatternEx(Input &in, QString *errMsg, HighlightPattern *pattern);
    static std::vector<HighlightPattern> readHighlightPatternsEx(Input &in, int withBraces, QString *errMsg, bool *ok);
    static QString createPatternsString(const PatternSet *patSet, const QString &indentStr);

public:
    static std::vector<HighlightStyle> HighlightStyles;
    static std::vector<PatternSet>     PatternSets;
};

void SyntaxHighlightModifyCBEx(int64_t pos, int64_t nInserted, int64_t nDeleted, int64_t nRestyled, view::string_view deletedText, void *user);


#endif
