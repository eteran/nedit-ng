
#include "SearchType.h"
#include "Settings.h"
#include "Util/Input.h"
#include "WrapStyle.h"
#include "parse.h"

#include <QColor>
#include <QDomDocument>
#include <QDomElement>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>
#include <QVariant>

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

// Placing these before X11 includes because that header does defines some
// inconvenient macros :-(
namespace {

struct Style {
	QString name;
	QString foreground;
	QString background;
	QString font;
};

struct MenuItem {
	QString name;
	QString shortcut;
	QString mnemonic;
	QString command;
	QString flags;
};

/**
 * @brief Writes a menu item string in the format used by NEdit.
 *
 * @param menuItems The list of menu items to write.
 * @param isShellCommand If `true`, the command is a shell command, otherwise it is a macro.
 * @return The formatted menu item string.
 */
QString WriteMenuItemString(const std::vector<MenuItem> &menuItems, bool isShellCommand) {

	QString outStr;
	auto outPtr = std::back_inserter(outStr);

	for (const MenuItem &item : menuItems) {

		QString accStr = item.shortcut;
		*outPtr++      = QLatin1Char('\t');

		QString name = item.name;

		name.replace(QLatin1String("&"), QLatin1String("&&"));

		// handle that we do mnemonics the Qt way...
		if (!item.mnemonic.isEmpty()) {
			const int n = name.indexOf(item.mnemonic);
			if (n != -1) {
				name.insert(n, QLatin1Char('&'));
			}
		}

		std::copy(name.begin(), name.end(), outPtr);
		*outPtr++ = QLatin1Char(':');

		std::copy(accStr.begin(), accStr.end(), outPtr);
		*outPtr++ = QLatin1Char(':');

		if (isShellCommand) {
			std::copy(item.flags.begin(), item.flags.end(), outPtr);

			*outPtr++ = QLatin1Char(':');
		} else {
			std::copy(item.flags.begin(), item.flags.end(), outPtr);

			*outPtr++ = QLatin1Char(':');
			*outPtr++ = QLatin1Char(' ');
			*outPtr++ = QLatin1Char('{');
		}

		*outPtr++ = QLatin1Char('\n');
		*outPtr++ = QLatin1Char('\t');
		*outPtr++ = QLatin1Char('\t');

		Q_FOREACH (const QChar ch, item.command) {
			if (ch == QLatin1Char('\n')) { // and newlines to backslash-n's,
				*outPtr++ = QLatin1Char('\n');
				*outPtr++ = QLatin1Char('\t');
				*outPtr++ = QLatin1Char('\t');
			} else {
				*outPtr++ = ch;
			}
		}

		if (!isShellCommand) {

			if (outStr.endsWith(QLatin1Char('\t'))) {
				outStr.chop(1);
			}

			*outPtr++ = QLatin1Char('}');
		}

		*outPtr++ = QLatin1Char('\n');
	}

	outStr.chop(1);
	return outStr;
}

/**
 * @brief Copies a macro from the input stream to the end of the string.
 *
 * @param in The input stream containing the macro.
 * @return The macro body, or an empty string if the macro is invalid.
 */
QString CopyMacroToEnd(Input &in) {

	Input input = in;

	// Skip over whitespace to find make sure there's a beginning brace
	// to anchor the parse (if not, it will take the whole file)
	input.skipWhitespaceNL();

	const QString code = input.mid();

	if (!code.startsWith(QLatin1Char('{'))) {
		qWarning("Error In Macro/Command String");
		return QString();
	}

	// Parse the input
	int stoppedAt;
	QString errMsg;
	if (!isMacroValid(code, &errMsg, &stoppedAt)) {
		qWarning("Error In Macro/Command String");
		return QString();
	}

	// Copy and return the body of the macro, stripping outer braces and
	// extra leading tabs added by the writer routine
	++input;
	input.skipWhitespace();

	if (*input == QLatin1Char('\n')) {
		++input;
	}

	if (*input == QLatin1Char('\t')) {
		++input;
	}

	if (*input == QLatin1Char('\t')) {
		++input;
	}

	QString retStr;
	retStr.reserve(stoppedAt - input.index());

	auto retPtr = std::back_inserter(retStr);

	while (input.index() != stoppedAt + in.index()) {
		if (input.match(QLatin1String("\n\t\t"))) {
			*retPtr++ = QLatin1Char('\n');
		} else {
			*retPtr++ = *input++;
		}
	}

	if (retStr.endsWith(QLatin1Char('\t'))) {
		retStr.chop(1);
	}

	// NOTE(eteran): move past the trailing '}'
	++input;

	in = input;

	return retStr;
}

/**
 * @brief Loads a menu item string from the input string.
 *
 * @param inString The input string containing the menu items.
 * @param isShellCommand if `true`, the items are shell commands, otherwise they are macros.
 * @return A vector of MenuItem objects parsed from the input string.
 */
std::vector<MenuItem> LoadMenuItemString(const QString &inString, bool isShellCommand) {

	std::vector<MenuItem> items;

	Input in(&inString);

	Q_FOREVER {

		// remove leading whitespace
		in.skipWhitespace();

		// end of string in proper place
		if (in.atEnd()) {
			return items;
		}

		MenuItem item;

		// read name field
		item.name = in.readUntil(QLatin1Char(':'));
		++in;

		// read shortcut field
		item.shortcut = in.readUntil(QLatin1Char(':'));
		++in;

		// read mnemonic field
		item.mnemonic = in.readUntil(QLatin1Char(':'));
		++in;

		// read flags field
		for (; !in.atEnd() && *in != QLatin1Char(':'); ++in) {
			item.flags.push_back(*in);
		}
		++in;

		// read command field
		if (isShellCommand) {
			in.skipWhitespaceNL();
			item.command = in.readUntil(QLatin1Char('\n'));
		} else {

			QString p = CopyMacroToEnd(in);
			if (p.isNull()) {
				return items;
			}

			item.command = std::move(p);
		}

		in.skipWhitespaceNL();

		items.push_back(item);
	}
}

/**
 * @brief Saves the current theme to a file in XML format.
 *
 * @param filename The name of the file to save the theme to.
 * @param styles The list of styles to save in the theme.
 */
void SaveTheme(const QString &filename, const std::vector<Style> &styles) {

	QFile file(filename);
	if (file.open(QIODevice::WriteOnly)) {
		QDomDocument xml;
		const QDomProcessingInstruction pi = xml.createProcessingInstruction(QLatin1String("xml"), QLatin1String(R"(version="1.0" encoding="UTF-8")"));

		xml.appendChild(pi);

		QDomElement root = xml.createElement(QLatin1String("theme"));
		root.setAttribute(QLatin1String("name"), QLatin1String("default"));
		xml.appendChild(root);

		// save basic color Settings::..
		{
			QDomElement text = xml.createElement(QLatin1String("text"));
			text.setAttribute(QLatin1String("foreground"), Settings::colors[ColorTypes::TEXT_FG_COLOR]);
			text.setAttribute(QLatin1String("background"), Settings::colors[ColorTypes::TEXT_BG_COLOR]);
			root.appendChild(text);
		}

		{
			QDomElement selection = xml.createElement(QLatin1String("selection"));
			selection.setAttribute(QLatin1String("foreground"), Settings::colors[ColorTypes::SELECT_FG_COLOR]);
			selection.setAttribute(QLatin1String("background"), Settings::colors[ColorTypes::SELECT_BG_COLOR]);
			root.appendChild(selection);
		}

		{
			QDomElement highlight = xml.createElement(QLatin1String("highlight"));
			highlight.setAttribute(QLatin1String("foreground"), Settings::colors[ColorTypes::HILITE_FG_COLOR]);
			highlight.setAttribute(QLatin1String("background"), Settings::colors[ColorTypes::HILITE_BG_COLOR]);
			root.appendChild(highlight);
		}

		{
			QDomElement cursor = xml.createElement(QLatin1String("cursor"));
			cursor.setAttribute(QLatin1String("foreground"), Settings::colors[ColorTypes::CURSOR_FG_COLOR]);
			root.appendChild(cursor);
		}

		{
			QDomElement lineno = xml.createElement(QLatin1String("line-numbers"));
			lineno.setAttribute(QLatin1String("foreground"), Settings::colors[ColorTypes::LINENO_FG_COLOR]);
			root.appendChild(lineno);
		}

		// save styles for syntax highlighting...
		for (const Style &hs : styles) {
			QDomElement style = xml.createElement(QLatin1String("style"));
			style.setAttribute(QLatin1String("name"), hs.name);
			style.setAttribute(QLatin1String("foreground"), hs.foreground);
			if (!hs.background.isEmpty()) {
				style.setAttribute(QLatin1String("background"), hs.background);
			}
			style.setAttribute(QLatin1String("font"), hs.font);

			root.appendChild(style);
		}

		QTextStream stream(&file);
		stream << xml.toString(4);
	}
}

/**
 * @brief Converts a string to a specific enum type.
 *
 * @param str The string to convert.
 * @return The converted enum value.
 */
template <class T>
T FromString(const QString &str);

/**
 * @brief Converts a string to a SearchType enum value.
 *
 * @param str The string to convert.
 * @return The converted SearchType value.
 */
template <>
SearchType FromString(const QString &str) {

	if (str == QLatin1String("Literal")) {
		return SearchType::Literal;
	}
	if (str == QLatin1String("CaseSense")) {
		return SearchType::CaseSense;
	}
	if (str == QLatin1String("RegExp")) {
		return SearchType::Regex;
	}
	if (str == QLatin1String("LiteralWord")) {
		return SearchType::LiteralWord;
	}
	if (str == QLatin1String("CaseSenseWord")) {
		return SearchType::CaseSenseWord;
	}
	if (str == QLatin1String("RegExpNoCase")) {
		return SearchType::RegexNoCase;
	}

	qDebug() << "Invalid configuration value: " << str;
	abort();
}

/**
 * @brief Converts a string to a WrapStyle enum value.
 *
 * @param str The string to convert.
 * @return The converted WrapStyle value.
 */
template <>
WrapStyle FromString(const QString &str) {

	if (str == QLatin1String("None")) {
		return WrapStyle::None;
	}
	if (str == QLatin1String("Newline")) {
		return WrapStyle::Newline;
	}
	if (str == QLatin1String("Continuous")) {
		return WrapStyle::Continuous;
	}

	qDebug() << "Invalid configuration value: " << str;
	abort();
}

/**
 * @brief Converts a string to a IndentStyle enum value.
 *
 * @param str The string to convert.
 * @return The converted IndentStyle value.
 */
template <>
IndentStyle FromString(const QString &str) {

	if (str == QLatin1String("None")) {
		return IndentStyle::None;
	}
	if (str == QLatin1String("Auto")) {
		return IndentStyle::Auto;
	}
	if (str == QLatin1String("Smart")) {
		return IndentStyle::Smart;
	}

	qDebug() << "Invalid configuration value: " << str;
	abort();
}

/**
 * @brief Converts a string to a ShowMatchingStyle enum value.
 *
 * @param str The string to convert.
 * @return The converted ShowMatchingStyle value.
 */
template <>
ShowMatchingStyle FromString(const QString &str) {

	/* For backward compatibility, "False" and "True" are still accepted.
	   They are internally converted to "Off" and "Delimiter" respectively.*/

	if (str == QLatin1String("Off")) {
		return ShowMatchingStyle::None;
	}
	if (str == QLatin1String("Delimiter")) {
		return ShowMatchingStyle::Delimiter;
	}
	if (str == QLatin1String("Range")) {
		return ShowMatchingStyle::Range;
	}
	if (str == QLatin1String("False")) {
		return ShowMatchingStyle::None;
	}
	if (str == QLatin1String("True")) {
		return ShowMatchingStyle::Delimiter;
	}

	qDebug() << "Invalid configuration value: " << str;
	abort();
}

}

