/*******************************************************************************
*                                                                              *
* userCmds.c -- Nirvana Editor shell and macro command dialogs                 *
*                                                                              *
* Copyright (C) 1999 Mark Edel                                                 *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute version of this program linked to   *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License        *
* for more details.                                                            *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* April, 1997                                                                  *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/

#include "userCmds.h"
#include "MenuItem.h"
#include "file.h"
#include "macro.h"
#include "parse.h"
#include "preferences.h"
#include <QTextStream>
#include <QVector>

/* Descriptions of the current user programmed menu items for re-generating
   menus and processing shell, macro, and background menu selections */
QVector<MenuData> ShellMenuData;
QVector<MenuData> MacroMenuData;
QVector<MenuData> BGMenuData;

namespace {

/* indicates, that an unknown (i.e. not existing) language mode
   is bound to an user menu item */
const int UNKNOWN_LANGUAGE_MODE = -2;


struct ParseError : std::exception {
public:
    ParseError(const std::string &s) : s_(s) {
    }

    virtual ~ParseError() = default;

    const char *what() const noexcept {
        return s_.c_str();
    }
private:
    std::string s_;
};

}

static char *copyMacroToEnd(const char **inPtr);
static char *writeMenuItemString(const QVector<MenuData> &menuItems, int listType);
static int loadMenuItemString(const char *inString, QVector<MenuData> &menuItems, DialogTypes listType);
static QString stripLanguageModeEx(const QString &menuItemName);
static QString writeMenuItemStringEx(const QVector<MenuData> &menuItems, int listType);
static userMenuInfo *parseMenuItemRec(MenuItem *item);
static void parseMenuItemName(const QString &menuItemName, userMenuInfo *info);
static void setDefaultIndex(const QVector<MenuData> &infoList, int index);

/*
** Generate a text string for the preferences file describing the contents
** of the shell cmd list.  This string is not exactly of the form that it
** can be read by LoadShellCmdsString, rather, it is what needs to be written
** to a resource file such that it will read back in that form.
*/
QString WriteShellCmdsStringEx() {
	return writeMenuItemStringEx(ShellMenuData, SHELL_CMDS);
}

/*
** Generate a text string for the preferences file describing the contents of
** the macro menu and background menu commands lists.  These strings are not
** exactly of the form that it can be read by LoadMacroCmdsString, rather, it
** is what needs to be written to a resource file such that it will read back
** in that form.
*/
QString WriteMacroCmdsStringEx() {
	return writeMenuItemStringEx(MacroMenuData, MACRO_CMDS);
}

QString WriteBGMenuCmdsStringEx() {
	return writeMenuItemStringEx(BGMenuData, BG_MENU_CMDS);
}

/*
** Read a string representing shell command menu items and add them to the
** internal list used for constructing shell menus
*/
int LoadShellCmdsStringEx(const QString &inString) {
	// TODO(eteran): make this more efficient
    return loadMenuItemString(inString.toLatin1().data(), ShellMenuData, SHELL_CMDS);
}

/*
** Read strings representing macro menu or background menu command menu items
** and add them to the internal lists used for constructing menus
*/
int LoadMacroCmdsStringEx(const QString &inString) {
	// TODO(eteran): make this more efficient
    return loadMenuItemString(inString.toLatin1().data(), MacroMenuData, MACRO_CMDS);
}


int LoadBGMenuCmdsStringEx(const QString &inString) {
	// TODO(eteran): make this more efficient
    return loadMenuItemString(inString.toLatin1().data(), BGMenuData, BG_MENU_CMDS);
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
	freeUserMenuInfoList(ShellMenuData);
    parseMenuItemList(ShellMenuData);

	freeUserMenuInfoList(MacroMenuData);
    parseMenuItemList(MacroMenuData);

	freeUserMenuInfoList(BGMenuData);
    parseMenuItemList(BGMenuData);
}

