
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

        // read flags fiel
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

void SaveTheme(const QString &filename, const Settings &settings, const std::vector<Style> &styles) {

    QFile file(filename);
    if(file.open(QIODevice::WriteOnly)) {
        QDomDocument xml;
        QDomProcessingInstruction pi = xml.createProcessingInstruction(QLatin1String("xml"), QLatin1String("version=\"1.0\" encoding=\"UTF-8\""));

        xml.appendChild(pi);

        QDomElement root = xml.createElement(QLatin1String("theme"));
        root.setAttribute(QLatin1String("name"), QLatin1String("default"));
        xml.appendChild(root);

        // save basic color settings...
        {
            QDomElement text = xml.createElement(QLatin1String("text"));
            text.setAttribute(QLatin1String("foreground"), settings.colors[ColorTypes::TEXT_FG_COLOR]);
            text.setAttribute(QLatin1String("background"), settings.colors[ColorTypes::TEXT_BG_COLOR]);
            root.appendChild(text);
        }

        {
            QDomElement selection = xml.createElement(QLatin1String("selection"));
            selection.setAttribute(QLatin1String("foreground"), settings.colors[ColorTypes::SELECT_FG_COLOR]);
            selection.setAttribute(QLatin1String("background"), settings.colors[ColorTypes::SELECT_BG_COLOR]);
            root.appendChild(selection);
        }

        {
            QDomElement highlight = xml.createElement(QLatin1String("highlight"));
            highlight.setAttribute(QLatin1String("foreground"), settings.colors[ColorTypes::HILITE_FG_COLOR]);
            highlight.setAttribute(QLatin1String("background"), settings.colors[ColorTypes::HILITE_BG_COLOR]);
            root.appendChild(highlight);
        }

        {
            QDomElement cursor = xml.createElement(QLatin1String("cursor"));
            cursor.setAttribute(QLatin1String("foreground"), settings.colors[ColorTypes::CURSOR_FG_COLOR]);
            root.appendChild(cursor);
        }

        {
            QDomElement lineno = xml.createElement(QLatin1String("line-numbers"));
            lineno.setAttribute(QLatin1String("foreground"), settings.colors[ColorTypes::LINENO_FG_COLOR]);
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
        stream << xml.toString();
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
	
	Settings settings;

    // Advanced stuff from secondary XResources...
    // make sure that there are some sensible defaults...
    settings.fileVersion = 1;
    settings.colorizeHighlightedText           = true;
    settings.alwaysCheckRelativeTagsSpecs      = true;
    settings.findReplaceUsesSelection          = false;
    settings.focusOnRaise                      = false;
    settings.forceOSConversion                 = true;
    settings.honorSymlinks                     = true;
    settings.stickyCaseSenseButton             = true;
    settings.typingHidesPointer                = false;
    settings.undoModifiesSelection             = true;
    settings.autoScrollVPadding                = 4;
    settings.maxPrevOpenFiles                  = 30;
    settings.truncSubstitution                 = TruncSubstitution::Fail;
    settings.backlightCharTypes                = QLatin1String("0-8,10-31,127:red;9:#dedede;32,160-255:#f0f0f0;128-159:orange");
    settings.tagFile                           = QString();
    settings.wordDelimiters                    = QLatin1String(".,/\\`'!|@#%^&*()-=+{}[]\":;<>?");

	// string preferences
    settings.shellCommands           = readResource<QString>(prefDB, "nedit.shellCommands");
    settings.macroCommands           = readResource<QString>(prefDB, "nedit.macroCommands");
    settings.bgMenuCommands          = readResource<QString>(prefDB, "nedit.bgMenuCommands");

    std::vector<MenuItem> shellCommands  = loadMenuItemString(settings.shellCommands, true);
    std::vector<MenuItem> macroCommands  = loadMenuItemString(settings.macroCommands, false);
    std::vector<MenuItem> bgMenuCommands = loadMenuItemString(settings.bgMenuCommands, false);

    settings.shellCommands           = writeMenuItemStringEx(shellCommands, true);
    settings.macroCommands           = writeMenuItemStringEx(macroCommands, false);
    settings.bgMenuCommands          = writeMenuItemStringEx(bgMenuCommands, false);

    settings.highlightPatterns       = readResource<QString>(prefDB, "nedit.highlightPatterns");
    settings.languageModes           = readResource<QString>(prefDB, "nedit.languageModes");
    settings.smartIndentInit         = readResource<QString>(prefDB, "nedit.smartIndentInit");
    settings.smartIndentInitCommon   = readResource<QString>(prefDB, "nedit.smartIndentInitCommon");
    settings.shell                   = readResource<QString>(prefDB, "nedit.shell");
    settings.titleFormat             = readResource<QString>(prefDB, "nedit.titleFormat");
	
	// boolean preferences
    settings.autoSave                = readResource<bool>(prefDB, "nedit.autoSave");
    settings.saveOldVersion          = readResource<bool>(prefDB, "nedit.saveOldVersion");
    settings.searchDialogs           = readResource<bool>(prefDB, "nedit.searchDialogs");
    settings.retainSearchDialogs     = readResource<bool>(prefDB, "nedit.retainSearchDialogs");
    settings.stickyCaseSenseButton   = readResource<bool>(prefDB, "nedit.stickyCaseSenseButton");
    settings.repositionDialogs       = readResource<bool>(prefDB, "nedit.repositionDialogs");
    settings.statisticsLine          = readResource<bool>(prefDB, "nedit.statisticsLine");
    settings.tabBar                  = readResource<bool>(prefDB, "nedit.tabBar");
    settings.searchWraps             = readResource<bool>(prefDB, "nedit.searchWraps");
    settings.prefFileRead            = readResource<bool>(prefDB, "nedit.prefFileRead");
    settings.sortOpenPrevMenu        = readResource<bool>(prefDB, "nedit.sortOpenPrevMenu");
    settings.backlightChars          = readResource<bool>(prefDB, "nedit.backlightChars");
    settings.highlightSyntax         = readResource<bool>(prefDB, "nedit.highlightSyntax");
	settings.smartTags               = readResource<bool>(prefDB, "nedit.smartTags");
	settings.insertTabs              = readResource<bool>(prefDB, "nedit.insertTabs");
	settings.warnFileMods            = readResource<bool>(prefDB, "nedit.warnFileMods");
	settings.toolTips                = readResource<bool>(prefDB, "nedit.toolTips");
	settings.tabBarHideOne           = readResource<bool>(prefDB, "nedit.tabBarHideOne");
	settings.globalTabNavigate       = readResource<bool>(prefDB, "nedit.globalTabNavigate");
	settings.sortTabs                = readResource<bool>(prefDB, "nedit.sortTabs");
	settings.appendLF                = readResource<bool>(prefDB, "nedit.appendLF");
	settings.matchSyntaxBased        = readResource<bool>(prefDB, "nedit.matchSyntaxBased");
	settings.beepOnSearchWrap        = readResource<bool>(prefDB, "nedit.beepOnSearchWrap");
	settings.autoScroll              = readResource<bool>(prefDB, "nedit.autoScroll");
	settings.iSearchLine             = readResource<bool>(prefDB, "nedit.iSearchLine");
	settings.lineNumbers             = readResource<bool>(prefDB, "nedit.lineNumbers");
	settings.pathInWindowsMenu       = readResource<bool>(prefDB, "nedit.pathInWindowsMenu");
	settings.warnRealFileMods        = readResource<bool>(prefDB, "nedit.warnRealFileMods");
	settings.warnExit                = readResource<bool>(prefDB, "nedit.warnExit");
	settings.openInTab               = readResource<bool>(prefDB, "nedit.openInTab");
	
	// integer preferences
	settings.emulateTabs             = readResource<int>(prefDB, "nedit.emulateTabs");
	settings.tabDistance             = readResource<int>(prefDB, "nedit.tabDistance");
	settings.textRows                = readResource<int>(prefDB, "nedit.textRows");
	settings.textCols                = readResource<int>(prefDB, "nedit.textCols");
	settings.wrapMargin              = readResource<int>(prefDB, "nedit.wrapMargin");

	// enumeration values
    settings.searchMethod            = from_string<SearchType> 	     (readResource<QString>(prefDB, "nedit.searchMethod"));
    settings.autoWrap                = from_string<WrapStyle>  	     (readResource<QString>(prefDB, "nedit.autoWrap"));
    settings.showMatching            = from_string<ShowMatchingStyle>(readResource<QString>(prefDB, "nedit.showMatching"));
    settings.autoIndent              = from_string<IndentStyle>	     (readResource<QString>(prefDB, "nedit.autoIndent"));

    // theme colors
    settings.colors[ColorTypes::TEXT_BG_COLOR]   = readResource<QString>(prefDB, "nedit.textBgColor");
    settings.colors[ColorTypes::TEXT_FG_COLOR]   = readResource<QString>(prefDB, "nedit.textFgColor");
    settings.colors[ColorTypes::SELECT_BG_COLOR] = readResource<QString>(prefDB, "nedit.selectBgColor");
    settings.colors[ColorTypes::SELECT_FG_COLOR] = readResource<QString>(prefDB, "nedit.selectFgColor");
    settings.colors[ColorTypes::HILITE_BG_COLOR] = readResource<QString>(prefDB, "nedit.hiliteBgColor");
    settings.colors[ColorTypes::HILITE_FG_COLOR] = readResource<QString>(prefDB, "nedit.hiliteFgColor");
    settings.colors[ColorTypes::CURSOR_FG_COLOR] = readResource<QString>(prefDB, "nedit.cursorFgColor");
    settings.colors[ColorTypes::LINENO_FG_COLOR] = readResource<QString>(prefDB, "nedit.lineNoFgColor");

    // fonts
#if 0
    settings.textFont                = readResource<QString>(prefDB, "nedit.textFont");
    settings.boldHighlightFont       = readResource<QString>(prefDB, "nedit.boldHighlightFont");
    settings.italicHighlightFont     = readResource<QString>(prefDB, "nedit.italicHighlightFont");
    settings.boldItalicHighlightFont = readResource<QString>(prefDB, "nedit.boldItalicHighlightFont");
#else
    qWarning("WARNING: fonts will not be imported\n"
             "X11 uses a different specification than Qt and it is difficult to map between the two reliably");

    settings.textFont                = QLatin1String("Courier New,10,-1,5,50,0,0,0,0,0");
    settings.boldHighlightFont       = QLatin1String("Courier New,10,-1,5,75,0,0,0,0,0");
    settings.italicHighlightFont     = QLatin1String("Courier New,10,-1,5,50,1,0,0,0,0");
    settings.boldItalicHighlightFont = QLatin1String("Courier New,10,-1,5,75,1,0,0,0,0");
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
            s.foreground = match.captured(QLatin1String("foreground"));
            s.background = match.captured(QLatin1String("background"));
            s.font       = match.captured(QLatin1String("font"));

            styles.push_back(s);
        }
    }
	
	XrmDestroyDatabase(prefDB);

    // Write the theme XML file
    QString themeFilename = Settings::themeFile();
    SaveTheme(themeFilename, settings, styles);

    // Write the INI file...
    settings.savePreferences();
}
