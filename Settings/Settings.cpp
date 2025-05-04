
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

	shellCommands         = settings.value(QStringLiteral("nedit.shellCommands"), QLatin1String("*")).toString();
	macroCommands         = settings.value(QStringLiteral("nedit.macroCommands"), QLatin1String("*")).toString();
	bgMenuCommands        = settings.value(QStringLiteral("nedit.bgMenuCommands"), QLatin1String("*")).toString();
	highlightPatterns     = settings.value(QStringLiteral("nedit.highlightPatterns"), QLatin1String("*")).toString();
	languageModes         = settings.value(QStringLiteral("nedit.languageModes"), QLatin1String("*")).toString();
	smartIndentInit       = settings.value(QStringLiteral("nedit.smartIndentInit"), QLatin1String("*")).toString();
	smartIndentInitCommon = settings.value(QStringLiteral("nedit.smartIndentInitCommon"), QLatin1String("*")).toString();

	autoWrap          = ReadEnum(settings, QStringLiteral("nedit.autoWrap"), WrapStyle::None);
	autoIndent        = ReadEnum(settings, QStringLiteral("nedit.autoIndent"), IndentStyle::Auto);
	showMatching      = ReadEnum(settings, QStringLiteral("nedit.showMatching"), ShowMatchingStyle::Delimiter);
	searchMethod      = ReadEnum(settings, QStringLiteral("nedit.searchMethod"), SearchType::Literal);
	truncSubstitution = ReadEnum(settings, QStringLiteral("nedit.truncSubstitution"), TruncSubstitution::Fail);

	wrapMargin             = settings.value(QStringLiteral("nedit.wrapMargin"), 0).toInt();
	autoSave               = settings.value(QStringLiteral("nedit.autoSave"), true).toBool();
	openInTab              = settings.value(QStringLiteral("nedit.openInTab"), true).toBool();
	saveOldVersion         = settings.value(QStringLiteral("nedit.saveOldVersion"), false).toBool();
	matchSyntaxBased       = settings.value(QStringLiteral("nedit.matchSyntaxBased"), true).toBool();
	highlightSyntax        = settings.value(QStringLiteral("nedit.highlightSyntax"), true).toBool();
	backlightChars         = settings.value(QStringLiteral("nedit.backlightChars"), false).toBool();
	backlightCharTypes     = settings.value(QStringLiteral("nedit.backlightCharTypes"), DEFAULT_BACKLIGHT_CHARS).toString();
	searchDialogs          = settings.value(QStringLiteral("nedit.searchDialogs"), false).toBool();
	beepOnSearchWrap       = settings.value(QStringLiteral("nedit.beepOnSearchWrap"), false).toBool();
	retainSearchDialogs    = settings.value(QStringLiteral("nedit.retainSearchDialogs"), false).toBool();
	searchWraps            = settings.value(QStringLiteral("nedit.searchWraps"), true).toBool();
	stickyCaseSenseButton  = settings.value(QStringLiteral("nedit.stickyCaseSenseButton"), true).toBool();
	repositionDialogs      = settings.value(QStringLiteral("nedit.repositionDialogs"), false).toBool();
	autoScroll             = settings.value(QStringLiteral("nedit.autoScroll"), false).toBool();
	autoScrollVPadding     = settings.value(QStringLiteral("nedit.autoScrollVPadding"), 4).toInt();
	appendLF               = settings.value(QStringLiteral("nedit.appendLF"), true).toBool();
	sortOpenPrevMenu       = settings.value(QStringLiteral("nedit.sortOpenPrevMenu"), true).toBool();
	statisticsLine         = settings.value(QStringLiteral("nedit.statisticsLine"), false).toBool();
	iSearchLine            = settings.value(QStringLiteral("nedit.iSearchLine"), false).toBool();
	sortTabs               = settings.value(QStringLiteral("nedit.sortTabs"), false).toBool();
	tabBar                 = settings.value(QStringLiteral("nedit.tabBar"), true).toBool();
	tabBarHideOne          = settings.value(QStringLiteral("nedit.tabBarHideOne"), true).toBool();
	toolTips               = settings.value(QStringLiteral("nedit.toolTips"), true).toBool();
	globalTabNavigate      = settings.value(QStringLiteral("nedit.globalTabNavigate"), false).toBool();
	lineNumbers            = settings.value(QStringLiteral("nedit.lineNumbers"), false).toBool();
	pathInWindowsMenu      = settings.value(QStringLiteral("nedit.pathInWindowsMenu"), true).toBool();
	warnFileMods           = settings.value(QStringLiteral("nedit.warnFileMods"), true).toBool();
	warnRealFileMods       = settings.value(QStringLiteral("nedit.warnRealFileMods"), true).toBool();
	warnExit               = settings.value(QStringLiteral("nedit.warnExit"), true).toBool();
	showResizeNotification = settings.value(QStringLiteral("nedit.showResizeNotification"), false).toBool();
	smartHome              = settings.value(QStringLiteral("nedit.smartHome"), false).toBool();

	textRows                     = settings.value(QStringLiteral("nedit.textRows"), 24).toInt();
	textCols                     = settings.value(QStringLiteral("nedit.textCols"), 80).toInt();
	tabDistance                  = settings.value(QStringLiteral("nedit.tabDistance"), 8).toInt();
	emulateTabs                  = settings.value(QStringLiteral("nedit.emulateTabs"), 0).toInt();
	insertTabs                   = settings.value(QStringLiteral("nedit.insertTabs"), true).toBool();
	fontName                     = settings.value(QStringLiteral("nedit.textFont"), DefaultTextFont()).toString();
	shell                        = settings.value(QStringLiteral("nedit.shell"), QStringLiteral("DEFAULT")).toString();
	geometry                     = settings.value(QStringLiteral("nedit.geometry"), QString()).toString();
	tagFile                      = settings.value(QStringLiteral("nedit.tagFile"), QString()).toString();
	wordDelimiters               = settings.value(QStringLiteral("nedit.wordDelimiters"), DEFAULT_DELIMITERS).toString();
	includePaths                 = settings.value(QStringLiteral("nedit.includePaths"), DEFAULT_INCLUDE_PATHS).toStringList();
	serverName                   = settings.value(QStringLiteral("nedit.serverName"), QString()).toString();
	maxPrevOpenFiles             = settings.value(QStringLiteral("nedit.maxPrevOpenFiles"), 30).toInt();
	autoSaveCharLimit            = settings.value(QStringLiteral("nedit.autoSaveCharLimit"), 80).toInt();
	autoSaveOpLimit              = settings.value(QStringLiteral("nedit.autoSaveOpLimit"), 8).toInt();
	smartTags                    = settings.value(QStringLiteral("nedit.smartTags"), true).toBool();
	typingHidesPointer           = settings.value(QStringLiteral("nedit.typingHidesPointer"), false).toBool();
	alwaysCheckRelativeTagsSpecs = settings.value(QStringLiteral("nedit.alwaysCheckRelativeTagsSpecs"), true).toBool();
	colorizeHighlightedText      = settings.value(QStringLiteral("nedit.colorizeHighlightedText"), true).toBool();
	autoWrapPastedText           = settings.value(QStringLiteral("nedit.autoWrapPastedText"), false).toBool();
	heavyCursor                  = settings.value(QStringLiteral("nedit.heavyCursor"), false).toBool();
	prefFileRead                 = settings.value(QStringLiteral("nedit.prefFileRead"), false).toBool();
	findReplaceUsesSelection     = settings.value(QStringLiteral("nedit.findReplaceUsesSelection"), false).toBool();
	titleFormat                  = settings.value(QStringLiteral("nedit.titleFormat"), QLatin1String("{%c} [%s] %f (%S) - %d")).toString();
	undoModifiesSelection        = settings.value(QStringLiteral("nedit.undoModifiesSelection"), true).toBool();
	splitHorizontally            = settings.value(QStringLiteral("nedit.splitHorizontally"), false).toBool();
	truncateLongNamesInTabs      = settings.value(QStringLiteral("nedit.truncateLongNamesInTabs"), 0).toInt();
	focusOnRaise                 = settings.value(QStringLiteral("nedit.focusOnRaise"), false).toBool();
	forceOSConversion            = settings.value(QStringLiteral("nedit.forceOSConversion"), true).toBool();
	honorSymlinks                = settings.value(QStringLiteral("nedit.honorSymlinks"), true).toBool();

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

	shellCommands         = settings.value(QStringLiteral("nedit.shellCommands"), shellCommands).toString();
	macroCommands         = settings.value(QStringLiteral("nedit.macroCommands"), macroCommands).toString();
	bgMenuCommands        = settings.value(QStringLiteral("nedit.bgMenuCommands"), bgMenuCommands).toString();
	highlightPatterns     = settings.value(QStringLiteral("nedit.highlightPatterns"), highlightPatterns).toString();
	languageModes         = settings.value(QStringLiteral("nedit.languageModes"), languageModes).toString();
	smartIndentInit       = settings.value(QStringLiteral("nedit.smartIndentInit"), smartIndentInit).toString();
	smartIndentInitCommon = settings.value(QStringLiteral("nedit.smartIndentInitCommon"), smartIndentInitCommon).toString();

	autoWrap          = ReadEnum(settings, QStringLiteral("nedit.autoWrap"), autoWrap);
	autoIndent        = ReadEnum(settings, QStringLiteral("nedit.autoIndent"), autoIndent);
	showMatching      = ReadEnum(settings, QStringLiteral("nedit.showMatching"), showMatching);
	searchMethod      = ReadEnum(settings, QStringLiteral("nedit.searchMethod"), searchMethod);
	truncSubstitution = ReadEnum(settings, QStringLiteral("nedit.truncSubstitution"), truncSubstitution);

	wrapMargin            = settings.value(QStringLiteral("nedit.wrapMargin"), wrapMargin).toInt();
	autoSave              = settings.value(QStringLiteral("nedit.autoSave"), autoSave).toBool();
	openInTab             = settings.value(QStringLiteral("nedit.openInTab"), openInTab).toBool();
	saveOldVersion        = settings.value(QStringLiteral("nedit.saveOldVersion"), saveOldVersion).toBool();
	matchSyntaxBased      = settings.value(QStringLiteral("nedit.matchSyntaxBased"), matchSyntaxBased).toBool();
	highlightSyntax       = settings.value(QStringLiteral("nedit.highlightSyntax"), highlightSyntax).toBool();
	backlightChars        = settings.value(QStringLiteral("nedit.backlightChars"), backlightChars).toBool();
	backlightCharTypes    = settings.value(QStringLiteral("nedit.backlightCharTypes"), backlightCharTypes).toString();
	searchDialogs         = settings.value(QStringLiteral("nedit.searchDialogs"), searchDialogs).toBool();
	beepOnSearchWrap      = settings.value(QStringLiteral("nedit.beepOnSearchWrap"), beepOnSearchWrap).toBool();
	retainSearchDialogs   = settings.value(QStringLiteral("nedit.retainSearchDialogs"), retainSearchDialogs).toBool();
	searchWraps           = settings.value(QStringLiteral("nedit.searchWraps"), searchWraps).toBool();
	stickyCaseSenseButton = settings.value(QStringLiteral("nedit.stickyCaseSenseButton"), stickyCaseSenseButton).toBool();
	repositionDialogs     = settings.value(QStringLiteral("nedit.repositionDialogs"), repositionDialogs).toBool();
	autoScroll            = settings.value(QStringLiteral("nedit.autoScroll"), autoScroll).toBool();
	autoScrollVPadding    = settings.value(QStringLiteral("nedit.autoScrollVPadding"), autoScrollVPadding).toInt();
	appendLF              = settings.value(QStringLiteral("nedit.appendLF"), appendLF).toBool();
	sortOpenPrevMenu      = settings.value(QStringLiteral("nedit.sortOpenPrevMenu"), sortOpenPrevMenu).toBool();
	statisticsLine        = settings.value(QStringLiteral("nedit.statisticsLine"), statisticsLine).toBool();
	iSearchLine           = settings.value(QStringLiteral("nedit.iSearchLine"), iSearchLine).toBool();
	sortTabs              = settings.value(QStringLiteral("nedit.sortTabs"), sortTabs).toBool();
	tabBar                = settings.value(QStringLiteral("nedit.tabBar"), tabBar).toBool();
	tabBarHideOne         = settings.value(QStringLiteral("nedit.tabBarHideOne"), tabBarHideOne).toBool();
	toolTips              = settings.value(QStringLiteral("nedit.toolTips"), toolTips).toBool();
	globalTabNavigate     = settings.value(QStringLiteral("nedit.globalTabNavigate"), globalTabNavigate).toBool();
	lineNumbers           = settings.value(QStringLiteral("nedit.lineNumbers"), lineNumbers).toBool();
	pathInWindowsMenu     = settings.value(QStringLiteral("nedit.pathInWindowsMenu"), pathInWindowsMenu).toBool();
	warnFileMods          = settings.value(QStringLiteral("nedit.warnFileMods"), warnFileMods).toBool();
	warnRealFileMods      = settings.value(QStringLiteral("nedit.warnRealFileMods"), warnRealFileMods).toBool();
	warnExit              = settings.value(QStringLiteral("nedit.warnExit"), warnExit).toBool();
	smartHome             = settings.value(QStringLiteral("nedit.smartHome"), smartHome).toBool();

	textRows                     = settings.value(QStringLiteral("nedit.textRows"), textRows).toInt();
	textCols                     = settings.value(QStringLiteral("nedit.textCols"), textCols).toInt();
	tabDistance                  = settings.value(QStringLiteral("nedit.tabDistance"), tabDistance).toInt();
	emulateTabs                  = settings.value(QStringLiteral("nedit.emulateTabs"), emulateTabs).toInt();
	insertTabs                   = settings.value(QStringLiteral("nedit.insertTabs"), insertTabs).toBool();
	fontName                     = settings.value(QStringLiteral("nedit.textFont"), fontName).toString();
	shell                        = settings.value(QStringLiteral("nedit.shell"), shell).toString();
	geometry                     = settings.value(QStringLiteral("nedit.geometry"), geometry).toString();
	tagFile                      = settings.value(QStringLiteral("nedit.tagFile"), tagFile).toString();
	wordDelimiters               = settings.value(QStringLiteral("nedit.wordDelimiters"), wordDelimiters).toString();
	includePaths                 = settings.value(QStringLiteral("nedit.includePaths"), includePaths).toStringList();
	serverName                   = settings.value(QStringLiteral("nedit.serverName"), serverName).toString();
	maxPrevOpenFiles             = settings.value(QStringLiteral("nedit.maxPrevOpenFiles"), maxPrevOpenFiles).toInt();
	autoSaveCharLimit            = settings.value(QStringLiteral("nedit.autoSaveCharLimit"), autoSaveCharLimit).toInt();
	autoSaveOpLimit              = settings.value(QStringLiteral("nedit.autoSaveOpLimit"), autoSaveOpLimit).toInt();
	smartTags                    = settings.value(QStringLiteral("nedit.smartTags"), smartTags).toBool();
	typingHidesPointer           = settings.value(QStringLiteral("nedit.typingHidesPointer"), typingHidesPointer).toBool();
	alwaysCheckRelativeTagsSpecs = settings.value(QStringLiteral("nedit.alwaysCheckRelativeTagsSpecs"), alwaysCheckRelativeTagsSpecs).toBool();
	heavyCursor                  = settings.value(QStringLiteral("nedit.heavyCursor"), heavyCursor).toBool();
	autoWrapPastedText           = settings.value(QStringLiteral("nedit.autoWrapPastedText"), autoWrapPastedText).toBool();
	colorizeHighlightedText      = settings.value(QStringLiteral("nedit.colorizeHighlightedText"), colorizeHighlightedText).toBool();
	prefFileRead                 = settings.value(QStringLiteral("nedit.prefFileRead"), prefFileRead).toBool();
	findReplaceUsesSelection     = settings.value(QStringLiteral("nedit.findReplaceUsesSelection"), findReplaceUsesSelection).toBool();
	titleFormat                  = settings.value(QStringLiteral("nedit.titleFormat"), titleFormat).toString();
	undoModifiesSelection        = settings.value(QStringLiteral("nedit.undoModifiesSelection"), undoModifiesSelection).toBool();
	splitHorizontally            = settings.value(QStringLiteral("nedit.splitHorizontally"), splitHorizontally).toBool();
	truncateLongNamesInTabs      = settings.value(QStringLiteral("nedit.truncateLongNamesInTabs"), truncateLongNamesInTabs).toInt();
	focusOnRaise                 = settings.value(QStringLiteral("nedit.focusOnRaise"), focusOnRaise).toBool();
	forceOSConversion            = settings.value(QStringLiteral("nedit.forceOSConversion"), forceOSConversion).toBool();
	honorSymlinks                = settings.value(QStringLiteral("nedit.honorSymlinks"), honorSymlinks).toBool();
}

