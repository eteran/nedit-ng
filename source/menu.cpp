/*******************************************************************************
*                                                                              *
* menu.c -- Nirvana Editor menus                                               *
*                                                                              *
* Copyright (C) 1999 Mark Edel                                                 *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute versions of this program linked to  *
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
* May 10, 1991                                                                 *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/

#include <QApplication>
#include <QInputDialog>
#include <QMessageBox>
#include <QStringList>
#include <QPushButton>
#include <QMenu>
#include "ui/MainWindow.h"
#include "ui/DialogExecuteCommand.h"
#include "ui/DialogFilter.h"
#include "ui/DialogTabs.h"
#include "ui/DialogFonts.h"
#include "ui/DialogWindowSize.h"
#include "macro.h"
#include "IndentStyle.h"
#include "WrapStyle.h"
#include "TextHelper.h"
#include "TextDisplay.h"
#include "menu.h"
#include "TextBuffer.h"
#include "text.h"
#include "nedit.h"
#include "file.h"
#include "window.h"
#include "search.h"
#include "selection.h"
#include "shift.h"
#include "help.h"
#include "preferences.h"
#include "tags.h"
#include "userCmds.h"
#include "shell.h"
#include "highlight.h"
#include "highlightData.h"
#include "interpret.h"
#include "smartIndent.h"
#include "util/MotifHelper.h"
#include "Document.h"
#include "util/misc.h"
#include "util/fileUtils.h"
#include "util/utils.h"
#include "Xlt/BubbleButton.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>

#include <Xm/Xm.h>
#include <Xm/CascadeB.h>
#include <Xm/PushB.h>
#include <Xm/ToggleB.h>
#include <Xm/Separator.h>
#include <Xm/RowColumn.h>
#include <Xm/MenuShell.h>

#define MENU_WIDGET(w) (XmGetPostedFromWidget(XtParent(w)))

// Menu modes for SGI_CUSTOM short-menus feature 
enum menuModes { FULL, SHORT };

typedef void (*menuCallbackProc)(Widget, XtPointer, XtPointer);

extern "C" void _XmDismissTearOff(Widget, XtPointer, XtPointer);

static void doActionCB(Widget w, XtPointer clientData, XtPointer callData);
static void doTabActionCB(Widget w, XtPointer clientData, XtPointer callData);
static void autoSaveDefCB(Widget w, XtPointer clientData, XtPointer callData);
static void preserveDefCB(Widget w, XtPointer clientData, XtPointer callData);
static void openInTabDefCB(Widget w, XtPointer clientData, XtPointer callData);
static void tabBarDefCB(Widget w, XtPointer clientData, XtPointer callData);
static void tabBarHideDefCB(Widget w, XtPointer clientData, XtPointer callData);
static void tabSortDefCB(Widget w, XtPointer clientData, XtPointer callData);
static void toolTipsDefCB(Widget w, XtPointer clientData, XtPointer callData);
static void tabNavigateDefCB(Widget w, XtPointer clientData, XtPointer callData);
static void statsLineDefCB(Widget w, XtPointer clientData, XtPointer callData);
static void iSearchLineDefCB(Widget w, XtPointer clientData, XtPointer callData);
static void lineNumsDefCB(Widget w, XtPointer clientData, XtPointer callData);
static void showMatchingOffDefCB(Widget w, XtPointer clientData, XtPointer callData);
static void showMatchingDelimitDefCB(Widget w, XtPointer clientData, XtPointer callData);
static void showMatchingRangeDefCB(Widget w, XtPointer clientData, XtPointer callData);
static void matchSyntaxBasedDefCB(Widget w, XtPointer clientData, XtPointer callData);
static void highlightOffDefCB(Widget w, XtPointer clientData, XtPointer callData);
static void highlightDefCB(Widget w, XtPointer clientData, XtPointer callData);
static void backlightCharsDefCB(Widget w, XtPointer clientData, XtPointer callData);
static void highlightingDefCB(Widget w, XtPointer clientData, XtPointer callData);
static void stylesDefCB(Widget w, XtPointer clientData, XtPointer callData);
static void searchDlogsDefCB(Widget w, XtPointer clientData, XtPointer callData);
static void beepOnSearchWrapDefCB(Widget w, XtPointer clientData, XtPointer callData);
static void keepSearchDlogsDefCB(Widget w, XtPointer clientData, XtPointer callDat);
static void searchWrapsDefCB(Widget w, XtPointer clientData, XtPointer callData);
static void appendLFCB(Widget w, XtPointer clientData, XtPointer callData);
static void reposDlogsDefCB(Widget w, XtPointer clientData, XtPointer callData);
static void autoScrollDefCB(Widget w, XtPointer clientData, XtPointer callData);
static void modWarnDefCB(Widget w, XtPointer clientData, XtPointer callData);
static void modWarnRealDefCB(Widget w, XtPointer clientData, XtPointer callData);
static void exitWarnDefCB(Widget w, XtPointer clientData, XtPointer callData);
static void searchLiteralCB(Widget w, XtPointer clientData, XtPointer callData);
static void searchCaseSenseCB(Widget w, XtPointer clientData, XtPointer callData);
static void searchLiteralWordCB(Widget w, XtPointer clientData, XtPointer callData);
static void searchCaseSenseWordCB(Widget w, XtPointer clientData, XtPointer callData);
static void searchRegexNoCaseCB(Widget w, XtPointer clientData, XtPointer callData);
static void searchRegexCB(Widget w, XtPointer clientData, XtPointer callData);
#ifdef REPLACE_SCOPE
static void replaceScopeWindowCB(Widget w, XtPointer clientData, XtPointer callData);
static void replaceScopeSelectionCB(Widget w, XtPointer clientData, XtPointer callData);
static void replaceScopeSmartCB(Widget w, XtPointer clientData, XtPointer callData);
#endif
static void size24x80CB(Widget w, XtPointer clientData, XtPointer callData);
static void size40x80CB(Widget w, XtPointer clientData, XtPointer callData);
static void size60x80CB(Widget w, XtPointer clientData, XtPointer callData);
static void size80x80CB(Widget w, XtPointer clientData, XtPointer callData);
static void sizeCustomCB(Widget w, XtPointer clientData, XtPointer callData);
static void cancelShellCB(Widget w, XtPointer clientData, XtPointer callData);
static void learnCB(Widget w, XtPointer clientData, XtPointer callData);
static void finishLearnCB(Widget w, XtPointer clientData, XtPointer callData);
static void cancelLearnCB(Widget w, XtPointer clientData, XtPointer callData);
static void replayCB(Widget w, XtPointer clientData, XtPointer callData);
static void windowMenuCB(Widget w, XtPointer clientData, XtPointer callData);
static void newAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void newOppositeAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void newTabAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void openDialogAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void openAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void closeAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void saveAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void saveAsDialogAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void saveAsAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void revertDialogAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void revertAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void exitAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void repeatDialogAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void repeatMacroAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void splitPaneAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void nextDocumentAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void prevDocumentAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void lastDocumentAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);

static void filterDialogAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void shellFilterAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void execDialogAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void execAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void execLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void shellMenuAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);

static void macroMenuAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void bgMenuAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void beginningOfSelectionAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void endOfSelectionAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static Widget createMenu(Widget parent, const char *name, const char *label, char mnemonic, Widget *cascadeBtn, int mode);
static Widget createMenuItem(Widget parent, const char *name, const char *label, char mnemonic, menuCallbackProc callback, const void *cbArg, int mode);
static Widget createMenuToggle(Widget parent, const char *name, const char *label, char mnemonic, menuCallbackProc callback, const void *cbArg, int set, int mode);
static Widget createMenuRadioToggle(Widget parent, const char *name, const char *label, char mnemonic, menuCallbackProc callback, const void *cbArg, int set, int mode);
static Widget createMenuSeparator(Widget parent, const char *name, int mode);
static void invalidatePrevOpenMenus(void);
static void updateWindowMenu(const Document *window);
static void raiseCB(Widget w, XtPointer clientData, XtPointer callData);
static void setWindowSizeDefault(int rows, int cols);
static void updateWindowSizeMenus(void);
static void updateWindowSizeMenu(Document *win);
static int strCaseCmp(const char *str1, const char *str2);
static void bgMenuPostAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void tabMenuPostAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void raiseWindowAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void focusPaneAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);

// Application action table 
static XtActionsRec Actions[] = {{(String) "new", newAP},
                                 {(String) "new_opposite", newOppositeAP},
                                 {(String) "new_tab", newTabAP},
                                 {(String) "open", openAP},
                                 {(String) "open-dialog", openDialogAP},
                                 {(String) "open_dialog", openDialogAP},
                                 //{(String) "open-selected", openSelectedAP},
                                 //{(String) "open_selected", openSelectedAP},
                                 {(String) "close", closeAP},
                                 {(String) "save", saveAP},
                                 {(String) "save-as", saveAsAP},
                                 {(String) "save_as", saveAsAP},
                                 {(String) "save-as-dialog", saveAsDialogAP},
                                 {(String) "save_as_dialog", saveAsDialogAP},
                                 {(String) "revert-to-saved", revertAP},
                                 {(String) "revert_to_saved", revertAP},
                                 {(String) "revert_to_saved_dialog", revertDialogAP},
                                 //{(String) "include-file", includeAP},
                                 //{(String) "include_file", includeAP},
                                 //{(String) "include-file-dialog", includeDialogAP},
                                 //{(String) "include_file_dialog", includeDialogAP},
                                 //{(String) "load-macro-file", loadMacroAP},
                                 //{(String) "load_macro_file", loadMacroAP},
                                 //{(String) "load-macro-file-dialog", loadMacroDialogAP},
                                 //{(String) "load_macro_file_dialog", loadMacroDialogAP},
                                 //{(String) "load-tags-file", loadTagsAP},
                                 //{(String) "load_tags_file", loadTagsAP},
                                 //{(String) "load-tags-file-dialog", loadTagsDialogAP},
                                 //{(String) "load_tags_file_dialog", loadTagsDialogAP},
                                 //{(String) "unload_tags_file", unloadTagsAP},
                                 //{(String) "load_tips_file", loadTipsAP},
                                 //{(String) "load_tips_file_dialog", loadTipsDialogAP},
                                 //{(String) "unload_tips_file", unloadTipsAP},
                                 //{(String) "print", printAP},
                                 //{(String) "print-selection", printSelAP},
                                 //{(String) "print_selection", printSelAP},
                                 {(String) "exit", exitAP},
                                 //{(String) "undo", undoAP},
                                 //{(String) "redo", redoAP},
                                 //{(String) "delete", clearAP},
                                 //{(String) "select-all", selAllAP},
                                 //{(String) "select_all", selAllAP},
                                 //{(String) "shift-left", shiftLeftAP},
                                 //{(String) "shift_left", shiftLeftAP},
                                 //{(String) "shift-left-by-tab", shiftLeftTabAP},
                                 //{(String) "shift_left_by_tab", shiftLeftTabAP},
                                 //{(String) "shift-right", shiftRightAP},
                                 //{(String) "shift_right", shiftRightAP},
                                 //{(String) "shift-right-by-tab", shiftRightTabAP},
                                 //{(String) "shift_right_by_tab", shiftRightTabAP},
                                 //{(String) "find", findAP},
                                 //{(String) "find-dialog", findDialogAP},
                                 //{(String) "find_dialog", findDialogAP},
                                 //{(String) "find-again", findSameAP},
                                 //{(String) "find_again", findSameAP},
                                 //{(String) "find-selection", findSelAP},
                                 //{(String) "find_selection", findSelAP},
                                 //{(String) "find_incremental", findIncrAP},
                                 //{(String) "start_incremental_find", startIncrFindAP},
                                 //{(String) "replace", replaceAP},
                                 //{(String) "replace-dialog", replaceDialogAP},
                                 //{(String) "replace_dialog", replaceDialogAP},
                                 //{(String) "replace-all", replaceAllAP},
                                 //{(String) "replace_all", replaceAllAP},
                                 //{(String) "replace-in-selection", replaceInSelAP},
                                 //{(String) "replace_in_selection", replaceInSelAP},
                                 //{(String) "replace-again", replaceSameAP},
                                 //{(String) "replace_again", replaceSameAP},
                                 //{(String) "replace_find", replaceFindAP},
                                 //{(String) "replace_find_same", replaceFindSameAP},
                                 //{(String) "replace_find_again", replaceFindSameAP},
                                 //{(String) "goto-line-number", gotoAP},
                                 //{(String) "goto_line_number", gotoAP},
                                 //{(String) "goto-line-number-dialog", gotoDialogAP},
                                 //{(String) "goto_line_number_dialog", gotoDialogAP},
                                 //{(String) "goto-selected", gotoSelectedAP},
                                 //{(String) "goto_selected", gotoSelectedAP},
                                 //{(String) "mark", markAP},
                                 //{(String) "mark-dialog", markDialogAP},
                                 //{(String) "mark_dialog", markDialogAP},
                                 //{(String) "goto-mark", gotoMarkAP},
                                 //{(String) "goto_mark", gotoMarkAP},
                                 //{(String) "goto-mark-dialog", gotoMarkDialogAP},
                                 //{(String) "goto_mark_dialog", gotoMarkDialogAP},
                                 //{(String) "match", selectToMatchingAP},
                                 //{(String) "select_to_matching", selectToMatchingAP},
                                 //{(String) "goto_matching", gotoMatchingAP},
                                 //{(String) "find-definition", findDefAP},
                                 //{(String) "find_definition", findDefAP},
                                 //{(String) "show_tip", showTipAP},
                                 {(String) "split-pane", splitPaneAP},
                                 {(String) "split_pane", splitPaneAP},
                                 //{(String) "close-pane", closePaneAP},
                                 //{(String) "close_pane", closePaneAP},
                                 //{(String) "detach_document", detachDocumentAP},
                                 //{(String) "detach_document_dialog", detachDocumentDialogAP},
                                 //{(String) "move_document_dialog", moveDocumentDialogAP},
                                 {(String) "next_document", nextDocumentAP},
                                 {(String) "previous_document", prevDocumentAP},
                                 {(String) "last_document", lastDocumentAP},
                                 //{(String) "uppercase", capitalizeAP },
                                 //{(String) "lowercase", lowercaseAP },
                                 //{(String) "fill-paragraph", fillAP },
                                 //{(String) "fill_paragraph", fillAP },
                                 //{(String) "control-code-dialog", controlDialogAP},
                                 //{(String) "control_code_dialog", controlDialogAP},

                                 {(String) "filter-selection-dialog", filterDialogAP},
                                 {(String) "filter_selection_dialog", filterDialogAP},
                                 {(String) "filter-selection", shellFilterAP},
                                 {(String) "filter_selection", shellFilterAP},
                                 {(String) "execute-command", execAP},
                                 {(String) "execute_command", execAP},
                                 {(String) "execute-command-dialog", execDialogAP},
                                 {(String) "execute_command_dialog", execDialogAP},
                                 {(String) "execute-command-line", execLineAP},
                                 {(String) "execute_command_line", execLineAP},
                                 {(String) "shell-menu-command", shellMenuAP},
                                 {(String) "shell_menu_command", shellMenuAP},

                                 {(String) "macro-menu-command", macroMenuAP},
                                 {(String) "macro_menu_command", macroMenuAP},
                                 {(String) "bg_menu_command", bgMenuAP},
                                 {(String) "post_window_bg_menu", bgMenuPostAP},
                                 {(String) "post_tab_context_menu", tabMenuPostAP},
                                 {(String) "beginning-of-selection", beginningOfSelectionAP},
                                 {(String) "beginning_of_selection", beginningOfSelectionAP},
                                 {(String) "end-of-selection", endOfSelectionAP},
                                 {(String) "end_of_selection", endOfSelectionAP},
                                 {(String) "repeat_macro", repeatMacroAP},
                                 {(String) "repeat_dialog", repeatDialogAP},
                                 {(String) "raise_window", raiseWindowAP},
                                 {(String) "focus_pane", focusPaneAP},
                                 //{(String) "set_statistics_line", setStatisticsLineAP},
                                 //{(String) "set_incremental_search_line", setIncrementalSearchLineAP},
                                 //{(String) "set_show_line_numbers", setShowLineNumbersAP},
                                 //{(String) "set_auto_indent", setAutoIndentAP},
                                 //{(String) "set_wrap_text", setWrapTextAP},
                                 //{(String) "set_wrap_margin", setWrapMarginAP},
                                 //{(String) "set_highlight_syntax", setHighlightSyntaxAP},

                                 //{(String) "set_make_backup_copy", setMakeBackupCopyAP},

                                 //{(String) "set_incremental_backup", setIncrementalBackupAP},
                                 //{(String) "set_show_matching", setShowMatchingAP},
                                 //{(String) "set_match_syntax_based", setMatchSyntaxBasedAP},
                                 //{(String) "set_overtype_mode", setOvertypeModeAP},
                                 //{(String) "set_locked", setLockedAP},
                                 //{(String) "set_tab_dist", setTabDistAP},
                                 //{(String) "set_em_tab_dist", setEmTabDistAP},
                                 //{(String) "set_use_tabs", setUseTabsAP},
                                 //{(String) "set_fonts", setFontsAP},
                                 //{(String) "set_language_mode", setLanguageModeAP}
                                };

// List of previously opened files for File menu 
static int NPrevOpen = 0;
static char **PrevOpen = nullptr;

void HidePointerOnKeyedEvent(Widget w, XEvent *event) {
	if (event && (event->type == KeyPress || event->type == KeyRelease)) {
		auto textD = textD_of(w);	
		textD->ShowHidePointer(true);
	}
}

/*
** Install actions for use in translation tables and macro recording, relating
** to menu item commands
*/
void InstallMenuActions(XtAppContext context) {
	XtAppAddActions(context, Actions, XtNumber(Actions));
}

