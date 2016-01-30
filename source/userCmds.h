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

#include "nedit.h"
#include "string_view.h"
#include "nullable_string.h"

int DoNamedBGMenuCmd(Document *window, const char *itemName);
int DoNamedMacroMenuCmd(Document *window, const char *itemName);
int DoNamedShellMenuCmd(Document *window, const char *itemName, int fromMacro);
int LoadBGMenuCmdsString(const char *inString);
int LoadBGMenuCmdsStringEx(view::string_view inString);
int LoadMacroCmdsString(const char *inString);
int LoadMacroCmdsStringEx(view::string_view inString);
int LoadShellCmdsString(const char *inString);
int LoadShellCmdsStringEx(view::string_view inString);
nullable_string WriteBGMenuCmdsStringEx(void);
nullable_string WriteMacroCmdsStringEx(void);
nullable_string WriteShellCmdsStringEx(void);
UserMenuCache *CreateUserMenuCache(void);
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
void SetupUserMenuInfo(void);
void UpdateUserMenuInfo(void);
void UpdateUserMenus(Document *window);

#endif
