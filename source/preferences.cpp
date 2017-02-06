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

#include <QMessageBox>
#include <QString>
#include <QPushButton>
#include <QInputDialog>
#include <QStandardPaths>
#include <QtDebug>
#include <QSettings>
#include "ui/DocumentWidget.h"
#include "ui/MainWindow.h"
#include "LanguageMode.h"
#include "Preferences.h"

#include "preferences.h"
#include "TextBuffer.h"
#include "nedit.h"
#include "search.h"
#include "userCmds.h"
#include "highlight.h"
#include "highlightData.h"
#include "smartIndent.h"
#include "server.h"
#include "tags.h"
#include <cctype>
#include <pwd.h>
#include <memory>
#include <unistd.h>
#include <sys/types.h>

#include "util/clearcase.h"

#define N_WRAP_STYLES 3
static const char *AutoWrapTypes[N_WRAP_STYLES + 3] = {"None", "Newline", "Continuous", "True", "False", nullptr};

#define N_INDENT_STYLES 3
static const char *AutoIndentTypes[N_INDENT_STYLES + 3] = {"None", "Auto", "Smart", "True", "False", nullptr};


/* suplement wrap and indent styles w/ a value meaning "use default" for
   the override fields in the language modes dialog */
#define DEFAULT_TAB_DIST -1
#define DEFAULT_EM_TAB_DIST -1

// list of available language modes and language specific preferences 
int NLanguageModes = 0;
LanguageMode *LanguageModes[MAX_LANGUAGE_MODES];

/* Module-global variable set when any preference changes (for asking the
   user about re-saving on exit) */
bool PrefsHaveChanged = false;

/* Module-global variable set when user uses -import to load additional
   preferences on top of the defaults.  Contains name of file loaded */
QString ImportedFile;

static void translatePrefFormats(bool convertOld, int fileVer);

static QStringList readExtensionList(const char **inPtr);
static QString getDefaultShell();
static int loadLanguageModesString(const char *inString, int fileVer);
static int loadLanguageModesStringEx(const std::string &string, int fileVer);
static int modeError(LanguageMode *lm, const char *stringStart, const char *stoppedAt, const char *message);
static QString WriteLanguageModesStringEx();

static Preferences g_Preferences;