/*
** Return the (statically allocated) action table for menu item actions.
*/
XtActionsRec *GetMenuActions(int *nActions) {
	*nActions = XtNumber(Actions);
	return Actions;
}

/*
** Create the menu bar
*/
Widget CreateMenuBar(Widget parent, Document *window) {
	Widget menuBar, menuPane, btn, subPane, subSubPane, subSubSubPane, cascade;

	/*
	** cache user menus:
	** allocate user menu cache
	*/
	window->userMenuCache_ = CreateUserMenuCache();

	/*
	** Create the menu bar (row column) widget
	*/
	menuBar = XmCreateMenuBar(parent, (String) "menuBar", nullptr, 0);

	/*
	** "File" pull down menu.
	*/
	menuPane = createMenu(menuBar, "fileMenu", "File", 0, nullptr, SHORT);
	createMenuItem(menuPane, "new", "New", 'N', doActionCB, "new", SHORT);
	if (GetPrefOpenInTab())
		window->newOppositeItem_ = createMenuItem(menuPane, "newOpposite", "New Window", 'W', doActionCB, "new_opposite", SHORT);
	else
		window->newOppositeItem_ = createMenuItem(menuPane, "newOpposite", "New Tab", 'T', doActionCB, "new_opposite", SHORT);
	createMenuItem(menuPane, "open", "Open...", 'O', doActionCB, "open_dialog", SHORT);
	createMenuSeparator(menuPane, "sep1", SHORT);
	window->closeItem_ = createMenuItem(menuPane, "close", "Close", 'C', doActionCB, "close", SHORT);
	createMenuItem(menuPane, "save", "Save", 'S', doActionCB, "save", SHORT);
	createMenuItem(menuPane, "saveAs", "Save As...", 'A', doActionCB, "save_as_dialog", SHORT);
	createMenuItem(menuPane, "revertToSaved", "Revert to Saved", 'R', doActionCB, "revert_to_saved_dialog", SHORT);
	createMenuSeparator(menuPane, "sep2", SHORT);
	createMenuItem(menuPane, "exit", "Exit", 'x', doActionCB, "exit", SHORT);
	CheckCloseDim();

	/*
	** "Edit" pull down menu.
	*/
	menuPane = createMenu(menuBar, "editMenu", "Edit", 0, nullptr, SHORT);

	/*
	** "Search" pull down menu.
	*/
	menuPane = createMenu(menuBar, "searchMenu", "Search", 0, nullptr, SHORT);

	/*
	** Preferences menu, Default Settings sub menu
	*/
	menuPane = createMenu(menuBar, "preferencesMenu", "Preferences", 0, nullptr, SHORT);
	subPane = createMenu(menuPane, "defaultSettings", "Default Settings", 'D', nullptr, FULL);

	// Customize Menus sub menu 

	// Search sub menu 
	subSubPane = createMenu(subPane, "searching", "Searching", 'g', nullptr, FULL);
	window->searchDlogsDefItem_ = createMenuToggle(subSubPane, "verbose", "Verbose", 'V', searchDlogsDefCB, window, GetPrefSearchDlogs(), SHORT);
	window->searchWrapsDefItem_ = createMenuToggle(subSubPane, "wrapAround", "Wrap Around", 'W', searchWrapsDefCB, window, GetPrefSearchWraps(), SHORT);
	window->beepOnSearchWrapDefItem_ = createMenuToggle(subSubPane, "beepOnSearchWrap", "Beep On Search Wrap", 'B', beepOnSearchWrapDefCB, window, GetPrefBeepOnSearchWrap(), SHORT);
	window->keepSearchDlogsDefItem_ = createMenuToggle(subSubPane, "keepDialogsUp", "Keep Dialogs Up", 'K', keepSearchDlogsDefCB, window, GetPrefKeepSearchDlogs(), SHORT);
	subSubSubPane = createMenu(subSubPane, "defaultSearchStyle", "Default Search Style", 'D', nullptr, FULL);
	XtVaSetValues(subSubSubPane, XmNradioBehavior, True, nullptr);
	window->searchLiteralDefItem_ = createMenuToggle(subSubSubPane, "literal", "Literal", 'L', searchLiteralCB, window, GetPrefSearch() == SEARCH_LITERAL, FULL);
	window->searchCaseSenseDefItem_ = createMenuToggle(subSubSubPane, "caseSensitive", "Literal, Case Sensitive", 'C', searchCaseSenseCB, window, GetPrefSearch() == SEARCH_CASE_SENSE, FULL);
	window->searchLiteralWordDefItem_ = createMenuToggle(subSubSubPane, "literalWord", "Literal, Whole Word", 'W', searchLiteralWordCB, window, GetPrefSearch() == SEARCH_LITERAL_WORD, FULL);
	window->searchCaseSenseWordDefItem_ = createMenuToggle(subSubSubPane, "caseSensitiveWord", "Literal, Case Sensitive, Whole Word", 't', searchCaseSenseWordCB, window, GetPrefSearch() == SEARCH_CASE_SENSE_WORD, FULL);
	window->searchRegexDefItem_ = createMenuToggle(subSubSubPane, "regularExpression", "Regular Expression", 'R', searchRegexCB, window, GetPrefSearch() == SEARCH_REGEX, FULL);
	window->searchRegexNoCaseDefItem_ = createMenuToggle(subSubSubPane, "regularExpressionNoCase", "Regular Expression, Case Insensitive", 'I', searchRegexNoCaseCB, window, GetPrefSearch() == SEARCH_REGEX_NOCASE, FULL);
#ifdef REPLACE_SCOPE
	subSubSubPane = createMenu(subSubPane, "defaultReplaceScope", "Default Replace Scope", 'R', nullptr, FULL);
	XtVaSetValues(subSubSubPane, XmNradioBehavior, True, nullptr);
	window->replScopeWinDefItem_ = createMenuToggle(subSubSubPane, "window", "In Window", 'W', replaceScopeWindowCB, window, GetPrefReplaceDefScope() == REPL_DEF_SCOPE_WINDOW, FULL);
	window->replScopeSelDefItem_ = createMenuToggle(subSubSubPane, "selection", "In Selection", 'S', replaceScopeSelectionCB, window, GetPrefReplaceDefScope() == REPL_DEF_SCOPE_SELECTION, FULL);
	window->replScopeSmartDefItem_ = createMenuToggle(subSubSubPane, "window", "Smart", 'm', replaceScopeSmartCB, window, GetPrefReplaceDefScope() == REPL_DEF_SCOPE_SMART, FULL);
#endif

	// Syntax Highlighting sub menu 
	subSubPane = createMenu(subPane, "syntaxHighlighting", "Syntax Highlighting", 'H', nullptr, FULL);
	window->highlightOffDefItem_ = createMenuRadioToggle(subSubPane, "off", "Off", 'O', highlightOffDefCB, window, !GetPrefHighlightSyntax(), FULL);
	window->highlightDefItem_ = createMenuRadioToggle(subSubPane, "on", "On", 'n', highlightDefCB, window, GetPrefHighlightSyntax(), FULL);
	createMenuSeparator(subSubPane, "sep1", SHORT);
	createMenuItem(subSubPane, "recognitionPatterns", "Recognition Patterns...", 'R', highlightingDefCB, window, FULL);
	createMenuItem(subSubPane, "textDrawingStyles", "Text Drawing Styles...", 'T', stylesDefCB, window, FULL);
	window->backlightCharsDefItem_ = createMenuToggle(subPane, "backlightChars", "Apply Backlighting", 'g', backlightCharsDefCB, window, GetPrefBacklightChars(), FULL);

	// tabbed editing sub menu 
	subSubPane = createMenu(subPane, "tabbedEditMenu", "Tabbed Editing", 0, &cascade, SHORT);
	window->openInTabDefItem_ = createMenuToggle(subSubPane, "openAsTab", "Open File In New Tab", 'T', openInTabDefCB, window, GetPrefOpenInTab(), FULL);
	window->tabBarDefItem_ = createMenuToggle(subSubPane, "showTabBar", "Show Tab Bar", 'B', tabBarDefCB, window, GetPrefTabBar(), FULL);
	window->tabBarHideDefItem_ = createMenuToggle(subSubPane, "hideTabBar", "Hide Tab Bar When Only One Document is Open", 'H', tabBarHideDefCB, window, GetPrefTabBarHideOne(), FULL);
	window->tabNavigateDefItem_ = createMenuToggle(subSubPane, "tabNavigateDef", "Next/Prev Tabs Across Windows", 'W', tabNavigateDefCB, window, GetPrefGlobalTabNavigate(), FULL);
	window->tabSortDefItem_ = createMenuToggle(subSubPane, "tabSortDef", "Sort Tabs Alphabetically", 'S', tabSortDefCB, window, GetPrefSortTabs(), FULL);

	window->toolTipsDefItem_ = createMenuToggle(subPane, "showTooltips", "Show Tooltips", 0, toolTipsDefCB, window, GetPrefToolTips(), FULL);
	window->statsLineDefItem_ = createMenuToggle(subPane, "statisticsLine", "Statistics Line", 'S', statsLineDefCB, window, GetPrefStatsLine(), SHORT);
	window->iSearchLineDefItem_ = createMenuToggle(subPane, "incrementalSearchLine", "Incremental Search Line", 'i', iSearchLineDefCB, window, GetPrefISearchLine(), FULL);
	window->lineNumsDefItem_ = createMenuToggle(subPane, "showLineNumbers", "Show Line Numbers", 'N', lineNumsDefCB, window, GetPrefLineNums(), SHORT);
	window->saveLastDefItem_ = createMenuToggle(subPane, "preserveLastVersion", "Make Backup Copy (*.bck)", 'e', preserveDefCB, window, GetPrefSaveOldVersion(), SHORT);
	window->autoSaveDefItem_ = createMenuToggle(subPane, "incrementalBackup", "Incremental Backup", 'B', autoSaveDefCB, window, GetPrefAutoSave(), SHORT);

	// Show Matching sub menu 
	subSubPane = createMenu(subPane, "showMatching", "Show Matching (..)", 'M', nullptr, FULL);
	window->showMatchingOffDefItem_ = createMenuRadioToggle(subSubPane, "off", "Off", 'O', showMatchingOffDefCB, window, GetPrefShowMatching() == NO_FLASH, SHORT);
	window->showMatchingDelimitDefItem_ = createMenuRadioToggle(subSubPane, "delimiter", "Delimiter", 'D', showMatchingDelimitDefCB, window, GetPrefShowMatching() == FLASH_DELIMIT, SHORT);
	window->showMatchingRangeDefItem_ = createMenuRadioToggle(subSubPane, "range", "Range", 'R', showMatchingRangeDefCB, window, GetPrefShowMatching() == FLASH_RANGE, SHORT);
	createMenuSeparator(subSubPane, "sep", SHORT);
	window->matchSyntaxBasedDefItem_ = createMenuToggle(subSubPane, "matchSyntax", "Syntax Based", 'S', matchSyntaxBasedDefCB, window, GetPrefMatchSyntaxBased(), SHORT);

	// Append LF at end of files on save 
	window->appendLFItem_ = createMenuToggle(subPane, "appendLFItem", "Terminate with Line Break on Save", 'v', appendLFCB, nullptr, GetPrefAppendLF(), FULL);

	window->reposDlogsDefItem_ = createMenuToggle(subPane, "popupsUnderPointer", "Popups Under Pointer", 'P', reposDlogsDefCB, window, GetPrefRepositionDialogs(), FULL);
	window->autoScrollDefItem_ = createMenuToggle(subPane, "autoScroll", "Auto Scroll Near Window Top/Bottom", 0, autoScrollDefCB, window, GetPrefAutoScroll(), FULL);
	subSubPane = createMenu(subPane, "warnings", "Warnings", 'r', nullptr, FULL);
	window->modWarnDefItem_ = createMenuToggle(subSubPane, "filesModifiedExternally", "Files Modified Externally", 'F', modWarnDefCB, window, GetPrefWarnFileMods(), FULL);
	window->modWarnRealDefItem_ = createMenuToggle(subSubPane, "checkModifiedFileContents", "Check Modified File Contents", 'C', modWarnRealDefCB, window, GetPrefWarnRealFileMods(), FULL);
	XtSetSensitive(window->modWarnRealDefItem_, GetPrefWarnFileMods());
	window->exitWarnDefItem_ = createMenuToggle(subSubPane, "onExit", "On Exit", 'O', exitWarnDefCB, window, GetPrefWarnExit(), FULL);

	// Initial Window Size sub menu (simulates radioBehavior) 
	subSubPane = createMenu(subPane, "initialwindowSize", "Initial Window Size", 'z', nullptr, FULL);
	// XtVaSetValues(subSubPane, XmNradioBehavior, True, nullptr);  
	window->size24x80DefItem_ = btn = createMenuToggle(subSubPane, "24X80", "24 x 80", '2', size24x80CB, window, False, SHORT);
	XtVaSetValues(btn, XmNindicatorType, XmONE_OF_MANY, nullptr);
	window->size40x80DefItem_ = btn = createMenuToggle(subSubPane, "40X80", "40 x 80", '4', size40x80CB, window, False, SHORT);
	XtVaSetValues(btn, XmNindicatorType, XmONE_OF_MANY, nullptr);
	window->size60x80DefItem_ = btn = createMenuToggle(subSubPane, "60X80", "60 x 80", '6', size60x80CB, window, False, SHORT);
	XtVaSetValues(btn, XmNindicatorType, XmONE_OF_MANY, nullptr);
	window->size80x80DefItem_ = btn = createMenuToggle(subSubPane, "80X80", "80 x 80", '8', size80x80CB, window, False, SHORT);
	XtVaSetValues(btn, XmNindicatorType, XmONE_OF_MANY, nullptr);
	window->sizeCustomDefItem_ = btn = createMenuToggle(subSubPane, "custom", "Custom...", 'C', sizeCustomCB, window, False, SHORT);
	XtVaSetValues(btn, XmNindicatorType, XmONE_OF_MANY, nullptr);
	updateWindowSizeMenu(window);

	/*
	** Remainder of Preferences menu
	*/
	createMenuSeparator(subPane, "sep1", SHORT);	

	/*
	** Create the Shell menu
	*/
	menuPane = window->shellMenuPane_ = createMenu(menuBar, "shellMenu", "Shell", 0, &cascade, FULL);
	btn = createMenuItem(menuPane, "executeCommand", "Execute Command...", 'E', doActionCB, "execute_command_dialog", SHORT);
	XtVaSetValues(btn, XmNuserData, PERMANENT_MENU_ITEM, nullptr);
	btn = createMenuItem(menuPane, "executeCommandLine", "Execute Command Line", 'x', doActionCB, "execute_command_line", SHORT);
	XtVaSetValues(btn, XmNuserData, PERMANENT_MENU_ITEM, nullptr);
	window->filterItem_ = createMenuItem(menuPane, "filterSelection", "Filter Selection...", 'F', doActionCB, "filter_selection_dialog", SHORT);
	XtVaSetValues(window->filterItem_, XmNuserData, PERMANENT_MENU_ITEM, XmNsensitive, window->wasSelected_, nullptr);
	window->cancelShellItem_ = createMenuItem(menuPane, "cancelShellCommand", "Cancel Shell Command", 'C', cancelShellCB, window, SHORT);
	XtVaSetValues(window->cancelShellItem_, XmNuserData, PERMANENT_MENU_ITEM, XmNsensitive, False, nullptr);
	btn = createMenuSeparator(menuPane, "sep1", SHORT);
	XtVaSetValues(btn, XmNuserData, PERMANENT_MENU_ITEM, nullptr);

	/*
	** Create the Macro menu
	*/
	menuPane = window->macroMenuPane_ = createMenu(menuBar, "macroMenu", "Macro", 0, &cascade, FULL);
	window->learnItem_ = createMenuItem(menuPane, "learnKeystrokes", "Learn Keystrokes", 'L', learnCB, window, SHORT);
	XtVaSetValues(window->learnItem_, XmNuserData, PERMANENT_MENU_ITEM, nullptr);
	window->finishLearnItem_ = createMenuItem(menuPane, "finishLearn", "Finish Learn", 'F', finishLearnCB, window, SHORT);
	XtVaSetValues(window->finishLearnItem_, XmNuserData, PERMANENT_MENU_ITEM, XmNsensitive, False, nullptr);
	window->cancelMacroItem_ = createMenuItem(menuPane, "cancelLearn", "Cancel Learn", 'C', cancelLearnCB, window, SHORT);
	XtVaSetValues(window->cancelMacroItem_, XmNuserData, PERMANENT_MENU_ITEM, XmNsensitive, False, nullptr);
	window->replayItem_ = createMenuItem(menuPane, "replayKeystrokes", "Replay Keystrokes", 'K', replayCB, window, SHORT);
	XtVaSetValues(window->replayItem_, XmNuserData, PERMANENT_MENU_ITEM, XmNsensitive, !GetReplayMacro().empty(), nullptr);
	window->repeatItem_ = createMenuItem(menuPane, "repeat", "Repeat...", 'R', doActionCB, "repeat_dialog", SHORT);
	XtVaSetValues(window->repeatItem_, XmNuserData, PERMANENT_MENU_ITEM, nullptr);
	btn = createMenuSeparator(menuPane, "sep1", SHORT);
	XtVaSetValues(btn, XmNuserData, PERMANENT_MENU_ITEM, nullptr);

	/*
	** Create the Windows menu
	*/
	menuPane = window->windowMenuPane_ = createMenu(menuBar, "windowsMenu", "Windows", 0, &cascade, FULL);
	XtAddCallback(cascade, XmNcascadingCallback, windowMenuCB, window);
	window->splitPaneItem_ = createMenuItem(menuPane, "splitPane", "Split Pane", 'S', doActionCB, "split_pane", SHORT);
	XtVaSetValues(window->splitPaneItem_, XmNuserData, PERMANENT_MENU_ITEM, nullptr);

	btn = createMenuSeparator(menuPane, "sep01", SHORT);
	XtVaSetValues(btn, XmNuserData, PERMANENT_MENU_ITEM, nullptr);
	window->detachDocumentItem_ = createMenuItem(menuPane, "detachBuffer", "Detach Tab", 'D', doActionCB, "detach_document", SHORT);
	XtSetSensitive(window->detachDocumentItem_, False);


	btn = createMenuSeparator(menuPane, "sep1", SHORT);
	XtVaSetValues(btn, XmNuserData, PERMANENT_MENU_ITEM, nullptr);

	/*
	** Create "Help" pull down menu.
	*/
	menuPane = createMenu(menuBar, "helpMenu", "Help", 0, &cascade, SHORT);
	XtVaSetValues(menuBar, XmNmenuHelpWidget, cascade, nullptr);

	return menuBar;
}


