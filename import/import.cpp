
#include "Settings.h"
#include "SearchType.h"
#include "WrapStyle.h"
#include <QVariant>
#include <string>
#include <fstream>
#include <iostream>
#include <iterator>
#include <X11/Xresource.h>

#define APP_CLASS "NEdit"	/* application class for loading resources */

template <class T>
T readResource(XrmDatabase db, const std::string &name);

template <>
QString readResource(XrmDatabase db, const std::string &name) {
	char *type;
	XrmValue resValue;
	XrmGetResource(db, name.c_str(), APP_CLASS, &type, &resValue);	
	return QString::fromLatin1(resValue.addr, resValue.size);
}

template <>
bool readResource(XrmDatabase db, const std::string &name) {
	char *type;
	XrmValue resValue;
	XrmGetResource(db, name.c_str(), APP_CLASS, &type, &resValue);
	
	auto value = QString::fromLatin1(resValue.addr, resValue.size);
	
	return value.compare(QLatin1String("true"), Qt::CaseInsensitive);
}

template <>
int readResource(XrmDatabase db, const std::string &name) {
	char *type;
	XrmValue resValue;
	XrmGetResource(db, name.c_str(), APP_CLASS, &type, &resValue);
	
	auto value = QString::fromLatin1(resValue.addr, resValue.size);
	
	bool ok;
	int n = value.toInt(&ok);
	Q_ASSERT(ok);
	return n;
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
	settings.shellCommands           = readResource<QString>(prefDB, "shellCommands");
	settings.macroCommands           = readResource<QString>(prefDB, "macroCommands");
	settings.bgMenuCommands          = readResource<QString>(prefDB, "bgMenuCommands");
	settings.highlightPatterns       = readResource<QString>(prefDB, "highlightPatterns");
	settings.languageModes           = readResource<QString>(prefDB, "languageModes");
	settings.smartIndentInit         = readResource<QString>(prefDB, "smartIndentInit");
	settings.smartIndentInitCommon   = readResource<QString>(prefDB, "smartIndentInitCommon");
	settings.shell                   = readResource<QString>(prefDB, "shell");
	settings.titleFormat             = readResource<QString>(prefDB, "titleFormat");
	
	// boolean preferences
	settings.autoSave                = readResource<bool>(prefDB, "autoSave");
	settings.saveOldVersion          = readResource<bool>(prefDB, "saveOldVersion");
	settings.searchDialogs           = readResource<bool>(prefDB, "searchDialogs");
	settings.retainSearchDialogs     = readResource<bool>(prefDB, "retainSearchDialogs");
	settings.stickyCaseSenseButton   = readResource<bool>(prefDB, "stickyCaseSenseButton");
	settings.repositionDialogs       = readResource<bool>(prefDB, "repositionDialogs");
	settings.statisticsLine          = readResource<bool>(prefDB, "statisticsLine");
	settings.tabBar                  = readResource<bool>(prefDB, "tabBar");
	settings.searchWraps             = readResource<bool>(prefDB, "searchWraps");
	settings.prefFileRead            = readResource<bool>(prefDB, "prefFileRead");
	settings.sortOpenPrevMenu        = readResource<bool>(prefDB, "sortOpenPrevMenu");
	settings.backlightChars          = readResource<bool>(prefDB, "backlightChars");
	settings.highlightSyntax         = readResource<bool>(prefDB, "highlightSyntax");
	settings.smartTags               = readResource<bool>(prefDB, "smartTags");
	settings.insertTabs              = readResource<bool>(prefDB, "insertTabs");
	settings.warnFileMods            = readResource<bool>(prefDB, "warnFileMods");
	settings.toolTips                = readResource<bool>(prefDB, "toolTips");
	settings.tabBarHideOne           = readResource<bool>(prefDB, "tabBarHideOne");
	settings.globalTabNavigate       = readResource<bool>(prefDB, "globalTabNavigate");
	settings.sortTabs                = readResource<bool>(prefDB, "sortTabs");
	settings.appendLF                = readResource<bool>(prefDB, "appendLF");
	settings.matchSyntaxBased        = readResource<bool>(prefDB, "matchSyntaxBased");
	settings.beepOnSearchWrap        = readResource<bool>(prefDB, "beepOnSearchWrap");
	settings.autoScroll              = readResource<bool>(prefDB, "autoScroll");
	settings.iSearchLine             = readResource<bool>(prefDB, "iSearchLine");
	settings.lineNumbers             = readResource<bool>(prefDB, "lineNumbers");
	settings.pathInWindowsMenu       = readResource<bool>(prefDB, "pathInWindowsMenu");
	settings.warnRealFileMods        = readResource<bool>(prefDB, "warnRealFileMods");
	settings.warnExit                = readResource<bool>(prefDB, "warnExit");
	settings.openInTab               = readResource<bool>(prefDB, "openInTab");
	
	// integer preferences
	settings.emulateTabs             = readResource<int>(prefDB, "emulateTabs");
	settings.tabDistance             = readResource<int>(prefDB, "tabDistance");
	settings.textRows                = readResource<int>(prefDB, "textRows");
	settings.textCols                = readResource<int>(prefDB, "textCols");
	settings.wrapMargin              = readResource<int>(prefDB, "wrapMargin");

	// enumeration values
	settings.searchMethod            = from_integer<SearchType> 	  (readResource<int>(prefDB, "searchMethod"));
	settings.autoWrap                = from_integer<WrapStyle>  	  (readResource<int>(prefDB, "autoWrap"));
	settings.showMatching            = from_integer<ShowMatchingStyle>(readResource<int>(prefDB, "showMatching"));
	settings.autoIndent              = from_integer<IndentStyle>	  (readResource<int>(prefDB, "autoIndent"));


#if 0 // fonts
	settings.textFont                = readResource(prefDB, "textFont");
	settings.boldHighlightFont       = readResource(prefDB, "boldHighlightFont");
	settings.italicHighlightFont     = readResource(prefDB, "italicHighlightFont");
	settings.boldItalicHighlightFont = readResource(prefDB, "boldItalicHighlightFont");
#endif

#if 0 // theme colors
	settings.styles        = readResource(prefDB, "styles");
	settings.textFgColor   = readResource(prefDB, "textFgColor");
	settings.textBgColor   = readResource(prefDB, "textBgColor");
	settings.selectFgColor = readResource(prefDB, "selectFgColor");
	settings.selectBgColor = readResource(prefDB, "selectBgColor");
	settings.hiliteFgColor = readResource(prefDB, "hiliteFgColor");
	settings.hiliteBgColor = readResource(prefDB, "hiliteBgColor");
	settings.lineNoFgColor = readResource(prefDB, "lineNoFgColor");
	settings.cursorFgColor = readResource(prefDB, "cursorFgColor");
#endif
	
	XrmDestroyDatabase(prefDB);
}
