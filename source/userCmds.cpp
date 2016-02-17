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
#include "TextBuffer.h"
#include "text.h"
#include "nedit.h"
#include "preferences.h"
#include "Document.h"
#include "window.h"
#include "menu.h"
#include "shell.h"
#include "macro.h"
#include "file.h"
#include "interpret.h"
#include "parse.h"
#include "MotifHelper.h"
#include "../util/DialogF.h"
#include "../util/misc.h"
#include "../util/managedList.h"
#include "XString.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

#include <sys/param.h>

#include <Xm/Xm.h>
#include <X11/keysym.h>
#include <X11/IntrinsicP.h>
#include <Xm/Text.h>
#include <Xm/Form.h>
#include <Xm/List.h>
#include <Xm/LabelG.h>
#include <Xm/PushB.h>
#include <Xm/ToggleB.h>
#include <Xm/SelectioB.h>
#include <Xm/RowColumn.h>
#include <Xm/CascadeB.h>
#include <Xm/MenuShell.h>

#define MENU_WIDGET(w) (XmGetPostedFromWidget(XtParent(w)))

#ifdef __cplusplus
extern "C" void _XmDismissTearOff(Widget, XtPointer, XtPointer);
#else
extern void _XmDismissTearOff(Widget, XtPointer, XtPointer);
#endif

/* max. number of user programmable menu commands allowed per each of the
   macro, shell, and background menus */
#define MAX_ITEMS_PER_MENU 400

/* indicates, that an unknown (i.e. not existing) language mode
   is bound to an user menu item */
#define UNKNOWN_LANGUAGE_MODE -2

/* major divisions (in position units) in User Commands dialogs */
#define LEFT_MARGIN_POS 1
#define RIGHT_MARGIN_POS 99
#define LIST_RIGHT 45
#define SHELL_CMD_TOP 70
#define MACRO_CMD_TOP 40

/* types of current dialog and/or menu */
enum dialogTypes { SHELL_CMDS, MACRO_CMDS, BG_MENU_CMDS };

/* Structure representing a menu item for shell, macro and BG menus*/
struct menuItemRec {
	XString name;
	unsigned int modifiers;
	KeySym keysym;
	char mnemonic;
	char input;
	char output;
	char repInput;
	char saveFirst;
	char loadAfter;
	char *cmd;
};

/* Structure for widgets and flags associated with shell command,
   macro command and BG command editing dialogs */
struct userCmdDialog {
	int dialogType;
	Document *window;
	Widget nameTextW, accTextW, mneTextW, cmdTextW, saveFirstBtn;
	Widget loadAfterBtn, selInpBtn, winInpBtn, eitherInpBtn, noInpBtn;
	Widget repInpBtn, sameOutBtn, dlogOutBtn, winOutBtn, dlogShell;
	Widget managedList;
	menuItemRec **menuItemsList;
	int nMenuItems;
};

/* Structure for keeping track of hierarchical sub-menus during user-menu
   creation */
struct menuTreeItem {
	XString name;
	Widget  menuPane;
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
	char *usmiName; /* hierarchical name of sub-menu */
	int *usmiId;    /* hierarchical ID of sub-menu   */
	int usmiIdLen;  /* length of hierarchical ID     */
};

/* Holds info about sub-menu structure of an user menu */
struct userSubMenuCache {
	int usmcNbrOfMainMenuItems; /* number of main menu items */
	int usmcNbrOfSubMenus;      /* number of sub-menus */
	userSubMenuInfo *usmcInfo;  /* list of sub-menu info */
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
	char *umiName;             /* hierarchical name of menu item
	                              (w.o. language mode info) */
	int *umiId;                /* hierarchical ID of menu item */
	int umiIdLen;              /* length of hierarchical ID */
	Boolean umiIsDefault;      /* menu item is default one ("@*") */
	int umiNbrOfLanguageModes; /* number of language modes
	                              applicable for this menu item */
	int *umiLanguageMode;      /* list of applicable lang. modes */
	int umiDefaultIndex;       /* array index of menu item to be
	                              used as default, if no lang. mode
	                              matches */
	Boolean umiToBeManaged;    /* indicates, that menu item needs
	                              to be managed */
};

/* Structure holding info about a selected user menu (shell, macro or
   background) */
struct selectedUserMenu {
	int sumType;                   /* type of menu (shell, macro or
	                                  background */
	Widget sumMenuPane;            /* pane of main menu */
	int sumNbrOfListItems;         /* number of menu items */
	menuItemRec **sumItemList;     /* list of menu items */
	userMenuInfo **sumInfoList;    /* list of infos about menu items */
	userSubMenuCache *sumSubMenus; /* info about sub-menu structure */
	UserMenuList *sumMainMenuList; /* cached info about main menu */
	Boolean *sumMenuCreated;       /* pointer to "menu created"
	                                  indicator */
};

/* Descriptions of the current user programmed menu items for re-generating
   menus and processing shell, macro, and background menu selections */
static menuItemRec *ShellMenuItems[MAX_ITEMS_PER_MENU];
static userMenuInfo *ShellMenuInfo[MAX_ITEMS_PER_MENU];
static userSubMenuCache ShellSubMenus;
static int NShellMenuItems = 0;
static menuItemRec *MacroMenuItems[MAX_ITEMS_PER_MENU];
static userMenuInfo *MacroMenuInfo[MAX_ITEMS_PER_MENU];
static userSubMenuCache MacroSubMenus;
static int NMacroMenuItems = 0;
static menuItemRec *BGMenuItems[MAX_ITEMS_PER_MENU];
static userMenuInfo *BGMenuInfo[MAX_ITEMS_PER_MENU];
static userSubMenuCache BGSubMenus;
static int NBGMenuItems = 0;

/* Top level shells of the user-defined menu editing dialogs */
static Widget ShellCmdDialog = nullptr;
static Widget MacroCmdDialog = nullptr;
static Widget BGMenuCmdDialog = nullptr;

/* Paste learn/replay sequence buttons in user defined menu editing dialogs
   (for dimming and undimming externally when replay macro is available) */
static Widget MacroPasteReplayBtn = nullptr;
static Widget BGMenuPasteReplayBtn = nullptr;

static void editMacroOrBGMenu(Document *window, int dialogType);
static void dimSelDepItemsInMenu(Widget menuPane, menuItemRec **menuList, int nMenuItems, int sensitive);
static void rebuildMenuOfAllWindows(int menuType);
static void rebuildMenu(Document *window, int menuType);
static Widget findInMenuTreeEx(menuTreeItem *menuTree, int nTreeEntries, const XString &hierName);
static char *copySubstring(const char *string, int length);
static XString copySubstringEx(const char *string, int length);
static Widget createUserMenuItem(Widget menuPane, char *name, menuItemRec *f, int index, XtCallbackProc cbRtn, XtPointer cbArg);
static Widget createUserSubMenuEx(Widget parent, const XString &label, Widget *menuItem);
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
static void okCB(Widget w, XtPointer clientData, XtPointer callData);
static void applyCB(Widget w, XtPointer clientData, XtPointer callData);
static void checkCB(Widget w, XtPointer clientData, XtPointer callData);
static int checkMacro(userCmdDialog *ucd);
static int checkMacroText(const char *macro, Widget errorParent, Widget errFocus);
static int applyDialogChanges(userCmdDialog *ucd);
static void closeCB(Widget w, XtPointer clientData, XtPointer callData);
static void pasteReplayCB(Widget w, XtPointer clientData, XtPointer callData);
static void destroyCB(Widget w, XtPointer clientData, XtPointer callData);
static void accKeyCB(Widget w, XtPointer clientData, XKeyEvent *event);
static void sameOutCB(Widget w, XtPointer clientData, XtPointer callData);
static void shellMenuCB(Widget w, XtPointer clientData, XtPointer callData);
static void macroMenuCB(Widget w, XtPointer clientData, XtPointer callData);
static void bgMenuCB(Widget w, XtPointer clientData, XtPointer callData);
static void accFocusCB(Widget w, XtPointer clientData, XtPointer callData);
static void accLoseFocusCB(Widget w, XtPointer clientData, XtPointer callData);
static void updateDialogFields(menuItemRec *f, userCmdDialog *ucd);
static menuItemRec *readDialogFields(userCmdDialog *ucd, int silent);
static menuItemRec *copyMenuItemRec(menuItemRec *item);
static void freeMenuItemRec(menuItemRec *item);
static void *getDialogDataCB(void *oldItem, int explicitRequest, int *abort, void *cbArg);
static void setDialogDataCB(void *item, void *cbArg);
static void freeItemCB(void *item);
static int dialogFieldsAreEmpty(userCmdDialog *ucd);
static void disableTextW(Widget textW);
static char *writeMenuItemString(menuItemRec **menuItems, int nItems, int listType);
static nullable_string writeMenuItemStringEx(menuItemRec **menuItems, int nItems, int listType);
static int loadMenuItemString(const char *inString, menuItemRec **menuItems, int *nItems, int listType);
static void generateAcceleratorString(char *text, unsigned int modifiers, KeySym keysym);
static void genAccelEventName(char *text, unsigned int modifiers, KeySym keysym);
static int parseAcceleratorString(const char *string, unsigned int *modifiers, KeySym *keysym);
static int parseError(const char *message);
static char *copyMacroToEnd(const char **inPtr);
static char *addTerminatingNewlineEx(char *string) ;
static void parseMenuItemList(menuItemRec **itemList, int nbrOfItems, userMenuInfo **infoList, userSubMenuCache *subMenus);
static int getSubMenuDepth(const char *menuName);
static userMenuInfo *parseMenuItemRec(menuItemRec *item);
static void parseMenuItemName(char *menuItemName, userMenuInfo *info);
static void generateUserMenuId(userMenuInfo *info, userSubMenuCache *subMenus);
static userSubMenuInfo *findSubMenuInfo(userSubMenuCache *subMenus, const char *hierName);
static userSubMenuInfo *findSubMenuInfoEx(userSubMenuCache *subMenus, const XString &hierName);
static char *stripLanguageModeEx(const XString &menuItemName);
static void setDefaultIndex(userMenuInfo **infoList, int nbrOfItems, int defaultIdx);
static void applyLangModeToUserMenuInfo(userMenuInfo **infoList, int nbrOfItems, int languageMode);
static int doesLanguageModeMatch(userMenuInfo *info, int languageMode);
static void freeUserMenuInfoList(userMenuInfo **infoList, int nbrOfItems);
static void freeUserMenuInfo(userMenuInfo *info);
static void allocSubMenuCache(userSubMenuCache *subMenus, int nbrOfItems);
static void freeSubMenuCache(userSubMenuCache *subMenus);
static void allocUserMenuList(UserMenuList *list, int nbrOfItems);
static void freeUserMenuList(UserMenuList *list);
static UserMenuListElement *allocUserMenuListElement(Widget menuItem, char *accKeys);
static void freeUserMenuListElement(UserMenuListElement *element);
static UserMenuList *allocUserSubMenuList(int nbrOfItems);
static void freeUserSubMenuList(UserMenuList *list);