/*----------------------------------------------------------------------------*/

/*
** handle actions called from the context menus of tabs.
*/
static void doTabActionCB(Widget w, XtPointer clientData, XtPointer callData) {
	Widget menu = MENU_WIDGET(w);
	Document *win, *window = Document::WidgetToWindow(menu);

	/* extract the window to be acted upon, see comment in
	   tabMenuPostAP() for detail */
	XtVaGetValues(window->tabMenuPane_, XmNuserData, &win, nullptr);

	HidePointerOnKeyedEvent(win->lastFocus_, static_cast<XmAnyCallbackStruct *>(callData)->event);
	XtCallActionProc(win->lastFocus_, (char *)clientData, static_cast<XmAnyCallbackStruct *>(callData)->event, nullptr, 0);
}

static void doActionCB(Widget w, XtPointer clientData, XtPointer callData) {
	Widget menu = MENU_WIDGET(w);
	Widget widget = Document::WidgetToWindow(menu)->lastFocus_;
	String action = (String)clientData;
	XEvent *event = static_cast<XmAnyCallbackStruct *>(callData)->event;

	HidePointerOnKeyedEvent(widget, event);

	XtCallActionProc(widget, action, event, nullptr, 0);
}






static void autoSaveDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);
	Q_UNUSED(w);

	bool state = XmToggleButtonGetState(w);

	// Set the preference and make the other windows' menus agree 
	SetPrefAutoSave(state);
	
	
	for(Document *win: WindowList) {
		if (win->IsTopDocument())
			XmToggleButtonSetState(win->autoSaveDefItem_, state, False);
	}
}

static void preserveDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);
	Q_UNUSED(w);

	bool state = XmToggleButtonGetState(w);

	// Set the preference and make the other windows' menus agree 
	SetPrefSaveOldVersion(state);
	for(Document *win: WindowList) {
		if (win->IsTopDocument())
			XmToggleButtonSetState(win->saveLastDefItem_, state, False);
	}
}


static void showMatchingOffDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);
	Q_UNUSED(w);

	// Set the preference and make the other windows' menus agree 
	SetPrefShowMatching(NO_FLASH);
	for(Document *win: WindowList) {
		if (win->IsTopDocument()) {
			XmToggleButtonSetState(win->showMatchingOffDefItem_, True, False);
			XmToggleButtonSetState(win->showMatchingDelimitDefItem_, False, False);
			XmToggleButtonSetState(win->showMatchingRangeDefItem_, False, False);
		}
	}
}

static void showMatchingDelimitDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);
	Q_UNUSED(w);

	// Set the preference and make the other windows' menus agree 
	SetPrefShowMatching(FLASH_DELIMIT);
	for(Document *win: WindowList) {
		if (win->IsTopDocument()) {
			XmToggleButtonSetState(win->showMatchingOffDefItem_, False, False);
			XmToggleButtonSetState(win->showMatchingDelimitDefItem_, True, False);
			XmToggleButtonSetState(win->showMatchingRangeDefItem_, False, False);
		}
	}
}

static void showMatchingRangeDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);
	Q_UNUSED(w);

	// Set the preference and make the other windows' menus agree 
	SetPrefShowMatching(FLASH_RANGE);
	for(Document *win: WindowList) {
		if (win->IsTopDocument()) {
			XmToggleButtonSetState(win->showMatchingOffDefItem_, False, False);
			XmToggleButtonSetState(win->showMatchingDelimitDefItem_, False, False);
			XmToggleButtonSetState(win->showMatchingRangeDefItem_, True, False);
		}
	}
}

