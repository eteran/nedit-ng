
#include "preferences.h"
#include "DocumentWidget.h"
#include "Font.h"
#include "highlight.h"
#include "Util/Input.h"
#include "LanguageMode.h"
#include "MainWindow.h"
#include "search.h"
#include "Settings.h"
#include "smartIndent.h"
#include "Tags.h"
#include "TextBuffer.h"
#include "userCmds.h"
#include "Util/ClearCase.h"
#include "Util/version.h"

#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QString>
#include <QtDebug>

#include <cctype>
#include <memory>
#include <pwd.h>
#include <unistd.h>

namespace {

constexpr int ConfigFileVersion = 1;

QLatin1String AutoWrapTypes[] = {
    QLatin1String("None"),
    QLatin1String("Newline"),
    QLatin1String("Continuous")
};

QLatin1String AutoIndentTypes[] = {
    QLatin1String("None"),
    QLatin1String("Auto"),
    QLatin1String("Smart")
};

/* Module-global variable set when any preference changes (for asking the
   user about re-saving on exit) */
bool PrefsHaveChanged = false;

/* Module-global variable set when user uses -import to load additional
   preferences on top of the defaults.  Contains name of file loaded */
QString ImportedFile;

}

// list of available language modes and language specific preferences
std::vector<LanguageMode> LanguageModes;

QString Preferences::ImportedSettingsFile() {
    return ImportedFile;
}

bool Preferences::PreferencesChanged() {
    return PrefsHaveChanged;
}

void Preferences::RestoreNEditPrefs() {

    Settings::loadPreferences();

    /* Do further parsing on resource types which RestorePreferences does
       not understand and reads as strings, to put them in the final form
       in which nedit stores and uses.  If the preferences file was
       written by an older version of NEdit, update regular expressions in
       highlight patterns to quote braces and use & instead of \0 */
    translatePrefFormats(NEDIT_VERSION);
}

void Preferences::SaveNEditPrefsEx(QWidget *parent, bool quietly) {

    QString prefFileName = Settings::configFile();
    if(prefFileName.isNull()) {
        QMessageBox::warning(parent, tr("Error saving Preferences"), tr("Unable to save preferences: Cannot determine filename."));
        return;
    }

    if (!quietly) {
        int resp = QMessageBox::information(parent, tr("Save Preferences"),
            ImportedFile.isNull() ? tr("Default preferences will be saved in the file:\n%1\nNEdit automatically loads this file each time it is started.").arg(prefFileName)
                                  : tr("Default preferences will be saved in the file:\n%1\nSAVING WILL INCORPORATE SETTINGS FROM FILE: %2").arg(prefFileName, ImportedFile),
                QMessageBox::Ok | QMessageBox::Cancel);



        if(resp == QMessageBox::Cancel) {
            return;
        }
    }

    Settings::fileVersion           = ConfigFileVersion;
    Settings::shellCommands         = WriteShellCmdsStringEx();
    Settings::macroCommands         = WriteMacroCmdsStringEx();
    Settings::bgMenuCommands        = WriteBGMenuCmdsStringEx();
    Settings::highlightPatterns     = WriteHighlightStringEx();
    Settings::languageModes         = WriteLanguageModesStringEx();
    Settings::smartIndentInit       = WriteSmartIndentStringEx();
    Settings::smartIndentInitCommon = WriteSmartIndentCommonStringEx();

    if (!Settings::savePreferences()) {
        QMessageBox::warning(
                    parent,
                    tr("Save Preferences"),
                    tr("Unable to save preferences in %1").arg(prefFileName));
    }

    SaveTheme();
    PrefsHaveChanged = false;
}

/*
** Load an additional preferences file on top of the existing preferences
** derived from defaults, the .nedit file, and X resources.
*/
void Preferences::ImportPrefFile(const QString &filename) {
    Settings::importSettings(filename);
}

void Preferences::SetPrefOpenInTab(bool state) {
    if(Settings::openInTab != state) {
        PrefsHaveChanged = true;
    }
    Settings::openInTab = state;
}

bool Preferences::GetPrefOpenInTab() {
    return Settings::openInTab;
}