/**
 * @brief Saves the user preferences to the configuration file.
 *
 * @return `true` if the preferences were saved successfully, `false` otherwise.
 */
bool Save() {
	const QString filename = ConfigFile();
	QSettings settings(filename, QSettings::IniFormat);

	settings.setValue(QStringLiteral("nedit.shellCommands"), shellCommands);
	settings.setValue(QStringLiteral("nedit.macroCommands"), macroCommands);
	settings.setValue(QStringLiteral("nedit.bgMenuCommands"), bgMenuCommands);
	settings.setValue(QStringLiteral("nedit.highlightPatterns"), highlightPatterns);
	settings.setValue(QStringLiteral("nedit.languageModes"), languageModes);
	settings.setValue(QStringLiteral("nedit.smartIndentInit"), smartIndentInit);
	settings.setValue(QStringLiteral("nedit.smartIndentInitCommon"), smartIndentInitCommon);

	WriteEnum(settings, QStringLiteral("nedit.autoWrap"), autoWrap);
	WriteEnum(settings, QStringLiteral("nedit.autoIndent"), autoIndent);
	WriteEnum(settings, QStringLiteral("nedit.showMatching"), showMatching);
	WriteEnum(settings, QStringLiteral("nedit.searchMethod"), searchMethod);
	WriteEnum(settings, QStringLiteral("nedit.truncSubstitution"), truncSubstitution);

	settings.setValue(QStringLiteral("nedit.wrapMargin"), wrapMargin);
	settings.setValue(QStringLiteral("nedit.autoSave"), autoSave);
	settings.setValue(QStringLiteral("nedit.openInTab"), openInTab);
	settings.setValue(QStringLiteral("nedit.saveOldVersion"), saveOldVersion);
	settings.setValue(QStringLiteral("nedit.matchSyntaxBased"), matchSyntaxBased);
	settings.setValue(QStringLiteral("nedit.highlightSyntax"), highlightSyntax);
	settings.setValue(QStringLiteral("nedit.backlightChars"), backlightChars);
	settings.setValue(QStringLiteral("nedit.backlightCharTypes"), backlightCharTypes);
	settings.setValue(QStringLiteral("nedit.searchDialogs"), searchDialogs);
	settings.setValue(QStringLiteral("nedit.beepOnSearchWrap"), beepOnSearchWrap);
	settings.setValue(QStringLiteral("nedit.retainSearchDialogs"), retainSearchDialogs);
	settings.setValue(QStringLiteral("nedit.searchWraps"), searchWraps);
	settings.setValue(QStringLiteral("nedit.stickyCaseSenseButton"), stickyCaseSenseButton);
	settings.setValue(QStringLiteral("nedit.repositionDialogs"), repositionDialogs);
	settings.setValue(QStringLiteral("nedit.autoScroll"), autoScroll);
	settings.setValue(QStringLiteral("nedit.autoScrollVPadding"), autoScrollVPadding);
	settings.setValue(QStringLiteral("nedit.appendLF"), appendLF);
	settings.setValue(QStringLiteral("nedit.sortOpenPrevMenu"), sortOpenPrevMenu);
	settings.setValue(QStringLiteral("nedit.statisticsLine"), statisticsLine);
	settings.setValue(QStringLiteral("nedit.iSearchLine"), iSearchLine);
	settings.setValue(QStringLiteral("nedit.sortTabs"), sortTabs);
	settings.setValue(QStringLiteral("nedit.tabBar"), tabBar);
	settings.setValue(QStringLiteral("nedit.tabBarHideOne"), tabBarHideOne);
	settings.setValue(QStringLiteral("nedit.toolTips"), toolTips);
	settings.setValue(QStringLiteral("nedit.globalTabNavigate"), globalTabNavigate);
	settings.setValue(QStringLiteral("nedit.lineNumbers"), lineNumbers);
	settings.setValue(QStringLiteral("nedit.pathInWindowsMenu"), pathInWindowsMenu);
	settings.setValue(QStringLiteral("nedit.warnFileMods"), warnFileMods);
	settings.setValue(QStringLiteral("nedit.warnRealFileMods"), warnRealFileMods);
	settings.setValue(QStringLiteral("nedit.warnExit"), warnExit);
	settings.setValue(QStringLiteral("nedit.smartHome"), smartHome);

	settings.setValue(QStringLiteral("nedit.textRows"), textRows);
	settings.setValue(QStringLiteral("nedit.textCols"), textCols);
	settings.setValue(QStringLiteral("nedit.tabDistance"), tabDistance);
	settings.setValue(QStringLiteral("nedit.emulateTabs"), emulateTabs);
	settings.setValue(QStringLiteral("nedit.insertTabs"), insertTabs);
	settings.setValue(QStringLiteral("nedit.textFont"), fontName);
	settings.setValue(QStringLiteral("nedit.shell"), shell);
	settings.setValue(QStringLiteral("nedit.geometry"), geometry);
	settings.setValue(QStringLiteral("nedit.tagFile"), tagFile);
	settings.setValue(QStringLiteral("nedit.wordDelimiters"), wordDelimiters);
	settings.setValue(QStringLiteral("nedit.includePaths"), includePaths);
	settings.setValue(QStringLiteral("nedit.serverName"), serverName);
	settings.setValue(QStringLiteral("nedit.maxPrevOpenFiles"), maxPrevOpenFiles);
	settings.setValue(QStringLiteral("nedit.autoSaveCharLimit"), autoSaveCharLimit);
	settings.setValue(QStringLiteral("nedit.autoSaveOpLimit"), autoSaveOpLimit);
	settings.setValue(QStringLiteral("nedit.smartTags"), smartTags);
	settings.setValue(QStringLiteral("nedit.typingHidesPointer"), typingHidesPointer);
	settings.setValue(QStringLiteral("nedit.autoWrapPastedText"), autoWrapPastedText);
	settings.setValue(QStringLiteral("nedit.heavyCursor"), heavyCursor);
	settings.setValue(QStringLiteral("nedit.alwaysCheckRelativeTagsSpecs"), alwaysCheckRelativeTagsSpecs);
	settings.setValue(QStringLiteral("nedit.colorizeHighlightedText"), colorizeHighlightedText);
	settings.setValue(QStringLiteral("nedit.prefFileRead"), prefFileRead);
	settings.setValue(QStringLiteral("nedit.findReplaceUsesSelection"), findReplaceUsesSelection);
	settings.setValue(QStringLiteral("nedit.titleFormat"), titleFormat);
	settings.setValue(QStringLiteral("nedit.undoModifiesSelection"), undoModifiesSelection);
	settings.setValue(QStringLiteral("nedit.splitHorizontally"), splitHorizontally);
	settings.setValue(QStringLiteral("nedit.truncateLongNamesInTabs"), truncateLongNamesInTabs);
	settings.setValue(QStringLiteral("nedit.focusOnRaise"), focusOnRaise);
	settings.setValue(QStringLiteral("nedit.forceOSConversion"), forceOSConversion);
	settings.setValue(QStringLiteral("nedit.honorSymlinks"), honorSymlinks);

	settings.sync();
	return settings.status() == QSettings::NoError;
}

}