#include <X11/Xresource.h>

#define APP_CLASS "NEdit" /* application class for loading resources */

namespace {

/**
 * @brief Reads a resource from the XrmDatabase.
 *
 * @param db The XrmDatabase to read from.
 * @param name The name of the resource to read.
 * @return The value of the resource as a QString, bool, or int.
 *         The type is determined by the template parameter T.
 */
template <class T>
T ReadResource(XrmDatabase db, const char *name);

/**
 * @brief Reads a resource from the XrmDatabase and returns it as a QString.
 *
 * @param db The XrmDatabase to read from.
 * @param name The name of the resource to read.
 * @return The value of the resource as a QString.
 */
template <>
QString ReadResource(XrmDatabase db, const char *name) {
	char *type;
	XrmValue resValue;
	XrmGetResource(db, name, APP_CLASS, &type, &resValue);
	auto ret = QString::fromLatin1(resValue.addr, static_cast<int>(resValue.size - 1));
	Q_ASSERT(!ret.isEmpty());
	return ret;
}

/**
 * @brief Reads a resource from the XrmDatabase and returns it as a bool.
 *
 * @param db The XrmDatabase to read from.
 * @param name The name of the resource to read.
 * @return The value of the resource as a bool.
 */
template <>
bool ReadResource(XrmDatabase db, const char *name) {
	auto value = ReadResource<QString>(db, name);
	return value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
}

/**
 * @brief Reads a resource from the XrmDatabase and returns it as an int.
 *
 * @param db The XrmDatabase to read from.
 * @param name The name of the resource to read.
 * @return The value of the resource as an int.
 */
template <>
int ReadResource(XrmDatabase db, const char *name) {
	auto value = ReadResource<QString>(db, name);
	bool ok;
	const int n = value.toInt(&ok);
	Q_ASSERT(ok);
	return n;
}

/**
 * @brief Converts a color string in RGB or RGBi format to a hex color string.
 *
 * @param color The color string to convert, in either RGB or RGBi format.
 * @return The color in hex format (e.g., "#RRGGBB").
 */
QString ConvertRgbColor(const QString &color) {

	{
		// convert RGB style colors
		static const QRegularExpression rgb_regex(QLatin1String("rgb:"
																"(?<red>[0-9a-fA-F]{1,4})"
																"/"
																"(?<green>[0-9a-fA-F]{1,4})"
																"/"
																"(?<blue>[0-9a-fA-F]{1,4})"));

		const QRegularExpressionMatch match = rgb_regex.match(color);
		if (match.hasMatch()) {
			const uint16_t r = match.captured(QLatin1String("red")).toUShort(nullptr, 16);
			const uint16_t g = match.captured(QLatin1String("green")).toUShort(nullptr, 16);
			const uint16_t b = match.captured(QLatin1String("blue")).toUShort(nullptr, 16);

			const QColor c(r, g, b);
			auto newColor = QString::asprintf("#%02x%02x%02x",
											  ((c.rgb() & 0x00ff0000) >> 16),
											  ((c.rgb() & 0x0000ff00) >> 8),
											  ((c.rgb() & 0x000000ff) >> 0));
			return newColor;
		}
	}

	{
		// convert RGBi style colors
		// NOTE(eteran): this regex is slightly more brittle than I'd like, but this is such a rarely used format
		// that I'm leaving it this way for now... I'll improve this later
		static const QRegularExpression rgbi_regex(QLatin1String("RGBi:"
																 "(?<red>[0-9]+(\\.[0-9]+)?)"
																 "/"
																 "(?<green>[0-9]+(\\.[0-9]+)?)"
																 "/"
																 "(?<blue>[0-9]+(\\.[0-9]+)?)"));

		const QRegularExpressionMatch match_rgbi = rgbi_regex.match(color);
		if (match_rgbi.hasMatch()) {
			const qreal r = match_rgbi.captured(QLatin1String("red")).toDouble();
			const qreal g = match_rgbi.captured(QLatin1String("green")).toDouble();
			const qreal b = match_rgbi.captured(QLatin1String("blue")).toDouble();

			const QColor c(static_cast<int>(r * 255), static_cast<int>(g * 255), static_cast<int>(b * 255));
			auto newColor = QString::asprintf("#%02x%02x%02x",
											  ((c.rgb() & 0x00ff0000) >> 16),
											  ((c.rgb() & 0x0000ff00) >> 8),
											  ((c.rgb() & 0x000000ff) >> 0));
			return newColor;
		}
	}

	// when in doubt, keep the current color
	return color;
}

}

