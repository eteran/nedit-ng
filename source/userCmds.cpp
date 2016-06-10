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

#include "ui/DialogWindowBackgroundMenu.h"
#include "ui/DialogMacros.h"
#include "ui/DialogShellMenu.h"
#include "userCmds.h"
#include "Document.h"
#include "menu.h"
#include "MenuItem.h"
#include "file.h"
#include "shell.h"
#include "macro.h"
#include "preferences.h"
#include "TextBuffer.h"
#include "parse.h"
#include "misc.h"
#include "MotifHelper.h"

#include <X11/IntrinsicP.h>
#include <Xm/PushB.h>
#include <Xm/RowColumn.h>
#include <Xm/CascadeB.h>
#include <Xm/MenuShell.h>

extern "C" void _XmDismissTearOff(Widget, XtPointer, XtPointer);

namespace {

Widget MENU_WIDGET(Widget w) {
	return (XmGetPostedFromWidget(XtParent(w)));
}

DialogWindowBackgroundMenu *WindowBackgroundMenu = nullptr;
DialogMacros               *WindowMacros         = nullptr;
DialogShellMenu            *WindowShellMenu      = nullptr;

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

/* Structure holding info about a selected user menu (shell, macro or
   background) */
struct selectedUserMenu {
	int sumType;                   // type of menu (shell, macro or background
	Widget sumMenuPane;            // pane of main menu 
	QVector<MenuData> sumDataList;
	userSubMenuCache *sumSubMenus; // info about sub-menu structure 
	UserMenuList *sumMainMenuList; // cached info about main menu 
	bool *sumMenuCreated;          // pointer to "menu created" indicator
};

/* Descriptions of the current user programmed menu items for re-generating
   menus and processing shell, macro, and background menu selections */
QVector<MenuData>  ShellMenuData;
userSubMenuCache ShellSubMenus;

QVector<MenuData> MacroMenuData;
userSubMenuCache MacroSubMenus;

QVector<MenuData> BGMenuData;
userSubMenuCache BGSubMenus;


static void rebuildMenu(Document *window, int menuType);
static Widget findInMenuTreeEx(menuTreeItem *menuTree, int nTreeEntries, const QString &hierName);
static char *copySubstring(const char *string, int length);
static QString copySubstringEx(const char *string, int length);
static Widget createUserMenuItem(Widget menuPane, char *name, MenuItem *f, int index, XtCallbackProc cbRtn, XtPointer cbArg);
static Widget createUserSubMenuEx(Widget parent, const QString &label, Widget *menuItem);
static void deleteMenuItems(Widget menuPane);
static void selectUserMenu(Document *window, int menuType, selectedUserMenu *menu);
static void updateMenu(Document *window, int menuType);
static void manageTearOffMenu(Widget menuPane);
static void resetManageMode(UserMenuList *list);
static void manageAllSubMenuWidgets(UserMenuListElement *subMenu);
static void unmanageAllSubMenuWidgets(UserMenuListElement *subMenu);
static void manageMenuWidgets(UserMenuList *list);
static void removeAccelFromMenuWidgets(UserMenuList *menuList);
static void assignAccelToMenuWidgets(UserMenuList *menuList, Document *window);
static void manageUserMenu(selectedUserMenu *menu, Document *window);
static void createMenuItems(Document *window, selectedUserMenu *menu);
static void shellMenuCB(Widget w, XtPointer clientData, XtPointer callData);
static void macroMenuCB(Widget w, XtPointer clientData, XtPointer callData);
static void bgMenuCB(Widget w, XtPointer clientData, XtPointer callData);
static void genAccelEventName(char *text, unsigned int modifiers, KeySym keysym);
static int parseError(const char *message);
static char *copyMacroToEnd(const char **inPtr);
static int getSubMenuDepth(const char *menuName);
static userMenuInfo *parseMenuItemRec(MenuItem *item);
static void parseMenuItemName(char *menuItemName, userMenuInfo *info);
static void generateUserMenuId(userMenuInfo *info, userSubMenuCache *subMenus);
static userSubMenuInfo *findSubMenuInfo(userSubMenuCache *subMenus, const char *hierName);
static userSubMenuInfo *findSubMenuInfoEx(userSubMenuCache *subMenus, const QString &hierName);
static char *stripLanguageModeEx(const QString &menuItemName);
static bool doesLanguageModeMatch(userMenuInfo *info, int languageMode);
static void freeUserMenuInfo(userMenuInfo *info);
static void allocSubMenuCache(userSubMenuCache *subMenus, int nbrOfItems);

static void applyLangModeToUserMenuInfo(const QVector<MenuData> &infoList, int languageMode);
static void setDefaultIndex(const QVector<MenuData> &infoList, int defaultIdx);
static int loadMenuItemString(const char *inString, QVector<MenuData> &menuItems, int listType);
static QString writeMenuItemStringEx(const QVector<MenuData> &menuItems, int listType);
static char *writeMenuItemString(const QVector<MenuData> &menuItems, int listType);
static void dimSelDepItemsInMenu(Widget menuPane, const QVector<MenuData> &menuList, int sensitive);

/*
** Present a dialog for editing the user specified commands in the shell menu
*/
void EditShellMenu(Document *window) {
	Q_UNUSED(window);

	if(!WindowShellMenu) {
		WindowShellMenu = new DialogShellMenu();
	}
	
	WindowShellMenu->show();
	WindowShellMenu->raise();
}

/*
** Present a dialogs for editing the user specified commands in the Macro
** and background menus
*/
void EditMacroMenu(Document *window) {
	(void)window;
	
	if(!WindowMacros) {
		WindowMacros = new DialogMacros();
	}

	WindowMacros->show();
	WindowMacros->raise();
}



void EditBGMenu(Document *window) {
	(void)window;

	if(!WindowBackgroundMenu) {
		WindowBackgroundMenu = new DialogWindowBackgroundMenu();
	}
	
	WindowBackgroundMenu->show();
	WindowBackgroundMenu->raise();
}

/*
** Update the Shell, Macro, and Window Background menus of window
** "window" from the currently loaded command descriptions.
*/
void UpdateUserMenus(Document *window) {
	if (!window->IsTopDocument())
		return;

	/* update user menus, which are shared over all documents, only
	   if language mode was changed */
	if (window->userMenuCache_->umcLanguageMode != window->languageMode_) {
		updateMenu(window, SHELL_CMDS);
		updateMenu(window, MACRO_CMDS);

		// remember language mode assigned to shared user menus 
		window->userMenuCache_->umcLanguageMode = window->languageMode_;
	}

	/* update background menu, which is owned by a single document, only
	   if language mode was changed */
	if (window->userBGMenuCache_.ubmcLanguageMode != window->languageMode_) {
		updateMenu(window, BG_MENU_CMDS);

		// remember language mode assigned to background menu 
		window->userBGMenuCache_.ubmcLanguageMode = window->languageMode_;
	}
}

/*
** Dim/undim buttons for pasting replay macros into macro and bg menu dialogs
*/
void DimPasteReplayBtns(int sensitive) {

	if(WindowMacros) {
		WindowMacros->setPasteReplayEnabled(sensitive);
	}

	if(WindowBackgroundMenu) {
		WindowBackgroundMenu->setPasteReplayEnabled(sensitive);
	}
}

/*
** Dim/undim user programmable menu items which depend on there being
** a selection in their associated window.
*/
void DimSelectionDepUserMenuItems(Document *window, int sensitive) {
	if (!window->IsTopDocument())
		return;

	dimSelDepItemsInMenu(window->shellMenuPane_, ShellMenuData, sensitive);
	dimSelDepItemsInMenu(window->macroMenuPane_, MacroMenuData, sensitive);
	dimSelDepItemsInMenu(window->bgMenuPane_,    BGMenuData,    sensitive);
}

static void dimSelDepItemsInMenu(Widget menuPane, const QVector<MenuData> &menuList, int sensitive) {
	WidgetList items;
	Widget subMenu;
	XtPointer userData;
	int n, index;
	Cardinal nItems;

	XtVaGetValues(menuPane, XmNchildren, &items, XmNnumChildren, &nItems, nullptr);
	for (n = 0; n < (int)nItems; n++) {
		XtVaGetValues(items[n], XmNuserData, &userData, nullptr);
		if (userData != (XtPointer)PERMANENT_MENU_ITEM) {
			if (XtClass(items[n]) == xmCascadeButtonWidgetClass) {
				XtVaGetValues(items[n], XmNsubMenuId, &subMenu, nullptr);
				dimSelDepItemsInMenu(subMenu, menuList, sensitive);
			} else {
				index = (long)userData - 10;
				if (index < 0 || index >= menuList.size())
					return;
				if (menuList[index].item->input == FROM_SELECTION)
					XtSetSensitive(items[n], sensitive);
			}
		}
	}
}

/*
** Harmless kludge for making undo/redo menu items in background menu properly
** sensitive (even though they're programmable) along with real undo item
** in the Edit menu
*/
void SetBGMenuUndoSensitivity(Document *window, int sensitive) {
	if (window->bgMenuUndoItem_)
		window->SetSensitive(window->bgMenuUndoItem_, sensitive);
}
void SetBGMenuRedoSensitivity(Document *window, int sensitive) {
	if (window->bgMenuRedoItem_)
		window->SetSensitive(window->bgMenuRedoItem_, sensitive);
}

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
int LoadShellCmdsString(const char *inString) {
	return loadMenuItemString(inString, ShellMenuData, SHELL_CMDS);
}

/*
** Read a string representing shell command menu items and add them to the
** internal list used for constructing shell menus
*/
int LoadShellCmdsStringEx(view::string_view inString) {
	// TODO(eteran): make this more efficient
	return loadMenuItemString(inString.to_string().c_str(), ShellMenuData, SHELL_CMDS);
}

/*
** Read strings representing macro menu or background menu command menu items
** and add them to the internal lists used for constructing menus
*/
int LoadMacroCmdsString(const char *inString) {
	return loadMenuItemString(inString, MacroMenuData, MACRO_CMDS);
}

int LoadMacroCmdsStringEx(view::string_view inString) {
	// TODO(eteran): make this more efficient
	return loadMenuItemString(inString.to_string().c_str(), MacroMenuData, MACRO_CMDS);
}

int LoadBGMenuCmdsString(const char *inString) {
	return loadMenuItemString(inString, BGMenuData, BG_MENU_CMDS);
}

int LoadBGMenuCmdsStringEx(view::string_view inString) {
	// TODO(eteran): make this more efficient
	return loadMenuItemString(inString.to_string().c_str(), BGMenuData, BG_MENU_CMDS);
}

/*
** Cache user menus:
** Setup user menu info after read of macro, shell and background menu
** string (reason: language mode info from preference string is read *after*
** user menu preference string was read).
*/
void SetupUserMenuInfo(void) {
	parseMenuItemList(ShellMenuData, &ShellSubMenus);
	parseMenuItemList(MacroMenuData, &MacroSubMenus);
	parseMenuItemList(BGMenuData,    &BGSubMenus);
}

/*
** Cache user menus:
** Update user menu info to take into account e.g. change of language modes
** (i.e. add / move / delete of language modes etc).
*/
void UpdateUserMenuInfo(void) {
	freeUserMenuInfoList(ShellMenuData);
	freeSubMenuCache(&ShellSubMenus);
	parseMenuItemList(ShellMenuData, &ShellSubMenus);

	freeUserMenuInfoList(MacroMenuData);
	freeSubMenuCache(&MacroSubMenus);
	parseMenuItemList(MacroMenuData, &MacroSubMenus);

	freeUserMenuInfoList(BGMenuData);
	freeSubMenuCache(&BGSubMenus);
	parseMenuItemList(BGMenuData, &BGSubMenus);
}

/*
** Search through the shell menu and execute the first command with menu item
** name "itemName".  Returns True on successs and False on failure.
*/
bool DoNamedShellMenuCmd(Document *window, const char *itemName, int fromMacro) {

	for(MenuData &data: ShellMenuData) {
		if (data.item->name == QLatin1String(itemName)) {
			if (data.item->output == TO_SAME_WINDOW && CheckReadOnly(window)) {
				return false;
			}
			
			DoShellMenuCmd(
				window, 
				data.item->cmd.toStdString(), 
				data.item->input, 
				data.item->output, 
				data.item->repInput, 
				data.item->saveFirst, 
				data.item->loadAfter, 
				fromMacro);
				
			return true;
		}
	}
	return false;
}

/*
** Search through the Macro or background menu and execute the first command
** with menu item name "itemName".  Returns True on successs and False on
** failure.
*/
bool DoNamedMacroMenuCmd(Document *window, const char *itemName) {

	for(MenuData &data: MacroMenuData) {
		if (data.item->name == QLatin1String(itemName)) {
		
			DoMacro(
				window, 
				data.item->cmd.toStdString(), 
				"macro menu command");

			return true;
		}
	}
	return false;
}

bool DoNamedBGMenuCmd(Document *window, const char *itemName) {

	for(MenuData &data: BGMenuData) {
		if (data.item->name == QLatin1String(itemName)) {		

			DoMacro(
				window, 
				data.item->cmd.toStdString(), 
				"background menu macro");

			return true;
		}
	}
	return false;
}

/*
** Cache user menus:
** Rebuild all of the Shell, Macro, Background menus of given editor window.
*/
void RebuildAllMenus(Document *window) {
	rebuildMenu(window, SHELL_CMDS);
	rebuildMenu(window, MACRO_CMDS);
	rebuildMenu(window, BG_MENU_CMDS);
}

/*
** Cache user menus:
** Rebuild either Shell, Macro or Background menus of all editor windows.
*/
void rebuildMenuOfAllWindows(int menuType) {

	for(Document *w: WindowList) {
		rebuildMenu(w, menuType);
	}
}

/*
** Rebuild either the Shell, Macro or Background menu of "window", depending
** on value of "menuType". Rebuild is realized by following main steps:
** - dismiss user (sub) menu tearoff.
** - delete all user defined menu widgets.
** - update user menu including (re)creation of menu widgets.
*/
static void rebuildMenu(Document *window, int menuType) {
	selectedUserMenu menu;

	/* Background menu is always rebuild (exists once per document).
	   Shell, macro (user) menu cache is rebuild only, if given window is
	   currently displayed on top. */
	if (menuType != BG_MENU_CMDS && !window->IsTopDocument())
		return;

	// Fetch the appropriate menu data 
	selectUserMenu(window, menuType, &menu);

	/* dismiss user menu tearoff, to workaround the quick
	   but noticeable shrink-expand bug, most probably
	   triggered by the rebuild of the user menus. In any
	   case, the submenu tearoffs will later be dismissed
	   too in order to prevent dangling tearoffs, so doing
	   this also for the main user menu tearoffs shouldn't
	   be so bad */
	if (!XmIsMenuShell(XtParent(menu.sumMenuPane)))
		_XmDismissTearOff(XtParent(menu.sumMenuPane), nullptr, nullptr);

	// destroy all widgets related to menu pane 
	deleteMenuItems(menu.sumMenuPane);

	// remove cached user menu info 
	freeUserMenuList(menu.sumMainMenuList);
	*menu.sumMenuCreated = false;

	// re-create & cache user menu items 
	updateMenu(window, menuType);
}

/*
** Fetch the appropriate menu info for given menu type
*/
static void selectUserMenu(Document *window, int menuType, selectedUserMenu *menu) {
	if (menuType == SHELL_CMDS) {
		menu->sumMenuPane       = window->shellMenuPane_;
		menu->sumDataList      = ShellMenuData;
		menu->sumSubMenus       = &ShellSubMenus;
		menu->sumMainMenuList   = &window->userMenuCache_->umcShellMenuList;
		menu->sumMenuCreated    = &window->userMenuCache_->umcShellMenuCreated;
	} else if (menuType == MACRO_CMDS) {
		menu->sumMenuPane       = window->macroMenuPane_;
		menu->sumDataList      = MacroMenuData;
		menu->sumSubMenus       = &MacroSubMenus;
		menu->sumMainMenuList   = &window->userMenuCache_->umcMacroMenuList;
		menu->sumMenuCreated    = &window->userMenuCache_->umcMacroMenuCreated;
	} else { // BG_MENU_CMDS 
		menu->sumMenuPane       = window->bgMenuPane_;
		menu->sumDataList      = BGMenuData;
		menu->sumSubMenus       = &BGSubMenus;
		menu->sumMainMenuList   = &window->userBGMenuCache_.ubmcMenuList;
		menu->sumMenuCreated    = &window->userBGMenuCache_.ubmcMenuCreated;
	}
	menu->sumType = menuType;
}

/*
** Updates either the Shell, Macro or Background menu of "window", depending
** on value of "menuType". Update is realized by following main steps:
** - set / reset "to be managed" flag of user menu info list items
**   according to current selected language mode.
** - create *all* user menu items (widgets etc). related to given
**   window & menu type, if not done before.
** - manage / unmanage user menu widgets according to "to be managed"
**   indication of user menu info list items.
*/
static void updateMenu(Document *window, int menuType) {
	selectedUserMenu menu;

	// Fetch the appropriate menu data 
	selectUserMenu(window, menuType, &menu);

	// Set / reset "to be managed" flag of all info list items 	
	applyLangModeToUserMenuInfo(menu.sumDataList, window->languageMode_);

	// create user menu items, if not done before 
	if (!*menu.sumMenuCreated)
		createMenuItems(window, &menu);

	// manage user menu items depending on current language mode 
	manageUserMenu(&menu, window);

	if (menuType == BG_MENU_CMDS) {
		// Set the proper sensitivity of items which may be dimmed 
		SetBGMenuUndoSensitivity(window, XtIsSensitive(window->undoItem_));
		SetBGMenuRedoSensitivity(window, XtIsSensitive(window->redoItem_));
	}

	DimSelectionDepUserMenuItems(window, window->buffer_->primary_.selected);
}

/*
** Manually adjust the dimension of the menuShell _before_
** re-managing the menu pane, to either expose hidden menu
** entries or remove empty space.
*/
static void manageTearOffMenu(Widget menuPane) {
	Dimension width, height, border;

	/* somehow OM went into a long CPU cycling when we
	   attempt to change the shell window dimension by
	   setting the XmNwidth & XmNheight directly. Using
	   XtResizeWidget() seem to fix it */
	XtVaGetValues(XtParent(menuPane), XmNborderWidth, &border, nullptr);
	XtVaGetValues(menuPane, XmNwidth, &width, XmNheight, &height, nullptr);
	XtResizeWidget(XtParent(menuPane), width, height, border);

	XtManageChild(menuPane);
}

/*
** Cache user menus:
** Reset manage mode of user menu items in window cache.
*/
static void resetManageMode(UserMenuList *list) {

	for (auto &&element : *list) {

		/* remember current manage mode before reset it to
		   "unmanaged" */
		element->umlePrevManageMode = element->umleManageMode;
		element->umleManageMode = UMMM_UNMANAGE;

		// recursively reset manage mode of sub-menus 
		if (element->umleSubMenuList)
			resetManageMode(element->umleSubMenuList);
	}
}

/*
** Cache user menus:
** Manage all menu widgets of given user sub-menu list.
*/
static void manageAllSubMenuWidgets(UserMenuListElement *subMenu) {

	UserMenuList *subMenuList;
	
	WidgetList widgetList;
	Cardinal nWidgetListItems;

	/* if the sub-menu is torn off, unmanage the menu pane
	   before updating it to prevent the tear-off menu
	   from shrinking and expanding as the menu entries
	   are (un)managed */
	if (!XmIsMenuShell(XtParent(subMenu->umleSubMenuPane))) {
		XtUnmanageChild(subMenu->umleSubMenuPane);
	}

	// manage all children of sub-menu pane 
	XtVaGetValues(subMenu->umleSubMenuPane, XmNchildren, &widgetList, XmNnumChildren, &nWidgetListItems, nullptr);
	XtManageChildren(widgetList, nWidgetListItems);

	/* scan, if an menu item of given sub-menu holds a nested
	   sub-menu */
	subMenuList = subMenu->umleSubMenuList;

	for(UserMenuListElement *element : *subMenuList) {

		if (element->umleSubMenuList) {
			/* if element is a sub-menu, then continue managing
			   all items of that sub-menu recursively */
			manageAllSubMenuWidgets(element);
		}
	}

	// manage sub-menu pane widget itself 
	XtManageChild(subMenu->umleMenuItem);

	// if the sub-menu is torn off, then adjust & manage the menu 
	if (!XmIsMenuShell(XtParent(subMenu->umleSubMenuPane))) {
		manageTearOffMenu(subMenu->umleSubMenuPane);
	}

	/* redisplay sub-menu tearoff window, if the sub-menu
	   was torn off before */
	ShowHiddenTearOff(subMenu->umleSubMenuPane);
}

/*
** Cache user menus:
** Unmanage all menu widgets of given user sub-menu list.
*/
static void unmanageAllSubMenuWidgets(UserMenuListElement *subMenu) {

	UserMenuList *subMenuList;
	WidgetList widgetList;
	Cardinal nWidgetListItems;

	/* if sub-menu is torn-off, then unmap its shell
	   (so tearoff window isn't displayed anymore) */
	Widget shell = XtParent(subMenu->umleSubMenuPane);
	if (!XmIsMenuShell(shell)) {
		XtUnmapWidget(shell);
	}

	// unmanage all children of sub-menu pane 
	XtVaGetValues(subMenu->umleSubMenuPane, XmNchildren, &widgetList, XmNnumChildren, &nWidgetListItems, nullptr);
	XtUnmanageChildren(widgetList, nWidgetListItems);

	/* scan, if an menu item of given sub-menu holds a nested
	   sub-menu */
	subMenuList = subMenu->umleSubMenuList;

	for (UserMenuListElement *element: *subMenuList) {

		if (element->umleSubMenuList) {
			/* if element is a sub-menu, then continue unmanaging
			   all items of that sub-menu recursively */
			unmanageAllSubMenuWidgets(element);
		}
	}

	// unmanage sub-menu pane widget itself 
	XtUnmanageChild(subMenu->umleMenuItem);
}

/*
** Cache user menus:
** Manage / unmanage menu widgets according to given user menu list.
*/
static void manageMenuWidgets(UserMenuList *list) {

	// (un)manage all elements of given user menu list 
	for (auto &&element : *list) {

		if (element->umlePrevManageMode != element->umleManageMode || element->umleManageMode == UMMM_MANAGE) {
			/* previous and current manage mode differ OR
			   current manage mode indicates: element needs to be
			   (un)managed individually */
			if (element->umleManageMode == UMMM_MANAGE_ALL) {
				/* menu item represented by "element" is a sub-menu and
				   needs to be completely managed */
				manageAllSubMenuWidgets(element);
			} else if (element->umleManageMode == UMMM_MANAGE) {
				if (element->umlePrevManageMode == UMMM_UNMANAGE || element->umlePrevManageMode == UMMM_UNMANAGE_ALL) {
					/* menu item represented by "element" was unmanaged
					   before and needs to be managed now */
					XtManageChild(element->umleMenuItem);
				}

				/* if element is a sub-menu, then continue (un)managing
				   single elements of that sub-menu one by one */
				if (element->umleSubMenuList) {
					/* if the sub-menu is torn off, unmanage the menu pane
					   before updating it to prevent the tear-off menu
					   from shrinking and expanding as the menu entries
					   are (un)managed */
					if (!XmIsMenuShell(XtParent(element->umleSubMenuPane))) {
						XtUnmanageChild(element->umleSubMenuPane);
					}

					// (un)manage menu entries of sub-menu 
					manageMenuWidgets(element->umleSubMenuList);

					// if the sub-menu is torn off, then adjust & manage the menu 
					if (!XmIsMenuShell(XtParent(element->umleSubMenuPane))) {
						manageTearOffMenu(element->umleSubMenuPane);
					}

					// if the sub-menu was torn off then redisplay it 
					ShowHiddenTearOff(element->umleSubMenuPane);
				}
			} else if (element->umleManageMode == UMMM_UNMANAGE_ALL) {
				/* menu item represented by "element" is a sub-menu and
				   needs to be completely unmanaged */
				unmanageAllSubMenuWidgets(element);
			} else {
				/* current mode is UMMM_UNMANAGE -> menu item represented
				   by "element" is a single menu item and needs to be
				   unmanaged */
				XtUnmanageChild(element->umleMenuItem);
			}
		}
	}
}

/*
** Cache user menus:
** Remove accelerators from all items of given user (sub-)menu list.
*/
static void removeAccelFromMenuWidgets(UserMenuList *menuList) {

	// scan all elements of this (sub-)menu 
	for(UserMenuListElement *element : *menuList) {

		if (element->umleSubMenuList) {
			/* if element is a sub-menu, then continue removing accelerators
			   from all items of that sub-menu recursively */
			removeAccelFromMenuWidgets(element->umleSubMenuList);
		} else if (!element->umleAccKeys.isNull() && element->umleManageMode == UMMM_UNMANAGE && element->umlePrevManageMode == UMMM_MANAGE) {
			// remove accelerator if one was bound 
			XtVaSetValues(element->umleMenuItem, XmNaccelerator, nullptr, nullptr);
		}
	}
}

/*
** Cache user menus:
** Assign accelerators to all managed items of given user (sub-)menu list.
*/
static void assignAccelToMenuWidgets(UserMenuList *menuList, Document *window) {

	// scan all elements of this (sub-)menu 
	for(UserMenuListElement *element : *menuList) {

		if (element->umleSubMenuList) {
			/* if element is a sub-menu, then continue assigning accelerators
			   to all managed items of that sub-menu recursively */
			assignAccelToMenuWidgets(element->umleSubMenuList, window);
		} else if (!element->umleAccKeys.isNull() && element->umleManageMode == UMMM_MANAGE && element->umlePrevManageMode == UMMM_UNMANAGE) {
			// assign accelerator if applicable 
			XtVaSetValues(element->umleMenuItem, XmNaccelerator, element->umleAccKeys.toLatin1().data(), nullptr);
			if (!element->umleAccLockPatchApplied) {
				UpdateAccelLockPatch(window->splitPane_, element->umleMenuItem);
				element->umleAccLockPatchApplied = true;
			}
		}
	}
}

/*
** Cache user menus:
** (Un)Manage all items of selected user menu.
*/
static void manageUserMenu(selectedUserMenu *menu, Document *window) {

	bool currentLEisSubMenu;
	UserMenuList *menuList;
	UserMenuListElement *currentLE;
	UserMenuManageMode *mode;

	// reset manage mode of all items of selected user menu in window cache 
	resetManageMode(menu->sumMainMenuList);

	/* set manage mode of all items of selected user menu in window cache
	   according to the "to be managed" indication of the info list */
	for(MenuData &data: menu->sumDataList) {	
		userMenuInfo *info = data.info;				
		
		menuList = menu->sumMainMenuList;
		int *id = info->umiId;

		/* select all menu list items belonging to menu record "info" using
		   hierarchical ID of current menu info (e.g. id = {3} means:
		   4th element of main menu; {0} = 1st element etc.)*/
		for (int i = 0; i < info->umiIdLen; i++) {
			currentLE = menuList->at(*id);
			mode = &currentLE->umleManageMode;
			currentLEisSubMenu = (currentLE->umleSubMenuList != nullptr);

			if (info->umiToBeManaged) {
				// menu record needs to be managed: 
				if (*mode == UMMM_UNMANAGE) {
					/* "mode" was not touched after reset ("init. state"):
					   if current list element represents a sub-menu, then
					   probably the complete sub-menu needs to be managed
					   too. If current list element indicates single menu
					   item, then just this item needs to be managed */
					if (currentLEisSubMenu) {
						*mode = UMMM_MANAGE_ALL;
					} else {
						*mode = UMMM_MANAGE;
					}
				} else if (*mode == UMMM_UNMANAGE_ALL) {
					/* "mode" was touched after reset:
					   current list element represents a sub-menu and min.
					   one element of the sub-menu needs to be unmanaged ->
					   the sub-menu needs to be (un)managed element by
					   element */
					*mode = UMMM_MANAGE;
				}
			} else {
				// menu record needs to be unmanaged: 
				if (*mode == UMMM_UNMANAGE) {
					/* "mode" was not touched after reset ("init. state"):
					   if current list element represents a sub-menu, then
					   probably the complete sub-menu needs to be unmanaged
					   too. */
					if (currentLEisSubMenu) {
						*mode = UMMM_UNMANAGE_ALL;
					}
				} else if (*mode == UMMM_MANAGE_ALL) {
					/* "mode" was touched after reset:
					   current list element represents a sub-menu and min.
					   one element of the sub-menu needs to be managed ->
					   the sub-menu needs to be (un)managed element by
					   element */
					*mode = UMMM_MANAGE;
				}
			}

			menuList = currentLE->umleSubMenuList;

			id++;
		}
	}

	/* if the menu is torn off, unmanage the menu pane
	   before updating it to prevent the tear-off menu
	   from shrinking and expanding as the menu entries
	   are managed */
	if (!XmIsMenuShell(XtParent(menu->sumMenuPane)))
		XtUnmanageChild(menu->sumMenuPane);

	/* manage menu widgets according to current / previous manage mode of
	   user menu window cache */
	manageMenuWidgets(menu->sumMainMenuList);

	/* Note: before new accelerator is assigned it seems to be necessary
	   to remove old accelerator from user menu widgets. Removing same
	   accelerator *after* it was assigned to another user menu widget
	   doesn't work */
	removeAccelFromMenuWidgets(menu->sumMainMenuList);

	assignAccelToMenuWidgets(menu->sumMainMenuList, window);

	// if the menu is torn off, then adjust & manage the menu 
	if (!XmIsMenuShell(XtParent(menu->sumMenuPane)))
		manageTearOffMenu(menu->sumMenuPane);
}

/*
** Create either the variable Shell menu, Macro menu or Background menu
** items of "window" (driven by value of "menuType")
*/
static void createMenuItems(Document *window, selectedUserMenu *menu) {
	Widget btn, subPane, newSubPane;
	int n;
	MenuItem *item;
	char *namePtr;
	char *subSep;
	char *fullName;
	int menuType = menu->sumType;
	userMenuInfo *info;
	userSubMenuCache *subMenus = menu->sumSubMenus;
	UserMenuList *menuList;
	UserMenuListElement *currentLE;
	int subMenuDepth;
	char accKeysBuf[MAX_ACCEL_LEN + 5];

	// Allocate storage for structures to help find panes of sub-menus 
	
	const int sumNbrOfListItems = menu->sumDataList.size();
	
	auto menuTree = new menuTreeItem[sumNbrOfListItems];
	int nTreeEntries = 0;

	/* Harmless kludge: undo and redo items are marked specially if found
	   in the background menu, and used to dim/undim with edit menu */
	window->bgMenuUndoItem_ = nullptr;
	window->bgMenuRedoItem_ = nullptr;

	/*
	** Add items to the menu pane, creating hierarchical sub-menus as
	** necessary
	*/
	menu->sumMainMenuList->clear();
	
	for (n = 0; n < sumNbrOfListItems; n++) {
	
		item = menu->sumDataList[n].item;
		info = menu->sumDataList[n].info;			
	
		menuList = menu->sumMainMenuList;
		subMenuDepth = 0;

		fullName = info->umiName;

		/* create/find sub-menus, stripping off '>' until item name is
		   reached, then create the menu item */
		namePtr = fullName;
		subPane = menu->sumMenuPane;
		for (;;) {
			subSep = strchr(namePtr, '>');
			if(!subSep) {
				
				switch(menuType) {
				case SHELL_CMDS:
					btn = createUserMenuItem(subPane, namePtr, item, n, shellMenuCB, (XtPointer)window);
					break;
				case MACRO_CMDS:
					btn = createUserMenuItem(subPane, namePtr, item, n, macroMenuCB, (XtPointer)window);
					break;
				case BG_MENU_CMDS:
				default:
					btn = createUserMenuItem(subPane, namePtr, item, n, bgMenuCB, (XtPointer)window);
					break;
				}

				if (menuType == BG_MENU_CMDS) {
					if (item->cmd == QLatin1String("undo()\n")) {
						window->bgMenuUndoItem_ = btn;
					} else if (item->cmd == QLatin1String("redo()\n")) {
						window->bgMenuRedoItem_ = btn;
					}
				}
				
				// generate accelerator keys 
				genAccelEventName(accKeysBuf, item->modifiers, item->keysym);
				QString accKeys = item->keysym == NoSymbol ? QString() : QLatin1String(accKeysBuf);
				
				// create corresponding menu list item 
				menuList->push_back(new UserMenuListElement(btn, accKeys));
				break;
			}
			
			auto hierName = copySubstringEx(fullName, subSep - fullName);
			userSubMenuInfo *subMenuInfo = findSubMenuInfoEx(subMenus, hierName);
			newSubPane = findInMenuTreeEx(menuTree, nTreeEntries, hierName);
			if(!newSubPane) {

				auto subMenuName = copySubstringEx(namePtr, subSep - namePtr);
				newSubPane = createUserSubMenuEx(subPane, subMenuName, &btn);

				menuTree[nTreeEntries].name       = hierName;
				menuTree[nTreeEntries++].menuPane = newSubPane;

				currentLE = new UserMenuListElement(btn, QString());
				menuList->push_back(currentLE);
				currentLE->umleSubMenuPane = newSubPane;
				currentLE->umleSubMenuList = new UserMenuList;
			} else {
				currentLE = menuList->at(subMenuInfo->usmiId[subMenuDepth]);
			}
			
			subPane = newSubPane;
			menuList = currentLE->umleSubMenuList;
			subMenuDepth++;
			namePtr = subSep + 1;
		}
	}

	*menu->sumMenuCreated = True;

	delete [] menuTree;
}

/*
** Find the widget corresponding to a hierarchical menu name (a>b>c...)
*/
static Widget findInMenuTreeEx(menuTreeItem *menuTree, int nTreeEntries, const QString &hierName) {

	for (int i = 0; i < nTreeEntries; i++) {
		if (hierName == menuTree[i].name) {
			return menuTree[i].menuPane;
		}
	}

	return nullptr;
}

static char *copySubstring(const char *string, int length) {
	char *retStr = XtMalloc(length + 1);

	strncpy(retStr, string, length);
	retStr[length] = '\0';
	return retStr;
}

static QString copySubstringEx(const char *string, int length) {
	return QString::fromLatin1(string, length);
}

static Widget createUserMenuItem(Widget menuPane, char *name, MenuItem *f, int index, XtCallbackProc cbRtn, XtPointer cbArg) {
	XmString st1, st2;
	char accText[MAX_ACCEL_LEN];
	Widget btn;

	generateAcceleratorString(accText, f->modifiers, f->keysym);
	st1 = XmStringCreateSimpleEx(name);
	st2 = XmStringCreateSimpleEx(accText);
	btn = XtVaCreateWidget("cmd", xmPushButtonWidgetClass, menuPane, XmNlabelString, st1, XmNacceleratorText, st2, XmNmnemonic, f->mnemonic, XmNuserData, (XtPointer)((long)index + 10), nullptr);
	XtAddCallback(btn, XmNactivateCallback, cbRtn, cbArg);
	XmStringFree(st1);
	XmStringFree(st2);
	return btn;
}

/*
** Add a user-defined sub-menu to an established pull-down menu, marking
** it's userData field with TEMPORARY_MENU_ITEM so it can be found and
** removed later if the menu is redefined.  Returns the menu pane of the
** new sub-menu.
*/
static Widget createUserSubMenuEx(Widget parent, const QString &label, Widget *menuItem) {
	Widget menuPane;	
	static Arg args[1] = {{XmNuserData, (XtArgVal)TEMPORARY_MENU_ITEM}};

	menuPane = CreatePulldownMenu(parent, (String) "userPulldown", args, 1);
	
	XmString st1 = XmStringCreateSimpleEx(label);
	*menuItem = XtVaCreateWidget("userCascade", xmCascadeButtonWidgetClass, parent, XmNlabelString, st1, XmNsubMenuId, menuPane, XmNuserData, TEMPORARY_MENU_ITEM, nullptr);
	XmStringFree(st1);
	return menuPane;
}

/*
** Cache user menus:
** Delete all variable menu items of given menu pane
*/
static void deleteMenuItems(Widget menuPane) {
	WidgetList itemList;
	Cardinal nItems;
	Widget subMenuID;
	XtPointer userData;

	// Fetch the list of children from the menu pane to delete 
	XtVaGetValues(menuPane, XmNchildren, &itemList, XmNnumChildren, &nItems, nullptr);

	// make a copy because the widget alters the list as you delete widgets 
	auto items = new Widget[nItems];
	std::copy_n(itemList, nItems, items);

	// delete all of the widgets not marked as PERMANENT_MENU_ITEM 
	for (size_t n = 0; n < nItems; n++) {
		XtVaGetValues(items[n], XmNuserData, &userData, nullptr);
		if (userData != (XtPointer)PERMANENT_MENU_ITEM) {
			if (XtClass(items[n]) == xmCascadeButtonWidgetClass) {
				XtVaGetValues(items[n], XmNsubMenuId, &subMenuID, nullptr);

				// prevent dangling submenu tearoffs 
				if (!XmIsMenuShell(XtParent(subMenuID)))
					_XmDismissTearOff(XtParent(subMenuID), nullptr, nullptr);

				deleteMenuItems(subMenuID);
			} else {
				// remove accel. before destroy or lose it forever 
				XtVaSetValues(items[n], XmNaccelerator, nullptr, nullptr);
			}
			XtDestroyWidget(items[n]);
		}
	}
	
	delete [] items;
}

int checkMacroText(const char *macro, Widget errorParent, Widget errFocus) {
	Program *prog;
	const char *errMsg;
	const char *stoppedAt;

	prog = ParseMacro(macro, &errMsg, &stoppedAt);
	if(!prog) {
		if (errorParent) {
			ParseError(errorParent, macro, stoppedAt, "macro", errMsg);
			XmTextSetInsertionPosition(errFocus, stoppedAt - macro);
			XmProcessTraversal(errFocus, XmTRAVERSE_CURRENT);
		}
		return False;
	}
	FreeProgram(prog);
	if (*stoppedAt != '\0') {
		if (errorParent) {
			ParseError(errorParent, macro, stoppedAt, "macro", "syntax error");
			XmTextSetInsertionPosition(errFocus, stoppedAt - macro);
			XmProcessTraversal(errFocus, XmTRAVERSE_CURRENT);
		}
		return False;
	}
	return True;
}

static void shellMenuCB(Widget w, XtPointer clientData, XtPointer callData) {

	auto window = static_cast<Document *>(clientData);

	XtArgVal userData;

	window = Document::WidgetToWindow(MENU_WIDGET(w));

	// get the index of the shell command and verify that it's in range 
	XtVaGetValues(w, XmNuserData, &userData, nullptr);
	int index = (int)userData - 10;
	if (index < 0 || index >= ShellMenuData.size())
		return;

	QByteArray str = ShellMenuData[index].item->name.toLatin1();
	XtCallActionProcEx(window->lastFocus_, "shell_menu_command", ((XmAnyCallbackStruct *)callData)->event, str.data());
}

static void macroMenuCB(Widget w, XtPointer clientData, XtPointer callData) {

	auto window = static_cast<Document *>(clientData);

	XtArgVal userData;

	window = Document::WidgetToWindow(MENU_WIDGET(w));

	/* Don't allow users to execute a macro command from the menu (or accel)
	   if there's already a macro command executing.  NEdit can't handle
	   running multiple, independent uncoordinated, macros in the same
	   window.  Macros may invoke macro menu commands recursively via the
	   macro_menu_command action proc, which is important for being able to
	   repeat any operation, and to embed macros within eachother at any
	   level, however, a call here with a macro running means that THE USER
	   is explicitly invoking another macro via the menu or an accelerator. */
	if (window->macroCmdData_) {
		QApplication::beep();
		return;
	}

	// get the index of the macro command and verify that it's in range 
	XtVaGetValues(w, XmNuserData, &userData, nullptr);
	int index = (int)userData - 10;
	if (index < 0 || index >= MacroMenuData.size())
		return;

	QByteArray str = MacroMenuData[index].item->name.toLatin1();
	XtCallActionProcEx(window->lastFocus_, "macro_menu_command", ((XmAnyCallbackStruct *)callData)->event, str.data());
}

static void bgMenuCB(Widget w, XtPointer clientData, XtPointer callData) {

	auto window = static_cast<Document *>(clientData);

	XtArgVal userData;

	// Same remark as for macro menu commands (see above). 
	if (window->macroCmdData_) {
		QApplication::beep();
		return;
	}

	// get the index of the macro command and verify that it's in range 
	XtVaGetValues(w, XmNuserData, &userData, nullptr);
	int index = (int)userData - 10;
	if (index < 0 || index >= BGMenuData.size())
		return;


	
	QByteArray str = BGMenuData[index].item->name.toLatin1();
	XtCallActionProcEx(window->lastFocus_, "bg_menu_command", ((XmAnyCallbackStruct *)callData)->event, str.data());
}

static char *writeMenuItemString(const QVector<MenuData> &menuItems, int listType) {
	char accStr[MAX_ACCEL_LEN];

	int length;

	/* determine the max. amount of memory needed for the returned string
	   and allocate a buffer for composing the string */
	   
	// NOTE(eteran): this code unconditionally writes at least 2 chars
	// so to avoid an off by one error, this needs to be initialized to
	// 1
	length = 1;
	
	for (const MenuData &data : menuItems) {
		MenuItem *f = data.item;
		generateAcceleratorString(accStr, f->modifiers, f->keysym);
		length += f->name.size() * 2; // allow for \n & \\ expansions 
		length += strlen(accStr);
		length += f->cmd.size() * 6; // allow for \n & \\ expansions 
		length += 21;                 // number of characters added below 
	}
	
	length++; // terminating null 
	char *outStr = XtMalloc(length);

	// write the string 
	char *outPtr = outStr;
	*outPtr++ = '\\';
	*outPtr++ = '\n';
	
	for (const MenuData &data : menuItems) {
		MenuItem *f = data.item;
		
		generateAcceleratorString(accStr, f->modifiers, f->keysym);
		*outPtr++ = '\t';
		
		for (auto ch : f->name) { // Copy the command name 
			if (ch == QLatin1Char('\\')) {                // changing backslashes to "\\" 
				*outPtr++ = '\\';
				*outPtr++ = '\\';
			} else if (ch == QLatin1Char('\n')) { // changing newlines to \n 
				*outPtr++ = '\\';
				*outPtr++ = 'n';
			} else {
				*outPtr++ = ch.toLatin1();
			}
		}
		
		*outPtr++ = ':';
		strcpy(outPtr, accStr);
		outPtr += strlen(accStr);
		*outPtr++ = ':';
		if (f->mnemonic != '\0')
			*outPtr++ = f->mnemonic;
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
		*outPtr++ = '\\';
		*outPtr++ = 'n';
		*outPtr++ = '\\';
		*outPtr++ = '\n';
		*outPtr++ = '\t';
		*outPtr++ = '\t';
		
		for(QChar c : f->cmd) {
			if (c == QLatin1Char('\\')) {                
				*outPtr++ = '\\';                // Copy the command string, changing 
				*outPtr++ = '\\';                // backslashes to double backslashes 
			} else if (c == QLatin1Char('\n')) { // and newlines to backslash-n's,    
				*outPtr++ = '\\';			     // followed by real newlines and tab 
				*outPtr++ = 'n';
				*outPtr++ = '\\';
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
		
		*outPtr++ = '\\';
		*outPtr++ = 'n';
		*outPtr++ = '\\';
		*outPtr++ = '\n';
	}
	--outPtr;
	*--outPtr = '\0';
	return outStr;
}

static QString writeMenuItemStringEx(const QVector<MenuData> &menuItems, int listType) {
	QString str;
	if(char *s = writeMenuItemString(menuItems, listType)) {
		str = QLatin1String(s);
		XtFree(s);
	}
	return str;
}

static int loadMenuItemString(const char *inString, QVector<MenuData> &menuItems, int listType) {
	char *cmdStr;
	const char *inPtr = inString;
	char *nameStr;
	char accStr[MAX_ACCEL_LEN];
	char mneChar;
	KeySym keysym;
	unsigned int modifiers;
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
		nameStr = XtMalloc(nameLen + 1);
		strncpy(nameStr, inPtr, nameLen);
		nameStr[nameLen] = '\0';
		inPtr += nameLen;
		if (*inPtr == '\0')
			return parseError("end not expected");
		inPtr++;

		// read accelerator field 
		accLen = strcspn(inPtr, ":");
		if (accLen >= MAX_ACCEL_LEN)
			return parseError("accelerator field too long");
		strncpy(accStr, inPtr, accLen);
		accStr[accLen] = '\0';
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
					repInput = True;
				else if (*inPtr == 'S')
					saveFirst = True;
				else if (*inPtr == 'L')
					loadAfter = True;
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
			cmdStr = XtMalloc(cmdLen + 1);
			strncpy(cmdStr, inPtr, cmdLen);
			cmdStr[cmdLen] = '\0';
			inPtr += cmdLen;
		} else {
			cmdStr = copyMacroToEnd(&inPtr);
			if(!cmdStr)
				return False;
		}
		while (*inPtr == ' ' || *inPtr == '\t' || *inPtr == '\n') {
			inPtr++; // skip trailing whitespace & newline 
		}

		// parse the accelerator field 
		if (!parseAcceleratorString(accStr, &modifiers, &keysym)) {
			return parseError("couldn't read accelerator field");
		}

		// create a menu item record 
		auto f = new MenuItem;
		f->name      = QLatin1String(nameStr);
		f->cmd       = QLatin1String(cmdStr);
		f->mnemonic  = mneChar;
		f->modifiers = modifiers;
		f->input     = input;
		f->output    = output;
		f->repInput  = repInput;
		f->saveFirst = saveFirst;
		f->loadAfter = loadAfter;
		f->keysym    = keysym;
		
		XtFree(nameStr);
		XtFree(cmdStr);

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
** Create a text string representing an accelerator for the dialog,
** the shellCommands or macroCommands resource, and for the menu item.
*/
void generateAcceleratorString(char *text, unsigned int modifiers, KeySym keysym) {
	const char *shiftStr = "";
	const char *ctrlStr = "";
	const char *altStr = "";
	const char *mod2Str = "";
	const char *mod3Str = "";
	const char *mod4Str = "";
	const char *mod5Str = "";
	char keyName[20];
	Modifiers numLockMask = GetNumLockModMask(TheDisplay);

	// if there's no accelerator, generate an empty string 
	if (keysym == NoSymbol) {
		*text = '\0';
		return;
	}

	/* Translate the modifiers into strings.
	   Lock and NumLock are always ignored (see util/misc.c),
	   so we don't display them either. */
	if (modifiers & ShiftMask)
		shiftStr = "Shift+";
	if (modifiers & ControlMask)
		ctrlStr = "Ctrl+";
	if (modifiers & Mod1Mask)
		altStr = "Alt+";
	if ((modifiers & Mod2Mask) && (Mod2Mask != numLockMask))
		mod2Str = "Mod2+";
	if ((modifiers & Mod3Mask) && (Mod3Mask != numLockMask))
		mod3Str = "Mod3+";
	if ((modifiers & Mod4Mask) && (Mod4Mask != numLockMask))
		mod4Str = "Mod4+";
	if ((modifiers & Mod5Mask) && (Mod5Mask != numLockMask))
		mod5Str = "Mod5+";

	/* for a consistent look to the accelerator names in the menus,
	   capitalize the first letter of the keysym */
	strcpy(keyName, XKeysymToString(keysym));
	*keyName = toupper(*keyName);

	// concatenate the strings together 
	sprintf(text, "%s%s%s%s%s%s%s%s", shiftStr, ctrlStr, altStr, mod2Str, mod3Str, mod4Str, mod5Str, keyName);
}

/*
** Create a translation table event description string for the menu
** XmNaccelerator resource.
*/
static void genAccelEventName(char *text, unsigned int modifiers, KeySym keysym) {
	const char *shiftStr = "";
	const char *lockStr = "";
	const char *ctrlStr = "";
	const char *altStr = "";
	const char *mod2Str = "";
	const char *mod3Str = "";
	const char *mod4Str = "";
	const char *mod5Str = "";

	// if there's no accelerator, generate an empty string 
	if (keysym == NoSymbol) {
		*text = '\0';
		return;
	}

	// translate the modifiers into strings 
	if (modifiers & ShiftMask)
		shiftStr = "Shift ";
	if (modifiers & LockMask)
		lockStr = "Lock ";
	if (modifiers & ControlMask)
		ctrlStr = "Ctrl ";
	if (modifiers & Mod1Mask)
		altStr = "Alt ";
	if (modifiers & Mod2Mask)
		mod2Str = "Mod2 ";
	if (modifiers & Mod3Mask)
		mod3Str = "Mod3 ";
	if (modifiers & Mod4Mask)
		mod4Str = "Mod4 ";
	if (modifiers & Mod5Mask)
		mod5Str = "Mod5 ";

	// put the modifiers together with the key name 
	sprintf(text, "%s%s%s%s%s%s%s%s<Key>%s", shiftStr, lockStr, ctrlStr, altStr, mod2Str, mod3Str, mod4Str, mod5Str, XKeysymToString(keysym));
}

/*
** Read an accelerator name and put it into the form of a modifier mask
** and a KeySym code.  Returns false if string can't be read
** ... does not handle whitespace in string (look at scanf)
*/
int parseAcceleratorString(const char *string, unsigned int *modifiers, KeySym *keysym) {
	int i, nFields, inputLength = strlen(string);
	char fields[10][MAX_ACCEL_LEN];

	// a blank field means no accelerator 
	if (inputLength == 0) {
		*modifiers = 0;
		*keysym = NoSymbol;
		return True;
	}

	// limit the string length so no field strings will overflow 
	if (inputLength > MAX_ACCEL_LEN)
		return False;

	// divide the input into '+' separated fields 
	nFields = sscanf(string, "%[^+]+%[^+]+%[^+]+%[^+]+%[^+]+%[^+]+%[^+]+%[^+]+%[^+]+%[^+]", fields[0], fields[1], fields[2], fields[3], fields[4], fields[5], fields[6], fields[7], fields[8], fields[9]);
	if (nFields == 0)
		return False;

	/* get the key name from the last field and translate it to a keysym.
	   If the name is capitalized, try it lowercase as well, since some
	   of the keysyms are "prettied up" by generateAcceleratorString */
	*keysym = XStringToKeysym(fields[nFields - 1]);
	if (*keysym == NoSymbol) {
		*fields[nFields - 1] = tolower(*fields[nFields - 1]);
		*keysym = XStringToKeysym(fields[nFields - 1]);
		if (*keysym == NoSymbol)
			return False;
	}

	// parse the modifier names from the rest of the fields 
	*modifiers = 0;
	for (i = 0; i < nFields - 1; i++) {
		if (!strcmp(fields[i], "Shift"))
			*modifiers |= ShiftMask;
		else if (!strcmp(fields[i], "Lock"))
			*modifiers |= LockMask;
		else if (!strcmp(fields[i], "Ctrl"))
			*modifiers |= ControlMask;
		// comparision with "Alt" for compatibility with old .nedit files
		else if (!strcmp(fields[i], "Alt"))
			*modifiers |= Mod1Mask;
		else if (!strcmp(fields[i], "Mod2"))
			*modifiers |= Mod2Mask;
		else if (!strcmp(fields[i], "Mod3"))
			*modifiers |= Mod3Mask;
		else if (!strcmp(fields[i], "Mod4"))
			*modifiers |= Mod4Mask;
		else if (!strcmp(fields[i], "Mod5"))
			*modifiers |= Mod5Mask;
		else
			return False;
	}

	// all fields successfully parsed 
	return True;
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
	char *retStr;
	const char *errMsg;
	const char *stoppedAt;
	const char *p;
	char *retPtr;
	Program *prog;

	/* Skip over whitespace to find make sure there's a beginning brace
	   to anchor the parse (if not, it will take the whole file) */
	*inPtr += strspn(*inPtr, " \t\n");
	if (**inPtr != '{') {
		ParseError(nullptr, *inPtr, *inPtr - 1, "macro menu item", "expecting '{'");
		return nullptr;
	}

	// Parse the input 
	prog = ParseMacro(*inPtr, &errMsg, &stoppedAt);
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
	retPtr = retStr = XtMalloc(stoppedAt - *inPtr + 1);
	for (p = *inPtr; p < stoppedAt - 1; p++) {
		if (!strncmp(p, "\n\t\t", 3)) {
			*retPtr++ = '\n';
			p += 2;
		} else
			*retPtr++ = *p;
	}
	if (*(retPtr - 1) == '\t')
		retPtr--;
	*retPtr = '\0';
	*inPtr = stoppedAt;
	return retStr;
}

/*
** Cache user menus:
** allocate an empty user (shell, macro) menu cache structure
*/
UserMenuCache *CreateUserMenuCache(void) {
	// allocate some memory for the new data structure 
	auto cache = new UserMenuCache;

	cache->umcLanguageMode = -2;
	cache->umcShellMenuCreated = False;
	cache->umcMacroMenuCreated = False;
	cache->umcShellMenuList.clear();
	cache->umcMacroMenuList.clear();

	return cache;
}

void FreeUserMenuCache(UserMenuCache *cache) {
	freeUserMenuList(&cache->umcShellMenuList);
	freeUserMenuList(&cache->umcMacroMenuList);

	delete cache;
}

/*
** Cache user menus:
** init. a user background menu cache structure
*/
void InitUserBGMenuCache(UserBGMenuCache *cache) {
	cache->ubmcLanguageMode = -2;
	cache->ubmcMenuCreated = False;
	cache->ubmcMenuList.clear();
}

void FreeUserBGMenuCache(UserBGMenuCache *cache) {
	freeUserMenuList(&cache->ubmcMenuList);
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
	newInfo->umiIdLen = 0;
	newInfo->umiIsDefault = False;
	newInfo->umiNbrOfLanguageModes = 0;
	newInfo->umiLanguageMode = nullptr;
	newInfo->umiDefaultIndex = -1;
	newInfo->umiToBeManaged = False;

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
			info->umiIsDefault = True;
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
			languageMode = FindLanguageMode(atPtr + 1);
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
** Find info corresponding to a hierarchical menu name (a>b>c...)
*/
static userSubMenuInfo *findSubMenuInfoEx(userSubMenuCache *subMenus, const QString &hierName) {

	for (int i = 0; i < subMenus->usmcNbrOfSubMenus; i++) {
		if (hierName == QLatin1String(subMenus->usmcInfo[i].usmiName)) {
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
		return XtNewStringEx(latin1.data());
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

/*
** Determine the info list menu items, which need to be managed
** for given language mode. Set / reset "to be managed" indication
** of info list items accordingly.
*/
static void applyLangModeToUserMenuInfo(const QVector<MenuData> &infoList, int languageMode) {
	/* 1st pass: mark all items as "to be managed", which are applicable
	   for all language modes or which are indicated as "default" items */
	
	for (const MenuData &data: infoList) {
		userMenuInfo *info = data.info;
		info->umiToBeManaged = (info->umiNbrOfLanguageModes == 0 || info->umiIsDefault);
	}

	/* 2nd pass: mark language mode specific items matching given language
	   mode as "to be managed". Reset "to be managed" indications of
	   "default" items, if applicable */
	for (const MenuData &data: infoList) {
		userMenuInfo *info = data.info;

		if (info->umiNbrOfLanguageModes != 0) {
			if (doesLanguageModeMatch(info, languageMode)) {
				info->umiToBeManaged = True;

				if (info->umiDefaultIndex != -1)
					infoList[info->umiDefaultIndex].info->umiToBeManaged = false;
			}
		}
	}
}

/*
** Returns true, if given user menu info is applicable for given language mode
*/
static bool doesLanguageModeMatch(userMenuInfo *info, int languageMode) {

	for (int i = 0; i < info->umiNbrOfLanguageModes; i++) {
		if (info->umiLanguageMode[i] == languageMode) {
			return true;
		}
	}

	return false;
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

void freeSubMenuCache(userSubMenuCache *subMenus) {

	for (int i = 0; i < subMenus->usmcNbrOfSubMenus; i++) {
		XtFree(subMenus->usmcInfo[i].usmiName);
		delete [] subMenus->usmcInfo[i].usmiId;
	}

	delete [] subMenus->usmcInfo;
}

void freeUserMenuList(UserMenuList *list) {

	qDeleteAll(*list);
	list->clear();
}