/*
** Present a dialog for editing the user specified commands in the shell menu
*/
void EditShellMenu(Document *window) {
	Widget form, accLabel, inpLabel, inpBox, outBox, outLabel;
	Widget nameLabel, cmdLabel, okBtn, applyBtn, closeBtn;
	XmString s1;
	int ac;
	Arg args[20];

	/* if the dialog is already displayed, just pop it to the top and return */
	if (ShellCmdDialog) {
		RaiseDialogWindow(ShellCmdDialog);
		return;
	}

	/* Create a structure for keeping track of dialog state */
	auto ucd = new userCmdDialog;
	ucd->window = window;

	/* Set the dialog to operate on the Shell menu */
	ucd->menuItemsList = new menuItemRec *[MAX_ITEMS_PER_MENU];
	for (int i = 0; i < NShellMenuItems; i++) {
		ucd->menuItemsList[i] = copyMenuItemRec(ShellMenuItems[i]);
	}
	
	ucd->nMenuItems = NShellMenuItems;
	ucd->dialogType = SHELL_CMDS;

	ac = 0;
	XtSetArg(args[ac], XmNdeleteResponse, XmDO_NOTHING); ac++;
	XtSetArg(args[ac], XmNiconName, "NEdit Shell Menu"); ac++;
	XtSetArg(args[ac], XmNtitle, "Shell Menu");          ac++;
	
	ucd->dlogShell = CreateWidget(TheAppShell, "shellCommands", topLevelShellWidgetClass, args, ac);
	AddSmallIcon(ucd->dlogShell);
	form = XtVaCreateManagedWidget("editShellCommands", xmFormWidgetClass, ucd->dlogShell, XmNautoUnmanage, False, XmNresizePolicy, XmRESIZE_NONE, nullptr);
	ShellCmdDialog = ucd->dlogShell;
	XtAddCallback(form, XmNdestroyCallback, destroyCB, ucd);
	AddMotifCloseCallback(ucd->dlogShell, closeCB, ucd);

	ac = 0;
	XtSetArg(args[ac], XmNtopAttachment, XmATTACH_POSITION);
	ac++;
	XtSetArg(args[ac], XmNtopPosition, 2);
	ac++;
	XtSetArg(args[ac], XmNleftAttachment, XmATTACH_POSITION);
	ac++;
	XtSetArg(args[ac], XmNleftPosition, LEFT_MARGIN_POS);
	ac++;
	XtSetArg(args[ac], XmNrightAttachment, XmATTACH_POSITION);
	ac++;
	XtSetArg(args[ac], XmNrightPosition, LIST_RIGHT - 1);
	ac++;
	XtSetArg(args[ac], XmNbottomAttachment, XmATTACH_POSITION);
	ac++;
	XtSetArg(args[ac], XmNbottomPosition, SHELL_CMD_TOP);
	ac++;
	ucd->managedList = CreateManagedList(form, "list", args, ac, (void **)ucd->menuItemsList, &ucd->nMenuItems, MAX_ITEMS_PER_MENU, 20, getDialogDataCB, ucd, setDialogDataCB, ucd, freeItemCB);

	ucd->loadAfterBtn = XtVaCreateManagedWidget("loadAfterBtn", xmToggleButtonWidgetClass, form, XmNlabelString, s1 = XmStringCreateLtoREx("Re-load file after executing command"), XmNmnemonic, 'R', XmNalignment, XmALIGNMENT_BEGINNING, XmNset,
	                                            False, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, LIST_RIGHT, XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, RIGHT_MARGIN_POS, XmNbottomAttachment, XmATTACH_POSITION,
	                                            XmNbottomPosition, SHELL_CMD_TOP, nullptr);
	XmStringFree(s1);
	ucd->saveFirstBtn = XtVaCreateManagedWidget("saveFirstBtn", xmToggleButtonWidgetClass, form, XmNlabelString, s1 = XmStringCreateLtoREx("Save file before executing command"), XmNmnemonic, 'f', XmNalignment, XmALIGNMENT_BEGINNING, XmNset,
	                                            False, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, LIST_RIGHT, XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, RIGHT_MARGIN_POS, XmNbottomAttachment, XmATTACH_WIDGET,
	                                            XmNbottomWidget, ucd->loadAfterBtn, nullptr);
	XmStringFree(s1);
	ucd->repInpBtn =
	    XtVaCreateManagedWidget("repInpBtn", xmToggleButtonWidgetClass, form, XmNlabelString, s1 = XmStringCreateLtoREx("Output replaces input"), XmNmnemonic, 'f', XmNalignment, XmALIGNMENT_BEGINNING, XmNset, False, XmNleftAttachment,
	                            XmATTACH_POSITION, XmNleftPosition, LIST_RIGHT, XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, RIGHT_MARGIN_POS, XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, ucd->saveFirstBtn, nullptr);
	XmStringFree(s1);
	outBox = XtVaCreateManagedWidget("outBox", xmRowColumnWidgetClass, form, XmNpacking, XmPACK_TIGHT, XmNorientation, XmHORIZONTAL, XmNradioBehavior, True, XmNradioAlwaysOne, True, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition,
	                                 LIST_RIGHT + 2, XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, RIGHT_MARGIN_POS, XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, ucd->repInpBtn, XmNbottomOffset, 4, nullptr);
	ucd->sameOutBtn =
	    XtVaCreateManagedWidget("sameOutBtn", xmToggleButtonWidgetClass, outBox, XmNlabelString, s1 = XmStringCreateLtoREx("same document"), XmNmnemonic, 'm', XmNalignment, XmALIGNMENT_BEGINNING, XmNmarginHeight, 0, XmNset, True, nullptr);
	XmStringFree(s1);
	XtAddCallback(ucd->sameOutBtn, XmNvalueChangedCallback, sameOutCB, ucd);
	ucd->dlogOutBtn =
	    XtVaCreateManagedWidget("dlogOutBtn", xmToggleButtonWidgetClass, outBox, XmNlabelString, s1 = XmStringCreateLtoREx("dialog"), XmNmnemonic, 'g', XmNalignment, XmALIGNMENT_BEGINNING, XmNmarginHeight, 0, XmNset, False, nullptr);
	XmStringFree(s1);
	ucd->winOutBtn =
	    XtVaCreateManagedWidget("winOutBtn", xmToggleButtonWidgetClass, outBox, XmNlabelString, s1 = XmStringCreateLtoREx("new document"), XmNmnemonic, 'n', XmNalignment, XmALIGNMENT_BEGINNING, XmNmarginHeight, 0, XmNset, False, nullptr);
	XmStringFree(s1);
	outLabel = XtVaCreateManagedWidget("outLabel", xmLabelGadgetClass, form, XmNlabelString, s1 = XmStringCreateLtoREx("Command Output (stdout/stderr):"), XmNalignment, XmALIGNMENT_BEGINNING, XmNmarginTop, 5, XmNleftAttachment,
	                                   XmATTACH_POSITION, XmNleftPosition, LIST_RIGHT, XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, RIGHT_MARGIN_POS, XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, outBox, nullptr);
	XmStringFree(s1);

	inpBox = XtVaCreateManagedWidget("inpBox", xmRowColumnWidgetClass, form, XmNpacking, XmPACK_TIGHT, XmNorientation, XmHORIZONTAL, XmNradioBehavior, True, XmNradioAlwaysOne, True, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition,
	                                 LIST_RIGHT + 2, XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, RIGHT_MARGIN_POS, XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, outLabel, nullptr);
	ucd->selInpBtn =
	    XtVaCreateManagedWidget("selInpBtn", xmToggleButtonWidgetClass, inpBox, XmNlabelString, s1 = XmStringCreateLtoREx("selection"), XmNmnemonic, 's', XmNalignment, XmALIGNMENT_BEGINNING, XmNmarginHeight, 0, XmNset, True, nullptr);
	XmStringFree(s1);
	ucd->winInpBtn =
	    XtVaCreateManagedWidget("winInpBtn", xmToggleButtonWidgetClass, inpBox, XmNlabelString, s1 = XmStringCreateLtoREx("document"), XmNmnemonic, 'w', XmNalignment, XmALIGNMENT_BEGINNING, XmNmarginHeight, 0, XmNset, False, nullptr);
	XmStringFree(s1);
	ucd->eitherInpBtn =
	    XtVaCreateManagedWidget("eitherInpBtn", xmToggleButtonWidgetClass, inpBox, XmNlabelString, s1 = XmStringCreateLtoREx("either"), XmNmnemonic, 't', XmNalignment, XmALIGNMENT_BEGINNING, XmNmarginHeight, 0, XmNset, False, nullptr);
	XmStringFree(s1);
	ucd->noInpBtn = XtVaCreateManagedWidget("noInpBtn", xmToggleButtonWidgetClass, inpBox, XmNlabelString, s1 = XmStringCreateLtoREx("none"), XmNmnemonic, 'o', XmNalignment, XmALIGNMENT_BEGINNING, XmNmarginHeight, 0, XmNset, False, nullptr);
	XmStringFree(s1);
	inpLabel = XtVaCreateManagedWidget("inpLabel", xmLabelGadgetClass, form, XmNlabelString, s1 = XmStringCreateLtoREx("Command Input (stdin):"), XmNalignment, XmALIGNMENT_BEGINNING, XmNmarginTop, 5, XmNleftAttachment, XmATTACH_POSITION,
	                                   XmNleftPosition, LIST_RIGHT, XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, RIGHT_MARGIN_POS, XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, inpBox, nullptr);
	XmStringFree(s1);

	ucd->mneTextW = XtVaCreateManagedWidget("mne", xmTextWidgetClass, form, XmNcolumns, 1, XmNmaxLength, 1, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, RIGHT_MARGIN_POS - 10, XmNrightAttachment, XmATTACH_POSITION,
	                                        XmNrightPosition, RIGHT_MARGIN_POS, XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, inpLabel, nullptr);
	RemapDeleteKey(ucd->mneTextW);

	ucd->accTextW = XtVaCreateManagedWidget("acc", xmTextWidgetClass, form, XmNcolumns, 12, XmNmaxLength, MAX_ACCEL_LEN - 1, XmNcursorPositionVisible, False, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, LIST_RIGHT,
	                                        XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, RIGHT_MARGIN_POS - 15, XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, inpLabel, nullptr);
	XtAddEventHandler(ucd->accTextW, KeyPressMask, False, (XtEventHandler)accKeyCB, ucd);
	XtAddCallback(ucd->accTextW, XmNfocusCallback, accFocusCB, ucd);
	XtAddCallback(ucd->accTextW, XmNlosingFocusCallback, accLoseFocusCB, ucd);
	accLabel = XtVaCreateManagedWidget("accLabel", xmLabelGadgetClass, form, XmNlabelString, s1 = XmStringCreateLtoREx("Accelerator"), XmNmnemonic, 'l', XmNuserData, ucd->accTextW, XmNalignment, XmALIGNMENT_BEGINNING, XmNmarginTop, 5,
	                                   XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, LIST_RIGHT, XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, LIST_RIGHT + 24, XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget,
	                                   ucd->mneTextW, nullptr);
	XmStringFree(s1);

	XtVaCreateManagedWidget("mneLabel", xmLabelGadgetClass, form, XmNlabelString, s1 = XmStringCreateLtoREx("Mnemonic"), XmNmnemonic, 'i', XmNuserData, ucd->mneTextW, XmNalignment, XmALIGNMENT_END, XmNmarginTop, 5, XmNleftAttachment,
	                        XmATTACH_POSITION, XmNleftPosition, LIST_RIGHT + 24, XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, RIGHT_MARGIN_POS, XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, ucd->mneTextW, nullptr);
	XmStringFree(s1);

	ucd->nameTextW = XtVaCreateManagedWidget("name", xmTextWidgetClass, form, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, LIST_RIGHT, XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, RIGHT_MARGIN_POS, XmNbottomAttachment,
	                                         XmATTACH_WIDGET, XmNbottomWidget, accLabel, nullptr);
	RemapDeleteKey(ucd->nameTextW);

	nameLabel = XtVaCreateManagedWidget("nameLabel", xmLabelGadgetClass, form, XmNlabelString, s1 = XmStringCreateLtoREx("Menu Entry"), XmNmnemonic, 'y', XmNuserData, ucd->nameTextW, XmNalignment, XmALIGNMENT_BEGINNING, XmNmarginTop, 5,
	                                    XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, LIST_RIGHT, XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, ucd->nameTextW, nullptr);
	XmStringFree(s1);

	XtVaCreateManagedWidget("nameNotes", xmLabelGadgetClass, form, XmNlabelString, s1 = XmStringCreateLtoREx("(> for sub-menu, @ language mode)"), XmNalignment, XmALIGNMENT_END, XmNmarginTop, 5, XmNleftAttachment, XmATTACH_WIDGET,
	                        XmNleftWidget, nameLabel, XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, RIGHT_MARGIN_POS, XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, ucd->nameTextW, nullptr);
	XmStringFree(s1);

	XtVaCreateManagedWidget("topLabel", xmLabelGadgetClass, form, XmNlabelString, s1 = XmStringCreateLtoREx("Select a shell menu item from the list at left.\n\
Select \"New\" to add a new command to the menu."),
	                        XmNtopAttachment, XmATTACH_POSITION, XmNtopPosition, 2, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, LIST_RIGHT, XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, RIGHT_MARGIN_POS,
	                        XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, nameLabel, nullptr);
	XmStringFree(s1);

	cmdLabel = XtVaCreateManagedWidget("cmdLabel", xmLabelGadgetClass, form, XmNlabelString, s1 = XmStringCreateLtoREx("Shell Command to Execute"), XmNmnemonic, 'x', XmNalignment, XmALIGNMENT_BEGINNING, XmNmarginTop, 5, XmNtopAttachment,
	                                   XmATTACH_POSITION, XmNtopPosition, SHELL_CMD_TOP, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, LEFT_MARGIN_POS, nullptr);
	XmStringFree(s1);
	XtVaCreateManagedWidget("cmdLabel", xmLabelGadgetClass, form, XmNlabelString, s1 = XmStringCreateLtoREx("(% expands to current filename, # to line number)"), XmNalignment, XmALIGNMENT_END, XmNmarginTop, 5, XmNtopAttachment,
	                        XmATTACH_POSITION, XmNtopPosition, SHELL_CMD_TOP, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, cmdLabel, XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, RIGHT_MARGIN_POS, nullptr);
	XmStringFree(s1);

	okBtn = XtVaCreateManagedWidget("ok", xmPushButtonWidgetClass, form, XmNlabelString, s1 = XmStringCreateLtoREx("OK"), XmNmarginWidth, BUTTON_WIDTH_MARGIN, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 13, XmNrightAttachment,
	                                XmATTACH_POSITION, XmNrightPosition, 29, XmNbottomAttachment, XmATTACH_POSITION, XmNbottomPosition, 99, nullptr);
	XtAddCallback(okBtn, XmNactivateCallback, okCB, ucd);
	XmStringFree(s1);

	applyBtn = XtVaCreateManagedWidget("apply", xmPushButtonWidgetClass, form, XmNlabelString, s1 = XmStringCreateLtoREx("Apply"), XmNmnemonic, 'A', XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 42, XmNrightAttachment,
	                                   XmATTACH_POSITION, XmNrightPosition, 58, XmNbottomAttachment, XmATTACH_POSITION, XmNbottomPosition, 99, nullptr);
	XtAddCallback(applyBtn, XmNactivateCallback, applyCB, ucd);
	XmStringFree(s1);

	closeBtn = XtVaCreateManagedWidget("close", xmPushButtonWidgetClass, form, XmNlabelString, s1 = XmStringCreateLtoREx("Close"), XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 71, XmNrightAttachment, XmATTACH_POSITION,
	                                   XmNrightPosition, 87, XmNbottomAttachment, XmATTACH_POSITION, XmNbottomPosition, 99, nullptr);
	XtAddCallback(closeBtn, XmNactivateCallback, closeCB, ucd);
	XmStringFree(s1);

	ac = 0;
	XtSetArg(args[ac], XmNeditMode, XmMULTI_LINE_EDIT);
	ac++;
	XtSetArg(args[ac], XmNscrollHorizontal, False);
	ac++;
	XtSetArg(args[ac], XmNwordWrap, True);
	ac++;
	XtSetArg(args[ac], XmNtopAttachment, XmATTACH_WIDGET);
	ac++;
	XtSetArg(args[ac], XmNtopWidget, cmdLabel);
	ac++;
	XtSetArg(args[ac], XmNleftAttachment, XmATTACH_POSITION);
	ac++;
	XtSetArg(args[ac], XmNleftPosition, LEFT_MARGIN_POS);
	ac++;
	XtSetArg(args[ac], XmNrightAttachment, XmATTACH_POSITION);
	ac++;
	XtSetArg(args[ac], XmNrightPosition, RIGHT_MARGIN_POS);
	ac++;
	XtSetArg(args[ac], XmNbottomAttachment, XmATTACH_WIDGET);
	ac++;
	XtSetArg(args[ac], XmNbottomWidget, okBtn);
	ac++;
	XtSetArg(args[ac], XmNbottomOffset, 5);
	ac++;
	ucd->cmdTextW = XmCreateScrolledText(form, (String) "name", args, ac);
	AddMouseWheelSupport(ucd->cmdTextW);
	XtManageChild(ucd->cmdTextW);
	MakeSingleLineTextW(ucd->cmdTextW);
	RemapDeleteKey(ucd->cmdTextW);
	XtVaSetValues(cmdLabel, XmNuserData, ucd->cmdTextW, nullptr); /* for mnemonic */

	/* Disable text input for the accelerator key widget, let the
	   event handler manage it instead */
	disableTextW(ucd->accTextW);

	/* initializs the dialog fields to match "New" list item */
	updateDialogFields(nullptr, ucd);

	/* Set initial default button */
	XtVaSetValues(form, XmNdefaultButton, okBtn, nullptr);
	XtVaSetValues(form, XmNcancelButton, closeBtn, nullptr);

	/* Handle mnemonic selection of buttons and focus to dialog */
	AddDialogMnemonicHandler(form, FALSE);

	/* realize all of the widgets in the new window */
	RealizeWithoutForcingPosition(ucd->dlogShell);
}

