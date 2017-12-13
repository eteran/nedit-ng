
#include "userCmds.h"
#include "MenuData.h"
#include "MenuItem.h"
#include "interpret.h"
#include "macro.h"
#include "parse.h"
#include "Input.h"
#include "preferences.h"
#include "LanguageMode.h"
#include "raise.h"
#include <QTextStream>
#include <memory>

/* Descriptions of the current user programmed menu items for re-generating
   menus and processing shell, macro, and background menu selections */
std::vector<MenuData> ShellMenuData;
std::vector<MenuData> MacroMenuData;
std::vector<MenuData> BGMenuData;

namespace {

struct ParseError : std::exception {
public:
	explicit ParseError(const std::string &s) : s_(s) {
    }

    const char *what() const noexcept {
        return s_.c_str();
    }
private:
    std::string s_;
};

}

static QString copyMacroToEndEx(Input &in);
static int loadMenuItemStringEx(const QString &inString, std::vector<MenuData> &menuItems, DialogTypes listType);
static QString stripLanguageModeEx(const QString &menuItemName);
static QString writeMenuItemStringEx(const std::vector<MenuData> &menuItems, DialogTypes listType);
static std::shared_ptr<userMenuInfo> parseMenuItemRec(const MenuItem &item);
static void parseMenuItemName(const QString &menuItemName, const std::shared_ptr<userMenuInfo> &info);
static void setDefaultIndex(const std::vector<MenuData> &infoList, size_t index);


std::vector<MenuData> &selectMenu(DialogTypes type) {
    switch(type) {
    case DialogTypes::SHELL_CMDS:
        return ShellMenuData;
    case DialogTypes::MACRO_CMDS:
        return MacroMenuData;
    case DialogTypes::BG_MENU_CMDS:
        return BGMenuData;
    }

    Q_UNREACHABLE();
}

MenuData *findMenuItem(const QString &name, DialogTypes type) {

    for(MenuData &data: selectMenu(type)) {
        if (data.item.name == name) {
            return &data;
        }
    }

    return nullptr;
}


/*
** Generate a text string for the preferences file describing the contents
** of the shell cmd list, macro menu and background menus.
*/
QString WriteShellCmdsStringEx() {
    return writeMenuItemStringEx(ShellMenuData, DialogTypes::SHELL_CMDS);
}

QString WriteMacroCmdsStringEx() {
    return writeMenuItemStringEx(MacroMenuData, DialogTypes::MACRO_CMDS);
}

QString WriteBGMenuCmdsStringEx() {
    return writeMenuItemStringEx(BGMenuData, DialogTypes::BG_MENU_CMDS);
}

/*
** Read a string representing shell command menu items, macro menu or
** background menu and add them to the internal list used for constructing
** menus
*/
int LoadShellCmdsStringEx(const QString &inString) {
    return loadMenuItemStringEx(inString, ShellMenuData, DialogTypes::SHELL_CMDS);
}

int LoadMacroCmdsStringEx(const QString &inString) {
    return loadMenuItemStringEx(inString, MacroMenuData, DialogTypes::MACRO_CMDS);
}

int LoadBGMenuCmdsStringEx(const QString &inString) {
    return loadMenuItemStringEx(inString, BGMenuData, DialogTypes::BG_MENU_CMDS);
}

/*
** Cache user menus:
** Setup user menu info after read of macro, shell and background menu
** string (reason: language mode info from preference string is read *after*
** user menu preference string was read).
*/
void SetupUserMenuInfo() {
    parseMenuItemList(ShellMenuData);
    parseMenuItemList(MacroMenuData);
    parseMenuItemList(BGMenuData);
}

/*
** Cache user menus:
** Update user menu info to take into account e.g. change of language modes
** (i.e. add / move / delete of language modes etc).
*/
void UpdateUserMenuInfo() {
    for(auto &item : ShellMenuData) {
        item.info = nullptr;
    }
    parseMenuItemList(ShellMenuData);

    for(auto &item : MacroMenuData) {
        item.info = nullptr;
    }
    parseMenuItemList(MacroMenuData);

    for(auto &item : BGMenuData) {
        item.info = nullptr;
    }
    parseMenuItemList(BGMenuData);
}

