
#ifndef SETTINGS_H_
#define SETTINGS_H_

#include "WrapStyle.h"
#include "IndentStyle.h"
#include "ShowMatchingStyle.h"
#include "TruncSubstitution.h"
#include "VirtKeyOverride.h"
#include "SearchType.h"
#include "ReplaceAllDefaultScope.h"

#include <QFont>
#include <QObject>
#include <QString>

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

class Settings : QObject {
    Q_OBJECT
public:
    void loadPreferences();
    bool savePreferences();
	void importSettings(const QString &filename);

public:
    static QString configFile();
    static QString historyFile();
    static QString autoLoadMacroFile();
    static QString styleFile();
    static QString themeFile();

public:
    bool appendLF;
    bool autoSave;
    bool autoScroll;
    bool backlightChars;
    bool beepOnSearchWrap;
    bool globalTabNavigate;
    bool highlightSyntax;
    bool insertTabs;
    bool iSearchLine;
    bool lineNumbers;
    bool matchSyntaxBased;
    bool openInTab;
    bool pathInWindowsMenu;
    bool prefFileRead;
    bool repositionDialogs;
    bool retainSearchDialogs;
    bool saveOldVersion;
    bool searchDialogs;
    bool searchWraps;
    bool smartTags;
    bool sortOpenPrevMenu;
    bool sortTabs;
    bool statisticsLine;
    bool tabBar;
    bool tabBarHideOne;
    bool toolTips;
    bool warnExit;
    bool warnFileMods;
    bool warnRealFileMods;
    int fileVersion;
    IndentStyle autoIndent;
    WrapStyle autoWrap;
    int emulateTabs;
    SearchType searchMethod;
    ShowMatchingStyle showMatching;
    int tabDistance;
    int textCols;
    int textRows;
    int wrapMargin;
    QString bgMenuCommands;
    QString boldHighlightFont;
    QString boldItalicHighlightFont;
    QString colors[8];
    QString geometry;
    QString highlightPatterns;
    QString italicHighlightFont;
    QString languageModes;
    QString macroCommands;
    QString serverName;
    QString shell;
    QString shellCommands;
    QString smartIndentInit;
    QString smartIndentInitCommon;
    QString textFont;
    QString titleFormat;
#if defined(REPLACE_SCOPE)
    int replaceDefaultScope;
#endif

public:
    // Advanced
    bool alwaysCheckRelativeTagsSpecs;
    bool findReplaceUsesSelection;
    bool focusOnRaise;
    bool forceOSConversion;
    bool honorSymlinks;
    bool remapDeleteKey;
    bool stdOpenDialog;
    bool stickyCaseSenseButton;
    bool typingHidesPointer;
    bool undoModifiesSelection;
    int autoScrollVPadding;
    int maxPrevOpenFiles;
    VirtKeyOverride overrideDefaultVirtualKeyBindings;
    TruncSubstitution truncSubstitution;
    QString backlightCharTypes;
    QString tagFile;
    QString wordDelimiters;

public:
    // Created implicitly from other "real" settings
    QFont plainFontStruct;
    QFont boldFontStruct;
    QFont italicFontStruct;
    QFont boldItalicFontStruct;

private:
    bool settingsLoaded_ = false;
};

#endif
