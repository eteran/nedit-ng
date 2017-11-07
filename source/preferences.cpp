/*******************************************************************************
*                                                                              *
* preferences.c -- Nirvana Editor preferences processing                       *
*                                                                              *
* Copyright (C) 1999 Mark Edel                                                 *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute version of this program linked to   *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License        *
* for more details.                                                            *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* April 20, 1993                                                               *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/

#include "preferences.h"
#include "version.h"
#include "DocumentWidget.h"
#include "LanguageMode.h"
#include "MainWindow.h"
#include "TextBuffer.h"
#include "highlight.h"
#include "highlightData.h"
#include "search.h"
#include "smartIndent.h"
#include "Font.h"
#include "tags.h"
#include "userCmds.h"
#include "Input.h"
#include "util/ClearCase.h"

#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QRegExp>
#include <QStandardPaths>
#include <QString>
#include <QtDebug>
#include <cctype>
#include <memory>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

constexpr int ConfigFileVersion = 1;

#define N_WRAP_STYLES 3
static const char *AutoWrapTypes[N_WRAP_STYLES + 3] = {"None", "Newline", "Continuous", "True", "False", nullptr};

#define N_INDENT_STYLES 3
static const char *AutoIndentTypes[N_INDENT_STYLES + 3] = {"None", "Auto", "Smart", "True", "False", nullptr};


/* suplement wrap and indent styles w/ a value meaning "use default" for
   the override fields in the language modes dialog */
constexpr int DEFAULT_TAB_DIST     = -1;
constexpr int DEFAULT_EM_TAB_DIST  = -1;

// list of available language modes and language specific preferences 
QList<LanguageMode> LanguageModes;

/* Module-global variable set when any preference changes (for asking the
   user about re-saving on exit) */
bool PrefsHaveChanged = false;

/* Module-global variable set when user uses -import to load additional
   preferences on top of the defaults.  Contains name of file loaded */
QString ImportedFile;

static void translatePrefFormats(quint32 fileVer);
static QStringList readExtensionListEx(Input &in);
static QString getDefaultShell();
static int loadLanguageModesStringEx(const QString &string);
static bool modeErrorEx(const Input &in, const QString &message);
static QString WriteLanguageModesStringEx();

static Settings g_Settings;

Settings &GetSettings() {
    return g_Settings;
}

void RestoreNEditPrefs() {

    g_Settings.loadPreferences();

    /* Do further parsing on resource types which RestorePreferences does
       not understand and reads as strings, to put them in the final form
       in which nedit stores and uses.  If the preferences file was
       written by an older version of NEdit, update regular expressions in
       highlight patterns to quote braces and use & instead of \0 */
    translatePrefFormats(NEDIT_VERSION);
}

/*
** Many of of NEdit's preferences are much more complicated than just simple
** integers or strings.  These are read as strings, but must be parsed and
** translated into something meaningful.  This routine does the translation,
** and, in most cases, frees the original string, which is no longer useful.
**
** In addition this function covers settings that, while simple, require
** additional steps before they can be published.
**
** The argument convertOld attempts a conversion from pre 5.1 format .nedit
** files (which means patterns and macros may contain regular expressions
** which are of the older syntax where braces were not quoted, and \0 was a
** legal substitution character).  Macros, so far can not be automatically
** converted, unfortunately.
*/
static void translatePrefFormats(quint32 fileVer) {

    Q_UNUSED(fileVer);

	/* Parse the strings which represent types which are not decoded by
	   the standard resource manager routines */

    if (!g_Settings.shellCommands.isNull()) {
        LoadShellCmdsStringEx(g_Settings.shellCommands);
	}
    if (!g_Settings.macroCommands.isNull()) {
        LoadMacroCmdsStringEx(g_Settings.macroCommands);
	}
    if (!g_Settings.bgMenuCommands.isNull()) {
        LoadBGMenuCmdsStringEx(g_Settings.bgMenuCommands);
	}
    if (!g_Settings.highlightPatterns.isNull()) {
        LoadHighlightStringEx(g_Settings.highlightPatterns);
	}
    if (!g_Settings.styles.isNull()) {
        LoadStylesStringEx(g_Settings.styles);
	}
    if (!g_Settings.languageModes.isNull()) {
        loadLanguageModesStringEx(g_Settings.languageModes);
	}
    if (!g_Settings.smartIndentInit.isNull()) {
        LoadSmartIndentStringEx(g_Settings.smartIndentInit);
	}
    if (!g_Settings.smartIndentInitCommon.isNull()) {
        LoadSmartIndentCommonStringEx(g_Settings.smartIndentInitCommon);
	}

	// translate the font names into fontLists suitable for the text widget 
    g_Settings.plainFontStruct      = Font::fromString(g_Settings.textFont);
    g_Settings.boldFontStruct       = Font::fromString(g_Settings.boldHighlightFont);
    g_Settings.italicFontStruct     = Font::fromString(g_Settings.italicHighlightFont);
    g_Settings.boldItalicFontStruct = Font::fromString(g_Settings.boldItalicHighlightFont);

	/*
	**  The default set for the comand shell in PrefDescrip ("DEFAULT") is
	**  only a place-holder, the actual default is the user's login shell
	**  (or whatever is implemented in getDefaultShell()). We put the login
	**  shell's name in PrefData here.
	*/
    if (g_Settings.shell == QLatin1String("DEFAULT")) {
        g_Settings.shell = getDefaultShell();
	}

	/* setup language mode dependent info of user menus (to increase
	   performance when switching between documents of different
	   language modes) */
    SetupUserMenuInfo();
}