/*
** Present a dialogs for editing the user specified commands in the Macro
** and background menus
*/
void EditMacroMenu(Document *window) {
	editMacroOrBGMenu(window, MACRO_CMDS);
}
void EditBGMenu(Document *window) {
	editMacroOrBGMenu(window, BG_MENU_CMDS);
}

static void editMacroOrBGMenu(Document *window, int dialogType) {
	Widget form, accLabel, pasteReplayBtn;
	Widget nameLabel, cmdLabel, okBtn, applyBtn, closeBtn;
	char *title;
	XmString s1;
	int ac, i;
	Arg args[20];

	/* if the dialog is already displayed, just pop it to the top and return */
	if (dialogType == MACRO_CMDS && MacroCmdDialog) {
		RaiseDialogWindow(MacroCmdDialog);
		return;
	}
	if (dialogType == BG_MENU_CMDS && BGMenuCmdDialog) {
		RaiseDialogWindow(BGMenuCmdDialog);
		return;
	}

	/* Create a structure for keeping track of dialog state */
	auto ucd = new userCmdDialog;
	ucd->window = window;

	/* Set the dialog to operate on the Macro menu */
	ucd->menuItemsList = new menuItemRec *[MAX_ITEMS_PER_MENU];
	if (dialogType == MACRO_CMDS) {
		for (i = 0; i < NMacroMenuItems; i++)
			ucd->menuItemsList[i] = copyMenuItemRec(MacroMenuItems[i]);
		ucd->nMenuItems = NMacroMenuItems;
	} else { /* BG_MENU_CMDS */
		for (i = 0; i < NBGMenuItems; i++)
			ucd->menuItemsList[i] = copyMenuItemRec(BGMenuItems[i]);
		ucd->nMenuItems = NBGMenuItems;
	}
	ucd->dialogType = dialogType;

	title = dialogType == MACRO_CMDS ? (String) "Macro Commands" : (String) "Window Background Menu";
	ac = 0;
	XtSetArg(args[ac], XmNdeleteResponse, XmDO_NOTHING);
	ac++;
	XtSetArg(args[ac], XmNiconName, title);
	ac++;
	XtSetArg(args[ac], XmNtitle, title);
	ac++;
	ucd->dlogShell = CreateWidget(TheAppShell, "macros", topLevelShellWidgetClass, args, ac);
	AddSmallIcon(ucd->dlogShell);
	form = XtVaCreateManagedWidget("editMacroCommands", xmFormWidgetClass, ucd->dlogShell, XmNautoUnmanage, False, XmNresizePolicy, XmRESIZE_NONE, nullptr);
	XtAddCallback(form, XmNdestroyCallback, destroyCB, ucd);
	AddMotifCloseCallback(ucd->dlogShell, closeCB, ucd);

	ac = 0;
	XtSetArg(args[ac], XmNtopAttachment, XmATTACH_POSITION);
	ac++;
	XtSetArg(args[ac], XmNtopPosition, 2);
	ac++;
	XtSetArg(args[ac], XmNleftAttachment, XmATTACH_POSITION);
	ac++;
	XtSetArg(args[ac], XmNleftPosition, LEFT_MARGIN_POS);
	ac++;
	XtSetArg(args[ac], XmNrightAttachment, XmATTACH_POSITION);
	ac++;
	XtSetArg(args[ac], XmNrightPosition, LIST_RIGHT - 1);
	ac++;
	XtSetArg(args[ac], XmNbottomAttachment, XmATTACH_POSITION);
	ac++;
	XtSetArg(args[ac], XmNbottomPosition, MACRO_CMD_TOP);
	ac++;
	ucd->managedList = CreateManagedList(form, "list", args, ac, (void **)ucd->menuItemsList, &ucd->nMenuItems, MAX_ITEMS_PER_MENU, 20, getDialogDataCB, ucd, setDialogDataCB, ucd, freeItemCB);

	ucd->selInpBtn = XtVaCreateManagedWidget("selInpBtn", xmToggleButtonWidgetClass, form, XmNlabelString, s1 = XmStringCreateLtoREx("Requires Selection"), XmNmnemonic, 'R', XmNalignment, XmALIGNMENT_BEGINNING, XmNmarginHeight, 0, XmNset,
	                                         False, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, LIST_RIGHT, XmNbottomAttachment, XmATTACH_POSITION, XmNbottomPosition, MACRO_CMD_TOP, nullptr);
	XmStringFree(s1);

	ucd->mneTextW = XtVaCreateManagedWidget("mne", xmTextWidgetClass, form, XmNcolumns, 1, XmNmaxLength, 1, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, RIGHT_MARGIN_POS - 21 - 5, XmNrightAttachment, XmATTACH_POSITION,
	                                        XmNrightPosition, RIGHT_MARGIN_POS - 21, XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, ucd->selInpBtn, XmNbottomOffset, 5, nullptr);
	RemapDeleteKey(ucd->mneTextW);

	ucd->accTextW = XtVaCreateManagedWidget("acc", xmTextWidgetClass, form, XmNcolumns, 12, XmNmaxLength, MAX_ACCEL_LEN - 1, XmNcursorPositionVisible, False, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, LIST_RIGHT,
	                                        XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, RIGHT_MARGIN_POS - 20 - 10, XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, ucd->selInpBtn, XmNbottomOffset, 5, nullptr);
	XtAddEventHandler(ucd->accTextW, KeyPressMask, False, (XtEventHandler)accKeyCB, ucd);
	XtAddCallback(ucd->accTextW, XmNfocusCallback, accFocusCB, ucd);
	XtAddCallback(ucd->accTextW, XmNlosingFocusCallback, accLoseFocusCB, ucd);

	accLabel = XtVaCreateManagedWidget("accLabel", xmLabelGadgetClass, form, XmNlabelString, s1 = XmStringCreateLtoREx("Accelerator"), XmNmnemonic, 'l', XmNuserData, ucd->accTextW, XmNalignment, XmALIGNMENT_BEGINNING, XmNmarginTop, 5,
	                                   XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, LIST_RIGHT, XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, LIST_RIGHT + 22, XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget,
	                                   ucd->mneTextW, nullptr);
	XmStringFree(s1);

	XtVaCreateManagedWidget("mneLabel", xmLabelGadgetClass, form, XmNlabelString, s1 = XmStringCreateLtoREx("Mnemonic"), XmNmnemonic, 'i', XmNuserData, ucd->mneTextW, XmNalignment, XmALIGNMENT_END, XmNmarginTop, 5, XmNleftAttachment,
	                        XmATTACH_POSITION, XmNleftPosition, LIST_RIGHT + 22, XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, RIGHT_MARGIN_POS - 21, XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, ucd->mneTextW, nullptr);
	XmStringFree(s1);

	pasteReplayBtn = XtVaCreateManagedWidget("pasteReplay", xmPushButtonWidgetClass, form, XmNlabelString, s1 = XmStringCreateLtoREx("Paste Learn/\nReplay Macro"), XmNmnemonic, 'P', XmNsensitive, !GetReplayMacro().empty(), XmNleftAttachment,
	                                         XmATTACH_POSITION, XmNleftPosition, RIGHT_MARGIN_POS - 20, XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, RIGHT_MARGIN_POS, XmNbottomAttachment, XmATTACH_POSITION, XmNbottomPosition,
	                                         MACRO_CMD_TOP, nullptr);
	XtAddCallback(pasteReplayBtn, XmNactivateCallback, pasteReplayCB, ucd);
	XmStringFree(s1);

	ucd->nameTextW = XtVaCreateManagedWidget("name", xmTextWidgetClass, form, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, LIST_RIGHT, XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, RIGHT_MARGIN_POS, XmNbottomAttachment,
	                                         XmATTACH_WIDGET, XmNbottomWidget, accLabel, nullptr);
	RemapDeleteKey(ucd->nameTextW);

	nameLabel = XtVaCreateManagedWidget("nameLabel", xmLabelGadgetClass, form, XmNlabelString, s1 = XmStringCreateLtoREx("Menu Entry"), XmNmnemonic, 'y', XmNuserData, ucd->nameTextW, XmNalignment, XmALIGNMENT_BEGINNING, XmNmarginTop, 5,
	                                    XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, LIST_RIGHT, XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, ucd->nameTextW, nullptr);
	XmStringFree(s1);

	XtVaCreateManagedWidget("nameNotes", xmLabelGadgetClass, form, XmNlabelString, s1 = XmStringCreateLtoREx("(> for sub-menu, @ language mode)"), XmNalignment, XmALIGNMENT_END, XmNmarginTop, 5, XmNleftAttachment, XmATTACH_WIDGET,
	                        XmNleftWidget, nameLabel, XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, RIGHT_MARGIN_POS, XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, ucd->nameTextW, nullptr);
	XmStringFree(s1);

	XtVaCreateManagedWidget("topLabel", xmLabelGadgetClass, form, XmNlabelString, s1 = XmStringCreateLtoREx("Select a macro menu item from the list at left.\n\
Select \"New\" to add a new command to the menu."),
	                        XmNtopAttachment, XmATTACH_POSITION, XmNtopPosition, 2, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, LIST_RIGHT, XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, RIGHT_MARGIN_POS,
	                        XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, nameLabel, nullptr);
	XmStringFree(s1);

	cmdLabel = XtVaCreateManagedWidget("cmdLabel", xmLabelGadgetClass, form, XmNlabelString, s1 = XmStringCreateLtoREx("Macro Command to Execute"), XmNmnemonic, 'x', XmNalignment, XmALIGNMENT_BEGINNING, XmNmarginTop, 5, XmNtopAttachment,
	                                   XmATTACH_POSITION, XmNtopPosition, MACRO_CMD_TOP, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, LEFT_MARGIN_POS, nullptr);
	XmStringFree(s1);

	okBtn = XtVaCreateManagedWidget("ok", xmPushButtonWidgetClass, form, XmNlabelString, s1 = XmStringCreateLtoREx("OK"), XmNmarginWidth, BUTTON_WIDTH_MARGIN, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 8, XmNrightAttachment,
	                                XmATTACH_POSITION, XmNrightPosition, 23, XmNbottomAttachment, XmATTACH_POSITION, XmNbottomPosition, 99, nullptr);
	XtAddCallback(okBtn, XmNactivateCallback, okCB, ucd);
	XmStringFree(s1);

	applyBtn = XtVaCreateManagedWidget("apply", xmPushButtonWidgetClass, form, XmNlabelString, s1 = XmStringCreateLtoREx("Apply"), XmNmnemonic, 'A', XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 31, XmNrightAttachment,
	                                   XmATTACH_POSITION, XmNrightPosition, 46, XmNbottomAttachment, XmATTACH_POSITION, XmNbottomPosition, 99, nullptr);
	XtAddCallback(applyBtn, XmNactivateCallback, applyCB, ucd);
	XmStringFree(s1);

	applyBtn = XtVaCreateManagedWidget("check", xmPushButtonWidgetClass, form, XmNlabelString, s1 = XmStringCreateLtoREx("Check"), XmNmnemonic, 'C', XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 54, XmNrightAttachment,
	                                   XmATTACH_POSITION, XmNrightPosition, 69, XmNbottomAttachment, XmATTACH_POSITION, XmNbottomPosition, 99, nullptr);
	XtAddCallback(applyBtn, XmNactivateCallback, checkCB, ucd);
	XmStringFree(s1);

	closeBtn = XtVaCreateManagedWidget("close", xmPushButtonWidgetClass, form, XmNlabelString, s1 = XmStringCreateLtoREx("Close"), XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 77, XmNrightAttachment, XmATTACH_POSITION,
	                                   XmNrightPosition, 92, XmNbottomAttachment, XmATTACH_POSITION, XmNbottomPosition, 99, nullptr);
	XtAddCallback(closeBtn, XmNactivateCallback, closeCB, ucd);
	XmStringFree(s1);

	ac = 0;
	XtSetArg(args[ac], XmNeditMode, XmMULTI_LINE_EDIT);
	ac++;
	XtSetArg(args[ac], XmNtopAttachment, XmATTACH_WIDGET);
	ac++;
	XtSetArg(args[ac], XmNtopWidget, cmdLabel);
	ac++;
	XtSetArg(args[ac], XmNleftAttachment, XmATTACH_POSITION);
	ac++;
	XtSetArg(args[ac], XmNleftPosition, LEFT_MARGIN_POS);
	ac++;
	XtSetArg(args[ac], XmNrightAttachment, XmATTACH_POSITION);
	ac++;
	XtSetArg(args[ac], XmNrightPosition, RIGHT_MARGIN_POS);
	ac++;
	XtSetArg(args[ac], XmNbottomAttachment, XmATTACH_WIDGET);
	ac++;
	XtSetArg(args[ac], XmNbottomWidget, okBtn);
	ac++;
	XtSetArg(args[ac], XmNbottomOffset, 5);
	ac++;
	ucd->cmdTextW = XmCreateScrolledText(form, (String) "name", args, ac);
	AddMouseWheelSupport(ucd->cmdTextW);
	XtManageChild(ucd->cmdTextW);
	RemapDeleteKey(ucd->cmdTextW);
	XtVaSetValues(cmdLabel, XmNuserData, ucd->cmdTextW, nullptr); /* for mnemonic */

	/* Disable text input for the accelerator key widget, let the
	   event handler manage it instead */
	disableTextW(ucd->accTextW);

	/* initializs the dialog fields to match "New" list item */
	updateDialogFields(nullptr, ucd);

	/* Set initial default button */
	XtVaSetValues(form, XmNdefaultButton, okBtn, nullptr);
	XtVaSetValues(form, XmNcancelButton, closeBtn, nullptr);

	/* Handle mnemonic selection of buttons and focus to dialog */
	AddDialogMnemonicHandler(form, FALSE);

	/* Make widgets for top level shell and paste-replay buttons available
	   to other functions */
	if (dialogType == MACRO_CMDS) {
		MacroCmdDialog = ucd->dlogShell;
		MacroPasteReplayBtn = pasteReplayBtn;
	} else {
		BGMenuCmdDialog = ucd->dlogShell;
		BGMenuPasteReplayBtn = pasteReplayBtn;
	}

	/* Realize all of the widgets in the new dialog */
	RealizeWithoutForcingPosition(ucd->dlogShell);
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

		/* remember language mode assigned to shared user menus */
		window->userMenuCache_->umcLanguageMode = window->languageMode_;
	}

	/* update background menu, which is owned by a single document, only
	   if language mode was changed */
	if (window->userBGMenuCache_.ubmcLanguageMode != window->languageMode_) {
		updateMenu(window, BG_MENU_CMDS);

		/* remember language mode assigned to background menu */
		window->userBGMenuCache_.ubmcLanguageMode = window->languageMode_;
	}
}

