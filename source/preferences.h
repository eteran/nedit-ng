/*******************************************************************************
*                                                                              *
* preference.h -- Nirvana Editor Preferences Header File                       *
*                                                                              *
* Copyright 2004 The NEdit Developers                                          *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute versions of this program linked to  *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for    *
* more details.                                                                *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* July 31, 2001                                                                *
*                                                                              *
*******************************************************************************/

#ifndef PREFERENCES_H_
#define PREFERENCES_H_

#include "IndentStyle.h"
#include "SearchType.h"
#include "WrapStyle.h"
#include "WrapMode.h"
#include "Settings.h"
#include "util/string_view.h"

#include <QList>

class QWidget;
class QString;
class LanguageMode;
class DocumentWidget;
class QFont;
class QDialog;
class Input;
class Settings;

constexpr int PLAIN_LANGUAGE_MODE = -1;

bool GetPrefFocusOnRaise();
bool GetPrefForceOSConversion();
bool GetPrefHighlightSyntax();
bool GetPrefHonorSymlinks();
bool GetPrefUndoModifiesSelection();
QString GetPrefBacklightCharTypes();
QString GetPrefBoldFontName();
QString GetPrefBoldItalicFontName();
QString GetPrefColorName(ColorTypes index);
QString GetPrefDelimiters();
QString GetPrefFontName();
QString GetPrefGeometry();
QString GetPrefItalicFontName();
QString GetPrefServerName();
QString GetPrefTagFile();
QString GetPrefTooltipBgColor();
QString GetWindowDelimitersEx(const DocumentWidget *window);
QString LanguageModeName(int mode);
QString GetPrefShell();
QString GetPrefTitleFormat();
int FindLanguageMode(const QString &languageName);
int FindLanguageMode(const QStringRef &languageName);
int GetPrefAlwaysCheckRelTagsSpecs();
int GetPrefAppendLF();
IndentStyle GetPrefAutoIndent(int langMode);
bool GetPrefAutoSave();
int GetPrefAutoScroll();
int GetPrefBacklightChars();
bool GetPrefBeepOnSearchWrap();
int GetPrefCols();
int GetPrefEmTabDist(int langMode);
int GetPrefFindReplaceUsesSelection();
int GetPrefGlobalTabNavigate();
int GetPrefInsertTabs();
int GetPrefISearchLine();
bool GetPrefKeepSearchDlogs();
int GetPrefLineNums();
int GetPrefMapDelete();
int GetPrefMatchSyntaxBased();
int GetPrefMaxPrevOpenFiles();
bool GetPrefOpenInTab();
int GetPrefOverrideVirtKeyBindings();
bool GetPrefRepositionDialogs();
int GetPrefRows();
bool GetPrefSaveOldVersion();
bool GetPrefSearchDlogs();
SearchType GetPrefSearch();
WrapMode GetPrefSearchWraps();
int GetPrefShowMatching();
int GetPrefShowPathInWindowsMenu();
int GetPrefSmartTags();
int GetPrefSortOpenPrevMenu();
int GetPrefSortTabs();
int GetPrefStatsLine();
int GetPrefStdOpenDialog();
int GetPrefStickyCaseSenseBtn();
int GetPrefTabBarHideOne();
int GetPrefTabBar();
int GetPrefTabDist(int langMode);
int GetPrefToolTips();
int GetPrefTruncSubstitution();
int GetPrefTypingHidesPointer();
int GetPrefWarnExit();
int GetPrefWarnFileMods();
int GetPrefWarnRealFileMods();
WrapStyle GetPrefWrap(int langMode);
int GetPrefWrapMargin();
int GetVerticalAutoScroll();
bool ParseErrorEx(QWidget *toDialog, const QString &string, int stoppedAt, const QString &errorIn, const QString &message);

Settings &GetSettings();

bool ReadNumericFieldEx(Input &in, int *value);
bool ReadQuotedStringEx(Input &in, QString *errMsg, QString *string);
bool SkipDelimiterEx(Input &in, QString *errMsg);

int SkipOptSeparatorEx(QChar separator, Input &in);
QString MakeQuotedStringEx(const QString &string);
QString ReadSymbolicFieldEx(Input &input);
void ImportPrefFile(const QString &filename, bool convertOld);
void MarkPrefsChanged();
void RestoreNEditPrefs();
void SaveNEditPrefsEx(QWidget *parent, bool quietly);
void SetPrefAppendLF(bool state);
void SetPrefAutoIndent(int state);
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
void SetPrefShowMatching(int state);
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

#if defined(REPLACE_SCOPE)
void SetPrefReplaceDefScope(int scope);
int GetPrefReplaceDefScope();
#endif

extern QString ImportedFile;
extern bool PrefsHaveChanged;
extern QList<LanguageMode> LanguageModes;

#endif
