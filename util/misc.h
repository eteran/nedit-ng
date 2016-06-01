/*******************************************************************************
*                                                                              *
* misc.h -- Nirvana Editor Miscellaneous Header File                           *
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

#ifndef MISC_H_
#define MISC_H_

#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/CutPaste.h>
#include <string>

/* maximum length for a window geometry string */
#define MAX_GEOM_STRING_LEN 24

/* Maximum length for a menu accelerator string.
   Which e.g. can be parsed by misc.c:parseAccelString()
   (how many modifier keys can you hold down at once?) */
#define MAX_ACCEL_LEN 100

/*  button margin width to avoid cramped buttons  */
#define BUTTON_WIDTH_MARGIN 12

bool FindBestVisual(Display *display, const char *appName, const char *appClass, Visual **visual, int *depth, Colormap *colormap);
std::string GetXmStringTextEx(XmString fromString);
int SpinClipboardCopy(Display *display, Window window, long item_id, char *format_name, XtPointer buffer, unsigned long length, long private_id, long *data_id);
int SpinClipboardEndCopy(Display *display, Window window, long item_id);
int SpinClipboardInquireLength(Display *display, Window window, char *format_name, unsigned long *length);
int SpinClipboardLock(Display *display, Window window);
int SpinClipboardRetrieve(Display *display, Window window, char *format_name, XtPointer buffer, unsigned long length, unsigned long *num_bytes, long *private_id);
int SpinClipboardStartCopy(Display *display, Window window, XmString clip_label, Time timestamp, Widget widget, XmCutPasteProc callback, long *item_id);
int SpinClipboardUnlock(Display *display, Window window);
long QueryCurrentDesktop(Display *display, Window rootWindow);
long QueryDesktop(Display *display, Widget shell);
Modifiers GetNumLockModMask(Display *display);
void AccelLockBugPatch(Widget topWidget, Widget topMenuContainer);
void AddMotifCloseCallback(Widget shell, XtCallbackProc closeCB, void *arg);
void AddMouseWheelSupport(Widget w);
void BeginWait(Widget topCursorWidget);
void BusyWait(Widget anyWidget);
void CloseAllPopupsFor(Widget shell);
void CreateGeometryString(char *string, int x, int y, int width, int height, int bitmask);
void EndWait(Widget topCursorWidget);
void InstallMouseWheelActions(XtAppContext context);
void MakeSingleLineTextW(Widget textW);
void ManageDialogCenteredOnPointer(Widget dialogChild);
void PopDownBugPatch(Widget w);
void RadioButtonChangeState(Widget widget, bool state, bool notify);
void RaiseShellWindow(Widget shell, bool focus);
void RaiseWindow(Display *display, Window w, bool focus);
void RealizeWithoutForcingPosition(Widget shell);
void RemapDeleteKey(Widget w);
void RemovePPositionHint(Widget shell);
void SetDeleteRemap(int state);
void SetPointerCenteredDialogs(int state);
void SuppressPassiveGrabWarnings(void);
void UpdateAccelLockPatch(Widget topWidget, Widget newButton);
void WmClientMsg(Display *disp, Window win, const char *msg, unsigned long data0, unsigned long data1, unsigned long data2, unsigned long data3, unsigned long data4);
Widget AddMenuItem(Widget parent, char *name, char *label, char mnemonic, char *acc, char *accText, XtCallbackProc callback, void *cbArg);
Widget AddMenuToggle(Widget parent, char *name, char *label, char mnemonic, char *acc, char *accText, XtCallbackProc callback, void *cbArg, int set);
Widget AddSubMenu(Widget parent, char *name, char *label, char mnemonic);
Widget CreatePopupMenu(Widget parent, const char *name, ArgList arglist, Cardinal argcount);
Widget CreatePopupShellWithBestVis(String shellName, WidgetClass clazz, Widget parent, ArgList arglist, Cardinal argcount);
Widget CreatePulldownMenu(Widget parent, const char *name, ArgList arglist, Cardinal argcount);
Widget CreateSelectionDialog(Widget parent, const char *name, ArgList arglist, Cardinal argcount);
Widget CreateShellWithBestVis(String appName, String appClass, WidgetClass clazz, Display *display, ArgList args, Cardinal nArgs);
Widget CreateWidget(Widget parent, const char *name, WidgetClass clazz, ArgList arglist, Cardinal argcount);
XFontStruct *GetDefaultFontStruct(XmFontList font);
	
#endif
