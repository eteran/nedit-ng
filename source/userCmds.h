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
#include <QVector>
#include <memory>

class MenuItem;

// types of current dialog and/or menu 
enum DialogTypes {
	SHELL_CMDS,
	MACRO_CMDS,
	BG_MENU_CMDS
};

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
    QString      umiName;          // hierarchical name of menu item (w.o. language mode info)
    bool         umiIsDefault;     // menu item is default one ("@*")
    QVector<int> umiLanguageModes; // list of applicable lang. modes
    int          umiDefaultIndex;  // array index of menu item to be used as default, if no lang. mode matches
};

struct MenuData {
    std::shared_ptr<MenuItem>     item;
    std::shared_ptr<userMenuInfo> info;
};

int LoadBGMenuCmdsStringEx(const QString &inString);
int LoadMacroCmdsStringEx(const QString &inString);
int LoadShellCmdsStringEx(const QString &inString);
QString WriteBGMenuCmdsStringEx();
QString WriteMacroCmdsStringEx();
QString WriteShellCmdsStringEx();
void SetupUserMenuInfo();
void UpdateUserMenuInfo();
void parseMenuItemList(QVector<MenuData> &itemList);

extern QVector<MenuData> ShellMenuData;
extern QVector<MenuData> BGMenuData;
extern QVector<MenuData> MacroMenuData;


#endif