void Preferences::SetPrefWrap(WrapStyle state) {
    if(Settings::autoWrap != state) {
        PrefsHaveChanged = true;
    }
    Settings::autoWrap = state;
}

WrapStyle Preferences::GetPrefWrap(size_t langMode) {
    if (langMode == PLAIN_LANGUAGE_MODE || LanguageModes[langMode].wrapStyle == WrapStyle::Default) {
        return Settings::autoWrap;
    }

    return LanguageModes[langMode].wrapStyle;
}

void Preferences::SetPrefWrapMargin(int margin) {
    if(Settings::wrapMargin != margin) {
        PrefsHaveChanged = true;
    }
    Settings::wrapMargin = margin;
}

int Preferences::GetPrefWrapMargin() {
    return Settings::wrapMargin;
}

void Preferences::SetPrefSearch(SearchType searchType) {
    if(Settings::searchMethod != searchType) {
        PrefsHaveChanged = true;
    }
    Settings::searchMethod = searchType;
}

bool Preferences::GetPrefHeavyCursor() {
    return Settings::heavyCursor;
}

bool Preferences::GetPrefAutoWrapPastedText() {
    return Settings::autoWrapPastedText;
}

bool Preferences::GetPrefColorizeHighlightedText() {
    return Settings::colorizeHighlightedText;
}

SearchType Preferences::GetPrefSearch() {
    return Settings::searchMethod;
}

void Preferences::SetPrefAutoIndent(IndentStyle state) {
    if(Settings::autoIndent != state) {
        PrefsHaveChanged = true;
    }
    Settings::autoIndent = state;
}

IndentStyle Preferences::GetPrefAutoIndent(size_t langMode) {
    if (langMode == PLAIN_LANGUAGE_MODE || LanguageModes[langMode].indentStyle == IndentStyle::Default) {
        return Settings::autoIndent;
    }

    return LanguageModes[langMode].indentStyle;
}

void Preferences::SetPrefAutoSave(bool state) {
    if(Settings::autoSave != state) {
        PrefsHaveChanged = true;
    }
    Settings::autoSave = state;
}

bool Preferences::GetPrefAutoSave() {
    return Settings::autoSave;
}

void Preferences::SetPrefSaveOldVersion(bool state) {
    if(Settings::saveOldVersion != state) {
        PrefsHaveChanged = true;
    }
    Settings::saveOldVersion = state;
}

bool Preferences::GetPrefSaveOldVersion() {
    return Settings::saveOldVersion;
}

void Preferences::SetPrefSearchDlogs(bool state) {
    if(Settings::searchDialogs != state) {
        PrefsHaveChanged = true;
    }
    Settings::searchDialogs = state;
}

bool Preferences::GetPrefSearchDlogs() {
    return Settings::searchDialogs;
}

void Preferences::SetPrefBeepOnSearchWrap(bool state) {
    if(Settings::beepOnSearchWrap != state) {
        PrefsHaveChanged = true;
    }
    Settings::beepOnSearchWrap = state;
}

bool Preferences::GetPrefBeepOnSearchWrap() {
    return Settings::beepOnSearchWrap;
}

void Preferences::SetPrefKeepSearchDlogs(bool state) {
    if(Settings::retainSearchDialogs != state) {
        PrefsHaveChanged = true;
    }
    Settings::retainSearchDialogs = state;
}

bool Preferences::GetPrefKeepSearchDlogs() {
    return Settings::retainSearchDialogs;
}

void Preferences::SetPrefSearchWraps(bool state) {
    if(Settings::searchWraps != state) {
        PrefsHaveChanged = true;
    }
    Settings::searchWraps = state;
}

WrapMode Preferences::GetPrefSearchWraps() {
    return Settings::searchWraps ? WrapMode::Wrap : WrapMode::NoWrap;
}

int Preferences::GetPrefStickyCaseSenseBtn() {
    return Settings::stickyCaseSenseButton;
}

void Preferences::SetPrefStatsLine(bool state) {
    if(Settings::statisticsLine != state) {
        PrefsHaveChanged = true;
    }
    Settings::statisticsLine = state;
}

int Preferences::GetPrefStatsLine() {
    return Settings::statisticsLine;
}

