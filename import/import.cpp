
#include "interpret.h"
#include "parse.h"
#include "SearchType.h"
#include "Settings.h"
#include "Util/Input.h"
#include "WrapStyle.h"
#include <QRegularExpression>
#include <QTextStream>
#include <QVariant>
#include <QFile>
#include <QDomDocument>
#include <QDomElement>
#include <QColor>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

// Placing these before X11 includes because that header does defines some
// invonvinient macros :-(
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

static QString writeMenuItemStringEx(const std::vector<MenuItem> &menuItems, bool isShellCommand) {

    QString outStr;
    auto outPtr = std::back_inserter(outStr);

    for (const MenuItem &item : menuItems) {

        QString accStr = item.shortcut;
        *outPtr++ = QLatin1Char('\t');

        QString name = item.name;

        name.replace(QLatin1String("&"), QLatin1String("&&"));

        // handle that we do mnemonics the Qt way...
        if(!item.mnemonic.isEmpty()) {
            int n = name.indexOf(item.mnemonic);
            if(n != -1) {
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

        Q_FOREACH(QChar ch, item.command) {
            if (ch == QLatin1Char('\n')) { // and newlines to backslash-n's,
                *outPtr++ = QLatin1Char('\n');
                *outPtr++ = QLatin1Char('\t');
                *outPtr++ = QLatin1Char('\t');
            } else
                *outPtr++ = ch;
        }

        if (!isShellCommand) {

            if(outStr.endsWith(QLatin1Char('\t'))) {
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
 * @brief copyMacroToEnd
 * @param in
 * @return
 */
QString copyMacroToEnd(Input &in) {

    Input input = in;

    // Skip over whitespace to find make sure there's a beginning brace
    // to anchor the parse (if not, it will take the whole file)
    input.skipWhitespaceNL();

    QString code = input.mid();

    if (!code.startsWith(QLatin1Char('{'))) {
        qWarning("Error In Macro/Command String");
        return QString();
    }

    // Parse the input
    int stoppedAt;
    QString errMsg;

    Program *const prog = ParseMacroEx(code, &errMsg, &stoppedAt);
    if(!prog) {
        qWarning("Error In Macro/Command String");
        return QString();
    }
    delete prog;

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


    while(input.index() != stoppedAt + in.index()) {
        if(input.match(QLatin1String("\n\t\t"))) {
            *retPtr++ = QLatin1Char('\n');
        } else {
            *retPtr++ = *input++;
        }
    }

    if(retStr.endsWith(QLatin1Char('\t'))) {
        retStr.chop(1);
    }

    // NOTE(eteran): move past the trailing '}'
    ++input;

    in = input;

    return retStr;
}

/**
 * @brief loadMenuItemString
 * @param inString
 * @param isShellCommand
 * @return
 */
std::vector<MenuItem> loadMenuItemString(const QString &inString, bool isShellCommand) {

    std::vector<MenuItem> items;

    Input in(&inString);

    Q_FOREVER {

        // remove leading whitespace
        in.skipWhitespace();

        // end of string in proper place
        if(in.atEnd()) {
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
        for(; !in.atEnd() && *in != QLatin1Char(':'); ++in) {
            item.flags.push_back(*in);
        }
        ++in;

        // read command field
        if (isShellCommand) {
            in.skipWhitespaceNL();
            item.command = in.readUntil(QLatin1Char('\n'));
        } else {

            QString p = copyMacroToEnd(in);
            if(p.isNull()) {
                return items;
            }

            item.command = p;
        }

        in.skipWhitespaceNL();

        items.push_back(item);
    }
}

void SaveTheme(const QString &filename, const std::vector<Style> &styles) {

    QFile file(filename);
    if(file.open(QIODevice::WriteOnly)) {
        QDomDocument xml;
        QDomProcessingInstruction pi = xml.createProcessingInstruction(QLatin1String("xml"), QLatin1String(R"(version="1.0" encoding="UTF-8")"));

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
        for(const Style &hs : styles) {
            QDomElement style = xml.createElement(QLatin1String("style"));
            style.setAttribute(QLatin1String("name"), hs.name);
            style.setAttribute(QLatin1String("foreground"), hs.foreground);
            if(!hs.background.isEmpty()) {
                style.setAttribute(QLatin1String("background"), hs.background);
            }
            style.setAttribute(QLatin1String("font"), hs.font);

            root.appendChild(style);
        }

        QTextStream stream(&file);
        stream << xml.toString(4);
    }
}

template <class T>
T from_string(const QString &str);

template <>
SearchType from_string(const QString &str) {

    if(str == QLatin1String("Literal")) {
        return SearchType::Literal;
    } else if(str == QLatin1String("CaseSense")) {
        return SearchType::CaseSense;
    } else if(str == QLatin1String("RegExp")) {
        return SearchType::Regex;
    } else if(str == QLatin1String("LiteralWord")) {
        return SearchType::LiteralWord;
    } else if(str == QLatin1String("CaseSenseWord")) {
        return SearchType::CaseSenseWord;
    } else if(str == QLatin1String("RegExpNoCase")) {
        return SearchType::RegexNoCase;
    }

    qDebug() << "Invalid configuration value: " << str;
    abort();
}

template <>
WrapStyle from_string(const QString &str) {

    if(str == QLatin1String("None")) {
        return WrapStyle::None;
    } else if(str == QLatin1String("Newline")) {
        return WrapStyle::Newline;
    } else if(str == QLatin1String("Continuous")) {
        return WrapStyle::Continuous;
    }

    qDebug() << "Invalid configuration value: " << str;
    abort();
}

template <>
IndentStyle from_string(const QString &str) {

    if(str == QLatin1String("None")) {
        return IndentStyle::None;
    } else if(str == QLatin1String("Auto")) {
        return IndentStyle::Auto;
    } else if(str == QLatin1String("Smart")) {
        return IndentStyle::Smart;
    }

    qDebug() << "Invalid configuration value: " << str;
    abort();
}



template <>
ShowMatchingStyle from_string(const QString &str) {

    /* For backward compatibility, "False" and "True" are still accepted.
       They are internally converted to "Off" and "Delimiter" respectively.*/

    if(str == QLatin1String("Off")) {
        return ShowMatchingStyle::None;
    } else if(str == QLatin1String("Delimiter")) {
        return ShowMatchingStyle::Delimiter;
    } else if(str == QLatin1String("Range")) {
        return ShowMatchingStyle::Range;
    } else if(str == QLatin1String("False")) {
        return ShowMatchingStyle::None;
    } else if(str == QLatin1String("True")) {
        return ShowMatchingStyle::Delimiter;
    }

    qDebug() << "Invalid configuration value: " << str;
    abort();
}

}

#include <X11/Xresource.h>


#define APP_CLASS "NEdit"	/* application class for loading resources */

namespace {

template <class T>
T readResource(XrmDatabase db, const std::string &name);

template <>
QString readResource(XrmDatabase db, const std::string &name) {
	char *type;
	XrmValue resValue;
	XrmGetResource(db, name.c_str(), APP_CLASS, &type, &resValue);	
    auto ret = QString::fromLatin1(resValue.addr, static_cast<int>(resValue.size - 1));
    Q_ASSERT(!ret.isEmpty());
    return ret;
}

template <>
bool readResource(XrmDatabase db, const std::string &name) {
    auto value = readResource<QString>(db, name);
    return value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
}

template <>
int readResource(XrmDatabase db, const std::string &name) {
    auto value = readResource<QString>(db, name);
	bool ok;
	int n = value.toInt(&ok);
	Q_ASSERT(ok);
	return n;
}

QString covertRGBColor(const QString &color) {

    {
        // convert RGB style colors
        static const QRegularExpression rgb_regex(QLatin1String("rgb:"
                                                                  "(?<red>[0-9a-fA-F]{1,4})"
                                                                  "/"
                                                                  "(?<green>[0-9a-fA-F]{1,4})"
                                                                  "/"
                                                                  "(?<blue>[0-9a-fA-F]{1,4})"
                                                                  ));

        QRegularExpressionMatch match = rgb_regex.match(color);
        if(match.hasMatch()) {
            uint16_t r = match.captured(QLatin1String("red")).toUShort(nullptr, 16);
            uint16_t g = match.captured(QLatin1String("green")).toUShort(nullptr, 16);
            uint16_t b = match.captured(QLatin1String("blue")).toUShort(nullptr, 16);

            QColor c(r, g, b);
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
                                                                  "(?<blue>[0-9]+(\\.[0-9]+)?)"
                                                                  ));


        QRegularExpressionMatch match_rgbi = rgbi_regex.match(color);
        if(match_rgbi.hasMatch()) {
            qreal r = match_rgbi.captured(QLatin1String("red")).toDouble();
            qreal g = match_rgbi.captured(QLatin1String("green")).toDouble();
            qreal b = match_rgbi.captured(QLatin1String("blue")).toDouble();

            QColor c(static_cast<int>(r * 255), static_cast<int>(g * 255), static_cast<int>(b * 255));
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
 * @brief main
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char *argv[]) {

	if(argc != 2) {
		std::cerr << "usage: " << argv[0] << " <filename>" << std::endl;
		return -1;
	}
	
	Display *dpy = XOpenDisplay(nullptr);
	
	if (!dpy) {
		std::cout << "Could not open DISPLAY." << std::endl;
		return -1;
	}

    XrmInitialize();
	
	std::ifstream file(argv[1]);
	if(!file) {
		std::cout << "Could not open resource file." << std::endl;
		return -1;
	}
	
	auto contents = std::string{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};

	XrmDatabase prefDB = XrmGetStringDatabase(&contents[0]);
	
	QString value = readResource<QString>(prefDB, "nedit.fileVersion");
	
	if(value != QLatin1String("5.6") && value != QLatin1String("5.7")) {
		std::cout << "Importing is only supported for NEdit 5.6 and 5.7" << std::endl;
		return -1;
	}
	
    // Advanced stuff from secondary XResources...
    // make sure that there are some sensible defaults...
    Settings::fileVersion = 1;
    Settings::colorizeHighlightedText           = true;
    Settings::alwaysCheckRelativeTagsSpecs      = true;
    Settings::findReplaceUsesSelection          = false;
    Settings::focusOnRaise                      = false;
    Settings::forceOSConversion                 = true;
    Settings::honorSymlinks                     = true;
    Settings::stickyCaseSenseButton             = true;
    Settings::typingHidesPointer                = false;
    Settings::undoModifiesSelection             = true;
    Settings::autoScrollVPadding                = 4;
    Settings::maxPrevOpenFiles                  = 30;
    Settings::truncSubstitution                 = TruncSubstitution::Fail;
    Settings::backlightCharTypes                = QLatin1String("0-8,10-31,127:red;9:#dedede;32,160-255:#f0f0f0;128-159:orange");
    Settings::tagFile                           = QString();
    Settings::wordDelimiters                    = QLatin1String(".,/\\`'!|@#%^&*()-=+{}[]\":;<>?");

	// string preferences
    Settings::shellCommands           = readResource<QString>(prefDB, "nedit.shellCommands");
    Settings::macroCommands           = readResource<QString>(prefDB, "nedit.macroCommands");
    Settings::bgMenuCommands          = readResource<QString>(prefDB, "nedit.bgMenuCommands");

    std::vector<MenuItem> shellCommands  = loadMenuItemString(Settings::shellCommands, true);
    std::vector<MenuItem> macroCommands  = loadMenuItemString(Settings::macroCommands, false);
    std::vector<MenuItem> bgMenuCommands = loadMenuItemString(Settings::bgMenuCommands, false);

    Settings::shellCommands           = writeMenuItemStringEx(shellCommands, true);
    Settings::macroCommands           = writeMenuItemStringEx(macroCommands, false);
    Settings::bgMenuCommands          = writeMenuItemStringEx(bgMenuCommands, false);

    Settings::highlightPatterns       = readResource<QString>(prefDB, "nedit.highlightPatterns");
    Settings::languageModes           = readResource<QString>(prefDB, "nedit.languageModes");
    Settings::smartIndentInit         = readResource<QString>(prefDB, "nedit.smartIndentInit");
    Settings::smartIndentInitCommon   = readResource<QString>(prefDB, "nedit.smartIndentInitCommon");
    Settings::shell                   = readResource<QString>(prefDB, "nedit.shell");
    Settings::titleFormat             = readResource<QString>(prefDB, "nedit.titleFormat");
	
	// boolean preferences
    Settings::autoSave                = readResource<bool>(prefDB, "nedit.autoSave");
    Settings::saveOldVersion          = readResource<bool>(prefDB, "nedit.saveOldVersion");
    Settings::searchDialogs           = readResource<bool>(prefDB, "nedit.searchDialogs");
    Settings::retainSearchDialogs     = readResource<bool>(prefDB, "nedit.retainSearchDialogs");
    Settings::stickyCaseSenseButton   = readResource<bool>(prefDB, "nedit.stickyCaseSenseButton");
    Settings::repositionDialogs       = readResource<bool>(prefDB, "nedit.repositionDialogs");
    Settings::statisticsLine          = readResource<bool>(prefDB, "nedit.statisticsLine");
    Settings::tabBar                  = readResource<bool>(prefDB, "nedit.tabBar");
    Settings::searchWraps             = readResource<bool>(prefDB, "nedit.searchWraps");
    Settings::prefFileRead            = readResource<bool>(prefDB, "nedit.prefFileRead");
    Settings::sortOpenPrevMenu        = readResource<bool>(prefDB, "nedit.sortOpenPrevMenu");
    Settings::backlightChars          = readResource<bool>(prefDB, "nedit.backlightChars");
    Settings::highlightSyntax         = readResource<bool>(prefDB, "nedit.highlightSyntax");
    Settings::smartTags               = readResource<bool>(prefDB, "nedit.smartTags");
    Settings::insertTabs              = readResource<bool>(prefDB, "nedit.insertTabs");
    Settings::warnFileMods            = readResource<bool>(prefDB, "nedit.warnFileMods");
    Settings::toolTips                = readResource<bool>(prefDB, "nedit.toolTips");
    Settings::tabBarHideOne           = readResource<bool>(prefDB, "nedit.tabBarHideOne");
    Settings::globalTabNavigate       = readResource<bool>(prefDB, "nedit.globalTabNavigate");
    Settings::sortTabs                = readResource<bool>(prefDB, "nedit.sortTabs");
    Settings::appendLF                = readResource<bool>(prefDB, "nedit.appendLF");
    Settings::matchSyntaxBased        = readResource<bool>(prefDB, "nedit.matchSyntaxBased");
    Settings::beepOnSearchWrap        = readResource<bool>(prefDB, "nedit.beepOnSearchWrap");
    Settings::autoScroll              = readResource<bool>(prefDB, "nedit.autoScroll");
    Settings::iSearchLine             = readResource<bool>(prefDB, "nedit.iSearchLine");
    Settings::lineNumbers             = readResource<bool>(prefDB, "nedit.lineNumbers");
    Settings::pathInWindowsMenu       = readResource<bool>(prefDB, "nedit.pathInWindowsMenu");
    Settings::warnRealFileMods        = readResource<bool>(prefDB, "nedit.warnRealFileMods");
    Settings::warnExit                = readResource<bool>(prefDB, "nedit.warnExit");
    Settings::openInTab               = readResource<bool>(prefDB, "nedit.openInTab");
	
	// integer preferences
    Settings::emulateTabs             = readResource<int>(prefDB, "nedit.emulateTabs");
    Settings::tabDistance             = readResource<int>(prefDB, "nedit.tabDistance");
    Settings::textRows                = readResource<int>(prefDB, "nedit.textRows");
    Settings::textCols                = readResource<int>(prefDB, "nedit.textCols");
    Settings::wrapMargin              = readResource<int>(prefDB, "nedit.wrapMargin");

	// enumeration values
    Settings::searchMethod            = from_string<SearchType> 	     (readResource<QString>(prefDB, "nedit.searchMethod"));
    Settings::autoWrap                = from_string<WrapStyle>  	     (readResource<QString>(prefDB, "nedit.autoWrap"));
    Settings::showMatching            = from_string<ShowMatchingStyle>(readResource<QString>(prefDB, "nedit.showMatching"));
    Settings::autoIndent              = from_string<IndentStyle>	     (readResource<QString>(prefDB, "nedit.autoIndent"));

    // theme colors    
    Settings::colors[ColorTypes::TEXT_BG_COLOR]   = readResource<QString>(prefDB, "nedit.textBgColor");
    Settings::colors[ColorTypes::TEXT_FG_COLOR]   = readResource<QString>(prefDB, "nedit.textFgColor");
    Settings::colors[ColorTypes::SELECT_BG_COLOR] = readResource<QString>(prefDB, "nedit.selectBgColor");
    Settings::colors[ColorTypes::SELECT_FG_COLOR] = readResource<QString>(prefDB, "nedit.selectFgColor");
    Settings::colors[ColorTypes::HILITE_BG_COLOR] = readResource<QString>(prefDB, "nedit.hiliteBgColor");
    Settings::colors[ColorTypes::HILITE_FG_COLOR] = readResource<QString>(prefDB, "nedit.hiliteFgColor");
    Settings::colors[ColorTypes::CURSOR_FG_COLOR] = readResource<QString>(prefDB, "nedit.cursorFgColor");
    Settings::colors[ColorTypes::LINENO_FG_COLOR] = readResource<QString>(prefDB, "nedit.lineNoFgColor");

    // convert RGB: style colors
    for(QString &color : Settings::colors) {
        color = covertRGBColor(color);
    }


    // fonts
#if 0
    Settings::textFont                = readResource<QString>(prefDB, "nedit.textFont");
    Settings::boldHighlightFont       = readResource<QString>(prefDB, "nedit.boldHighlightFont");
    Settings::italicHighlightFont     = readResource<QString>(prefDB, "nedit.italicHighlightFont");
    Settings::boldItalicHighlightFont = readResource<QString>(prefDB, "nedit.boldItalicHighlightFont");
#else
    qWarning("WARNING: fonts will not be imported\n"
             "X11 uses a different specification than Qt and it is difficult to map between the two reliably");

    Settings::textFont                = QLatin1String("Courier New,10,-1,5,50,0,0,0,0,0");
    Settings::boldHighlightFont       = QLatin1String("Courier New,10,-1,5,75,0,0,0,0,0");
    Settings::italicHighlightFont     = QLatin1String("Courier New,10,-1,5,50,1,0,0,0,0");
    Settings::boldItalicHighlightFont = QLatin1String("Courier New,10,-1,5,75,1,0,0,0,0");
#endif

    std::vector<Style> styles;
    QString style = readResource<QString>(prefDB, "nedit.styles");
    QTextStream stream(&style);
    static const QRegularExpression re(QLatin1String("\\s*(?<name>[^:]+):(?<foreground>[^:/]+)(/(?<background>[^:]+))?:(?<font>[^:]+)"));

    QString line;
    while(stream.readLineInto(&line)) {
        QRegularExpressionMatch match = re.match(line);
        if(match.hasMatch()) {
            Style s;
            s.name       = match.captured(QLatin1String("name"));
            s.foreground = covertRGBColor(match.captured(QLatin1String("foreground")));
            s.background = covertRGBColor(match.captured(QLatin1String("background")));
            s.font       = match.captured(QLatin1String("font"));

            styles.push_back(s);
        }
    }
	
	XrmDestroyDatabase(prefDB);

    // Write the INI file...
    // this has the side effect of creating the configuration directory as well
    // making it so we don't have to try extra hard to make SaveTheme work :-)
    Settings::savePreferences();

    // Write the theme XML file
    QString themeFilename = Settings::themeFile();
    SaveTheme(themeFilename, styles);
}
