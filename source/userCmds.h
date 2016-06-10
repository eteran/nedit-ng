/*******************************************************************************
*                                                                              *
* userCmds.h -- Nirvana Editor user commands header file                       *
*                                                                              *
* Copyright 2004 The NEdit Developers                                          *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute versions of this program linked to  *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for    *
* more details.                                                                *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* July 31, 2001                                                                *
*                                                                              *
*******************************************************************************/

#ifndef USER_CMDS_H_
#define USER_CMDS_H_

#include <QString>
#include "nedit.h"
#include "string_view.h"

// types of current dialog and/or menu 
enum DialogTypes {
	SHELL_CMDS,
	MACRO_CMDS,
	BG_MENU_CMDS
};



class MenuItem;

/* Structure holding info about a single menu item.
   According to above example there exist 5 user menu items:
   a.) "menuItem1"  (hierarchical ID = {0} means: element nbr. "0" of main menu)
   b.) "menuItemA1" (hierarchical ID = {1, 0} means: el. nbr. "0" of
                     "subMenuA", which itself is el. nbr. "1" of main menu)
   c.) "menuItemA2" (hierarchical ID = {1, 1})
   d.) "menuItemB1" (hierarchical ID = {1, 2, 0})
   e.) "menuItemB2" (hierarchical ID = {1, 2, 1})
 */
struct userMenuInfo {
	char *umiName;               // hierarchical name of menu item (w.o. language mode info)
	int * umiId;                 // hierarchical ID of menu item 
	int   umiIdLen;              // length of hierarchical ID 
	bool  umiIsDefault;          // menu item is default one ("@*") 
	int   umiNbrOfLanguageModes; // number of language modes applicable for this menu item
	int * umiLanguageMode;       // list of applicable lang. modes 
	int   umiDefaultIndex;       // array index of menu item to be used as default, if no lang. mode matches
	bool  umiToBeManaged;        // indicates, that menu item needs to be managed
};

struct MenuData {
	MenuItem     *item;
	userMenuInfo *info;
};

/* Structure holding hierarchical info about one sub-menu.

   Suppose following user menu items:
   a.) "menuItem1"
   b.) "subMenuA>menuItemA1"
   c.) "subMenuA>menuItemA2"
   d.) "subMenuA>subMenuB>menuItemB1"
   e.) "subMenuA>subMenuB>menuItemB2"

   Structure of this user menu is:

   Main Menu    Name       Sub-Menu A   Name       Sub-Menu B   Name
   element nbr.            element nbr.            element nbr.
        0       menuItem1
        1       subMenuA --+->    0     menuItemA1
                           +->    1     menuItemA2
                           +->    2     subMenuB --+->    0     menuItemB1
                                                   +->    1     menuItemB2

   Above example holds 2 sub-menus:
   1.) "subMenuA" (hierarchical ID = {1} means: element nbr. "1" of main menu)
   2.) "subMenuA>subMenuB" (hierarchical ID = {1, 2} means: el. nbr. "2" of
       "subMenuA", which itself is el. nbr. "0" of main menu) */
struct userSubMenuInfo {
	char *usmiName;  // hierarchical name of sub-menu 
	int * usmiId;    // hierarchical ID of sub-menu   
	int   usmiIdLen; // length of hierarchical ID     
};

// Holds info about sub-menu structure of an user menu 
struct userSubMenuCache {
	int              usmcNbrOfMainMenuItems; // number of main menu items 
	int              usmcNbrOfSubMenus;      // number of sub-menus 
	userSubMenuInfo *usmcInfo;               // list of sub-menu info 
};


bool DoNamedBGMenuCmd(Document *window, const char *itemName);
bool DoNamedMacroMenuCmd(Document *window, const char *itemName);
bool DoNamedShellMenuCmd(Document *window, const char *itemName, int fromMacro);
int LoadBGMenuCmdsString(const char *inString);
int LoadBGMenuCmdsStringEx(view::string_view inString);
int LoadMacroCmdsString(const char *inString);
int LoadMacroCmdsStringEx(view::string_view inString);
int LoadShellCmdsString(const char *inString);
int LoadShellCmdsStringEx(view::string_view inString);
QString WriteBGMenuCmdsStringEx();
QString WriteMacroCmdsStringEx();
QString WriteShellCmdsStringEx();
UserMenuCache *CreateUserMenuCache();
void DimPasteReplayBtns(int sensitive);
void DimSelectionDepUserMenuItems(Document *window, int sensitive);
void EditBGMenu(Document *window);
void EditMacroMenu(Document *window);
void EditShellMenu(Document *window);
void FreeUserBGMenuCache(UserBGMenuCache *cache);
void FreeUserMenuCache(UserMenuCache *cache);
void InitUserBGMenuCache(UserBGMenuCache *cache);
void RebuildAllMenus(Document *window);
void SetBGMenuRedoSensitivity(Document *window, int sensitive);
void SetBGMenuUndoSensitivity(Document *window, int sensitive);
void SetupUserMenuInfo();
void UpdateUserMenuInfo();
void UpdateUserMenus(Document *window);

void generateAcceleratorString(char *text, unsigned int modifiers, KeySym keysym);
int checkMacroText(const char *macro, Widget errorParent, Widget errFocus);
int parseAcceleratorString(const char *string, unsigned int *modifiers, KeySym *keysym);
void freeUserMenuInfoList(userMenuInfo **infoList, int nbrOfItems);
void freeUserMenuInfoList(MenuData *infoList, int nbrOfItems);
void parseMenuItemList(MenuItem **itemList, int nbrOfItems, userMenuInfo **infoList, userSubMenuCache *subMenus);
void parseMenuItemList(MenuData *itemList, int nbrOfItems, userSubMenuCache *subMenus);
void freeSubMenuCache(userSubMenuCache *subMenus);
void rebuildMenuOfAllWindows(int menuType);

/* max. number of user programmable menu commands allowed per each of the
   macro, shell, and background menus */
#define MAX_ITEMS_PER_MENU 400

extern int NShellMenuItems;
#if 0
extern MenuItem *ShellMenuItems[MAX_ITEMS_PER_MENU];
extern userMenuInfo *ShellMenuInfo[MAX_ITEMS_PER_MENU];
#else
extern MenuData ShellMenuData[MAX_ITEMS_PER_MENU];
#endif
extern userSubMenuCache ShellSubMenus;

extern int NBGMenuItems;
extern MenuItem *BGMenuItems[MAX_ITEMS_PER_MENU];
extern userMenuInfo *BGMenuInfo[MAX_ITEMS_PER_MENU];
extern userSubMenuCache BGSubMenus;

extern int NMacroMenuItems;
extern MenuItem *MacroMenuItems[MAX_ITEMS_PER_MENU];
extern userMenuInfo *MacroMenuInfo[MAX_ITEMS_PER_MENU];
extern userSubMenuCache MacroSubMenus;

#endif