void Preferences::SetPrefISearchLine(bool state) {
    if(Settings::iSearchLine != state) {
        PrefsHaveChanged = true;
    }
    Settings::iSearchLine = state;
}

int Preferences::GetPrefISearchLine() {
    return Settings::iSearchLine;
}

void Preferences::SetPrefSortTabs(bool state) {
    if(Settings::sortTabs != state) {
        PrefsHaveChanged = true;
    }
    Settings::sortTabs = state;
}

bool Preferences::GetPrefSortTabs() {
    return Settings::sortTabs;
}

void Preferences::SetPrefTabBar(bool state) {
    if(Settings::tabBar != state) {
        PrefsHaveChanged = true;
    }
    Settings::tabBar = state;
}

bool Preferences::GetPrefTabBar() {
    return Settings::tabBar;
}

void Preferences::SetPrefTabBarHideOne(bool state) {
    if(Settings::tabBarHideOne != state) {
        PrefsHaveChanged = true;
    }
    Settings::tabBarHideOne = state;
}

int Preferences::GetPrefTabBarHideOne() {
    return Settings::tabBarHideOne;
}

void Preferences::SetPrefGlobalTabNavigate(bool state) {
    if(Settings::globalTabNavigate != state) {
        PrefsHaveChanged = true;
    }
    Settings::globalTabNavigate = state;
}

int Preferences::GetPrefGlobalTabNavigate() {
    return Settings::globalTabNavigate;
}

void Preferences::SetPrefToolTips(bool state) {
    if(Settings::toolTips != state) {
        PrefsHaveChanged = true;
    }
    Settings::toolTips = state;
}

bool Preferences::GetPrefToolTips() {
    return Settings::toolTips;
}

void Preferences::SetPrefLineNums(bool state) {
    if(Settings::lineNumbers != state) {
        PrefsHaveChanged = true;
    }
    Settings::lineNumbers = state;
}

int Preferences::GetPrefLineNums() {
    return Settings::lineNumbers;
}

void Preferences::SetPrefShowPathInWindowsMenu(bool state) {
    if(Settings::pathInWindowsMenu != state) {
        PrefsHaveChanged = true;
    }
    Settings::pathInWindowsMenu = state;
}

int Preferences::GetPrefShowPathInWindowsMenu() {
    return Settings::pathInWindowsMenu;
}

void Preferences::SetPrefWarnFileMods(bool state) {
    if(Settings::warnFileMods != state) {
        PrefsHaveChanged = true;
    }
    Settings::warnFileMods = state;
}

bool Preferences::GetPrefWarnFileMods() {
    return Settings::warnFileMods;
}

void Preferences::SetPrefWarnRealFileMods(bool state) {
    if(Settings::warnRealFileMods != state) {
        PrefsHaveChanged = true;
    }
    Settings::warnRealFileMods = state;
}

bool Preferences::GetPrefWarnRealFileMods() {
    return Settings::warnRealFileMods;
}

void Preferences::SetPrefWarnExit(bool state) {
    if(Settings::warnExit != state) {
        PrefsHaveChanged = true;
    }
    Settings::warnExit = state;
}

bool Preferences::GetPrefWarnExit() {
    return Settings::warnExit;
}

void Preferences::SetPrefFindReplaceUsesSelection(bool state) {
    if(Settings::findReplaceUsesSelection != state) {
        PrefsHaveChanged = true;
    }
    Settings::findReplaceUsesSelection = state;
}

bool Preferences::GetPrefFindReplaceUsesSelection() {
    return Settings::findReplaceUsesSelection;
}

void Preferences::SetPrefRows(int nRows) {
    if(Settings::textRows != nRows) {
        PrefsHaveChanged = true;
    }
    Settings::textRows = nRows;
}

int Preferences::GetPrefRows() {
    return Settings::textRows;
}

void Preferences::SetPrefCols(int nCols) {
    if(Settings::textCols != nCols) {
        PrefsHaveChanged = true;
    }
    Settings::textCols = nCols;
}

int Preferences::GetPrefCols() {
    return Settings::textCols;
}

void Preferences::SetPrefTabDist(int tabDist) {
    if(Settings::tabDistance != tabDist) {
        PrefsHaveChanged = true;
    }
    Settings::tabDistance = tabDist;
}

