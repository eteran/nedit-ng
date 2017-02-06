
#ifndef SETTINGS_H_
#define SETTINGS_H_

#include <QString>
#include <QObject>
#include <QFont>

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
    QString fileVersion;
    QString shellCommands;
    QString macroCommands;
    QString bgMenuCommands;
    QString highlightPatterns;
    QString languageModes;
    QString styles;
    QString smartIndentInit;
    QString smartIndentInitCommon;
    int autoWrap;
    int wrapMargin;
    int autoIndent;
    bool autoSave;
    bool openInTab;
    bool saveOldVersion;
    int showMatching;
    bool matchSyntaxBased;
    bool highlightSyntax;
    bool backlightChars;
    QString backlightCharTypes;
    bool searchDialogs;
    bool beepOnSearchWrap;
    bool retainSearchDialogs;
    bool searchWraps;
    bool stickyCaseSenseButton;
    bool repositionDialogs;
    bool autoScroll;
    int autoScrollVPadding;
    bool appendLF;
    bool sortOpenPrevMenu;
    bool statisticsLine;
    bool iSearchLine;
    bool sortTabs;
    bool tabBar;
    bool tabBarHideOne;
    bool toolTips;
    bool globalTabNavigate;
    bool lineNumbers;
    bool pathInWindowsMenu;
    bool warnFileMods;
    bool warnRealFileMods;
    bool warnExit;
    int searchMethod;
#ifdef REPLACE_SCOPE
    int replaceDefaultScope;
#endif
    int textRows;
    int textCols;
    int tabDistance;
    int emulateTabs;
    bool insertTabs;
    QString textFont;
    QString boldHighlightFont;
    QString italicHighlightFont;
    QString boldItalicHighlightFont;
    QString helpFonts[12];
    QString helpLinkColor;
    QString colors[8];
    QString tooltipBgColor;
    QString shell;
    QString geometry;
    bool remapDeleteKey;
    bool stdOpenDialog;
    QString tagFile;
    QString wordDelimiters;
    QString serverName;
    int maxPrevOpenFiles;
    QString bgMenuButton;
    bool smartTags;
    bool typingHidesPointer;
    bool alwaysCheckRelativeTagsSpecs;
    bool prefFileRead;
    bool findReplaceUsesSelection;
    int overrideDefaultVirtualKeyBindings;
    QString titleFormat;
    bool undoModifiesSelection;
    bool focusOnRaise;
    bool forceOSConversion;
    int truncSubstitution;
    bool honorSymlinks;

public:
	// created implicitly from other "real" settings
    QFont boldFontStruct;
    QFont italicFontStruct;
    QFont boldItalicFontStruct;
};

#endif