void SaveNEditPrefsEx(QWidget *parent, bool quietly) {

    QString prefFileName = Settings::configFile();
    if(prefFileName.isNull()) {
        QMessageBox::warning(parent, QLatin1String("Error saving Preferences"), QLatin1String("Unable to save preferences: Cannot determine filename."));
        return;
    }

    if (!quietly) {
        int resp = QMessageBox::information(parent, QLatin1String("Save Preferences"),
            ImportedFile.isNull() ? QString(QLatin1String("Default preferences will be saved in the file:\n%1\nNEdit automatically loads this file\neach time it is started.")).arg(prefFileName)
                                  : QString(QLatin1String("Default preferences will be saved in the file:\n%1\nSAVING WILL INCORPORATE SETTINGS\nFROM FILE: %2")).arg(prefFileName, ImportedFile),
                QMessageBox::Ok | QMessageBox::Cancel);



        if(resp == QMessageBox::Cancel) {
            return;
        }
    }

    g_Settings.fileVersion           = ConfigFileVersion;
    g_Settings.shellCommands         = WriteShellCmdsStringEx();
    g_Settings.macroCommands         = WriteMacroCmdsStringEx();
    g_Settings.bgMenuCommands        = WriteBGMenuCmdsStringEx();
    g_Settings.highlightPatterns     = WriteHighlightStringEx();
    g_Settings.languageModes         = WriteLanguageModesStringEx();
    g_Settings.styles                = WriteStylesStringEx();
    g_Settings.smartIndentInit       = WriteSmartIndentStringEx();
    g_Settings.smartIndentInitCommon = WriteSmartIndentCommonStringEx();

    if (!g_Settings.savePreferences()) {
        QMessageBox::warning(parent, QLatin1String("Save Preferences"), QString(QLatin1String("Unable to save preferences in %1")).arg(prefFileName));
    }

    PrefsHaveChanged = false;
}

/*
** Load an additional preferences file on top of the existing preferences
** derived from defaults, the .nedit file, and X resources.
*/
void ImportPrefFile(const QString &filename, bool convertOld) {
    Q_UNUSED(convertOld);

    g_Settings.importSettings(filename);
}

