
#ifndef SETTINGS_H_
#define SETTINGS_H_

#include <QFont>
#include <QObject>
#include <QString>

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
    int autoIndent;
    int autoWrap;
    int emulateTabs;
    int searchMethod;
    int showMatching;
    int tabDistance;
    int textCols;
    int textRows;
    int wrapMargin;
    QString bgMenuCommands;
    QString boldHighlightFont;
    QString boldItalicHighlightFont;
    QString colors[8];
    QString fileVersion;
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
    QString styles;
    QString textFont;
    QString titleFormat;
    QString tooltipBgColor;
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
    int overrideDefaultVirtualKeyBindings;
    int truncSubstitution;
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
