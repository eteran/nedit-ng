
#ifndef PREFERENCES_H_
#define PREFERENCES_H_

#include "IndentStyle.h"
#include "SearchType.h"
#include "ShowMatchingStyle.h"
#include "TruncSubstitution.h"
#include "WrapMode.h"
#include "WrapStyle.h"

#include <QCoreApplication>
#include <vector>

class Input;
class LanguageMode;
class Settings;

class QDialog;
class QFont;
class QString;
class QWidget;

enum ColorTypes : int;

class Preferences  {
    Q_DECLARE_TR_FUNCTIONS(Preferences)

public:
    Preferences() = delete;

public:
    static size_t FindLanguageMode(const QString &languageName);
    static QString LanguageModeName(size_t mode);

public:
    static bool GetPrefColorizeHighlightedText();
    static bool GetPrefAlwaysCheckRelTagsSpecs();
    static bool GetPrefAppendLF();
    static bool GetPrefAutoSave();
    static bool GetPrefAutoScroll();
    static bool GetPrefBacklightChars();
    static bool GetPrefBeepOnSearchWrap();
    static bool GetPrefFindReplaceUsesSelection();
    static bool GetPrefFocusOnRaise();
    static bool GetPrefForceOSConversion();
    static bool GetPrefHighlightSyntax();
    static bool GetPrefHonorSymlinks();
    static bool GetPrefKeepSearchDlogs();
    static bool GetPrefOpenInTab();
    static bool GetPrefRepositionDialogs();
    static bool GetPrefSaveOldVersion();
    static bool GetPrefSearchDlogs();
    static bool GetPrefSortTabs();
    static bool GetPrefTabBar();
    static bool GetPrefUndoModifiesSelection();
    static bool GetPrefWarnExit();
    static IndentStyle GetPrefAutoIndent(size_t langMode);
    static int GetPrefCols();
    static int GetPrefEmTabDist(size_t langMode);
    static int GetPrefGlobalTabNavigate();
    static int GetPrefInsertTabs();
    static int GetPrefISearchLine();
    static int GetPrefLineNums();
    static int GetPrefMatchSyntaxBased();
    static int GetPrefMaxPrevOpenFiles();
    static int GetPrefRows();
    static int GetPrefShowPathInWindowsMenu();
    static int GetPrefSmartTags();
    static int GetPrefSortOpenPrevMenu();
    static int GetPrefStatsLine();
    static int GetPrefStickyCaseSenseBtn();
    static int GetPrefTabBarHideOne();
    static int GetPrefTabDist(size_t langMode);
    static bool GetPrefToolTips();
    static bool GetPrefTypingHidesPointer();
    static bool GetPrefAutoWrapPastedText();
    static bool GetPrefHeavyCursor();
    static bool GetPrefWarnFileMods();
    static bool GetPrefWarnRealFileMods();
    static int GetPrefWrapMargin();
    static int GetVerticalAutoScroll();
    static QString GetPrefBacklightCharTypes();
    static QString GetPrefBoldFontName();
    static QString GetPrefBoldItalicFontName();
    static QString GetPrefColorName(ColorTypes index);
    static QString GetPrefDelimiters();
    static QString GetPrefFontName();
    static QString GetPrefGeometry();
    static QString GetPrefItalicFontName();
    static QString GetPrefServerName();
    static QString GetPrefShell();
    static QString GetPrefTagFile();
    static QString GetPrefTitleFormat();
    static SearchType GetPrefSearch();
    static ShowMatchingStyle GetPrefShowMatching();
    static TruncSubstitution GetPrefTruncSubstitution();
    static WrapMode GetPrefSearchWraps();
    static WrapStyle GetPrefWrap(size_t langMode);
    static QFont GetPrefDefaultFont();
    static QFont GetPrefBoldFont();
    static QFont GetPrefBoldItalicFont();
    static QFont GetPrefItalicFont();

public:
    static void SetPrefAppendLF(bool state);
    static void SetPrefAutoIndent(IndentStyle state);
    static void SetPrefAutoSave(bool state);
    static void SetPrefAutoScroll(bool state);
    static void SetPrefBacklightChars(bool state);
    static void SetPrefBeepOnSearchWrap(bool state);
    static void SetPrefBoldFont(const QString &fontName);
    static void SetPrefBoldItalicFont(const QString &fontName);
    static void SetPrefColorName(ColorTypes index, const QString &name);
    static void SetPrefCols(int nCols);
    static void SetPrefEmTabDist(int tabDist);
    static void SetPrefFindReplaceUsesSelection(bool state);
    static void SetPrefFocusOnRaise(bool);
    static void SetPrefFont(const QString &fontName);
    static void SetPrefGlobalTabNavigate(bool state);
    static void SetPrefHighlightSyntax(bool state);
    static void SetPrefInsertTabs(bool state);
    static void SetPrefISearchLine(bool state);
    static void SetPrefItalicFont(const QString &fontName);
    static void SetPrefKeepSearchDlogs(bool state);
    static void SetPrefLineNums(bool state);
    static void SetPrefMatchSyntaxBased(bool state);
    static void SetPrefOpenInTab(bool state);
    static void SetPrefRepositionDialogs(bool state);
    static void SetPrefRows(int nRows);
    static void SetPrefSaveOldVersion(bool state);
    static void SetPrefSearchDlogs(bool state);
    static void SetPrefSearch(SearchType searchType);
    static void SetPrefSearchWraps(bool state);
    static void SetPrefShell(const QString &shell);
    static void SetPrefShowMatching(ShowMatchingStyle state);
    static void SetPrefShowPathInWindowsMenu(bool state);
    static void SetPrefSmartTags(bool state);
    static void SetPrefSortOpenPrevMenu(bool state);
    static void SetPrefSortTabs(bool state);
    static void SetPrefStatsLine(bool state);
    static void SetPrefTabBarHideOne(bool state);
    static void SetPrefTabBar(bool state);
    static void SetPrefTabDist(int tabDist);
    static void SetPrefTitleFormat(const QString &format);
    static void SetPrefToolTips(bool state);
    static void SetPrefUndoModifiesSelection(bool);
    static void SetPrefWarnExit(bool state);
    static void SetPrefWarnFileMods(bool state);
    static void SetPrefWarnRealFileMods(bool state);
    static void SetPrefWrap(WrapStyle state);
    static void SetPrefWrapMargin(int margin);

public:
    static bool ParseErrorEx(QWidget *toDialog, const QString &string, int stoppedAt, const QString &errorIn, const QString &message);
    static bool ReadNumericField(Input &in, int *value);
    static bool ReadQuotedString(Input &in, QString *errMsg, QString *string);
    static bool SkipDelimiter(Input &in, QString *errMsg);

public:
    static QString MakeQuotedString(const QString &string);
    static QString ReadSymbolicField(Input &input);
    static void ImportPrefFile(const QString &filename);
    static void MarkPrefsChanged();
    static void RestoreNEditPrefs();
    static void SaveNEditPrefs(QWidget *parent, bool quietly);
    static bool PreferencesChanged();
    static QString ImportedSettingsFile();

private:
    static int loadLanguageModesString(const QString &string);
    static QString WriteLanguageModesStringEx();
    static void translatePrefFormats(uint32_t fileVer);
    static QStringList readExtensionListEx(Input &in);
    static QString getDefaultShell();

public:
    static std::vector<LanguageMode> LanguageModes;
};



#endif