void SetPrefOpenInTab(bool state) {
    if(g_Settings.openInTab != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.openInTab = state;
}

bool GetPrefOpenInTab() {
    return g_Settings.openInTab;
}

void SetPrefWrap(WrapStyle state) {
    if(g_Settings.autoWrap != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.autoWrap = state;
}

WrapStyle GetPrefWrap(int langMode) {
    if (langMode == PLAIN_LANGUAGE_MODE || LanguageModes[langMode].wrapStyle == DEFAULT_WRAP) {
        return g_Settings.autoWrap;
    }

    return LanguageModes[langMode].wrapStyle;
}

void SetPrefWrapMargin(int margin) {
    if(g_Settings.wrapMargin != margin) {
        PrefsHaveChanged = true;
    }
    g_Settings.wrapMargin = margin;
}

int GetPrefWrapMargin() {
    return g_Settings.wrapMargin;
}

void SetPrefSearch(SearchType searchType) {
    if(g_Settings.searchMethod != searchType) {
        PrefsHaveChanged = true;
    }
    g_Settings.searchMethod = searchType;
}

SearchType GetPrefSearch() {
    return static_cast<SearchType>(g_Settings.searchMethod);
}

#if defined(REPLACE_SCOPE)
void SetPrefReplaceDefScope(int scope) {
    if(g_Settings.replaceDefaultScope != scope) {
        PrefsHaveChanged = true;
    }
    g_Settings.replaceDefaultScope = scope;
}

int GetPrefReplaceDefScope() {
    return g_Settings.replaceDefaultScope;
}
#endif

void SetPrefAutoIndent(IndentStyle state) {
    if(g_Settings.autoIndent != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.autoIndent = state;
}

IndentStyle GetPrefAutoIndent(int langMode) {
    if (langMode == PLAIN_LANGUAGE_MODE || LanguageModes[langMode].indentStyle == IndentStyle::Default) {
        return static_cast<IndentStyle>(g_Settings.autoIndent);
    }

    return LanguageModes[langMode].indentStyle;
}

void SetPrefAutoSave(bool state) {
    if(g_Settings.autoSave != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.autoSave = state;
}

bool GetPrefAutoSave() {
    return g_Settings.autoSave;
}

void SetPrefSaveOldVersion(bool state) {
    if(g_Settings.saveOldVersion != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.saveOldVersion = state;
}

bool GetPrefSaveOldVersion() {
    return g_Settings.saveOldVersion;
}

void SetPrefSearchDlogs(bool state) {
    if(g_Settings.searchDialogs != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.searchDialogs = state;
}

bool GetPrefSearchDlogs() {
    return g_Settings.searchDialogs;
}

void SetPrefBeepOnSearchWrap(bool state) {
    if(g_Settings.beepOnSearchWrap != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.beepOnSearchWrap = state;
}

bool GetPrefBeepOnSearchWrap() {
    return g_Settings.beepOnSearchWrap;
}

void SetPrefKeepSearchDlogs(bool state) {
    if(g_Settings.retainSearchDialogs != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.retainSearchDialogs = state;
}

bool GetPrefKeepSearchDlogs() {
    return g_Settings.retainSearchDialogs;
}

void SetPrefSearchWraps(bool state) {
    if(g_Settings.searchWraps != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.searchWraps = state;
}

WrapMode GetPrefSearchWraps() {
    return g_Settings.searchWraps ? WrapMode::Wrap : WrapMode::NoWrap;
}

int GetPrefStickyCaseSenseBtn() {
    return g_Settings.stickyCaseSenseButton;
}

void SetPrefStatsLine(bool state) {
    if(g_Settings.statisticsLine != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.statisticsLine = state;
}

int GetPrefStatsLine() {
    return g_Settings.statisticsLine;
}

void SetPrefISearchLine(bool state) {
    if(g_Settings.iSearchLine != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.iSearchLine = state;
}

int GetPrefISearchLine() {
    return g_Settings.iSearchLine;
}

void SetPrefSortTabs(bool state) {
    if(g_Settings.sortTabs != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.sortTabs = state;
}

int GetPrefSortTabs() {
    return g_Settings.sortTabs;
}

void SetPrefTabBar(bool state) {
    if(g_Settings.tabBar != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.tabBar = state;
}

int GetPrefTabBar() {
    return g_Settings.tabBar;
}

void SetPrefTabBarHideOne(bool state) {
    if(g_Settings.tabBarHideOne != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.tabBarHideOne = state;
}

int GetPrefTabBarHideOne() {
    return g_Settings.tabBarHideOne;
}

void SetPrefGlobalTabNavigate(bool state) {
    if(g_Settings.globalTabNavigate != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.globalTabNavigate = state;
}

int GetPrefGlobalTabNavigate() {
    return g_Settings.globalTabNavigate;
}

void SetPrefToolTips(bool state) {
    if(g_Settings.toolTips != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.toolTips = state;
}

int GetPrefToolTips() {
    return g_Settings.toolTips;
}

void SetPrefLineNums(bool state) {
    if(g_Settings.lineNumbers != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.lineNumbers = state;
}

int GetPrefLineNums() {
    return g_Settings.lineNumbers;
}

void SetPrefShowPathInWindowsMenu(bool state) {
    if(g_Settings.pathInWindowsMenu != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.pathInWindowsMenu = state;
}

int GetPrefShowPathInWindowsMenu() {
    return g_Settings.pathInWindowsMenu;
}

void SetPrefWarnFileMods(bool state) {
    if(g_Settings.warnFileMods != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.warnFileMods = state;
}

int GetPrefWarnFileMods() {
    return g_Settings.warnFileMods;
}

void SetPrefWarnRealFileMods(bool state) {
    if(g_Settings.warnRealFileMods != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.warnRealFileMods = state;
}

int GetPrefWarnRealFileMods() {
    return g_Settings.warnRealFileMods;
}

void SetPrefWarnExit(bool state) {
    if(g_Settings.warnExit != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.warnExit = state;
}

int GetPrefWarnExit() {
    return g_Settings.warnExit;
}

void SetPrefFindReplaceUsesSelection(bool state) {
    if(g_Settings.findReplaceUsesSelection != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.findReplaceUsesSelection = state;
}

int GetPrefFindReplaceUsesSelection() {
    return g_Settings.findReplaceUsesSelection;
}

// Advanced
int GetPrefMapDelete() {
    return g_Settings.remapDeleteKey;
}

// Advanced
int GetPrefStdOpenDialog() {
    return g_Settings.stdOpenDialog;
}

void SetPrefRows(int nRows) {
    if(g_Settings.textRows != nRows) {
        PrefsHaveChanged = true;
    }
    g_Settings.textRows = nRows;
}

int GetPrefRows() {
    return g_Settings.textRows;
}

void SetPrefCols(int nCols) {
    if(g_Settings.textCols != nCols) {
        PrefsHaveChanged = true;
    }
    g_Settings.textCols = nCols;
}

int GetPrefCols() {
    return g_Settings.textCols;
}

void SetPrefTabDist(int tabDist) {
    if(g_Settings.tabDistance != tabDist) {
        PrefsHaveChanged = true;
    }
    g_Settings.tabDistance = tabDist;
}

int GetPrefTabDist(int langMode) {
	int tabDist;

    if (langMode == PLAIN_LANGUAGE_MODE || LanguageModes[langMode].tabDist == DEFAULT_TAB_DIST) {
        tabDist = g_Settings.tabDistance;
	} else {
        tabDist = LanguageModes[langMode].tabDist;
	}

	/* Make sure that the tab distance is in range (garbage may have
	   been entered via the command line or the X resources, causing
	   errors later on, like division by zero). */
    if (tabDist <= 0) {
		return 1;
    }

    if (tabDist > MAX_EXP_CHAR_LEN) {
		return MAX_EXP_CHAR_LEN;
    }

	return tabDist;
}

void SetPrefEmTabDist(int tabDist) {
    if(g_Settings.emulateTabs != tabDist) {
        PrefsHaveChanged = true;
    }
    g_Settings.emulateTabs = tabDist;
}

int GetPrefEmTabDist(int langMode) {
    if (langMode == PLAIN_LANGUAGE_MODE || LanguageModes[langMode].emTabDist == DEFAULT_EM_TAB_DIST) {
        return g_Settings.emulateTabs;
    }

    return LanguageModes[langMode].emTabDist;
}

void SetPrefInsertTabs(bool state) {
    if(g_Settings.insertTabs != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.insertTabs = state;
}

int GetPrefInsertTabs() {
    return g_Settings.insertTabs;
}

void SetPrefShowMatching(ShowMatchingStyle state) {
    if(g_Settings.showMatching != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.showMatching = state;
}

ShowMatchingStyle GetPrefShowMatching() {
    return g_Settings.showMatching;
}

void SetPrefMatchSyntaxBased(bool state) {
    if(g_Settings.matchSyntaxBased != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.matchSyntaxBased = state;
}

int GetPrefMatchSyntaxBased() {
    return g_Settings.matchSyntaxBased;
}

void SetPrefHighlightSyntax(bool state) {
    if(g_Settings.highlightSyntax != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.highlightSyntax = state;
}

bool GetPrefHighlightSyntax() {
    return g_Settings.highlightSyntax;
}

void SetPrefBacklightChars(bool state) {
    if(g_Settings.backlightChars != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.backlightChars = state;
}

int GetPrefBacklightChars() {
    return g_Settings.backlightChars;
}

QString GetPrefBacklightCharTypes() {
    return g_Settings.backlightCharTypes;
}

void SetPrefRepositionDialogs(bool state) {
    if(g_Settings.repositionDialogs != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.repositionDialogs = state;
}

bool GetPrefRepositionDialogs() {
    return g_Settings.repositionDialogs;
}

void SetPrefAutoScroll(bool state) {
    if(g_Settings.autoScroll != state) {
        PrefsHaveChanged = true;
    }

    const int margin = state ? g_Settings.autoScrollVPadding : 0;

    g_Settings.autoScroll = state;
	
    for(DocumentWidget *document : DocumentWidget::allDocuments()) {
        document->SetAutoScroll(margin);
	}
}

int GetPrefAutoScroll() {
    return g_Settings.autoScroll;
}

int GetVerticalAutoScroll() {
    return g_Settings.autoScroll ? g_Settings.autoScrollVPadding : 0;
}

void SetPrefAppendLF(bool state) {
    if(g_Settings.appendLF != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.appendLF = state;
}

int GetPrefAppendLF() {
    return g_Settings.appendLF;
}

void SetPrefSortOpenPrevMenu(bool state) {
    if(g_Settings.sortOpenPrevMenu != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.sortOpenPrevMenu = state;
}

int GetPrefSortOpenPrevMenu() {
    return g_Settings.sortOpenPrevMenu;
}

QString GetPrefTagFile() {
    return g_Settings.tagFile;
}

void SetPrefSmartTags(bool state) {
    if(g_Settings.smartTags != state) {
        PrefsHaveChanged = true;
    }
    g_Settings.smartTags = state;
}

int GetPrefSmartTags() {
    return g_Settings.smartTags;
}

// Advanced
int GetPrefAlwaysCheckRelTagsSpecs() {
    return g_Settings.alwaysCheckRelativeTagsSpecs;
}

QString GetPrefDelimiters() {
    return g_Settings.wordDelimiters;
}

QString GetPrefColorName(ColorTypes index) {
    return g_Settings.colors[index];
}

void SetPrefColorName(ColorTypes index, const QString &name) {
	if(g_Settings.colors[index] != name) {
        PrefsHaveChanged = true;
    }
	g_Settings.colors[index] = name;
}

void SetPrefFont(const QString &fontName) {
    if(g_Settings.textFont != fontName) {
        PrefsHaveChanged = true;
    }
    g_Settings.textFont        = fontName;
    g_Settings.plainFontStruct = Font::fromString(fontName);
}

void SetPrefBoldFont(const QString &fontName) {
    if(g_Settings.boldHighlightFont != fontName) {
        PrefsHaveChanged = true;
    }
    g_Settings.boldHighlightFont = fontName;
    g_Settings.boldFontStruct    = Font::fromString(fontName);
}

void SetPrefItalicFont(const QString &fontName) {
    if(g_Settings.italicHighlightFont != fontName) {
        PrefsHaveChanged = true;
    }
    g_Settings.italicHighlightFont = fontName;
    g_Settings.italicFontStruct    = Font::fromString(fontName);
}

void SetPrefBoldItalicFont(const QString &fontName) {
    if(g_Settings.boldItalicHighlightFont != fontName) {
        PrefsHaveChanged = true;
    }
    g_Settings.boldItalicHighlightFont = fontName;
    g_Settings.boldItalicFontStruct    = Font::fromString(fontName);
}

QString GetPrefFontName() {
    return g_Settings.textFont;
}

QString GetPrefBoldFontName() {
    return g_Settings.boldHighlightFont;
}

QString GetPrefItalicFontName() {
    return g_Settings.italicHighlightFont;
}

QString GetPrefBoldItalicFontName() {
    return g_Settings.boldItalicHighlightFont;
}

QFont GetPrefDefaultFont() {
    return g_Settings.plainFontStruct;
}

QFont GetPrefBoldFont() {
    return g_Settings.boldFontStruct;
}

QFont GetPrefItalicFont() {
    return g_Settings.italicFontStruct;
}

QFont GetPrefBoldItalicFont() {
    return g_Settings.boldItalicFontStruct;
}

QString GetPrefTooltipBgColor() {
    return g_Settings.tooltipBgColor;
}

void SetPrefShell(const QString &shell) {
    if(g_Settings.shell != shell) {
        PrefsHaveChanged = true;
    }
    g_Settings.shell = shell;
}

QString GetPrefShell() {
    return g_Settings.shell;
}

QString GetPrefGeometry() {
    return g_Settings.geometry;
}

QString GetPrefServerName() {
    return g_Settings.serverName;
}

int GetPrefMaxPrevOpenFiles() {
    return g_Settings.maxPrevOpenFiles;
}

int GetPrefTypingHidesPointer() {
    return g_Settings.typingHidesPointer;
}

void SetPrefTitleFormat(const QString &format) {
    if(g_Settings.titleFormat != format) {
        PrefsHaveChanged = true;
    }

    g_Settings.titleFormat = format;

    // update all windows
    for(MainWindow *window : MainWindow::allWindows()) {
        window->UpdateWindowTitle(window->currentDocument());
	}
}
QString GetPrefTitleFormat() {
    return g_Settings.titleFormat;
}

bool GetPrefUndoModifiesSelection() {
    return g_Settings.undoModifiesSelection;
}

bool GetPrefFocusOnRaise() {
    return g_Settings.focusOnRaise;
}

bool GetPrefForceOSConversion() {
    return g_Settings.forceOSConversion;
}

bool GetPrefHonorSymlinks() {
    return g_Settings.honorSymlinks;
}

VirtKeyOverride GetPrefOverrideVirtKeyBindings() {
    return g_Settings.overrideDefaultVirtualKeyBindings;
}

TruncSubstitution GetPrefTruncSubstitution() {
    return g_Settings.truncSubstitution;
}

/*
** If preferences don't get saved, ask the user on exit whether to save
*/
void MarkPrefsChanged() {
	PrefsHaveChanged = true;
}

/*
** Lookup a language mode by name, returning the index of the language
** mode or PLAIN_LANGUAGE_MODE if the name is not found
*/
int FindLanguageMode(const QString &languageName) {

    // Compare each language mode to the one we were presented
    for (int i = 0; i < LanguageModes.size(); i++) {
        if (LanguageModes[i].name == languageName) {
            return i;
        }
    }

    return PLAIN_LANGUAGE_MODE;
}

int FindLanguageMode(const QStringRef &languageName) {

    // Compare each language mode to the one we were presented
    for (int i = 0; i < LanguageModes.size(); i++) {
        if (LanguageModes[i].name == languageName) {
            return i;
        }
    }

    return PLAIN_LANGUAGE_MODE;
}

/*
** Return the name of the current language mode set in "window", or nullptr
** if the current mode is "Plain".
*/
QString LanguageModeName(int mode) {
	if (mode == PLAIN_LANGUAGE_MODE)
		return QString();
	else
        return LanguageModes[mode].name;
}

/*
** Get the set of word delimiters for the language mode set in the current
** window.  Returns nullptr when no language mode is set (it would be easy to
** return the default delimiter set when the current language mode is "Plain",
** or the mode doesn't have its own delimiters, but this is usually used
** to supply delimiters for RE searching, and ExecRE can skip compiling a
** delimiter table when delimiters is nullptr).
*/
QString GetWindowDelimitersEx(const DocumentWidget *window) {
    if (window->languageMode_ == PLAIN_LANGUAGE_MODE)
        return QString();
    else
        return LanguageModes[window->languageMode_].delimiters;
}


static int loadLanguageModesStringEx(const QString &string) {
	QString errMsg;
	int i;

	Input in(&string);

	Q_FOREVER {

		// skip over blank space
		in.skipWhitespaceNL();

		LanguageMode lm;

		// read language mode name
		QString name = ReadSymbolicFieldEx(in);
		if (name.isNull()) {
			return modeErrorEx(in, QLatin1String("language mode name required"));
		}

		lm.name = name;

		if (!SkipDelimiterEx(in, &errMsg))
			return modeErrorEx(in, errMsg);

		// read list of extensions
		lm.extensions = readExtensionListEx(in);
		if (!SkipDelimiterEx(in, &errMsg))
			return modeErrorEx(in, errMsg);

		// read the recognition regular expression
		QString recognitionExpr;
		if (in.atEnd() || *in == QLatin1Char('\n') || *in == QLatin1Char(':')) {
			recognitionExpr = QString();
		} else if (!ReadQuotedStringEx(in, &errMsg, &recognitionExpr)) {
			return modeErrorEx(in, errMsg);
		}

		lm.recognitionExpr = recognitionExpr;

		if (!SkipDelimiterEx(in, &errMsg)) {
			return modeErrorEx(in, errMsg);
		}

		// read the indent style
		QString styleName = ReadSymbolicFieldEx(in);
		if(styleName.isNull()) {
            lm.indentStyle = IndentStyle::Default;
		} else {                     
			for (i = 0; i < N_INDENT_STYLES; i++) {
				if (styleName == QString::fromLatin1(AutoIndentTypes[i])) {
					lm.indentStyle = static_cast<IndentStyle>(i);
					break;
				}
			}

            if (i == N_INDENT_STYLES) {
				return modeErrorEx(in, QLatin1String("unrecognized indent style"));
            }
		}

        if (!SkipDelimiterEx(in, &errMsg)) {
			return modeErrorEx(in, errMsg);
        }

		// read the wrap style
		styleName = ReadSymbolicFieldEx(in);
		if(styleName.isNull()) {
			lm.wrapStyle = DEFAULT_WRAP;
		} else {
			for (i = 0; i < N_WRAP_STYLES; i++) {
				if ((styleName == QString::fromLatin1(AutoWrapTypes[i]))) {
                    lm.wrapStyle = static_cast<WrapStyle>(i);
					break;
				}
			}

            if (i == N_WRAP_STYLES) {
				return modeErrorEx(in, QLatin1String("unrecognized wrap style"));
            }
		}

        if (!SkipDelimiterEx(in, &errMsg)) {
			return modeErrorEx(in, errMsg);
        }

		// read the tab distance
		if (in.atEnd() || *in == QLatin1Char('\n') || *in == QLatin1Char(':')) {
			lm.tabDist = DEFAULT_TAB_DIST;
		} else if (!ReadNumericFieldEx(in, &lm.tabDist)) {
			return modeErrorEx(in, QLatin1String("bad tab spacing"));
		}

		if (!SkipDelimiterEx(in, &errMsg)) {
			return modeErrorEx(in, errMsg);
		}

		// read emulated tab distance
		if (in.atEnd() || *in == QLatin1Char('\n') || *in == QLatin1Char(':')) {
			lm.emTabDist = DEFAULT_EM_TAB_DIST;
		}
		else if (!ReadNumericFieldEx(in, &lm.emTabDist))
			return modeErrorEx(in, QLatin1String("bad emulated tab spacing"));

		if (!SkipDelimiterEx(in, &errMsg))
			return modeErrorEx(in, errMsg);

		// read the delimiters string
		QString delimiters;
		if (in.atEnd() || *in == QLatin1Char('\n') || *in == QLatin1Char(':')) {
			delimiters = QString();
		} else if (!ReadQuotedStringEx(in, &errMsg, &delimiters)) {
			return modeErrorEx(in, errMsg);
		}

		lm.delimiters = delimiters;

		// After 5.3 all language modes need a default tips file field
		if (!SkipDelimiterEx(in, &errMsg))
			return modeErrorEx(in, errMsg);

		// read the default tips file

		QString defTipsFile;
		if (in.atEnd() || *in == QLatin1Char('\n')) {
			defTipsFile = QString();
		} else if (!ReadQuotedStringEx(in, &errMsg, &defTipsFile)) {
			return modeErrorEx(in, errMsg);
		}

		lm.defTipsFile = defTipsFile;

		// pattern set was read correctly, add/replace it in the list
		for (i = 0; i < LanguageModes.size(); i++) {
			if (LanguageModes[i].name == lm.name) {
				LanguageModes[i] = lm;
				break;
			}
		}

		if (i == LanguageModes.size()) {
			LanguageModes.push_back(lm);
		}

		// if the string ends here, we're done
		in.skipWhitespaceNL();

		if (in.atEnd()) {
			return true;
		}
	}
}

static QString WriteLanguageModesStringEx() {

    QString str;
    QTextStream out(&str);

    for(const LanguageMode &language : LanguageModes) {

        out << QLatin1Char('\t')
            << language.name
            << QLatin1Char(':');

        QString exts = language.extensions.join(QLatin1String(" "));
        out << exts
            << QLatin1Char(':');

        if (!language.recognitionExpr.isEmpty()) {
            out << MakeQuotedStringEx(language.recognitionExpr);
        }

        out << QLatin1Char(':');
        if (language.indentStyle != IndentStyle::Default) {
            out << QString::fromLatin1(AutoIndentTypes[static_cast<int>(language.indentStyle)]);
        }

        out << QLatin1Char(':');
        if (language.wrapStyle != DEFAULT_WRAP) {
            out << QString::fromLatin1(AutoWrapTypes[language.wrapStyle]);
        }

        out << QLatin1Char(':');
        if (language.tabDist != DEFAULT_TAB_DIST) {
            out << language.tabDist;
        }

        out << QLatin1Char(':');
        if (language.emTabDist != DEFAULT_EM_TAB_DIST) {
            out << language.emTabDist;
        }

        out << QLatin1Char(':');
        if (!language.delimiters.isEmpty()) {
            out << MakeQuotedStringEx(language.delimiters);
        }

        out << QLatin1Char(':');
        if (!language.defTipsFile.isEmpty()) {
            out << MakeQuotedStringEx(language.defTipsFile);
        }

        out << QLatin1Char('\n');
	}

    if(!str.isEmpty()) {
        str.chop(1);
    }

    return str;
}

static QStringList readExtensionListEx(Input &in) {
	QStringList extensionList;

	// skip over blank space
	in.skipWhitespace();

	while(!in.atEnd() && *in != QLatin1Char(':')) {

		in.skipWhitespace();

		Input strStart = in;

		while (!in.atEnd() && *in != QLatin1Char(' ') && *in != QLatin1Char('\t') && *in != QLatin1Char(':')) {
			++in;
		}

		int len = in - strStart;
		QString ext = strStart.mid(len);
		extensionList.push_back(ext);
	}

	return extensionList;
}

bool ReadNumericFieldEx(Input &in, int *value) {

	// skip over blank space
	in.skipWhitespace();

	QRegExp regex(QLatin1String("^(0|[-+]?[1-9][0-9]*)"));

	int n = regex.indexIn(*in.string(), in.index(), QRegExp::CaretAtOffset);
	if(n == in.index()) {
		bool ok;
		*value = regex.cap(1).toInt(&ok);
		if(ok) {
			in += regex.matchedLength();
			return true;
		}
	}

	return false;
}

/*
** Parse a symbolic field, skipping initial and trailing whitespace,
** stops on first invalid character or end of string.  Valid characters
** are letters, numbers, _, -, +, $, #, and internal whitespace.  Internal
** whitespace is compressed to single space characters.
*/
QString ReadSymbolicFieldEx(Input &input) {

	// skip over initial blank space
	input.skipWhitespace();

	/* Find the first invalid character or end of string to know how
	   much memory to allocate for the returned string */
	Input strStart = input;
	while ((*input).isLetterOrNumber() || *input == QLatin1Char('_') || *input == QLatin1Char('-') || *input == QLatin1Char('+') || *input == QLatin1Char('$') || *input == QLatin1Char('#') || *input == QLatin1Char(' ') || *input == QLatin1Char('\t')) {
		++input;
	}

	const int len = input - strStart;
	if (len == 0) {
		return QString();
	}

	QString outStr;
	outStr.reserve(len);

	auto outPtr = std::back_inserter(outStr);

	// Copy the string, compressing internal whitespace to a single space
	Input strPtr = strStart;
	while (strPtr - strStart < len) {
		if (*strPtr == QLatin1Char(' ') || *strPtr == QLatin1Char('\t')) {
			strPtr.skipWhitespace();
			*outPtr++ = QLatin1Char(' ');
		} else {
			*outPtr++ = *strPtr++;
		}
	}

	// If there's space on the end, take it back off
	if(outStr.endsWith(QLatin1Char(' '))) {
		outStr.chop(1);
	}

	return outStr;
}

/*
** parse an individual quoted string.  Anything between
** double quotes is acceptable, quote characters can be escaped by "".
** Returns allocated string "string" containing
** argument minus quotes.  If not successful, returns False with
** (statically allocated) message in "errMsg".
*/
bool ReadQuotedStringEx(Input &in, QString *errMsg, QString *string) {

	// skip over blank space
	in.skipWhitespace();

	// look for initial quote
	if (*in != QLatin1Char('\"')) {
		*errMsg = QLatin1String("expecting quoted string");
		return false;
	}
	++in;

	// calculate max length and allocate returned string
	Input c = in;

	for ( ;; ++c) {
		if (c.atEnd()) {
			*errMsg = QLatin1String("string not terminated");
			return false;
		} else if (*c == QLatin1Char('\"')) {
			if (*(c + 1) == QLatin1Char('\"')) {
				++c;
			} else {
				break;
			}
		}
	}

	// copy string up to end quote, transforming escaped quotes into quotes
	QString str;
	str.reserve(c - in);

	auto outPtr = std::back_inserter(str);

	while (true) {
		if (*in == QLatin1Char('\"')) {
			if (*(in + 1) == QLatin1Char('\"')) {
				++in;
			} else {
				break;
			}
		}
		*outPtr++ = *in++;
	}

	// skip end quote
	++in;

	*string = str;
	return true;
}

/*
** Adds double quotes around a string and escape existing double quote
** characters with two double quotes.  Enables the string to be read back
** by ReadQuotedString.
*/
QString MakeQuotedStringEx(const QString &string) {

    int length = 0;

    // calculate length and allocate returned string
    for(QChar ch: string) {
        if (ch == QLatin1Char('\"')) {
            length++;
        }
        length++;
    }

    QString outStr;
    outStr.reserve(length + 3);
    auto outPtr = std::back_inserter(outStr);

    // add starting quote
    *outPtr++ = QLatin1Char('\"');

    // copy string, escaping quotes with ""
    for(QChar ch: string) {
        if (ch == QLatin1Char('\"')) {
            *outPtr++ = QLatin1Char('\"');
        }
        *outPtr++ = ch;
    }

    // add ending quote
    *outPtr++ = QLatin1Char('\"');

    return outStr;
}

/*
** Skip a delimiter and it's surrounding whitespace
*/
bool SkipDelimiterEx(Input &in, QString *errMsg) {

	in.skipWhitespace();

	if (*in != QLatin1Char(':')) {
		*errMsg = QLatin1String("syntax error");
		return false;
	}

	++in;

	in.skipWhitespace();
	return true;
}

/*
** Skip an optional separator and its surrounding whitespace
** return true if delimiter found
*/
int SkipOptSeparatorEx(QChar separator, Input &in) {
	in.skipWhitespace();

	if (*in != separator) {
		return false;
	}

	++in;
	in.skipWhitespace();
	return true;
}

/*
** Short-hand error processing for language mode parsing errors, frees
** lm (if non-null), prints a formatted message explaining where the
** error is, and returns False;
*/
static bool modeErrorEx(const Input &in, const QString &message) {
	return ParseErrorEx(
	            nullptr,
	            *in.string(),
	            in.index(),
	            QLatin1String("language mode specification"),
	            message);
}

/*
** Report parsing errors in resource strings or macros, formatted nicely so
** the user can tell where things became botched.  Errors can be sent either
** to stderr, or displayed in a dialog.  For stderr, pass toDialog as nullptr.
** For a dialog, pass the dialog parent in toDialog.
*/

bool ParseErrorEx(QWidget *toDialog, const QString &string, int stoppedAt, const QString &errorIn, const QString &message) {

	// NOTE(eteran): hack to work around the fact that stoppedAt can be a "one past the end iterator"
    stoppedAt = qBound(0, stoppedAt, string.size() - 1);

	int nNonWhite = 0;
	int c;

	for (c = stoppedAt; c >= 0; c--) {
		if (c == 0) {
			break;
		} else if (string[c] == QLatin1Char('\n') && nNonWhite >= 5) {
			break;
		} else if (string[c] != QLatin1Char(' ') && string[c] != QLatin1Char('\t')) {
			nNonWhite++;
		}
	}
	
	int len = stoppedAt - c + (stoppedAt == string.size() ? 0 : 1);

	QString errorLine = QString(QLatin1String("%1<==")).arg(string.mid(c, len));

	if(!toDialog) {
        qWarning("NEdit: %s in %s:\n%s", qPrintable(message), qPrintable(errorIn), qPrintable(errorLine));
	} else {
		QMessageBox::warning(toDialog, QLatin1String("Parse Error"), QString(QLatin1String("%1 in %2:\n%3")).arg(message, errorIn, errorLine));
	}
	
	return false;
}

/*
**  This function passes up a pointer to the static name of the default
**  shell, currently defined as the user's login shell.
**  In case of errors, the fallback of "sh" will be returned.
*/
static QString getDefaultShell() {
    struct passwd *passwdEntry = getpwuid(getuid()); //  getuid() never fails.

    if (!passwdEntry) {
        //  Something bad happened! Do something, quick!
        perror("nedit: Failed to get passwd entry (falling back to 'sh')");
        return QLatin1String("sh");
    }

    return QString::fromLatin1(passwdEntry->pw_shell);
}