static void matchSyntaxBasedDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	bool state = XmToggleButtonGetState(w);

	// Set the preference and make the other windows' menus agree 
	SetPrefMatchSyntaxBased(state);
	for(Document *win: WindowList) {
		if (win->IsTopDocument())
			XmToggleButtonSetState(win->matchSyntaxBasedDefItem_, state, False);
	}
}

static void backlightCharsDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	bool state = XmToggleButtonGetState(w);

	// Set the preference and make the other windows' menus agree 
	SetPrefBacklightChars(state);
	for(Document *win: WindowList) {
		if (win->IsTopDocument())
			XmToggleButtonSetState(win->backlightCharsDefItem_, state, False);
	}
}

static void highlightOffDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);
	Q_UNUSED(w);

	// Set the preference and make the other windows' menus agree 
	SetPrefHighlightSyntax(False);
	for(Document *win: WindowList) {
		if (win->IsTopDocument()) {
			XmToggleButtonSetState(win->highlightOffDefItem_, True, False);
			XmToggleButtonSetState(win->highlightDefItem_, False, False);
		}
	}
}

static void highlightDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);
	Q_UNUSED(w);

	// Set the preference and make the other windows' menus agree 
	SetPrefHighlightSyntax(True);
	for(Document *win: WindowList) {
		if (win->IsTopDocument()) {
			XmToggleButtonSetState(win->highlightOffDefItem_, False, False);
			XmToggleButtonSetState(win->highlightDefItem_, True, False);
		}
	}
}

static void highlightingDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	HidePointerOnKeyedEvent(Document::WidgetToWindow(MENU_WIDGET(w))->lastFocus_, static_cast<XmAnyCallbackStruct *>(callData)->event);
	EditHighlightPatterns(Document::WidgetToWindow(MENU_WIDGET(w)));
}

static void stylesDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	HidePointerOnKeyedEvent(Document::WidgetToWindow(MENU_WIDGET(w))->lastFocus_, static_cast<XmAnyCallbackStruct *>(callData)->event);
	EditHighlightStyles(nullptr);
}

static void searchDlogsDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	bool state = XmToggleButtonGetState(w);

	// Set the preference and make the other windows' menus agree 
	SetPrefSearchDlogs(state);
	for(Document *win: WindowList) {
		if (win->IsTopDocument())
			XmToggleButtonSetState(win->searchDlogsDefItem_, state, False);
	}
}

static void beepOnSearchWrapDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	bool state = XmToggleButtonGetState(w);

	// Set the preference and make the other windows' menus agree 
	SetPrefBeepOnSearchWrap(state);
	for(Document *win: WindowList) {
		if (win->IsTopDocument())
			XmToggleButtonSetState(win->beepOnSearchWrapDefItem_, state, False);
	}
}

static void keepSearchDlogsDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	bool state = XmToggleButtonGetState(w);

	// Set the preference and make the other windows' menus agree 
	SetPrefKeepSearchDlogs(state);
	for(Document *win: WindowList) {
		if (win->IsTopDocument())
			XmToggleButtonSetState(win->keepSearchDlogsDefItem_, state, False);
	}
}

static void searchWrapsDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	bool state = XmToggleButtonGetState(w);

	// Set the preference and make the other windows' menus agree 
	SetPrefSearchWraps(state);
	for(Document *win: WindowList) {
		if (win->IsTopDocument())
			XmToggleButtonSetState(win->searchWrapsDefItem_, state, False);
	}
}

static void appendLFCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	bool state = XmToggleButtonGetState(w);

	SetPrefAppendLF(state);
	for(Document *win: WindowList) {
		if (win->IsTopDocument())
			XmToggleButtonSetState(win->appendLFItem_, state, False);
	}
}

static void reposDlogsDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	bool state = XmToggleButtonGetState(w);

	// Set the preference and make the other windows' menus agree 
	SetPrefRepositionDialogs(state);
	SetPointerCenteredDialogs(state);
	for(Document *win: WindowList) {
		if (win->IsTopDocument())
			XmToggleButtonSetState(win->reposDlogsDefItem_, state, False);
	}
}

static void autoScrollDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	bool state = XmToggleButtonGetState(w);

	// Set the preference and make the other windows' menus agree 
	SetPrefAutoScroll(state);
	// XXX: Should we ensure auto-scroll now if needed? 
	for(Document *win: WindowList) {
		if (win->IsTopDocument())
			XmToggleButtonSetState(win->autoScrollDefItem_, state, False);
	}
}

static void modWarnDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	bool state = XmToggleButtonGetState(w);

	// Set the preference and make the other windows' menus agree 
	SetPrefWarnFileMods(state);
	for(Document *win: WindowList) {
		if (win->IsTopDocument()) {
			XmToggleButtonSetState(win->modWarnDefItem_, state, False);
			XtSetSensitive(win->modWarnRealDefItem_, state);
		}
	}
}

static void modWarnRealDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);
	
	bool state = XmToggleButtonGetState(w);

	// Set the preference and make the other windows' menus agree 
	SetPrefWarnRealFileMods(state);
	for(Document *win: WindowList) {
		if (win->IsTopDocument())
			XmToggleButtonSetState(win->modWarnRealDefItem_, state, False);
	}
}

static void exitWarnDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	bool state = XmToggleButtonGetState(w);

	// Set the preference and make the other windows' menus agree 
	SetPrefWarnExit(state);
	for(Document *win: WindowList) {
		if (win->IsTopDocument())
			XmToggleButtonSetState(win->exitWarnDefItem_, state, False);
	}
}

static void openInTabDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	bool state = XmToggleButtonGetState(w);

	// Set the preference and make the other windows' menus agree 
	SetPrefOpenInTab(state);
	for(Document *win: WindowList) {
		XmToggleButtonSetState(win->openInTabDefItem_, state, False);
	}
}

static void tabBarDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	bool state = XmToggleButtonGetState(w);

	// Set the preference and make the other windows' menus agree 
	SetPrefTabBar(state);
	for(Document *win: WindowList) {
		if (win->IsTopDocument()) {
			XmToggleButtonSetState(win->tabBarDefItem_, state, False);
			win->ShowWindowTabBar();
		}
	}
}

static void tabBarHideDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	bool state = XmToggleButtonGetState(w);

	// Set the preference and make the other windows' menus agree 
	SetPrefTabBarHideOne(state);
	for(Document *win: WindowList) {
		if (win->IsTopDocument()) {
			XmToggleButtonSetState(win->tabBarHideDefItem_, state, False);
			win->ShowWindowTabBar();
		}
	}
}

static void toolTipsDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	bool state = XmToggleButtonGetState(w);

	// Set the preference and make the other windows' menus agree 
	SetPrefToolTips(state);
	for(Document *win: WindowList) {
		XtVaSetValues(win->tab_, XltNshowBubble, GetPrefToolTips(), nullptr);
		if (win->IsTopDocument()) {
			XmToggleButtonSetState(win->toolTipsDefItem_, state, False);
		}
	}
}

static void tabNavigateDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	bool state = XmToggleButtonGetState(w);

	// Set the preference and make the other windows' menus agree 
	SetPrefGlobalTabNavigate(state);
	for(Document *win: WindowList) {
		if (win->IsTopDocument())
			XmToggleButtonSetState(win->tabNavigateDefItem_, state, False);
	}
}

static void tabSortDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	bool state = XmToggleButtonGetState(w);

	// Set the preference and make the other windows' menus agree 
	SetPrefSortTabs(state);
	for(Document *win: WindowList) {
		if (win->IsTopDocument()) {
			XmToggleButtonSetState(win->tabSortDefItem_, state, False);
		}
	}

	/* If we just enabled sorting, sort all tabs.  Note that this reorders
	   the next pointers underneath us, which is scary, but SortTabBar never
	   touches windows that are earlier in the WindowList so it's ok. */
	if (state) {
		Widget shell = nullptr;
		for(Document *win: WindowList) {
			if (win->shell_ != shell) {
				win->SortTabBar();
				shell = win->shell_;
			}
		}
	}
}

static void statsLineDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	bool state = XmToggleButtonGetState(w);

	// Set the preference and make the other windows' menus agree 
	SetPrefStatsLine(state);
	for(Document *win: WindowList) {
		if (win->IsTopDocument()) {
			XmToggleButtonSetState(win->statsLineDefItem_, state, False);
		}
	}
}

static void iSearchLineDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	bool state = XmToggleButtonGetState(w);

	// Set the preference and make the other windows' menus agree 
	SetPrefISearchLine(state);
	for(Document *win: WindowList) {
		if (win->IsTopDocument())
			XmToggleButtonSetState(win->iSearchLineDefItem_, state, False);
	}
}

static void lineNumsDefCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	bool state = XmToggleButtonGetState(w);

	// Set the preference and make the other windows' menus agree 
	SetPrefLineNums(state);
	for(Document *win: WindowList) {
		if (win->IsTopDocument())
			XmToggleButtonSetState(win->lineNumsDefItem_, state, False);
	}
}

static void searchLiteralCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	// Set the preference and make the other windows' menus agree 
	if (XmToggleButtonGetState(w)) {
		SetPrefSearch(SEARCH_LITERAL);
		for(Document *win: WindowList) {
			if (win->IsTopDocument()) {
				XmToggleButtonSetState(win->searchLiteralDefItem_, True, False);
				XmToggleButtonSetState(win->searchCaseSenseDefItem_, False, False);
				XmToggleButtonSetState(win->searchLiteralWordDefItem_, False, False);
				XmToggleButtonSetState(win->searchCaseSenseWordDefItem_, False, False);
				XmToggleButtonSetState(win->searchRegexDefItem_, False, False);
				XmToggleButtonSetState(win->searchRegexNoCaseDefItem_, False, False);
			}
		}
	}
}

static void searchCaseSenseCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	// Set the preference and make the other windows' menus agree 
	if (XmToggleButtonGetState(w)) {
		SetPrefSearch(SEARCH_CASE_SENSE);
		for(Document *win: WindowList) {
			if (win->IsTopDocument()) {
				XmToggleButtonSetState(win->searchLiteralDefItem_, False, False);
				XmToggleButtonSetState(win->searchCaseSenseDefItem_, True, False);
				XmToggleButtonSetState(win->searchLiteralWordDefItem_, False, False);
				XmToggleButtonSetState(win->searchCaseSenseWordDefItem_, False, False);
				XmToggleButtonSetState(win->searchRegexDefItem_, False, False);
				XmToggleButtonSetState(win->searchRegexNoCaseDefItem_, False, False);
			}
		}
	}
}

static void searchLiteralWordCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	// Set the preference and make the other windows' menus agree 
	if (XmToggleButtonGetState(w)) {
		SetPrefSearch(SEARCH_LITERAL_WORD);
		for(Document *win: WindowList) {
			if (win->IsTopDocument()) {
				XmToggleButtonSetState(win->searchLiteralDefItem_, False, False);
				XmToggleButtonSetState(win->searchCaseSenseDefItem_, False, False);
				XmToggleButtonSetState(win->searchLiteralWordDefItem_, True, False);
				XmToggleButtonSetState(win->searchCaseSenseWordDefItem_, False, False);
				XmToggleButtonSetState(win->searchRegexDefItem_, False, False);
				XmToggleButtonSetState(win->searchRegexNoCaseDefItem_, False, False);
			}
		}
	}
}

static void searchCaseSenseWordCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	// Set the preference and make the other windows' menus agree 
	if (XmToggleButtonGetState(w)) {
		SetPrefSearch(SEARCH_CASE_SENSE_WORD);
		for(Document *win: WindowList) {
			if (win->IsTopDocument()) {
				XmToggleButtonSetState(win->searchLiteralDefItem_, False, False);
				XmToggleButtonSetState(win->searchCaseSenseDefItem_, False, False);
				XmToggleButtonSetState(win->searchLiteralWordDefItem_, False, False);
				XmToggleButtonSetState(win->searchCaseSenseWordDefItem_, True, False);
				XmToggleButtonSetState(win->searchRegexDefItem_, False, False);
				XmToggleButtonSetState(win->searchRegexNoCaseDefItem_, False, False);
			}
		}
	}
}

static void searchRegexCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	// Set the preference and make the other windows' menus agree 
	if (XmToggleButtonGetState(w)) {
		SetPrefSearch(SEARCH_REGEX);
		for(Document *win: WindowList) {
			if (win->IsTopDocument()) {
				XmToggleButtonSetState(win->searchLiteralDefItem_, False, False);
				XmToggleButtonSetState(win->searchCaseSenseDefItem_, False, False);
				XmToggleButtonSetState(win->searchLiteralWordDefItem_, False, False);
				XmToggleButtonSetState(win->searchCaseSenseWordDefItem_, False, False);
				XmToggleButtonSetState(win->searchRegexDefItem_, True, False);
				XmToggleButtonSetState(win->searchRegexNoCaseDefItem_, False, False);
			}
		}
	}
}

static void searchRegexNoCaseCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	// Set the preference and make the other windows' menus agree 
	if (XmToggleButtonGetState(w)) {
		SetPrefSearch(SEARCH_REGEX_NOCASE);
		for(Document *win: WindowList) {
			if (win->IsTopDocument()) {
				XmToggleButtonSetState(win->searchLiteralDefItem_, False, False);
				XmToggleButtonSetState(win->searchCaseSenseDefItem_, False, False);
				XmToggleButtonSetState(win->searchLiteralWordDefItem_, False, False);
				XmToggleButtonSetState(win->searchCaseSenseWordDefItem_, False, False);
				XmToggleButtonSetState(win->searchRegexDefItem_, False, False);
				XmToggleButtonSetState(win->searchRegexNoCaseDefItem_, True, False);
			}
		}
	}
}