/*
** Dim/undim buttons for pasting replay macros into macro and bg menu dialogs
*/
void DimPasteReplayBtns(int sensitive) {
	if(MacroCmdDialog)
		XtSetSensitive(MacroPasteReplayBtn, sensitive);
	if(BGMenuCmdDialog)
		XtSetSensitive(BGMenuPasteReplayBtn, sensitive);
}

/*
** Dim/undim user programmable menu items which depend on there being
** a selection in their associated window.
*/
void DimSelectionDepUserMenuItems(Document *window, int sensitive) {
	if (!window->IsTopDocument())
		return;

	dimSelDepItemsInMenu(window->shellMenuPane_, ShellMenuItems, NShellMenuItems, sensitive);
	dimSelDepItemsInMenu(window->macroMenuPane_, MacroMenuItems, NMacroMenuItems, sensitive);
	dimSelDepItemsInMenu(window->bgMenuPane_, BGMenuItems, NBGMenuItems, sensitive);
}

static void dimSelDepItemsInMenu(Widget menuPane, menuItemRec **menuList, int nMenuItems, int sensitive) {
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
				dimSelDepItemsInMenu(subMenu, menuList, nMenuItems, sensitive);
			} else {
				index = (long)userData - 10;
				if (index < 0 || index >= nMenuItems)
					return;
				if (menuList[index]->input == FROM_SELECTION)
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
nullable_string WriteShellCmdsStringEx(void) {
	return writeMenuItemStringEx(ShellMenuItems, NShellMenuItems, SHELL_CMDS);
}

/*
** Generate a text string for the preferences file describing the contents of
** the macro menu and background menu commands lists.  These strings are not
** exactly of the form that it can be read by LoadMacroCmdsString, rather, it
** is what needs to be written to a resource file such that it will read back
** in that form.
*/

nullable_string WriteMacroCmdsStringEx(void) {
	return writeMenuItemStringEx(MacroMenuItems, NMacroMenuItems, MACRO_CMDS);
}

nullable_string WriteBGMenuCmdsStringEx(void) {
	return writeMenuItemStringEx(BGMenuItems, NBGMenuItems, BG_MENU_CMDS);
}

/*
** Read a string representing shell command menu items and add them to the
** internal list used for constructing shell menus
*/
int LoadShellCmdsString(const char *inString) {
	return loadMenuItemString(inString, ShellMenuItems, &NShellMenuItems, SHELL_CMDS);
}

/*
** Read a string representing shell command menu items and add them to the
** internal list used for constructing shell menus
*/
int LoadShellCmdsStringEx(view::string_view inString) {
	// TODO(eteran): make this more efficient
	return loadMenuItemString(inString.to_string().c_str(), ShellMenuItems, &NShellMenuItems, SHELL_CMDS);
}

/*
** Read strings representing macro menu or background menu command menu items
** and add them to the internal lists used for constructing menus
*/
int LoadMacroCmdsString(const char *inString) {
	return loadMenuItemString(inString, MacroMenuItems, &NMacroMenuItems, MACRO_CMDS);
}

int LoadMacroCmdsStringEx(view::string_view inString) {
	// TODO(eteran): make this more efficient
	return loadMenuItemString(inString.to_string().c_str(), MacroMenuItems, &NMacroMenuItems, MACRO_CMDS);
}

int LoadBGMenuCmdsString(const char *inString) {
	return loadMenuItemString(inString, BGMenuItems, &NBGMenuItems, BG_MENU_CMDS);
}

int LoadBGMenuCmdsStringEx(view::string_view inString) {
	// TODO(eteran): make this more efficient
	return loadMenuItemString(inString.to_string().c_str(), BGMenuItems, &NBGMenuItems, BG_MENU_CMDS);
}

/*
** Cache user menus:
** Setup user menu info after read of macro, shell and background menu
** string (reason: language mode info from preference string is read *after*
** user menu preference string was read).
*/
void SetupUserMenuInfo(void) {
	parseMenuItemList(ShellMenuItems, NShellMenuItems, ShellMenuInfo, &ShellSubMenus);
	parseMenuItemList(MacroMenuItems, NMacroMenuItems, MacroMenuInfo, &MacroSubMenus);
	parseMenuItemList(BGMenuItems, NBGMenuItems, BGMenuInfo, &BGSubMenus);
}

/*
** Cache user menus:
** Update user menu info to take into account e.g. change of language modes
** (i.e. add / move / delete of language modes etc).
*/
void UpdateUserMenuInfo(void) {
	freeUserMenuInfoList(ShellMenuInfo, NShellMenuItems);
	freeSubMenuCache(&ShellSubMenus);
	parseMenuItemList(ShellMenuItems, NShellMenuItems, ShellMenuInfo, &ShellSubMenus);

	freeUserMenuInfoList(MacroMenuInfo, NMacroMenuItems);
	freeSubMenuCache(&MacroSubMenus);
	parseMenuItemList(MacroMenuItems, NMacroMenuItems, MacroMenuInfo, &MacroSubMenus);

	freeUserMenuInfoList(BGMenuInfo, NBGMenuItems);
	freeSubMenuCache(&BGSubMenus);
	parseMenuItemList(BGMenuItems, NBGMenuItems, BGMenuInfo, &BGSubMenus);
}

/*
** Search through the shell menu and execute the first command with menu item
** name "itemName".  Returns True on successs and False on failure.
*/
int DoNamedShellMenuCmd(Document *window, const char *itemName, int fromMacro) {
	int i;

	for (i = 0; i < NShellMenuItems; i++) {
		if (ShellMenuItems[i]->name == itemName) {
			if (ShellMenuItems[i]->output == TO_SAME_WINDOW && CheckReadOnly(window))
				return False;
			DoShellMenuCmd(window, ShellMenuItems[i]->cmd, ShellMenuItems[i]->input, ShellMenuItems[i]->output, ShellMenuItems[i]->repInput, ShellMenuItems[i]->saveFirst, ShellMenuItems[i]->loadAfter, fromMacro);
			return True;
		}
	}
	return False;
}

/*
** Search through the Macro or background menu and execute the first command
** with menu item name "itemName".  Returns True on successs and False on
** failure.
*/
int DoNamedMacroMenuCmd(Document *window, const char *itemName) {
	int i;

	for (i = 0; i < NMacroMenuItems; i++) {
		if (MacroMenuItems[i]->name == itemName) {
			DoMacro(window, MacroMenuItems[i]->cmd, "macro menu command");
			return True;
		}
	}
	return False;
}

int DoNamedBGMenuCmd(Document *window, const char *itemName) {
	int i;

	for (i = 0; i < NBGMenuItems; i++) {
		if (BGMenuItems[i]->name == itemName) {
			DoMacro(window, BGMenuItems[i]->cmd, "background menu macro");
			return True;
		}
	}
	return False;
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
static void rebuildMenuOfAllWindows(int menuType) {

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

	/* Fetch the appropriate menu data */
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

	/* destroy all widgets related to menu pane */
	deleteMenuItems(menu.sumMenuPane);

	/* remove cached user menu info */
	freeUserMenuList(menu.sumMainMenuList);
	*menu.sumMenuCreated = False;

	/* re-create & cache user menu items */
	updateMenu(window, menuType);
}

/*
** Fetch the appropriate menu info for given menu type
*/
static void selectUserMenu(Document *window, int menuType, selectedUserMenu *menu) {
	if (menuType == SHELL_CMDS) {
		menu->sumMenuPane = window->shellMenuPane_;
		menu->sumNbrOfListItems = NShellMenuItems;
		menu->sumItemList = ShellMenuItems;
		menu->sumInfoList = ShellMenuInfo;
		menu->sumSubMenus = &ShellSubMenus;
		menu->sumMainMenuList = &window->userMenuCache_->umcShellMenuList;
		menu->sumMenuCreated = &window->userMenuCache_->umcShellMenuCreated;
	} else if (menuType == MACRO_CMDS) {
		menu->sumMenuPane = window->macroMenuPane_;
		menu->sumNbrOfListItems = NMacroMenuItems;
		menu->sumItemList = MacroMenuItems;
		menu->sumInfoList = MacroMenuInfo;
		menu->sumSubMenus = &MacroSubMenus;
		menu->sumMainMenuList = &window->userMenuCache_->umcMacroMenuList;
		menu->sumMenuCreated = &window->userMenuCache_->umcMacroMenuCreated;
	} else { /* BG_MENU_CMDS */
		menu->sumMenuPane = window->bgMenuPane_;
		menu->sumNbrOfListItems = NBGMenuItems;
		menu->sumItemList = BGMenuItems;
		menu->sumInfoList = BGMenuInfo;
		menu->sumSubMenus = &BGSubMenus;
		menu->sumMainMenuList = &window->userBGMenuCache_.ubmcMenuList;
		menu->sumMenuCreated = &window->userBGMenuCache_.ubmcMenuCreated;
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

	/* Fetch the appropriate menu data */
	selectUserMenu(window, menuType, &menu);

	/* Set / reset "to be managed" flag of all info list items */
	applyLangModeToUserMenuInfo(menu.sumInfoList, menu.sumNbrOfListItems, window->languageMode_);

	/* create user menu items, if not done before */
	if (!*menu.sumMenuCreated)
		createMenuItems(window, &menu);

	/* manage user menu items depending on current language mode */
	manageUserMenu(&menu, window);

	if (menuType == BG_MENU_CMDS) {
		/* Set the proper sensitivity of items which may be dimmed */
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

	for (int i = 0; i < list->umlNbrItems; i++) {
		UserMenuListElement *element = list->umlItems[i];

		/* remember current manage mode before reset it to
		   "unmanaged" */
		element->umlePrevManageMode = element->umleManageMode;
		element->umleManageMode = UMMM_UNMANAGE;

		/* recursively reset manage mode of sub-menus */
		if (element->umleSubMenuList)
			resetManageMode(element->umleSubMenuList);
	}
}

/*
** Cache user menus:
** Manage all menu widgets of given user sub-menu list.
*/
static void manageAllSubMenuWidgets(UserMenuListElement *subMenu) {
	int i;
	UserMenuList *subMenuList;
	UserMenuListElement *element;
	WidgetList widgetList;
	Cardinal nWidgetListItems;

	/* if the sub-menu is torn off, unmanage the menu pane
	   before updating it to prevent the tear-off menu
	   from shrinking and expanding as the menu entries
	   are (un)managed */
	if (!XmIsMenuShell(XtParent(subMenu->umleSubMenuPane))) {
		XtUnmanageChild(subMenu->umleSubMenuPane);
	}

	/* manage all children of sub-menu pane */
	XtVaGetValues(subMenu->umleSubMenuPane, XmNchildren, &widgetList, XmNnumChildren, &nWidgetListItems, nullptr);
	XtManageChildren(widgetList, nWidgetListItems);

	/* scan, if an menu item of given sub-menu holds a nested
	   sub-menu */
	subMenuList = subMenu->umleSubMenuList;

	for (i = 0; i < subMenuList->umlNbrItems; i++) {
		element = subMenuList->umlItems[i];

		if (element->umleSubMenuList) {
			/* if element is a sub-menu, then continue managing
			   all items of that sub-menu recursively */
			manageAllSubMenuWidgets(element);
		}
	}

	/* manage sub-menu pane widget itself */
	XtManageChild(subMenu->umleMenuItem);

	/* if the sub-menu is torn off, then adjust & manage the menu */
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
	int i;
	Widget shell;
	UserMenuList *subMenuList;
	UserMenuListElement *element;
	WidgetList widgetList;
	Cardinal nWidgetListItems;

	/* if sub-menu is torn-off, then unmap its shell
	   (so tearoff window isn't displayed anymore) */
	shell = XtParent(subMenu->umleSubMenuPane);
	if (!XmIsMenuShell(shell)) {
		XtUnmapWidget(shell);
	}

	/* unmanage all children of sub-menu pane */
	XtVaGetValues(subMenu->umleSubMenuPane, XmNchildren, &widgetList, XmNnumChildren, &nWidgetListItems, nullptr);
	XtUnmanageChildren(widgetList, nWidgetListItems);

	/* scan, if an menu item of given sub-menu holds a nested
	   sub-menu */
	subMenuList = subMenu->umleSubMenuList;

	for (i = 0; i < subMenuList->umlNbrItems; i++) {
		element = subMenuList->umlItems[i];

		if (element->umleSubMenuList) {
			/* if element is a sub-menu, then continue unmanaging
			   all items of that sub-menu recursively */
			unmanageAllSubMenuWidgets(element);
		}
	}

	/* unmanage sub-menu pane widget itself */
	XtUnmanageChild(subMenu->umleMenuItem);
}

/*
** Cache user menus:
** Manage / unmanage menu widgets according to given user menu list.
*/
static void manageMenuWidgets(UserMenuList *list) {
	int i;
	UserMenuListElement *element;

	/* (un)manage all elements of given user menu list */
	for (i = 0; i < list->umlNbrItems; i++) {
		element = list->umlItems[i];

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

					/* (un)manage menu entries of sub-menu */
					manageMenuWidgets(element->umleSubMenuList);

					/* if the sub-menu is torn off, then adjust & manage the menu */
					if (!XmIsMenuShell(XtParent(element->umleSubMenuPane))) {
						manageTearOffMenu(element->umleSubMenuPane);
					}

					/* if the sub-menu was torn off then redisplay it */
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

	/* scan all elements of this (sub-)menu */
	for (int i = 0; i < menuList->umlNbrItems; i++) {
		UserMenuListElement *element = menuList->umlItems[i];

		if (element->umleSubMenuList) {
			/* if element is a sub-menu, then continue removing accelerators
			   from all items of that sub-menu recursively */
			removeAccelFromMenuWidgets(element->umleSubMenuList);
		} else if (element->umleAccKeys != nullptr && element->umleManageMode == UMMM_UNMANAGE && element->umlePrevManageMode == UMMM_MANAGE) {
			/* remove accelerator if one was bound */
			XtVaSetValues(element->umleMenuItem, XmNaccelerator, nullptr, nullptr);
		}
	}
}

/*
** Cache user menus:
** Assign accelerators to all managed items of given user (sub-)menu list.
*/
static void assignAccelToMenuWidgets(UserMenuList *menuList, Document *window) {
	int i;
	UserMenuListElement *element;

	/* scan all elements of this (sub-)menu */
	for (i = 0; i < menuList->umlNbrItems; i++) {
		element = menuList->umlItems[i];

		if (element->umleSubMenuList) {
			/* if element is a sub-menu, then continue assigning accelerators
			   to all managed items of that sub-menu recursively */
			assignAccelToMenuWidgets(element->umleSubMenuList, window);
		} else if (element->umleAccKeys != nullptr && element->umleManageMode == UMMM_MANAGE && element->umlePrevManageMode == UMMM_UNMANAGE) {
			/* assign accelerator if applicable */
			XtVaSetValues(element->umleMenuItem, XmNaccelerator, element->umleAccKeys, nullptr);
			if (!element->umleAccLockPatchApplied) {
				UpdateAccelLockPatch(window->splitPane_, element->umleMenuItem);
				element->umleAccLockPatchApplied = True;
			}
		}
	}
}

/*
** Cache user menus:
** (Un)Manage all items of selected user menu.
*/
static void manageUserMenu(selectedUserMenu *menu, Document *window) {
	int n, i;
	int *id;
	Boolean currentLEisSubMenu;
	userMenuInfo *info;
	UserMenuList *menuList;
	UserMenuListElement *currentLE;
	UserMenuManageMode *mode;

	/* reset manage mode of all items of selected user menu in window cache */
	resetManageMode(menu->sumMainMenuList);

	/* set manage mode of all items of selected user menu in window cache
	   according to the "to be managed" indication of the info list */
	for (n = 0; n < menu->sumNbrOfListItems; n++) {
		info = menu->sumInfoList[n];

		menuList = menu->sumMainMenuList;
		id = info->umiId;

		/* select all menu list items belonging to menu record "info" using
		   hierarchical ID of current menu info (e.g. id = {3} means:
		   4th element of main menu; {0} = 1st element etc.)*/
		for (i = 0; i < info->umiIdLen; i++) {
			currentLE = menuList->umlItems[*id];
			mode = &currentLE->umleManageMode;
			currentLEisSubMenu = (currentLE->umleSubMenuList != nullptr);

			if (info->umiToBeManaged) {
				/* menu record needs to be managed: */
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
				/* menu record needs to be unmanaged: */
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

	/* if the menu is torn off, then adjust & manage the menu */
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
	menuItemRec *item;
	int nTreeEntries;
	char *namePtr, *subSep, *fullName;
	int menuType = menu->sumType;
	userMenuInfo *info;
	userSubMenuCache *subMenus = menu->sumSubMenus;
	UserMenuList *menuList;
	UserMenuListElement *currentLE;
	int subMenuDepth;
	char accKeysBuf[MAX_ACCEL_LEN + 5];
	char *accKeys;

	/* Allocate storage for structures to help find panes of sub-menus */
	auto menuTree = new menuTreeItem[menu->sumNbrOfListItems];
	nTreeEntries = 0;

	/* Harmless kludge: undo and redo items are marked specially if found
	   in the background menu, and used to dim/undim with edit menu */
	window->bgMenuUndoItem_ = nullptr;
	window->bgMenuRedoItem_ = nullptr;

	/*
	** Add items to the menu pane, creating hierarchical sub-menus as
	** necessary
	*/
	allocUserMenuList(menu->sumMainMenuList, subMenus->usmcNbrOfMainMenuItems);
	for (n = 0; n < menu->sumNbrOfListItems; n++) {
		item = menu->sumItemList[n];
		info = menu->sumInfoList[n];
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
				
				if(menuType == SHELL_CMDS) {
					btn = createUserMenuItem(subPane, namePtr, item, n, shellMenuCB, (XtPointer)window);
				} else if(menuType == MACRO_CMDS) {
					btn = createUserMenuItem(subPane, namePtr, item, n, macroMenuCB, (XtPointer)window);
				} else {
					btn = createUserMenuItem(subPane, namePtr, item, n, bgMenuCB, (XtPointer)window);
				}
				
				if (menuType == BG_MENU_CMDS && !strcmp(item->cmd, "undo()\n"))
					window->bgMenuUndoItem_ = btn;
				else if (menuType == BG_MENU_CMDS && !strcmp(item->cmd, "redo()\n"))
					window->bgMenuRedoItem_ = btn;
				/* generate accelerator keys */
				genAccelEventName(accKeysBuf, item->modifiers, item->keysym);
				accKeys = item->keysym == NoSymbol ? nullptr : XtNewStringEx(accKeysBuf);
				/* create corresponding menu list item */
				menuList->umlItems[menuList->umlNbrItems++] = allocUserMenuListElement(btn, accKeys);
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

				currentLE = allocUserMenuListElement(btn, nullptr);
				menuList->umlItems[menuList->umlNbrItems++] = currentLE;
				currentLE->umleSubMenuPane = newSubPane;
				currentLE->umleSubMenuList = allocUserSubMenuList(subMenuInfo->usmiId[subMenuInfo->usmiIdLen]);
			} else {
				currentLE = menuList->umlItems[subMenuInfo->usmiId[subMenuDepth]];
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
static Widget findInMenuTreeEx(menuTreeItem *menuTree, int nTreeEntries, const XString &hierName) {

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

static XString copySubstringEx(const char *string, int length) {
	return XString(string, length);
}

static Widget createUserMenuItem(Widget menuPane, char *name, menuItemRec *f, int index, XtCallbackProc cbRtn, XtPointer cbArg) {
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
static Widget createUserSubMenuEx(Widget parent, const XString &label, Widget *menuItem) {
	Widget menuPane;	
	static Arg args[1] = {{XmNuserData, (XtArgVal)TEMPORARY_MENU_ITEM}};

	menuPane = CreatePulldownMenu(parent, (String) "userPulldown", args, 1);
	
	XmString st1 = label.toXmString();
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

	/* Fetch the list of children from the menu pane to delete */
	XtVaGetValues(menuPane, XmNchildren, &itemList, XmNnumChildren, &nItems, nullptr);

	/* make a copy because the widget alters the list as you delete widgets */
	auto items = new Widget[nItems];
	std::copy_n(itemList, nItems, items);

	/* delete all of the widgets not marked as PERMANENT_MENU_ITEM */
	for (size_t n = 0; n < nItems; n++) {
		XtVaGetValues(items[n], XmNuserData, &userData, nullptr);
		if (userData != (XtPointer)PERMANENT_MENU_ITEM) {
			if (XtClass(items[n]) == xmCascadeButtonWidgetClass) {
				XtVaGetValues(items[n], XmNsubMenuId, &subMenuID, nullptr);

				/* prevent dangling submenu tearoffs */
				if (!XmIsMenuShell(XtParent(subMenuID)))
					_XmDismissTearOff(XtParent(subMenuID), nullptr, nullptr);

				deleteMenuItems(subMenuID);
			} else {
				/* remove accel. before destroy or lose it forever */
				XtVaSetValues(items[n], XmNaccelerator, nullptr, nullptr);
			}
			XtDestroyWidget(items[n]);
		}
	}
	
	delete [] items;
}

static void closeCB(Widget w, XtPointer clientData, XtPointer callData) {
	(void)w;
	(void)callData;

	auto ucd = static_cast<userCmdDialog *>(clientData);

	/* Mark that there's no longer a (macro, bg, or shell) dialog up */
	if (ucd->dialogType == SHELL_CMDS)
		ShellCmdDialog = nullptr;
	else if (ucd->dialogType == MACRO_CMDS)
		MacroCmdDialog = nullptr;
	else
		BGMenuCmdDialog = nullptr;

	/* pop down and destroy the dialog (memory for ucd is freed in the
	   destroy callback) */
	XtDestroyWidget(ucd->dlogShell);
}

static void okCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)callData;

	auto ucd = static_cast<userCmdDialog *>(clientData);

	/* Read the dialog fields, and update the menus */
	if (!applyDialogChanges(ucd))
		return;

	/* Mark that there's no longer a (macro, bg, or shell) dialog up */
	if (ucd->dialogType == SHELL_CMDS)
		ShellCmdDialog = nullptr;
	else if (ucd->dialogType == MACRO_CMDS)
		MacroCmdDialog = nullptr;
	else
		BGMenuCmdDialog = nullptr;

	/* pop down and destroy the dialog (memory for ucd is freed in the
	   destroy callback) */
	XtDestroyWidget(ucd->dlogShell);
}

static void applyCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)callData;

	applyDialogChanges((userCmdDialog *)clientData);
}

static void checkCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)callData;

	auto ucd = static_cast<userCmdDialog *>(clientData);

	if (checkMacro(ucd)) {
		DialogF(DF_INF, ucd->dlogShell, 1, "Macro", "Macro compiled without error", "OK");
	}
}

static int checkMacro(userCmdDialog *ucd) {
	menuItemRec *f;

	f = readDialogFields(ucd, False);
	if(!f)
		return False;
	if (!checkMacroText(f->cmd, ucd->dlogShell, ucd->cmdTextW)) {
		freeMenuItemRec(f);
		return False;
	}
	return True;
}

static int checkMacroText(const char *macro, Widget errorParent, Widget errFocus) {
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

static int applyDialogChanges(userCmdDialog *ucd) {
	int i;

	/* Get the current contents of the dialog fields */
	if (!UpdateManagedList(ucd->managedList, True))
		return False;

	/* Test compile the macro */
	if (ucd->dialogType == MACRO_CMDS)
		if (!checkMacro(ucd))
			return False;

	/* Update the menu information */
	if (ucd->dialogType == SHELL_CMDS) {
		for (i = 0; i < NShellMenuItems; i++)
			freeMenuItemRec(ShellMenuItems[i]);
		freeUserMenuInfoList(ShellMenuInfo, NShellMenuItems);
		freeSubMenuCache(&ShellSubMenus);
		for (i = 0; i < ucd->nMenuItems; i++)
			ShellMenuItems[i] = copyMenuItemRec(ucd->menuItemsList[i]);
		NShellMenuItems = ucd->nMenuItems;
		parseMenuItemList(ShellMenuItems, NShellMenuItems, ShellMenuInfo, &ShellSubMenus);
	} else if (ucd->dialogType == MACRO_CMDS) {
		for (i = 0; i < NMacroMenuItems; i++)
			freeMenuItemRec(MacroMenuItems[i]);
		freeUserMenuInfoList(MacroMenuInfo, NMacroMenuItems);
		freeSubMenuCache(&MacroSubMenus);
		for (i = 0; i < ucd->nMenuItems; i++)
			MacroMenuItems[i] = copyMenuItemRec(ucd->menuItemsList[i]);
		NMacroMenuItems = ucd->nMenuItems;
		parseMenuItemList(MacroMenuItems, NMacroMenuItems, MacroMenuInfo, &MacroSubMenus);
	} else { /* BG_MENU_CMDS */
		for (i = 0; i < NBGMenuItems; i++)
			freeMenuItemRec(BGMenuItems[i]);
		freeUserMenuInfoList(BGMenuInfo, NBGMenuItems);
		freeSubMenuCache(&BGSubMenus);
		for (i = 0; i < ucd->nMenuItems; i++)
			BGMenuItems[i] = copyMenuItemRec(ucd->menuItemsList[i]);
		NBGMenuItems = ucd->nMenuItems;
		parseMenuItemList(BGMenuItems, NBGMenuItems, BGMenuInfo, &BGSubMenus);
	}

	/* Update the menus themselves in all of the NEdit windows */
	rebuildMenuOfAllWindows(ucd->dialogType);

	/* Note that preferences have been changed */
	MarkPrefsChanged();
	return True;
}

static void pasteReplayCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)callData;
	auto ucd = static_cast<userCmdDialog *>(clientData);

	if (GetReplayMacro().empty())
		return;

	XmTextInsert(ucd->cmdTextW, XmTextGetInsertionPosition(ucd->cmdTextW), (String)GetReplayMacro().c_str());
}

static void destroyCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)callData;

	auto ucd = static_cast<userCmdDialog *>(clientData);

	for (int i = 0; i < ucd->nMenuItems; i++) {
		freeMenuItemRec(ucd->menuItemsList[i]);
	}
	
	delete [] ucd->menuItemsList;
	delete ucd;
}

static void accFocusCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)callData;

	auto ucd = static_cast<userCmdDialog *>(clientData);

	RemoveDialogMnemonicHandler(XtParent(ucd->accTextW));
}

static void accLoseFocusCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)callData;

	auto ucd = static_cast<userCmdDialog *>(clientData);

	AddDialogMnemonicHandler(XtParent(ucd->accTextW), FALSE);
}