static QString writeMenuItemStringEx(const std::vector<MenuData> &menuItems, DialogTypes listType) {

    QString outStr;
    auto outPtr = std::back_inserter(outStr);

    for (const MenuData &data : menuItems) {
        auto &f = data.item;

        QString accStr = f.shortcut.toString();
        *outPtr++ = QLatin1Char('\t');

        Q_FOREACH(QChar ch, f.name) { // Copy the command name
            *outPtr++ = ch;
        }

        *outPtr++ = QLatin1Char(':');
        std::copy(accStr.begin(), accStr.end(), outPtr);

        *outPtr++ = QLatin1Char(':');
        if (listType == DialogTypes::SHELL_CMDS) {
            switch(f.input) {
            case FROM_SELECTION: *outPtr++ = QLatin1Char('I'); break;
            case FROM_WINDOW:    *outPtr++ = QLatin1Char('A'); break;
            case FROM_EITHER:    *outPtr++ = QLatin1Char('E'); break;
            default:
                break;
            }

            switch(f.output) {
            case TO_DIALOG:     *outPtr++ = QLatin1Char('D'); break;
            case TO_NEW_WINDOW: *outPtr++ = QLatin1Char('W'); break;
            default:
                break;
            }

            if (f.repInput)  *outPtr++ = QLatin1Char('X');
            if (f.saveFirst) *outPtr++ = QLatin1Char('S');
            if (f.loadAfter) *outPtr++ = QLatin1Char('L');

            *outPtr++ = QLatin1Char(':');
        } else {
            if (f.input == FROM_SELECTION) {
                *outPtr++ = QLatin1Char('R');
            }

            *outPtr++ = QLatin1Char(':');
            *outPtr++ = QLatin1Char(' ');
            *outPtr++ = QLatin1Char('{');
        }

        *outPtr++ = QLatin1Char('\n');
        *outPtr++ = QLatin1Char('\t');
        *outPtr++ = QLatin1Char('\t');

        Q_FOREACH(QChar ch, f.cmd) {
            if (ch == QLatin1Char('\n')) { // and newlines to backslash-n's,
                *outPtr++ = QLatin1Char('\n');
                *outPtr++ = QLatin1Char('\t');
                *outPtr++ = QLatin1Char('\t');
            } else
                *outPtr++ = ch;
        }

        if (listType == DialogTypes::MACRO_CMDS || listType == DialogTypes::BG_MENU_CMDS) {

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

static int loadMenuItemStringEx(const QString &inString, std::vector<MenuData> &menuItems, DialogTypes listType) {

    try {
        Input in(&inString);

        Q_FOREVER {

            // remove leading whitespace
            in.skipWhitespace();

            // end of string in proper place
            if(in.atEnd()) {
                return true;
            }

            // read name field
            QString nameStr = in.readUntil(QLatin1Char(':'));
            if(nameStr.isEmpty()) {
                raise<ParseError>("no name field");
            }

            if(in.atEnd()) {
                raise<ParseError>("end not expected");
            }

            ++in;

            // read accelerator field
            QString accStr = in.readUntil(QLatin1Char(':'));

            if(in.atEnd()) {
                raise<ParseError>("end not expected");
            }

            ++in;

            // read flags field
            InSrcs input    = FROM_NONE;
            OutDests output = TO_SAME_WINDOW;
            bool repInput   = false;
            bool saveFirst  = false;
            bool loadAfter  = false;

            for(; !in.atEnd() && *in != QLatin1Char(':'); ++in) {

                if (listType == DialogTypes::SHELL_CMDS) {

                    switch((*in).toLatin1()) {
                    case 'I':
                        input = FROM_SELECTION;
                        break;
                    case 'A':
                        input = FROM_WINDOW;
                        break;
                    case 'E':
                        input = FROM_EITHER;
                        break;
                    case 'W':
                        output = TO_NEW_WINDOW;
                        break;
                    case 'D':
                        output = TO_DIALOG;
                        break;
                    case 'X':
                        repInput = true;
                        break;
                    case 'S':
                        saveFirst = true;
                        break;
                    case 'L':
                        loadAfter = true;
                        break;
                    default:
                        raise<ParseError>("unreadable flag field");
                    }
                } else {
                    switch((*in).toLatin1()) {
                    case 'R':
                        input = FROM_SELECTION;
                        break;
                    default:
                        raise<ParseError>("unreadable flag field");
                    }
                }
            }
            ++in;

            QString cmdStr;

            // read command field
            if (listType == DialogTypes::SHELL_CMDS) {

                if (*in++ != QLatin1Char('\n')) {
                    raise<ParseError>("command must begin with newline");
                }

                // leading whitespace
                in.skipWhitespace();

                cmdStr = in.readUntil(QLatin1Char('\n'));

                if (cmdStr.isEmpty()) {
                    raise<ParseError>("shell command field is empty");
                }

            } else {

                QString p = copyMacroToEndEx(in);
                if(p.isNull()) {
                    return false;
                }

                cmdStr = p;
            }

            in.skipWhitespaceNL();

            // parse the accelerator field
            auto shortcut = QKeySequence::fromString(accStr);

            // create a menu item record
            auto f = std::make_unique<MenuItem>();
            f->name      = nameStr;
            f->cmd       = cmdStr;
            f->input     = input;
            f->output    = output;
            f->repInput  = repInput;
            f->saveFirst = saveFirst;
            f->loadAfter = loadAfter;
            f->shortcut  = shortcut;

            // add/replace menu record in the list
            auto it = std::find_if(menuItems.begin(), menuItems.end(), [&f](MenuData &data) {
                return data.item.name == f->name;
            });

            if(it == menuItems.end()) {
                menuItems.push_back({ *f, nullptr });
            } else {
                it->item = *f;
            }
        }
    } catch(const ParseError &ex) {
        qWarning("NEdit: Parse error in user defined menu item, %s", ex.what());
        return false;
    }
}

/*
** Scan text from "*inPtr" to the end of macro input (matching brace),
** advancing inPtr, and return macro text as function return value.
**
** This is kind of wastefull in that it throws away the compiled macro,
** to be re-generated from the text as needed, but compile time is
** negligible for most macros.
*/
static QString copyMacroToEndEx(Input &in) {

    Input input = in;

    // Skip over whitespace to find make sure there's a beginning brace
    // to anchor the parse (if not, it will take the whole file)
    input.skipWhitespaceNL();

    QString code = input.mid();

    if (code[0] != QLatin1Char('{')) {
        ParseErrorEx(nullptr, code, input.index() - in.index(), QLatin1String("macro menu item"), QLatin1String("expecting '{'"));
        return QString();
    }

    // Parse the input
    int stoppedAt;
    QString errMsg;

    Program *const prog = ParseMacroEx(code, &errMsg, &stoppedAt);
    if(!prog) {
        ParseErrorEx(nullptr, code, stoppedAt, QLatin1String("macro menu item"), errMsg);
        return QString();
    }
    FreeProgram(prog);

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
            input += 3;
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

/*
** Cache user menus:
** Parse given menu item list and setup a user menu info list for
** management of user menu.
*/

void parseMenuItemList(std::vector<MenuData> &itemList) {

	/* 1st pass: setup user menu info: extract language modes, menu name &
	   default indication; build user menu ID */
	for (MenuData &data: itemList) {
		data.info = parseMenuItemRec(data.item);
	}

	// 2nd pass: solve "default" dependencies 
    for (size_t i = 0; i < itemList.size(); i++) {
        const std::shared_ptr<userMenuInfo> &info = itemList[i].info;

		/* If the user menu item is a default one, then scan the list for
		   items with the same name and a language mode specified.
		   If one is found, then set the default index to the index of the
		   current default item. */
		if (info->umiIsDefault) {
			setDefaultIndex(itemList, i);
		}
    }
}

/*
** Cache user menus:
** Parse a single menu item. Allocate & setup a user menu info element
** holding extracted info.
*/
static std::shared_ptr<userMenuInfo> parseMenuItemRec(const MenuItem &item) {

	// allocate a new user menu info element 
    auto newInfo = std::make_shared<userMenuInfo>();

	/* determine sub-menu depth and allocate some memory
	   for hierarchical ID; init. ID with {0,.., 0} */
    newInfo->umiName = stripLanguageModeEx(item.name);

	// init. remaining parts of user menu info element 
    newInfo->umiIsDefault    = false;
    newInfo->umiDefaultIndex = static_cast<size_t>(-1);

	// assign language mode info to new user menu info element 
    parseMenuItemName(item.name, newInfo);

    return newInfo;
}

/*
** Cache user menus:
** Extract language mode related info out of given menu item name string.
** Store this info in given user menu info structure.
*/
static void parseMenuItemName(const QString &menuItemName, const std::shared_ptr<userMenuInfo> &info) {

    int index = menuItemName.indexOf(QLatin1Char('@'));
    if(index != -1) {
        QStringRef languageString = menuItemName.midRef(index);
        if(languageString == QLatin1String("*")) {
            /* only language is "*": this is for all but language specific macros */
            info->umiIsDefault = true;
            return;
        }

        QVector<QStringRef> languages = languageString.split(QLatin1Char('@'), QString::SkipEmptyParts);
        std::vector<size_t> languageModes;

        // setup a list of all language modes related to given menu item
        for(const QStringRef &language : languages) {
            /* lookup corresponding language mode index; if PLAIN is
               returned then this means, that language mode name after
               "@" is unknown (i.e. not defined) */

            size_t languageMode = FindLanguageMode(language);
            if (languageMode == PLAIN_LANGUAGE_MODE) {
                languageModes.push_back(UNKNOWN_LANGUAGE_MODE);
            } else {
                languageModes.push_back(languageMode);
            }
        }

        if (!languageModes.empty()) {
            info->umiLanguageModes = languageModes;
        }
    }
}

/*
** Cache user menus:
** Returns an allocated copy of menuItemName stripped of language mode
** parts (i.e. parts starting with "@").
*/
static QString stripLanguageModeEx(const QString &menuItemName) {
	
    int index = menuItemName.indexOf(QLatin1Char('@'));
    if(index != -1) {
        return menuItemName.mid(0, index);
    } else {
        return menuItemName;
    }
}

static void setDefaultIndex(const std::vector<MenuData> &infoList, size_t index) {
    QString defaultMenuName = infoList[index].info->umiName;

    /* Scan the list for items with the same name and a language mode
       specified. If one is found, then set the default index to the
       index of the current default item. */
    for (const MenuData &data: infoList) {
        if(const std::shared_ptr<userMenuInfo> &info = data.info) {
            if (!info->umiIsDefault && info->umiName == defaultMenuName) {
                info->umiDefaultIndex = index;
            }
        }
    }
}