#ifdef REPLACE_SCOPE
static void replaceScopeWindowCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	// Set the preference and make the other windows' menus agree 
	if (XmToggleButtonGetState(w)) {
		SetPrefReplaceDefScope(REPL_DEF_SCOPE_WINDOW);
		for(Document *win: WindowList) {
			if (win->IsTopDocument()) {
				XmToggleButtonSetState(win->replScopeWinDefItem_, True, False);
				XmToggleButtonSetState(win->replScopeSelDefItem_, False, False);
				XmToggleButtonSetState(win->replScopeSmartDefItem_, False, False);
			}
		}
	}
}

static void replaceScopeSelectionCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	// Set the preference and make the other windows' menus agree 
	if (XmToggleButtonGetState(w)) {
		SetPrefReplaceDefScope(REPL_DEF_SCOPE_SELECTION);
		for(Document *win: WindowList) {
			if (win->IsTopDocument()) {
				XmToggleButtonSetState(win->replScopeWinDefItem_, False, False);
				XmToggleButtonSetState(win->replScopeSelDefItem_, True, False);
				XmToggleButtonSetState(win->replScopeSmartDefItem_, False, False);
			}
		}
	}
}

static void replaceScopeSmartCB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	// Set the preference and make the other windows' menus agree 
	if (XmToggleButtonGetState(w)) {
		SetPrefReplaceDefScope(REPL_DEF_SCOPE_SMART);
		for(Document *win: WindowList) {
			if (win->IsTopDocument()) {
				XmToggleButtonSetState(win->replScopeWinDefItem_, False, False);
				XmToggleButtonSetState(win->replScopeSelDefItem_, False, False);
				XmToggleButtonSetState(win->replScopeSmartDefItem_, True, False);
			}
		}
	}
}
#endif

static void size24x80CB(Widget w, XtPointer clientData, XtPointer callData) {

	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	HidePointerOnKeyedEvent(Document::WidgetToWindow(MENU_WIDGET(w))->lastFocus_, static_cast<XmAnyCallbackStruct *>(callData)->event);
	setWindowSizeDefault(24, 80);
}

static void size40x80CB(Widget w, XtPointer clientData, XtPointer callData) {
	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	HidePointerOnKeyedEvent(Document::WidgetToWindow(MENU_WIDGET(w))->lastFocus_, static_cast<XmAnyCallbackStruct *>(callData)->event);
	setWindowSizeDefault(40, 80);
}

static void size60x80CB(Widget w, XtPointer clientData, XtPointer callData) {
	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	HidePointerOnKeyedEvent(Document::WidgetToWindow(MENU_WIDGET(w))->lastFocus_, static_cast<XmAnyCallbackStruct *>(callData)->event);
	setWindowSizeDefault(60, 80);
}

static void size80x80CB(Widget w, XtPointer clientData, XtPointer callData) {
	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	HidePointerOnKeyedEvent(Document::WidgetToWindow(MENU_WIDGET(w))->lastFocus_, static_cast<XmAnyCallbackStruct *>(callData)->event);
	setWindowSizeDefault(80, 80);
}

static void sizeCustomCB(Widget w, XtPointer clientData, XtPointer callData) {
	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	HidePointerOnKeyedEvent(Document::WidgetToWindow(MENU_WIDGET(w))->lastFocus_, static_cast<XmAnyCallbackStruct *>(callData)->event);
	
	
	auto dialog = new DialogWindowSize(nullptr /*Document::WidgetToWindow(MENU_WIDGET(w))->shell_*/);
	dialog->exec();
	delete dialog;
	
	updateWindowSizeMenus();
}

static void cancelShellCB(Widget w, XtPointer clientData, XtPointer callData) {
	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	HidePointerOnKeyedEvent(Document::WidgetToWindow(MENU_WIDGET(w))->lastFocus_, static_cast<XmAnyCallbackStruct *>(callData)->event);
	AbortShellCommand(Document::WidgetToWindow(MENU_WIDGET(w)));
}

static void learnCB(Widget w, XtPointer clientData, XtPointer callData) {
	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	HidePointerOnKeyedEvent(Document::WidgetToWindow(MENU_WIDGET(w))->lastFocus_, static_cast<XmAnyCallbackStruct *>(callData)->event);
	BeginLearn(Document::WidgetToWindow(MENU_WIDGET(w)));
}

static void finishLearnCB(Widget w, XtPointer clientData, XtPointer callData) {
	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	HidePointerOnKeyedEvent(Document::WidgetToWindow(MENU_WIDGET(w))->lastFocus_, static_cast<XmAnyCallbackStruct *>(callData)->event);
	FinishLearn();
}

static void cancelLearnCB(Widget w, XtPointer clientData, XtPointer callData) {
	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	HidePointerOnKeyedEvent(Document::WidgetToWindow(MENU_WIDGET(w))->lastFocus_, static_cast<XmAnyCallbackStruct *>(callData)->event);
	CancelMacroOrLearn(Document::WidgetToWindow(MENU_WIDGET(w)));
}

static void replayCB(Widget w, XtPointer clientData, XtPointer callData) {
	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	HidePointerOnKeyedEvent(Document::WidgetToWindow(MENU_WIDGET(w))->lastFocus_, static_cast<XmAnyCallbackStruct *>(callData)->event);
	Replay(Document::WidgetToWindow(MENU_WIDGET(w)));
}

static void windowMenuCB(Widget w, XtPointer clientData, XtPointer callData) {
	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	Document *window = Document::WidgetToWindow(MENU_WIDGET(w));

	if (!window->windowMenuValid_) {
		updateWindowMenu(window);
		window->windowMenuValid_ = True;
	}
}

/*
** open a new tab or window.
*/
static void newAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	Q_UNUSED(event);

	Document *window = Document::WidgetToWindow(w);
	int openInTab = GetPrefOpenInTab();

	if (*nArgs > 0) {
		if (strcmp(args[0], "prefs") == 0) {
			/* accept default */;
		} else if (strcmp(args[0], "tab") == 0) {
			openInTab = 1;
		} else if (strcmp(args[0], "window") == 0) {
			openInTab = 0;
		} else if (strcmp(args[0], "opposite") == 0) {
			openInTab = !openInTab;
		} else {
			fprintf(stderr, "nedit: Unknown argument to action procedure \"new\": %s\n", args[0]);
		}
	}

	EditNewFile(openInTab ? window : nullptr, nullptr, False, nullptr, window->path_.toLatin1().data());
	CheckCloseDim();
}

/*
** These are just here because our techniques make it hard to bind a menu item
** to an action procedure that takes arguments.  The user doesn't need to know
** about them -- they can use new( "opposite" ) or new( "tab" ).
*/
static void newOppositeAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	Q_UNUSED(event);
	Q_UNUSED(args);
	Q_UNUSED(nArgs)

	Document *window = Document::WidgetToWindow(w);

	EditNewFile(GetPrefOpenInTab() ? nullptr : window, nullptr, False, nullptr, window->path_.toLatin1().data());
	CheckCloseDim();
}
static void newTabAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	Q_UNUSED(event);
	Q_UNUSED(args);
	Q_UNUSED(nArgs)

	Document *window = Document::WidgetToWindow(w);

	EditNewFile(window, nullptr, False, nullptr, window->path_.toLatin1().data());
	CheckCloseDim();
}

static void openDialogAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	Q_UNUSED(event);
	Q_UNUSED(args);
	Q_UNUSED(nArgs)

	Document *window = Document::WidgetToWindow(w);
	const char *params[2];
	int n = 1;

	QString filename = PromptForExistingFile(window, QLatin1String("Open File"));
	if (filename.isNull()) {
		return;
	}
	
	QByteArray fileStr = filename.toLatin1();
	params[0] = fileStr.data();

	if (*nArgs > 0 && !strcmp(args[0], "1")) {
		params[n++] = "1";
	}

	XtCallActionProc(window->lastFocus_, "open", event, const_cast<char **>(params), n);
	CheckCloseDim();
}

static void openAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	Q_UNUSED(event);
	Q_UNUSED(args);
	Q_UNUSED(nArgs)

	Document *window = Document::WidgetToWindow(w);
	char filename[MAXPATHLEN], pathname[MAXPATHLEN];

	if (*nArgs == 0) {
		fprintf(stderr, "nedit: open action requires file argument\n");
		return;
	}
	if (ParseFilename(args[0], filename, pathname) != 0 || strlen(filename) + strlen(pathname) > MAXPATHLEN - 1) {
		fprintf(stderr, "nedit: invalid file name for open action: %s\n", args[0]);
		return;
	}
	EditExistingFile(window, QLatin1String(filename), QLatin1String(pathname), 0, nullptr, False, nullptr, GetPrefOpenInTab(), False);
	CheckCloseDim();
}

static void closeAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	Q_UNUSED(event);
	Q_UNUSED(args);
	Q_UNUSED(nArgs)

	int preResponse = PROMPT_SBC_DIALOG_RESPONSE;

	if (*nArgs > 0) {
		if (strcmp(args[0], "prompt") == 0) {
			preResponse = PROMPT_SBC_DIALOG_RESPONSE;
		} else if (strcmp(args[0], "save") == 0) {
			preResponse = YES_SBC_DIALOG_RESPONSE;
		} else if (strcmp(args[0], "nosave") == 0) {
			preResponse = NO_SBC_DIALOG_RESPONSE;
		}
	}
	CloseFileAndWindow(Document::WidgetToWindow(w), preResponse);
	CheckCloseDim();
}

static void saveAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	Q_UNUSED(event);
	Q_UNUSED(args);
	Q_UNUSED(nArgs)

	Document *window = Document::WidgetToWindow(w);

	if (CheckReadOnly(window))
		return;
	SaveWindow(window);
}

static void saveAsDialogAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	Q_UNUSED(event);
	Q_UNUSED(args);
	Q_UNUSED(nArgs)

	Document *window = Document::WidgetToWindow(w);
	bool addWrap;
	FileFormats fileFormat;
	char fullname[MAXPATHLEN];
	const char *params[2];

	bool response = PromptForNewFile(window, "Save File As", fullname, &fileFormat, &addWrap);
	if (!response) {
		return;
	}
	
	window->fileFormat_ = fileFormat;
	params[0] = fullname;
	params[1] = "wrapped";
	XtCallActionProc(window->lastFocus_, "save_as", event, const_cast<char **>(params), addWrap ? 2 : 1);
}

static void saveAsAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	Q_UNUSED(event);

	if (*nArgs == 0) {
		fprintf(stderr, "nedit: save_as action requires file argument\n");
		return;
	}
	SaveWindowAs(Document::WidgetToWindow(w), args[0], *nArgs == 2 && !strCaseCmp(args[1], "wrapped"));
}

static void revertDialogAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	Q_UNUSED(event);
	Q_UNUSED(args);
	Q_UNUSED(nArgs)

	Document *window = Document::WidgetToWindow(w);

	// re-reading file is irreversible, prompt the user first 
	if (window->fileChanged_) {
	
		QMessageBox messageBox(nullptr /*window->shell_*/);
		messageBox.setWindowTitle(QLatin1String("Discard Changes"));
		messageBox.setIcon(QMessageBox::Question);
		messageBox.setText(QString(QLatin1String("Discard changes to\n%1%2?")).arg(window->path_).arg(window->filename_));
		QPushButton *buttonOk   = messageBox.addButton(QMessageBox::Ok);
		QPushButton *buttonCancel = messageBox.addButton(QMessageBox::Cancel);
		Q_UNUSED(buttonOk);

		messageBox.exec();
		if(messageBox.clickedButton() == buttonCancel) {
			return;
		}
		
	} else {
	
		QMessageBox messageBox(nullptr /*window->shell_*/);
		messageBox.setWindowTitle(QLatin1String("Reload File"));
		messageBox.setIcon(QMessageBox::Question);
		messageBox.setText(QString(QLatin1String("Re-load file\n%1%2?")).arg(window->path_).arg(window->filename_));
		QPushButton *buttonOk   = messageBox.addButton(QLatin1String("Re-read"), QMessageBox::AcceptRole);
		QPushButton *buttonCancel = messageBox.addButton(QMessageBox::Cancel);
		Q_UNUSED(buttonOk);

		messageBox.exec();
		if(messageBox.clickedButton() == buttonCancel) {
			return;
		}	
	}

	
	XtCallActionProc(window->lastFocus_, "revert_to_saved", event, nullptr, 0);
}

static void revertAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	Q_UNUSED(event);
	Q_UNUSED(args);
	Q_UNUSED(nArgs)

	RevertToSaved(Document::WidgetToWindow(w));
}