static void accKeyCB(Widget w, XtPointer clientData, XKeyEvent *event) {

	(void)w;

	auto ucd = static_cast<userCmdDialog *>(clientData);
	KeySym keysym = XLookupKeysym(event, 0);
	char outStr[MAX_ACCEL_LEN];

	/* Accept only real keys, not modifiers alone */
	if (IsModifierKey(keysym))
		return;

	/* Tab key means go to the next field, don't enter */
	if (keysym == XK_Tab)
		return;

	/* Beep and return if the modifiers are buttons or ones we don't support */
	if (event->state & ~(ShiftMask | LockMask | ControlMask | Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask)) {
		XBell(TheDisplay, 0);
		return;
	}

	/* Delete or backspace clears field */
	if (keysym == XK_Delete || keysym == XK_BackSpace) {
		XmTextSetStringEx(ucd->accTextW, "");
		return;
	}

	/* generate the string to use in the dialog field */
	generateAcceleratorString(outStr, event->state, keysym);

	/* Reject single character accelerators (a very simple way to eliminate
	   un-modified letters and numbers)  The goal is give users a clue that
	   they're supposed to type the actual keys, not the name.  This scheme
	   is not rigorous and still allows accelerators like Comma. */
	if (strlen(outStr) == 1) {
		XBell(TheDisplay, 0);
		return;
	}

	/* fill in the accelerator field in the dialog */
	XmTextSetStringEx(ucd->accTextW, outStr);
}

