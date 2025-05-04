
#include "Settings.h"
#include "FromInteger.h"
#include "Util/Environment.h"
#include "Util/Resource.h"

#include <QFontDatabase>
#include <QSettings>
#include <QStandardPaths>
#include <QtDebug>
#include <QtGlobal>

#include <random>
#include <type_traits>

namespace Settings {

namespace {

bool settingsLoaded_ = false;

const QStringList DEFAULT_INCLUDE_PATHS = {
	QLatin1String("/usr/include/"),
	QLatin1String("/usr/local/include/"),
};

const auto DEFAULT_DELIMITERS      = QLatin1String(".,/\\`'!|@#%^&*()-=+{}[]\":;<>?");
const auto DEFAULT_BACKLIGHT_CHARS = QLatin1String("0-8,10-31,127:red;9:#dedede;32,160-255:#f0f0f0;128-159:orange");

QString DefaultTextFont() {
	QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	fixedFont.setPointSize(12);
	return fixedFont.toString();
}

template <class T>
using IsEnum = std::enable_if_t<std::is_enum_v<T>>;

template <class T, class = IsEnum<T>>
T ReadEnum(QSettings &settings, const QString &key, const T &defaultValue = T()) {
	using U = std::underlying_type_t<T>;
	return FromInteger<T>(settings.value(key, static_cast<U>(defaultValue)).toInt());
}

template <class T, class = IsEnum<T>>
void WriteEnum(QSettings &settings, const QString &key, const T &value) {
	using U = std::underlying_type_t<T>;
	settings.setValue(key, static_cast<U>(value));
}

/**
 * @brief Generates a random string of the specified length using alphanumeric characters.
 *
 * @param length The length of the random string to generate.
 * @return The random string.
 */
QString RandomString(int length) {
	static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

	QString str;
	str.reserve(length);

	std::random_device rd;
	std::mt19937 mt(rd());
	std::uniform_int_distribution<> dist(0, sizeof(alphabet) - 1);

	for (int i = 0; i < length; ++i) {
		const size_t index = dist(mt);
		str.append(QChar::fromLatin1(alphabet[index]));
	}

	return str;
}

/**
 * @brief Returns the configuration directory.
 *
 * @return The path to the configuration directory.
 *
 * @note If the environment variable `NEDIT_NG_HOME` is set,
 * it will be used as the configuration directory.
 */
QString ConfigDirectory() {
	static const QString nedit_home = GetEnvironmentVariable("NEDIT_NG_HOME");
	if (!nedit_home.isEmpty()) {
		return nedit_home;
	}

	static const QString configDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
	static const auto filename     = QStringLiteral("%1/nedit-ng").arg(configDir);
	return filename;
}

}

bool alwaysCheckRelativeTagsSpecs;
bool appendLF;
bool autoSave;
bool autoScroll;
bool autoWrapPastedText;
bool backlightChars;
bool beepOnSearchWrap;
bool colorizeHighlightedText;
bool findReplaceUsesSelection;
bool focusOnRaise;
bool forceOSConversion;
bool globalTabNavigate;
bool heavyCursor;
bool highlightSyntax;
bool honorSymlinks;
bool insertTabs;
bool iSearchLine;
bool lineNumbers;
bool matchSyntaxBased;
bool openInTab;
bool pathInWindowsMenu;
bool prefFileRead;
bool repositionDialogs;
bool retainSearchDialogs;
bool saveOldVersion;
bool searchDialogs;
bool searchWraps;
bool showResizeNotification;
bool smartHome;
bool smartTags;
bool sortOpenPrevMenu;
bool sortTabs;
bool splitHorizontally;
bool statisticsLine;
bool stickyCaseSenseButton;
bool tabBar;
bool tabBarHideOne;
bool toolTips;
bool typingHidesPointer;
bool undoModifiesSelection;
bool warnExit;
bool warnFileMods;
bool warnRealFileMods;
IndentStyle autoIndent;
int autoSaveCharLimit;
int autoSaveOpLimit;
int autoScrollVPadding;
int emulateTabs;
int maxPrevOpenFiles;
int tabDistance;
int textCols;
int textRows;
int truncateLongNamesInTabs;
int wrapMargin;
QFont font;
QString backlightCharTypes;
QString bgMenuCommands;
QString colors[9];
QString fontName;
QString geometry;
QString highlightPatterns;
QString languageModes;
QString macroCommands;
QString serverName;
QString serverNameOverride;
QString shell;
QString shellCommands;
QString smartIndentInit;
QString smartIndentInitCommon;
QString tagFile;
QString titleFormat;
QString wordDelimiters;
QStringList includePaths;
SearchType searchMethod;
ShowMatchingStyle showMatching;
TruncSubstitution truncSubstitution;
WrapStyle autoWrap;

/**
 * @brief Gets the path of the theme file.
 *
 * @return The path to the theme file.
 */
QString ThemeFile() {
	static const QString configDir = ConfigDirectory();
	static const auto filename     = QStringLiteral("%1/theme.xml").arg(configDir);
	return filename;
}

/**
 * @brief Gets the path of the configuration file.
 *
 * @return The path to the configuration file.
 */
QString ConfigFile() {
	static const QString configDir = ConfigDirectory();
	static const auto filename     = QStringLiteral("%1/config.ini").arg(configDir);
	return filename;
}

/**
 * @brief Gets the path of the history file.
 *
 * @return The path to the history file.
 */
QString HistoryFile() {
	static const QString configDir = ConfigDirectory();
	static const auto filename     = QStringLiteral("%1/history").arg(configDir);
	return filename;
}

/**
 * @brief Gets the path of the auto-load macro file.
 *
 * @return The path to the auto-load macro file.
 */
QString AutoLoadMacroFile() {
	static const QString configDir = ConfigDirectory();
	static const auto filename     = QStringLiteral("%1/autoload.nm").arg(configDir);
	return filename;
}

/**
 * @brief Gets the path of the language mode file.
 *
 * @return The path to the language mode file.
 */
QString LanguageModeFile() {
	static const QString configDir = ConfigDirectory();
	static const auto filename     = QStringLiteral("%1/languages.yaml").arg(configDir);
	return filename;
}

/**
 * @brief Gets the path of the macro menu file.
 *
 * @return The path to the macro menu file.
 */
QString MactoMenuFile() {
	static const QString configDir = ConfigDirectory();
	static const auto filename     = QStringLiteral("%1/macros.yaml").arg(configDir);
	return filename;
}

/**
 * @brief Gets the path of the shell menu file.
 *
 * @return The path to the shell menu file.
 */
QString ShellMenuFile() {
	static const QString configDir = ConfigDirectory();
	static const auto filename     = QStringLiteral("%1/shell.yaml").arg(configDir);
	return filename;
}

/**
 * @brief Gets the path of the context menu file.
 *
 * @return The path to the context menu file.
 */
QString ContextMenuFile() {
	static const QString configDir = ConfigDirectory();
	static const auto filename     = QStringLiteral("%1/context.yaml").arg(configDir);
	return filename;
}

/**
 * @brief Gets the path of the style file.
 *
 * @return The path to the style file.
 */
QString StyleFile() {
	static const QString configDir = ConfigDirectory();
	static const auto filename     = QStringLiteral("%1/style.qss").arg(configDir);
	return filename;
}

/**
 * @brief Gets the path of the highlight patterns file.
 *
 * @return The path to the highlight patterns file.
 */
QString HighlightPatternsFile() {
	static const QString configDir = ConfigDirectory();
	static const auto filename     = QStringLiteral("%1/patterns.yaml").arg(configDir);
	return filename;
}

/**
 * @brief Gets the path of the smart indent file.
 *
 * @return The path to the smart indent file.
 */
QString SmartIndentFile() {
	static const QString configDir = ConfigDirectory();
	static const auto filename     = QStringLiteral("%1/indent.yaml").arg(configDir);
	return filename;
}

/**
 * @brief Loads the user preferences from the configuration file.
 *
 * @param isServer If `true`, the and the configuration does not specify a server name,
 *                 a random server name will be generated.
 */
void Load(bool isServer) {

	if (settingsLoaded_) {
		return; // Already loaded
	}

	const QString filename = ConfigFile();
	QSettings settings(filename, QSettings::IniFormat);

	shellCommands         = settings.value(QLatin1String("nedit.shellCommands"), QLatin1String("*")).toString();
	macroCommands         = settings.value(QLatin1String("nedit.macroCommands"), QLatin1String("*")).toString();
	bgMenuCommands        = settings.value(QLatin1String("nedit.bgMenuCommands"), QLatin1String("*")).toString();
	highlightPatterns     = settings.value(QLatin1String("nedit.highlightPatterns"), QLatin1String("*")).toString();
	languageModes         = settings.value(QLatin1String("nedit.languageModes"), QLatin1String("*")).toString();
	smartIndentInit       = settings.value(QLatin1String("nedit.smartIndentInit"), QLatin1String("*")).toString();
	smartIndentInitCommon = settings.value(QLatin1String("nedit.smartIndentInitCommon"), QLatin1String("*")).toString();

	autoWrap          = ReadEnum(settings, QLatin1String("nedit.autoWrap"), WrapStyle::None);
	autoIndent        = ReadEnum(settings, QLatin1String("nedit.autoIndent"), IndentStyle::Auto);
	showMatching      = ReadEnum(settings, QLatin1String("nedit.showMatching"), ShowMatchingStyle::Delimiter);
	searchMethod      = ReadEnum(settings, QLatin1String("nedit.searchMethod"), SearchType::Literal);
	truncSubstitution = ReadEnum(settings, QLatin1String("nedit.truncSubstitution"), TruncSubstitution::Fail);

	wrapMargin             = settings.value(QLatin1String("nedit.wrapMargin"), 0).toInt();
	autoSave               = settings.value(QLatin1String("nedit.autoSave"), true).toBool();
	openInTab              = settings.value(QLatin1String("nedit.openInTab"), true).toBool();
	saveOldVersion         = settings.value(QLatin1String("nedit.saveOldVersion"), false).toBool();
	matchSyntaxBased       = settings.value(QLatin1String("nedit.matchSyntaxBased"), true).toBool();
	highlightSyntax        = settings.value(QLatin1String("nedit.highlightSyntax"), true).toBool();
	backlightChars         = settings.value(QLatin1String("nedit.backlightChars"), false).toBool();
	backlightCharTypes     = settings.value(QLatin1String("nedit.backlightCharTypes"), DEFAULT_BACKLIGHT_CHARS).toString();
	searchDialogs          = settings.value(QLatin1String("nedit.searchDialogs"), false).toBool();
	beepOnSearchWrap       = settings.value(QLatin1String("nedit.beepOnSearchWrap"), false).toBool();
	retainSearchDialogs    = settings.value(QLatin1String("nedit.retainSearchDialogs"), false).toBool();
	searchWraps            = settings.value(QLatin1String("nedit.searchWraps"), true).toBool();
	stickyCaseSenseButton  = settings.value(QLatin1String("nedit.stickyCaseSenseButton"), true).toBool();
	repositionDialogs      = settings.value(QLatin1String("nedit.repositionDialogs"), false).toBool();
	autoScroll             = settings.value(QLatin1String("nedit.autoScroll"), false).toBool();
	autoScrollVPadding     = settings.value(QLatin1String("nedit.autoScrollVPadding"), 4).toInt();
	appendLF               = settings.value(QLatin1String("nedit.appendLF"), true).toBool();
	sortOpenPrevMenu       = settings.value(QLatin1String("nedit.sortOpenPrevMenu"), true).toBool();
	statisticsLine         = settings.value(QLatin1String("nedit.statisticsLine"), false).toBool();
	iSearchLine            = settings.value(QLatin1String("nedit.iSearchLine"), false).toBool();
	sortTabs               = settings.value(QLatin1String("nedit.sortTabs"), false).toBool();
	tabBar                 = settings.value(QLatin1String("nedit.tabBar"), true).toBool();
	tabBarHideOne          = settings.value(QLatin1String("nedit.tabBarHideOne"), true).toBool();
	toolTips               = settings.value(QLatin1String("nedit.toolTips"), true).toBool();
	globalTabNavigate      = settings.value(QLatin1String("nedit.globalTabNavigate"), false).toBool();
	lineNumbers            = settings.value(QLatin1String("nedit.lineNumbers"), false).toBool();
	pathInWindowsMenu      = settings.value(QLatin1String("nedit.pathInWindowsMenu"), true).toBool();
	warnFileMods           = settings.value(QLatin1String("nedit.warnFileMods"), true).toBool();
	warnRealFileMods       = settings.value(QLatin1String("nedit.warnRealFileMods"), true).toBool();
	warnExit               = settings.value(QLatin1String("nedit.warnExit"), true).toBool();
	showResizeNotification = settings.value(QLatin1String("nedit.showResizeNotification"), false).toBool();
	smartHome              = settings.value(QLatin1String("nedit.smartHome"), false).toBool();

	textRows                     = settings.value(QLatin1String("nedit.textRows"), 24).toInt();
	textCols                     = settings.value(QLatin1String("nedit.textCols"), 80).toInt();
	tabDistance                  = settings.value(QLatin1String("nedit.tabDistance"), 8).toInt();
	emulateTabs                  = settings.value(QLatin1String("nedit.emulateTabs"), 0).toInt();
	insertTabs                   = settings.value(QLatin1String("nedit.insertTabs"), true).toBool();
	fontName                     = settings.value(QLatin1String("nedit.textFont"), DefaultTextFont()).toString();
	shell                        = settings.value(QLatin1String("nedit.shell"), QLatin1String("DEFAULT")).toString();
	geometry                     = settings.value(QLatin1String("nedit.geometry"), QString()).toString();
	tagFile                      = settings.value(QLatin1String("nedit.tagFile"), QString()).toString();
	wordDelimiters               = settings.value(QLatin1String("nedit.wordDelimiters"), DEFAULT_DELIMITERS).toString();
	includePaths                 = settings.value(QLatin1String("nedit.includePaths"), DEFAULT_INCLUDE_PATHS).toStringList();
	serverName                   = settings.value(QLatin1String("nedit.serverName"), QString()).toString();
	maxPrevOpenFiles             = settings.value(QLatin1String("nedit.maxPrevOpenFiles"), 30).toInt();
	autoSaveCharLimit            = settings.value(QLatin1String("nedit.autoSaveCharLimit"), 80).toInt();
	autoSaveOpLimit              = settings.value(QLatin1String("nedit.autoSaveOpLimit"), 8).toInt();
	smartTags                    = settings.value(QLatin1String("nedit.smartTags"), true).toBool();
	typingHidesPointer           = settings.value(QLatin1String("nedit.typingHidesPointer"), false).toBool();
	alwaysCheckRelativeTagsSpecs = settings.value(QLatin1String("nedit.alwaysCheckRelativeTagsSpecs"), true).toBool();
	colorizeHighlightedText      = settings.value(QLatin1String("nedit.colorizeHighlightedText"), true).toBool();
	autoWrapPastedText           = settings.value(QLatin1String("nedit.autoWrapPastedText"), false).toBool();
	heavyCursor                  = settings.value(QLatin1String("nedit.heavyCursor"), false).toBool();
	prefFileRead                 = settings.value(QLatin1String("nedit.prefFileRead"), false).toBool();
	findReplaceUsesSelection     = settings.value(QLatin1String("nedit.findReplaceUsesSelection"), false).toBool();
	titleFormat                  = settings.value(QLatin1String("nedit.titleFormat"), QLatin1String("{%c} [%s] %f (%S) - %d")).toString();
	undoModifiesSelection        = settings.value(QLatin1String("nedit.undoModifiesSelection"), true).toBool();
	splitHorizontally            = settings.value(QLatin1String("nedit.splitHorizontally"), false).toBool();
	truncateLongNamesInTabs      = settings.value(QLatin1String("nedit.truncateLongNamesInTabs"), 0).toInt();
	focusOnRaise                 = settings.value(QLatin1String("nedit.focusOnRaise"), false).toBool();
	forceOSConversion            = settings.value(QLatin1String("nedit.forceOSConversion"), true).toBool();
	honorSymlinks                = settings.value(QLatin1String("nedit.honorSymlinks"), true).toBool();

	if (isServer && serverName.isEmpty()) {
		serverName = RandomString(8);
	}

	if (includePaths.isEmpty()) {
		includePaths.append(DEFAULT_INCLUDE_PATHS[0]);
	}

	settingsLoaded_ = true;
}

/**
 * @brief Loads setting from a file and merges them into the current settings.
 *
 * @param filename The path to the settings file to import.
 *
 * @note This function assumes that settings have already been loaded.
 */
void Import(const QString &filename) {
	if (!settingsLoaded_) {
		qWarning("NEdit: Warning, importing while no previous settings loaded!");
	}

	QSettings settings(filename, QSettings::IniFormat);

	shellCommands         = settings.value(QLatin1String("nedit.shellCommands"), shellCommands).toString();
	macroCommands         = settings.value(QLatin1String("nedit.macroCommands"), macroCommands).toString();
	bgMenuCommands        = settings.value(QLatin1String("nedit.bgMenuCommands"), bgMenuCommands).toString();
	highlightPatterns     = settings.value(QLatin1String("nedit.highlightPatterns"), highlightPatterns).toString();
	languageModes         = settings.value(QLatin1String("nedit.languageModes"), languageModes).toString();
	smartIndentInit       = settings.value(QLatin1String("nedit.smartIndentInit"), smartIndentInit).toString();
	smartIndentInitCommon = settings.value(QLatin1String("nedit.smartIndentInitCommon"), smartIndentInitCommon).toString();

	autoWrap          = ReadEnum(settings, QLatin1String("nedit.autoWrap"), autoWrap);
	autoIndent        = ReadEnum(settings, QLatin1String("nedit.autoIndent"), autoIndent);
	showMatching      = ReadEnum(settings, QLatin1String("nedit.showMatching"), showMatching);
	searchMethod      = ReadEnum(settings, QLatin1String("nedit.searchMethod"), searchMethod);
	truncSubstitution = ReadEnum(settings, QLatin1String("nedit.truncSubstitution"), truncSubstitution);

	wrapMargin            = settings.value(QLatin1String("nedit.wrapMargin"), wrapMargin).toInt();
	autoSave              = settings.value(QLatin1String("nedit.autoSave"), autoSave).toBool();
	openInTab             = settings.value(QLatin1String("nedit.openInTab"), openInTab).toBool();
	saveOldVersion        = settings.value(QLatin1String("nedit.saveOldVersion"), saveOldVersion).toBool();
	matchSyntaxBased      = settings.value(QLatin1String("nedit.matchSyntaxBased"), matchSyntaxBased).toBool();
	highlightSyntax       = settings.value(QLatin1String("nedit.highlightSyntax"), highlightSyntax).toBool();
	backlightChars        = settings.value(QLatin1String("nedit.backlightChars"), backlightChars).toBool();
	backlightCharTypes    = settings.value(QLatin1String("nedit.backlightCharTypes"), backlightCharTypes).toString();
	searchDialogs         = settings.value(QLatin1String("nedit.searchDialogs"), searchDialogs).toBool();
	beepOnSearchWrap      = settings.value(QLatin1String("nedit.beepOnSearchWrap"), beepOnSearchWrap).toBool();
	retainSearchDialogs   = settings.value(QLatin1String("nedit.retainSearchDialogs"), retainSearchDialogs).toBool();
	searchWraps           = settings.value(QLatin1String("nedit.searchWraps"), searchWraps).toBool();
	stickyCaseSenseButton = settings.value(QLatin1String("nedit.stickyCaseSenseButton"), stickyCaseSenseButton).toBool();
	repositionDialogs     = settings.value(QLatin1String("nedit.repositionDialogs"), repositionDialogs).toBool();
	autoScroll            = settings.value(QLatin1String("nedit.autoScroll"), autoScroll).toBool();
	autoScrollVPadding    = settings.value(QLatin1String("nedit.autoScrollVPadding"), autoScrollVPadding).toInt();
	appendLF              = settings.value(QLatin1String("nedit.appendLF"), appendLF).toBool();
	sortOpenPrevMenu      = settings.value(QLatin1String("nedit.sortOpenPrevMenu"), sortOpenPrevMenu).toBool();
	statisticsLine        = settings.value(QLatin1String("nedit.statisticsLine"), statisticsLine).toBool();
	iSearchLine           = settings.value(QLatin1String("nedit.iSearchLine"), iSearchLine).toBool();
	sortTabs              = settings.value(QLatin1String("nedit.sortTabs"), sortTabs).toBool();
	tabBar                = settings.value(QLatin1String("nedit.tabBar"), tabBar).toBool();
	tabBarHideOne         = settings.value(QLatin1String("nedit.tabBarHideOne"), tabBarHideOne).toBool();
	toolTips              = settings.value(QLatin1String("nedit.toolTips"), toolTips).toBool();
	globalTabNavigate     = settings.value(QLatin1String("nedit.globalTabNavigate"), globalTabNavigate).toBool();
	lineNumbers           = settings.value(QLatin1String("nedit.lineNumbers"), lineNumbers).toBool();
	pathInWindowsMenu     = settings.value(QLatin1String("nedit.pathInWindowsMenu"), pathInWindowsMenu).toBool();
	warnFileMods          = settings.value(QLatin1String("nedit.warnFileMods"), warnFileMods).toBool();
	warnRealFileMods      = settings.value(QLatin1String("nedit.warnRealFileMods"), warnRealFileMods).toBool();
	warnExit              = settings.value(QLatin1String("nedit.warnExit"), warnExit).toBool();
	smartHome             = settings.value(QLatin1String("nedit.smartHome"), smartHome).toBool();

	textRows                     = settings.value(QLatin1String("nedit.textRows"), textRows).toInt();
	textCols                     = settings.value(QLatin1String("nedit.textCols"), textCols).toInt();
	tabDistance                  = settings.value(QLatin1String("nedit.tabDistance"), tabDistance).toInt();
	emulateTabs                  = settings.value(QLatin1String("nedit.emulateTabs"), emulateTabs).toInt();
	insertTabs                   = settings.value(QLatin1String("nedit.insertTabs"), insertTabs).toBool();
	fontName                     = settings.value(QLatin1String("nedit.textFont"), fontName).toString();
	shell                        = settings.value(QLatin1String("nedit.shell"), shell).toString();
	geometry                     = settings.value(QLatin1String("nedit.geometry"), geometry).toString();
	tagFile                      = settings.value(QLatin1String("nedit.tagFile"), tagFile).toString();
	wordDelimiters               = settings.value(QLatin1String("nedit.wordDelimiters"), wordDelimiters).toString();
	includePaths                 = settings.value(QLatin1String("nedit.includePaths"), includePaths).toStringList();
	serverName                   = settings.value(QLatin1String("nedit.serverName"), serverName).toString();
	maxPrevOpenFiles             = settings.value(QLatin1String("nedit.maxPrevOpenFiles"), maxPrevOpenFiles).toInt();
	autoSaveCharLimit            = settings.value(QLatin1String("nedit.autoSaveCharLimit"), autoSaveCharLimit).toInt();
	autoSaveOpLimit              = settings.value(QLatin1String("nedit.autoSaveOpLimit"), autoSaveOpLimit).toInt();
	smartTags                    = settings.value(QLatin1String("nedit.smartTags"), smartTags).toBool();
	typingHidesPointer           = settings.value(QLatin1String("nedit.typingHidesPointer"), typingHidesPointer).toBool();
	alwaysCheckRelativeTagsSpecs = settings.value(QLatin1String("nedit.alwaysCheckRelativeTagsSpecs"), alwaysCheckRelativeTagsSpecs).toBool();
	heavyCursor                  = settings.value(QLatin1String("nedit.heavyCursor"), heavyCursor).toBool();
	autoWrapPastedText           = settings.value(QLatin1String("nedit.autoWrapPastedText"), autoWrapPastedText).toBool();
	colorizeHighlightedText      = settings.value(QLatin1String("nedit.colorizeHighlightedText"), colorizeHighlightedText).toBool();
	prefFileRead                 = settings.value(QLatin1String("nedit.prefFileRead"), prefFileRead).toBool();
	findReplaceUsesSelection     = settings.value(QLatin1String("nedit.findReplaceUsesSelection"), findReplaceUsesSelection).toBool();
	titleFormat                  = settings.value(QLatin1String("nedit.titleFormat"), titleFormat).toString();
	undoModifiesSelection        = settings.value(QLatin1String("nedit.undoModifiesSelection"), undoModifiesSelection).toBool();
	splitHorizontally            = settings.value(QLatin1String("nedit.splitHorizontally"), splitHorizontally).toBool();
	truncateLongNamesInTabs      = settings.value(QLatin1String("nedit.truncateLongNamesInTabs"), truncateLongNamesInTabs).toInt();
	focusOnRaise                 = settings.value(QLatin1String("nedit.focusOnRaise"), focusOnRaise).toBool();
	forceOSConversion            = settings.value(QLatin1String("nedit.forceOSConversion"), forceOSConversion).toBool();
	honorSymlinks                = settings.value(QLatin1String("nedit.honorSymlinks"), honorSymlinks).toBool();
}

/**
 * @brief Saves the user preferences to the configuration file.
 *
 * @return `true` if the preferences were saved successfully, `false` otherwise.
 */
bool Save() {
	const QString filename = ConfigFile();
	QSettings settings(filename, QSettings::IniFormat);

	settings.setValue(QLatin1String("nedit.shellCommands"), shellCommands);
	settings.setValue(QLatin1String("nedit.macroCommands"), macroCommands);
	settings.setValue(QLatin1String("nedit.bgMenuCommands"), bgMenuCommands);
	settings.setValue(QLatin1String("nedit.highlightPatterns"), highlightPatterns);
	settings.setValue(QLatin1String("nedit.languageModes"), languageModes);
	settings.setValue(QLatin1String("nedit.smartIndentInit"), smartIndentInit);
	settings.setValue(QLatin1String("nedit.smartIndentInitCommon"), smartIndentInitCommon);

	WriteEnum(settings, QLatin1String("nedit.autoWrap"), autoWrap);
	WriteEnum(settings, QLatin1String("nedit.autoIndent"), autoIndent);
	WriteEnum(settings, QLatin1String("nedit.showMatching"), showMatching);
	WriteEnum(settings, QLatin1String("nedit.searchMethod"), searchMethod);
	WriteEnum(settings, QLatin1String("nedit.truncSubstitution"), truncSubstitution);

	settings.setValue(QLatin1String("nedit.wrapMargin"), wrapMargin);
	settings.setValue(QLatin1String("nedit.autoSave"), autoSave);
	settings.setValue(QLatin1String("nedit.openInTab"), openInTab);
	settings.setValue(QLatin1String("nedit.saveOldVersion"), saveOldVersion);
	settings.setValue(QLatin1String("nedit.matchSyntaxBased"), matchSyntaxBased);
	settings.setValue(QLatin1String("nedit.highlightSyntax"), highlightSyntax);
	settings.setValue(QLatin1String("nedit.backlightChars"), backlightChars);
	settings.setValue(QLatin1String("nedit.backlightCharTypes"), backlightCharTypes);
	settings.setValue(QLatin1String("nedit.searchDialogs"), searchDialogs);
	settings.setValue(QLatin1String("nedit.beepOnSearchWrap"), beepOnSearchWrap);
	settings.setValue(QLatin1String("nedit.retainSearchDialogs"), retainSearchDialogs);
	settings.setValue(QLatin1String("nedit.searchWraps"), searchWraps);
	settings.setValue(QLatin1String("nedit.stickyCaseSenseButton"), stickyCaseSenseButton);
	settings.setValue(QLatin1String("nedit.repositionDialogs"), repositionDialogs);
	settings.setValue(QLatin1String("nedit.autoScroll"), autoScroll);
	settings.setValue(QLatin1String("nedit.autoScrollVPadding"), autoScrollVPadding);
	settings.setValue(QLatin1String("nedit.appendLF"), appendLF);
	settings.setValue(QLatin1String("nedit.sortOpenPrevMenu"), sortOpenPrevMenu);
	settings.setValue(QLatin1String("nedit.statisticsLine"), statisticsLine);
	settings.setValue(QLatin1String("nedit.iSearchLine"), iSearchLine);
	settings.setValue(QLatin1String("nedit.sortTabs"), sortTabs);
	settings.setValue(QLatin1String("nedit.tabBar"), tabBar);
	settings.setValue(QLatin1String("nedit.tabBarHideOne"), tabBarHideOne);
	settings.setValue(QLatin1String("nedit.toolTips"), toolTips);
	settings.setValue(QLatin1String("nedit.globalTabNavigate"), globalTabNavigate);
	settings.setValue(QLatin1String("nedit.lineNumbers"), lineNumbers);
	settings.setValue(QLatin1String("nedit.pathInWindowsMenu"), pathInWindowsMenu);
	settings.setValue(QLatin1String("nedit.warnFileMods"), warnFileMods);
	settings.setValue(QLatin1String("nedit.warnRealFileMods"), warnRealFileMods);
	settings.setValue(QLatin1String("nedit.warnExit"), warnExit);
	settings.setValue(QLatin1String("nedit.smartHome"), smartHome);

	settings.setValue(QLatin1String("nedit.textRows"), textRows);
	settings.setValue(QLatin1String("nedit.textCols"), textCols);
	settings.setValue(QLatin1String("nedit.tabDistance"), tabDistance);
	settings.setValue(QLatin1String("nedit.emulateTabs"), emulateTabs);
	settings.setValue(QLatin1String("nedit.insertTabs"), insertTabs);
	settings.setValue(QLatin1String("nedit.textFont"), fontName);
	settings.setValue(QLatin1String("nedit.shell"), shell);
	settings.setValue(QLatin1String("nedit.geometry"), geometry);
	settings.setValue(QLatin1String("nedit.tagFile"), tagFile);
	settings.setValue(QLatin1String("nedit.wordDelimiters"), wordDelimiters);
	settings.setValue(QLatin1String("nedit.includePaths"), includePaths);
	settings.setValue(QLatin1String("nedit.serverName"), serverName);
	settings.setValue(QLatin1String("nedit.maxPrevOpenFiles"), maxPrevOpenFiles);
	settings.setValue(QLatin1String("nedit.autoSaveCharLimit"), autoSaveCharLimit);
	settings.setValue(QLatin1String("nedit.autoSaveOpLimit"), autoSaveOpLimit);
	settings.setValue(QLatin1String("nedit.smartTags"), smartTags);
	settings.setValue(QLatin1String("nedit.typingHidesPointer"), typingHidesPointer);
	settings.setValue(QLatin1String("nedit.autoWrapPastedText"), autoWrapPastedText);
	settings.setValue(QLatin1String("nedit.heavyCursor"), heavyCursor);
	settings.setValue(QLatin1String("nedit.alwaysCheckRelativeTagsSpecs"), alwaysCheckRelativeTagsSpecs);
	settings.setValue(QLatin1String("nedit.colorizeHighlightedText"), colorizeHighlightedText);
	settings.setValue(QLatin1String("nedit.prefFileRead"), prefFileRead);
	settings.setValue(QLatin1String("nedit.findReplaceUsesSelection"), findReplaceUsesSelection);
	settings.setValue(QLatin1String("nedit.titleFormat"), titleFormat);
	settings.setValue(QLatin1String("nedit.undoModifiesSelection"), undoModifiesSelection);
	settings.setValue(QLatin1String("nedit.splitHorizontally"), splitHorizontally);
	settings.setValue(QLatin1String("nedit.truncateLongNamesInTabs"), truncateLongNamesInTabs);
	settings.setValue(QLatin1String("nedit.focusOnRaise"), focusOnRaise);
	settings.setValue(QLatin1String("nedit.forceOSConversion"), forceOSConversion);
	settings.setValue(QLatin1String("nedit.honorSymlinks"), honorSymlinks);

	settings.sync();
	return settings.status() == QSettings::NoError;
}

}