static void exitAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	Q_UNUSED(event);
	Q_UNUSED(args);
	Q_UNUSED(nArgs)

	Document *window = Document::WidgetToWindow(w);
	
	const int DF_MAX_MSG_LENGTH = 2048;
	
	
	auto it = std::find_if(WindowList.begin(), WindowList.end(), [window](Document *doc) {
		return doc == window;
	});
	
	if (!CheckPrefsChangesSaved(window->shell_)) {
		return;
	}

	/* If this is not the last window (more than one window is open),
	   confirm with the user before exiting. */
	// NOTE(eteran): test if the current window is NOT the only window
	if (GetPrefWarnExit() && !(it == WindowList.begin() && std::next(it) == WindowList.end())) {
	
		QString exitMsg(QLatin1String("Editing: "));
		
		/* List the windows being edited and make sure the
		   user really wants to exit */	
		// This code assembles a list of document names being edited and elides as necessary
		for(auto it = WindowList.begin(); it != WindowList.end(); ++it) {
			
			Document *win = *it;
			
			QString filename = QString(QLatin1String("%1%2")).arg(win->filename_).arg(win->fileChanged_ ? QLatin1String("*") : QLatin1String(""));
						
			if (exitMsg.size() + filename.size() + 30 >= DF_MAX_MSG_LENGTH) {
				exitMsg.append(QLatin1String("..."));
				break;
			}
			
			// NOTE(eteran): test if this is the last window
			if (std::next(it) == WindowList.end()) {
				exitMsg.append(QString(QLatin1String("and %1.")).arg(filename));
			} else {
				exitMsg.append(QString(QLatin1String("%1, ")).arg(filename));
			}
		}
		
		exitMsg.append(QLatin1String("\n\nExit NEdit?"));
		
		QMessageBox messageBox(nullptr /*window->shell_*/);
		messageBox.setWindowTitle(QLatin1String("Exit"));
		messageBox.setIcon(QMessageBox::Question);
		messageBox.setText(exitMsg);
		QPushButton *buttonExit   = messageBox.addButton(QLatin1String("Exit"), QMessageBox::AcceptRole);
		QPushButton *buttonCancel = messageBox.addButton(QMessageBox::Cancel);
		Q_UNUSED(buttonExit);

		messageBox.exec();
		if(messageBox.clickedButton() == buttonCancel) {
			return;
		}
	}

	// Close all files and exit when the last one is closed 
	if (CloseAllFilesAndWindows())
		exit(EXIT_SUCCESS);
}

static void repeatDialogAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	Q_UNUSED(event);
	Q_UNUSED(args);
	Q_UNUSED(nArgs)

	RepeatDialog(Document::WidgetToWindow(w));
}

static void repeatMacroAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	Q_UNUSED(event);

	int how;

	if (*nArgs != 2) {
		fprintf(stderr, "nedit: repeat_macro requires two arguments\n");
		return;
	}
	if (!strcmp(args[0], "in_selection"))
		how = REPEAT_IN_SEL;
	else if (!strcmp(args[0], "to_end"))
		how = REPEAT_TO_END;
	else if (sscanf(args[0], "%d", &how) != 1) {
		fprintf(stderr, "nedit: repeat_macro requires method/count\n");
		return;
	}
	RepeatMacro(Document::WidgetToWindow(w), args[1], how);
}

static void splitPaneAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	Q_UNUSED(args);
	Q_UNUSED(nArgs)
	Q_UNUSED(event);

	Document *window = Document::WidgetToWindow(w);

	window->SplitPane();
	if (window->IsTopDocument()) {
		XtSetSensitive(window->splitPaneItem_, window->textPanes_.size() < MAX_PANES);
		XtSetSensitive(window->closePaneItem_, window->textPanes_.size() > 0);
	}
}

static void nextDocumentAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	Q_UNUSED(args);
	Q_UNUSED(nArgs)
	Q_UNUSED(event);

	Document::WidgetToWindow(w)->NextDocument();
}

static void prevDocumentAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	Q_UNUSED(args);
	Q_UNUSED(nArgs)
	Q_UNUSED(event);

	Document::WidgetToWindow(w)->PreviousDocument();
}

static void lastDocumentAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	Q_UNUSED(args);
	Q_UNUSED(nArgs)
	Q_UNUSED(event);

	Document::WidgetToWindow(w)->LastDocument();
}

static void filterDialogAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	Q_UNUSED(args);
	Q_UNUSED(nArgs)
	
	static DialogFilter *dialog = nullptr;

	Document *window = Document::WidgetToWindow(w);
	char *params[1];

	if (CheckReadOnly(window)) {
		return;
	}
	
	if (!window->buffer_->primary_.selected) {
		QApplication::beep();
		return;
	}	
		
	if(!dialog) {
		dialog = new DialogFilter(nullptr /*window->shell_ */);
	}
	
	int r = dialog->exec();
	if(!r) {
		return;
	}
	
	QString filterText = dialog->ui.textFilter->text();
	if(!filterText.isEmpty()) {
		QByteArray filterString = filterText.toLatin1();
		params[0] = filterString.data();
		XtCallActionProc(w, "filter_selection", event, params, 1);
	}
}

static void shellFilterAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	Document *window = Document::WidgetToWindow(w);

	if (CheckReadOnly(window))
		return;
	if (*nArgs == 0) {
		fprintf(stderr, "nedit: filter_selection requires shell command argument\n");
		return;
	}
	FilterSelection(window, args[0], event->xany.send_event == MACRO_EVENT_MARKER);
}

static void execDialogAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	Q_UNUSED(args);
	Q_UNUSED(nArgs)
	
	static DialogExecuteCommand *dialog = nullptr;

	Document *window = Document::WidgetToWindow(w);
	char *params[1];

	if (CheckReadOnly(window))
		return;
		
	if(!dialog) {
		dialog = new DialogExecuteCommand(nullptr /*window->shell_ */);
	}
	
	int r = dialog->exec();
	if(!r) {
		return;
	}
	
	QString commandText = dialog->ui.textCommand->text();
	if(!commandText.isEmpty()) {
		QByteArray commandString = commandText.toLatin1();
		params[0] = commandString.data();
		XtCallActionProc(w, "execute_command", event, params, 1);	
	}
}

static void execAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	Q_UNUSED(args);
	Q_UNUSED(nArgs)

	Document *window = Document::WidgetToWindow(w);

	if (CheckReadOnly(window))
		return;
	if (*nArgs == 0) {
		fprintf(stderr, "nedit: execute_command requires shell command argument\n");
		return;
	}
	ExecShellCommand(window, args[0], event->xany.send_event == MACRO_EVENT_MARKER);
}

static void execLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	Q_UNUSED(args);
	Q_UNUSED(nArgs)

	Document *window = Document::WidgetToWindow(w);

	if (CheckReadOnly(window))
		return;
	ExecCursorLine(window, event->xany.send_event == MACRO_EVENT_MARKER);
}

static void shellMenuAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	if (*nArgs == 0) {
		fprintf(stderr, "nedit: shell_menu_command requires item-name argument\n");
		return;
	}
	HidePointerOnKeyedEvent(w, event);
	DoNamedShellMenuCmd(Document::WidgetToWindow(w), args[0], event->xany.send_event == MACRO_EVENT_MARKER);
}

static void macroMenuAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	if (*nArgs == 0) {
		fprintf(stderr, "nedit: macro_menu_command requires item-name argument\n");
		return;
	}
	/* Don't allow users to execute a macro command from the menu (or accel)
	   if there's already a macro command executing, UNLESS the macro is
	   directly called from another one.  NEdit can't handle
	   running multiple, independent uncoordinated, macros in the same
	   window.  Macros may invoke macro menu commands recursively via the
	   macro_menu_command action proc, which is important for being able to
	   repeat any operation, and to embed macros within eachother at any
	   level, however, a call here with a macro running means that THE USER
	   is explicitly invoking another macro via the menu or an accelerator,
	   UNLESS the macro event marker is set */
	if (event->xany.send_event != MACRO_EVENT_MARKER) {
		if (Document::WidgetToWindow(w)->macroCmdData_) {
			QApplication::beep();
			return;
		}
	}
	HidePointerOnKeyedEvent(w, event);
	DoNamedMacroMenuCmd(Document::WidgetToWindow(w), args[0]);
}

static void bgMenuAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	if (*nArgs == 0) {
		fprintf(stderr, "nedit: bg_menu_command requires item-name argument\n");
		return;
	}
	// Same remark as for macro menu commands (see above). 
	if (event->xany.send_event != MACRO_EVENT_MARKER) {
		if (Document::WidgetToWindow(w)->macroCmdData_) {
			QApplication::beep();
			return;
		}
	}
	HidePointerOnKeyedEvent(w, event);
	DoNamedBGMenuCmd(Document::WidgetToWindow(w), args[0]);
}

static void beginningOfSelectionAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	Q_UNUSED(nArgs)
	Q_UNUSED(args);
	Q_UNUSED(event);
	
	
	auto textD = textD_of(w);

	TextBuffer *buf = textD->TextGetBuffer();
	int start;
	int end;
	int rectStart;
	int rectEnd;
	bool isRect;

	if (!buf->BufGetSelectionPos(&start, &end, &isRect, &rectStart, &rectEnd)) {
		return;
	}
	
	if (!isRect) {
		textD->TextSetCursorPos(start);
	} else {
		textD->TextSetCursorPos(buf->BufCountForwardDispChars(buf->BufStartOfLine(start), rectStart));
	}
}

static void endOfSelectionAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	Q_UNUSED(nArgs)
	Q_UNUSED(args);
	Q_UNUSED(event);

	auto textD = textD_of(w);
	TextBuffer *buf = textD->TextGetBuffer();
	int start;
	int end;
	int rectStart;
	int rectEnd;
	bool isRect;

	if (!buf->BufGetSelectionPos(&start, &end, &isRect, &rectStart, &rectEnd)) {
		return;
	}

	if (!isRect) {
		textD->TextSetCursorPos(end);
	} else {
		textD->TextSetCursorPos(buf->BufCountForwardDispChars(buf->BufStartOfLine(end), rectEnd));
	}
}

static void raiseWindowAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	Q_UNUSED(event);

	Document *window = Document::WidgetToWindow(w);
	int windowIndex;
	Boolean focus = GetPrefFocusOnRaise();
	
	auto curr = std::find_if(WindowList.begin(), WindowList.end(), [window](Document *doc) {
		return doc == window;
	});	
	
	
	// NOTE(eteran): the list is sorted *backwards*, so all of the iteration is reverse
	//               order here

	if (*nArgs > 0) {
		if (strcmp(args[0], "last") == 0) {
			curr = WindowList.begin();
		} else if (strcmp(args[0], "first") == 0) {
			curr = WindowList.begin();
			if (curr != WindowList.end()) {
			
				// NOTE(eteran): i think this is looking for the last window?
				auto nextWindow = std::next(curr);
				while (nextWindow != WindowList.end()) {
					curr = nextWindow;
					++nextWindow;
				}
			}
		} else if (strcmp(args[0], "previous") == 0) {
			auto tmpWindow = curr;
			curr = WindowList.begin();
			if (curr != WindowList.end()) {
				auto nextWindow = std::next(curr);
				while (nextWindow != WindowList.end() && nextWindow != tmpWindow) {
					curr = nextWindow;
					++nextWindow;
				}
				
				if (nextWindow == WindowList.end() && tmpWindow != WindowList.begin()) {
					curr = WindowList.end();
				}
			}
		} else if (strcmp(args[0], "next") == 0) {
			if (curr != WindowList.end()) {
				++curr;
				if(curr == WindowList.end()) {
					curr = WindowList.begin();
				}
			}
		} else {
			if (sscanf(args[0], "%d", &windowIndex) == 1) {
				if (windowIndex > 0) {
					for (curr = WindowList.begin(); curr != WindowList.end() && windowIndex > 1; --windowIndex) {
						++curr;
					}
				} else if (windowIndex < 0) {
				
				
					for (curr = WindowList.begin(); curr != WindowList.end(); ++curr) {
						++windowIndex;
					}
					
					if (windowIndex >= 0) {
						for (curr = WindowList.begin(); curr != WindowList.end() && windowIndex > 0; ++curr) {
							--windowIndex;
						}
					} else {
						curr = WindowList.end();
					}
				} else {
					curr = WindowList.end();
				}
			} else {
				curr = WindowList.end();
			}
		}

		if (*nArgs > 1) {
			if (strcmp(args[1], "focus") == 0) {
				focus = True;
			} else if (strcmp(args[1], "nofocus") == 0) {
				focus = False;
			}
		}
	}

	if (curr != WindowList.end()) {
		(*curr)->RaiseFocusDocumentWindow(focus);
	} else {
		QApplication::beep();
	}
}

static void focusPaneAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	Q_UNUSED(event);

	Document *window = Document::WidgetToWindow(w);
	Widget newFocusPane = nullptr;
	int paneIndex;

	if (*nArgs > 0) {
		if (strcmp(args[0], "first") == 0) {
			paneIndex = 0;
		} else if (strcmp(args[0], "last") == 0) {
			paneIndex = window->textPanes_.size();
		} else if (strcmp(args[0], "next") == 0) {
			paneIndex = window->WidgetToPaneIndex(window->lastFocus_) + 1;
			if (paneIndex > window->textPanes_.size()) {
				paneIndex = 0;
			}
		} else if (strcmp(args[0], "previous") == 0) {
			paneIndex = window->WidgetToPaneIndex(window->lastFocus_) - 1;
			if (paneIndex < 0) {
				paneIndex = window->textPanes_.size();
			}
		} else {
			if (sscanf(args[0], "%d", &paneIndex) == 1) {
				if (paneIndex > 0) {
					paneIndex = paneIndex - 1;
				} else if (paneIndex < 0) {
					paneIndex = window->textPanes_.size() + (paneIndex + 1);
				} else {
					paneIndex = -1;
				}
			}
		}
		if (paneIndex >= 0 && paneIndex <= window->textPanes_.size()) {
			newFocusPane = window->GetPaneByIndex(paneIndex);
		}
		if (newFocusPane) {
			window->lastFocus_ = newFocusPane;
			XmProcessTraversal(window->lastFocus_, XmTRAVERSE_CURRENT);
		} else {
			QApplication::beep();
		}
	} else {
		fprintf(stderr, "nedit: focus_pane requires argument\n");
	}
}

