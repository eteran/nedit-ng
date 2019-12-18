
#include "Preferences.h"
#include "DocumentWidget.h"
#include "Font.h"
#include "Highlight.h"
#include "LanguageMode.h"
#include "MainWindow.h"
#include "Settings.h"
#include "SmartIndent.h"
#include "Tags.h"
#include "TextBuffer.h"
#include "Theme.h"
#include "Util/ClearCase.h"
#include "Util/Input.h"
#include "Util/Resource.h"
#include "Util/algorithm.h"
#include "Util/version.h"
#include "nedit.h"
#include "search.h"
#include "userCmds.h"

#include <yaml-cpp/yaml.h>

#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QString>
#include <QtDebug>

#include <cctype>
#include <fstream>
#include <iostream>
#include <memory>

#ifdef Q_OS_UNIX
#include <pwd.h>
#include <unistd.h>
#endif

namespace Preferences {
namespace {

const QLatin1String AutoWrapTypes[] = {
	QLatin1String("None"),
	QLatin1String("Newline"),
	QLatin1String("Continuous")};

const QLatin1String AutoIndentTypes[] = {
	QLatin1String("None"),
	QLatin1String("Auto"),
	QLatin1String("Smart")};

/* Module-global variable set when any preference changes (for asking the
   user about re-saving on exit) */
bool PrefsHaveChanged = false;

/* Module-global variable set when user uses -import to load additional
   preferences on top of the defaults.  Contains name of file loaded */
QString ImportedFile;

/**
 * @brief readExtensionList
 * @param in
 * @return
 */
QStringList readExtensionList(Input &in) {
	QStringList extensionList;

	// skip over blank space
	in.skipWhitespace();

	while (!in.atEnd() && *in != QLatin1Char(':')) {

		in.skipWhitespace();

		Input strStart = in;

		while (!in.atEnd() && *in != QLatin1Char(' ') && *in != QLatin1Char('\t') && *in != QLatin1Char(':')) {
			++in;
		}

		int len     = in - strStart;
		QString ext = strStart.mid(len);
		extensionList.push_back(ext);
	}

	return extensionList;
}

/*
**  This function returns the user's login shell.
**  In case of errors, the fallback of "sh" will be returned.
*/
QString getDefaultShell() {
#ifdef Q_OS_UNIX
	struct passwd *passwdEntry = getpwuid(getuid()); //  getuid() never fails.

	if (!passwdEntry) {
		//  Something bad happened! Do something, quick!
		perror("NEdit: Failed to get passwd entry (falling back to 'sh')");
		return QLatin1String("sh");
	}

	return QString::fromUtf8(passwdEntry->pw_shell);
#else
	return QString();
#endif
}

boost::optional<LanguageMode> readLanguageModeYaml(const YAML::Node &language) {

	struct ModeError {
		QString message;
	};

	try {
		LanguageMode lm;
		for (auto it = language.begin(); it != language.end(); ++it) {

			const auto &key         = it->first.as<std::string>();
			const YAML::Node &value = it->second;

			if (key == "name") {
				lm.name = QString::fromUtf8(value.as<std::string>().c_str());
			} else if (key == "delimiters") {
				lm.delimiters = QString::fromUtf8(value.as<std::string>().c_str());
			} else if (key == "tab_distance") {
				lm.tabDist = value.as<int>();
			} else if (key == "em_tab_distance") {
				lm.emTabDist = value.as<int>();
			} else if (key == "regex") {
				lm.recognitionExpr = QString::fromUtf8(value.as<std::string>().c_str());
			} else if (key == "wrap") {
				const auto &string_val = QString::fromUtf8(value.as<std::string>().c_str());
				auto it                = std::find(std::begin(AutoWrapTypes), std::end(AutoWrapTypes), string_val);
				if (it == std::end(AutoWrapTypes)) {
					Raise<ModeError>(tr("unrecognized wrap style"));
				}

				lm.wrapStyle = static_cast<WrapStyle>(it - std::begin(AutoWrapTypes));
			} else if (key == "indent") {
				const auto &string_val = QString::fromUtf8(value.as<std::string>().c_str());
				auto it                = std::find(std::begin(AutoIndentTypes), std::end(AutoIndentTypes), string_val);
				if (it == std::end(AutoIndentTypes)) {
					Raise<ModeError>(tr("unrecognized indent style"));
				}

				lm.indentStyle = static_cast<IndentStyle>(it - std::begin(AutoIndentTypes));
			} else if (key == "default_tips") {
				lm.defTipsFile = QString::fromUtf8(value.as<std::string>().c_str());
			} else if (key == "extensions") {
				QStringList extensions;
				for (size_t i = 0; i < value.size(); i++) {
					const YAML::Node &extension = value[i];
					extensions.push_back(QString::fromUtf8(extension.as<std::string>().c_str()));
				}
				lm.extensions = extensions;
			}
		}

		if (lm.name.isEmpty()) {
			Raise<ModeError>(tr("language entry has no name."));
		}

		return lm;
	} catch (const YAML::Exception &ex) {
		qWarning("NEdit: Invalid YAML:\n%s", ex.what());
	} catch (const ModeError &ex) {
		qWarning("NEdit: %s", qPrintable(ex.message));
	}

	return boost::none;
}

boost::optional<LanguageMode> readLanguageMode(Input &in) {

	struct ModeError {
		QString message;
	};

	try {
		QString errMsg;

		LanguageMode lm;

		// skip over blank space
		in.skipWhitespaceNL();

		// read language mode name
		const QString name = ReadSymbolicField(in);
		if (name.isNull()) {
			Raise<ModeError>(tr("language mode name required"));
		}

		lm.name = name;

		if (!SkipDelimiter(in, &errMsg)) {
			Raise<ModeError>(errMsg);
		}

		// read list of extensions
		lm.extensions = readExtensionList(in);
		if (!SkipDelimiter(in, &errMsg)) {
			Raise<ModeError>(errMsg);
		}

		// read the recognition regular expression
		QString recognitionExpr;
		if (in.atEnd() || *in == QLatin1Char('\n') || *in == QLatin1Char(':')) {
			recognitionExpr = QString();
		} else if (!ReadQuotedString(in, &errMsg, &recognitionExpr)) {
			Raise<ModeError>(errMsg);
		}

		lm.recognitionExpr = recognitionExpr;

		if (!SkipDelimiter(in, &errMsg)) {
			Raise<ModeError>(errMsg);
		}

		// read the indent style
		const QString styleName = ReadSymbolicField(in);
		if (styleName.isNull()) {
			lm.indentStyle = IndentStyle::Default;
		} else {
			auto it = std::find(std::begin(AutoIndentTypes), std::end(AutoIndentTypes), styleName);
			if (it == std::end(AutoIndentTypes)) {
				Raise<ModeError>(tr("unrecognized indent style"));
			}

			lm.indentStyle = static_cast<IndentStyle>(it - std::begin(AutoIndentTypes));
		}

		if (!SkipDelimiter(in, &errMsg)) {
			Raise<ModeError>(errMsg);
		}

		// read the wrap style
		const QString wrapStyle = ReadSymbolicField(in);
		if (wrapStyle.isNull()) {
			lm.wrapStyle = WrapStyle::Default;
		} else {
			auto it = std::find(std::begin(AutoWrapTypes), std::end(AutoWrapTypes), wrapStyle);
			if (it == std::end(AutoWrapTypes)) {
				Raise<ModeError>(tr("unrecognized wrap style"));
			}

			lm.wrapStyle = static_cast<WrapStyle>(it - std::begin(AutoWrapTypes));
		}

		if (!SkipDelimiter(in, &errMsg)) {
			Raise<ModeError>(errMsg);
		}

		// read the tab distance
		if (in.atEnd() || *in == QLatin1Char('\n') || *in == QLatin1Char(':')) {
			lm.tabDist = LanguageMode::DEFAULT_TAB_DIST;
		} else if (!ReadNumericField(in, &lm.tabDist)) {
			Raise<ModeError>(tr("bad tab spacing"));
		}

		if (!SkipDelimiter(in, &errMsg)) {
			Raise<ModeError>(errMsg);
		}

		// read emulated tab distance
		if (in.atEnd() || *in == QLatin1Char('\n') || *in == QLatin1Char(':')) {
			lm.emTabDist = LanguageMode::DEFAULT_EM_TAB_DIST;
		} else if (!ReadNumericField(in, &lm.emTabDist)) {
			Raise<ModeError>(tr("bad emulated tab spacing"));
		}

		if (!SkipDelimiter(in, &errMsg)) {
			Raise<ModeError>(errMsg);
		}

		// read the delimiters string
		QString delimiters;
		if (in.atEnd() || *in == QLatin1Char('\n') || *in == QLatin1Char(':')) {
			delimiters = QString();
		} else if (!ReadQuotedString(in, &errMsg, &delimiters)) {
			Raise<ModeError>(errMsg);
		}

		lm.delimiters = delimiters;

		// After 5.3 all language modes need a default tips file field
		if (!SkipDelimiter(in, &errMsg)) {
			Raise<ModeError>(errMsg);
		}

		// read the default tips file
		QString defTipsFile;
		if (in.atEnd() || *in == QLatin1Char('\n')) {
			defTipsFile = QString();
		} else if (!ReadQuotedString(in, &errMsg, &defTipsFile)) {
			Raise<ModeError>(errMsg);
		}

		lm.defTipsFile = defTipsFile;

		return lm;
	} catch (const ModeError &error) {
		reportError(
			nullptr,
			*in.string(),
			in.index(),
			tr("language mode specification"),
			error.message);
	}

	return boost::none;
}

void loadLanguageModesString(const QString &string) {

	if (string == QLatin1String("*")) {

		YAML::Node languages;

		const QString languageModeFile = Settings::languageModeFile();
		if (QFileInfo(languageModeFile).exists()) {
			languages = YAML::LoadAllFromFile(languageModeFile.toUtf8().data());
		} else {
			static QByteArray defaultLanguageModes = loadResource(QLatin1String("DefaultLanguageModes.yaml"));
			languages                              = YAML::LoadAll(defaultLanguageModes.data());
		}

		for (const YAML::Node &language : languages) {

			boost::optional<LanguageMode> lm = readLanguageModeYaml(language);
			if (!lm) {
				break;
			}

			insert_or_replace(LanguageModes, *lm, [&lm](const LanguageMode &languageMode) {
				return languageMode.name == lm->name;
			});
		}
	} else {
		Input in(&string);

		Q_FOREVER {
			boost::optional<LanguageMode> lm = readLanguageMode(in);
			if (!lm) {
				break;
			}

			insert_or_replace(LanguageModes, *lm, [&lm](const LanguageMode &languageMode) {
				return languageMode.name == lm->name;
			});

			// if the string ends here, we're done
			in.skipWhitespaceNL();

			if (in.atEnd()) {
				break;
			}
		}
	}
}

/*
** Many of of NEdit's preferences are much more complicated than just simple
** integers or strings.  These are read as strings, but must be parsed and
** translated into something meaningful.  This routine does the translation.
**
** In addition this function covers settings that, while simple, require
** additional steps before they can be published.
*/
void translatePrefFormats(uint32_t fileVer) {

	Q_UNUSED(fileVer)

	/* Parse the strings which represent types which are not decoded by
	   the standard resource manager routines */

	if (!Settings::shellCommands.isNull()) {
		load_shell_commands_string(Settings::shellCommands);
	}
	if (!Settings::macroCommands.isNull()) {
		load_macro_commands_string(Settings::macroCommands);
	}
	if (!Settings::bgMenuCommands.isNull()) {
		load_bg_menu_commands_string(Settings::bgMenuCommands);
	}
	if (!Settings::highlightPatterns.isNull()) {
		Highlight::LoadHighlightString(Settings::highlightPatterns);
	}
	if (!Settings::languageModes.isNull()) {
		loadLanguageModesString(Settings::languageModes);
	}
	if (!Settings::smartIndentInit.isNull()) {
		SmartIndent::loadSmartIndentString(Settings::smartIndentInit);
	}
	if (!Settings::smartIndentInitCommon.isNull()) {
		SmartIndent::loadSmartIndentCommonString(Settings::smartIndentInitCommon);
	}

	Theme::load();

	// translate the font names into QFont suitable for the text widget
	Settings::font = Font::fromString(Settings::fontName);

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
	setup_user_menu_info();
}

/**
 * @brief WriteLanguageModesString
 * @return
 */
QString WriteLanguageModesString() {

	const QString filename = Settings::languageModeFile();

	try {
		YAML::Emitter out;
		for (const LanguageMode &lang : LanguageModes) {

			out << YAML::BeginDoc;
			out << YAML::BeginMap;
			out << YAML::Key << "name" << YAML::Value << lang.name.toUtf8().data();

			if (!lang.extensions.empty()) {
				out << YAML::Key << "extensions" << YAML::Value << YAML::BeginSeq;
				for (const QString &s : lang.extensions) {
					out << s.toUtf8().data();
				}
				out << YAML::EndSeq;
			}

			if (!lang.delimiters.isEmpty()) {
				out << YAML::Key << "delimiters" << YAML::Value << lang.delimiters.toUtf8().data();
			}

			if (lang.tabDist != LanguageMode::DEFAULT_TAB_DIST) {
				out << YAML::Key << "tab_distance" << YAML::Value << lang.tabDist;
			}

			if (lang.tabDist != LanguageMode::DEFAULT_EM_TAB_DIST) {
				out << YAML::Key << "em_tab_distance" << YAML::Value << lang.emTabDist;
			}

			if (!lang.recognitionExpr.isEmpty()) {
				out << YAML::Key << "regex" << YAML::Value << lang.recognitionExpr.toUtf8().data();
			}

			if (lang.wrapStyle != WrapStyle::Default) {
				out << YAML::Key << "wrap" << YAML::Value << AutoWrapTypes[static_cast<int>(lang.wrapStyle)].data();
			}

			if (lang.indentStyle != IndentStyle::Default) {
				out << YAML::Key << "indent" << YAML::Value << AutoIndentTypes[static_cast<int>(lang.indentStyle)].data();
			}

			if (!lang.defTipsFile.isEmpty()) {
				out << YAML::Key << "default_tips" << YAML::Value << lang.defTipsFile.toUtf8().data();
			}
			out << YAML::EndMap;
		}

		QFile file(filename);
		if (file.open(QIODevice::WriteOnly)) {
			file.write(out.c_str());
			file.write("\n");
		}

		return QLatin1String("*");
	} catch (const YAML::Exception &ex) {
		qWarning("NEdit: Error writing %s in config directory:\n%s", qPrintable(filename), ex.what());
	}

	return QString();
}

}

// list of available language modes and language specific preferences
std::vector<LanguageMode> LanguageModes;

QString ImportedSettingsFile() {
	return ImportedFile;
}

bool PreferencesChanged() {
	return PrefsHaveChanged;
}

void RestoreNEditPrefs() {

	Settings::loadPreferences(IsServer);

	/* Do further parsing on resource types which RestorePreferences does
	 * not understand and reads as strings, to put them in the final form
	 * in which nedit stores and uses. */
	translatePrefFormats(NEDIT_VERSION);
}

void SaveNEditPrefs(QWidget *parent, Verbosity verbosity) {

	QString prefFileName = Settings::configFile();
	if (prefFileName.isNull()) {
		QMessageBox::warning(parent, tr("Error saving Preferences"), tr("Unable to save preferences: Cannot determine filename."));
		return;
	}

	if (verbosity == Verbosity::Verbose) {
		int resp = QMessageBox::information(parent, tr("Save Preferences"),
											ImportedFile.isNull() ? tr("Default preferences will be saved in the file:\n%1\nNEdit automatically loads this file each time it is started.").arg(prefFileName)
																  : tr("Default preferences will be saved in the file:\n%1\nSAVING WILL INCORPORATE SETTINGS FROM FILE: %2").arg(prefFileName, ImportedFile),
											QMessageBox::Ok | QMessageBox::Cancel);

		if (resp == QMessageBox::Cancel) {
			return;
		}
	}

	Settings::shellCommands         = write_shell_commands_string();
	Settings::macroCommands         = write_macro_commands_string();
	Settings::bgMenuCommands        = write_bg_menu_commands_string();
	Settings::highlightPatterns     = Highlight::WriteHighlightString();
	Settings::languageModes         = WriteLanguageModesString();
	Settings::smartIndentInit       = SmartIndent::writeSmartIndentString();
	Settings::smartIndentInitCommon = SmartIndent::writeSmartIndentCommonString();

	if (!Settings::savePreferences()) {
		QMessageBox::warning(
			parent,
			tr("Save Preferences"),
			tr("Unable to save preferences in %1").arg(prefFileName));
	}

	Theme::save();
	PrefsHaveChanged = false;
}

/*
** Load an additional preferences file on top of the existing preferences
** derived from defaults, the .nedit file, and X resources.
*/
void ImportPrefFile(const QString &filename) {
	Settings::importSettings(filename);

	// NOTE(eteran): fix for issue #106
	translatePrefFormats(NEDIT_VERSION);
}

void SetPrefOpenInTab(bool state) {
	if (Settings::openInTab != state) {
		PrefsHaveChanged = true;
	}
	Settings::openInTab = state;
}

bool GetPrefOpenInTab() {
	return Settings::openInTab;
}

void SetPrefWrap(WrapStyle state) {
	if (Settings::autoWrap != state) {
		PrefsHaveChanged = true;
	}
	Settings::autoWrap = state;
}

WrapStyle GetPrefWrap(size_t langMode) {
	if (langMode == PLAIN_LANGUAGE_MODE || LanguageModes[langMode].wrapStyle == WrapStyle::Default) {
		return Settings::autoWrap;
	}

	return LanguageModes[langMode].wrapStyle;
}

void SetPrefWrapMargin(int margin) {
	if (Settings::wrapMargin != margin) {
		PrefsHaveChanged = true;
	}
	Settings::wrapMargin = margin;
}

int GetPrefWrapMargin() {
	return Settings::wrapMargin;
}

void SetPrefSearch(SearchType searchType) {
	if (Settings::searchMethod != searchType) {
		PrefsHaveChanged = true;
	}
	Settings::searchMethod = searchType;
}

bool GetPrefHeavyCursor() {
	return Settings::heavyCursor;
}

bool GetPrefAutoWrapPastedText() {
	return Settings::autoWrapPastedText;
}

bool GetPrefColorizeHighlightedText() {
	return Settings::colorizeHighlightedText;
}

bool GetPrefShowResizeNotification() {
	return Settings::showResizeNotification;
}

SearchType GetPrefSearch() {
	return Settings::searchMethod;
}

void SetPrefAutoIndent(IndentStyle state) {
	if (Settings::autoIndent != state) {
		PrefsHaveChanged = true;
	}
	Settings::autoIndent = state;
}

IndentStyle GetPrefAutoIndent(size_t langMode) {
	if (langMode == PLAIN_LANGUAGE_MODE || LanguageModes[langMode].indentStyle == IndentStyle::Default) {
		return Settings::autoIndent;
	}

	return LanguageModes[langMode].indentStyle;
}

void SetPrefAutoSave(bool state) {
	if (Settings::autoSave != state) {
		PrefsHaveChanged = true;
	}
	Settings::autoSave = state;
}

bool GetPrefAutoSave() {
	return Settings::autoSave;
}

void SetPrefSaveOldVersion(bool state) {
	if (Settings::saveOldVersion != state) {
		PrefsHaveChanged = true;
	}
	Settings::saveOldVersion = state;
}

bool GetPrefSaveOldVersion() {
	return Settings::saveOldVersion;
}

void SetPrefSearchDlogs(bool state) {
	if (Settings::searchDialogs != state) {
		PrefsHaveChanged = true;
	}
	Settings::searchDialogs = state;
}

bool GetPrefSearchDlogs() {
	return Settings::searchDialogs;
}

void SetPrefBeepOnSearchWrap(bool state) {
	if (Settings::beepOnSearchWrap != state) {
		PrefsHaveChanged = true;
	}
	Settings::beepOnSearchWrap = state;
}

bool GetPrefBeepOnSearchWrap() {
	return Settings::beepOnSearchWrap;
}

void SetPrefKeepSearchDlogs(bool state) {
	if (Settings::retainSearchDialogs != state) {
		PrefsHaveChanged = true;
	}
	Settings::retainSearchDialogs = state;
}

bool GetPrefKeepSearchDlogs() {
	return Settings::retainSearchDialogs;
}

void SetPrefSearchWraps(bool state) {
	if (Settings::searchWraps != state) {
		PrefsHaveChanged = true;
	}
	Settings::searchWraps = state;
}

WrapMode GetPrefSearchWraps() {
	return Settings::searchWraps ? WrapMode::Wrap : WrapMode::NoWrap;
}

int GetPrefStickyCaseSenseBtn() {
	return Settings::stickyCaseSenseButton;
}

void SetPrefStatsLine(bool state) {
	if (Settings::statisticsLine != state) {
		PrefsHaveChanged = true;
	}
	Settings::statisticsLine = state;
}

int GetPrefStatsLine() {
	return Settings::statisticsLine;
}

void SetPrefISearchLine(bool state) {
	if (Settings::iSearchLine != state) {
		PrefsHaveChanged = true;
	}
	Settings::iSearchLine = state;
}

int GetPrefISearchLine() {
	return Settings::iSearchLine;
}

void SetPrefSortTabs(bool state) {
	if (Settings::sortTabs != state) {
		PrefsHaveChanged = true;
	}
	Settings::sortTabs = state;
}

bool GetPrefSortTabs() {
	return Settings::sortTabs;
}

void SetPrefTabBar(bool state) {
	if (Settings::tabBar != state) {
		PrefsHaveChanged = true;
	}
	Settings::tabBar = state;
}

bool GetPrefTabBar() {
	return Settings::tabBar;
}

void SetPrefTabBarHideOne(bool state) {
	if (Settings::tabBarHideOne != state) {
		PrefsHaveChanged = true;
	}
	Settings::tabBarHideOne = state;
}

int GetPrefTabBarHideOne() {
	return Settings::tabBarHideOne;
}

void SetPrefGlobalTabNavigate(bool state) {
	if (Settings::globalTabNavigate != state) {
		PrefsHaveChanged = true;
	}
	Settings::globalTabNavigate = state;
}

int GetPrefGlobalTabNavigate() {
	return Settings::globalTabNavigate;
}

void SetPrefToolTips(bool state) {
	if (Settings::toolTips != state) {
		PrefsHaveChanged = true;
	}
	Settings::toolTips = state;
}

bool GetPrefToolTips() {
	return Settings::toolTips;
}

void SetPrefLineNums(bool state) {
	if (Settings::lineNumbers != state) {
		PrefsHaveChanged = true;
	}
	Settings::lineNumbers = state;
}

int GetPrefLineNums() {
	return Settings::lineNumbers;
}

void SetPrefShowPathInWindowsMenu(bool state) {
	if (Settings::pathInWindowsMenu != state) {
		PrefsHaveChanged = true;
	}
	Settings::pathInWindowsMenu = state;
}

int GetPrefShowPathInWindowsMenu() {
	return Settings::pathInWindowsMenu;
}

void SetPrefWarnFileMods(bool state) {
	if (Settings::warnFileMods != state) {
		PrefsHaveChanged = true;
	}
	Settings::warnFileMods = state;
}

bool GetPrefWarnFileMods() {
	return Settings::warnFileMods;
}

void SetPrefWarnRealFileMods(bool state) {
	if (Settings::warnRealFileMods != state) {
		PrefsHaveChanged = true;
	}
	Settings::warnRealFileMods = state;
}

bool GetPrefWarnRealFileMods() {
	return Settings::warnRealFileMods;
}

void SetPrefWarnExit(bool state) {
	if (Settings::warnExit != state) {
		PrefsHaveChanged = true;
	}
	Settings::warnExit = state;
}

bool GetPrefWarnExit() {
	return Settings::warnExit;
}

void SetPrefFindReplaceUsesSelection(bool state) {
	if (Settings::findReplaceUsesSelection != state) {
		PrefsHaveChanged = true;
	}
	Settings::findReplaceUsesSelection = state;
}

bool GetPrefFindReplaceUsesSelection() {
	return Settings::findReplaceUsesSelection;
}

void SetPrefRows(int nRows) {
	if (Settings::textRows != nRows) {
		PrefsHaveChanged = true;
	}
	Settings::textRows = nRows;
}

int GetPrefRows() {
	return Settings::textRows;
}

void SetPrefCols(int nCols) {
	if (Settings::textCols != nCols) {
		PrefsHaveChanged = true;
	}
	Settings::textCols = nCols;
}

int GetPrefCols() {
	return Settings::textCols;
}

void SetPrefTabDist(int tabDist) {
	if (Settings::tabDistance != tabDist) {
		PrefsHaveChanged = true;
	}
	Settings::tabDistance = tabDist;
}

int GetPrefTabDist(size_t langMode) {
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

void SetPrefEmTabDist(int tabDist) {
	if (Settings::emulateTabs != tabDist) {
		PrefsHaveChanged = true;
	}
	Settings::emulateTabs = tabDist;
}

int GetPrefEmTabDist(size_t langMode) {
	if (langMode == PLAIN_LANGUAGE_MODE || LanguageModes[langMode].emTabDist == LanguageMode::DEFAULT_EM_TAB_DIST) {
		return Settings::emulateTabs;
	}

	return LanguageModes[langMode].emTabDist;
}

void SetPrefInsertTabs(bool state) {
	if (Settings::insertTabs != state) {
		PrefsHaveChanged = true;
	}
	Settings::insertTabs = state;
}

int GetPrefInsertTabs() {
	return Settings::insertTabs;
}

void SetPrefShowMatching(ShowMatchingStyle state) {
	if (Settings::showMatching != state) {
		PrefsHaveChanged = true;
	}
	Settings::showMatching = state;
}

ShowMatchingStyle GetPrefShowMatching() {
	return Settings::showMatching;
}

void SetPrefMatchSyntaxBased(bool state) {
	if (Settings::matchSyntaxBased != state) {
		PrefsHaveChanged = true;
	}
	Settings::matchSyntaxBased = state;
}

bool GetPrefMatchSyntaxBased() {
	return Settings::matchSyntaxBased;
}

void SetPrefHighlightSyntax(bool state) {
	if (Settings::highlightSyntax != state) {
		PrefsHaveChanged = true;
	}
	Settings::highlightSyntax = state;
}

bool GetPrefHighlightSyntax() {
	return Settings::highlightSyntax;
}

void SetPrefBacklightChars(bool state) {
	if (Settings::backlightChars != state) {
		PrefsHaveChanged = true;
	}
	Settings::backlightChars = state;
}

bool GetPrefBacklightChars() {
	return Settings::backlightChars;
}

QString GetPrefBacklightCharTypes() {
	return Settings::backlightCharTypes;
}

void SetPrefRepositionDialogs(bool state) {
	if (Settings::repositionDialogs != state) {
		PrefsHaveChanged = true;
	}
	Settings::repositionDialogs = state;
}

bool GetPrefRepositionDialogs() {
	return Settings::repositionDialogs;
}

void SetPrefAutoScroll(bool state) {
	if (Settings::autoScroll != state) {
		PrefsHaveChanged = true;
	}

	const int margin = state ? Settings::autoScrollVPadding : 0;

	Settings::autoScroll = state;

	// TODO(eteran): should this be done at this level? maybe a signal/slot update?
	for (DocumentWidget *document : DocumentWidget::allDocuments()) {
		document->setAutoScroll(margin);
	}
}

bool GetPrefAutoScroll() {
	return Settings::autoScroll;
}

int GetVerticalAutoScroll() {
	return Settings::autoScroll ? Settings::autoScrollVPadding : 0;
}

void SetPrefAppendLF(bool state) {
	if (Settings::appendLF != state) {
		PrefsHaveChanged = true;
	}
	Settings::appendLF = state;
}

bool GetPrefAppendLF() {
	return Settings::appendLF;
}

void SetPrefSortOpenPrevMenu(bool state) {
	if (Settings::sortOpenPrevMenu != state) {
		PrefsHaveChanged = true;
	}
	Settings::sortOpenPrevMenu = state;
}

int GetPrefSortOpenPrevMenu() {
	return Settings::sortOpenPrevMenu;
}

QString GetPrefTagFile() {
	return Settings::tagFile;
}

void SetPrefSmartTags(bool state) {
	if (Settings::smartTags != state) {
		PrefsHaveChanged = true;
	}
	Settings::smartTags = state;
}

int GetPrefSmartTags() {
	return Settings::smartTags;
}

// Advanced
bool GetPrefAlwaysCheckRelTagsSpecs() {
	return Settings::alwaysCheckRelativeTagsSpecs;
}

QString GetPrefDelimiters() {
	return Settings::wordDelimiters;
}

QString GetPrefColorName(ColorTypes index) {
	return Settings::colors[index];
}

void SetPrefColorName(ColorTypes index, const QString &name) {
	if (Settings::colors[index] != name) {
		PrefsHaveChanged = true;
	}
	Settings::colors[index] = name;
}

void SetPrefFont(const QString &fontName) {
	if (Settings::fontName != fontName) {
		PrefsHaveChanged = true;
	}
	Settings::fontName = fontName;
	Settings::font     = Font::fromString(fontName);
}

QString GetPrefFontName() {
	return Settings::fontName;
}

QFont GetPrefDefaultFont() {
	return Settings::font;
}

void SetPrefShell(const QString &shell) {
	if (Settings::shell != shell) {
		PrefsHaveChanged = true;
	}
	Settings::shell = shell;
}

QString GetPrefShell() {
	return Settings::shell;
}

QString GetPrefGeometry() {
	return Settings::geometry;
}

QString GetPrefServerName() {
	return Settings::serverName;
}

int GetPrefMaxPrevOpenFiles() {
	return Settings::maxPrevOpenFiles;
}

bool GetPrefTypingHidesPointer() {
	return Settings::typingHidesPointer;
}

bool GetPrefSmartHome() {
	return Settings::smartHome;
}

void SetPrefTitleFormat(const QString &format) {
	if (Settings::titleFormat != format) {
		PrefsHaveChanged = true;
	}

	Settings::titleFormat = format;

	// update all windows
	// TODO(eteran): should this be done at this level? maybe a signal/slot update?
	for (MainWindow *window : MainWindow::allWindows()) {
		DocumentWidget *document = window->currentDocument();
		window->updateWindowTitle(document);
	}
}

QString GetPrefTitleFormat() {
	return Settings::titleFormat;
}

QStringList GetPrefIncludePaths() {
	return Settings::includePaths;
}

bool GetPrefUndoModifiesSelection() {
	return Settings::undoModifiesSelection;
}

bool GetPrefFocusOnRaise() {
	return Settings::focusOnRaise;
}

bool GetPrefForceOSConversion() {
	return Settings::forceOSConversion;
}

bool GetPrefHonorSymlinks() {
	return Settings::honorSymlinks;
}

TruncSubstitution GetPrefTruncSubstitution() {
	return Settings::truncSubstitution;
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
size_t FindLanguageMode(const QString &languageName) {

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
QString LanguageModeName(size_t mode) {
	if (mode == PLAIN_LANGUAGE_MODE || LanguageModes.empty()) {
		return QString();
	} else {
		Q_ASSERT(mode < LanguageModes.size());
		return LanguageModes[mode].name;
	}
}

bool ReadNumericField(Input &in, int *value) {

	// skip over blank space
	in.skipWhitespace();

	static const QRegularExpression re(QLatin1String("(0|[-+]?[1-9][0-9]*)"));
	QString number;
	if (in.match(re, &number)) {
		bool ok;
		*value = number.toInt(&ok);
		return ok;
	}

	return false;
}

/*
** Parse a symbolic field, skipping initial and trailing whitespace,
** stops on first invalid character or end of string.  Valid characters
** are letters, numbers, _, -, +, $, #, and internal whitespace.  Internal
** whitespace is compressed to single space characters.
*/
QString ReadSymbolicField(Input &input) {

	// skip over initial blank space
	input.skipWhitespace();

	Input strStart = input;

	static const QRegularExpression re(QLatin1String("[A-Za-z0-9_+$# \t-]*"));

	input.match(re);

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
	if (outStr.endsWith(QLatin1Char(' '))) {
		outStr.chop(1);
	}

	return outStr;
}

/*
** parse an individual quoted string.  Anything between
** double quotes is acceptable, quote characters can be escaped by "".
** Returns string in "string" containing argument minus quotes.
** If not successful, returns false with message in "errMsg".
*/
bool ReadQuotedString(Input &in, QString *errMsg, QString *string) {

	// TODO(eteran): return optional QString?

	constexpr auto Quote = QLatin1Char('"');

	// skip over blank space
	in.skipWhitespace();

	// look for initial quote
	if (*in != Quote) {
		*errMsg = tr("expecting quoted string");
		return false;
	}
	++in;

	// calculate max length
	Input c = in;

	for (;; ++c) {
		if (c.atEnd()) {
			*errMsg = tr("string not terminated");
			return false;
		} else if (*c == Quote) {
			if (*(c + 1) == Quote) {
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
		if (*in == Quote) {
			if (*(in + 1) == Quote) {
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
QString MakeQuotedString(const QString &string) {

	constexpr auto Quote = QLatin1Char('"');

	int length = 0;

	// calculate length
	for (QChar ch : string) {
		if (ch == Quote) {
			++length;
		}
		++length;
	}

	QString outStr;
	outStr.reserve(length + 3);
	auto outPtr = std::back_inserter(outStr);

	// add starting quote
	*outPtr++ = Quote;

	// copy string, escaping quotes with ""
	for (QChar ch : string) {
		if (ch == Quote) {
			*outPtr++ = Quote;
		}
		*outPtr++ = ch;
	}

	// add ending quote
	*outPtr++ = Quote;

	return outStr;
}

/*
** Skip a delimiter and it's surrounding whitespace
*/
bool SkipDelimiter(Input &in, QString *errMsg) {

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

bool reportError(QWidget *toDialog, const QString &string, int stoppedAt, const QString &errorIn, const QString &message) {

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
			++nNonWhite;
		}
	}

	int len = stoppedAt - c + (stoppedAt == string.size() ? 0 : 1);

	QString errorLine = tr("%1<==").arg(string.mid(c, len));

	if (!toDialog) {
		qWarning("NEdit: %s in %s:\n%s", qPrintable(message), qPrintable(errorIn), qPrintable(errorLine));
	} else {
		QMessageBox::warning(toDialog, tr("Parse Error"), tr("%1 in %2:\n%3").arg(message, errorIn, errorLine));
	}

	return false;
}

}