int Preferences::GetPrefTabDist(size_t langMode) {
	int tabDist;

    if (langMode == PLAIN_LANGUAGE_MODE || LanguageModes[langMode].tabDist == LanguageMode::DEFAULT_TAB_DIST) {
        tabDist = Settings::tabDistance;
	} else {
        tabDist = LanguageModes[langMode].tabDist;
	}

	/* Make sure that the tab distance is in range (garbage may have
       been entered via the command line or the ini file, causing
       errors later on, like division by zero). */
    if (tabDist <= 0) {
        return 1;
    }

    if (tabDist > TextBuffer::MAX_EXP_CHAR_LEN) {
        return TextBuffer::MAX_EXP_CHAR_LEN;
    }

	return tabDist;
}

void Preferences::SetPrefEmTabDist(int tabDist) {
    if(Settings::emulateTabs != tabDist) {
        PrefsHaveChanged = true;
    }
    Settings::emulateTabs = tabDist;
}

int Preferences::GetPrefEmTabDist(size_t langMode) {
    if (langMode == PLAIN_LANGUAGE_MODE || LanguageModes[langMode].emTabDist == LanguageMode::DEFAULT_EM_TAB_DIST) {
        return Settings::emulateTabs;
    }

    return LanguageModes[langMode].emTabDist;
}

void Preferences::SetPrefInsertTabs(bool state) {
    if(Settings::insertTabs != state) {
        PrefsHaveChanged = true;
    }
    Settings::insertTabs = state;
}

int Preferences::GetPrefInsertTabs() {
    return Settings::insertTabs;
}

void Preferences::SetPrefShowMatching(ShowMatchingStyle state) {
    if(Settings::showMatching != state) {
        PrefsHaveChanged = true;
    }
    Settings::showMatching = state;
}

ShowMatchingStyle Preferences::GetPrefShowMatching() {
    return Settings::showMatching;
}

void Preferences::SetPrefMatchSyntaxBased(bool state) {
    if(Settings::matchSyntaxBased != state) {
        PrefsHaveChanged = true;
    }
    Settings::matchSyntaxBased = state;
}

int Preferences::GetPrefMatchSyntaxBased() {
    return Settings::matchSyntaxBased;
}

void Preferences::SetPrefHighlightSyntax(bool state) {
    if(Settings::highlightSyntax != state) {
        PrefsHaveChanged = true;
    }
    Settings::highlightSyntax = state;
}

bool Preferences::GetPrefHighlightSyntax() {
    return Settings::highlightSyntax;
}

void Preferences::SetPrefBacklightChars(bool state) {
    if(Settings::backlightChars != state) {
        PrefsHaveChanged = true;
    }
    Settings::backlightChars = state;
}

bool Preferences::GetPrefBacklightChars() {
    return Settings::backlightChars;
}

QString Preferences::GetPrefBacklightCharTypes() {
    return Settings::backlightCharTypes;
}

void Preferences::SetPrefRepositionDialogs(bool state) {
    if(Settings::repositionDialogs != state) {
        PrefsHaveChanged = true;
    }
    Settings::repositionDialogs = state;
}

bool Preferences::GetPrefRepositionDialogs() {
    return Settings::repositionDialogs;
}

void Preferences::SetPrefAutoScroll(bool state) {
    if(Settings::autoScroll != state) {
        PrefsHaveChanged = true;
    }

    const int margin = state ? Settings::autoScrollVPadding : 0;

    Settings::autoScroll = state;
	
    // TODO(eteran): should this update be done at this level?
    for(DocumentWidget *document : DocumentWidget::allDocuments()) {
        document->SetAutoScroll(margin);
	}
}

bool Preferences::GetPrefAutoScroll() {
    return Settings::autoScroll;
}

int Preferences::GetVerticalAutoScroll() {
    return Settings::autoScroll ? Settings::autoScrollVPadding : 0;
}

void Preferences::SetPrefAppendLF(bool state) {
    if(Settings::appendLF != state) {
        PrefsHaveChanged = true;
    }
    Settings::appendLF = state;
}

bool Preferences::GetPrefAppendLF() {
    return Settings::appendLF;
}

