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

#include <vector>

struct MenuItem;
struct MenuData;
class QString;

// types of current dialog and/or menu
enum class DialogTypes {
    SHELL_CMDS,
    MACRO_CMDS,
    BG_MENU_CMDS
};

int LoadBGMenuCmdsStringEx(const QString &inString);
int LoadMacroCmdsStringEx(const QString &inString);
int LoadShellCmdsStringEx(const QString &inString);
QString WriteBGMenuCmdsStringEx();
QString WriteMacroCmdsStringEx();
QString WriteShellCmdsStringEx();
void SetupUserMenuInfo();
void UpdateUserMenuInfo();
void parseMenuItemList(std::vector<MenuData> &itemList);
MenuData *findMenuItem(const QString &name, DialogTypes type);

extern std::vector<MenuData> ShellMenuData;
extern std::vector<MenuData> BGMenuData;
extern std::vector<MenuData> MacroMenuData;

#endif