static void sameOutCB(Widget w, XtPointer clientData, XtPointer callData) {
	(void)callData;
	XtSetSensitive(((userCmdDialog *)clientData)->repInpBtn, XmToggleButtonGetState(w));
}

static void shellMenuCB(Widget w, XtPointer clientData, XtPointer callData) {

	auto window = static_cast<Document *>(clientData);

	XtArgVal userData;
	int index;
	char *params[1];

	window = Document::WidgetToWindow(MENU_WIDGET(w));

	/* get the index of the shell command and verify that it's in range */
	XtVaGetValues(w, XmNuserData, &userData, nullptr);
	index = (int)userData - 10;
	if (index < 0 || index >= NShellMenuItems)
		return;

	params[0] = ShellMenuItems[index]->name.str();
	XtCallActionProc(window->lastFocus_, "shell_menu_command", ((XmAnyCallbackStruct *)callData)->event, params, 1);
}

static void macroMenuCB(Widget w, XtPointer clientData, XtPointer callData) {

	auto window = static_cast<Document *>(clientData);

	XtArgVal userData;
	int index;
	char *params[1];

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
		XBell(TheDisplay, 0);
		return;
	}

	/* get the index of the macro command and verify that it's in range */
	XtVaGetValues(w, XmNuserData, &userData, nullptr);
	index = (int)userData - 10;
	if (index < 0 || index >= NMacroMenuItems)
		return;

	params[0] = MacroMenuItems[index]->name.str();
	XtCallActionProc(window->lastFocus_, "macro_menu_command", ((XmAnyCallbackStruct *)callData)->event, params, 1);
}

static void bgMenuCB(Widget w, XtPointer clientData, XtPointer callData) {

	auto window = static_cast<Document *>(clientData);

	XtArgVal userData;
	int index;
	char *params[1];

	/* Same remark as for macro menu commands (see above). */
	if (window->macroCmdData_) {
		XBell(TheDisplay, 0);
		return;
	}

	/* get the index of the macro command and verify that it's in range */
	XtVaGetValues(w, XmNuserData, &userData, nullptr);
	index = (int)userData - 10;
	if (index < 0 || index >= NBGMenuItems)
		return;

	params[0] = BGMenuItems[index]->name.str();
	XtCallActionProc(window->lastFocus_, "bg_menu_command", ((XmAnyCallbackStruct *)callData)->event, params, 1);
}

