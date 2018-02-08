
#ifndef PREFERENCES_H_
#define PREFERENCES_H_

#include "IndentStyle.h"
#include "SearchType.h"
#include "ShowMatchingStyle.h"
#include "TruncSubstitution.h"
#include "WrapMode.h"
#include "WrapStyle.h"

#include <vector>

class Input;
class LanguageMode;
class Settings;

class QDialog;
class QFont;
class QString;
class QWidget;

enum ColorTypes : int;

bool GetPrefColorizeHighlightedText();
bool GetPrefAlwaysCheckRelTagsSpecs();
bool GetPrefAppendLF();
bool GetPrefAutoSave();
bool GetPrefAutoScroll();
bool GetPrefBacklightChars();
bool GetPrefBeepOnSearchWrap();
bool GetPrefFindReplaceUsesSelection();
bool GetPrefFocusOnRaise();
bool GetPrefForceOSConversion();
bool GetPrefHighlightSyntax();
bool GetPrefHonorSymlinks();
bool GetPrefKeepSearchDlogs();
bool GetPrefOpenInTab();
bool GetPrefRepositionDialogs();
bool GetPrefSaveOldVersion();
bool GetPrefSearchDlogs();
bool GetPrefSortTabs();
bool GetPrefTabBar();
bool GetPrefUndoModifiesSelection();
bool GetPrefWarnExit();
IndentStyle GetPrefAutoIndent(size_t langMode);
size_t FindLanguageMode(const QString &languageName);
size_t FindLanguageMode(const QStringRef &languageName);
int GetPrefCols();
int GetPrefEmTabDist(size_t langMode);
int GetPrefGlobalTabNavigate();
int GetPrefInsertTabs();
int GetPrefISearchLine();
int GetPrefLineNums();
int GetPrefMapDelete();
int GetPrefMatchSyntaxBased();
int GetPrefMaxPrevOpenFiles();
int GetPrefRows();
int GetPrefShowPathInWindowsMenu();
int GetPrefSmartTags();
int GetPrefSortOpenPrevMenu();
int GetPrefStatsLine();
int GetPrefStdOpenDialog();
int GetPrefStickyCaseSenseBtn();
int GetPrefTabBarHideOne();
int GetPrefTabDist(size_t langMode);
bool GetPrefToolTips();
bool GetPrefTypingHidesPointer();
bool GetPrefAutoWrapPastedText();
bool GetPrefHeavyCursor();
int GetPrefWarnFileMods();
int GetPrefWarnRealFileMods();
int GetPrefWrapMargin();
int GetVerticalAutoScroll();
QString GetPrefBacklightCharTypes();
QString GetPrefBoldFontName();
QString GetPrefBoldItalicFontName();
QString GetPrefColorName(ColorTypes index);
QString GetPrefDelimiters();
QString GetPrefFontName();
QString GetPrefGeometry();
QString GetPrefItalicFontName();
QString GetPrefServerName();
QString GetPrefShell();
QString GetPrefTagFile();
QString GetPrefTitleFormat();
QString LanguageModeName(size_t mode);
SearchType GetPrefSearch();
ShowMatchingStyle GetPrefShowMatching();
TruncSubstitution GetPrefTruncSubstitution();
WrapMode GetPrefSearchWraps();
WrapStyle GetPrefWrap(size_t langMode);

bool ParseErrorEx(QWidget *toDialog, const QString &string, int stoppedAt, const QString &errorIn, const QString &message);

Settings &GetSettings();

bool ReadNumericFieldEx(Input &in, int *value);
bool ReadQuotedStringEx(Input &in, QString *errMsg, QString *string);
bool SkipDelimiterEx(Input &in, QString *errMsg);

QString MakeQuotedStringEx(const QString &string);
QString ReadSymbolicFieldEx(Input &input);
void ImportPrefFile(const QString &filename);
void MarkPrefsChanged();
void RestoreNEditPrefs();
void SaveNEditPrefsEx(QWidget *parent, bool quietly);
void SetPrefAppendLF(bool state);
void SetPrefAutoIndent(IndentStyle state);
void SetPrefAutoSave(bool state);
void SetPrefAutoScroll(bool state);
void SetPrefBacklightChars(bool state);
void SetPrefBeepOnSearchWrap(bool state);
void SetPrefBoldFont(const QString &fontName);
void SetPrefBoldItalicFont(const QString &fontName);
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
void SetPrefItalicFont(const QString &fontName);
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
void SetPrefWrap(WrapStyle state);
void SetPrefWrapMargin(int margin);

QFont GetPrefDefaultFont();
QFont GetPrefBoldFont();
QFont GetPrefBoldItalicFont();
QFont GetPrefItalicFont();

bool PreferencesChanged();
QString ImportedSettingsFile();

extern std::vector<LanguageMode> LanguageModes;

#endif