/**
 * @brief Main function for importing NEdit preferences from a resource file.
 *
 * @param argc The number of command line arguments.
 * @param argv The command line arguments, where argv[1] is the filename of the resource file to import.
 * @return 0 on success, -1 if an error occurred.
 */
int main(int argc, char *argv[]) {

	if (argc != 2) {
		std::cerr << "usage: " << argv[0] << " <filename>" << '\n';
		return -1;
	}

	Display *dpy = XOpenDisplay(nullptr);

	if (!dpy) {
		std::cout << "Could not open DISPLAY." << '\n';
		return -1;
	}

	XrmInitialize();

	std::ifstream file(argv[1]);
	if (!file) {
		std::cout << "Could not open resource file." << '\n';
		return -1;
	}

	auto contents = std::string{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};

	XrmDatabase prefDB = XrmGetStringDatabase(contents.data());

	const QString value = ReadResource<QString>(prefDB, "nedit.fileVersion");

	if (value != QLatin1String("5.6") && value != QLatin1String("5.7")) {
		std::cout << "Importing is only supported for NEdit 5.6 and 5.7" << '\n';
		return -1;
	}

	// Advanced stuff from secondary XResources...
	// make sure that there are some sensible defaults...
	Settings::colorizeHighlightedText      = true;
	Settings::alwaysCheckRelativeTagsSpecs = true;
	Settings::findReplaceUsesSelection     = false;
	Settings::focusOnRaise                 = false;
	Settings::forceOSConversion            = true;
	Settings::honorSymlinks                = true;
	Settings::stickyCaseSenseButton        = true;
	Settings::typingHidesPointer           = false;
	Settings::undoModifiesSelection        = true;
	Settings::autoScrollVPadding           = 4;
	Settings::maxPrevOpenFiles             = 30;
	Settings::truncSubstitution            = TruncSubstitution::Fail;
	Settings::backlightCharTypes           = QLatin1String("0-8,10-31,127:red;9:#dedede;32,160-255:#f0f0f0;128-159:orange");
	Settings::tagFile                      = QString();
	Settings::wordDelimiters               = QLatin1String(".,/\\`'!|@#%^&*()-=+{}[]\":;<>?");

	// string preferences
	Settings::shellCommands  = ReadResource<QString>(prefDB, "nedit.shellCommands");
	Settings::macroCommands  = ReadResource<QString>(prefDB, "nedit.macroCommands");
	Settings::bgMenuCommands = ReadResource<QString>(prefDB, "nedit.bgMenuCommands");

	const std::vector<MenuItem> shellCommands  = LoadMenuItemString(Settings::shellCommands, true);
	const std::vector<MenuItem> macroCommands  = LoadMenuItemString(Settings::macroCommands, false);
	const std::vector<MenuItem> bgMenuCommands = LoadMenuItemString(Settings::bgMenuCommands, false);

	Settings::shellCommands  = WriteMenuItemString(shellCommands, true);
	Settings::macroCommands  = WriteMenuItemString(macroCommands, false);
	Settings::bgMenuCommands = WriteMenuItemString(bgMenuCommands, false);

	Settings::highlightPatterns     = ReadResource<QString>(prefDB, "nedit.highlightPatterns");
	Settings::languageModes         = ReadResource<QString>(prefDB, "nedit.languageModes");
	Settings::smartIndentInit       = ReadResource<QString>(prefDB, "nedit.smartIndentInit");
	Settings::smartIndentInitCommon = ReadResource<QString>(prefDB, "nedit.smartIndentInitCommon");
	Settings::shell                 = ReadResource<QString>(prefDB, "nedit.shell");
	Settings::titleFormat           = ReadResource<QString>(prefDB, "nedit.titleFormat");

	// boolean preferences
	Settings::autoSave              = ReadResource<bool>(prefDB, "nedit.autoSave");
	Settings::saveOldVersion        = ReadResource<bool>(prefDB, "nedit.saveOldVersion");
	Settings::searchDialogs         = ReadResource<bool>(prefDB, "nedit.searchDialogs");
	Settings::retainSearchDialogs   = ReadResource<bool>(prefDB, "nedit.retainSearchDialogs");
	Settings::stickyCaseSenseButton = ReadResource<bool>(prefDB, "nedit.stickyCaseSenseButton");
	Settings::repositionDialogs     = ReadResource<bool>(prefDB, "nedit.repositionDialogs");
	Settings::statisticsLine        = ReadResource<bool>(prefDB, "nedit.statisticsLine");
	Settings::tabBar                = ReadResource<bool>(prefDB, "nedit.tabBar");
	Settings::searchWraps           = ReadResource<bool>(prefDB, "nedit.searchWraps");
	Settings::prefFileRead          = ReadResource<bool>(prefDB, "nedit.prefFileRead");
	Settings::sortOpenPrevMenu      = ReadResource<bool>(prefDB, "nedit.sortOpenPrevMenu");
	Settings::backlightChars        = ReadResource<bool>(prefDB, "nedit.backlightChars");
	Settings::highlightSyntax       = ReadResource<bool>(prefDB, "nedit.highlightSyntax");
	Settings::smartTags             = ReadResource<bool>(prefDB, "nedit.smartTags");
	Settings::insertTabs            = ReadResource<bool>(prefDB, "nedit.insertTabs");
	Settings::warnFileMods          = ReadResource<bool>(prefDB, "nedit.warnFileMods");
	Settings::toolTips              = ReadResource<bool>(prefDB, "nedit.toolTips");
	Settings::tabBarHideOne         = ReadResource<bool>(prefDB, "nedit.tabBarHideOne");
	Settings::globalTabNavigate     = ReadResource<bool>(prefDB, "nedit.globalTabNavigate");
	Settings::sortTabs              = ReadResource<bool>(prefDB, "nedit.sortTabs");
	Settings::appendLF              = ReadResource<bool>(prefDB, "nedit.appendLF");
	Settings::matchSyntaxBased      = ReadResource<bool>(prefDB, "nedit.matchSyntaxBased");
	Settings::beepOnSearchWrap      = ReadResource<bool>(prefDB, "nedit.beepOnSearchWrap");
	Settings::autoScroll            = ReadResource<bool>(prefDB, "nedit.autoScroll");
	Settings::iSearchLine           = ReadResource<bool>(prefDB, "nedit.iSearchLine");
	Settings::lineNumbers           = ReadResource<bool>(prefDB, "nedit.lineNumbers");
	Settings::pathInWindowsMenu     = ReadResource<bool>(prefDB, "nedit.pathInWindowsMenu");
	Settings::warnRealFileMods      = ReadResource<bool>(prefDB, "nedit.warnRealFileMods");
	Settings::warnExit              = ReadResource<bool>(prefDB, "nedit.warnExit");
	Settings::openInTab             = ReadResource<bool>(prefDB, "nedit.openInTab");

	// integer preferences
	Settings::emulateTabs = ReadResource<int>(prefDB, "nedit.emulateTabs");
	Settings::tabDistance = ReadResource<int>(prefDB, "nedit.tabDistance");
	Settings::textRows    = ReadResource<int>(prefDB, "nedit.textRows");
	Settings::textCols    = ReadResource<int>(prefDB, "nedit.textCols");
	Settings::wrapMargin  = ReadResource<int>(prefDB, "nedit.wrapMargin");

	// enumeration values
	Settings::searchMethod = FromString<SearchType>(ReadResource<QString>(prefDB, "nedit.searchMethod"));
	Settings::autoWrap     = FromString<WrapStyle>(ReadResource<QString>(prefDB, "nedit.autoWrap"));
	Settings::showMatching = FromString<ShowMatchingStyle>(ReadResource<QString>(prefDB, "nedit.showMatching"));
	Settings::autoIndent   = FromString<IndentStyle>(ReadResource<QString>(prefDB, "nedit.autoIndent"));

	// theme colors
	Settings::colors[ColorTypes::TEXT_BG_COLOR]   = ReadResource<QString>(prefDB, "nedit.textBgColor");
	Settings::colors[ColorTypes::TEXT_FG_COLOR]   = ReadResource<QString>(prefDB, "nedit.textFgColor");
	Settings::colors[ColorTypes::SELECT_BG_COLOR] = ReadResource<QString>(prefDB, "nedit.selectBgColor");
	Settings::colors[ColorTypes::SELECT_FG_COLOR] = ReadResource<QString>(prefDB, "nedit.selectFgColor");
	Settings::colors[ColorTypes::HILITE_BG_COLOR] = ReadResource<QString>(prefDB, "nedit.hiliteBgColor");
	Settings::colors[ColorTypes::HILITE_FG_COLOR] = ReadResource<QString>(prefDB, "nedit.hiliteFgColor");
	Settings::colors[ColorTypes::CURSOR_FG_COLOR] = ReadResource<QString>(prefDB, "nedit.cursorFgColor");
	Settings::colors[ColorTypes::LINENO_FG_COLOR] = ReadResource<QString>(prefDB, "nedit.lineNoFgColor");

	// convert RGB: style colors
	for (QString &color : Settings::colors) {
		color = ConvertRgbColor(color);
	}

	// fonts
#if 0
	Settings::textFont                = ReadResource<QString>(prefDB, "nedit.textFont");
	Settings::boldHighlightFont       = ReadResource<QString>(prefDB, "nedit.boldHighlightFont");
	Settings::italicHighlightFont     = ReadResource<QString>(prefDB, "nedit.italicHighlightFont");
	Settings::boldItalicHighlightFont = ReadResource<QString>(prefDB, "nedit.boldItalicHighlightFont");
#else
	qWarning("WARNING: fonts will not be imported\n"
			 "X11 uses a different specification than Qt and it is difficult to map between the two reliably");

	Settings::fontName = QLatin1String("Courier New,10,-1,5,50,0,0,0,0,0");
#endif

	std::vector<Style> styles;
	QString style = ReadResource<QString>(prefDB, "nedit.styles");
	QTextStream stream(&style);
	static const QRegularExpression re(QLatin1String("\\s*(?<name>[^:]+):(?<foreground>[^:/]+)(/(?<background>[^:]+))?:(?<font>[^:]+)"));

	QString line;
	while (stream.readLineInto(&line)) {
		const QRegularExpressionMatch match = re.match(line);
		if (match.hasMatch()) {
			Style s;
			s.name       = match.captured(QLatin1String("name"));
			s.foreground = ConvertRgbColor(match.captured(QLatin1String("foreground")));
			s.background = ConvertRgbColor(match.captured(QLatin1String("background")));
			s.font       = match.captured(QLatin1String("font"));

			styles.push_back(s);
		}
	}

	XrmDestroyDatabase(prefDB);

	// Write the INI file...
	// this has the side effect of creating the configuration directory as well
	// making it so we don't have to try extra hard to make SaveTheme work :-)
	Settings::Save();

	// Write the theme XML file
	const QString themeFilename = Settings::ThemeFile();
	SaveTheme(themeFilename, styles);
}