/*
** Update the name, accelerator, mnemonic, and command fields in the shell
** command or macro dialog to agree with the currently selected item in the
** menu item list.
*/
static void updateDialogFields(menuItemRec *f, userCmdDialog *ucd) {
	char mneString[2], accString[MAX_ACCEL_LEN];

	/* fill in the name, accelerator, mnemonic, and command fields of the
	   dialog for the newly selected item, or blank them if "New" is selected */
	if(!f) {
		XmTextSetStringEx(ucd->nameTextW, "");
		XmTextSetStringEx(ucd->cmdTextW, "");
		XmTextSetStringEx(ucd->accTextW, "");
		XmTextSetStringEx(ucd->mneTextW, "");
		if (ucd->dialogType == SHELL_CMDS) {
			RadioButtonChangeState(ucd->selInpBtn, True, True);
			RadioButtonChangeState(ucd->sameOutBtn, True, True);
			RadioButtonChangeState(ucd->repInpBtn, False, False);
			XtSetSensitive(ucd->repInpBtn, True);
			RadioButtonChangeState(ucd->saveFirstBtn, False, False);
			RadioButtonChangeState(ucd->loadAfterBtn, False, False);
		}
	} else {
		mneString[0] = f->mnemonic;
		mneString[1] = '\0';
		generateAcceleratorString(accString, f->modifiers, f->keysym);
		
		XmTextSetStringEx(ucd->nameTextW, f->name.str());
		XmTextSetStringEx(ucd->cmdTextW,  f->cmd);
		XmTextSetStringEx(ucd->accTextW,  accString);
		XmTextSetStringEx(ucd->mneTextW,  mneString);
		
		RadioButtonChangeState(ucd->selInpBtn, f->input == FROM_SELECTION, False);
		if (ucd->dialogType == SHELL_CMDS) {
			RadioButtonChangeState(ucd->winInpBtn, f->input == FROM_WINDOW, False);
			RadioButtonChangeState(ucd->eitherInpBtn, f->input == FROM_EITHER, False);
			RadioButtonChangeState(ucd->noInpBtn, f->input == FROM_NONE, False);
			RadioButtonChangeState(ucd->sameOutBtn, f->output == TO_SAME_WINDOW, False);
			RadioButtonChangeState(ucd->winOutBtn, f->output == TO_NEW_WINDOW, False);
			RadioButtonChangeState(ucd->dlogOutBtn, f->output == TO_DIALOG, False);
			RadioButtonChangeState(ucd->repInpBtn, f->repInput, False);
			XtSetSensitive(ucd->repInpBtn, f->output == TO_SAME_WINDOW);
			RadioButtonChangeState(ucd->saveFirstBtn, f->saveFirst, False);
			RadioButtonChangeState(ucd->loadAfterBtn, f->loadAfter, False);
		}
	}
}

/*
** Read the name, accelerator, mnemonic, and command fields from the shell or
** macro commands dialog into a newly allocated menuItemRec.  Returns a
** pointer to the new menuItemRec structure as the function value, or nullptr on
** failure.
*/
static menuItemRec *readDialogFields(userCmdDialog *ucd, int silent) {
	char *cmdText, *mneText, *accText;

	auto nameTextTemp = XmTextGetString(ucd->nameTextW);
	auto nameText = XString::takeString(nameTextTemp);
	
	if (nameText.empty()) {
		if (!silent) {
			DialogF(DF_WARN, ucd->dlogShell, 1, "Menu Entry", "Please specify a name\nfor the menu item", "OK");
			XmProcessTraversal(ucd->nameTextW, XmTRAVERSE_CURRENT);
		}
		return nullptr;
	}

	if (strchr(nameText.str(), ':')) {
		if (!silent) {
			DialogF(DF_WARN, ucd->dlogShell, 1, "Menu Entry", "Menu item names may not\ncontain colon (:) characters", "OK");
			XmProcessTraversal(ucd->nameTextW, XmTRAVERSE_CURRENT);
		}
		return nullptr;
	}

	cmdText = XmTextGetString(ucd->cmdTextW);
	if (cmdText == nullptr || *cmdText == '\0') {
		if (!silent) {
			DialogF(DF_WARN, ucd->dlogShell, 1, "Command to Execute", "Please specify %s to execute", "OK", ucd->dialogType == SHELL_CMDS ? "shell command" : "macro command(s)");
			XmProcessTraversal(ucd->cmdTextW, XmTRAVERSE_CURRENT);
		}
		XtFree(cmdText);

		return nullptr;
	}

	if (ucd->dialogType == MACRO_CMDS || ucd->dialogType == BG_MENU_CMDS) {
		cmdText = addTerminatingNewlineEx(cmdText);
		if (!checkMacroText(cmdText, silent ? nullptr : ucd->dlogShell, ucd->cmdTextW)) {
			XtFree(cmdText);
			return nullptr;
		}
	}

	auto f = new menuItemRec;
	f->name = nameText;
	f->cmd  = cmdText;

	if ((mneText = XmTextGetString(ucd->mneTextW))) {
		f->mnemonic = mneText == nullptr ? '\0' : mneText[0];
		XtFree(mneText);
		if (f->mnemonic == ':') /* colons mess up string parsing */
			f->mnemonic = '\0';
	}
	if ((accText = XmTextGetString(ucd->accTextW))) {
		parseAcceleratorString(accText, &f->modifiers, &f->keysym);
		XtFree(accText);
	}
	if (ucd->dialogType == SHELL_CMDS) {
		if (XmToggleButtonGetState(ucd->selInpBtn))
			f->input = FROM_SELECTION;
		else if (XmToggleButtonGetState(ucd->winInpBtn))
			f->input = FROM_WINDOW;
		else if (XmToggleButtonGetState(ucd->eitherInpBtn))
			f->input = FROM_EITHER;
		else
			f->input = FROM_NONE;
		if (XmToggleButtonGetState(ucd->winOutBtn))
			f->output = TO_NEW_WINDOW;
		else if (XmToggleButtonGetState(ucd->dlogOutBtn))
			f->output = TO_DIALOG;
		else
			f->output = TO_SAME_WINDOW;
		f->repInput = XmToggleButtonGetState(ucd->repInpBtn);
		f->saveFirst = XmToggleButtonGetState(ucd->saveFirstBtn);
		f->loadAfter = XmToggleButtonGetState(ucd->loadAfterBtn);
	} else {
		f->input = XmToggleButtonGetState(ucd->selInpBtn) ? FROM_SELECTION : FROM_NONE;
		f->output = TO_SAME_WINDOW;
		f->repInput = False;
		f->saveFirst = False;
		f->loadAfter = False;
	}
	return f;
}

/*
** Copy a menu item record, and its associated memory
*/
static menuItemRec *copyMenuItemRec(menuItemRec *item) {

	auto newItem = new menuItemRec(*item);
	newItem->name = XString(item->name);
	newItem->cmd  = XtStringDup(item->cmd);
	return newItem;
}

/*
** Free a menu item record, and its associated memory
*/
static void freeMenuItemRec(menuItemRec *item) {
	XtFree(item->cmd);
	delete item;
}

/*
** Callbacks for managed-list operations
*/
static void *getDialogDataCB(void *oldItem, int explicitRequest, int *abort, void *cbArg) {
	userCmdDialog *ucd = (userCmdDialog *)cbArg;
	menuItemRec *currentFields;

	/* If the dialog is currently displaying the "new" entry and the
	   fields are empty, that's just fine */
	if (oldItem == nullptr && dialogFieldsAreEmpty(ucd))
		return nullptr;

	/* If there are no problems reading the data, just return it */
	currentFields = readDialogFields(ucd, True);
	if(currentFields)
		return currentFields;

	/* If user might not be expecting fields to be read, give more warning */
	if (!explicitRequest) {
		if (DialogF(DF_WARN, ucd->dlogShell, 2, "Discard Entry", "Discard incomplete entry\nfor current menu item?", "Keep", "Discard") == 2) {
			return oldItem == nullptr ? nullptr : copyMenuItemRec((menuItemRec *)oldItem);
		}
	}

	/* Do readDialogFields again without "silent" mode to display warning(s) */
	readDialogFields(ucd, False);
	*abort = True;
	return nullptr;
}

static void setDialogDataCB(void *item, void *cbArg) {
	updateDialogFields((menuItemRec *)item, (userCmdDialog *)cbArg);
}

static int dialogFieldsAreEmpty(userCmdDialog *ucd) {
	return TextWidgetIsBlank(ucd->nameTextW) && TextWidgetIsBlank(ucd->cmdTextW) && TextWidgetIsBlank(ucd->accTextW) && TextWidgetIsBlank(ucd->mneTextW) &&
	       (ucd->dialogType != SHELL_CMDS ||
	        (XmToggleButtonGetState(ucd->selInpBtn) && XmToggleButtonGetState(ucd->sameOutBtn) && !XmToggleButtonGetState(ucd->repInpBtn) && !XmToggleButtonGetState(ucd->saveFirstBtn) && !XmToggleButtonGetState(ucd->loadAfterBtn)));
}

static void freeItemCB(void *item) {
	freeMenuItemRec((menuItemRec *)item);
}

/*
** Gut a text widget of it's ability to process input
*/
static void disableTextW(Widget textW) {
	static XtTranslations emptyTable = nullptr;
	static const char *emptyTranslations = "\
    	<EnterWindow>:	enter()\n\
	<Btn1Down>:	grab-focus()\n\
	<Btn1Motion>:	extend-adjust()\n\
	<Btn1Up>:	extend-end()\n\
	Shift<Key>Tab:	prev-tab-group()\n\
	Ctrl<Key>Tab:	next-tab-group()\n\
	<Key>Tab:	next-tab-group()\n\
	<LeaveWindow>:	leave()\n\
	<FocusIn>:	focusIn()\n\
	<FocusOut>:	focusOut()\n\
	<Unmap>:	unmap()\n";

	/* replace the translation table with the slimmed down one above */
	if(!emptyTable)
		emptyTable = XtParseTranslationTable(emptyTranslations);
	XtVaSetValues(textW, XmNtranslations, emptyTable, nullptr);
}