/*
** Same as AddSubMenu from libNUtil.a but 1) mnemonic is optional (NEdit
** users like to be able to re-arrange the mnemonics so they can set Alt
** key combinations as accelerators), 2) supports the short/full option
** of SGI_CUSTOM mode, 3) optionally returns the cascade button widget
** in "cascadeBtn" if "cascadeBtn" is non-nullptr.
*/
static Widget createMenu(Widget parent, const char *name, const char *label, char mnemonic, Widget *cascadeBtn, int mode) {

	(void)mode;

	XmString st1;

	Widget menu = CreatePulldownMenu(parent, name, nullptr, 0);
	Widget cascade = XtVaCreateWidget(name, xmCascadeButtonWidgetClass, parent, XmNlabelString, st1 = XmStringCreateSimpleEx(label), XmNsubMenuId, menu, nullptr);
	XmStringFree(st1);
	
	if (mnemonic != 0) {
		XtVaSetValues(cascade, XmNmnemonic, mnemonic, nullptr);
	}
	
	XtManageChild(cascade);

	if(cascadeBtn) {
		*cascadeBtn = cascade;
	}
	
	return menu;
}

/*
** Same as AddMenuItem from libNUtil.a without setting the accelerator
** (these are set in the fallback app-defaults so users can change them),
** and with the short/full option required in SGI_CUSTOM mode.
*/
static Widget createMenuItem(Widget parent, const char *name, const char *label, char mnemonic, menuCallbackProc callback, const void *cbArg, int mode) {

	(void)mode;

	Widget button;
	XmString st1;

	button = XtVaCreateWidget(name, xmPushButtonWidgetClass, parent, XmNlabelString, st1 = XmStringCreateSimpleEx(label), XmNmnemonic, mnemonic, nullptr);
	XtAddCallback(button, XmNactivateCallback, callback, const_cast<void *>(cbArg));
	XmStringFree(st1);

	XtManageChild(button);
	return button;
}

/*
** Add a toggle button item to an already established pull-down or pop-up
** menu, including mnemonics, accelerators and callbacks.
*/
static Widget createMenuToggle(Widget parent, const char *name, const char *label, char mnemonic, menuCallbackProc callback, const void *cbArg, int set, int mode) {

	(void)mode;

	Widget button;
	XmString st1;

	button = XtVaCreateWidget(name, xmToggleButtonWidgetClass, parent, XmNlabelString, st1 = XmStringCreateSimpleEx(label), XmNmnemonic, mnemonic, XmNset, set, nullptr);
	XtAddCallback(button, XmNvalueChangedCallback, callback, const_cast<void *>(cbArg));
	XmStringFree(st1);

	XtManageChild(button);
	return button;
}

/*
** Create a toggle button with a diamond (radio-style) appearance
*/
static Widget createMenuRadioToggle(Widget parent, const char *name, const char *label, char mnemonic, menuCallbackProc callback, const void *cbArg, int set, int mode) {

	Widget button = createMenuToggle(parent, name, label, mnemonic, callback, const_cast<void *>(cbArg), set, mode);
	XtVaSetValues(button, XmNindicatorType, XmONE_OF_MANY, nullptr);
	return button;
}

static Widget createMenuSeparator(Widget parent, const char *name, int mode) {

	(void)mode;

	Widget button;

	button = XmCreateSeparator(parent, (String)name, nullptr, 0);
	XtManageChild(button);
	return button;
}

/*
** Make sure the close menu item is dimmed appropriately for the current
** set of windows.  It should be dim only for the last Untitled, unmodified,
** editor window, and sensitive otherwise.
*/
void CheckCloseDim(void) {

	if(WindowList.empty()) {
		return;
	}
	
	// NOTE(eteran): list has a size of 1	
	if (WindowList.size() == 1 && !WindowList.front()->filenameSet_ && !WindowList.front()->fileChanged_) {
		Document *doc = WindowList.front();
		XtSetSensitive(doc->closeItem_, false);
		return;
	}

	for(Document *window: WindowList) {
		if (window->IsTopDocument()) {
			XtSetSensitive(window->closeItem_, true);
		}
	}
}

/*
** Invalidate the Window menus of all NEdit windows to but don't change
** the menus until they're needed (Originally, this was "UpdateWindowMenus",
** but creating and destroying manu items for every window every time a
** new window was created or something changed, made things move very
** slowly with more than 10 or so windows).
*/
void InvalidateWindowMenus(void) {

	/* Mark the window menus invalid (to be updated when the user pulls one
	   down), unless the menu is torn off, meaning it is visible to the user
	   and should be updated immediately */
	for(Document *w: WindowList) {
		if (!XmIsMenuShell(XtParent(w->windowMenuPane_))) {
			updateWindowMenu(w);
		} else {
			w->windowMenuValid_ = false;
		}
	}
}

/*
** Mark the Previously Opened Files menus of all NEdit windows as invalid.
** Since actually changing the menus is slow, they're just marked and updated
** when the user pulls one down.
*/
static void invalidatePrevOpenMenus(void) {

	/* Mark the menus invalid (to be updated when the user pulls one
	   down), unless the menu is torn off, meaning it is visible to the user
	   and should be updated immediately */
#if 0 // TODO(eteran): transitioned
	for(Document *w: WindowList) {
		if (!XmIsMenuShell(XtParent(w->prevOpenMenuPane_))) {
			updatePrevOpenMenu(w);
		}
    }
#endif
}

/*
** Add a file to the list of previously opened files for display in the
** File menu.
*/
void AddToPrevOpenMenu(const char *filename) {
	int i;
	char *nameCopy;

	// If the Open Previous command is disabled, just return 
	if (GetPrefMaxPrevOpenFiles() < 1) {
		return;
	}

	/*  Refresh list of previously opened files to avoid Big Race Condition,
	    where two sessions overwrite each other's changes in NEdit's
	    history file.
	    Of course there is still Little Race Condition, which occurs if a
	    Session A reads the list, then Session B reads the list and writes
	    it before Session A gets a chance to write.  */
	ReadNEditDB();

	// If the name is already in the list, move it to the start 
	for (i = 0; i < NPrevOpen; i++) {
		if (!strcmp(filename, PrevOpen[i])) {
			nameCopy = PrevOpen[i];
			memmove(&PrevOpen[1], &PrevOpen[0], sizeof(char *) * i);
			PrevOpen[0] = nameCopy;
			invalidatePrevOpenMenus();
			WriteNEditDB();
			return;
		}
	}

	// If the list is already full, make room 
	if (NPrevOpen >= GetPrefMaxPrevOpenFiles()) {
		//  This is only safe if GetPrefMaxPrevOpenFiles() > 0.  
		XtFree(PrevOpen[--NPrevOpen]);
	}

	// Add it to the list 
	nameCopy = XtStringDup(filename);
	memmove(&PrevOpen[1], &PrevOpen[0], sizeof(char *) * NPrevOpen);
	PrevOpen[0] = nameCopy;
	NPrevOpen++;

	// Mark the Previously Opened Files menu as invalid in all windows 
	invalidatePrevOpenMenus();

	// Undim the menu in all windows if it was previously empty 
	if (NPrevOpen > 0) {
		for(Document *w: WindowList) {
			if (w->IsTopDocument()) {
				XtSetSensitive(w->prevOpenMenuItem_, true);
			}
		}
	}

	// Write the menu contents to disk to restore in later sessions 
	WriteNEditDB();
}

static QString getWindowsMenuEntry(const Document *window) {

	QString fullTitle = QString(QLatin1String("%1%2")).arg(window->filename_).arg(window->fileChanged_ ? QLatin1String("*") : QLatin1String(""));

	if (GetPrefShowPathInWindowsMenu() && window->filenameSet_) {
		fullTitle.append(QLatin1String(" - "));
		fullTitle.append(window->path_);
	}

	return fullTitle;
}

/*
** Update the Window menu of a single window to reflect the current state of
** all NEdit windows as determined by the global WindowList.
*/
static void updateWindowMenu(const Document *window) {

	WidgetList items;
	Cardinal nItems;
	int n;

	if (!window->IsTopDocument())
		return;

	// Make a sorted list of windows 
	std::vector<Document *> windows;
	for(Document *w: WindowList) {
		windows.push_back(w);
	}
	
	std::sort(windows.begin(), windows.end(), [](const Document *a, const Document *b) {

		// Untitled first 
		int rc = a->filenameSet_ == b->filenameSet_ ? 0 : a->filenameSet_ && !b->filenameSet_ ? 1 : -1;
		if (rc != 0) {
			return rc < 0;
		}
		
		if(a->filename_ < b->filename_) {
			return true;
		}
		
		
		return a->path_ < b->path_;
	});

	/* if the menu is torn off, unmanage the menu pane
	   before updating it to prevent the tear-off menu
	   from shrinking/expanding as the menu entries
	   are added */
	if (!XmIsMenuShell(XtParent(window->windowMenuPane_)))
		XtUnmanageChild(window->windowMenuPane_);

	/* While it is not possible on some systems (ibm at least) to substitute
	   a new menu pane, it is possible to substitute menu items, as long as
	   at least one remains in the menu at all times. This routine assumes
	   that the menu contains permanent items marked with the value
	   PERMANENT_MENU_ITEM in the userData resource, and adds and removes items
	   which it marks with the value TEMPORARY_MENU_ITEM */

	/* Go thru all of the items in the menu and rename them to
	   match the window list.  Delete any extras */
	XtVaGetValues(window->windowMenuPane_, XmNchildren, &items, XmNnumChildren, &nItems, nullptr);
	int windowIndex = 0;
	int nWindows = Document::WindowCount();
	for (n = 0; n < (int)nItems; n++) {
		XtPointer userData;
		XtVaGetValues(items[n], XmNuserData, &userData, nullptr);
		if (userData == TEMPORARY_MENU_ITEM) {
			if (windowIndex >= nWindows) {
				// unmanaging before destroying stops parent from displaying 
				XtUnmanageChild(items[n]);
				XtDestroyWidget(items[n]);
			} else {
				XmString st1;
				QString title = getWindowsMenuEntry(windows[windowIndex]);
				XtVaSetValues(items[n], XmNlabelString, st1 = XmStringCreateSimpleEx(title), nullptr);
				XtRemoveAllCallbacks(items[n], XmNactivateCallback);
				XtAddCallback(items[n], XmNactivateCallback, raiseCB, windows[windowIndex]);
				XmStringFree(st1);
				windowIndex++;
			}
		}
	}

	// Add new items for the titles of the remaining windows to the menu 
	for (; windowIndex < nWindows; windowIndex++) {
		XmString st1;
		QString title = getWindowsMenuEntry(windows[windowIndex]);
		Widget btn = XtVaCreateManagedWidget("win", xmPushButtonWidgetClass, window->windowMenuPane_, XmNlabelString, st1 = XmStringCreateSimpleEx(title), XmNmarginHeight, 0, XmNuserData, TEMPORARY_MENU_ITEM, nullptr);
		XtAddCallback(btn, XmNactivateCallback, raiseCB, windows[windowIndex]);
		XmStringFree(st1);
	}
	
	/* if the menu is torn off, we need to manually adjust the
	   dimension of the menuShell _before_ re-managing the menu
	   pane, to either expose the hidden menu entries or remove
	   the empty space */
	if (!XmIsMenuShell(XtParent(window->windowMenuPane_))) {
		Dimension width, height;

		XtVaGetValues(window->windowMenuPane_, XmNwidth, &width, XmNheight, &height, nullptr);
		XtVaSetValues(XtParent(window->windowMenuPane_), XmNwidth, width, XmNheight, height, nullptr);
		XtManageChild(window->windowMenuPane_);
	}
}

static const char neditDBBadFilenameChars[] = "\n";

/*
** Write dynamic database of file names for "Open Previous".  Eventually,
** this may hold window positions, and possibly file marks, in which case,
** it should be moved to a different module, but for now it's just a list
** of previously opened files.
*/
void WriteNEditDB(void) {

	QString fullName = GetRCFileNameEx(NEDIT_HISTORY);
	if(fullName.isNull()) {
		return;
	}

	FILE *fp;
	int i;
	static char fileHeader[] = "# File name database for NEdit Open Previous command\n";

	// If the Open Previous command is disabled, just return 
	if (GetPrefMaxPrevOpenFiles() < 1) {
		return;
	}

	// open the file 
	if ((fp = fopen(fullName.toLatin1().data(), "w")) == nullptr) {
		return;
	}

	// write the file header text to the file 
	fprintf(fp, "%s", fileHeader);

	// Write the list of file names 
	for (i = 0; i < NPrevOpen; ++i) {
		size_t lineLen = strlen(PrevOpen[i]);

		if (lineLen > 0 && PrevOpen[i][0] != '#' && strcspn(PrevOpen[i], neditDBBadFilenameChars) == lineLen) {
			fprintf(fp, "%s\n", PrevOpen[i]);
		}
	}

	fclose(fp);

}

