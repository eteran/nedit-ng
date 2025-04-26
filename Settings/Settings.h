
#ifndef SETTINGS_H_
#define SETTINGS_H_

#include "IndentStyle.h"
#include "SearchType.h"
#include "ShowMatchingStyle.h"
#include "TruncSubstitution.h"
#include "Util/QtHelper.h"
#include "WrapStyle.h"

#include <QCoreApplication>
#include <QFont>
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
	LINENO_BG_COLOR,
};

namespace Settings {
Q_DECLARE_NAMESPACE_TR(Settings)

void Load(bool isServer);
bool Save();
void Import(const QString &filename);

// Paths
QString ConfigFile();
QString HistoryFile();
QString AutoLoadMacroFile();
QString StyleFile();
QString ThemeFile();
QString LanguageModeFile();
QString HighlightPatternsFile();
QString MactoMenuFile();
QString ShellMenuFile();
QString ContextMenuFile();
QString SmartIndentFile();

// Standard
extern bool showResizeNotification;
extern bool appendLF;
extern bool autoSave;
extern bool autoScroll;
extern bool backlightChars;
extern bool beepOnSearchWrap;
extern bool globalTabNavigate;
extern bool highlightSyntax;
extern bool insertTabs;
extern bool iSearchLine;
extern bool lineNumbers;
extern bool matchSyntaxBased;
extern bool openInTab;
extern bool pathInWindowsMenu;
extern bool prefFileRead;
extern bool repositionDialogs;
extern bool retainSearchDialogs;
extern bool saveOldVersion;
extern bool searchDialogs;
extern bool searchWraps;
extern bool smartTags;
extern bool sortOpenPrevMenu;
extern bool sortTabs;
extern bool statisticsLine;
extern bool tabBar;
extern bool tabBarHideOne;
extern bool toolTips;
extern bool warnExit;
extern bool warnFileMods;
extern bool warnRealFileMods;
extern bool smartHome;
extern IndentStyle autoIndent;
extern WrapStyle autoWrap;
extern int emulateTabs;
extern SearchType searchMethod;
extern ShowMatchingStyle showMatching;
extern int tabDistance;
extern int textCols;
extern int textRows;
extern int wrapMargin;
extern QString bgMenuCommands;
extern QString colors[9];
extern QString geometry;
extern QString highlightPatterns;
extern QString languageModes;
extern QString macroCommands;
extern QString serverName;
extern QString serverNameOverride;
extern QString shell;
extern QString shellCommands;
extern QString smartIndentInit;
extern QString smartIndentInitCommon;
extern QString fontName;
extern QString titleFormat;

// Advanced
extern bool autoWrapPastedText;
extern bool colorizeHighlightedText;
extern bool heavyCursor;
extern bool alwaysCheckRelativeTagsSpecs;
extern bool findReplaceUsesSelection;
extern bool focusOnRaise;
extern bool forceOSConversion;
extern bool honorSymlinks;
extern bool stickyCaseSenseButton;
extern bool typingHidesPointer;
extern bool undoModifiesSelection;
extern bool splitHorizontally;
extern int truncateLongNamesInTabs;
extern int autoScrollVPadding;
extern int maxPrevOpenFiles;
extern int autoSaveCharLimit;
extern int autoSaveOpLimit;
extern TruncSubstitution truncSubstitution;
extern QString backlightCharTypes;
extern QString tagFile;
extern QString wordDelimiters;
extern QStringList includePaths;

// Created implicitly from other "real" settings
extern QFont font;
}

#endif
