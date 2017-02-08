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

#include <QVector>
#include "userCmds.h"
#include "MenuItem.h"
#include "file.h"
#include "macro.h"
#include "preferences.h"
#include "parse.h"
#include "util/MotifHelper.h"

namespace {

/* indicates, that an unknown (i.e. not existing) language mode
   is bound to an user menu item */
const int UNKNOWN_LANGUAGE_MODE = -2;

}

/* Structure for keeping track of hierarchical sub-menus during user-menu
   creation */
struct menuTreeItem {
	QString name;
	Widget  menuPane;
};

/* Descriptions of the current user programmed menu items for re-generating
   menus and processing shell, macro, and background menu selections */
QVector<MenuData>  ShellMenuData;
userSubMenuCache ShellSubMenus;

QVector<MenuData> MacroMenuData;
userSubMenuCache MacroSubMenus;

QVector<MenuData> BGMenuData;
userSubMenuCache BGSubMenus;

static char *copySubstring(const char *string, int length);
static int parseError(const char *message);
static char *copyMacroToEnd(const char **inPtr);
static int getSubMenuDepth(const char *menuName);
static userMenuInfo *parseMenuItemRec(MenuItem *item);
static void parseMenuItemName(char *menuItemName, userMenuInfo *info);
static void generateUserMenuId(userMenuInfo *info, userSubMenuCache *subMenus);
static userSubMenuInfo *findSubMenuInfo(userSubMenuCache *subMenus, const char *hierName);
static char *stripLanguageModeEx(const QString &menuItemName);
static void freeUserMenuInfo(userMenuInfo *info);
static void allocSubMenuCache(userSubMenuCache *subMenus, int nbrOfItems);

static void setDefaultIndex(const QVector<MenuData> &infoList, int defaultIdx);
static int loadMenuItemString(const char *inString, QVector<MenuData> &menuItems, int listType);
static QString writeMenuItemStringEx(const QVector<MenuData> &menuItems, int listType);
static char *writeMenuItemString(const QVector<MenuData> &menuItems, int listType);

/*
** Generate a text string for the preferences file describing the contents
** of the shell cmd list.  This string is not exactly of the form that it
** can be read by LoadShellCmdsString, rather, it is what needs to be written
** to a resource file such that it will read back in that form.
*/
QString WriteShellCmdsStringEx(void) {
	return writeMenuItemStringEx(ShellMenuData, SHELL_CMDS);
}

/*
** Generate a text string for the preferences file describing the contents of
** the macro menu and background menu commands lists.  These strings are not
** exactly of the form that it can be read by LoadMacroCmdsString, rather, it
** is what needs to be written to a resource file such that it will read back
** in that form.
*/
QString WriteMacroCmdsStringEx(void) {
	return writeMenuItemStringEx(MacroMenuData, MACRO_CMDS);
}

QString WriteBGMenuCmdsStringEx(void) {
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
	parseMenuItemList(ShellMenuData, &ShellSubMenus);
	parseMenuItemList(MacroMenuData, &MacroSubMenus);
	parseMenuItemList(BGMenuData,    &BGSubMenus);
}

/*
** Cache user menus:
** Update user menu info to take into account e.g. change of language modes
** (i.e. add / move / delete of language modes etc).
*/
void UpdateUserMenuInfo() {
	freeUserMenuInfoList(ShellMenuData);
	parseMenuItemList(ShellMenuData, &ShellSubMenus);

	freeUserMenuInfoList(MacroMenuData);
	parseMenuItemList(MacroMenuData, &MacroSubMenus);

	freeUserMenuInfoList(BGMenuData);
	parseMenuItemList(BGMenuData, &BGSubMenus);
}