void RestoreNEditPrefs() {

    g_Preferences.loadPreferences();

    /* Do further parsing on resource types which RestorePreferences does
       not understand and reads as strings, to put them in the final form
       in which nedit stores and uses.  If the preferences file was
       written by an older version of NEdit, update regular expressions in
       highlight patterns to quote braces and use & instead of \0 */
    translatePrefFormats(false, 1000);
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
static void translatePrefFormats(bool convertOld, int fileVer) {

	/* Parse the strings which represent types which are not decoded by
	   the standard resource manager routines */

    if (!g_Preferences.shellCommands.isNull()) {
        LoadShellCmdsStringEx(g_Preferences.shellCommands.toStdString());
	}
    if (!g_Preferences.macroCommands.isNull()) {
        LoadMacroCmdsStringEx(g_Preferences.macroCommands.toStdString());
	}
    if (!g_Preferences.bgMenuCommands.isNull()) {
        LoadBGMenuCmdsStringEx(g_Preferences.bgMenuCommands.toStdString());
	}
    if (!g_Preferences.highlightPatterns.isNull()) {
        LoadHighlightStringEx(g_Preferences.highlightPatterns.toStdString(), convertOld);
	}
    if (!g_Preferences.styles.isNull()) {
        LoadStylesStringEx(g_Preferences.styles.toStdString());
	}
    if (!g_Preferences.languageModes.isNull()) {
        loadLanguageModesStringEx(g_Preferences.languageModes.toStdString(), fileVer);
	}
    if (!g_Preferences.smartIndentInit.isNull()) {
        LoadSmartIndentStringEx(g_Preferences.smartIndentInit);
	}
    if (!g_Preferences.smartIndentInitCommon.isNull()) {
        LoadSmartIndentCommonStringEx(g_Preferences.smartIndentInitCommon.toStdString());
	}

	// translate the font names into fontLists suitable for the text widget 
    g_Preferences.boldFontStruct.fromString(g_Preferences.boldHighlightFont);
    g_Preferences.italicFontStruct.fromString(g_Preferences.italicHighlightFont);
    g_Preferences.boldItalicFontStruct.fromString(g_Preferences.boldItalicHighlightFont);

    g_Preferences.boldFontStruct.setStyleStrategy(QFont::ForceIntegerMetrics);
    g_Preferences.italicFontStruct.setStyleStrategy(QFont::ForceIntegerMetrics);
    g_Preferences.boldItalicFontStruct.setStyleStrategy(QFont::ForceIntegerMetrics);

	/*
	**  The default set for the comand shell in PrefDescrip ("DEFAULT") is
	**  only a place-holder, the actual default is the user's login shell
	**  (or whatever is implemented in getDefaultShell()). We put the login
	**  shell's name in PrefData here.
	*/
    if (g_Preferences.shell == QLatin1String("DEFAULT")) {
        g_Preferences.shell = getDefaultShell();
	}

	/* For compatability with older (4.0.3 and before) versions, the autoWrap
	   and autoIndent resources can accept values of True and False.  Translate
	   them into acceptable wrap and indent styles */
    if (g_Preferences.autoWrap== 3)
        g_Preferences.autoWrap = CONTINUOUS_WRAP;
    if (g_Preferences.autoWrap == 4)
        g_Preferences.autoWrap = NO_WRAP;

    if (g_Preferences.autoIndent == 3)
        g_Preferences.autoIndent = AUTO_INDENT;
    if (g_Preferences.autoIndent == 4)
        g_Preferences.autoIndent = NO_AUTO_INDENT;

	/* setup language mode dependent info of user menus (to increase
	   performance when switching between documents of different
	   language modes) */
    SetupUserMenuInfo();

}

void SaveNEditPrefsEx(QWidget *parent, bool quietly) {

    QString prefFileName = Preferences::configFile();
    if(prefFileName.isNull()) {
        QMessageBox::warning(parent, QLatin1String("Error saving Preferences"), QLatin1String("Unable to save preferences: Cannot determine filename."));
        return;
    }

    if (!quietly) {
        int resp = QMessageBox::information(parent, QLatin1String("Save Preferences"),
            ImportedFile.isNull() ? QString(QLatin1String("Default preferences will be saved in the file:\n%1\nNEdit automatically loads this file\neach time it is started.")).arg(prefFileName)
                                  : QString(QLatin1String("Default preferences will be saved in the file:\n%1\nSAVING WILL INCORPORATE SETTINGS\nFROM FILE: %2")).arg(prefFileName).arg(ImportedFile),
                QMessageBox::Ok | QMessageBox::Cancel);



        if(resp == QMessageBox::Cancel) {
            return;
        }
    }

    g_Preferences.fileVersion           = QLatin1String("1.0");
    g_Preferences.shellCommands         = WriteShellCmdsStringEx();
    g_Preferences.macroCommands         = WriteMacroCmdsStringEx();
    g_Preferences.bgMenuCommands        = WriteBGMenuCmdsStringEx();
    g_Preferences.highlightPatterns     = WriteHighlightStringEx();
    g_Preferences.languageModes         = WriteLanguageModesStringEx();
    g_Preferences.styles                = WriteStylesStringEx();
    g_Preferences.smartIndentInit       = WriteSmartIndentStringEx();
    g_Preferences.smartIndentInitCommon = WriteSmartIndentCommonStringEx();

    if (!g_Preferences.savePreferences()) {
        QMessageBox::warning(parent, QLatin1String("Save Preferences"), QString(QLatin1String("Unable to save preferences in %1")).arg(prefFileName));
    }

    PrefsHaveChanged = false;
}

/*
** Load an additional preferences file on top of the existing preferences
** derived from defaults, the .nedit file, and X resources.
*/
void ImportPrefFile(const char *filename, int convertOld) {
    Q_UNUSED(filename);
    Q_UNUSED(convertOld);
    // TODO(eteran): implement this
    qDebug("Not Implemented, please report");
}

void SetPrefOpenInTab(int state) {
    if(g_Preferences.openInTab != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.openInTab = state;
}

int GetPrefOpenInTab() {
    return g_Preferences.openInTab;
}

void SetPrefWrap(int state) {
    if(g_Preferences.autoWrap != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.autoWrap = state;
}

int GetPrefWrap(int langMode) {
    if (langMode == PLAIN_LANGUAGE_MODE || LanguageModes[langMode]->wrapStyle == DEFAULT_WRAP) {
        return g_Preferences.autoWrap;
    }

	return LanguageModes[langMode]->wrapStyle;
}

void SetPrefWrapMargin(int margin) {
    if(g_Preferences.wrapMargin != margin) {
        PrefsHaveChanged = true;
    }
    g_Preferences.wrapMargin = margin;
}

int GetPrefWrapMargin() {
    return g_Preferences.wrapMargin;
}

void SetPrefSearch(SearchType searchType) {
    if(g_Preferences.searchMethod != searchType) {
        PrefsHaveChanged = true;
    }
    g_Preferences.searchMethod = searchType;
}

SearchType GetPrefSearch() {
    return static_cast<SearchType>(g_Preferences.searchMethod);
}

#ifdef REPLACE_SCOPE
void SetPrefReplaceDefScope(int scope) {
    if(g_Preferences.replaceDefaultScope != scope) {
        PrefsHaveChanged = true;
    }
    g_Preferences.replaceDefaultScope = scope;
}

int GetPrefReplaceDefScope() {
    return g_Preferences.replaceDefaultScope;
}
#endif

void SetPrefAutoIndent(int state) {
    if(g_Preferences.autoIndent != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.autoIndent = state;
}

IndentStyle GetPrefAutoIndent(int langMode) {
    if (langMode == PLAIN_LANGUAGE_MODE || LanguageModes[langMode]->indentStyle == DEFAULT_INDENT) {
        return static_cast<IndentStyle>(g_Preferences.autoIndent);
    }

	return LanguageModes[langMode]->indentStyle;
}

void SetPrefAutoSave(int state) {
    if(g_Preferences.autoSave != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.autoSave = state;
}

int GetPrefAutoSave() {
    return g_Preferences.autoSave;
}

void SetPrefSaveOldVersion(int state) {
    if(g_Preferences.saveOldVersion != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.saveOldVersion = state;
}

int GetPrefSaveOldVersion() {
    return g_Preferences.saveOldVersion;
}

void SetPrefSearchDlogs(int state) {
    if(g_Preferences.searchDialogs != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.searchDialogs = state;
}

int GetPrefSearchDlogs() {
    return g_Preferences.searchDialogs;
}

void SetPrefBeepOnSearchWrap(int state) {
    if(g_Preferences.beepOnSearchWrap != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.beepOnSearchWrap = state;
}

int GetPrefBeepOnSearchWrap() {
    return g_Preferences.beepOnSearchWrap;
}

void SetPrefKeepSearchDlogs(int state) {
    if(g_Preferences.retainSearchDialogs != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.retainSearchDialogs = state;
}

int GetPrefKeepSearchDlogs() {
    return g_Preferences.retainSearchDialogs;
}

void SetPrefSearchWraps(int state) {
    if(g_Preferences.searchWraps != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.searchWraps = state;
}

int GetPrefSearchWraps() {
    return g_Preferences.searchWraps;
}

int GetPrefStickyCaseSenseBtn() {
    return g_Preferences.stickyCaseSenseButton;
}

void SetPrefStatsLine(int state) {
    if(g_Preferences.statisticsLine != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.statisticsLine = state;
}

int GetPrefStatsLine() {
    return g_Preferences.statisticsLine;
}

void SetPrefISearchLine(int state) {
    if(g_Preferences.iSearchLine != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.iSearchLine = state;
}

int GetPrefISearchLine() {
    return g_Preferences.iSearchLine;
}

void SetPrefSortTabs(int state) {
    if(g_Preferences.sortTabs != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.sortTabs = state;
}

int GetPrefSortTabs() {
    return g_Preferences.sortTabs;
}

void SetPrefTabBar(int state) {
    if(g_Preferences.tabBar != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.tabBar = state;
}

int GetPrefTabBar() {
    return g_Preferences.tabBar;
}

void SetPrefTabBarHideOne(int state) {
    if(g_Preferences.tabBarHideOne != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.tabBarHideOne = state;
}

int GetPrefTabBarHideOne() {
    return g_Preferences.tabBarHideOne;
}

void SetPrefGlobalTabNavigate(int state) {
    if(g_Preferences.globalTabNavigate != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.globalTabNavigate = state;
}

int GetPrefGlobalTabNavigate() {
    return g_Preferences.globalTabNavigate;
}

void SetPrefToolTips(int state) {
    if(g_Preferences.toolTips != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.toolTips = state;
}

int GetPrefToolTips() {
    return g_Preferences.toolTips;
}

void SetPrefLineNums(int state) {
    if(g_Preferences.lineNumbers != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.lineNumbers = state;
}

int GetPrefLineNums() {
    return g_Preferences.lineNumbers;
}

void SetPrefShowPathInWindowsMenu(int state) {
    if(g_Preferences.pathInWindowsMenu != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.pathInWindowsMenu = state;
}

int GetPrefShowPathInWindowsMenu() {
    return g_Preferences.pathInWindowsMenu;
}

void SetPrefWarnFileMods(int state) {
    if(g_Preferences.warnFileMods != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.warnFileMods = state;
}

int GetPrefWarnFileMods() {
    return g_Preferences.warnFileMods;
}

void SetPrefWarnRealFileMods(int state) {
    if(g_Preferences.warnRealFileMods != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.warnRealFileMods = state;
}

int GetPrefWarnRealFileMods() {
    return g_Preferences.warnRealFileMods;
}

void SetPrefWarnExit(int state) {
    if(g_Preferences.warnExit != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.warnExit = state;
}

int GetPrefWarnExit() {
    return g_Preferences.warnExit;
}

void SetPrefFindReplaceUsesSelection(int state) {
    if(g_Preferences.findReplaceUsesSelection != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.findReplaceUsesSelection = state;
}

int GetPrefFindReplaceUsesSelection() {
    return g_Preferences.findReplaceUsesSelection;
}

// Advanced
int GetPrefMapDelete() {
    return g_Preferences.remapDeleteKey;
}

// Advanced
int GetPrefStdOpenDialog() {
    return g_Preferences.stdOpenDialog;
}

void SetPrefRows(int nRows) {
    if(g_Preferences.textRows != nRows) {
        PrefsHaveChanged = true;
    }
    g_Preferences.textRows = nRows;
}

int GetPrefRows() {
    return g_Preferences.textRows;
}

void SetPrefCols(int nCols) {
    if(g_Preferences.textCols != nCols) {
        PrefsHaveChanged = true;
    }
    g_Preferences.textCols = nCols;
}

int GetPrefCols() {
    return g_Preferences.textCols;
}

void SetPrefTabDist(int tabDist) {
    if(g_Preferences.tabDistance != tabDist) {
        PrefsHaveChanged = true;
    }
    g_Preferences.tabDistance = tabDist;
}

int GetPrefTabDist(int langMode) {
	int tabDist;

    if (langMode == PLAIN_LANGUAGE_MODE || LanguageModes[langMode]->tabDist == DEFAULT_TAB_DIST) {
        tabDist = g_Preferences.tabDistance;
	} else {
		tabDist = LanguageModes[langMode]->tabDist;
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
    if(g_Preferences.emulateTabs != tabDist) {
        PrefsHaveChanged = true;
    }
    g_Preferences.emulateTabs = tabDist;
}

int GetPrefEmTabDist(int langMode) {
    if (langMode == PLAIN_LANGUAGE_MODE || LanguageModes[langMode]->emTabDist == DEFAULT_EM_TAB_DIST) {
        return g_Preferences.emulateTabs;
    }

	return LanguageModes[langMode]->emTabDist;
}

void SetPrefInsertTabs(int state) {
    if(g_Preferences.insertTabs != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.insertTabs = state;
}

int GetPrefInsertTabs() {
    return g_Preferences.insertTabs;
}

void SetPrefShowMatching(int state) {
    if(g_Preferences.showMatching != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.showMatching = state;
}

int GetPrefShowMatching() {
    return g_Preferences.showMatching;
}

void SetPrefMatchSyntaxBased(int state) {
    if(g_Preferences.matchSyntaxBased != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.matchSyntaxBased = state;
}

int GetPrefMatchSyntaxBased() {
    return g_Preferences.matchSyntaxBased;
}

void SetPrefHighlightSyntax(bool state) {
    if(g_Preferences.highlightSyntax != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.highlightSyntax = state;
}

bool GetPrefHighlightSyntax() {
    return g_Preferences.highlightSyntax;
}

void SetPrefBacklightChars(int state) {
    if(g_Preferences.backlightChars != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.backlightChars = state;
}

int GetPrefBacklightChars() {
    return g_Preferences.backlightChars;
}

QString GetPrefBacklightCharTypes() {
    return g_Preferences.backlightCharTypes;
}

void SetPrefRepositionDialogs(int state) {
    if(g_Preferences.repositionDialogs != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.repositionDialogs = state;
}

int GetPrefRepositionDialogs() {
    return g_Preferences.repositionDialogs;
}

void SetPrefAutoScroll(int state) {
    if(g_Preferences.autoScroll != state) {
        PrefsHaveChanged = true;
    }

    const int margin = state ? g_Preferences.autoScrollVPadding : 0;

    g_Preferences.autoScroll = state;
	
    for(DocumentWidget *document : MainWindow::allDocuments()) {
        document->SetAutoScroll(margin);
	}
}

int GetPrefAutoScroll() {
    return g_Preferences.autoScroll;
}

int GetVerticalAutoScroll() {
    return g_Preferences.autoScroll ? g_Preferences.autoScrollVPadding : 0;
}

void SetPrefAppendLF(int state) {
    if(g_Preferences.appendLF != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.appendLF = state;
}

int GetPrefAppendLF() {
    return g_Preferences.appendLF;
}

void SetPrefSortOpenPrevMenu(int state) {
    if(g_Preferences.sortOpenPrevMenu != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.sortOpenPrevMenu = state;
}

int GetPrefSortOpenPrevMenu() {
    return g_Preferences.sortOpenPrevMenu;
}

QString GetPrefTagFile() {
    return g_Preferences.tagFile;
}

void SetPrefSmartTags(int state) {
    if(g_Preferences.smartTags != state) {
        PrefsHaveChanged = true;
    }
    g_Preferences.smartTags = state;
}

int GetPrefSmartTags() {
    return g_Preferences.smartTags;
}

// Advanced
int GetPrefAlwaysCheckRelTagsSpecs() {
    return g_Preferences.alwaysCheckRelativeTagsSpecs;
}

QString GetPrefDelimiters() {
    return g_Preferences.wordDelimiters;
}

QString GetPrefColorName(int index) {
    return g_Preferences.colors[index];
}

void SetPrefColorName(int index, const QString &name) {
    if(g_Preferences.colors[index] != name) {
        PrefsHaveChanged = true;
    }
    g_Preferences.colors[index] = name;
}

/*
** Set the font preferences using the font name (the fontList is generated
** in this call).  Note that this leaks memory and server resources each
** time the default font is re-set.  See note on SetFontByName in window.c
** for more information.
*/
void SetPrefFont(const QString &fontName) {
    if(g_Preferences.textFont != fontName) {
        PrefsHaveChanged = true;
    }
    g_Preferences.textFont = fontName;
}

void SetPrefBoldFont(const QString &fontName) {
    if(g_Preferences.boldHighlightFont != fontName) {
        PrefsHaveChanged = true;
    }
    g_Preferences.boldHighlightFont = fontName;
    g_Preferences.boldFontStruct.fromString(fontName);
    g_Preferences.boldFontStruct.setStyleStrategy(QFont::ForceIntegerMetrics);
}

void SetPrefItalicFont(const QString &fontName) {
    if(g_Preferences.italicHighlightFont != fontName) {
        PrefsHaveChanged = true;
    }
    g_Preferences.italicHighlightFont = fontName;
    g_Preferences.italicFontStruct.fromString(fontName);
    g_Preferences.italicFontStruct.setStyleStrategy(QFont::ForceIntegerMetrics);
}
void SetPrefBoldItalicFont(const QString &fontName) {
    if(g_Preferences.boldItalicHighlightFont != fontName) {
        PrefsHaveChanged = true;
    }
    g_Preferences.boldItalicHighlightFont = fontName;
    g_Preferences.boldItalicFontStruct.fromString(fontName);
    g_Preferences.boldItalicFontStruct.setStyleStrategy(QFont::ForceIntegerMetrics);
}

QString GetPrefFontName() {
    return g_Preferences.textFont;
}

QString GetPrefBoldFontName() {
    return g_Preferences.boldHighlightFont;
}

QString GetPrefItalicFontName() {
    return g_Preferences.italicHighlightFont;
}

QString GetPrefBoldItalicFontName() {
    return g_Preferences.boldItalicHighlightFont;
}

QFont GetPrefBoldFont() {
    return g_Preferences.boldFontStruct;
}

QFont GetPrefItalicFont() {
    return g_Preferences.italicFontStruct;
}

QFont GetPrefBoldItalicFont() {
    return g_Preferences.boldItalicFontStruct;
}

QString GetPrefHelpFontName(int index) {
    return g_Preferences.helpFonts[index];
}

QString GetPrefHelpLinkColor() {
    return g_Preferences.helpLinkColor;
}

QString GetPrefTooltipBgColor() {
    return g_Preferences.tooltipBgColor;
}

void SetPrefShell(const QString &shell) {
    if(g_Preferences.shell != shell) {
        PrefsHaveChanged = true;
    }
    g_Preferences.shell = shell;
}

QString GetPrefShell() {
    return g_Preferences.shell;
}

QString GetPrefGeometry() {
    return g_Preferences.geometry;
}

QString GetPrefServerName() {
    return g_Preferences.serverName;
}

QString GetPrefBGMenuBtn() {
    return g_Preferences.bgMenuButton;
}

int GetPrefMaxPrevOpenFiles() {
    return g_Preferences.maxPrevOpenFiles;
}

int GetPrefTypingHidesPointer() {
    return g_Preferences.typingHidesPointer;
}

void SetPrefTitleFormat(const QString &format) {
    if(g_Preferences.titleFormat != format) {
        PrefsHaveChanged = true;
    }

    g_Preferences.titleFormat = format;

    // update all windows
    for(MainWindow *window : MainWindow::allWindows()) {
        window->UpdateWindowTitle(window->currentDocument());
	}
}
QString GetPrefTitleFormat() {
    return g_Preferences.titleFormat;
}

bool GetPrefUndoModifiesSelection() {
    return g_Preferences.undoModifiesSelection;
}

bool GetPrefFocusOnRaise() {
    return g_Preferences.focusOnRaise;
}

bool GetPrefForceOSConversion() {
    return g_Preferences.forceOSConversion;
}

bool GetPrefHonorSymlinks() {
    return g_Preferences.honorSymlinks;
}

int GetPrefOverrideVirtKeyBindings() {
    return g_Preferences.overrideDefaultVirtualKeyBindings;
}

int GetPrefTruncSubstitution() {
    return g_Preferences.truncSubstitution;
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
int FindLanguageMode(const char *languageName) {

	// Compare each language mode to the one we were presented 
	for (int i = 0; i < NLanguageModes; i++) {
        if (LanguageModes[i]->name == QString::fromLatin1(languageName)) {
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
		return LanguageModes[mode]->name;
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
        return LanguageModes[window->languageMode_]->delimiters;
}


static int loadLanguageModesStringEx(const std::string &string, int fileVer) {
	
	// TODO(eteran): implement this natively
	return loadLanguageModesString(string.c_str(), fileVer);
}

static int loadLanguageModesString(const char *inString, int fileVer) {
	const char *errMsg;
	char *styleName;
	const char *inPtr = inString;
	int i;

	for (;;) {

		// skip over blank space 
		inPtr += strspn(inPtr, " \t\n");

		auto lm = new LanguageMode;
		
		// read language mode name 		
		char *name = ReadSymbolicField(&inPtr);
		if (!name) {
			delete lm;
			return modeError(nullptr, inString, inPtr, "language mode name required");
		}
		
        lm->name = QString::fromLatin1(name);
		
		if (!SkipDelimiter(&inPtr, &errMsg))
			return modeError(lm, inString, inPtr, errMsg);

		// read list of extensions 
		lm->extensions = readExtensionList(&inPtr);
		if (!SkipDelimiter(&inPtr, &errMsg))
			return modeError(lm, inString, inPtr, errMsg);

		// read the recognition regular expression 
		char *recognitionExpr;
		if (*inPtr == '\n' || *inPtr == '\0' || *inPtr == ':') {
			recognitionExpr = nullptr;
		} else if (!ReadQuotedString(&inPtr, &errMsg, &recognitionExpr)) {
			return modeError(lm, inString, inPtr, errMsg);
		}
			
			
        lm->recognitionExpr = QString::fromLatin1(recognitionExpr);
			
		if (!SkipDelimiter(&inPtr, &errMsg)) {
			return modeError(lm, inString, inPtr, errMsg);
		}

		// read the indent style 
		styleName = ReadSymbolicField(&inPtr);
		if(!styleName)
			lm->indentStyle = DEFAULT_INDENT;
		else {
			for (i = 0; i < N_INDENT_STYLES; i++) {
				if (!strcmp(styleName, AutoIndentTypes[i])) {
                    lm->indentStyle = static_cast<IndentStyle>(i);
					break;
				}
			}
			XtFree(styleName);
			if (i == N_INDENT_STYLES)
				return modeError(lm, inString, inPtr, "unrecognized indent style");
		}
		if (!SkipDelimiter(&inPtr, &errMsg))
			return modeError(lm, inString, inPtr, errMsg);

		// read the wrap style 
		styleName = ReadSymbolicField(&inPtr);
		if(!styleName)
			lm->wrapStyle = DEFAULT_WRAP;
		else {
			for (i = 0; i < N_WRAP_STYLES; i++) {
				if (!strcmp(styleName, AutoWrapTypes[i])) {
					lm->wrapStyle = i;
					break;
				}
			}
			XtFree(styleName);
			if (i == N_WRAP_STYLES)
				return modeError(lm, inString, inPtr, "unrecognized wrap style");
		}
		if (!SkipDelimiter(&inPtr, &errMsg))
			return modeError(lm, inString, inPtr, errMsg);

		// read the tab distance 
		if (*inPtr == '\n' || *inPtr == '\0' || *inPtr == ':')
			lm->tabDist = DEFAULT_TAB_DIST;
		else if (!ReadNumericField(&inPtr, &lm->tabDist))
			return modeError(lm, inString, inPtr, "bad tab spacing");
		if (!SkipDelimiter(&inPtr, &errMsg))
			return modeError(lm, inString, inPtr, errMsg);

		// read emulated tab distance 
		if (*inPtr == '\n' || *inPtr == '\0' || *inPtr == ':')
			lm->emTabDist = DEFAULT_EM_TAB_DIST;
		else if (!ReadNumericField(&inPtr, &lm->emTabDist))
			return modeError(lm, inString, inPtr, "bad emulated tab spacing");
		if (!SkipDelimiter(&inPtr, &errMsg))
			return modeError(lm, inString, inPtr, errMsg);

		// read the delimiters string 
		char *delimiters;
		if (*inPtr == '\n' || *inPtr == '\0' || *inPtr == ':') {
			delimiters = nullptr;
		} else if (!ReadQuotedString(&inPtr, &errMsg, &delimiters)) {
			return modeError(lm, inString, inPtr, errMsg);
		}
			
        lm->delimiters = delimiters ? QString::fromLatin1(delimiters) : QString();

		// After 5.3 all language modes need a default tips file field 
		if (!SkipDelimiter(&inPtr, &errMsg))
			if (fileVer > 5003)
				return modeError(lm, inString, inPtr, errMsg);

		// read the default tips file 
		
		char *defTipsFile;
		if (*inPtr == '\n' || *inPtr == '\0') {
			defTipsFile = nullptr;
		} else if (!ReadQuotedString(&inPtr, &errMsg, &defTipsFile)) {
			return modeError(lm, inString, inPtr, errMsg);
		}
		
        lm->defTipsFile = defTipsFile ? QString::fromLatin1(defTipsFile) : QString();

		// pattern set was read correctly, add/replace it in the list 
		for (i = 0; i < NLanguageModes; i++) {
			if (LanguageModes[i]->name == lm->name) {
				delete LanguageModes[i];
				LanguageModes[i] = lm;
				break;
			}
		}
		
		if (i == NLanguageModes) {
			LanguageModes[NLanguageModes++] = lm;
			if (NLanguageModes > MAX_LANGUAGE_MODES) {
				return modeError(nullptr, inString, inPtr, "maximum allowable number of language modes exceeded");
			}
		}

		// if the string ends here, we're done 
		inPtr += strspn(inPtr, " \t\n");
		if (*inPtr == '\0')
			return true;
	}
}

static QString WriteLanguageModesStringEx() {
	char numBuf[25];

    auto outBuf = std::make_unique<TextBuffer>();

	for (int i = 0; i < NLanguageModes; i++) {
		outBuf->BufAppendEx("\t");
		outBuf->BufAppendEx(LanguageModes[i]->name.toStdString());
		outBuf->BufAppendEx(":");
		
		QString str = LanguageModes[i]->extensions.join(QLatin1String(" "));
		
		outBuf->BufAppendEx(str.toStdString());
		outBuf->BufAppendEx(":");
		if (!LanguageModes[i]->recognitionExpr.isEmpty()) {
			std::string str = MakeQuotedStringEx(LanguageModes[i]->recognitionExpr.toStdString());
			outBuf->BufAppendEx(str);
		}
		
		outBuf->BufAppendEx(":");
		if (LanguageModes[i]->indentStyle != DEFAULT_INDENT)
			outBuf->BufAppendEx(AutoIndentTypes[LanguageModes[i]->indentStyle]);
		
		outBuf->BufAppendEx(":");
		if (LanguageModes[i]->wrapStyle != DEFAULT_WRAP)
			outBuf->BufAppendEx(AutoWrapTypes[LanguageModes[i]->wrapStyle]);
		
		outBuf->BufAppendEx(":");		
		if (LanguageModes[i]->tabDist != DEFAULT_TAB_DIST) {
			sprintf(numBuf, "%d", LanguageModes[i]->tabDist);
			outBuf->BufAppendEx(numBuf);
		}
		
		outBuf->BufAppendEx(":");
		if (LanguageModes[i]->emTabDist != DEFAULT_EM_TAB_DIST) {
			sprintf(numBuf, "%d", LanguageModes[i]->emTabDist);
			outBuf->BufAppendEx(numBuf);
		}
		
		outBuf->BufAppendEx(":");
		if (!LanguageModes[i]->delimiters.isEmpty()) {
			std::string str = MakeQuotedStringEx(LanguageModes[i]->delimiters.toStdString());
			outBuf->BufAppendEx(str);
		}
		
		outBuf->BufAppendEx(":");
		if (!LanguageModes[i]->defTipsFile.isEmpty()) {
			std::string str = MakeQuotedStringEx(LanguageModes[i]->defTipsFile.toStdString());
			outBuf->BufAppendEx(str);
		}

		outBuf->BufAppendEx("\n");
	}

	// Get the output, and lop off the trailing newline 
	std::string outStr = outBuf->BufGetRangeEx(0, outBuf->BufGetLength() - 1);
    return QString::fromStdString(outStr);
}

static QStringList readExtensionList(const char **inPtr) {
	QStringList extensionList;
	const char *strStart;
	int len;

	// skip over blank space 
	*inPtr += strspn(*inPtr, " \t");

	while(**inPtr != ':' && **inPtr != '\0') {
		
		*inPtr += strspn(*inPtr, " \t");
		strStart = *inPtr;
		
		while (**inPtr != ' ' && **inPtr != '\t' && **inPtr != ':' && **inPtr != '\0') {
			(*inPtr)++;
		}
		
		len = *inPtr - strStart;
		
		
		extensionList.push_back(QString::fromLatin1(strStart, len));
	}


	return extensionList;
}

int ReadNumericField(const char **inPtr, int *value) {
	int charsRead;

	// skip over blank space 
	*inPtr += strspn(*inPtr, " \t");

	if (sscanf(*inPtr, "%d%n", value, &charsRead) != 1) {
		return false;
	}
	
	*inPtr += charsRead;
	return true;
}

/*
** Parse a symbolic field, skipping initial and trailing whitespace,
** stops on first invalid character or end of string.  Valid characters
** are letters, numbers, _, -, +, $, #, and internal whitespace.  Internal
** whitespace is compressed to single space characters.
*/
char *ReadSymbolicField(const char **inPtr) {
	char *outStr, *outPtr;
	const char *strStart;
	const char *strPtr;
	int len;

	// skip over initial blank space 
	*inPtr += strspn(*inPtr, " \t");

	/* Find the first invalid character or end of string to know how
	   much memory to allocate for the returned string */
	strStart = *inPtr;
	while (isalnum((uint8_t)**inPtr) || **inPtr == '_' || **inPtr == '-' || **inPtr == '+' || **inPtr == '$' || **inPtr == '#' || **inPtr == ' ' || **inPtr == '\t')
		(*inPtr)++;
	len = *inPtr - strStart;
	if (len == 0)
		return nullptr;
	outStr = outPtr = XtMalloc(len + 1);

	// Copy the string, compressing internal whitespace to a single space 
	strPtr = strStart;
	while (strPtr - strStart < len) {
		if (*strPtr == ' ' || *strPtr == '\t') {
			strPtr += strspn(strPtr, " \t");
			*outPtr++ = ' ';
		} else
			*outPtr++ = *strPtr++;
	}

	// If there's space on the end, take it back off 
	if (outPtr > outStr && *(outPtr - 1) == ' ')
		outPtr--;
	if (outPtr == outStr) {
		XtFree(outStr);
		return nullptr;
	}
	*outPtr = '\0';
	return outStr;
}

/*
** Parse a symbolic field, skipping initial and trailing whitespace,
** stops on first invalid character or end of string.  Valid characters
** are letters, numbers, _, -, +, $, #, and internal whitespace.  Internal
** whitespace is compressed to single space characters.
*/
QString ReadSymbolicFieldEx(const char **inPtr) {

	// skip over initial blank space 
	*inPtr += strspn(*inPtr, " \t");

	/* Find the first invalid character or end of string to know how
	   much memory to allocate for the returned string */
	const char *strStart = *inPtr;
	while (isalnum((uint8_t)**inPtr) || **inPtr == '_' || **inPtr == '-' || **inPtr == '+' || **inPtr == '$' || **inPtr == '#' || **inPtr == ' ' || **inPtr == '\t') {
		(*inPtr)++;
	}
	
	int len = *inPtr - strStart;
	if (len == 0) {
		return QString();
	}
	
	std::string outStr;
	outStr.reserve(len);
	
	auto outPtr = std::back_inserter(outStr);

	// Copy the string, compressing internal whitespace to a single space 
	const char *strPtr = strStart;
	while (strPtr - strStart < len) {
		if (*strPtr == ' ' || *strPtr == '\t') {
			strPtr += strspn(strPtr, " \t");
			*outPtr++ = ' ';
		} else {
			*outPtr++ = *strPtr++;
		}
	}

	// If there's space on the end, take it back off 
	if(!outStr.empty() && outStr.back() == ' ') {
		outStr.pop_back();	
	}
	
	if(outStr.empty()) {
		return QString();
	}
	
	return QString::fromStdString(outStr);
}

/*
** parse an individual quoted string.  Anything between
** double quotes is acceptable, quote characters can be escaped by "".
** Returns allocated string "string" containing
** argument minus quotes.  If not successful, returns False with
** (statically allocated) message in "errMsg".
*/
int ReadQuotedString(const char **inPtr, const char **errMsg, char **string) {
	char *outPtr;
	const char *c;

	// skip over blank space 
	*inPtr += strspn(*inPtr, " \t");

	// look for initial quote 
	if (**inPtr != '\"') {
		*errMsg = "expecting quoted string";
		return false;
	}
	(*inPtr)++;

	// calculate max length and allocate returned string 
	for (c = *inPtr;; c++) {
		if (*c == '\0') {
			*errMsg = "string not terminated";
			return false;
		} else if (*c == '\"') {
			if (*(c + 1) == '\"')
				c++;
			else
				break;
		}
	}

	// copy string up to end quote, transforming escaped quotes into quotes 
	*string = XtMalloc(c - *inPtr + 1);
	outPtr = *string;
	while (true) {
		if (**inPtr == '\"') {
			if (*(*inPtr + 1) == '\"')
				(*inPtr)++;
			else
				break;
		}
		*outPtr++ = *(*inPtr)++;
	}
	*outPtr = '\0';

	// skip end quote 
	(*inPtr)++;
	return true;
}

/*
** parse an individual quoted string.  Anything between
** double quotes is acceptable, quote characters can be escaped by "".
** Returns allocated string "string" containing
** argument minus quotes.  If not successful, returns False with
** (statically allocated) message in "errMsg".
*/
int ReadQuotedStringEx(const char **inPtr, const char **errMsg, QString *string) {
	const char *c;

	// skip over blank space 
	*inPtr += strspn(*inPtr, " \t");

	// look for initial quote 
	if (**inPtr != '\"') {
		*errMsg = "expecting quoted string";
		return false;
	}
	(*inPtr)++;

	// calculate max length and allocate returned string 
	for (c = *inPtr;; c++) {
		if (*c == '\0') {
			*errMsg = "string not terminated";
			return false;
		} else if (*c == '\"') {
			if (*(c + 1) == '\"')
				c++;
			else
				break;
		}
	}

	// copy string up to end quote, transforming escaped quotes into quotes
	QString str;
	str.reserve(c - *inPtr);
	
	auto outPtr = std::back_inserter(str);

	while (true) {
		if (**inPtr == '\"') {
			if (*(*inPtr + 1) == '\"')
				(*inPtr)++;
			else
				break;
		}
		*outPtr++ = QLatin1Char(*(*inPtr)++);
	}

	// skip end quote 
	(*inPtr)++;
	
	*string = str;
	return true;
}

/*
** Replace characters which the X resource file reader considers control
** characters, such that a string will read back as it appears in "string".
** (So far, newline characters are replaced with with \n\<newline> and
** backslashes with \\.  This has not been tested exhaustively, and
** probably should be.  It would certainly be more asthetic if other
** control characters were replaced as well).
**
** Returns an allocated string which must be freed by the caller with XtFree.
*/
char *EscapeSensitiveChars(const char *string) {
	const char *c;
	char *outStr, *outPtr;
	int length = 0;

	// calculate length and allocate returned string 
	for (c = string; *c != '\0'; c++) {
		if (*c == '\\')
			length++;
		else if (*c == '\n')
			length += 3;
		length++;
	}
	outStr = XtMalloc(length + 1);
	outPtr = outStr;

	// add backslashes 
	for (c = string; *c != '\0'; c++) {
		if (*c == '\\')
			*outPtr++ = '\\';
		else if (*c == '\n') {
			*outPtr++ = '\\';
			*outPtr++ = 'n';
			*outPtr++ = '\\';
		}
		*outPtr++ = *c;
	}
	*outPtr = '\0';
	return outStr;
}


/*
** Replace characters which the X resource file reader considers control
** characters, such that a string will read back as it appears in "string".
** (So far, newline characters are replaced with with \n\<newline> and
** backslashes with \\.  This has not been tested exhaustively, and
** probably should be.  It would certainly be more asthetic if other
** control characters were replaced as well).
**
** Returns an allocated string which must be freed by the caller with XtFree.
*/
QString EscapeSensitiveCharsEx(const QString &string) {

	int length = 0;

	// calculate length and allocate returned string 
	for(QChar ch : string) {
		if (ch == QLatin1Char('\\')) {
			length++;
		} else if (ch == QLatin1Char('\n')) {
			length += 3;
		}
		
		length++;
	}
	
	QString outStr;
	outStr.reserve(length);
	auto outPtr = std::back_inserter(outStr);

	// add backslashes 
	for(QChar ch : string) {
		if (ch == QLatin1Char('\\'))
			*outPtr++ = QLatin1Char('\\');
			
		else if (ch == QLatin1Char('\n')) {
		
			*outPtr++ = QLatin1Char('\\');
			*outPtr++ = QLatin1Char('n');
			*outPtr++ = QLatin1Char('\\');
		}
		*outPtr++ = ch;
	}

	return outStr;
}

std::string EscapeSensitiveCharsEx(view::string_view string) {

	int length = 0;

	// calculate length and allocate returned string 
	for(char ch : string) {
		if (ch == '\\')
			length++;
		else if (ch == '\n')
			length += 3;
		length++;
	}
	
	std::string outStr;
	outStr.reserve(length);
	auto outPtr = std::back_inserter(outStr);

	// add backslashes 
	for(char ch : string) {
		if (ch == '\\')
			*outPtr++ = '\\';
		else if (ch == '\n') {
			*outPtr++ = '\\';
			*outPtr++ = 'n';
			*outPtr++ = '\\';
		}
		*outPtr++ = ch;
	}

	return outStr;
}

/*
** Adds double quotes around a string and escape existing double quote
** characters with two double quotes.  Enables the string to be read back
** by ReadQuotedString.
*/
std::string MakeQuotedStringEx(view::string_view string) {

	int length = 0;

	// calculate length and allocate returned string 
	for(char ch: string) {
		if (ch == '\"') {
			length++;
		}
		length++;
	}
	
	std::string outStr;
	outStr.reserve(length + 3);
	auto outPtr = std::back_inserter(outStr);

	// add starting quote 
	*outPtr++ = '\"';

	// copy string, escaping quotes with "" 
	for(char ch: string) {
		if (ch == '\"') {
			*outPtr++ = '\"';
		}
		*outPtr++ = ch;
	}

	// add ending quote 
	*outPtr++ = '\"';

	return outStr;
}

/*
** Skip a delimiter and it's surrounding whitespace
*/
int SkipDelimiter(const char **inPtr, const char **errMsg) {
	*inPtr += strspn(*inPtr, " \t");
	if (**inPtr != ':') {
		*errMsg = "syntax error";
		return false;
	}
	(*inPtr)++;
	*inPtr += strspn(*inPtr, " \t");
	return true;
}

/*
** Skip an optional separator and its surrounding whitespace
** return true if delimiter found
*/
int SkipOptSeparator(char separator, const char **inPtr) {
	*inPtr += strspn(*inPtr, " \t");
	if (**inPtr != separator) {
		return false;
	}
	(*inPtr)++;
	*inPtr += strspn(*inPtr, " \t");
	return true;
}

/*
** Short-hand error processing for language mode parsing errors, frees
** lm (if non-null), prints a formatted message explaining where the
** error is, and returns False;
*/
static int modeError(LanguageMode *lm, const char *stringStart, const char *stoppedAt, const char *message) {
	if(lm) {
		delete lm;
	}

    return ParseErrorEx(nullptr, QString::fromLatin1(stringStart), stoppedAt - stringStart, QLatin1String("language mode specification"), QString::fromLatin1(message));
}

/*
** Report parsing errors in resource strings or macros, formatted nicely so
** the user can tell where things became botched.  Errors can be sent either
** to stderr, or displayed in a dialog.  For stderr, pass toDialog as nullptr.
** For a dialog, pass the dialog parent in toDialog.
*/

bool ParseErrorEx(QWidget *toDialog, const QString &string, int stoppedAt, const QString &errorIn, const QString &message) {

	// NOTE(eteran): hack to work around the fact that stoppedAt can be a "one past the end iterator"
	if(stoppedAt == string.size()) {
		stoppedAt = string.size() - 1;
	}

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
		fprintf(stderr, "NEdit: %s in %s:\n%s\n", message.toLatin1().data(), errorIn.toLatin1().data(), errorLine.toLatin1().data());
	} else {
		QMessageBox::warning(toDialog, QLatin1String("Parse Error"), QString(QLatin1String("%1 in %2:\n%3")).arg(message).arg(errorIn).arg(errorLine));
	}
	
	return false;
}

int ParseError(Widget toDialog, const char *stringStart, const char *stoppedAt, const char *errorIn, const char *message) {
	int len;
	int nNonWhite = 0;
	const char *c;

	for (c = stoppedAt; c >= stringStart; c--) {
		if (c == stringStart)
			break;
		else if (*c == '\n' && nNonWhite >= 5)
			break;
		else if (*c != ' ' && *c != '\t')
			nNonWhite++;
	}
	len = stoppedAt - c + (*stoppedAt == '\0' ? 0 : 1);

	auto errorLine = new char[len + 4];
	strncpy(errorLine, c, len);
	errorLine[len++] = '<';
	errorLine[len++] = '=';
	errorLine[len++] = '=';
	errorLine[len] = '\0';
	
	if(!toDialog) {
		fprintf(stderr, "NEdit: %s in %s:\n%s\n", message, errorIn, errorLine);
	} else {
        QMessageBox::warning(nullptr /*toDialog*/, QLatin1String("Parse Error"), QString(QLatin1String("%1 in %2:\n%3")).arg(QString::fromLatin1(message)).arg(QString::fromLatin1(errorIn)).arg(QString::fromLatin1(errorLine)));
	}
	
	delete [] errorLine;
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