/*
**  Read database of file names for 'Open Previous' submenu.
**
**  Eventually, this may hold window positions, and possibly file marks (in
**  which case it should be moved to a different module) but for now it's
**  just a list of previously opened files.
**
**  This list is read once at startup and potentially refreshed before a
**  new entry is about to be written to the file or before the menu is
**  displayed. If the file is modified since the last read (or not read
**  before), it is read in, otherwise nothing is done.
*/
void ReadNEditDB(void) {
	
	char line[MAXPATHLEN + 2];
	char *nameCopy;
	struct stat attribute;
	FILE *fp;
	size_t lineLen;
	static time_t lastNeditdbModTime = 0;

	/*  If the Open Previous command is disabled or the user set the
	    resource to an (invalid) negative value, just return.  */
	if (GetPrefMaxPrevOpenFiles() < 1) {
		return;
	}

	/* Initialize the files list and allocate a (permanent) block memory
	   of the size prescribed by the maxPrevOpenFiles resource */
	if (!PrevOpen) {
		PrevOpen = new char*[GetPrefMaxPrevOpenFiles()];
		NPrevOpen = 0;
	}

	/* Don't move this check ahead of the previous statements. PrevOpen
	   must be initialized at all times. */
	QString fullName = GetRCFileNameEx(NEDIT_HISTORY);
	if(fullName.isNull()) {
		return;
	}

	/*  Stat history file to see whether someone touched it after this
	    session last changed it.  */
	if (stat(fullName.toLatin1().data(), &attribute) == 0) {
		if (lastNeditdbModTime >= attribute.st_mtime) {
			//  Do nothing, history file is unchanged.  
			return;
		} else {
			//  Memorize modtime to compare to next time.  
			lastNeditdbModTime = attribute.st_mtime;
		}
	} else {
		//  stat() failed, probably for non-exiting history database.  
		if (ENOENT != errno) {
			perror("nedit: Error reading history database");
		}
		return;
	}

	// open the file 
	if ((fp = fopen(fullName.toLatin1().data(), "r")) == nullptr) {
		return;
	}

	//  Clear previous list.  
	while (NPrevOpen != 0) {
		XtFree(PrevOpen[--NPrevOpen]);
	}

	/* read lines of the file, lines beginning with # are considered to be
	   comments and are thrown away.  Lines are subject to cursory checking,
	   then just copied to the Open Previous file menu list */
	while (true) {
		if (fgets(line, sizeof(line), fp) == nullptr) {
			// end of file 
			fclose(fp);
			return;
		}
		if (line[0] == '#') {
			// comment 
			continue;
		}
		lineLen = strlen(line);
		if (lineLen == 0) {
			// blank line 
			continue;
		}
		if (line[lineLen - 1] != '\n') {
			// no newline, probably truncated 
			fprintf(stderr, "nedit: Line too long in history file\n");
			while (fgets(line, sizeof(line), fp)) {
				lineLen = strlen(line);
				if (lineLen > 0 && line[lineLen - 1] == '\n') {
					break;
				}
			}
			continue;
		}
		line[--lineLen] = '\0';
		if (strcspn(line, neditDBBadFilenameChars) != lineLen) {
			// non-filename characters 
			fprintf(stderr, "nedit: History file may be corrupted\n");
			continue;
		}
		nameCopy = XtMalloc(lineLen + 1);
		strcpy(nameCopy, line);
		PrevOpen[NPrevOpen++] = nameCopy;
		if (NPrevOpen >= GetPrefMaxPrevOpenFiles()) {
			// too many entries 
			fclose(fp);
			return;
		}
	}

	// NOTE(eteran): fixes resource leak
	fclose(fp);
}

static void setWindowSizeDefault(int rows, int cols) {
	SetPrefRows(rows);
	SetPrefCols(cols);
	updateWindowSizeMenus();
}

static void updateWindowSizeMenus(void) {
	for(Document *win: WindowList) {
		updateWindowSizeMenu(win);
	}
}

static void updateWindowSizeMenu(Document *win) {
	int rows = GetPrefRows(), cols = GetPrefCols();
	char title[50];
	XmString st1;

	if (!win->IsTopDocument())
		return;

	XmToggleButtonSetState(win->size24x80DefItem_, rows == 24 && cols == 80, False);
	XmToggleButtonSetState(win->size40x80DefItem_, rows == 40 && cols == 80, False);
	XmToggleButtonSetState(win->size60x80DefItem_, rows == 60 && cols == 80, False);
	XmToggleButtonSetState(win->size80x80DefItem_, rows == 80 && cols == 80, False);
	if ((rows != 24 && rows != 40 && rows != 60 && rows != 80) || cols != 80) {
		XmToggleButtonSetState(win->sizeCustomDefItem_, True, False);
		snprintf(title, sizeof(title), "Custom... (%d x %d)", rows, cols);
		XtVaSetValues(win->sizeCustomDefItem_, XmNlabelString, st1 = XmStringCreateSimpleEx(title), nullptr);
		XmStringFree(st1);
	} else {
		XmToggleButtonSetState(win->sizeCustomDefItem_, False, False);
		XtVaSetValues(win->sizeCustomDefItem_, XmNlabelString, st1 = XmStringCreateSimpleEx("Custom..."), nullptr);
		XmStringFree(st1);
	}
}

static void raiseCB(Widget w, XtPointer clientData, XtPointer callData) {
	HidePointerOnKeyedEvent(Document::WidgetToWindow(MENU_WIDGET(w))->lastFocus_, static_cast<XmAnyCallbackStruct *>(callData)->event);
	static_cast<Document *>(clientData)->RaiseFocusDocumentWindow(True /* always focus */);
}

/*
** strCaseCmp compares its arguments and returns 0 if the two strings
** are equal IGNORING case differences.  Otherwise returns 1.
*/
static int strCaseCmp(const char *str1, const char *str2) {
	const char *c1, *c2;

	for (c1 = str1, c2 = str2; *c1 != '\0' && *c2 != '\0'; c1++, c2++)
		if (toupper((uint8_t)*c1) != toupper((uint8_t)*c2))
			return 1;
	if (*c1 == *c2) {
		return (0);
	} else {
		return (1);
	}
}

/*
** Create popup for right button programmable menu
*/
Widget CreateBGMenu(Document *window) {
	Arg args[1];

	/* There is still some mystery here.  It's important to get the XmNmenuPost
	   resource set to the correct menu button, or the menu will not post
	   properly, but there's also some danger that it will take over the entire
	   button and interfere with text widget translations which use the button
	   with modifiers.  I don't entirely understand why it works properly now
	   when it failed often in development, and certainly ignores the ~ syntax
	   in translation event specifications. */
	XtSetArg(args[0], XmNmenuPost, GetPrefBGMenuBtn());
	return CreatePopupMenu(window->textArea_, "bgMenu", args, 1);
}

/*
** Create context popup menu for tabs & tab bar
*/
Widget CreateTabContextMenu(Widget parent, Document *window) {
	Widget menu;
	Arg args[8];
	int n;

	n = 0;
	XtSetArg(args[n], XmNtearOffModel, XmTEAR_OFF_DISABLED);
	n++;
	menu = CreatePopupMenu(parent, "tabContext", args, n);

	createMenuItem(menu, "new", "New Tab", 0, doTabActionCB, "new_tab", SHORT);
	createMenuItem(menu, "close", "Close Tab", 0, doTabActionCB, "close", SHORT);
	createMenuSeparator(menu, "sep1", SHORT);
	window->contextDetachDocumentItem_ = createMenuItem(menu, "detach", "Detach Tab", 0, doTabActionCB, "detach_document", SHORT);
	XtSetSensitive(window->contextDetachDocumentItem_, False);
	window->contextMoveDocumentItem_ = createMenuItem(menu, "attach", "Move Tab To...", 0, doTabActionCB, "move_document_dialog", SHORT);

	return menu;
}

/*
** Add a translation to the text widget to trigger the background menu using
** the mouse-button + modifier combination specified in the resource:
** nedit.bgMenuBtn.
*/
void AddBGMenuAction(Widget widget) {
	static XtTranslations table = nullptr;

	if(!table) {
		char translations[MAX_ACCEL_LEN + 25];
		sprintf(translations, "%s: post_window_bg_menu()\n", GetPrefBGMenuBtn());
		table = XtParseTranslationTable(translations);
	}
	XtOverrideTranslations(widget, table);
}

static void bgMenuPostAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	Q_UNUSED(nArgs)
	Q_UNUSED(args);

#if 1
	Document *window = Document::WidgetToWindow(w);

	/* The Motif popup handling code BLOCKS events while the menu is posted,
	   including the matching btn-up events which complete various dragging
	   operations which it may interrupt.  Cancel to head off problems */
	XtCallActionProc(window->lastFocus_, "process_cancel", event, nullptr, 0);

	// Pop up the menu 
	XmMenuPosition(window->bgMenuPane_, (XButtonPressedEvent *)event);
	XtManageChild(window->bgMenuPane_);
#endif
	/*
	   These statements have been here for a very long time, but seem
	   unnecessary and are even dangerous: when any of the lock keys are on,
	   Motif thinks it shouldn't display the background menu, but this
	   callback is called anyway. When we then grab the focus and force the
	   menu to be drawn, bad things can happen (like a total lockup of the X
	   server).

	   XtPopup(XtParent(window->bgMenuPane_), XtGrabNonexclusive);
	   XtMapWidget(XtParent(window->bgMenuPane_));
	   XtMapWidget(window->bgMenuPane_);
	*/
#if 0
	QMenu menu;
	menu.addAction(QLatin1String("Test 1"));
	menu.addAction(QLatin1String("Test 2"));
	menu.addAction(QLatin1String("Test 3"));
	menu.addAction(QLatin1String("Test 4"));
	menu.exec(QCursor::pos());
#endif
}

void AddTabContextMenuAction(Widget widget) {
	static XtTranslations table = nullptr;

	if(!table) {
		const char *translations = "<Btn3Down>: post_tab_context_menu()\n";
		table = XtParseTranslationTable((String)translations);
	}
	XtOverrideTranslations(widget, table);
}

/*
** action procedure for posting context menu of tabs
*/
static void tabMenuPostAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	Q_UNUSED(nArgs)
	Q_UNUSED(args);

	Document *window;
	auto xbutton = reinterpret_cast<XButtonPressedEvent *>(event);
	Widget wgt;

	/* Determine if the context menu was called from tabs or gutter,
	   then stored the corresponding window info as userData of
	   the popup menu pane, which will later be extracted by
	   doTabActionCB() to act upon. When the context menu was called
	   from the gutter, the active doc is assumed.

	   Lesstif requires the action [to pupop the menu] to also be
	   to the tabs, else nothing happed when right-click on tabs.
	   Even so, the action procedure sometime appear to be called
	   from the gutter even if users did right-click on the tabs.
	   Here we try to cater for the uncertainty. */
	if (XtClass(w) == xrwsBubbleButtonWidgetClass)
		window = Document::TabToWindow(w);
	else if (xbutton->subwindow) {
		wgt = XtWindowToWidget(XtDisplay(w), xbutton->subwindow);
		window = Document::TabToWindow(wgt);
	} else {
		window = Document::WidgetToWindow(w);
	}
	XtVaSetValues(window->tabMenuPane_, XmNuserData, (XtPointer)window, nullptr);

	/* The Motif popup handling code BLOCKS events while the menu is posted,
	   including the matching btn-up events which complete various dragging
	   operations which it may interrupt.  Cancel to head off problems */
	XtCallActionProc(window->lastFocus_, "process_cancel", event, nullptr, 0);

	// Pop up the menu 
	XmMenuPosition(window->tabMenuPane_, (XButtonPressedEvent *)event);
	XtManageChild(window->tabMenuPane_);
}

/*
** Event handler for restoring the input hint of menu tearoffs
** previously disabled in ShowHiddenTearOff()
*/
static void tearoffMappedCB(Widget w, XtPointer clientData, XUnmapEvent *event) {

	Q_UNUSED(w);

	Widget shell = static_cast<Widget>(clientData);
	XWMHints *wmHints;

	if (event->type != MapNotify)
		return;

	// restore the input hint previously disabled in ShowHiddenTearOff() 
	wmHints = XGetWMHints(TheDisplay, XtWindow(shell));
	wmHints->input = True;
	wmHints->flags |= InputHint;
	XSetWMHints(TheDisplay, XtWindow(shell), wmHints);
	XFree(wmHints);

	// we only need to do this only 
	XtRemoveEventHandler(shell, StructureNotifyMask, False, (XtEventHandler)tearoffMappedCB, shell);
}

/*
** Redisplay (map) a hidden tearoff
*/
void ShowHiddenTearOff(Widget menuPane) {
	Widget shell;

	if (!menuPane)
		return;

	shell = XtParent(menuPane);
	if (!XmIsMenuShell(shell)) {
		XWindowAttributes winAttr;

		XGetWindowAttributes(XtDisplay(shell), XtWindow(shell), &winAttr);
		if (winAttr.map_state == IsUnmapped) {
			XWMHints *wmHints;

			/* to workaround a problem where the remapped tearoffs
			   always receive the input focus insteads of the text
			   editing window, we disable the input hint of the
			   tearoff shell temporarily. */
			wmHints = XGetWMHints(XtDisplay(shell), XtWindow(shell));
			wmHints->input = False;
			wmHints->flags |= InputHint;
			XSetWMHints(XtDisplay(shell), XtWindow(shell), wmHints);
			XFree(wmHints);

			// show the tearoff 
			XtMapWidget(shell);

			/* the input hint will be restored when the tearoff
		   is mapped */
			XtAddEventHandler(shell, StructureNotifyMask, False, (XtEventHandler)tearoffMappedCB, shell);
		}
	}
}