static char *writeMenuItemString(menuItemRec **menuItems, int nItems, int listType) {
	char accStr[MAX_ACCEL_LEN];
	menuItemRec *f;
	int i, length;

	/* determine the max. amount of memory needed for the returned string
	   and allocate a buffer for composing the string */
	   
	// NOTE(eteran): this code unconditionally writes at least 2 chars
	// so to avoid an off by one error, this needs to be initialized to
	// 1
	length = 1;
	for (i = 0; i < nItems; i++) {
		f = menuItems[i];
		generateAcceleratorString(accStr, f->modifiers, f->keysym);
		length += f->name.size() * 2; /* allow for \n & \\ expansions */
		length += strlen(accStr);
		length += strlen(f->cmd) * 6; /* allow for \n & \\ expansions */
		length += 21;                 /* number of characters added below */
	}
	length++; /* terminating null */
	char *outStr = XtMalloc(length);

	/* write the string */
	char *outPtr = outStr;
	*outPtr++ = '\\';
	*outPtr++ = '\n';
	for (i = 0; i < nItems; i++) {
		f = menuItems[i];
		generateAcceleratorString(accStr, f->modifiers, f->keysym);
		*outPtr++ = '\t';
		
		for (auto ch : f->name) { /* Copy the command name */
			if (ch == '\\') {                /* changing backslashes to \\ */
				*outPtr++ = '\\';
				*outPtr++ = '\\';
			} else if (ch == '\n') { /* changing newlines to \n */
				*outPtr++ = '\\';
				*outPtr++ = 'n';
			} else {
				*outPtr++ = ch;
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
		
		for (auto c = f->cmd; *c != '\0'; c++) { /* Copy the command string, changing */
			if (*c == '\\') {               /* backslashes to double backslashes */
				*outPtr++ = '\\';           /* and newlines to backslash-n's,    */
				*outPtr++ = '\\';           /* followed by real newlines and tab */
			} else if (*c == '\n') {
				*outPtr++ = '\\';
				*outPtr++ = 'n';
				*outPtr++ = '\\';
				*outPtr++ = '\n';
				*outPtr++ = '\t';
				*outPtr++ = '\t';
			} else
				*outPtr++ = *c;
		}
		
		if (listType == MACRO_CMDS || listType == BG_MENU_CMDS) {
			if (*(outPtr - 1) == '\t')
				outPtr--;
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


static nullable_string writeMenuItemStringEx(menuItemRec **menuItems, int nItems, int listType) {

	nullable_string str;
	if(char *s = writeMenuItemString(menuItems, nItems, listType)) {
		str = std::string(s);
		XtFree(s);
	}
	return str;
}

static int loadMenuItemString(const char *inString, menuItemRec **menuItems, int *nItems, int listType) {

	char *cmdStr;
	const char *inPtr = inString;
	char *nameStr, accStr[MAX_ACCEL_LEN], mneChar;
	KeySym keysym;
	unsigned int modifiers;
	int i, input, output, saveFirst, loadAfter, repInput;
	int nameLen, accLen, mneLen, cmdLen;

	for (;;) {

		/* remove leading whitespace */
		while (*inPtr == ' ' || *inPtr == '\t')
			inPtr++;

		/* end of string in proper place */
		if (*inPtr == '\0') {
			return True;
		}

		/* read name field */
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

		/* read accelerator field */
		accLen = strcspn(inPtr, ":");
		if (accLen >= MAX_ACCEL_LEN)
			return parseError("accelerator field too long");
		strncpy(accStr, inPtr, accLen);
		accStr[accLen] = '\0';
		inPtr += accLen;
		if (*inPtr == '\0')
			return parseError("end not expected");
		inPtr++;

		/* read menemonic field */
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

		/* read flags field */
		input = FROM_NONE;
		output = TO_SAME_WINDOW;
		repInput = False;
		saveFirst = False;
		loadAfter = False;
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

		/* read command field */
		if (listType == SHELL_CMDS) {
			if (*inPtr++ != '\n')
				return parseError("command must begin with newline");
			while (*inPtr == ' ' || *inPtr == '\t') /* leading whitespace */
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
		while (*inPtr == ' ' || *inPtr == '\t' || *inPtr == '\n')
			inPtr++; /* skip trailing whitespace & newline */

		/* parse the accelerator field */
		if (!parseAcceleratorString(accStr, &modifiers, &keysym))
			return parseError("couldn't read accelerator field");

		/* create a menu item record */
		auto f = new menuItemRec;
		f->name      = nameStr;
		f->cmd       = cmdStr;
		f->mnemonic  = mneChar;
		f->modifiers = modifiers;
		f->input     = input;
		f->output    = output;
		f->repInput  = repInput;
		f->saveFirst = saveFirst;
		f->loadAfter = loadAfter;
		f->keysym    = keysym;

		/* add/replace menu record in the list */
		for (i = 0; i < *nItems; i++) {
			if (menuItems[i]->name == f->name) {
				freeMenuItemRec(menuItems[i]);
				menuItems[i] = f;
				break;
			}
		}
		if (i == *nItems)
			menuItems[(*nItems)++] = f;
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
static void generateAcceleratorString(char *text, unsigned int modifiers, KeySym keysym) {
	const char *shiftStr = "";
	const char *ctrlStr = "";
	const char *altStr = "";
	const char *mod2Str = "";
	const char *mod3Str = "";
	const char *mod4Str = "";
	const char *mod5Str = "";
	char keyName[20];
	Modifiers numLockMask = GetNumLockModMask(TheDisplay);

	/* if there's no accelerator, generate an empty string */
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

	/* concatenate the strings together */
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

	/* if there's no accelerator, generate an empty string */
	if (keysym == NoSymbol) {
		*text = '\0';
		return;
	}

	/* translate the modifiers into strings */
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

	/* put the modifiers together with the key name */
	sprintf(text, "%s%s%s%s%s%s%s%s<Key>%s", shiftStr, lockStr, ctrlStr, altStr, mod2Str, mod3Str, mod4Str, mod5Str, XKeysymToString(keysym));
}

/*
** Read an accelerator name and put it into the form of a modifier mask
** and a KeySym code.  Returns false if string can't be read
** ... does not handle whitespace in string (look at scanf)
*/
static int parseAcceleratorString(const char *string, unsigned int *modifiers, KeySym *keysym) {
	int i, nFields, inputLength = strlen(string);
	char fields[10][MAX_ACCEL_LEN];

	/* a blank field means no accelerator */
	if (inputLength == 0) {
		*modifiers = 0;
		*keysym = NoSymbol;
		return True;
	}

	/* limit the string length so no field strings will overflow */
	if (inputLength > MAX_ACCEL_LEN)
		return False;

	/* divide the input into '+' separated fields */
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

	/* parse the modifier names from the rest of the fields */
	*modifiers = 0;
	for (i = 0; i < nFields - 1; i++) {
		if (!strcmp(fields[i], "Shift"))
			*modifiers |= ShiftMask;
		else if (!strcmp(fields[i], "Lock"))
			*modifiers |= LockMask;
		else if (!strcmp(fields[i], "Ctrl"))
			*modifiers |= ControlMask;
		/* comparision with "Alt" for compatibility with old .nedit files*/
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

	/* all fields successfully parsed */
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

	/* Parse the input */
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
** If "*string" is not terminated with a newline character, reallocate the
** string and add one.  (The macro language requires newline terminators for
** statements, but the text widget doesn't force it like the NEdit text buffer
** does, so this might avoid some confusion.)
*/
static char *addTerminatingNewlineEx(char *string) {

	int length = strlen(string);
	if (string[length - 1] == '\n') {
		return string;
	}

	char *str = XtMalloc(length + 2);
	strcpy(str, string);
	str[length] = '\n';
	str[length + 1] = '\0';
	XtFree(string);
	return str;
}

/*
** Cache user menus:
** allocate an empty user (shell, macro) menu cache structure
*/
UserMenuCache *CreateUserMenuCache(void) {
	/* allocate some memory for the new data structure */
	auto cache = new UserMenuCache;

	cache->umcLanguageMode = -2;
	cache->umcShellMenuCreated = False;
	cache->umcMacroMenuCreated = False;
	cache->umcShellMenuList.umlNbrItems = 0;
	cache->umcShellMenuList.umlItems = nullptr;
	cache->umcMacroMenuList.umlNbrItems = 0;
	cache->umcMacroMenuList.umlItems = nullptr;

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
	cache->ubmcMenuList.umlNbrItems = 0;
	cache->ubmcMenuList.umlItems = nullptr;
}

void FreeUserBGMenuCache(UserBGMenuCache *cache) {
	freeUserMenuList(&cache->ubmcMenuList);
}

/*
** Cache user menus:
** Parse given menu item list and setup a user menu info list for
** management of user menu.
*/
static void parseMenuItemList(menuItemRec **itemList, int nbrOfItems, userMenuInfo **infoList, userSubMenuCache *subMenus) {
	int i;
	userMenuInfo *info;

	/* Allocate storage for structures to keep track of sub-menus */
	allocSubMenuCache(subMenus, nbrOfItems);

	/* 1st pass: setup user menu info: extract language modes, menu name &
	   default indication; build user menu ID */
	for (i = 0; i < nbrOfItems; i++) {
		infoList[i] = parseMenuItemRec(itemList[i]);
		generateUserMenuId(infoList[i], subMenus);
	}

	/* 2nd pass: solve "default" dependencies */
	for (i = 0; i < nbrOfItems; i++) {
		info = infoList[i];

		/* If the user menu item is a default one, then scan the list for
		   items with the same name and a language mode specified.
		   If one is found, then set the default index to the index of the
		   current default item. */
		if (info->umiIsDefault) {
			setDefaultIndex(infoList, nbrOfItems, i);
		}
	}
}

/*
** Returns the sub-menu depth (i.e. nesting level) of given
** menu name.
*/
static int getSubMenuDepth(const char *menuName) {
	const char *subSep;
	int depth = 0;

	/* determine sub-menu depth by counting '>' of given "menuName" */
	subSep = menuName;
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
static userMenuInfo *parseMenuItemRec(menuItemRec *item) {

	int subMenuDepth;
	int idSize;

	/* allocate a new user menu info element */
	auto newInfo = new userMenuInfo;

	/* determine sub-menu depth and allocate some memory
	   for hierarchical ID; init. ID with {0,.., 0} */
	newInfo->umiName = stripLanguageModeEx(item->name);

	subMenuDepth = getSubMenuDepth(newInfo->umiName);
	idSize = sizeof(int) * (subMenuDepth + 1);

	newInfo->umiId = (int *)XtMalloc(idSize);
	memset(newInfo->umiId, 0, idSize);

	/* init. remaining parts of user menu info element */
	newInfo->umiIdLen = 0;
	newInfo->umiIsDefault = False;
	newInfo->umiNbrOfLanguageModes = 0;
	newInfo->umiLanguageMode = nullptr;
	newInfo->umiDefaultIndex = -1;
	newInfo->umiToBeManaged = False;

	/* assign language mode info to new user menu info element */
	parseMenuItemName(item->name.str(), newInfo);

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

		/* setup a list of all language modes related to given menu item */
		while (atPtr) {
			/* extract language mode name after "@" sign */
			for (endPtr = atPtr + 1; isalnum((unsigned char)*endPtr) || *endPtr == '_' || *endPtr == '-' || *endPtr == ' ' || *endPtr == '+' || *endPtr == '$' || *endPtr == '#'; endPtr++)
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

			/* look for next "@" */
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
	int idSize;
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
			idSize = sizeof(int) * (subMenuDepth + 2);
			curSubMenu->usmiId = (int *)XtMalloc(idSize);
			memcpy(curSubMenu->usmiId, info->umiId, idSize);
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

	/* remember position of menu item within final (sub) menu */
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
static userSubMenuInfo *findSubMenuInfoEx(userSubMenuCache *subMenus, const XString &hierName) {

	for (int i = 0; i < subMenus->usmcNbrOfSubMenus; i++) {
		if (hierName == subMenus->usmcInfo[i].usmiName) {
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
static char *stripLanguageModeEx(const XString &menuItemName) {
	
	if(const char *firstAtPtr = strchr(menuItemName.str(), '@')) {
		return copySubstring(menuItemName.str(), firstAtPtr - menuItemName.str());
	} else {
		return XtNewStringEx(menuItemName.str());
	}
}

static void setDefaultIndex(userMenuInfo **infoList, int nbrOfItems, int defaultIdx) {
	char *defaultMenuName = infoList[defaultIdx]->umiName;
	int i;
	userMenuInfo *info;

	/* Scan the list for items with the same name and a language mode
	   specified. If one is found, then set the default index to the
	   index of the current default item. */
	for (i = 0; i < nbrOfItems; i++) {
		info = infoList[i];

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
static void applyLangModeToUserMenuInfo(userMenuInfo **infoList, int nbrOfItems, int languageMode) {
	int i;
	userMenuInfo *info;

	/* 1st pass: mark all items as "to be managed", which are applicable
	   for all language modes or which are indicated as "default" items */
	for (i = 0; i < nbrOfItems; i++) {
		info = infoList[i];

		info->umiToBeManaged = (info->umiNbrOfLanguageModes == 0 || info->umiIsDefault);
	}

	/* 2nd pass: mark language mode specific items matching given language
	   mode as "to be managed". Reset "to be managed" indications of
	   "default" items, if applicable */
	for (i = 0; i < nbrOfItems; i++) {
		info = infoList[i];

		if (info->umiNbrOfLanguageModes != 0) {
			if (doesLanguageModeMatch(info, languageMode)) {
				info->umiToBeManaged = True;

				if (info->umiDefaultIndex != -1)
					infoList[info->umiDefaultIndex]->umiToBeManaged = False;
			}
		}
	}
}

/*
** Returns true, if given user menu info is applicable for given language mode
*/
static int doesLanguageModeMatch(userMenuInfo *info, int languageMode) {

	for (int i = 0; i < info->umiNbrOfLanguageModes; i++) {
		if (info->umiLanguageMode[i] == languageMode) {
			return True;
		}
	}

	return False;
}

static void freeUserMenuInfoList(userMenuInfo **infoList, int nbrOfItems) {
	for (int i = 0; i < nbrOfItems; i++) {
		freeUserMenuInfo(infoList[i]);
	}
}

static void freeUserMenuInfo(userMenuInfo *info) {


	XtFree((char *)info->umiId);

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

static void freeSubMenuCache(userSubMenuCache *subMenus) {
	int i;

	for (i = 0; i < subMenus->usmcNbrOfSubMenus; i++) {
		XtFree(subMenus->usmcInfo[i].usmiName);
		XtFree((char *)subMenus->usmcInfo[i].usmiId);
	}

	delete [] subMenus->usmcInfo;
}

static void allocUserMenuList(UserMenuList *list, int nbrOfItems) {
	list->umlNbrItems = 0;
	list->umlItems = new UserMenuListElement *[nbrOfItems];
}

static void freeUserMenuList(UserMenuList *list) {
	int i;

	for (i = 0; i < list->umlNbrItems; i++)
		freeUserMenuListElement(list->umlItems[i]);

	list->umlNbrItems = 0;

	delete [] list->umlItems;
	list->umlItems = nullptr;
}

static UserMenuListElement *allocUserMenuListElement(Widget menuItem, char *accKeys) {

	auto element = new UserMenuListElement;

	element->umleManageMode          = UMMM_UNMANAGE;
	element->umlePrevManageMode      = UMMM_UNMANAGE;
	element->umleAccKeys             = accKeys;
	element->umleAccLockPatchApplied = False;
	element->umleMenuItem            = menuItem;
	element->umleSubMenuPane         = nullptr;
	element->umleSubMenuList         = nullptr;

	return element;
}

static void freeUserMenuListElement(UserMenuListElement *element) {
	if (element->umleSubMenuList)
		freeUserSubMenuList(element->umleSubMenuList);

	XtFree(element->umleAccKeys);
	
	delete element;
}

static UserMenuList *allocUserSubMenuList(int nbrOfItems) {
	auto list = new UserMenuList;
	allocUserMenuList(list, nbrOfItems);
	return list;
}

static void freeUserSubMenuList(UserMenuList *list) {
	freeUserMenuList(list);
	delete list;
}
