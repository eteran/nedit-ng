/*******************************************************************************
*                                                                              *
* window.h -- Nirvana Editor Window header file                                *
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

#ifndef WINDOW_H_
#define WINDOW_H_

#include "nedit.h"
#include "textBuf.h"

#include <X11/Intrinsic.h>

Boolean IsTopDocument(const WindowInfo *window);
Widget GetPaneByIndex(WindowInfo *window, int paneIndex);
WindowInfo *CreateDocument(WindowInfo *shellWindow, const char *name);
WindowInfo *CreateWindow(const char *title, char *geometry, int iconic);
WindowInfo *DetachDocument(WindowInfo *window);
WindowInfo *FindWindowWithFile(const char *name, const char *path);
WindowInfo *GetTopDocument(Widget w);
WindowInfo *MarkActiveDocument(WindowInfo *window);
WindowInfo *MarkLastDocument(WindowInfo *window);
WindowInfo *MoveDocument(WindowInfo *toWindow, WindowInfo *window);
WindowInfo *TabToWindow(Widget tab);
WindowInfo *WidgetToWindow(Widget w);
int CloseAllDocumentInWindow(WindowInfo *window);
int GetShowTabBar(WindowInfo *window);
int GetSimpleSelection(textBuffer *buf, int *left, int *right);
int IsIconic(WindowInfo *window);
int IsValidWindow(WindowInfo *window);
int NDocuments(WindowInfo *window);
int NWindows(void);
int WidgetToPaneIndex(WindowInfo *window, Widget w);
void AddSmallIcon(Widget shell);
void AllWindowsBusy(const char *message);
void AllWindowsUnbusy(void);
void AttachSessionMgrHandler(Widget appShell);
void CleanUpTabBarExposeQueue(WindowInfo *window);
void ClearModeMessage(WindowInfo *window);
void ClosePane(WindowInfo *window);
void CloseWindow(WindowInfo *window);
void LastDocument(WindowInfo *window);
void MakeSelectionVisible(WindowInfo *window, Widget textPane);
void MoveDocumentDialog(WindowInfo *window);
void NextDocument(WindowInfo *window);
void PreviousDocument(WindowInfo *window);
void RaiseDocument(WindowInfo *window);
void RaiseDocumentWindow(WindowInfo *window);
void RaiseFocusDocumentWindow(WindowInfo *window, Boolean focus);
void RefreshMenuToggleStates(WindowInfo *window);
void RefreshTabState(WindowInfo *window);
void RefreshWindowStates(WindowInfo *window);
void SetAutoIndent(WindowInfo *window, int state);
void SetAutoScroll(WindowInfo *window, int margin);
void SetAutoWrap(WindowInfo *window, int state);
void SetBacklightChars(WindowInfo *window, char *applyBacklightTypes);
void SetColors(WindowInfo *window, const char *textFg, const char *textBg, const char *selectFg, const char *selectBg, const char *hiliteFg, const char *hiliteBg, const char *lineNoFg, const char *cursorFg);
void SetEmTabDist(WindowInfo *window, int emTabDist);
void SetFonts(WindowInfo *window, const char *fontName, const char *italicName, const char *boldName, const char *boldItalicName);
void SetModeMessage(WindowInfo *window, const char *message);
void SetOverstrike(WindowInfo *window, int overstrike);
void SetSensitive(WindowInfo *window, Widget w, Boolean sensitive);
void SetShowMatching(WindowInfo *window, int state);
void SetTabDist(WindowInfo *window, int tabDist);
void SetToggleButtonState(WindowInfo *window, Widget w, Boolean state, Boolean notify);
void SetWindowModified(WindowInfo *window, int modified);
void ShowISearchLine(WindowInfo *window, int state);
void ShowLineNumbers(WindowInfo *window, int state);
void ShowStatsLine(WindowInfo *window, int state);
void ShowTabBar(WindowInfo *window, int state);
void ShowWindowTabBar(WindowInfo *window);
void SortTabBar(WindowInfo *window);
void SplitPane(WindowInfo *window);
void TempShowISearch(WindowInfo *window, int state);
void UpdateMinPaneHeights(WindowInfo *window);
void UpdateNewOppositeMenu(WindowInfo *window, int openInTab);
void UpdateStatsLine(WindowInfo *window);
void UpdateWMSizeHints(WindowInfo *window);
void UpdateWindowReadOnly(WindowInfo *window);
void UpdateWindowTitle(const WindowInfo *window);

#endif