static char *writeMenuItemString(const QVector<MenuData> &menuItems, int listType) {

	int length;

	/* determine the max. amount of memory needed for the returned string
	   and allocate a buffer for composing the string */
	   
	// NOTE(eteran): this code unconditionally writes at least 2 chars
	// so to avoid an off by one error, this needs to be initialized to
	// 1
	length = 1;
	
	for (const MenuData &data : menuItems) {
		MenuItem *f = data.item;
        QString accStr = f->shortcut.toString();

		length += f->name.size() * 2; // allow for \n & \\ expansions 
        length += accStr.size();
		length += f->cmd.size() * 6; // allow for \n & \\ expansions 
		length += 21;                 // number of characters added below 
	}
	
	length++; // terminating null 
    char *outStr = new char[length];

	// write the string 
	char *outPtr = outStr;
	
	for (const MenuData &data : menuItems) {
		MenuItem *f = data.item;
		
        QString accStr = f->shortcut.toString();
		*outPtr++ = '\t';
		
		for (auto ch : f->name) { // Copy the command name 
            *outPtr++ = ch.toLatin1();
		}
		
		*outPtr++ = ':';
        strcpy(outPtr, accStr.toLatin1());
        outPtr += accStr.size();

        *outPtr++ = ':';
        if (listType == SHELL_CMDS) {
            switch(f->input) {
            case FROM_SELECTION: *outPtr++ = 'I'; break;
            case FROM_WINDOW:    *outPtr++ = 'A'; break;
            case FROM_EITHER:    *outPtr++ = 'E'; break;
            default:
                break;
            }

            switch(f->output) {
            case TO_DIALOG:     *outPtr++ = 'D'; break;
            case TO_NEW_WINDOW: *outPtr++ = 'W'; break;
            default:
                break;
            }

            if (f->repInput)  *outPtr++ = 'X';
            if (f->saveFirst) *outPtr++ = 'S';
            if (f->loadAfter) *outPtr++ = 'L';

			*outPtr++ = ':';
		} else {
            if (f->input == FROM_SELECTION) {
				*outPtr++ = 'R';
            }

			*outPtr++ = ':';
			*outPtr++ = ' ';
			*outPtr++ = '{';
		}

		*outPtr++ = '\n';
		*outPtr++ = '\t';
		*outPtr++ = '\t';
		
		for(QChar c : f->cmd) {
            if (c == QLatin1Char('\n')) { // and newlines to backslash-n's,
				*outPtr++ = '\n';
				*outPtr++ = '\t';
				*outPtr++ = '\t';
			} else
				*outPtr++ = c.toLatin1();
		}
		
		if (listType == MACRO_CMDS || listType == BG_MENU_CMDS) {
			if (*(outPtr - 1) == '\t') {
				outPtr--;
			}
			*outPtr++ = '}';
		}
		
		*outPtr++ = '\n';
	}

	*--outPtr = '\0';
	return outStr;
}

static QString writeMenuItemStringEx(const QVector<MenuData> &menuItems, int listType) {
	QString str;
    if(const char *s = writeMenuItemString(menuItems, listType)) {
        str = QString::fromLatin1(s);
        delete [] s;
	}
    return str;
}

