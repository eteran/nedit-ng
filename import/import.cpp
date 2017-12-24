
#include "Settings.h"
#include "SearchType.h"
#include "WrapStyle.h"
#include <QVariant>
#include <QRegularExpression>
#include <QTextStream>
#include <string>
#include <fstream>
#include <iostream>
#include <iterator>

// Placing these before X11 includes because that header does defines some
// invonvinience macros :-(
namespace {

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
	return value.compare(QLatin1String("true"), Qt::CaseInsensitive);
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


int main(int argc, char *argv[]) {

	if(argc != 2) {
		std::cerr << "usage: " << argv[0] << " <filename>" << std::endl;
		return -1;
	}
	
	Display *dpy = XOpenDisplay(nullptr);
	
	/* connect to X */
	if (!dpy) {
		std::cout << "Could not open DISPLAY." << std::endl;
		return -1;
	}

	/* initialize xresources */
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

	// string preferences
    settings.shellCommands           = readResource<QString>(prefDB, "nedit.shellCommands");  // TODO(eteran): handle the slight difference in format
    settings.macroCommands           = readResource<QString>(prefDB, "nedit.macroCommands");  // TODO(eteran): handle the slight difference in format
    settings.bgMenuCommands          = readResource<QString>(prefDB, "nedit.bgMenuCommands"); // TODO(eteran): handle the slight difference in format
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

    std::cout << "WARNING: fonts will not be imported\n"
                 "X11 uses a different specification than Qt and it is difficult to map between the two reliably" << std::endl;

#if 0 // fonts
    settings.textFont                = readResource<QString>(prefDB, "nedit.textFont");
    settings.boldHighlightFont       = readResource<QString>(prefDB, "nedit.boldHighlightFont");
    settings.italicHighlightFont     = readResource<QString>(prefDB, "nedit.italicHighlightFont");
    settings.boldItalicHighlightFont = readResource<QString>(prefDB, "nedit.boldItalicHighlightFont");
#endif


    QString style = readResource<QString>(prefDB, "nedit.styles");
    QTextStream stream(&style);
    QRegularExpression re(QLatin1String("\\s*(?<name>[^:]+):(?<color>[^:]+):(?<font>[^:]+)"));

    QString line;
    while(stream.readLineInto(&line)) {
        qDebug() << line;
        QRegularExpressionMatch match = re.match(line);
        if(match.hasMatch()) {
            qDebug() << match.captured(QLatin1String("name"));
            qDebug() << match.captured(QLatin1String("color"));
            qDebug() << match.captured(QLatin1String("font"));
        }
    }


    // Style format: Name:FGColor/BGColor:FontStyle
	
	XrmDestroyDatabase(prefDB);
}