void Preferences::SetPrefSortOpenPrevMenu(bool state) {
    if(Settings::sortOpenPrevMenu != state) {
        PrefsHaveChanged = true;
    }
    Settings::sortOpenPrevMenu = state;
}

int Preferences::GetPrefSortOpenPrevMenu() {
    return Settings::sortOpenPrevMenu;
}

QString Preferences::GetPrefTagFile() {
    return Settings::tagFile;
}

void Preferences::SetPrefSmartTags(bool state) {
    if(Settings::smartTags != state) {
        PrefsHaveChanged = true;
    }
    Settings::smartTags = state;
}

int Preferences::GetPrefSmartTags() {
    return Settings::smartTags;
}

// Advanced
bool Preferences::GetPrefAlwaysCheckRelTagsSpecs() {
    return Settings::alwaysCheckRelativeTagsSpecs;
}

QString Preferences::GetPrefDelimiters() {
    return Settings::wordDelimiters;
}

QString Preferences::GetPrefColorName(ColorTypes index) {
    return Settings::colors[index];
}

void Preferences::SetPrefColorName(ColorTypes index, const QString &name) {
    if(Settings::colors[index] != name) {
        PrefsHaveChanged = true;
    }
    Settings::colors[index] = name;
}

void Preferences::SetPrefFont(const QString &fontName) {
    if(Settings::textFont != fontName) {
        PrefsHaveChanged = true;
    }
    Settings::textFont        = fontName;
    Settings::plainFontStruct = Font::fromString(fontName);
}

void Preferences::SetPrefBoldFont(const QString &fontName) {
    if(Settings::boldHighlightFont != fontName) {
        PrefsHaveChanged = true;
    }
    Settings::boldHighlightFont = fontName;
    Settings::boldFontStruct    = Font::fromString(fontName);
}

void Preferences::SetPrefItalicFont(const QString &fontName) {
    if(Settings::italicHighlightFont != fontName) {
        PrefsHaveChanged = true;
    }
    Settings::italicHighlightFont = fontName;
    Settings::italicFontStruct    = Font::fromString(fontName);
}

void Preferences::SetPrefBoldItalicFont(const QString &fontName) {
    if(Settings::boldItalicHighlightFont != fontName) {
        PrefsHaveChanged = true;
    }
    Settings::boldItalicHighlightFont = fontName;
    Settings::boldItalicFontStruct    = Font::fromString(fontName);
}

QString Preferences::GetPrefFontName() {
    return Settings::textFont;
}

QString Preferences::GetPrefBoldFontName() {
    return Settings::boldHighlightFont;
}

QString Preferences::GetPrefItalicFontName() {
    return Settings::italicHighlightFont;
}

QString Preferences::GetPrefBoldItalicFontName() {
    return Settings::boldItalicHighlightFont;
}

QFont Preferences::GetPrefDefaultFont() {
    return Settings::plainFontStruct;
}

QFont Preferences::GetPrefBoldFont() {
    return Settings::boldFontStruct;
}

QFont Preferences::GetPrefItalicFont() {
    return Settings::italicFontStruct;
}

QFont Preferences::GetPrefBoldItalicFont() {
    return Settings::boldItalicFontStruct;
}

void Preferences::SetPrefShell(const QString &shell) {
    if(Settings::shell != shell) {
        PrefsHaveChanged = true;
    }
    Settings::shell = shell;
}

QString Preferences::GetPrefShell() {
    return Settings::shell;
}

QString Preferences::GetPrefGeometry() {
    return Settings::geometry;
}

QString Preferences::GetPrefServerName() {
    return Settings::serverName;
}

int Preferences::GetPrefMaxPrevOpenFiles() {
    return Settings::maxPrevOpenFiles;
}

bool Preferences::GetPrefTypingHidesPointer() {
    return Settings::typingHidesPointer;
}

void Preferences::SetPrefTitleFormat(const QString &format) {
    if(Settings::titleFormat != format) {
        PrefsHaveChanged = true;
    }

    Settings::titleFormat = format;

    // update all windows
    // TODO(eteran): should this be done at this level? maybe a signal/slot update?
    for(MainWindow *window : MainWindow::allWindows()) {
        window->UpdateWindowTitle(window->currentDocument());
	}
}
QString Preferences::GetPrefTitleFormat() {
    return Settings::titleFormat;
}