static int loadMenuItemString(const char *inString, QVector<MenuData> &menuItems, DialogTypes listType) {

    try {
        const char *inPtr = inString;

        Q_FOREVER {

            // remove leading whitespace
            while (*inPtr == ' ' || *inPtr == '\t')
                inPtr++;

            // end of string in proper place
            if (*inPtr == '\0') {
                return true;
            }

            // read name field
            int nameLen = strcspn(inPtr, ":");
            if (nameLen == 0)
                throw ParseError("no name field");

            auto nameStr = QString::fromLatin1(inPtr, nameLen);

            inPtr += nameLen;
            if (*inPtr == '\0')
                throw ParseError("end not expected");
            inPtr++;

            // read accelerator field
            int accLen = strcspn(inPtr, ":");

            auto accStr = QString::fromLatin1(inPtr, accLen);

            inPtr += accLen;
            if (*inPtr == '\0')
                throw ParseError("end not expected");
            inPtr++;

            // read flags field
            InSrcs input = FROM_NONE;
            OutDests output = TO_SAME_WINDOW;
            bool repInput = false;
            bool saveFirst = false;
            bool loadAfter = false;
            for (; *inPtr != ':'; inPtr++) {
                if (listType == SHELL_CMDS) {

                    switch(*inPtr) {
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
                        throw ParseError("unreadable flag field");
                    }
                } else {
                    switch(*inPtr) {
                    case 'R':
                        input = FROM_SELECTION;
                        break;
                    default:
                        throw ParseError("unreadable flag field");
                    }
                }
            }
            inPtr++;

            QString cmdStr;

            // read command field
            if (listType == SHELL_CMDS) {
                if (*inPtr++ != '\n')
                    throw ParseError("command must begin with newline");
                while (*inPtr == ' ' || *inPtr == '\t') // leading whitespace
                    inPtr++;
                int cmdLen = strcspn(inPtr, "\n");
                if (cmdLen == 0)
                    throw ParseError("shell command field is empty");

                cmdStr = QString::fromLatin1(inPtr, cmdLen);
                inPtr += cmdLen;
            } else {
                char *p = copyMacroToEnd(&inPtr);
                if(!p) {
                    return false;
                }

                cmdStr = QString::fromLatin1(p);
                delete [] p;
            }
            while (*inPtr == ' ' || *inPtr == '\t' || *inPtr == '\n') {
                inPtr++; // skip trailing whitespace & newline
            }

            // parse the accelerator field
            QKeySequence shortcut = QKeySequence::fromString(accStr);

            // create a menu item record
            auto f = new MenuItem;
            f->name      = nameStr;
            f->cmd       = cmdStr;
            f->input     = input;
            f->output    = output;
            f->repInput  = repInput;
            f->saveFirst = saveFirst;
            f->loadAfter = loadAfter;
            f->shortcut  = shortcut;

            // add/replace menu record in the list
            bool found = false;
            for (MenuData &data: menuItems) {
                if (data.item->name == f->name) {
                    delete data.item;
                    data.item = f;
                    found = true;
                    break;
                }
            }

            if (!found) {
                menuItems.push_back({f, nullptr});
            }
        }
    } catch(const ParseError &ex) {
        qDebug("NEdit: Parse error in user defined menu item, %s", ex.what());
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
static char *copyMacroToEnd(const char **inPtr) {

    const char *&ptr = *inPtr;
    const char *begin = ptr;
    const char *errMsg;


    // Skip over whitespace to find make sure there's a beginning brace
    // to anchor the parse (if not, it will take the whole file)
    ptr += strspn(ptr, " \t\n");

    if (*ptr != '{') {
        ParseErrorEx(nullptr, QString::fromLatin1(ptr), ptr - 1 - begin, QLatin1String("macro menu item"), QLatin1String("expecting '{'"));
		return nullptr;
	}

	// Parse the input 
    const char *stoppedAt;
    Program *const prog = ParseMacro(ptr, &errMsg, &stoppedAt);
	if(!prog) {
        ParseErrorEx(nullptr, QString::fromLatin1(ptr), stoppedAt - begin, QLatin1String("macro menu item"), QString::fromLatin1(errMsg));
		return nullptr;
	}
	FreeProgram(prog);

    // Copy and return the body of the macro, stripping outer braces and
    // extra leading tabs added by the writer routine
    ptr++;
    ptr += strspn(ptr, " \t");

    if (*ptr == '\n') {
        ptr++;
    }

    if (*ptr == '\t') {
        ptr++;
    }

    if (*ptr == '\t') {
        ptr++;
    }


    auto retStr = new char[stoppedAt - ptr + 1];
    char *retPtr = retStr;

    for (const char *p = ptr; p < stoppedAt - 1; p++) {
		if (!strncmp(p, "\n\t\t", 3)) {
			*retPtr++ = '\n';
			p += 2;
        } else {
			*retPtr++ = *p;
        }
	}

    if (*(retPtr - 1) == '\t') {
		retPtr--;
    }

	*retPtr = '\0';

    ptr = stoppedAt;
	return retStr;
}

/*
** Cache user menus:
** Parse given menu item list and setup a user menu info list for
** management of user menu.
*/

void parseMenuItemList(QVector<MenuData> &itemList) {

	/* 1st pass: setup user menu info: extract language modes, menu name &
	   default indication; build user menu ID */
	for (MenuData &data: itemList) {
		data.info = parseMenuItemRec(data.item);
	}

	// 2nd pass: solve "default" dependencies 
	for (int i = 0; i < itemList.size(); i++) {
		userMenuInfo *info = itemList[i].info;

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
static userMenuInfo *parseMenuItemRec(MenuItem *item) {

	// allocate a new user menu info element 
	auto newInfo = new userMenuInfo;

	/* determine sub-menu depth and allocate some memory
	   for hierarchical ID; init. ID with {0,.., 0} */
	newInfo->umiName = stripLanguageModeEx(item->name);

	// init. remaining parts of user menu info element 
    newInfo->umiIsDefault          = false;
    newInfo->umiDefaultIndex       = -1;

	// assign language mode info to new user menu info element 
    parseMenuItemName(item->name, newInfo);

	return newInfo;
}

/*
** Cache user menus:
** Extract language mode related info out of given menu item name string.
** Store this info in given user menu info structure.
*/
static void parseMenuItemName(const QString &menuItemName, userMenuInfo *info) {


    int index = menuItemName.indexOf(QLatin1Char('@'));
    if(index != -1) {
        QStringRef languageString = menuItemName.midRef(index);
        if(languageString == QLatin1String("*")) {
            /* only language is "*": this is for all but language specific macros */
            info->umiIsDefault = true;
            return;
        }

        QVector<QStringRef> languages = languageString.split(QLatin1Char('@'), QString::SkipEmptyParts);
        QVector<int> languageModes;

        // setup a list of all language modes related to given menu item
        for(const QStringRef &language : languages) {
            /* lookup corresponding language mode index; if PLAIN is
               returned then this means, that language mode name after
               "@" is unknown (i.e. not defined) */

            int languageMode = FindLanguageMode(language);
            if (languageMode == PLAIN_LANGUAGE_MODE) {
                languageModes.push_back(UNKNOWN_LANGUAGE_MODE);
            } else {
                languageModes.push_back(languageMode);
            }
        }

        if (!languageModes.isEmpty()) {
            info->umiLanguageModes = languageModes;
        }
    }

#if 0
    if (const char *atPtr = strchr(menuItemName, '@')) {
		if (!strcmp(atPtr + 1, "*")) {
            /* only language is "*": this is for all but language specific macros */
            info->umiIsDefault = true;
			return;
		}

        QVector<int> languageModes;

		// setup a list of all language modes related to given menu item 
		while (atPtr) {

            const char *endPtr;

			// extract language mode name after "@" sign 
            for (endPtr = atPtr + 1; isalnum((uint8_t)*endPtr) || *endPtr == '_' || *endPtr == '-' || *endPtr == ' ' || *endPtr == '+' || *endPtr == '$' || *endPtr == '#'; endPtr++) {
				;
            }

			/* lookup corresponding language mode index; if PLAIN is
			   returned then this means, that language mode name after
			   "@" is unknown (i.e. not defined) */

            int languageMode = FindLanguageMode(QString::fromLatin1(atPtr + 1, endPtr - (atPtr + 1)));
            if (languageMode == PLAIN_LANGUAGE_MODE) {
                languageModes.push_back(UNKNOWN_LANGUAGE_MODE);
			} else {
                languageModes.push_back(languageMode);
			}

			// look for next "@" 
			atPtr = strchr(endPtr, '@');
		}

        if (!languageModes.isEmpty()) {
            info->umiLanguageModes = languageModes;
		}
	}
#endif
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

static void setDefaultIndex(const QVector<MenuData> &infoList, int index) {
    QString defaultMenuName = infoList[index].info->umiName;

    /* Scan the list for items with the same name and a language mode
       specified. If one is found, then set the default index to the
       index of the current default item. */

    for (const MenuData &data: infoList) {
        userMenuInfo *info = data.info;

        if (!info->umiIsDefault && info->umiName == defaultMenuName) {
            info->umiDefaultIndex = index;
        }
    }
}

void freeUserMenuInfoList(QVector<MenuData> &menuList) {
    for(MenuData &data: menuList) {
        delete data.item;
        delete data.info;
	}

    menuList.clear();
}




