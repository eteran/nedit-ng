
#include "Settings.h"
#include "Util/Resource.h"

#include <QFontDatabase>
#include <QSettings>
#include <QStandardPaths>
#include <QtDebug>
#include <QtGlobal>
#include <random>

namespace Settings {

namespace {

bool settingsLoaded_ = false;

const QStringList DEFAULT_INCLUDE_PATHS = {
	QLatin1String("/usr/include/"),
	QLatin1String("/usr/local/include/")};

const auto DEFAULT_DELIMETERS      = QLatin1String(".,/\\`'!|@#%^&*()-=+{}[]\":;<>?");
const auto DEFAULT_BACKLIGHT_CHARS = QLatin1String("0-8,10-31,127:red;9:#dedede;32,160-255:#f0f0f0;128-159:orange");

QString defaultTextFont() {
	QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	fixedFont.setPointSize(12);
	return fixedFont.toString();
}

template <class T>
using IsEnum = typename std::enable_if<std::is_enum<T>::value>::type;

template <class T, class = IsEnum<T>>
T readEnum(QSettings &settings, const QString &key, const T &defaultValue = T()) {
	return from_integer<T>(settings.value(key, static_cast<int>(defaultValue)).toInt());
}

template <class T, class = IsEnum<T>>
void writeEnum(QSettings &settings, const QString &key, const T &value) {
	settings.setValue(key, static_cast<int>(value));
}

/**
 * @brief randomString
 * @param length
 * @return
 */
QString randomString(int length) {
	static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

	QString randomString;
	randomString.reserve(length);

	std::random_device rd;
	std::mt19937 mt(rd());
	std::uniform_int_distribution<> dist(0, sizeof(alphabet) - 1);

	for (int i = 0; i < length; ++i) {
		size_t index = dist(mt);
		randomString.append(QChar::fromLatin1(alphabet[index]));
	}

	return randomString;
}

/**
 * @brief configDirectory
 * @return
 */
QString configDirectory() {
	static const QByteArray nedit_home = qgetenv("NEDIT_NG_HOME");
	if (!nedit_home.isEmpty()) {
		return QString::fromLocal8Bit(nedit_home);
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
QStringList includePaths;
QString macroCommands;
QString serverName;
QString shell;
QString shellCommands;
QString smartIndentInit;
QString smartIndentInitCommon;
QString tagFile;
QString titleFormat;
QString wordDelimiters;
SearchType searchMethod;
ShowMatchingStyle showMatching;
TruncSubstitution truncSubstitution;
WrapStyle autoWrap;

/**
 * @brief themeFile
 * @return
 */
QString themeFile() {
	static const QString configDir = configDirectory();
	static const auto filename     = QStringLiteral("%1/theme.xml").arg(configDir);
	return filename;
}

/**
 * @brief configFile
 * @return
 */
QString configFile() {
	static const QString configDir = configDirectory();
	static const auto filename     = QStringLiteral("%1/config.ini").arg(configDir);
	return filename;
}

/**
 * @brief historyFile
 * @return
 */
QString historyFile() {
	static const QString configDir = configDirectory();
	static const auto filename     = QStringLiteral("%1/history").arg(configDir);
	return filename;
}

/**
 * @brief autoLoadMacroFile
 * @return
 */
QString autoLoadMacroFile() {
	static const QString configDir = configDirectory();
	static const auto filename     = QStringLiteral("%1/autoload.nm").arg(configDir);
	return filename;
}

/**
 * @brief languageModeFile
 * @return
 */
QString languageModeFile() {
	static const QString configDir = configDirectory();
	static const auto filename     = QStringLiteral("%1/languages.yaml").arg(configDir);
	return filename;
}

/**
 * @brief macroMenuFile
 * @return
 */
QString macroMenuFile() {
	static const QString configDir = configDirectory();
	static const auto filename     = QStringLiteral("%1/macros.yaml").arg(configDir);
	return filename;
}

/**
 * @brief shellMenuFile
 * @return
 */
QString shellMenuFile() {
	static const QString configDir = configDirectory();
	static const auto filename     = QStringLiteral("%1/shell.yaml").arg(configDir);
	return filename;
}

/**
 * @brief contextMenuFile
 * @return
 */
QString contextMenuFile() {
	static const QString configDir = configDirectory();
	static const auto filename     = QStringLiteral("%1/context.yaml").arg(configDir);
	return filename;
}

/**
 * @brief styleFile
 * @return
 */
QString styleFile() {
	static const QString configDir = configDirectory();
	static const auto filename     = QStringLiteral("%1/style.qss").arg(configDir);
	return filename;
}

/**
 * @brief highlightPatternsFile
 * @return
 */
QString highlightPatternsFile() {
	static const QString configDir = configDirectory();
	static const auto filename     = QStringLiteral("%1/patterns.yaml").arg(configDir);
	return filename;
}

/**
 * @brief smartIndentFile
 * @return
 */
QString smartIndentFile() {
	static const QString configDir = configDirectory();
	static const auto filename     = QStringLiteral("%1/indent.yaml").arg(configDir);
	return filename;
}

/**
 * @brief loadPreferences
 */
void loadPreferences(bool isServer) {

	QString filename = configFile();
	QSettings settings(filename, QSettings::IniFormat);

	shellCommands         = settings.value(tr("nedit.shellCommands"), QLatin1String("*")).toString();
	macroCommands         = settings.value(tr("nedit.macroCommands"), QLatin1String("*")).toString();
	bgMenuCommands        = settings.value(tr("nedit.bgMenuCommands"), QLatin1String("*")).toString();
	highlightPatterns     = settings.value(tr("nedit.highlightPatterns"), QLatin1String("*")).toString();
	languageModes         = settings.value(tr("nedit.languageModes"), QLatin1String("*")).toString();
	smartIndentInit       = settings.value(tr("nedit.smartIndentInit"), QLatin1String("*")).toString();
	smartIndentInitCommon = settings.value(tr("nedit.smartIndentInitCommon"), QLatin1String("*")).toString();

	autoWrap          = readEnum(settings, tr("nedit.autoWrap"), WrapStyle::None);
	autoIndent        = readEnum(settings, tr("nedit.autoIndent"), IndentStyle::Auto);
	showMatching      = readEnum(settings, tr("nedit.showMatching"), ShowMatchingStyle::Delimiter);
	searchMethod      = readEnum(settings, tr("nedit.searchMethod"), SearchType::Literal);
	truncSubstitution = readEnum(settings, tr("nedit.truncSubstitution"), TruncSubstitution::Fail);

	wrapMargin             = settings.value(tr("nedit.wrapMargin"), 0).toInt();
	autoSave               = settings.value(tr("nedit.autoSave"), true).toBool();
	openInTab              = settings.value(tr("nedit.openInTab"), true).toBool();
	saveOldVersion         = settings.value(tr("nedit.saveOldVersion"), false).toBool();
	matchSyntaxBased       = settings.value(tr("nedit.matchSyntaxBased"), true).toBool();
	highlightSyntax        = settings.value(tr("nedit.highlightSyntax"), true).toBool();
	backlightChars         = settings.value(tr("nedit.backlightChars"), false).toBool();
	backlightCharTypes     = settings.value(tr("nedit.backlightCharTypes"), DEFAULT_BACKLIGHT_CHARS).toString();
	searchDialogs          = settings.value(tr("nedit.searchDialogs"), false).toBool();
	beepOnSearchWrap       = settings.value(tr("nedit.beepOnSearchWrap"), false).toBool();
	retainSearchDialogs    = settings.value(tr("nedit.retainSearchDialogs"), false).toBool();
	searchWraps            = settings.value(tr("nedit.searchWraps"), true).toBool();
	stickyCaseSenseButton  = settings.value(tr("nedit.stickyCaseSenseButton"), true).toBool();
	repositionDialogs      = settings.value(tr("nedit.repositionDialogs"), false).toBool();
	autoScroll             = settings.value(tr("nedit.autoScroll"), false).toBool();
	autoScrollVPadding     = settings.value(tr("nedit.autoScrollVPadding"), 4).toInt();
	appendLF               = settings.value(tr("nedit.appendLF"), true).toBool();
	sortOpenPrevMenu       = settings.value(tr("nedit.sortOpenPrevMenu"), true).toBool();
	statisticsLine         = settings.value(tr("nedit.statisticsLine"), false).toBool();
	iSearchLine            = settings.value(tr("nedit.iSearchLine"), false).toBool();
	sortTabs               = settings.value(tr("nedit.sortTabs"), false).toBool();
	tabBar                 = settings.value(tr("nedit.tabBar"), true).toBool();
	tabBarHideOne          = settings.value(tr("nedit.tabBarHideOne"), true).toBool();
	toolTips               = settings.value(tr("nedit.toolTips"), true).toBool();
	globalTabNavigate      = settings.value(tr("nedit.globalTabNavigate"), false).toBool();
	lineNumbers            = settings.value(tr("nedit.lineNumbers"), false).toBool();
	pathInWindowsMenu      = settings.value(tr("nedit.pathInWindowsMenu"), true).toBool();
	warnFileMods           = settings.value(tr("nedit.warnFileMods"), true).toBool();
	warnRealFileMods       = settings.value(tr("nedit.warnRealFileMods"), true).toBool();
	warnExit               = settings.value(tr("nedit.warnExit"), true).toBool();
	showResizeNotification = settings.value(tr("nedit.showResizeNotification"), false).toBool();
	smartHome              = settings.value(tr("nedit.smartHome"), false).toBool();

	textRows                     = settings.value(tr("nedit.textRows"), 24).toInt();
	textCols                     = settings.value(tr("nedit.textCols"), 80).toInt();
	tabDistance                  = settings.value(tr("nedit.tabDistance"), 8).toInt();
	emulateTabs                  = settings.value(tr("nedit.emulateTabs"), 0).toInt();
	insertTabs                   = settings.value(tr("nedit.insertTabs"), true).toBool();
	fontName                     = settings.value(tr("nedit.textFont"), defaultTextFont()).toString();
	shell                        = settings.value(tr("nedit.shell"), QLatin1String("DEFAULT")).toString();
	geometry                     = settings.value(tr("nedit.geometry"), QString()).toString();
	tagFile                      = settings.value(tr("nedit.tagFile"), QString()).toString();
	wordDelimiters               = settings.value(tr("nedit.wordDelimiters"), DEFAULT_DELIMETERS).toString();
	includePaths                 = settings.value(tr("nedit.includePaths"), DEFAULT_INCLUDE_PATHS).toStringList();
	serverName                   = settings.value(tr("nedit.serverName"), QString()).toString();
	maxPrevOpenFiles             = settings.value(tr("nedit.maxPrevOpenFiles"), 30).toInt();
	autoSaveCharLimit            = settings.value(tr("nedit.autoSaveCharLimit"), 80).toInt();
	autoSaveOpLimit              = settings.value(tr("nedit.autoSaveOpLimit"), 8).toInt();
	smartTags                    = settings.value(tr("nedit.smartTags"), true).toBool();
	typingHidesPointer           = settings.value(tr("nedit.typingHidesPointer"), false).toBool();
	alwaysCheckRelativeTagsSpecs = settings.value(tr("nedit.alwaysCheckRelativeTagsSpecs"), true).toBool();
	colorizeHighlightedText      = settings.value(tr("nedit.colorizeHighlightedText"), true).toBool();
	autoWrapPastedText           = settings.value(tr("nedit.autoWrapPastedText"), false).toBool();
	heavyCursor                  = settings.value(tr("nedit.heavyCursor"), false).toBool();
	prefFileRead                 = settings.value(tr("nedit.prefFileRead"), false).toBool();
	findReplaceUsesSelection     = settings.value(tr("nedit.findReplaceUsesSelection"), false).toBool();
	titleFormat                  = settings.value(tr("nedit.titleFormat"), QLatin1String("{%c} [%s] %f (%S) - %d")).toString();
	undoModifiesSelection        = settings.value(tr("nedit.undoModifiesSelection"), true).toBool();
	splitHorizontally            = settings.value(tr("nedit.splitHorizontally"), false).toBool();
	truncateLongNamesInTabs      = settings.value(tr("nedit.truncateLongNamesInTabs"), 0).toInt();
	focusOnRaise                 = settings.value(tr("nedit.focusOnRaise"), false).toBool();
	forceOSConversion            = settings.value(tr("nedit.forceOSConversion"), true).toBool();
	honorSymlinks                = settings.value(tr("nedit.honorSymlinks"), true).toBool();

	if (isServer && serverName.isEmpty()) {
		serverName = randomString(8);
	}

	if (includePaths.isEmpty()) {
		includePaths.append(DEFAULT_INCLUDE_PATHS[0]);
	}

	settingsLoaded_ = true;
}

/**
 * @brief importSettings
 * @param filename
 */
void importSettings(const QString &filename) {
	if (!settingsLoaded_) {
		qWarning("NEdit: Warning, importing while no previous settings loaded!");
	}

	QSettings settings(filename, QSettings::IniFormat);

	shellCommands         = settings.value(tr("nedit.shellCommands"), shellCommands).toString();
	macroCommands         = settings.value(tr("nedit.macroCommands"), macroCommands).toString();
	bgMenuCommands        = settings.value(tr("nedit.bgMenuCommands"), bgMenuCommands).toString();
	highlightPatterns     = settings.value(tr("nedit.highlightPatterns"), highlightPatterns).toString();
	languageModes         = settings.value(tr("nedit.languageModes"), languageModes).toString();
	smartIndentInit       = settings.value(tr("nedit.smartIndentInit"), smartIndentInit).toString();
	smartIndentInitCommon = settings.value(tr("nedit.smartIndentInitCommon"), smartIndentInitCommon).toString();

	autoWrap          = readEnum(settings, tr("nedit.autoWrap"), autoWrap);
	autoIndent        = readEnum(settings, tr("nedit.autoIndent"), autoIndent);
	showMatching      = readEnum(settings, tr("nedit.showMatching"), showMatching);
	searchMethod      = readEnum(settings, tr("nedit.searchMethod"), searchMethod);
	truncSubstitution = readEnum(settings, tr("nedit.truncSubstitution"), truncSubstitution);

	wrapMargin            = settings.value(tr("nedit.wrapMargin"), wrapMargin).toInt();
	autoSave              = settings.value(tr("nedit.autoSave"), autoSave).toBool();
	openInTab             = settings.value(tr("nedit.openInTab"), openInTab).toBool();
	saveOldVersion        = settings.value(tr("nedit.saveOldVersion"), saveOldVersion).toBool();
	matchSyntaxBased      = settings.value(tr("nedit.matchSyntaxBased"), matchSyntaxBased).toBool();
	highlightSyntax       = settings.value(tr("nedit.highlightSyntax"), highlightSyntax).toBool();
	backlightChars        = settings.value(tr("nedit.backlightChars"), backlightChars).toBool();
	backlightCharTypes    = settings.value(tr("nedit.backlightCharTypes"), backlightCharTypes).toString();
	searchDialogs         = settings.value(tr("nedit.searchDialogs"), searchDialogs).toBool();
	beepOnSearchWrap      = settings.value(tr("nedit.beepOnSearchWrap"), beepOnSearchWrap).toBool();
	retainSearchDialogs   = settings.value(tr("nedit.retainSearchDialogs"), retainSearchDialogs).toBool();
	searchWraps           = settings.value(tr("nedit.searchWraps"), searchWraps).toBool();
	stickyCaseSenseButton = settings.value(tr("nedit.stickyCaseSenseButton"), stickyCaseSenseButton).toBool();
	repositionDialogs     = settings.value(tr("nedit.repositionDialogs"), repositionDialogs).toBool();
	autoScroll            = settings.value(tr("nedit.autoScroll"), autoScroll).toBool();
	autoScrollVPadding    = settings.value(tr("nedit.autoScrollVPadding"), autoScrollVPadding).toInt();
	appendLF              = settings.value(tr("nedit.appendLF"), appendLF).toBool();
	sortOpenPrevMenu      = settings.value(tr("nedit.sortOpenPrevMenu"), sortOpenPrevMenu).toBool();
	statisticsLine        = settings.value(tr("nedit.statisticsLine"), statisticsLine).toBool();
	iSearchLine           = settings.value(tr("nedit.iSearchLine"), iSearchLine).toBool();
	sortTabs              = settings.value(tr("nedit.sortTabs"), sortTabs).toBool();
	tabBar                = settings.value(tr("nedit.tabBar"), tabBar).toBool();
	tabBarHideOne         = settings.value(tr("nedit.tabBarHideOne"), tabBarHideOne).toBool();
	toolTips              = settings.value(tr("nedit.toolTips"), toolTips).toBool();
	globalTabNavigate     = settings.value(tr("nedit.globalTabNavigate"), globalTabNavigate).toBool();
	lineNumbers           = settings.value(tr("nedit.lineNumbers"), lineNumbers).toBool();
	pathInWindowsMenu     = settings.value(tr("nedit.pathInWindowsMenu"), pathInWindowsMenu).toBool();
	warnFileMods          = settings.value(tr("nedit.warnFileMods"), warnFileMods).toBool();
	warnRealFileMods      = settings.value(tr("nedit.warnRealFileMods"), warnRealFileMods).toBool();
	warnExit              = settings.value(tr("nedit.warnExit"), warnExit).toBool();
	smartHome             = settings.value(tr("nedit.smartHome"), smartHome).toBool();

	textRows                     = settings.value(tr("nedit.textRows"), textRows).toInt();
	textCols                     = settings.value(tr("nedit.textCols"), textCols).toInt();
	tabDistance                  = settings.value(tr("nedit.tabDistance"), tabDistance).toInt();
	emulateTabs                  = settings.value(tr("nedit.emulateTabs"), emulateTabs).toInt();
	insertTabs                   = settings.value(tr("nedit.insertTabs"), insertTabs).toBool();
	fontName                     = settings.value(tr("nedit.textFont"), fontName).toString();
	shell                        = settings.value(tr("nedit.shell"), shell).toString();
	geometry                     = settings.value(tr("nedit.geometry"), geometry).toString();
	tagFile                      = settings.value(tr("nedit.tagFile"), tagFile).toString();
	wordDelimiters               = settings.value(tr("nedit.wordDelimiters"), wordDelimiters).toString();
	includePaths                 = settings.value(tr("nedit.includePaths"), includePaths).toStringList();
	serverName                   = settings.value(tr("nedit.serverName"), serverName).toString();
	maxPrevOpenFiles             = settings.value(tr("nedit.maxPrevOpenFiles"), maxPrevOpenFiles).toInt();
	autoSaveCharLimit            = settings.value(tr("nedit.autoSaveCharLimit"), autoSaveCharLimit).toInt();
	autoSaveOpLimit              = settings.value(tr("nedit.autoSaveOpLimit"), autoSaveOpLimit).toInt();
	smartTags                    = settings.value(tr("nedit.smartTags"), smartTags).toBool();
	typingHidesPointer           = settings.value(tr("nedit.typingHidesPointer"), typingHidesPointer).toBool();
	alwaysCheckRelativeTagsSpecs = settings.value(tr("nedit.alwaysCheckRelativeTagsSpecs"), alwaysCheckRelativeTagsSpecs).toBool();
	heavyCursor                  = settings.value(tr("nedit.heavyCursor"), heavyCursor).toBool();
	autoWrapPastedText           = settings.value(tr("nedit.autoWrapPastedText"), autoWrapPastedText).toBool();
	colorizeHighlightedText      = settings.value(tr("nedit.colorizeHighlightedText"), colorizeHighlightedText).toBool();
	prefFileRead                 = settings.value(tr("nedit.prefFileRead"), prefFileRead).toBool();
	findReplaceUsesSelection     = settings.value(tr("nedit.findReplaceUsesSelection"), findReplaceUsesSelection).toBool();
	titleFormat                  = settings.value(tr("nedit.titleFormat"), titleFormat).toString();
	undoModifiesSelection        = settings.value(tr("nedit.undoModifiesSelection"), undoModifiesSelection).toBool();
	splitHorizontally            = settings.value(tr("nedit.splitHorizontally"), splitHorizontally).toBool();
	truncateLongNamesInTabs      = settings.value(tr("nedit.truncateLongNamesInTabs"), truncateLongNamesInTabs).toInt();
	focusOnRaise                 = settings.value(tr("nedit.focusOnRaise"), focusOnRaise).toBool();
	forceOSConversion            = settings.value(tr("nedit.forceOSConversion"), forceOSConversion).toBool();
	honorSymlinks                = settings.value(tr("nedit.honorSymlinks"), honorSymlinks).toBool();
}

/**
 * @brief savePreferences
 * @return
 */
bool savePreferences() {
	QString filename = configFile();
	QSettings settings(filename, QSettings::IniFormat);

	settings.setValue(tr("nedit.shellCommands"), shellCommands);
	settings.setValue(tr("nedit.macroCommands"), macroCommands);
	settings.setValue(tr("nedit.bgMenuCommands"), bgMenuCommands);
	settings.setValue(tr("nedit.highlightPatterns"), highlightPatterns);
	settings.setValue(tr("nedit.languageModes"), languageModes);
	settings.setValue(tr("nedit.smartIndentInit"), smartIndentInit);
	settings.setValue(tr("nedit.smartIndentInitCommon"), smartIndentInitCommon);

	writeEnum(settings, tr("nedit.autoWrap"), autoWrap);
	writeEnum(settings, tr("nedit.autoIndent"), autoIndent);
	writeEnum(settings, tr("nedit.showMatching"), showMatching);
	writeEnum(settings, tr("nedit.searchMethod"), searchMethod);
	writeEnum(settings, tr("nedit.truncSubstitution"), truncSubstitution);

	settings.setValue(tr("nedit.wrapMargin"), wrapMargin);
	settings.setValue(tr("nedit.autoSave"), autoSave);
	settings.setValue(tr("nedit.openInTab"), openInTab);
	settings.setValue(tr("nedit.saveOldVersion"), saveOldVersion);
	settings.setValue(tr("nedit.matchSyntaxBased"), matchSyntaxBased);
	settings.setValue(tr("nedit.highlightSyntax"), highlightSyntax);
	settings.setValue(tr("nedit.backlightChars"), backlightChars);
	settings.setValue(tr("nedit.backlightCharTypes"), backlightCharTypes);
	settings.setValue(tr("nedit.searchDialogs"), searchDialogs);
	settings.setValue(tr("nedit.beepOnSearchWrap"), beepOnSearchWrap);
	settings.setValue(tr("nedit.retainSearchDialogs"), retainSearchDialogs);
	settings.setValue(tr("nedit.searchWraps"), searchWraps);
	settings.setValue(tr("nedit.stickyCaseSenseButton"), stickyCaseSenseButton);
	settings.setValue(tr("nedit.repositionDialogs"), repositionDialogs);
	settings.setValue(tr("nedit.autoScroll"), autoScroll);
	settings.setValue(tr("nedit.autoScrollVPadding"), autoScrollVPadding);
	settings.setValue(tr("nedit.appendLF"), appendLF);
	settings.setValue(tr("nedit.sortOpenPrevMenu"), sortOpenPrevMenu);
	settings.setValue(tr("nedit.statisticsLine"), statisticsLine);
	settings.setValue(tr("nedit.iSearchLine"), iSearchLine);
	settings.setValue(tr("nedit.sortTabs"), sortTabs);
	settings.setValue(tr("nedit.tabBar"), tabBar);
	settings.setValue(tr("nedit.tabBarHideOne"), tabBarHideOne);
	settings.setValue(tr("nedit.toolTips"), toolTips);
	settings.setValue(tr("nedit.globalTabNavigate"), globalTabNavigate);
	settings.setValue(tr("nedit.lineNumbers"), lineNumbers);
	settings.setValue(tr("nedit.pathInWindowsMenu"), pathInWindowsMenu);
	settings.setValue(tr("nedit.warnFileMods"), warnFileMods);
	settings.setValue(tr("nedit.warnRealFileMods"), warnRealFileMods);
	settings.setValue(tr("nedit.warnExit"), warnExit);
	settings.setValue(tr("nedit.smartHome"), smartHome);

	settings.setValue(tr("nedit.textRows"), textRows);
	settings.setValue(tr("nedit.textCols"), textCols);
	settings.setValue(tr("nedit.tabDistance"), tabDistance);
	settings.setValue(tr("nedit.emulateTabs"), emulateTabs);
	settings.setValue(tr("nedit.insertTabs"), insertTabs);
	settings.setValue(tr("nedit.textFont"), fontName);
	settings.setValue(tr("nedit.shell"), shell);
	settings.setValue(tr("nedit.geometry"), geometry);
	settings.setValue(tr("nedit.tagFile"), tagFile);
	settings.setValue(tr("nedit.wordDelimiters"), wordDelimiters);
	settings.setValue(tr("nedit.includePaths"), includePaths);
	settings.setValue(tr("nedit.serverName"), serverName);
	settings.setValue(tr("nedit.maxPrevOpenFiles"), maxPrevOpenFiles);
	settings.setValue(tr("nedit.autoSaveCharLimit"), autoSaveCharLimit);
	settings.setValue(tr("nedit.autoSaveOpLimit"), autoSaveOpLimit);
	settings.setValue(tr("nedit.smartTags"), smartTags);
	settings.setValue(tr("nedit.typingHidesPointer"), typingHidesPointer);
	settings.setValue(tr("nedit.autoWrapPastedText"), autoWrapPastedText);
	settings.setValue(tr("nedit.heavyCursor"), heavyCursor);
	settings.setValue(tr("nedit.alwaysCheckRelativeTagsSpecs"), alwaysCheckRelativeTagsSpecs);
	settings.setValue(tr("nedit.colorizeHighlightedText"), colorizeHighlightedText);
	settings.setValue(tr("nedit.prefFileRead"), prefFileRead);
	settings.setValue(tr("nedit.findReplaceUsesSelection"), findReplaceUsesSelection);
	settings.setValue(tr("nedit.titleFormat"), titleFormat);
	settings.setValue(tr("nedit.undoModifiesSelection"), undoModifiesSelection);
	settings.setValue(tr("nedit.splitHorizontally"), splitHorizontally);
	settings.setValue(tr("nedit.truncateLongNamesInTabs"), truncateLongNamesInTabs);
	settings.setValue(tr("nedit.focusOnRaise"), focusOnRaise);
	settings.setValue(tr("nedit.forceOSConversion"), forceOSConversion);
	settings.setValue(tr("nedit.honorSymlinks"), honorSymlinks);

	settings.sync();
	return settings.status() == QSettings::NoError;
}

}
