
#include "userCmds.h"
#include "MenuData.h"
#include "MenuItem.h"
#include "interpret.h"
#include "parse.h"
#include "Util/Input.h"
#include "preferences.h"
#include "LanguageMode.h"
#include "Util/raise.h"
#include <QTextStream>
#include <memory>

/* Descriptions of the current user programmed menu items for re-generating
   menus and processing shell, macro, and background menu selections */
std::vector<MenuData> ShellMenuData;
std::vector<MenuData> MacroMenuData;
std::vector<MenuData> BGMenuData;

namespace {

/*
** Scan text from "*in" to the end of macro input (matching brace),
** advancing in, and return macro text as function return value.
**
** This is kind of wastefull in that it throws away the compiled macro,
** to be re-generated from the text as needed, but compile time is
** negligible for most macros.
*/
QString copyMacroToEnd(Input &in) {

    Input input = in;

    // Skip over whitespace to find make sure there's a beginning brace
    // to anchor the parse (if not, it will take the whole file)
    input.skipWhitespaceNL();

    QString code = input.mid();

    if (!code.startsWith(QLatin1Char('{'))) {
        Preferences::reportError(
                    nullptr,
                    code,
                    input.index() - in.index(),
                    QLatin1String("macro menu item"),
                    QLatin1String("expecting '{'"));

        return QString();
    }

    // Parse the input
    int stoppedAt;
    QString errMsg;

    Program *const prog = ParseMacro(code, &errMsg, &stoppedAt);
    if(!prog) {
        Preferences::reportError(
                    nullptr,
                    code,
                    stoppedAt,
                    QLatin1String("macro menu item"),
                    errMsg);

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

QString writeMenuItemString(const std::vector<MenuData> &menuItems, CommandTypes listType) {

    QString outStr;
    auto outPtr = std::back_inserter(outStr);

    for (const MenuData &data : menuItems) {
        auto &f = data.item;

        QString accStr = f.shortcut.toString();
        *outPtr++ = QLatin1Char('\t');

        std::copy(f.name.begin(), f.name.end(), outPtr);
        *outPtr++ = QLatin1Char(':');

        std::copy(accStr.begin(), accStr.end(), outPtr);
        *outPtr++ = QLatin1Char(':');

        if (listType == CommandTypes::SHELL_CMDS) {
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

        if (listType == CommandTypes::MACRO_CMDS || listType == CommandTypes::BG_MENU_CMDS) {

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

bool loadMenuItemStringEx(const QString &inString, std::vector<MenuData> &menuItems, CommandTypes listType) {

    struct ParseError {
        std::string message;
    };

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
                Raise<ParseError>("no name field");
            }

            if(in.atEnd()) {
                Raise<ParseError>("end not expected");
            }

            ++in;

            // read accelerator field
            QString accStr = in.readUntil(QLatin1Char(':'));

            if(in.atEnd()) {
                Raise<ParseError>("end not expected");
            }

            ++in;

            // read flags field
            InSrcs input    = FROM_NONE;
            OutDests output = TO_SAME_WINDOW;
            bool repInput   = false;
            bool saveFirst  = false;
            bool loadAfter  = false;

            for(; !in.atEnd() && *in != QLatin1Char(':'); ++in) {

                if (listType == CommandTypes::SHELL_CMDS) {

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
                        Raise<ParseError>("unreadable flag field");
                    }
                } else {
                    switch((*in).toLatin1()) {
                    case 'R':
                        input = FROM_SELECTION;
                        break;
                    default:
                        Raise<ParseError>("unreadable flag field");
                    }
                }
            }
            ++in;

            QString cmdStr;

            // read command field
            if (listType == CommandTypes::SHELL_CMDS) {

                if (*in++ != QLatin1Char('\n')) {
                    Raise<ParseError>("command must begin with newline");
                }

                // leading whitespace
                in.skipWhitespace();

                cmdStr = in.readUntil(QLatin1Char('\n'));

                if (cmdStr.isEmpty()) {
                    Raise<ParseError>("shell command field is empty");
                }

            } else {

                QString p = copyMacroToEnd(in);
                if(p.isNull()) {
                    return false;
                }

                cmdStr = p;
            }

            in.skipWhitespaceNL();

            // create a menu item record
            auto f = std::make_unique<MenuItem>();
            f->name      = nameStr;
            f->cmd       = cmdStr;
            f->input     = input;
            f->output    = output;
            f->repInput  = repInput;
            f->saveFirst = saveFirst;
            f->loadAfter = loadAfter;
            f->shortcut  = QKeySequence::fromString(accStr);

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
    } catch(const ParseError &error) {
        qWarning("NEdit: Parse error in user defined menu item, %s", error.message.c_str());
        return false;
    }
}

/**
 * @brief setDefaultIndex
 * @param infoList
 * @param index
 */
void setDefaultIndex(const std::vector<MenuData> &infoList, size_t index) {
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


/*
** Cache user menus:
** Extract language mode related info out of given menu item name string.
** Store this info in given user menu info structure.
*/
void parseMenuItemName(const QString &menuItemName, const std::shared_ptr<userMenuInfo> &info) {

    int index = menuItemName.indexOf(QLatin1Char('@'));
    if(index != -1) {
        QString languageString = menuItemName.mid(index);
        if(languageString == QLatin1String("*")) {
            /* only language is "*": this is for all but language specific macros */
            info->umiIsDefault = true;
            return;
        }

        QStringList languages = languageString.split(QLatin1Char('@'), QString::SkipEmptyParts);
        std::vector<size_t> languageModes;

        // setup a list of all language modes related to given menu item
        for(const QString &language : languages) {
            /* lookup corresponding language mode index; if PLAIN is
               returned then this means, that language mode name after
               "@" is unknown (i.e. not defined) */

            size_t languageMode = Preferences::FindLanguageMode(language);
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
** Returns the menuItemName stripped of language mode parts
** (i.e. parts starting with "@").
*/
QString stripLanguageModeEx(const QString &menuItemName) {

    int index = menuItemName.indexOf(QLatin1Char('@'));
    if(index != -1) {
        return menuItemName.mid(0, index);
    } else {
        return menuItemName;
    }
}

/*
** Cache user menus:
** Parse a single menu item. Allocate & setup a user menu info element
** holding extracted info.
*/
std::shared_ptr<userMenuInfo> parseMenuItemRec(const MenuItem &item) {

    auto newInfo = std::make_shared<userMenuInfo>();

    newInfo->umiName = stripLanguageModeEx(item.name);

    // init. remaining parts of user menu info element
    newInfo->umiIsDefault    = false;
    newInfo->umiDefaultIndex = static_cast<size_t>(-1);

    // assign language mode info to new user menu info element
    parseMenuItemName(item.name, newInfo);

    return newInfo;
}

}

std::vector<MenuData> &selectMenu(CommandTypes type) {
    switch(type) {
    case CommandTypes::SHELL_CMDS:
        return ShellMenuData;
    case CommandTypes::MACRO_CMDS:
        return MacroMenuData;
    case CommandTypes::BG_MENU_CMDS:
        return BGMenuData;
    }

    Q_UNREACHABLE();
}

MenuData *findMenuItem(const QString &name, CommandTypes type) {

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
    return writeMenuItemString(ShellMenuData, CommandTypes::SHELL_CMDS);
}

QString WriteMacroCmdsStringEx() {
    return writeMenuItemString(MacroMenuData, CommandTypes::MACRO_CMDS);
}

QString WriteBGMenuCmdsStringEx() {
    return writeMenuItemString(BGMenuData, CommandTypes::BG_MENU_CMDS);
}

/*
** Read a string representing shell command menu items, macro menu or
** background menu and add them to the internal list used for constructing
** menus
*/
bool LoadShellCmdsStringEx(const QString &inString) {
    return loadMenuItemStringEx(inString, ShellMenuData, CommandTypes::SHELL_CMDS);
}

bool LoadMacroCmdsStringEx(const QString &inString) {
    return loadMenuItemStringEx(inString, MacroMenuData, CommandTypes::MACRO_CMDS);
}

bool LoadBGMenuCmdsStringEx(const QString &inString) {
    return loadMenuItemStringEx(inString, BGMenuData, CommandTypes::BG_MENU_CMDS);
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
