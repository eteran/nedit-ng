
#ifndef SETTINGS_H_
#define SETTINGS_H_

#include "WrapStyle.h"
#include "IndentStyle.h"
#include "ShowMatchingStyle.h"
#include "TruncSubstitution.h"
#include "SearchType.h"

#include <QFont>
#include <QString>
#include <QCoreApplication>

// Identifiers for the different colors that can be adjusted.
enum ColorTypes : int {
    TEXT_FG_COLOR,
    TEXT_BG_COLOR,
    SELECT_FG_COLOR,
    SELECT_BG_COLOR,
    HILITE_FG_COLOR,
    HILITE_BG_COLOR,
    LINENO_FG_COLOR,
    CURSOR_FG_COLOR,
};

class Settings  {
    Q_DECLARE_TR_FUNCTIONS(Settings)

private:
    Settings() = delete;

public:
    static void loadPreferences();
    static bool savePreferences();
    static void importSettings(const QString &filename);

public:
    static QString configFile();
    static QString historyFile();
    static QString autoLoadMacroFile();
    static QString styleFile();
    static QString themeFile();

public:
    static bool appendLF;
    static bool autoSave;
    static bool autoScroll;
    static bool backlightChars;
    static bool beepOnSearchWrap;
    static bool globalTabNavigate;
    static bool highlightSyntax;
    static bool insertTabs;
    static bool iSearchLine;
    static bool lineNumbers;
    static bool matchSyntaxBased;
    static bool openInTab;
    static bool pathInWindowsMenu;
    static bool prefFileRead;
    static bool repositionDialogs;
    static bool retainSearchDialogs;
    static bool saveOldVersion;
    static bool searchDialogs;
    static bool searchWraps;
    static bool smartTags;
    static bool sortOpenPrevMenu;
    static bool sortTabs;
    static bool statisticsLine;
    static bool tabBar;
    static bool tabBarHideOne;
    static bool toolTips;
    static bool warnExit;
    static bool warnFileMods;
    static bool warnRealFileMods;
    static int fileVersion;
    static IndentStyle autoIndent;
    static WrapStyle autoWrap;
    static int emulateTabs;
    static SearchType searchMethod;
    static ShowMatchingStyle showMatching;
    static int tabDistance;
    static int textCols;
    static int textRows;
    static int wrapMargin;
    static QString bgMenuCommands;
    static QString boldHighlightFont;
    static QString boldItalicHighlightFont;
    static QString colors[8];
    static QString geometry;
    static QString highlightPatterns;
    static QString italicHighlightFont;
    static QString languageModes;
    static QString macroCommands;
    static QString serverName;
    static QString shell;
    static QString shellCommands;
    static QString smartIndentInit;
    static QString smartIndentInitCommon;
    static QString textFont;
    static QString titleFormat;

public:
    // Advanced
    static bool autoWrapPastedText;
    static bool colorizeHighlightedText;
    static bool heavyCursor;
    static bool alwaysCheckRelativeTagsSpecs;
    static bool findReplaceUsesSelection;
    static bool focusOnRaise;
    static bool forceOSConversion;
    static bool honorSymlinks;
    static bool stickyCaseSenseButton;
    static bool typingHidesPointer;
    static bool undoModifiesSelection;
    static int autoScrollVPadding;
    static int maxPrevOpenFiles;
    static TruncSubstitution truncSubstitution;
    static QString backlightCharTypes;
    static QString tagFile;
    static QString wordDelimiters;

public:
    // Created implicitly from other "real" settings
    static QFont plainFontStruct;
    static QFont boldFontStruct;
    static QFont italicFontStruct;
    static QFont boldItalicFontStruct;
};

#endif
