
#ifndef PREFERENCES_H_
#define PREFERENCES_H_

#include "IndentStyle.h"
#include "SearchType.h"
#include "ShowMatchingStyle.h"
#include "TruncSubstitution.h"
#include "Util/QtHelper.h"
#include "Verbosity.h"
#include "WrapMode.h"
#include "WrapStyle.h"

#include <vector>

class Input;
class LanguageMode;

class QDialog;
class QFont;
class QString;
class QWidget;

enum ColorTypes : int;

namespace Preferences {
Q_DECLARE_NAMESPACE_TR(Preferences)

size_t FindLanguageMode(const QString &languageName);
QString LanguageModeName(size_t mode);

bool GetPrefAlwaysCheckRelTagsSpecs();
bool GetPrefAppendLF();
bool GetPrefAutoSave();
bool GetPrefAutoScroll();
bool GetPrefAutoWrapPastedText();
bool GetPrefBacklightChars();
bool GetPrefBeepOnSearchWrap();
bool GetPrefColorizeHighlightedText();
bool GetPrefFindReplaceUsesSelection();
bool GetPrefFocusOnRaise();
bool GetPrefForceOSConversion();
bool GetPrefGlobalTabNavigate();
bool GetPrefHeavyCursor();
bool GetPrefHighlightSyntax();
bool GetPrefHonorSymlinks();
bool GetPrefISearchLine();
bool GetPrefKeepSearchDlogs();
bool GetPrefLineNums();
bool GetPrefMatchSyntaxBased();
bool GetPrefOpenInTab();
bool GetPrefRepositionDialogs();
bool GetPrefSaveOldVersion();
bool GetPrefSearchDlogs();
bool GetPrefShowPathInWindowsMenu();
bool GetPrefShowResizeNotification();
bool GetPrefSmartHome();
bool GetPrefSmartTags();
bool GetPrefSortOpenPrevMenu();
bool GetPrefSortTabs();
bool GetPrefStatsLine();
bool GetPrefStickyCaseSenseBtn();
bool GetPrefTabBar();
bool GetPrefTabBarHideOne();
bool GetPrefToolTips();
bool GetPrefTypingHidesPointer();
bool GetPrefUndoModifiesSelection();
bool GetPrefWarnExit();
bool GetPrefWarnFileMods();
bool GetPrefWarnRealFileMods();
bool GetPrefWarnAlways();
bool GetPrefWarnWhenLocalMods();
bool GetPrefWarnWhenUserInteracts();
IndentStyle GetPrefAutoIndent(size_t langMode);
int GetPrefAutoSaveCharLimit();
int GetPrefAutoSaveOpLimit();
int GetPrefCols();
int GetPrefEmTabDist(size_t langMode);
int GetPrefInsertTabs(size_t langMode);
int GetPrefMaxPrevOpenFiles();
int GetPrefRows();
int GetPrefTabDist(size_t langMode);
int GetPrefWrapMargin();
int GetVerticalAutoScroll();
QFont GetPrefDefaultFont();
QString GetPrefBacklightCharTypes();
QString GetPrefColorName(ColorTypes index);
QString GetPrefDelimiters();
QString GetPrefFontName();
QString GetPrefGeometry();
QString GetPrefServerName();
QString GetPrefShell();
QString GetPrefTagFile();
QString GetPrefTitleFormat();
QStringList GetPrefIncludePaths();
SearchType GetPrefSearch();
ShowMatchingStyle GetPrefShowMatching();
TruncSubstitution GetPrefTruncSubstitution();
WrapMode GetPrefSearchWraps();
WrapStyle GetPrefWrap(size_t langMode);

void SetPrefAppendLF(bool state);
void SetPrefAutoIndent(IndentStyle state);
void SetPrefAutoSave(bool state);
void SetPrefAutoScroll(bool state);
void SetPrefBacklightChars(bool state);
void SetPrefBeepOnSearchWrap(bool state);
void SetPrefColorName(ColorTypes index, const QString &name);
void SetPrefCols(int nCols);
void SetPrefEmTabDist(int tabDist);
void SetPrefFindReplaceUsesSelection(bool state);
void SetPrefFocusOnRaise(bool);
void SetPrefFont(const QString &fontName);
void SetPrefGlobalTabNavigate(bool state);
void SetPrefHighlightSyntax(bool state);
void SetPrefInsertTabs(bool state);
void SetPrefISearchLine(bool state);
void SetPrefKeepSearchDlogs(bool state);
void SetPrefLineNums(bool state);
void SetPrefMatchSyntaxBased(bool state);
void SetPrefOpenInTab(bool state);
void SetPrefRepositionDialogs(bool state);
void SetPrefRows(int nRows);
void SetPrefSaveOldVersion(bool state);
void SetPrefSearchDlogs(bool state);
void SetPrefSearch(SearchType searchType);
void SetPrefSearchWraps(bool state);
void SetPrefShell(const QString &shell);
void SetPrefShowMatching(ShowMatchingStyle state);
void SetPrefShowPathInWindowsMenu(bool state);
void SetPrefSmartTags(bool state);
void SetPrefSortOpenPrevMenu(bool state);
void SetPrefSortTabs(bool state);
void SetPrefStatsLine(bool state);
void SetPrefTabBarHideOne(bool state);
void SetPrefTabBar(bool state);
void SetPrefTabDist(int tabDist);
void SetPrefTitleFormat(const QString &format);
void SetPrefToolTips(bool state);
void SetPrefUndoModifiesSelection(bool);
void SetPrefWarnExit(bool state);
void SetPrefWarnFileMods(bool state);
void SetPrefWarnRealFileMods(bool state);
void SetPrefWarnAlways(bool state);
void SetPrefWarnWhenLocalMods(bool state);
void SetPrefWarnWhenUserInteracts(bool state);
void SetPrefWrap(WrapStyle state);
void SetPrefWrapMargin(int margin);

bool reportError(QWidget *toDialog, const QString &string, int stoppedAt, const QString &errorIn, const QString &message);
bool ReadNumericField(Input &in, int *value);
bool ReadQuotedString(Input &in, QString *errMsg, QString *string);
bool SkipDelimiter(Input &in, QString *errMsg);

QString MakeQuotedString(const QString &string);
QString ReadSymbolicField(Input &input);
void ImportPrefFile(const QString &filename);
void MarkPrefsChanged();
void RestoreNEditPrefs();
void SaveNEditPrefs(QWidget *parent, Verbosity verbosity);
bool PreferencesChanged();
QString ImportedSettingsFile();

extern std::vector<LanguageMode> LanguageModes;
}

#endif