bool Preferences::GetPrefUndoModifiesSelection() {
    return Settings::undoModifiesSelection;
}

bool Preferences::GetPrefFocusOnRaise() {
    return Settings::focusOnRaise;
}

bool Preferences::GetPrefForceOSConversion() {
    return Settings::forceOSConversion;
}

bool Preferences::GetPrefHonorSymlinks() {
    return Settings::honorSymlinks;
}

TruncSubstitution Preferences::GetPrefTruncSubstitution() {
    return Settings::truncSubstitution;
}

/*
** If preferences don't get saved, ask the user on exit whether to save
*/
void Preferences::MarkPrefsChanged() {
	PrefsHaveChanged = true;
}

/*
** Lookup a language mode by name, returning the index of the language
** mode or PLAIN_LANGUAGE_MODE if the name is not found
*/
size_t Preferences::FindLanguageMode(const QString &languageName) {

    // Compare each language mode to the one we were presented
    for (size_t i = 0; i < LanguageModes.size(); i++) {
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
QString Preferences::LanguageModeName(size_t mode) {
	if (mode == PLAIN_LANGUAGE_MODE)
		return QString();
	else
        return LanguageModes[mode].name;
}

bool Preferences::ReadNumericFieldEx(Input &in, int *value) {

	// skip over blank space
	in.skipWhitespace();

    static const QRegularExpression re(QLatin1String("(0|[-+]?[1-9][0-9]*)"));
    QString number;
    if(in.match(re, &number)) {
        bool ok;
        *value = number.toInt(&ok);
        if(ok) {
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
QString Preferences::ReadSymbolicFieldEx(Input &input) {

	// skip over initial blank space
	input.skipWhitespace();

	/* Find the first invalid character or end of string to know how
	   much memory to allocate for the returned string */
	Input strStart = input;


    // TODO(eteran): we can replace this with a regex based input.match
    while ((*input).isLetterOrNumber() ||
           *input == QLatin1Char('_') ||
           *input == QLatin1Char('-') ||
           *input == QLatin1Char('+') ||
           *input == QLatin1Char('$') ||
           *input == QLatin1Char('#') ||
           *input == QLatin1Char(' ') ||
           *input == QLatin1Char('\t')) {
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
** argument minus quotes.  If not successful, returns false with
** (statically allocated) message in "errMsg".
*/
bool Preferences::ReadQuotedStringEx(Input &in, QString *errMsg, QString *string) {

	// skip over blank space
	in.skipWhitespace();

	// look for initial quote
    if (*in != QLatin1Char('"')) {
        *errMsg = tr("expecting quoted string");
		return false;
	}
	++in;

	// calculate max length and allocate returned string
	Input c = in;

	for ( ;; ++c) {
		if (c.atEnd()) {
            *errMsg = tr("string not terminated");
			return false;
        } else if (*c == QLatin1Char('"')) {
            if (*(c + 1) == QLatin1Char('"')) {
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
        if (*in == QLatin1Char('"')) {
            if (*(in + 1) == QLatin1Char('"')) {
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
QString Preferences::MakeQuotedStringEx(const QString &string) {

    int length = 0;

    // calculate length and allocate returned string
    for(QChar ch: string) {
        if (ch == QLatin1Char('"')) {
            length++;
        }
        length++;
    }

    QString outStr;
    outStr.reserve(length + 3);
    auto outPtr = std::back_inserter(outStr);

    // add starting quote
    *outPtr++ = QLatin1Char('"');

    // copy string, escaping quotes with ""
    for(QChar ch: string) {
        if (ch == QLatin1Char('"')) {
            *outPtr++ = QLatin1Char('"');
        }
        *outPtr++ = ch;
    }

    // add ending quote
    *outPtr++ = QLatin1Char('"');

    return outStr;
}

/*
** Skip a delimiter and it's surrounding whitespace
*/
bool Preferences::SkipDelimiterEx(Input &in, QString *errMsg) {

	in.skipWhitespace();

	if (*in != QLatin1Char(':')) {
        *errMsg = tr("syntax error");
		return false;
	}

	++in;
	in.skipWhitespace();
	return true;
}

/*
** Report parsing errors in resource strings or macros, formatted nicely so
** the user can tell where things became botched.  Errors can be sent either
** to stderr, or displayed in a dialog.  For stderr, pass toDialog as nullptr.
** For a dialog, pass the dialog parent in toDialog.
*/

bool Preferences::ParseErrorEx(QWidget *toDialog, const QString &string, int stoppedAt, const QString &errorIn, const QString &message) {

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

    QString errorLine = tr("%1<==").arg(string.mid(c, len));

	if(!toDialog) {
        qWarning("NEdit: %s in %s:\n%s", qPrintable(message), qPrintable(errorIn), qPrintable(errorLine));
	} else {
        QMessageBox::warning(toDialog, tr("Parse Error"), tr("%1 in %2:\n%3").arg(message, errorIn, errorLine));
	}
	
	return false;
}

int Preferences::loadLanguageModesStringEx(const QString &string) {
    QString errMsg;

    Input in(&string);

    Q_FOREVER {

        // skip over blank space
        in.skipWhitespaceNL();

        LanguageMode lm;

        // read language mode name
        QString name = ReadSymbolicFieldEx(in);
        if (name.isNull()) {
            return modeErrorEx(in, tr("language mode name required"));
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
            auto it = std::find(std::begin(AutoIndentTypes), std::end(AutoIndentTypes), styleName);
            if(it == std::end(AutoIndentTypes)) {
                return modeErrorEx(in, tr("unrecognized indent style"));
            }

            lm.indentStyle = static_cast<IndentStyle>(it - std::begin(AutoIndentTypes));
        }

        if (!SkipDelimiterEx(in, &errMsg)) {
            return modeErrorEx(in, errMsg);
        }

        // read the wrap style
        styleName = ReadSymbolicFieldEx(in);
        if(styleName.isNull()) {
            lm.wrapStyle = WrapStyle::Default;
        } else {
            auto it = std::find(std::begin(AutoWrapTypes), std::end(AutoWrapTypes), styleName);
            if(it == std::end(AutoWrapTypes)) {
                return modeErrorEx(in, tr("unrecognized wrap style"));
            }

            lm.wrapStyle = static_cast<WrapStyle>(it - std::begin(AutoWrapTypes));
        }

        if (!SkipDelimiterEx(in, &errMsg)) {
            return modeErrorEx(in, errMsg);
        }

        // read the tab distance
        if (in.atEnd() || *in == QLatin1Char('\n') || *in == QLatin1Char(':')) {
            lm.tabDist = LanguageMode::DEFAULT_TAB_DIST;
        } else if (!ReadNumericFieldEx(in, &lm.tabDist)) {
            return modeErrorEx(in, tr("bad tab spacing"));
        }

        if (!SkipDelimiterEx(in, &errMsg)) {
            return modeErrorEx(in, errMsg);
        }

        // read emulated tab distance
        if (in.atEnd() || *in == QLatin1Char('\n') || *in == QLatin1Char(':')) {
            lm.emTabDist = LanguageMode::DEFAULT_EM_TAB_DIST;
        }
        else if (!ReadNumericFieldEx(in, &lm.emTabDist))
            return modeErrorEx(in, tr("bad emulated tab spacing"));

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
        if (!SkipDelimiterEx(in, &errMsg)) {
            return modeErrorEx(in, errMsg);
        }

        // read the default tips file
        QString defTipsFile;
        if (in.atEnd() || *in == QLatin1Char('\n')) {
            defTipsFile = QString();
        } else if (!ReadQuotedStringEx(in, &errMsg, &defTipsFile)) {
            return modeErrorEx(in, errMsg);
        }

        lm.defTipsFile = defTipsFile;

        // pattern set was read correctly, add/replace it in the list
        auto it = std::find_if(LanguageModes.begin(), LanguageModes.end(), [&lm](const LanguageMode &languageMode) {
            return languageMode.name == lm.name;
        });

        if(it != LanguageModes.end()) {
            *it = lm;
        } else {
            LanguageModes.push_back(lm);
        }

        // if the string ends here, we're done
        in.skipWhitespaceNL();

        if (in.atEnd()) {
            return true;
        }
    }
}

QString Preferences::WriteLanguageModesStringEx() {

    QString str;
    QTextStream out(&str);

    for(const LanguageMode &language : LanguageModes) {

        out << QLatin1Char('\t')
            << language.name
            << QLatin1Char(':');

        QString exts = language.extensions.join(QLatin1Char(' '));
        out << exts
            << QLatin1Char(':');

        if (!language.recognitionExpr.isEmpty()) {
            out << MakeQuotedStringEx(language.recognitionExpr);
        }

        out << QLatin1Char(':');
        if (language.indentStyle != IndentStyle::Default) {
            out << AutoIndentTypes[static_cast<int>(language.indentStyle)];
        }

        out << QLatin1Char(':');
        if (language.wrapStyle != WrapStyle::Default) {
            out << AutoWrapTypes[static_cast<int>(language.wrapStyle)];
        }

        out << QLatin1Char(':');
        if (language.tabDist != LanguageMode::DEFAULT_TAB_DIST) {
            out << language.tabDist;
        }

        out << QLatin1Char(':');
        if (language.emTabDist != LanguageMode::DEFAULT_EM_TAB_DIST) {
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
void Preferences::translatePrefFormats(uint32_t fileVer) {

    Q_UNUSED(fileVer);

    /* Parse the strings which represent types which are not decoded by
       the standard resource manager routines */

    if (!Settings::shellCommands.isNull()) {
        LoadShellCmdsStringEx(Settings::shellCommands);
    }
    if (!Settings::macroCommands.isNull()) {
        LoadMacroCmdsStringEx(Settings::macroCommands);
    }
    if (!Settings::bgMenuCommands.isNull()) {
        LoadBGMenuCmdsStringEx(Settings::bgMenuCommands);
    }
    if (!Settings::highlightPatterns.isNull()) {
        LoadHighlightStringEx(Settings::highlightPatterns);
    }
    if (!Settings::languageModes.isNull()) {
        loadLanguageModesStringEx(Settings::languageModes);
    }
    if (!Settings::smartIndentInit.isNull()) {
        LoadSmartIndentStringEx(Settings::smartIndentInit);
    }
    if (!Settings::smartIndentInitCommon.isNull()) {
        LoadSmartIndentCommonStringEx(Settings::smartIndentInitCommon);
    }

    LoadTheme();

    // translate the font names into fontLists suitable for the text widget
    Settings::plainFontStruct      = Font::fromString(Settings::textFont);
    Settings::boldFontStruct       = Font::fromString(Settings::boldHighlightFont);
    Settings::italicFontStruct     = Font::fromString(Settings::italicHighlightFont);
    Settings::boldItalicFontStruct = Font::fromString(Settings::boldItalicHighlightFont);

    /*
    **  The default set for the comand shell in PrefDescrip ("DEFAULT") is
    **  only a place-holder, the actual default is the user's login shell
    **  (or whatever is implemented in getDefaultShell()). We put the login
    **  shell's name in PrefData here.
    */
    if (Settings::shell == QLatin1String("DEFAULT")) {
        Settings::shell = getDefaultShell();
    }

    /* setup language mode dependent info of user menus (to increase
       performance when switching between documents of different
       language modes) */
    SetupUserMenuInfo();
}

QStringList Preferences::readExtensionListEx(Input &in) {
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

/*
** Short-hand error processing for language mode parsing errors, frees
** lm (if non-null), prints a formatted message explaining where the
** error is, and returns false;
*/
bool Preferences::modeErrorEx(const Input &in, const QString &message) {
    return ParseErrorEx(
                nullptr,
                *in.string(),
                in.index(),
                tr("language mode specification"),
                message);
}

/*
**  This function passes up a pointer to the static name of the default
**  shell, currently defined as the user's login shell.
**  In case of errors, the fallback of "sh" will be returned.
*/
QString Preferences::getDefaultShell() {
    struct passwd *passwdEntry = getpwuid(getuid()); //  getuid() never fails.

    if (!passwdEntry) {
        //  Something bad happened! Do something, quick!
        perror("NEdit: Failed to get passwd entry (falling back to 'sh')");
        return QLatin1String("sh");
    }

    return QString::fromUtf8(passwdEntry->pw_shell);
}