static char *copySubstring(const char *string, int length) {
	char *retStr = XtMalloc(length + 1);

	strncpy(retStr, string, length);
	retStr[length] = '\0';
	return retStr;
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

        if (f->mnemonic != '\0') {
			*outPtr++ = f->mnemonic;
        }

		*outPtr++ = ':';
		if (listType == SHELL_CMDS) {
			if (f->input == FROM_SELECTION)
				*outPtr++ = 'I';
			else if (f->input == FROM_WINDOW)
				*outPtr++ = 'A';
			else if (f->input == FROM_EITHER)
				*outPtr++ = 'E';
			if (f->output == TO_DIALOG)
				*outPtr++ = 'D';
			else if (f->output == TO_NEW_WINDOW)
				*outPtr++ = 'W';
			if (f->repInput)
				*outPtr++ = 'X';
			if (f->saveFirst)
				*outPtr++ = 'S';
			if (f->loadAfter)
				*outPtr++ = 'L';
			*outPtr++ = ':';
		} else {
			if (f->input == FROM_SELECTION)
				*outPtr++ = 'R';
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
	if(char *s = writeMenuItemString(menuItems, listType)) {
        str = QString::fromLatin1(s);
        delete [] s;
	}
	return str;
}

static int loadMenuItemString(const char *inString, QVector<MenuData> &menuItems, int listType) {

	const char *inPtr = inString;

    QString cmdStr;
    QString nameStr;
    QString accStr;
	char mneChar;
	int nameLen;
	int accLen;
	int mneLen;
	int cmdLen;

	for (;;) {

		// remove leading whitespace 
		while (*inPtr == ' ' || *inPtr == '\t')
			inPtr++;

		// end of string in proper place 
		if (*inPtr == '\0') {
			return True;
		}

		// read name field 
		nameLen = strcspn(inPtr, ":");
		if (nameLen == 0)
			return parseError("no name field");

        nameStr = QString::fromLatin1(inPtr, nameLen);

		inPtr += nameLen;
		if (*inPtr == '\0')
			return parseError("end not expected");
		inPtr++;

		// read accelerator field 
		accLen = strcspn(inPtr, ":");

        accStr = QString::fromLatin1(inPtr, accLen);

        inPtr += accLen;
		if (*inPtr == '\0')
			return parseError("end not expected");
		inPtr++;

		// read menemonic field 
		mneLen = strcspn(inPtr, ":");
		if (mneLen > 1)
			return parseError("mnemonic field too long");
		if (mneLen == 1)
			mneChar = *inPtr++;
		else
			mneChar = '\0';
		inPtr++;
		if (*inPtr == '\0')
			return parseError("end not expected");

		// read flags field 
		InSrcs input = FROM_NONE;
		OutDests output = TO_SAME_WINDOW;
		bool repInput = false;
		bool saveFirst = false;
		bool loadAfter = false;
		for (; *inPtr != ':'; inPtr++) {
			if (listType == SHELL_CMDS) {
				if (*inPtr == 'I')
					input = FROM_SELECTION;
				else if (*inPtr == 'A')
					input = FROM_WINDOW;
				else if (*inPtr == 'E')
					input = FROM_EITHER;
				else if (*inPtr == 'W')
					output = TO_NEW_WINDOW;
				else if (*inPtr == 'D')
					output = TO_DIALOG;
				else if (*inPtr == 'X')
                    repInput = true;
				else if (*inPtr == 'S')
                    saveFirst = true;
				else if (*inPtr == 'L')
                    loadAfter = true;
				else
					return parseError("unreadable flag field");
			} else {
				if (*inPtr == 'R')
					input = FROM_SELECTION;
				else
					return parseError("unreadable flag field");
			}
		}
		inPtr++;

		// read command field 
		if (listType == SHELL_CMDS) {
			if (*inPtr++ != '\n')
				return parseError("command must begin with newline");
			while (*inPtr == ' ' || *inPtr == '\t') // leading whitespace 
				inPtr++;
			cmdLen = strcspn(inPtr, "\n");
			if (cmdLen == 0)
				return parseError("shell command field is empty");

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
		f->mnemonic  = mneChar;
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
}

static int parseError(const char *message) {
	fprintf(stderr, "NEdit: Parse error in user defined menu item, %s\n", message);
	return False;
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

    const char *errMsg;
    const char *stoppedAt;
	const char *p;

	/* Skip over whitespace to find make sure there's a beginning brace
	   to anchor the parse (if not, it will take the whole file) */
	*inPtr += strspn(*inPtr, " \t\n");
    if (**inPtr != '{') {
        ParseError(nullptr, *inPtr, *inPtr - 1, "macro menu item", "expecting '{'");
		return nullptr;
	}

	// Parse the input 
    Program *prog = ParseMacro(*inPtr, &errMsg, &stoppedAt);
	if(!prog) {
        ParseError(nullptr, *inPtr, stoppedAt, "macro menu item", errMsg);
		return nullptr;
	}
	FreeProgram(prog);

	/* Copy and return the body of the macro, stripping outer braces and
	   extra leading tabs added by the writer routine */
	(*inPtr)++;
	*inPtr += strspn(*inPtr, " \t");

    if (**inPtr == '\n')
		(*inPtr)++;

    if (**inPtr == '\t')
		(*inPtr)++;

	if (**inPtr == '\t')
		(*inPtr)++;

    char *retStr;
    char *retPtr;

    retStr = new char[stoppedAt - *inPtr + 1];
    retPtr = retStr;

	for (p = *inPtr; p < stoppedAt - 1; p++) {
		if (!strncmp(p, "\n\t\t", 3)) {
			*retPtr++ = '\n';
			p += 2;
        } else {
			*retPtr++ = *p;
        }
	}

	if (*(retPtr - 1) == '\t')
		retPtr--;
	*retPtr = '\0';

	*inPtr = stoppedAt;


	return retStr;
}

/*
** Cache user menus:
** Parse given menu item list and setup a user menu info list for
** management of user menu.
*/

void parseMenuItemList(QVector<MenuData> &itemList, userSubMenuCache *subMenus) {
	// Allocate storage for structures to keep track of sub-menus 
	allocSubMenuCache(subMenus, itemList.size());

	/* 1st pass: setup user menu info: extract language modes, menu name &
	   default indication; build user menu ID */
	for (MenuData &data: itemList) {
		data.info = parseMenuItemRec(data.item);
		generateUserMenuId(data.info, subMenus);
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
** Returns the sub-menu depth (i.e. nesting level) of given
** menu name.
*/
static int getSubMenuDepth(const char *menuName) {

	int depth = 0;

	// determine sub-menu depth by counting '>' of given "menuName" 
	const char *subSep = menuName;
	while ((subSep = strchr(subSep, '>'))) {
		depth++;
		subSep++;
	}

	return depth;
}

/*
** Cache user menus:
** Parse a singe menu item. Allocate & setup a user menu info element
** holding extracted info.
*/
static userMenuInfo *parseMenuItemRec(MenuItem *item) {

	// allocate a new user menu info element 
	auto newInfo = new userMenuInfo;

	/* determine sub-menu depth and allocate some memory
	   for hierarchical ID; init. ID with {0,.., 0} */
	newInfo->umiName = stripLanguageModeEx(item->name);

	int subMenuDepth = getSubMenuDepth(newInfo->umiName);

	newInfo->umiId = new int[subMenuDepth + 1]();

	// init. remaining parts of user menu info element 
    newInfo->umiIdLen              = 0;
    newInfo->umiIsDefault          = false;
	newInfo->umiNbrOfLanguageModes = 0;
    newInfo->umiLanguageMode       = nullptr;
    newInfo->umiDefaultIndex       = -1;
    newInfo->umiToBeManaged        = false;

	// assign language mode info to new user menu info element 
	parseMenuItemName(item->name.toLatin1().data(), newInfo);

	return newInfo;
}

/*
** Cache user menus:
** Extract language mode related info out of given menu item name string.
** Store this info in given user menu info structure.
*/
static void parseMenuItemName(char *menuItemName, userMenuInfo *info) {
	char *endPtr;
	char c;
	int languageMode;
	int langModes[MAX_LANGUAGE_MODES];
	int nbrLM = 0;
	int size;

	if (char *atPtr = strchr(menuItemName, '@')) {
		if (!strcmp(atPtr + 1, "*")) {
			/* only language is "*": this is for all but language specific
			   macros */
            info->umiIsDefault = true;
			return;
		}

		// setup a list of all language modes related to given menu item 
		while (atPtr) {
			// extract language mode name after "@" sign 
			for (endPtr = atPtr + 1; isalnum((uint8_t)*endPtr) || *endPtr == '_' || *endPtr == '-' || *endPtr == ' ' || *endPtr == '+' || *endPtr == '$' || *endPtr == '#'; endPtr++)
				;

			/* lookup corresponding language mode index; if PLAIN is
			   returned then this means, that language mode name after
			   "@" is unknown (i.e. not defined) */
			c = *endPtr;
			*endPtr = '\0';
            languageMode = FindLanguageMode(QString::fromLatin1(atPtr + 1));
			if (languageMode == PLAIN_LANGUAGE_MODE) {
				langModes[nbrLM] = UNKNOWN_LANGUAGE_MODE;
			} else {
				langModes[nbrLM] = languageMode;
			}
			nbrLM++;
			*endPtr = c;

			// look for next "@" 
			atPtr = strchr(endPtr, '@');
		}

		if (nbrLM != 0) {
			info->umiNbrOfLanguageModes = nbrLM;
			size = sizeof(int) * nbrLM;
			info->umiLanguageMode = (int *)XtMalloc(size);
			memcpy(info->umiLanguageMode, langModes, size);
		}
	}
}

/*
** Cache user menus:
** generates an ID (= array of integers) of given user menu info, which
** allows to find the user menu  item within the menu tree later on: 1st
** integer of ID indicates position within main menu; 2nd integer indicates
** position within 1st sub-menu etc.
*/
static void generateUserMenuId(userMenuInfo *info, userSubMenuCache *subMenus) {

	char *hierName, *subSep;
	int subMenuDepth = 0;
	int *menuIdx = &subMenus->usmcNbrOfMainMenuItems;
	userSubMenuInfo *curSubMenu;

	/* find sub-menus, stripping off '>' until item name is
	   reached */
	subSep = info->umiName;
	while ((subSep = strchr(subSep, '>'))) {
		hierName = copySubstring(info->umiName, subSep - info->umiName);
		curSubMenu = findSubMenuInfo(subMenus, hierName);
		if(!curSubMenu) {
			/* sub-menu info not stored before: new sub-menu;
			   remember its hierarchical position */
			info->umiId[subMenuDepth] = *menuIdx;
			(*menuIdx)++;

			/* store sub-menu info in list of subMenus; allocate
			   some memory for hierarchical ID of sub-menu & take over
			   current hierarchical ID of current user menu info */
			curSubMenu = &subMenus->usmcInfo[subMenus->usmcNbrOfSubMenus];
			subMenus->usmcNbrOfSubMenus++;
			curSubMenu->usmiName = hierName;

			curSubMenu->usmiId = new int[subMenuDepth + 2];			
			std::copy_n(info->umiId, subMenuDepth + 2, curSubMenu->usmiId);
			
			curSubMenu->usmiIdLen = subMenuDepth + 1;
		} else {
			/* sub-menu info already stored before: takeover its
			   hierarchical position */
			XtFree(hierName);
			info->umiId[subMenuDepth] = curSubMenu->usmiId[subMenuDepth];
		}

		subMenuDepth++;
		menuIdx = &curSubMenu->usmiId[subMenuDepth];

		subSep++;
	}

	// remember position of menu item within final (sub) menu 
	info->umiId[subMenuDepth] = *menuIdx;
	info->umiIdLen = subMenuDepth + 1;
	(*menuIdx)++;
}

/*
** Cache user menus:
** Find info corresponding to a hierarchical menu name (a>b>c...)
*/
static userSubMenuInfo *findSubMenuInfo(userSubMenuCache *subMenus, const char *hierName) {

	for (int i = 0; i < subMenus->usmcNbrOfSubMenus; i++) {
		if (!strcmp(hierName, subMenus->usmcInfo[i].usmiName)) {
			return &subMenus->usmcInfo[i];
		}
	}
	
	return nullptr;
}

/*
** Cache user menus:
** Returns an allocated copy of menuItemName stripped of language mode
** parts (i.e. parts starting with "@").
*/
static char *stripLanguageModeEx(const QString &menuItemName) {
	
	QByteArray latin1 = menuItemName.toLatin1();
	
	if(const char *firstAtPtr = strchr(latin1.data(), '@')) {
		return copySubstring(latin1.data(), firstAtPtr - latin1.data());
	} else {
        return XtNewStringEx(menuItemName);
	}
}

static void setDefaultIndex(const QVector<MenuData> &infoList, int defaultIdx) {
	char *defaultMenuName = infoList[defaultIdx].info->umiName;

	/* Scan the list for items with the same name and a language mode
	   specified. If one is found, then set the default index to the
	   index of the current default item. */
	
	for (const MenuData &data: infoList) {
		userMenuInfo *info = data.info;

		if (!info->umiIsDefault && strcmp(info->umiName, defaultMenuName) == 0) {
			info->umiDefaultIndex = defaultIdx;
		}
	}
}

void freeUserMenuInfoList(QVector<MenuData> &infoList) {
	for(MenuData &data: infoList) {			
		freeUserMenuInfo(data.info);
	}
}

static void freeUserMenuInfo(userMenuInfo *info) {


	delete [] info->umiId;

	if (info->umiNbrOfLanguageModes != 0)
		XtFree((char *)info->umiLanguageMode);

	delete [] info;
}

/*
** Cache user menus:
** Allocate & init. storage for structures to manage sub-menus
*/
static void allocSubMenuCache(userSubMenuCache *subMenus, int nbrOfItems) {
	subMenus->usmcNbrOfMainMenuItems = 0;
	subMenus->usmcNbrOfSubMenus      = 0;
	subMenus->usmcInfo               = new userSubMenuInfo[nbrOfItems];
}




