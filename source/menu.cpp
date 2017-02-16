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

#ifdef REPLACE_SCOPE
static void replaceScopeWindowCB(Widget w, XtPointer clientData, XtPointer callData);
static void replaceScopeSelectionCB(Widget w, XtPointer clientData, XtPointer callData);
static void replaceScopeSmartCB(Widget w, XtPointer clientData, XtPointer callData);
#endif


#if 0
// Menu modes for SGI_CUSTOM short-menus feature 
enum menuModes { FULL, SHORT };

// Application action table 
static XtActionsRec Actions[] = {
                                 //{(String) "new", newAP},
                                 //{(String) "new_opposite", newOppositeAP},
                                 //{(String) "new_tab", newTabAP},
                                 //{(String) "open", openAP},
                                 //{(String) "open-dialog", openDialogAP},
                                 //{(String) "open_dialog", openDialogAP},
                                 //{(String) "open-selected", openSelectedAP},
                                 //{(String) "open_selected", openSelectedAP},
                                 //{(String) "close", closeAP},
                                 //{(String) "save", saveAP},
                                 //{(String) "save-as", saveAsAP},
                                 //{(String) "save_as", saveAsAP},
                                 //{(String) "save-as-dialog", saveAsDialogAP},
                                 //{(String) "save_as_dialog", saveAsDialogAP},
                                 //{(String) "revert-to-saved", revertAP},
                                 //{(String) "revert_to_saved", revertAP},
                                 //{(String) "revert_to_saved_dialog", revertDialogAP},
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
                                 //{(String) "exit", exitAP},
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
                                 //{(String) "next_document", nextDocumentAP},
                                 //{(String) "previous_document", prevDocumentAP},
                                 //{(String) "last_document", lastDocumentAP},
                                 //{(String) "uppercase", capitalizeAP },
                                 //{(String) "lowercase", lowercaseAP },
                                 //{(String) "fill-paragraph", fillAP },
                                 //{(String) "fill_paragraph", fillAP },
                                 //{(String) "control-code-dialog", controlDialogAP},
                                 //{(String) "control_code_dialog", controlDialogAP},

                                 //{(String) "filter-selection-dialog", filterDialogAP},
                                 //{(String) "filter_selection_dialog", filterDialogAP},
                                 //{(String) "filter-selection", shellFilterAP},
                                 //{(String) "filter_selection", shellFilterAP},
                                 //{(String) "execute-command", execAP},
                                 //{(String) "execute_command", execAP},
                                 //{(String) "execute-command-dialog", execDialogAP},
                                 //{(String) "execute_command_dialog", execDialogAP},
                                 //{(String) "execute-command-line", execLineAP},
                                 //{(String) "execute_command_line", execLineAP},
                                 //{(String) "shell-menu-command", shellMenuAP},
                                 //{(String) "shell_menu_command", shellMenuAP},
                                 //{(String) "macro-menu-command", macroMenuAP},
                                 //{(String) "macro_menu_command", macroMenuAP},
                                 //{(String) "bg_menu_command", bgMenuAP},
                                 //{(String) "post_window_bg_menu", bgMenuPostAP},
                                 //{(String) "post_tab_context_menu", tabMenuPostAP},
                                 {(String) "beginning-of-selection", beginningOfSelectionAP},
                                 {(String) "beginning_of_selection", beginningOfSelectionAP},
                                 {(String) "end-of-selection", endOfSelectionAP},
                                 {(String) "end_of_selection", endOfSelectionAP},
                                 //{(String) "repeat_macro", repeatMacroAP},
                                 //{(String) "repeat_dialog", repeatDialogAP},
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


/*
** Create the menu bar
*/

Widget CreateMenuBar(Widget parent, Document *window) {
    Widget menuBar, menuPane, btn, cascade;

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

	// Customize Menus sub menu 

	// Search sub menu 

#ifdef REPLACE_SCOPE
	subSubSubPane = createMenu(subSubPane, "defaultReplaceScope", "Default Replace Scope", 'R', nullptr, FULL);
	XtVaSetValues(subSubSubPane, XmNradioBehavior, True, nullptr);
	window->replScopeWinDefItem_ = createMenuToggle(subSubSubPane, "window", "In Window", 'W', replaceScopeWindowCB, window, GetPrefReplaceDefScope() == REPL_DEF_SCOPE_WINDOW, FULL);
	window->replScopeSelDefItem_ = createMenuToggle(subSubSubPane, "selection", "In Selection", 'S', replaceScopeSelectionCB, window, GetPrefReplaceDefScope() == REPL_DEF_SCOPE_SELECTION, FULL);
	window->replScopeSmartDefItem_ = createMenuToggle(subSubSubPane, "window", "Smart", 'm', replaceScopeSmartCB, window, GetPrefReplaceDefScope() == REPL_DEF_SCOPE_SMART, FULL);
#endif

	/*
	** Remainder of Preferences menu
	*/

	/*
	** Create the Shell menu
	*/
    menuPane = window->shellMenuPane_ = createMenu(menuBar, "shellMenu", "Shell", 0, &cascade, FULL);


	/*
	** Create the Macro menu
	*/
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
	window->splitPaneItem_ = createMenuItem(menuPane, "splitPane", "Split Pane", 'S', doActionCB, "split_pane", SHORT);
	XtVaSetValues(window->splitPaneItem_, XmNuserData, PERMANENT_MENU_ITEM, nullptr);



	btn = createMenuSeparator(menuPane, "sep1", SHORT);
	XtVaSetValues(btn, XmNuserData, PERMANENT_MENU_ITEM, nullptr);

	/*
	** Create "Help" pull down menu.
	*/
	menuPane = createMenu(menuBar, "helpMenu", "Help", 0, &cascade, SHORT);

	return menuBar;
}
#endif


/*----------------------------------------------------------------------------*/



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




#if 0
static void replayCB(Widget w, XtPointer clientData, XtPointer callData) {
	Q_UNUSED(clientData);
	Q_UNUSED(callData);

	HidePointerOnKeyedEvent(Document::WidgetToWindow(MENU_WIDGET(w))->lastFocus_, static_cast<XmAnyCallbackStruct *>(callData)->event);
	Replay(Document::WidgetToWindow(MENU_WIDGET(w)));
}
#endif

#if 0 // NOTE(eteran): transitioned
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

static void raiseCB(Widget w, XtPointer clientData, XtPointer callData) {
	HidePointerOnKeyedEvent(Document::WidgetToWindow(MENU_WIDGET(w))->lastFocus_, static_cast<XmAnyCallbackStruct *>(callData)->event);
	static_cast<Document *>(clientData)->RaiseFocusDocumentWindow(True /* always focus */);
}
#endif

