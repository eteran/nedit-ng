/*******************************************************************************
*                                                                              *
* text.c - Display text from a text buffer                                     *
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
* June 15, 1995                                                                *
*                                                                              *
*******************************************************************************/

#include "text.h"
#include "textP.h"
#include "TextBuffer.h"
#include "TextDisplay.h"
#include "textSel.h"
#include "textDrag.h"
#include "nedit.h"
#include "Document.h"
#include "calltips.h"
#include "../util/DialogF.h"
#include "window.h"
#include "../util/memory.h"

#include <cstdio>
#include <cstdlib>
#include <climits>
#include <cstring>
#include <cctype>
#include <sys/param.h>

#include <climits>
#include <algorithm>

#include <X11/Intrinsic.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/cursorfont.h>
#include <Xm/Xm.h>
#include <Xm/XmP.h>
#if XmVersion >= 1002
#include <Xm/PrimitiveP.h>
#endif

#ifdef UNICOS
#define XtOffset(p_type, field) ((size_t)__INTADDR_(&(((p_type)0)->field)))
#endif

/* Number of pixels of motion from the initial (grab-focus) button press
   required to begin recognizing a mouse drag for the purpose of making a
   selection */
#define SELECT_THRESHOLD 5

/* Length of delay in milliseconds for vertical autoscrolling */
#define VERTICAL_SCROLL_DELAY 50

static void initialize(Widget request, Widget newWidget, ArgList args, Cardinal *num_args);
static void handleHidePointer(Widget w, XtPointer unused, XEvent *event, Boolean *continue_to_dispatch);
static void handleShowPointer(Widget w, XtPointer unused, XEvent *event, Boolean *continue_to_dispatch);
static void redisplay(TextWidget w, XEvent *event, Region region);
static void redisplayGE(TextWidget w, XtPointer client_data, XEvent *event, Boolean *continue_to_dispatch_return);
static void destroy(TextWidget w);
static void resize(TextWidget w);
static Boolean setValues(TextWidget current, TextWidget request, TextWidget new_widget);
static void realize(Widget w, XtValueMask *valueMask, XSetWindowAttributes *attributes);
static XtGeometryResult queryGeometry(Widget w, XtWidgetGeometry *proposed, XtWidgetGeometry *answer);
static void grabFocusAP(Widget w, XEvent *event, String *args, Cardinal *n_args);
static void moveDestinationAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void extendAdjustAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void extendStartAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void extendEndAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void processCancelAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void secondaryStartAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void secondaryOrDragStartAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void secondaryAdjustAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void secondaryOrDragAdjustAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void copyToAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void copyToOrEndDragAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void copyPrimaryAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void cutPrimaryAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void moveToAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void moveToOrEndDragAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void endDragAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void exchangeAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void mousePanAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void pasteClipboardAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void copyClipboardAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void cutClipboardAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void insertStringAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void selfInsertAP(Widget w, XEvent *event, String *args, Cardinal *n_args);
static void newlineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void newlineAndIndentAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void newlineNoIndentAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void processTabAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void endOfLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void beginningOfLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void deleteSelectionAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void deletePreviousCharacterAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void deleteNextCharacterAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void deletePreviousWordAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void deleteNextWordAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void deleteToStartOfLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void deleteToEndOfLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void forwardCharacterAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void backwardCharacterAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void forwardWordAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void backwardWordAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void forwardParagraphAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void backwardParagraphAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void keySelectAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void processUpAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void processShiftUpAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void processDownAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void processShiftDownAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void beginningOfFileAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void endOfFileAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void nextPageAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void previousPageAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void pageLeftAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void pageRightAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void toggleOverstrikeAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void scrollUpAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void scrollDownAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void scrollLeftAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void scrollRightAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void scrollToLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void selectAllAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void deselectAllAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void focusInAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void focusOutAP(Widget w, XEvent *event, String *args, Cardinal *nArgs);
static void checkMoveSelectionChange(Widget w, XEvent *event, int startPos, String *args, Cardinal *nArgs);
static void keyMoveExtendSelection(Widget w, XEvent *event, int startPos, int rectangular);
static void checkAutoShowInsertPos(Widget w);
static int checkReadOnly(Widget w);
static void simpleInsertAtCursorEx(Widget w, view::string_view chars, XEvent *event, int allowPendingDelete);
static int pendingSelection(Widget w);
static int deletePendingSelection(Widget w, XEvent *event);
static int deleteEmulatedTab(Widget w, XEvent *event);
static void selectWord(Widget w, int pointerX);
static int spanForward(TextBuffer *buf, int startPos, const char *searchChars, int ignoreSpace, int *foundPos);
static int spanBackward(TextBuffer *buf, int startPos, const char *searchChars, int ignoreSpace, int *foundPos);
static void selectLine(Widget w);
static int startOfWord(TextWidget w, int pos);
static int endOfWord(TextWidget w, int pos);
static void checkAutoScroll(TextWidget w, int x, int y);
static void endDrag(Widget w);
static void cancelDrag(Widget w);
static void callCursorMovementCBs(Widget w, XEvent *event);
static void adjustSelection(TextWidget tw, int x, int y);
static void adjustSecondarySelection(TextWidget tw, int x, int y);
static void autoScrollTimerProc(XtPointer clientData, XtIntervalId *id);
static std::string wrapTextEx(TextWidget tw, view::string_view startLine, view::string_view text, int bufOffset, int wrapMargin, int *breakBefore);
static int wrapLine(TextWidget tw, TextBuffer *buf, int bufOffset, int lineStartPos, int lineEndPos, int limitPos, int *breakAt, int *charsAdded);
static std::string createIndentStringEx(TextWidget tw, TextBuffer *buf, int bufOffset, int lineStartPos, int lineEndPos, int *length, int *column);
static void cursorBlinkTimerProc(XtPointer clientData, XtIntervalId *id);
static int hasKey(const char *key, const String *args, const Cardinal *nArgs);
static int strCaseCmp(const char *str1, const char *str2);
static void ringIfNecessary(Boolean silent, Widget w);

static char defaultTranslations[] =
    /* Home */
    "~Shift ~Ctrl Alt<Key>osfBeginLine: last_document()\n"

    /* Backspace */
    "Ctrl<KeyPress>osfBackSpace: delete_previous_word()\n"
    "<KeyPress>osfBackSpace: delete_previous_character()\n"

    /* Delete */
    "Alt Shift Ctrl<KeyPress>osfDelete: cut_primary(\"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfDelete: cut_primary(\"rect\")\n"
    "Shift Ctrl<KeyPress>osfDelete: cut_primary()\n"
    "Ctrl<KeyPress>osfDelete: delete_to_end_of_line()\n"
    "Shift<KeyPress>osfDelete: cut_clipboard()\n"
    "<KeyPress>osfDelete: delete_next_character()\n"

    /* Insert */
    "Alt Shift Ctrl<KeyPress>osfInsert: copy_primary(\"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfInsert: copy_primary(\"rect\")\n"
    "Shift Ctrl<KeyPress>osfInsert: copy_primary()\n"
    "Shift<KeyPress>osfInsert: paste_clipboard()\n"
    "Ctrl<KeyPress>osfInsert: copy_clipboard()\n"
    "~Shift ~Ctrl<KeyPress>osfInsert: set_overtype_mode()\n"

    /* Cut/Copy/Paste */
    "Shift Ctrl<KeyPress>osfCut: cut_primary()\n"
    "<KeyPress>osfCut: cut_clipboard()\n"
    "<KeyPress>osfCopy: copy_clipboard()\n"
    "<KeyPress>osfPaste: paste_clipboard()\n"
    "<KeyPress>osfPrimaryPaste: copy_primary()\n"

    /* BeginLine */
    "Alt Shift Ctrl<KeyPress>osfBeginLine: beginning_of_file(\"extend\", \"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfBeginLine: beginning_of_file(\"extend\" \"rect\")\n"
    "Alt Shift<KeyPress>osfBeginLine: beginning_of_line(\"extend\", \"rect\")\n"
    "Meta Shift<KeyPress>osfBeginLine: beginning_of_line(\"extend\", \"rect\")\n"
    "Shift Ctrl<KeyPress>osfBeginLine: beginning_of_file(\"extend\")\n"
    "Ctrl<KeyPress>osfBeginLine: beginning_of_file()\n"
    "Shift<KeyPress>osfBeginLine: beginning_of_line(\"extend\")\n"
    "~Alt~Shift~Ctrl~Meta<KeyPress>osfBeginLine: beginning_of_line()\n"

    /* EndLine */
    "Alt Shift Ctrl<KeyPress>osfEndLine: end_of_file(\"extend\", \"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfEndLine: end_of_file(\"extend\", \"rect\")\n"
    "Alt Shift<KeyPress>osfEndLine: end_of_line(\"extend\", \"rect\")\n"
    "Meta Shift<KeyPress>osfEndLine: end_of_line(\"extend\", \"rect\")\n"
    "Shift Ctrl<KeyPress>osfEndLine: end_of_file(\"extend\")\n"
    "Ctrl<KeyPress>osfEndLine: end_of_file()\n"
    "Shift<KeyPress>osfEndLine: end_of_line(\"extend\")\n"
    "~Alt~Shift~Ctrl~Meta<KeyPress>osfEndLine: end_of_line()\n"

    /* Left */
    "Alt Shift Ctrl<KeyPress>osfLeft: backward_word(\"extend\", \"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfLeft: backward_word(\"extend\", \"rect\")\n"
    "Alt Shift<KeyPress>osfLeft: key_select(\"left\", \"rect\")\n"
    "Meta Shift<KeyPress>osfLeft: key_select(\"left\", \"rect\")\n"
    "Shift Ctrl<KeyPress>osfLeft: backward_word(\"extend\")\n"
    "Ctrl<KeyPress>osfLeft: backward_word()\n"
    "Shift<KeyPress>osfLeft: key_select(\"left\")\n"
    "~Alt~Shift~Ctrl~Meta<KeyPress>osfLeft: backward_character()\n"

    /* Right */
    "Alt Shift Ctrl<KeyPress>osfRight: forward_word(\"extend\", \"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfRight: forward_word(\"extend\", \"rect\")\n"
    "Alt Shift<KeyPress>osfRight: key_select(\"right\", \"rect\")\n"
    "Meta Shift<KeyPress>osfRight: key_select(\"right\", \"rect\")\n"
    "Shift Ctrl<KeyPress>osfRight: forward_word(\"extend\")\n"
    "Ctrl<KeyPress>osfRight: forward_word()\n"
    "Shift<KeyPress>osfRight: key_select(\"right\")\n"
    "~Alt~Shift~Ctrl~Meta<KeyPress>osfRight: forward_character()\n"

    /* Up */
    "Alt Shift Ctrl<KeyPress>osfUp: backward_paragraph(\"extend\", \"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfUp: backward_paragraph(\"extend\", \"rect\")\n"
    "Alt Shift<KeyPress>osfUp: process_shift_up(\"rect\")\n"
    "Meta Shift<KeyPress>osfUp: process_shift_up(\"rect\")\n"
    "Shift Ctrl<KeyPress>osfUp: backward_paragraph(\"extend\")\n"
    "Ctrl<KeyPress>osfUp: backward_paragraph()\n"
    "Shift<KeyPress>osfUp: process_shift_up()\n"
    "~Alt~Shift~Ctrl~Meta<KeyPress>osfUp: process_up()\n"

    /* Down */
    "Alt Shift Ctrl<KeyPress>osfDown: forward_paragraph(\"extend\", \"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfDown: forward_paragraph(\"extend\", \"rect\")\n"
    "Alt Shift<KeyPress>osfDown: process_shift_down(\"rect\")\n"
    "Meta Shift<KeyPress>osfDown: process_shift_down(\"rect\")\n"
    "Shift Ctrl<KeyPress>osfDown: forward_paragraph(\"extend\")\n"
    "Ctrl<KeyPress>osfDown: forward_paragraph()\n"
    "Shift<KeyPress>osfDown: process_shift_down()\n"
    "~Alt~Shift~Ctrl~Meta<KeyPress>osfDown: process_down()\n"

    /* PageUp */
    "Alt Shift Ctrl<KeyPress>osfPageUp: page_left(\"extend\", \"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfPageUp: page_left(\"extend\", \"rect\")\n"
    "Alt Shift<KeyPress>osfPageUp: previous_page(\"extend\", \"rect\")\n"
    "Meta Shift<KeyPress>osfPageUp: previous_page(\"extend\", \"rect\")\n"
    "Shift Ctrl<KeyPress>osfPageUp: page_left(\"extend\")\n"
    "Ctrl<KeyPress>osfPageUp: previous_document()\n"
    "Shift<KeyPress>osfPageUp: previous_page(\"extend\")\n"
    "~Alt ~Shift ~Ctrl ~Meta<KeyPress>osfPageUp: previous_page()\n"

    /* PageDown */
    "Alt Shift Ctrl<KeyPress>osfPageDown: page_right(\"extend\", \"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfPageDown: page_right(\"extend\", \"rect\")\n"
    "Alt Shift<KeyPress>osfPageDown: next_page(\"extend\", \"rect\")\n"
    "Meta Shift<KeyPress>osfPageDown: next_page(\"extend\", \"rect\")\n"
    "Shift Ctrl<KeyPress>osfPageDown: page_right(\"extend\")\n"
    "Ctrl<KeyPress>osfPageDown: next_document()\n"
    "Shift<KeyPress>osfPageDown: next_page(\"extend\")\n"
    "~Alt ~Shift ~Ctrl ~Meta<KeyPress>osfPageDown: next_page()\n"

    /* PageLeft and PageRight are placed later than the PageUp/PageDown
       bindings.  Some systems map osfPageLeft to Ctrl-PageUp.
       Overloading this single key gives problems, and we want to give
       priority to the normal version. */

    /* PageLeft */
    "Alt Shift<KeyPress>osfPageLeft: page_left(\"extend\", \"rect\")\n"
    "Meta Shift<KeyPress>osfPageLeft: page_left(\"extend\", \"rect\")\n"
    "Shift<KeyPress>osfPageLeft: page_left(\"extend\")\n"
    "~Alt ~Shift ~Ctrl ~Meta<KeyPress>osfPageLeft: page_left()\n"

    /* PageRight */
    "Alt Shift<KeyPress>osfPageRight: page_right(\"extend\", \"rect\")\n"
    "Meta Shift<KeyPress>osfPageRight: page_right(\"extend\", \"rect\")\n"
    "Shift<KeyPress>osfPageRight: page_right(\"extend\")\n"
    "~Alt ~Shift ~Ctrl ~Meta<KeyPress>osfPageRight: page_right()\n"

    "Shift<KeyPress>osfSelect: key_select()\n"
    "<KeyPress>osfCancel: process_cancel()\n"
    "Ctrl~Alt~Meta<KeyPress>v: paste_clipboard()\n"
    "Ctrl~Alt~Meta<KeyPress>c: copy_clipboard()\n"
    "Ctrl~Alt~Meta<KeyPress>x: cut_clipboard()\n"
    "Ctrl~Alt~Meta<KeyPress>u: delete_to_start_of_line()\n"
    "Ctrl<KeyPress>Return: newline_and_indent()\n"
    "Shift<KeyPress>Return: newline_no_indent()\n"
    "<KeyPress>Return: newline()\n"
    /* KP_Enter = osfActivate
       Note: Ctrl+KP_Enter is already bound to Execute Command Line... */
    "Shift<KeyPress>osfActivate: newline_no_indent()\n"
    "<KeyPress>osfActivate: newline()\n"
    "Ctrl<KeyPress>Tab: self_insert()\n"
    "<KeyPress>Tab: process_tab()\n"
    "Alt Shift Ctrl<KeyPress>space: key_select(\"rect\")\n"
    "Meta Shift Ctrl<KeyPress>space: key_select(\"rect\")\n"
    "Shift Ctrl~Meta~Alt<KeyPress>space: key_select()\n"
    "Ctrl~Meta~Alt<KeyPress>slash: select_all()\n"
    "Ctrl~Meta~Alt<KeyPress>backslash: deselect_all()\n"
    "<KeyPress>: self_insert()\n"
    "Alt Ctrl<Btn1Down>: move_destination()\n"
    "Meta Ctrl<Btn1Down>: move_destination()\n"
    "Shift Ctrl<Btn1Down>: extend_start(\"rect\")\n"
    "Shift<Btn1Down>: extend_start()\n"
    "<Btn1Down>: grab_focus()\n"
    "Button1 Ctrl<MotionNotify>: extend_adjust(\"rect\")\n"
    "Button1~Ctrl<MotionNotify>: extend_adjust()\n"
    "<Btn1Up>: extend_end()\n"
    "<Btn2Down>: secondary_or_drag_start()\n"
    "Shift Ctrl Button2<MotionNotify>: secondary_or_drag_adjust(\"rect\", \"copy\", \"overlay\")\n"
    "Shift Button2<MotionNotify>: secondary_or_drag_adjust(\"copy\")\n"
    "Ctrl Button2<MotionNotify>: secondary_or_drag_adjust(\"rect\", \"overlay\")\n"
    "Button2<MotionNotify>: secondary_or_drag_adjust()\n"
    "Shift Ctrl<Btn2Up>: move_to_or_end_drag(\"copy\", \"overlay\")\n"
    "Shift <Btn2Up>: move_to_or_end_drag(\"copy\")\n"
    "Alt<Btn2Up>: exchange()\n"
    "Meta<Btn2Up>: exchange()\n"
    "Ctrl<Btn2Up>: copy_to_or_end_drag(\"overlay\")\n"
    "<Btn2Up>: copy_to_or_end_drag()\n"
    "Ctrl~Meta~Alt<Btn3Down>: mouse_pan()\n"
    "Ctrl~Meta~Alt Button3<MotionNotify>: mouse_pan()\n"
    "<Btn3Up>: end_drag()\n"
    "<FocusIn>: focusIn()\n"
    "<FocusOut>: focusOut()\n"
    /* Support for mouse wheel in XFree86 */
    "Shift<Btn4Down>,<Btn4Up>: scroll_up(1)\n"
    "Shift<Btn5Down>,<Btn5Up>: scroll_down(1)\n"
    "Ctrl<Btn4Down>,<Btn4Up>: scroll_up(1, pages)\n"
    "Ctrl<Btn5Down>,<Btn5Up>: scroll_down(1, pages)\n"
    "<Btn4Down>,<Btn4Up>: scroll_up(5)\n"
    "<Btn5Down>,<Btn5Up>: scroll_down(5)\n";
/* some of the translations from the Motif text widget were not picked up:
 :<KeyPress>osfSelect: set-anchor()\n\
 :<KeyPress>osfActivate: activate()\n\
~Shift Ctrl~Meta~Alt<KeyPress>Return: activate()\n\
 ~Shift Ctrl~Meta~Alt<KeyPress>space: set-anchor()\n\
  :<KeyPress>osfClear: clear-selection()\n\
 ~Shift~Ctrl~Meta~Alt<KeyPress>Return: process-return()\n\
 Shift~Meta~Alt<KeyPress>Tab: prev-tab-group()\n\
 Ctrl~Meta~Alt<KeyPress>Tab: next-tab-group()\n\
 <UnmapNotify>: unmap()\n\
 <EnterNotify>: enter()\n\
 <LeaveNotify>: leave()\n
*/

static XtActionsRec actionsList[] = {
    {(String) "self-insert", selfInsertAP},
    {(String) "self_insert", selfInsertAP},
    {(String) "grab-focus", grabFocusAP},
    {(String) "grab_focus", grabFocusAP},
    {(String) "extend-adjust", extendAdjustAP},
    {(String) "extend_adjust", extendAdjustAP},
    {(String) "extend-start", extendStartAP},
    {(String) "extend_start", extendStartAP},
    {(String) "extend-end", extendEndAP},
    {(String) "extend_end", extendEndAP},
    {(String) "secondary-adjust", secondaryAdjustAP},
    {(String) "secondary_adjust", secondaryAdjustAP},
    {(String) "secondary-or-drag-adjust", secondaryOrDragAdjustAP},
    {(String) "secondary_or_drag_adjust", secondaryOrDragAdjustAP},
    {(String) "secondary-start", secondaryStartAP},
    {(String) "secondary_start", secondaryStartAP},
    {(String) "secondary-or-drag-start", secondaryOrDragStartAP},
    {(String) "secondary_or_drag_start", secondaryOrDragStartAP},
    {(String) "process-bdrag", secondaryOrDragStartAP},
    {(String) "process_bdrag", secondaryOrDragStartAP},
    {(String) "move-destination", moveDestinationAP},
    {(String) "move_destination", moveDestinationAP},
    {(String) "move-to", moveToAP},
    {(String) "move_to", moveToAP},
    {(String) "move-to-or-end-drag", moveToOrEndDragAP},
    {(String) "move_to_or_end_drag", moveToOrEndDragAP},
    {(String) "end_drag", endDragAP},
    {(String) "copy-to", copyToAP},
    {(String) "copy_to", copyToAP},
    {(String) "copy-to-or-end-drag", copyToOrEndDragAP},
    {(String) "copy_to_or_end_drag", copyToOrEndDragAP},
    {(String) "exchange", exchangeAP},
    {(String) "process-cancel", processCancelAP},
    {(String) "process_cancel", processCancelAP},
    {(String) "paste-clipboard", pasteClipboardAP},
    {(String) "paste_clipboard", pasteClipboardAP},
    {(String) "copy-clipboard", copyClipboardAP},
    {(String) "copy_clipboard", copyClipboardAP},
    {(String) "cut-clipboard", cutClipboardAP},
    {(String) "cut_clipboard", cutClipboardAP},
    {(String) "copy-primary", copyPrimaryAP},
    {(String) "copy_primary", copyPrimaryAP},
    {(String) "cut-primary", cutPrimaryAP},
    {(String) "cut_primary", cutPrimaryAP},
    {(String) "newline", newlineAP},
    {(String) "newline-and-indent", newlineAndIndentAP},
    {(String) "newline_and_indent", newlineAndIndentAP},
    {(String) "newline-no-indent", newlineNoIndentAP},
    {(String) "newline_no_indent", newlineNoIndentAP},
    {(String) "delete-selection", deleteSelectionAP},
    {(String) "delete_selection", deleteSelectionAP},
    {(String) "delete-previous-character", deletePreviousCharacterAP},
    {(String) "delete_previous_character", deletePreviousCharacterAP},
    {(String) "delete-next-character", deleteNextCharacterAP},
    {(String) "delete_next_character", deleteNextCharacterAP},
    {(String) "delete-previous-word", deletePreviousWordAP},
    {(String) "delete_previous_word", deletePreviousWordAP},
    {(String) "delete-next-word", deleteNextWordAP},
    {(String) "delete_next_word", deleteNextWordAP},
    {(String) "delete-to-start-of-line", deleteToStartOfLineAP},
    {(String) "delete_to_start_of_line", deleteToStartOfLineAP},
    {(String) "delete-to-end-of-line", deleteToEndOfLineAP},
    {(String) "delete_to_end_of_line", deleteToEndOfLineAP},
    {(String) "forward-character", forwardCharacterAP},
    {(String) "forward_character", forwardCharacterAP},
    {(String) "backward-character", backwardCharacterAP},
    {(String) "backward_character", backwardCharacterAP},
    {(String) "key-select", keySelectAP},
    {(String) "key_select", keySelectAP},
    {(String) "process-up", processUpAP},
    {(String) "process_up", processUpAP},
    {(String) "process-down", processDownAP},
    {(String) "process_down", processDownAP},
    {(String) "process-shift-up", processShiftUpAP},
    {(String) "process_shift_up", processShiftUpAP},
    {(String) "process-shift-down", processShiftDownAP},
    {(String) "process_shift_down", processShiftDownAP},
    {(String) "process-home", beginningOfLineAP},
    {(String) "process_home", beginningOfLineAP},
    {(String) "forward-word", forwardWordAP},
    {(String) "forward_word", forwardWordAP},
    {(String) "backward-word", backwardWordAP},
    {(String) "backward_word", backwardWordAP},
    {(String) "forward-paragraph", forwardParagraphAP},
    {(String) "forward_paragraph", forwardParagraphAP},
    {(String) "backward-paragraph", backwardParagraphAP},
    {(String) "backward_paragraph", backwardParagraphAP},
    {(String) "beginning-of-line", beginningOfLineAP},
    {(String) "beginning_of_line", beginningOfLineAP},
    {(String) "end-of-line", endOfLineAP},
    {(String) "end_of_line", endOfLineAP},
    {(String) "beginning-of-file", beginningOfFileAP},
    {(String) "beginning_of_file", beginningOfFileAP},
    {(String) "end-of-file", endOfFileAP},
    {(String) "end_of_file", endOfFileAP},
    {(String) "next-page", nextPageAP},
    {(String) "next_page", nextPageAP},
    {(String) "previous-page", previousPageAP},
    {(String) "previous_page", previousPageAP},
    {(String) "page-left", pageLeftAP},
    {(String) "page_left", pageLeftAP},
    {(String) "page-right", pageRightAP},
    {(String) "page_right", pageRightAP},
    {(String) "toggle-overstrike", toggleOverstrikeAP},
    {(String) "toggle_overstrike", toggleOverstrikeAP},
    {(String) "scroll-up", scrollUpAP},
    {(String) "scroll_up", scrollUpAP},
    {(String) "scroll-down", scrollDownAP},
    {(String) "scroll_down", scrollDownAP},
    {(String) "scroll_left", scrollLeftAP},
    {(String) "scroll_right", scrollRightAP},
    {(String) "scroll-to-line", scrollToLineAP},
    {(String) "scroll_to_line", scrollToLineAP},
    {(String) "select-all", selectAllAP},
    {(String) "select_all", selectAllAP},
    {(String) "deselect-all", deselectAllAP},
    {(String) "deselect_all", deselectAllAP},
    {(String) "focusIn", focusInAP},
    {(String) "focusOut", focusOutAP},
    {(String) "process-return", selfInsertAP},
    {(String) "process_return", selfInsertAP},
    {(String) "process-tab", processTabAP},
    {(String) "process_tab", processTabAP},
    {(String) "insert-string", insertStringAP},
    {(String) "insert_string", insertStringAP},
    {(String) "mouse_pan", mousePanAP},
};

/* The motif text widget defined a bunch of actions which the nedit text
   widget as-of-yet does not support:

     Actions which were not bound to keys (for emacs emulation, some of
     them should probably be supported:

    kill-next-character()
    kill-next-word()
    kill-previous-character()
    kill-previous-word()
    kill-selection()
    kill-to-end-of-line()
    kill-to-start-of-line()
    unkill()
    next-line()
    newline-and-backup()
    beep()
    redraw-display()
    scroll-one-line-down()
    scroll-one-line-up()
    set-insertion-point()

    Actions which are not particularly useful:

    set-anchor()
    activate()
    clear-selection() -> this is a wierd one
    do-quick-action() -> don't think this ever worked
    Help()
    next-tab-group()
    select-adjust()
    select-start()
    select-end()
*/

static XtResource resources[] = {
	{ XmNhighlightThickness,	   XmCHighlightThickness,		XmRDimension,  sizeof(Dimension),	  XtOffset(TextWidget, primitive.highlight_thickness), XmRInt,    0                                         },
	{ XmNshadowThickness,		   XmCShadowThickness,  		XmRDimension,  sizeof(Dimension),	  XtOffset(TextWidget, primitive.shadow_thickness),    XmRInt,    0                                         },
	{ textNfont,				   textCFont,					XmRFontStruct, sizeof(XFontStruct *), XtOffset(TextWidget, text.fontStruct),			   XmRString, (String) "fixed"                          },
	{ textNselectForeground,	   textCSelectForeground,		XmRPixel,	   sizeof(Pixel),		  XtOffset(TextWidget, text.selectFGPixel), 		   XmRString, (String)NEDIT_DEFAULT_SEL_FG              },
	{ textNselectBackground,	   textCSelectBackground,		XmRPixel,	   sizeof(Pixel),		  XtOffset(TextWidget, text.selectBGPixel), 		   XmRString, (String)NEDIT_DEFAULT_SEL_BG              },
	{ textNhighlightForeground,    textCHighlightForeground,	XmRPixel,	   sizeof(Pixel),		  XtOffset(TextWidget, text.highlightFGPixel),  	   XmRString, (String)NEDIT_DEFAULT_HI_FG               },
	{ textNhighlightBackground,    textCHighlightBackground,	XmRPixel,	   sizeof(Pixel),		  XtOffset(TextWidget, text.highlightBGPixel),  	   XmRString, (String)NEDIT_DEFAULT_HI_BG               },
	{ textNlineNumForeground,	   textCLineNumForeground,  	XmRPixel,	   sizeof(Pixel),		  XtOffset(TextWidget, text.lineNumFGPixel),		   XmRString, (String)NEDIT_DEFAULT_LINENO_FG           },
	{ textNcursorForeground,	   textCCursorForeground,		XmRPixel,	   sizeof(Pixel),		  XtOffset(TextWidget, text.cursorFGPixel), 		   XmRString, (String)NEDIT_DEFAULT_CURSOR_FG           },
	{ textNcalltipForeground,	   textCcalltipForeground,  	XmRPixel,	   sizeof(Pixel),		  XtOffset(TextWidget, text.calltipFGPixel),		   XmRString, (String)NEDIT_DEFAULT_CALLTIP_FG          },
	{ textNcalltipBackground,	   textCcalltipBackground,  	XmRPixel,	   sizeof(Pixel),		  XtOffset(TextWidget, text.calltipBGPixel),		   XmRString, (String)NEDIT_DEFAULT_CALLTIP_BG          },
	{ textNbacklightCharTypes,     textCBacklightCharTypes, 	XmRString,     sizeof(XmString),	  XtOffset(TextWidget, text.backlightCharTypes),	   XmRString, nullptr                                   },
	{ textNrows,				   textCRows,					XmRInt, 	   sizeof(int), 		  XtOffset(TextWidget, text.rows),  				   XmRString, (String) "24"                             },
	{ textNcolumns, 			   textCColumns,				XmRInt, 	   sizeof(int), 		  XtOffset(TextWidget, text.columns),				   XmRString, (String) "80"                             },
	{ textNmarginWidth, 		   textCMarginWidth,			XmRInt, 	   sizeof(int), 		  XtOffset(TextWidget, text.marginWidth),			   XmRString, (String) "5"                              },
	{ textNmarginHeight,		   textCMarginHeight,			XmRInt, 	   sizeof(int), 		  XtOffset(TextWidget, text.marginHeight),  		   XmRString, (String) "5"                              },
	{ textNpendingDelete,		   textCPendingDelete,  		XmRBoolean,    sizeof(Boolean), 	  XtOffset(TextWidget, text.pendingDelete), 		   XmRString, (String) "True"                           },
	{ textNautoWrap,			   textCAutoWrap,				XmRBoolean,    sizeof(Boolean), 	  XtOffset(TextWidget, text.autoWrap),  			   XmRString, (String) "True"                           },
	{ textNcontinuousWrap,  	   textCContinuousWrap, 		XmRBoolean,    sizeof(Boolean), 	  XtOffset(TextWidget, text.continuousWrap),		   XmRString, (String) "True"                           },
	{ textNautoIndent,  		   textCAutoIndent, 			XmRBoolean,    sizeof(Boolean), 	  XtOffset(TextWidget, text.autoIndent),			   XmRString, (String) "True"                           },
	{ textNsmartIndent, 		   textCSmartIndent,			XmRBoolean,    sizeof(Boolean), 	  XtOffset(TextWidget, text.smartIndent),			   XmRString, (String) "False"                          },
	{ textNoverstrike,  		   textCOverstrike, 			XmRBoolean,    sizeof(Boolean), 	  XtOffset(TextWidget, text.overstrike),			   XmRString, (String) "False"                          },
	{ textNheavyCursor, 		   textCHeavyCursor,			XmRBoolean,    sizeof(Boolean), 	  XtOffset(TextWidget, text.heavyCursor),			   XmRString, (String) "False"                          },
	{ textNreadOnly,			   textCReadOnly,				XmRBoolean,    sizeof(Boolean), 	  XtOffset(TextWidget, text.readOnly),  			   XmRString, (String) "False"                          },
	{ textNhidePointer, 		   textCHidePointer,			XmRBoolean,    sizeof(Boolean), 	  XtOffset(TextWidget, text.hidePointer),			   XmRString, (String) "False"                          },
	{ textNwrapMargin,  		   textCWrapMargin, 			XmRInt, 	   sizeof(int), 		  XtOffset(TextWidget, text.wrapMargin),			   XmRString, (String) "0"                              },
	{ textNhScrollBar,  		   textCHScrollBar, 			XmRWidget,     sizeof(Widget),  	  XtOffset(TextWidget, text.hScrollBar),			   XmRString, (String) ""                               },
	{ textNvScrollBar,  		   textCVScrollBar, 			XmRWidget,     sizeof(Widget),  	  XtOffset(TextWidget, text.vScrollBar),			   XmRString, (String) ""                               },
	{ textNlineNumCols, 		   textCLineNumCols,			XmRInt, 	   sizeof(int), 		  XtOffset(TextWidget, text.lineNumCols),			   XmRString, (String) "0"                              },
	{ textNautoShowInsertPos,	   textCAutoShowInsertPos,  	XmRBoolean,    sizeof(Boolean), 	  XtOffset(TextWidget, text.autoShowInsertPos), 	   XmRString, (String) "True"                           },
	{ textNautoWrapPastedText,     textCAutoWrapPastedText, 	XmRBoolean,    sizeof(Boolean), 	  XtOffset(TextWidget, text.autoWrapPastedText),	   XmRString, (String) "False"                          },
	{ textNwordDelimiters,  	   textCWordDelimiters, 		XmRString,     sizeof(char *),  	  XtOffset(TextWidget, text.delimiters),			   XmRString, (String) ".,/\\`'!@#%^&*()-=+{}[]\":;<>?" },
	{ textNblinkRate,			   textCBlinkRate,  			XmRInt, 	   sizeof(int), 		  XtOffset(TextWidget, text.cursorBlinkRate),		   XmRString, (String) "500"                            },
	{ textNemulateTabs, 		   textCEmulateTabs,			XmRInt, 	   sizeof(int), 		  XtOffset(TextWidget, text.emulateTabs),			   XmRString, (String) "0"                              },
	{ textNfocusCallback,		   textCFocusCallback,  		XmRCallback,   sizeof(XtPointer),	  XtOffset(TextWidget, text.focusInCB), 			   XtRCallback, nullptr                                 },
	{ textNlosingFocusCallback,    textCLosingFocusCallback,	XmRCallback,   sizeof(XtPointer),	  XtOffset(TextWidget, text.focusOutCB),			   XtRCallback, nullptr                                 },
	{ textNcursorMovementCallback, textCCursorMovementCallback, XmRCallback,   sizeof(XtPointer),	  XtOffset(TextWidget, text.cursorCB),  			   XtRCallback, nullptr                                 },
	{ textNdragStartCallback,	   textCDragStartCallback,  	XmRCallback,   sizeof(XtPointer),	  XtOffset(TextWidget, text.dragStartCB),			   XtRCallback, nullptr                                 },
	{ textNdragEndCallback, 	   textCDragEndCallback,		XmRCallback,   sizeof(XtPointer),	  XtOffset(TextWidget, text.dragEndCB), 			   XtRCallback, nullptr                                 },
	{ textNsmartIndentCallback,    textCSmartIndentCallback,	XmRCallback,   sizeof(XtPointer),	  XtOffset(TextWidget, text.smartIndentCB), 		   XtRCallback, nullptr                                 },
	{ textNcursorVPadding,  	   textCCursorVPadding, 		XtRCardinal,   sizeof(Cardinal),	  XtOffset(TextWidget, text.cursorVPadding),		   XmRString, (String) "0"                              }
};

static TextClassRec textClassRec = {
	/* CoreClassPart */
	{
		(WidgetClass)&xmPrimitiveClassRec, /* superclass            */
		(String) "Text",                   /* class_name            */
		sizeof(TextRec),                   /* widget_size           */
		nullptr,                           /* class_initialize      */
		nullptr,                           /* class_part_initialize */
		FALSE,                             /* class_inited          */
		initialize,                        /* initialize            */
		nullptr,                           /* initialize_hook       */
		realize,                           /* realize               */
		actionsList,                       /* actions               */
		XtNumber(actionsList),             /* num_actions           */
		resources,                         /* resources             */
		XtNumber(resources),               /* num_resources         */
		NULLQUARK,                         /* xrm_class             */
		TRUE,                              /* compress_motion       */
		TRUE,                              /* compress_exposure     */
		TRUE,                              /* compress_enterleave   */
		FALSE,                             /* visible_interest      */
		(XtWidgetProc)destroy,             /* destroy               */
		(XtWidgetProc)resize,              /* resize                */
		(XtExposeProc)redisplay,           /* expose                */
		(XtSetValuesFunc)setValues,        /* set_values            */
		nullptr,                           /* set_values_hook       */
		XtInheritSetValuesAlmost,          /* set_values_almost     */
		nullptr,                           /* get_values_hook       */
		nullptr,                           /* accept_focus          */
		XtVersion,                         /* version               */
		nullptr,                           /* callback private      */
		defaultTranslations,               /* tm_table              */
		queryGeometry,                     /* query_geometry        */
		nullptr,                           /* display_accelerator   */
		nullptr,                           /* extension             */
	},
	/* Motif primitive class fields */
	{
		(XtWidgetProc)_XtInherit,           /* Primitive border_highlight   */
		(XtWidgetProc)_XtInherit,           /* Primitive border_unhighlight */
		nullptr, /*XtInheritTranslations,*/ /* translations                 */
		nullptr,                            /* arm_and_activate             */
		nullptr,                            /* get resources      		*/
		0,                                  /* num get_resources  		*/
		nullptr,                            /* extension                    */
	},
	/* Text class part */
	{
		0, /* ignored	                */
	}
};

WidgetClass textWidgetClass = (WidgetClass)&textClassRec;
#define NEDIT_HIDE_CURSOR_MASK (KeyPressMask)
#define NEDIT_SHOW_CURSOR_MASK (FocusChangeMask | PointerMotionMask | ButtonMotionMask | ButtonPressMask | ButtonReleaseMask)
static char empty_bits[] = {0x00, 0x00, 0x00, 0x00};
static Cursor empty_cursor = 0;

/*
** Widget initialize method
*/
static void initialize(Widget request, Widget newWidget, ArgList args, Cardinal *num_args) {

	(void)args;
	(void)num_args;
	
	auto new_widget = reinterpret_cast<TextWidget>(newWidget);

	XFontStruct *fs = new_widget->text.fontStruct;
	int textLeft;
	int charWidth   = fs->max_bounds.width;
	int marginWidth = new_widget->text.marginWidth;
	int lineNumCols = new_widget->text.lineNumCols;
	
	/* Set the initial window size based on the rows and columns resources */
	if (request->core.width == 0) {
		new_widget->core.width = charWidth * new_widget->text.columns + marginWidth * 2 + (lineNumCols == 0 ? 0 : marginWidth + charWidth * lineNumCols);
	}
	
	if (request->core.height == 0) {
		new_widget->core.height = (fs->ascent + fs->descent) * new_widget->text.rows + new_widget->text.marginHeight * 2;
	}

	/* The default colors work for B&W as well as color, except for
	   selectFGPixel and selectBGPixel, where color highlighting looks
	   much better without reverse video, so if we get here, and the
	   selection is totally unreadable because of the bad default colors,
	   swap the colors and make the selection reverse video */
	Pixel white = WhitePixelOfScreen(XtScreen(newWidget));
	Pixel black = BlackPixelOfScreen(XtScreen(newWidget));
	
	if (new_widget->text.selectBGPixel == white && new_widget->core.background_pixel == white && new_widget->text.selectFGPixel == black && new_widget->primitive.foreground == black) {
		new_widget->text.selectBGPixel = black;
		new_widget->text.selectFGPixel = white;
	}

	/* Create the initial text buffer for the widget to display (which can
	   be replaced later with TextSetBuffer) */
	auto buf = new TextBuffer;

	/* Create and initialize the text-display part of the widget */
	textLeft = new_widget->text.marginWidth + (lineNumCols == 0 ? 0 : marginWidth + charWidth * lineNumCols);
	new_widget->text.textD = new TextDisplay(newWidget, new_widget->text.hScrollBar, new_widget->text.vScrollBar, textLeft, new_widget->text.marginHeight, new_widget->core.width - marginWidth - textLeft,
	                new_widget->core.height - new_widget->text.marginHeight * 2, lineNumCols == 0 ? 0 : marginWidth, lineNumCols == 0 ? 0 : lineNumCols * charWidth, buf, new_widget->text.fontStruct, new_widget->core.background_pixel,
	                new_widget->primitive.foreground, new_widget->text.selectFGPixel, new_widget->text.selectBGPixel, new_widget->text.highlightFGPixel, new_widget->text.highlightBGPixel, new_widget->text.cursorFGPixel,
	                new_widget->text.lineNumFGPixel, new_widget->text.continuousWrap, new_widget->text.wrapMargin, new_widget->text.backlightCharTypes, new_widget->text.calltipFGPixel, new_widget->text.calltipBGPixel);

	/* Add mandatory delimiters blank, tab, and newline to the list of
	   delimiters.  The memory use scheme here is that new values are
	   always copied, and can therefore be safely freed on subsequent
	   set-values calls or destroy */

    char *delimiters = XtMalloc(strlen(new_widget->text.delimiters) + 4);
    sprintf(delimiters, "%s%s", " \t\n", new_widget->text.delimiters);
    new_widget->text.delimiters = delimiters;

	/* Start with the cursor blanked (widgets don't have focus on creation,
	   the initial FocusIn event will unblank it and get blinking started) */
	new_widget->text.textD->cursorOn = false;

	/* Initialize the widget variables */
	new_widget->text.autoScrollProcID = 0;
	new_widget->text.cursorBlinkProcID = 0;
	new_widget->text.dragState = NOT_CLICKED;
	new_widget->text.multiClickState = NO_CLICKS;
	new_widget->text.lastBtnDown = 0;
	new_widget->text.selectionOwner = false;
	new_widget->text.motifDestOwner = false;
	new_widget->text.emTabsBeforeCursor = 0;

	/* Register the widget to the input manager */
	XmImRegister(newWidget, 0);
	/* In case some Resources for the IC need to be set, add them below */
	XmImVaSetValues(newWidget, nullptr);

	XtAddEventHandler(newWidget, GraphicsExpose, True, (XtEventHandler)redisplayGE, (Opaque) nullptr);

	if (new_widget->text.hidePointer) {
		Display *theDisplay;
		Pixmap empty_pixmap;
		XColor black_color;
		/* Set up the empty Cursor */
		if (empty_cursor == 0) {
			theDisplay = XtDisplay(newWidget);
			empty_pixmap = XCreateBitmapFromData(theDisplay, RootWindowOfScreen(XtScreen(newWidget)), empty_bits, 1, 1);
			XParseColor(theDisplay, DefaultColormapOfScreen(XtScreen(newWidget)), "black", &black_color);
			empty_cursor = XCreatePixmapCursor(theDisplay, empty_pixmap, empty_pixmap, &black_color, &black_color, 0, 0);
		}

		/* Add event handler to hide the pointer on keypresses */
		XtAddEventHandler(newWidget, NEDIT_HIDE_CURSOR_MASK, False, handleHidePointer, (Opaque) nullptr);
	}
}

/* Hide the pointer while the user is typing */
static void handleHidePointer(Widget w, XtPointer unused, XEvent *event, Boolean *continue_to_dispatch) {
	(void)unused;
	(void)event;
	(void)continue_to_dispatch;

	auto tw = reinterpret_cast<TextWidget>(w);
	ShowHidePointer(tw, True);
}

/* Restore the pointer if the mouse moves or focus changes */
static void handleShowPointer(Widget w, XtPointer unused, XEvent *event, Boolean *continue_to_dispatch) {
	(void)unused;
	(void)event;
	(void)continue_to_dispatch;

	auto tw = reinterpret_cast<TextWidget>(w);
	ShowHidePointer(tw, False);
}

void ShowHidePointer(TextWidget w, Boolean hidePointer) {
	if (w->text.hidePointer) {
		if (hidePointer != w->text.textD->pointerHidden) {
			if (hidePointer) {
				/* Don't listen for keypresses any more */
				XtRemoveEventHandler((Widget)w, NEDIT_HIDE_CURSOR_MASK, False, handleHidePointer, (Opaque) nullptr);
				/* Switch to empty cursor */
				XDefineCursor(XtDisplay(w), XtWindow(w), empty_cursor);

				w->text.textD->pointerHidden = true;

				/* Listen to mouse movement, focus change, and button presses */
				XtAddEventHandler((Widget)w, NEDIT_SHOW_CURSOR_MASK, False, handleShowPointer, (Opaque) nullptr);
			} else {
				/* Don't listen to mouse/focus events any more */
				XtRemoveEventHandler((Widget)w, NEDIT_SHOW_CURSOR_MASK, False, handleShowPointer, (Opaque) nullptr);
				/* Switch to regular cursor */
				XUndefineCursor(XtDisplay(w), XtWindow(w));

				w->text.textD->pointerHidden = false;

				/* Listen for keypresses now */
				XtAddEventHandler((Widget)w, NEDIT_HIDE_CURSOR_MASK, False, handleHidePointer, (Opaque) nullptr);
			}
		}
	}
}

/*
** Widget destroy method
*/
static void destroy(TextWidget w) {
	TextBuffer *buf;

	/* Free the text display and possibly the attached buffer.  The buffer
	   is freed only if after removing all of the modify procs (by calling
	   StopHandlingXSelections and TextDFree) there are no modify procs
	   left */
	StopHandlingXSelections((Widget)w);
	buf = w->text.textD->buffer;
	delete w->text.textD;
	if (buf->modifyProcs_.empty())
		delete buf;

	if (w->text.cursorBlinkProcID != 0)
		XtRemoveTimeOut(w->text.cursorBlinkProcID);

	XtRemoveAllCallbacks((Widget)w, textNfocusCallback);
	XtRemoveAllCallbacks((Widget)w, textNlosingFocusCallback);
	XtRemoveAllCallbacks((Widget)w, textNcursorMovementCallback);
	XtRemoveAllCallbacks((Widget)w, textNdragStartCallback);
	XtRemoveAllCallbacks((Widget)w, textNdragEndCallback);

	/* Unregister the widget from the input manager */
	XmImUnregister((Widget)w);
}

/*
** Widget resize method.  Called when the size of the widget changes
*/
static void resize(TextWidget w) {
	XFontStruct *fs = w->text.fontStruct;
	int height = w->core.height, width = w->core.width;
	int marginWidth = w->text.marginWidth, marginHeight = w->text.marginHeight;
	int lineNumAreaWidth = w->text.lineNumCols == 0 ? 0 : w->text.marginWidth + fs->max_bounds.width * w->text.lineNumCols;

	w->text.columns = (width - marginWidth * 2 - lineNumAreaWidth) / fs->max_bounds.width;
	w->text.rows = (height - marginHeight * 2) / (fs->ascent + fs->descent);

	/* Reject widths and heights less than a character, which the text
	   display can't tolerate.  This is not strictly legal, but I've seen
	   it done in other widgets and it seems to do no serious harm.  NEdit
	   prevents panes from getting smaller than one line, but sometimes
	   splitting windows on Linux 2.0 systems (same Motif, why the change in
	   behavior?), causes one or two resize calls with < 1 line of height.
	   Fixing it here is 100x easier than re-designing TextDisplay.c */
	if (w->text.columns < 1) {
		w->text.columns = 1;
		w->core.width = width = fs->max_bounds.width + marginWidth * 2 + lineNumAreaWidth;
	}
	if (w->text.rows < 1) {
		w->text.rows = 1;
		w->core.height = height = fs->ascent + fs->descent + marginHeight * 2;
	}

	/* Resize the text display that the widget uses to render text */
	w->text.textD->TextDResize(width - marginWidth * 2 - lineNumAreaWidth, height - marginHeight * 2);

	/* if the window became shorter or narrower, there may be text left
	   in the bottom or right margin area, which must be cleaned up */
	if (XtIsRealized((Widget)w)) {
		XClearArea(XtDisplay(w), XtWindow(w), 0, height - marginHeight, width, marginHeight, False);
		XClearArea(XtDisplay(w), XtWindow(w), width - marginWidth, 0, marginWidth, height, False);
	}
}

/*
** Widget redisplay method
*/
static void redisplay(TextWidget w, XEvent *event, Region region) {

	(void)region;

	XExposeEvent *e = &event->xexpose;

	w->text.textD->TextDRedisplayRect(e->x, e->y, e->width, e->height);
}

static Bool findGraphicsExposeOrNoExposeEvent(Display *theDisplay, XEvent *event, XPointer arg) {
	if ((theDisplay == event->xany.display) && (event->type == GraphicsExpose || event->type == NoExpose) && ((Widget)arg == XtWindowToWidget(event->xany.display, event->xany.window))) {
		return (True);
	} else {
		return (False);
	}
}

static void adjustRectForGraphicsExposeOrNoExposeEvent(TextWidget w, XEvent *event, bool *first, int *left, int *top, int *width, int *height) {
	bool removeQueueEntry = false;

	if (event->type == GraphicsExpose) {
		XGraphicsExposeEvent *e = &event->xgraphicsexpose;
		int x = e->x, y = e->y;

		w->text.textD->TextDImposeGraphicsExposeTranslation(&x, &y);
		if (*first) {
			*left = x;
			*top = y;
			*width = e->width;
			*height = e->height;

			*first = false;
		} else {
			int prev_left = *left;
			int prev_top = *top;

			*left = std::min<int>(*left, x);
			*top = std::min<int>(*top, y);
			*width = std::max<int>(prev_left + *width, x + e->width) - *left;
			*height = std::max<int>(prev_top + *height, y + e->height) - *top;
		}
		if (e->count == 0) {
			removeQueueEntry = true;
		}
	} else if (event->type == NoExpose) {
		removeQueueEntry = true;
	}
	if (removeQueueEntry) {
		w->text.textD->TextDPopGraphicExposeQueueEntry();
	}
}

static void redisplayGE(TextWidget w, XtPointer client_data, XEvent *event, Boolean *continue_to_dispatch_return) {

	(void)client_data;
	(void)continue_to_dispatch_return;

	if (event->type == GraphicsExpose || event->type == NoExpose) {
		HandleAllPendingGraphicsExposeNoExposeEvents(w, event);
	}
}

void HandleAllPendingGraphicsExposeNoExposeEvents(TextWidget w, XEvent *event) {
	XEvent foundEvent;
	int left;
	int top;
	int width;
	int height;
	bool invalidRect = true;

	if (event) {
		adjustRectForGraphicsExposeOrNoExposeEvent(w, event, &invalidRect, &left, &top, &width, &height);
	}
	while (XCheckIfEvent(XtDisplay(w), &foundEvent, findGraphicsExposeOrNoExposeEvent, (XPointer)w)) {
		adjustRectForGraphicsExposeOrNoExposeEvent(w, &foundEvent, &invalidRect, &left, &top, &width, &height);
	}
	if (!invalidRect) {
		w->text.textD->TextDRedisplayRect(left, top, width, height);
	}
}

/*
** Widget setValues method
*/
static Boolean setValues(TextWidget current, TextWidget request, TextWidget new_widget) {

	(void)request;

	bool redraw = false;
	bool reconfigure = false;

	if (new_widget->text.overstrike != current->text.overstrike) {
		if (current->text.textD->cursorStyle == BLOCK_CURSOR)
			current->text.textD->TextDSetCursorStyle(current->text.heavyCursor ? HEAVY_CURSOR : NORMAL_CURSOR);
		else if (current->text.textD->cursorStyle == NORMAL_CURSOR || current->text.textD->cursorStyle == HEAVY_CURSOR)
			current->text.textD->TextDSetCursorStyle(BLOCK_CURSOR);
	}

	if (new_widget->text.fontStruct != current->text.fontStruct) {
		if (new_widget->text.lineNumCols != 0)
			reconfigure = true;
		current->text.textD->TextDSetFont(new_widget->text.fontStruct);
	}

	if (new_widget->text.wrapMargin != current->text.wrapMargin || new_widget->text.continuousWrap != current->text.continuousWrap)
		current->text.textD->TextDSetWrapMode(new_widget->text.continuousWrap, new_widget->text.wrapMargin);

	/* When delimiters are changed, copy the memory, so that the caller
	   doesn't have to manage it, and add mandatory delimiters blank,
	   tab, and newline to the list */
	if (new_widget->text.delimiters != current->text.delimiters) {

		char *delimiters = XtMalloc(strlen(new_widget->text.delimiters) + 4);
		XtFree(current->text.delimiters);
		sprintf(delimiters, "%s%s", " \t\n", new_widget->text.delimiters);
		new_widget->text.delimiters = delimiters;
	}

	/* Setting the lineNumCols resource tells the text widget to hide or
	   show, or change the number of columns of the line number display,
	   which requires re-organizing the x coordinates of both the line
	   number display and the main text display */
	if (new_widget->text.lineNumCols != current->text.lineNumCols || reconfigure) {
		int marginWidth = new_widget->text.marginWidth;
		int charWidth = new_widget->text.fontStruct->max_bounds.width;
		int lineNumCols = new_widget->text.lineNumCols;
		if (lineNumCols == 0) {
			new_widget->text.textD->TextDSetLineNumberArea(0, 0, marginWidth);
			new_widget->text.columns = (new_widget->core.width - marginWidth * 2) / charWidth;
		} else {
			new_widget->text.textD->TextDSetLineNumberArea(marginWidth, charWidth * lineNumCols, 2 * marginWidth + charWidth * lineNumCols);
			new_widget->text.columns = (new_widget->core.width - marginWidth * 3 - charWidth * lineNumCols) / charWidth;
		}
	}

	if (new_widget->text.backlightCharTypes != current->text.backlightCharTypes) {
		TextDisplay::TextDSetupBGClasses((Widget)new_widget, new_widget->text.backlightCharTypes, &new_widget->text.textD->bgClassPixel, &new_widget->text.textD->bgClass, new_widget->text.textD->bgPixel);
		redraw = true;
	}

	return redraw;
}

/*
** Widget realize method
*/
static void realize(Widget w, XtValueMask *valueMask, XSetWindowAttributes *attributes) {
	/* Set bit gravity window attribute.  This saves a full blank and redraw
	   on window resizing */
	*valueMask |= CWBitGravity;
	attributes->bit_gravity = NorthWestGravity;

	/* Continue with realize method from superclass */
	(xmPrimitiveClassRec.core_class.realize)(w, valueMask, attributes);
}

/*
** Widget query geometry method ... unless asked to negotiate a different size simply return current size.
*/
static XtGeometryResult queryGeometry(Widget w, XtWidgetGeometry *proposed, XtWidgetGeometry *answer) {
	auto tw = reinterpret_cast<TextWidget>(w);

	int curHeight = tw->core.height;
	int curWidth = tw->core.width;
	XFontStruct *fs = tw->text.textD->fontStruct;
	int fontWidth = fs->max_bounds.width;
	int fontHeight = fs->ascent + fs->descent;
	int marginHeight = tw->text.marginHeight;
	int propWidth = (proposed->request_mode & CWWidth) ? proposed->width : 0;
	int propHeight = (proposed->request_mode & CWHeight) ? proposed->height : 0;

	answer->request_mode = CWHeight | CWWidth;

	if (proposed->request_mode & CWWidth)
		/* Accept a width no smaller than 10 chars */
		answer->width = std::max<int>(fontWidth * 10, proposed->width);
	else
		answer->width = curWidth;

	if (proposed->request_mode & CWHeight)
		/* Accept a height no smaller than an exact multiple of the line height
		   and at least one line high */
		answer->height = std::max<int>(1, ((propHeight - 2 * marginHeight) / fontHeight)) * fontHeight + 2 * marginHeight;
	else
		answer->height = curHeight;

	/*printf("propWidth %d, propHeight %d, ansWidth %d, ansHeight %d\n",
	        propWidth, propHeight, answer->width, answer->height);*/
	if (propWidth == answer->width && propHeight == answer->height)
		return XtGeometryYes;
	else if (answer->width == curWidth && answer->height == curHeight)
		return XtGeometryNo;
	else
		return XtGeometryAlmost;
}

/*
** Set the text buffer which this widget will display and interact with.
** The currently attached buffer is automatically freed, ONLY if it has
** no additional modify procs attached (as it would if it were being
** displayed by another text widget).
*/
void TextSetBuffer(Widget w, TextBuffer *buffer) {
	TextBuffer *oldBuf = reinterpret_cast<TextWidget>(w)->text.textD->buffer;

	StopHandlingXSelections(w);
	reinterpret_cast<TextWidget>(w)->text.textD->TextDSetBuffer(buffer);
	if (oldBuf->modifyProcs_.empty())
		delete oldBuf;
}

/*
** Get the buffer associated with this text widget.  Note that attaching
** additional modify callbacks to the buffer will prevent it from being
** automatically freed when the widget is destroyed.
*/
TextBuffer *TextGetBuffer(Widget w) {
	return reinterpret_cast<TextWidget>(w)->text.textD->buffer;
}

/*
** Translate a line number and column into a position
*/
int TextLineAndColToPos(Widget w, int lineNum, int column) {
	return reinterpret_cast<TextWidget>(w)->text.textD->TextDLineAndColToPos(lineNum, column);
}

/*
** Translate a position into a line number (if the position is visible,
** if it's not, return False
*/
int TextPosToLineAndCol(Widget w, int pos, int *lineNum, int *column) {
	return reinterpret_cast<TextWidget>(w)->text.textD->TextDPosToLineAndCol(pos, lineNum, column);
}

/*
** Translate a buffer text position to the XY location where the center
** of the cursor would be positioned to point to that character.  Returns
** False if the position is not displayed because it is VERTICALLY out
** of view.  If the position is horizontally out of view, returns the
** x coordinate where the position would be if it were visible.
*/
int TextPosToXY(Widget w, int pos, int *x, int *y) {
	return reinterpret_cast<TextWidget>(w)->text.textD->TextDPositionToXY(pos, x, y);
}

/*
** Return the cursor position
*/
int TextGetCursorPos(Widget w) {
	return reinterpret_cast<TextWidget>(w)->text.textD->TextDGetInsertPosition();
}

/*
** Set the cursor position
*/
void TextSetCursorPos(Widget w, int pos) {
	reinterpret_cast<TextWidget>(w)->text.textD->TextDSetInsertPosition(pos);
	checkAutoShowInsertPos(w);
	callCursorMovementCBs(w, nullptr);
}

/*
** Return the horizontal and vertical scroll positions of the widget
*/
void TextGetScroll(Widget w, int *topLineNum, int *horizOffset) {
	reinterpret_cast<TextWidget>(w)->text.textD->TextDGetScroll(topLineNum, horizOffset);
}

/*
** Set the horizontal and vertical scroll positions of the widget
*/
void TextSetScroll(Widget w, int topLineNum, int horizOffset) {
	reinterpret_cast<TextWidget>(w)->text.textD->TextDSetScroll(topLineNum, horizOffset);
}

int TextGetMinFontWidth(Widget w, Boolean considerStyles) {
	return reinterpret_cast<TextWidget>(w)->text.textD->TextDMinFontWidth(considerStyles);
}

int TextGetMaxFontWidth(Widget w, Boolean considerStyles) {
	return reinterpret_cast<TextWidget>(w)->text.textD->TextDMaxFontWidth(considerStyles);
}

/*
** Set this widget to be the owner of selections made in it's attached
** buffer (text buffers may be shared among several text widgets).
*/
void TextHandleXSelections(Widget w) {
	HandleXSelections(w);
}

void TextPasteClipboard(Widget w, Time time) {
	cancelDrag(w);
	if (checkReadOnly(w))
		return;
	TakeMotifDestination(w, time);
	InsertClipboard(w, False);
	callCursorMovementCBs(w, nullptr);
}

void TextColPasteClipboard(Widget w, Time time) {
	cancelDrag(w);
	if (checkReadOnly(w))
		return;
	TakeMotifDestination(w, time);
	InsertClipboard(w, True);
	callCursorMovementCBs(w, nullptr);
}

void TextCopyClipboard(Widget w, Time time) {
	cancelDrag(w);
	if (!reinterpret_cast<TextWidget>(w)->text.textD->buffer->primary_.selected) {
		XBell(XtDisplay(w), 0);
		return;
	}
	CopyToClipboard(w, time);
}

void TextCutClipboard(Widget w, Time time) {
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;

	cancelDrag(w);
	if (checkReadOnly(w))
		return;
	if (!textD->buffer->primary_.selected) {
		XBell(XtDisplay(w), 0);
		return;
	}
	TakeMotifDestination(w, time);
	CopyToClipboard(w, time);
	textD->buffer->BufRemoveSelected();
	textD->TextDSetInsertPosition(textD->buffer->cursorPosHint_);
	checkAutoShowInsertPos(w);
}

int TextFirstVisibleLine(Widget w) {
	return (reinterpret_cast<TextWidget>(w)->text.textD->topLineNum);
}

int TextNumVisibleLines(Widget w) {
	return (reinterpret_cast<TextWidget>(w)->text.textD->nVisibleLines);
}

int TextVisibleWidth(Widget w) {
	return (reinterpret_cast<TextWidget>(w)->text.textD->width);
}

int TextFirstVisiblePos(Widget w) {
	return reinterpret_cast<TextWidget>(w)->text.textD->firstChar;
}

int TextLastVisiblePos(Widget w) {
	return reinterpret_cast<TextWidget>(w)->text.textD->lastChar;
}

/*
** Insert text "chars" at the cursor position, respecting pending delete
** selections, overstrike, and handling cursor repositioning as if the text
** had been typed.  If autoWrap is on wraps the text to fit within the wrap
** margin, auto-indenting where the line was wrapped (but nowhere else).
** "allowPendingDelete" controls whether primary selections in the widget are
** treated as pending delete selections (True), or ignored (False). "event"
** is optional and is just passed on to the cursor movement callbacks.
*/
void TextInsertAtCursorEx(Widget w, view::string_view chars, XEvent *event, int allowPendingDelete, int allowWrap) {
	int wrapMargin, colNum, lineStartPos, cursorPos;
	TextWidget tw   = reinterpret_cast<TextWidget>(w);
	TextDisplay *textD = tw->text.textD;
	TextBuffer *buf = textD->buffer;
	int fontWidth   = textD->fontStruct->max_bounds.width;
	int replaceSel, singleLine, breakAt = 0;

	/* Don't wrap if auto-wrap is off or suppressed, or it's just a newline */
	if (!allowWrap || !tw->text.autoWrap || (chars[0] == '\n' && chars[1] == '\0')) {
		simpleInsertAtCursorEx(w, chars, event, allowPendingDelete);
		return;
	}

	/* If this is going to be a pending delete operation, the real insert
	   position is the start of the selection.  This will make rectangular
	   selections wrap strangely, but this routine should rarely be used for
	   them, and even more rarely when they need to be wrapped. */
	replaceSel = allowPendingDelete && pendingSelection(w);
	cursorPos = replaceSel ? buf->primary_.start : textD->TextDGetInsertPosition();

	/* If the text is only one line and doesn't need to be wrapped, just insert
	   it and be done (for efficiency only, this routine is called for each
	   character typed). (Of course, it may not be significantly more efficient
	   than the more general code below it, so it may be a waste of time!) */
	wrapMargin = tw->text.wrapMargin != 0 ? tw->text.wrapMargin : textD->width / fontWidth;
	lineStartPos = buf->BufStartOfLine(cursorPos);
	colNum = buf->BufCountDispChars(lineStartPos, cursorPos);
	
	auto it = chars.begin();
	for (; it != chars.end() && *it != '\n'; it++) {
		colNum += TextBuffer::BufCharWidth(*it, colNum, buf->tabDist_, buf->nullSubsChar_);
	}
	
	singleLine = it == chars.end();
	if (colNum < wrapMargin && singleLine) {
		simpleInsertAtCursorEx(w, chars, event, True);
		return;
	}

	/* Wrap the text */
	std::string lineStartText = buf->BufGetRangeEx(lineStartPos, cursorPos);
	std::string wrappedText = wrapTextEx(tw, lineStartText, chars, lineStartPos, wrapMargin, replaceSel ? nullptr : &breakAt);

	/* Insert the text.  Where possible, use TextDInsert which is optimized
	   for less redraw. */
	if (replaceSel) {
		buf->BufReplaceSelectedEx(wrappedText);
		textD->TextDSetInsertPosition(buf->cursorPosHint_);
	} else if (tw->text.overstrike) {
		if (breakAt == 0 && singleLine)
			textD->TextDOverstrikeEx(wrappedText);
		else {
			buf->BufReplaceEx(cursorPos - breakAt, cursorPos, wrappedText);
			textD->TextDSetInsertPosition(buf->cursorPosHint_);
		}
	} else {
		if (breakAt == 0) {
			textD->TextDInsertEx(wrappedText);
		} else {
			buf->BufReplaceEx(cursorPos - breakAt, cursorPos, wrappedText);
			textD->TextDSetInsertPosition(buf->cursorPosHint_);
		}
	}
	checkAutoShowInsertPos(w);
	callCursorMovementCBs(w, event);
}

/*
** Fetch text from the widget's buffer, adding wrapping newlines to emulate
** effect acheived by wrapping in the text display in continuous wrap mode.
*/
std::string TextGetWrappedEx(Widget w, int startPos, int endPos) {
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	TextBuffer *buf = textD->buffer;

	if (!reinterpret_cast<TextWidget>(w)->text.continuousWrap || startPos == endPos) {
		return buf->BufGetRangeEx(startPos, endPos);
	}

	/* Create a text buffer with a good estimate of the size that adding
	   newlines will expand it to.  Since it's a text buffer, if we guess
	   wrong, it will fail softly, and simply expand the size */
	auto outBuf = new TextBuffer((endPos - startPos) + (endPos - startPos) / 5);
	int outPos = 0;

	/* Go (displayed) line by line through the buffer, adding newlines where
	   the text is wrapped at some character other than an existing newline */
	int fromPos = startPos;
	int toPos = textD->TextDCountForwardNLines(startPos, 1, False);
	while (toPos < endPos) {
		outBuf->BufCopyFromBuf(buf, fromPos, toPos, outPos);
		outPos += toPos - fromPos;
		char c = outBuf->BufGetCharacter(outPos - 1);
		if (c == ' ' || c == '\t')
			outBuf->BufReplaceEx(outPos - 1, outPos, "\n");
		else if (c != '\n') {
			outBuf->BufInsertEx(outPos, "\n");
			outPos++;
		}
		fromPos = toPos;
		toPos = textD->TextDCountForwardNLines(fromPos, 1, True);
	}
	outBuf->BufCopyFromBuf(buf, fromPos, endPos, outPos);

	/* return the contents of the output buffer as a string */
	std::string outString = outBuf->BufGetAllEx();
	delete outBuf;
	return outString;
}

/*
** Return the (statically allocated) action table for menu item actions.
**
** Warning: This routine can only be used before the first text widget is
** created!  After that, apparently, Xt takes over the table and overwrites
** it with its own version.  XtGetActionList is preferable, but is not
** available before X11R5.
*/
XtActionsRec *TextGetActions(int *nActions) {
	*nActions = XtNumber(actionsList);
	return actionsList;
}

static void grabFocusAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	XButtonEvent *e = &event->xbutton;
	auto tw = reinterpret_cast<TextWidget>(w);
	TextDisplay *textD = tw->text.textD;
	Time lastBtnDown = tw->text.lastBtnDown;
	int row, column;

	/* Indicate state for future events, PRIMARY_CLICKED indicates that
	   the proper initialization has been done for primary dragging and/or
	   multi-clicking.  Also record the timestamp for multi-click processing */
	tw->text.dragState = PRIMARY_CLICKED;
	tw->text.lastBtnDown = e->time;

	/* Become owner of the MOTIF_DESTINATION selection, making this widget
	   the designated recipient of secondary quick actions in Motif XmText
	   widgets and in other NEdit text widgets */
	TakeMotifDestination(w, e->time);

	/* Check for possible multi-click sequence in progress */
	if (tw->text.multiClickState != NO_CLICKS) {
		if (e->time < lastBtnDown + XtGetMultiClickTime(XtDisplay(w))) {
			if (tw->text.multiClickState == ONE_CLICK) {
				selectWord(w, e->x);
				callCursorMovementCBs(w, event);
				return;
			} else if (tw->text.multiClickState == TWO_CLICKS) {
				selectLine(w);
				callCursorMovementCBs(w, event);
				return;
			} else if (tw->text.multiClickState == THREE_CLICKS) {
				textD->buffer->BufSelect(0, textD->buffer->BufGetLength());
				return;
			} else if (tw->text.multiClickState > THREE_CLICKS)
				tw->text.multiClickState = NO_CLICKS;
		} else
			tw->text.multiClickState = NO_CLICKS;
	}

	/* Clear any existing selections */
	textD->buffer->BufUnselect();

	/* Move the cursor to the pointer location */
	moveDestinationAP(w, event, args, nArgs);

	/* Record the site of the initial button press and the initial character
	   position so subsequent motion events and clicking can decide when and
	   where to begin a primary selection */
	tw->text.btnDownCoord = Point{e->x, e->y};
	tw->text.anchor = textD->TextDGetInsertPosition();
	textD->TextDXYToUnconstrainedPosition(Point{e->x, e->y}, &row, &column);
	column = textD->TextDOffsetWrappedColumn(row, column);
	tw->text.rectAnchor = column;
}

static void moveDestinationAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;
	(void)event;

	XButtonEvent *e = &event->xbutton;
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;

	/* Get input focus */
	XmProcessTraversal(w, XmTRAVERSE_CURRENT);

	/* Move the cursor */
	textD->TextDSetInsertPosition(textD->TextDXYToPosition(Point{e->x, e->y}));
	checkAutoShowInsertPos(w);
	callCursorMovementCBs(w, event);
}

static void extendAdjustAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;
	(void)event;

	auto tw = reinterpret_cast<TextWidget>(w);
	XMotionEvent *e = &event->xmotion;
	int dragState = tw->text.dragState;
	int rectDrag = hasKey("rect", args, nArgs);

	/* Make sure the proper initialization was done on mouse down */
	if (dragState != PRIMARY_DRAG && dragState != PRIMARY_CLICKED && dragState != PRIMARY_RECT_DRAG)
		return;

	/* If the selection hasn't begun, decide whether the mouse has moved
	   far enough from the initial mouse down to be considered a drag */
	if (tw->text.dragState == PRIMARY_CLICKED) {
		if (abs(e->x - tw->text.btnDownCoord.x) > SELECT_THRESHOLD || abs(e->y - tw->text.btnDownCoord.y) > SELECT_THRESHOLD)
			tw->text.dragState = rectDrag ? PRIMARY_RECT_DRAG : PRIMARY_DRAG;
		else
			return;
	}

	/* If "rect" argument has appeared or disappeared, keep dragState up
	   to date about which type of drag this is */
	tw->text.dragState = rectDrag ? PRIMARY_RECT_DRAG : PRIMARY_DRAG;

	/* Record the new position for the autoscrolling timer routine, and
	   engage or disengage the timer if the mouse is in/out of the window */
	checkAutoScroll(tw, e->x, e->y);

	/* Adjust the selection and move the cursor */
	adjustSelection(tw, e->x, e->y);
}

static void extendStartAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	XMotionEvent *e = &event->xmotion;
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	TextBuffer *buf = textD->buffer;
	TextSelection *sel = &buf->primary_;
	int anchor, rectAnchor, anchorLineStart, newPos, row, column;

	/* Find the new anchor point for the rest of this drag operation */
	newPos = textD->TextDXYToPosition(Point{e->x, e->y});
	textD->TextDXYToUnconstrainedPosition(Point{e->x, e->y}, &row, &column);
	column = textD->TextDOffsetWrappedColumn(row, column);
	if (sel->selected) {
		if (sel->rectangular) {
			rectAnchor = column < (sel->rectEnd + sel->rectStart) / 2 ? sel->rectEnd : sel->rectStart;
			anchorLineStart = buf->BufStartOfLine(newPos < (sel->end + sel->start) / 2 ? sel->end : sel->start);
			anchor = buf->BufCountForwardDispChars(anchorLineStart, rectAnchor);
		} else {
			if (abs(newPos - sel->start) < abs(newPos - sel->end))
				anchor = sel->end;
			else
				anchor = sel->start;
			anchorLineStart = buf->BufStartOfLine(anchor);
			rectAnchor = buf->BufCountDispChars(anchorLineStart, anchor);
		}
	} else {
		anchor = textD->TextDGetInsertPosition();
		anchorLineStart = buf->BufStartOfLine(anchor);
		rectAnchor = buf->BufCountDispChars(anchorLineStart, anchor);
	}
	reinterpret_cast<TextWidget>(w)->text.anchor = anchor;
	reinterpret_cast<TextWidget>(w)->text.rectAnchor = rectAnchor;

	/* Make the new selection */
	if (hasKey("rect", args, nArgs))
		buf->BufRectSelect(buf->BufStartOfLine(std::min<int>(anchor, newPos)), buf->BufEndOfLine(std::max<int>(anchor, newPos)), std::min<int>(rectAnchor, column), std::max<int>(rectAnchor, column));
	else
		buf->BufSelect(std::min<int>(anchor, newPos), std::max<int>(anchor, newPos));

	/* Never mind the motion threshold, go right to dragging since
	   extend-start is unambiguously the start of a selection */
	reinterpret_cast<TextWidget>(w)->text.dragState = PRIMARY_DRAG;

	/* Don't do by-word or by-line adjustment, just by character */
	reinterpret_cast<TextWidget>(w)->text.multiClickState = NO_CLICKS;

	/* Move the cursor */
	textD->TextDSetInsertPosition(newPos);
	callCursorMovementCBs(w, event);
}

static void extendEndAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;
	(void)event;

	XButtonEvent *e = &event->xbutton;
	auto tw = reinterpret_cast<TextWidget>(w);

	if (tw->text.dragState == PRIMARY_CLICKED && tw->text.lastBtnDown <= e->time + XtGetMultiClickTime(XtDisplay(w)))
		tw->text.multiClickState++;
	endDrag(w);
}

static void processCancelAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;
	(void)event;

	int dragState = reinterpret_cast<TextWidget>(w)->text.dragState;
	TextBuffer *buf = reinterpret_cast<TextWidget>(w)->text.textD->buffer;
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;

	/* If there's a calltip displayed, kill it. */
	TextDKillCalltip(textD, 0);

	if (dragState == PRIMARY_DRAG || dragState == PRIMARY_RECT_DRAG)
		buf->BufUnselect();
	cancelDrag(w);
}

static void secondaryStartAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	XMotionEvent *e = &event->xmotion;
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	TextBuffer *buf = textD->buffer;
	TextSelection *sel = &buf->secondary_;
	int anchor, pos, row, column;

	/* Find the new anchor point and make the new selection */
	pos = textD->TextDXYToPosition(Point{e->x, e->y});
	if (sel->selected) {
		if (abs(pos - sel->start) < abs(pos - sel->end))
			anchor = sel->end;
		else
			anchor = sel->start;
		buf->BufSecondarySelect(anchor, pos);
	} else
		anchor = pos;

	/* Record the site of the initial button press and the initial character
	   position so subsequent motion events can decide when to begin a
	   selection, (and where the selection began) */
	reinterpret_cast<TextWidget>(w)->text.btnDownCoord = Point{e->x, e->y};
	reinterpret_cast<TextWidget>(w)->text.anchor       = pos;
	
	textD->TextDXYToUnconstrainedPosition(Point{e->x, e->y}, &row, &column);
	
	column = textD->TextDOffsetWrappedColumn(row, column);
	reinterpret_cast<TextWidget>(w)->text.rectAnchor = column;
	reinterpret_cast<TextWidget>(w)->text.dragState = SECONDARY_CLICKED;
}

static void secondaryOrDragStartAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	XMotionEvent *e = &event->xmotion;
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	TextBuffer *buf = textD->buffer;

	/* If the click was outside of the primary selection, this is not
	   a drag, start a secondary selection */
	if (!buf->primary_.selected || !textD->TextDInSelection(Point{e->x, e->y})) {
		secondaryStartAP(w, event, args, nArgs);
		return;
	}

	if (checkReadOnly(w))
		return;

	/* Record the site of the initial button press and the initial character
	   position so subsequent motion events can decide when to begin a
	   drag, and where to drag to */
	reinterpret_cast<TextWidget>(w)->text.btnDownCoord = Point{e->x, e->y};
	reinterpret_cast<TextWidget>(w)->text.dragState    = CLICKED_IN_SELECTION;
}

static void secondaryAdjustAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	auto tw = reinterpret_cast<TextWidget>(w);
	XMotionEvent *e = &event->xmotion;
	int dragState = tw->text.dragState;
	int rectDrag = hasKey("rect", args, nArgs);

	/* Make sure the proper initialization was done on mouse down */
	if (dragState != SECONDARY_DRAG && dragState != SECONDARY_RECT_DRAG && dragState != SECONDARY_CLICKED)
		return;

	/* If the selection hasn't begun, decide whether the mouse has moved
	   far enough from the initial mouse down to be considered a drag */
	if (tw->text.dragState == SECONDARY_CLICKED) {
		if (abs(e->x - tw->text.btnDownCoord.x) > SELECT_THRESHOLD || abs(e->y - tw->text.btnDownCoord.y) > SELECT_THRESHOLD)
			tw->text.dragState = rectDrag ? SECONDARY_RECT_DRAG : SECONDARY_DRAG;
		else
			return;
	}

	/* If "rect" argument has appeared or disappeared, keep dragState up
	   to date about which type of drag this is */
	tw->text.dragState = rectDrag ? SECONDARY_RECT_DRAG : SECONDARY_DRAG;

	/* Record the new position for the autoscrolling timer routine, and
	   engage or disengage the timer if the mouse is in/out of the window */
	checkAutoScroll(tw, e->x, e->y);

	/* Adjust the selection */
	adjustSecondarySelection(tw, e->x, e->y);
}

static void secondaryOrDragAdjustAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	auto tw = reinterpret_cast<TextWidget>(w);
	XMotionEvent *e = &event->xmotion;
	int dragState = tw->text.dragState;

	/* Only dragging of blocks of text is handled in this action proc.
	   Otherwise, defer to secondaryAdjust to handle the rest */
	if (dragState != CLICKED_IN_SELECTION && dragState != PRIMARY_BLOCK_DRAG) {
		secondaryAdjustAP(w, event, args, nArgs);
		return;
	}

	/* Decide whether the mouse has moved far enough from the
	   initial mouse down to be considered a drag */
	if (tw->text.dragState == CLICKED_IN_SELECTION) {
		if (abs(e->x - tw->text.btnDownCoord.x) > SELECT_THRESHOLD || abs(e->y - tw->text.btnDownCoord.y) > SELECT_THRESHOLD)
			BeginBlockDrag(tw);
		else
			return;
	}

	/* Record the new position for the autoscrolling timer routine, and
	   engage or disengage the timer if the mouse is in/out of the window */
	checkAutoScroll(tw, e->x, e->y);

	/* Adjust the selection */
	BlockDragSelection(tw, Point{e->x, e->y}, hasKey("overlay", args, nArgs) ? (hasKey("copy", args, nArgs) ? DRAG_OVERLAY_COPY : DRAG_OVERLAY_MOVE) : (hasKey("copy", args, nArgs) ? DRAG_COPY : DRAG_MOVE));
}

static void copyToAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	XButtonEvent *e = &event->xbutton;
	auto tw = reinterpret_cast<TextWidget>(w);
	TextDisplay *textD = tw->text.textD;
	int dragState = tw->text.dragState;
	TextBuffer *buf = textD->buffer;
	TextSelection *secondary = &buf->secondary_, *primary = &buf->primary_;
	int rectangular = secondary->rectangular;
	int insertPos, lineStart, column;

	endDrag(w);
	if (!((dragState == SECONDARY_DRAG && secondary->selected) || (dragState == SECONDARY_RECT_DRAG && secondary->selected) || dragState == SECONDARY_CLICKED || dragState == NOT_CLICKED))
		return;
	if (!(secondary->selected && !reinterpret_cast<TextWidget>(w)->text.motifDestOwner)) {
		if (checkReadOnly(w)) {
			buf->BufSecondaryUnselect();
			return;
		}
	}
	if (secondary->selected) {
		if (tw->text.motifDestOwner) {
			textD->TextDBlankCursor();
			std::string textToCopy = buf->BufGetSecSelectTextEx();
			if (primary->selected && rectangular) {
				insertPos = textD->TextDGetInsertPosition();
				buf->BufReplaceSelectedEx(textToCopy);
				textD->TextDSetInsertPosition(buf->cursorPosHint_);
			} else if (rectangular) {
				insertPos = textD->TextDGetInsertPosition();
				lineStart = buf->BufStartOfLine(insertPos);
				column = buf->BufCountDispChars(lineStart, insertPos);
				buf->BufInsertColEx(column, lineStart, textToCopy, nullptr, nullptr);
				textD->TextDSetInsertPosition(buf->cursorPosHint_);
			} else
				TextInsertAtCursorEx(w, textToCopy, event, True, tw->text.autoWrapPastedText);

			buf->BufSecondaryUnselect();
			textD->TextDUnblankCursor();
		} else
			SendSecondarySelection(w, e->time, False);
	} else if (primary->selected) {
		std::string textToCopy = buf->BufGetSelectionTextEx();
		textD->TextDSetInsertPosition(textD->TextDXYToPosition(Point{e->x, e->y}));
		TextInsertAtCursorEx(w, textToCopy, event, False, tw->text.autoWrapPastedText);
	} else {
		textD->TextDSetInsertPosition(textD->TextDXYToPosition(Point{e->x, e->y}));
		InsertPrimarySelection(w, e->time, False);
	}
}

static void copyToOrEndDragAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	int dragState = reinterpret_cast<TextWidget>(w)->text.dragState;

	if (dragState != PRIMARY_BLOCK_DRAG) {
		copyToAP(w, event, args, nArgs);
		return;
	}

	FinishBlockDrag(reinterpret_cast<TextWidget>(w));
}

static void moveToAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	XButtonEvent *e = &event->xbutton;
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	int dragState = reinterpret_cast<TextWidget>(w)->text.dragState;
	TextBuffer *buf = textD->buffer;
	TextSelection *secondary = &buf->secondary_, *primary = &buf->primary_;
	int insertPos, rectangular = secondary->rectangular;
	int column, lineStart;

	endDrag(w);
	if (!((dragState == SECONDARY_DRAG && secondary->selected) || (dragState == SECONDARY_RECT_DRAG && secondary->selected) || dragState == SECONDARY_CLICKED || dragState == NOT_CLICKED))
		return;
	if (checkReadOnly(w)) {
		buf->BufSecondaryUnselect();
		return;
	}

	if (secondary->selected) {
		if (reinterpret_cast<TextWidget>(w)->text.motifDestOwner) {
			std::string textToCopy = buf->BufGetSecSelectTextEx();
			if (primary->selected && rectangular) {
				insertPos = textD->TextDGetInsertPosition();
				buf->BufReplaceSelectedEx(textToCopy);
				textD->TextDSetInsertPosition(buf->cursorPosHint_);
			} else if (rectangular) {
				insertPos = textD->TextDGetInsertPosition();
				lineStart = buf->BufStartOfLine(insertPos);
				column = buf->BufCountDispChars(lineStart, insertPos);
				buf->BufInsertColEx(column, lineStart, textToCopy, nullptr, nullptr);
				textD->TextDSetInsertPosition(buf->cursorPosHint_);
			} else
				TextInsertAtCursorEx(w, textToCopy, event, True, reinterpret_cast<TextWidget>(w)->text.autoWrapPastedText);

			buf->BufRemoveSecSelect();
			buf->BufSecondaryUnselect();
		} else
			SendSecondarySelection(w, e->time, True);
	} else if (primary->selected) {
		std::string textToCopy = buf->BufGetRangeEx(primary->start, primary->end);
		textD->TextDSetInsertPosition(textD->TextDXYToPosition(Point{e->x, e->y}));
		TextInsertAtCursorEx(w, textToCopy, event, False, reinterpret_cast<TextWidget>(w)->text.autoWrapPastedText);

		buf->BufRemoveSelected();
		buf->BufUnselect();
	} else {
		textD->TextDSetInsertPosition(textD->TextDXYToPosition(Point{e->x, e->y}));
		MovePrimarySelection(w, e->time, False);
	}
}

static void moveToOrEndDragAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	int dragState = reinterpret_cast<TextWidget>(w)->text.dragState;

	if (dragState != PRIMARY_BLOCK_DRAG) {
		moveToAP(w, event, args, nArgs);
		return;
	}

	FinishBlockDrag(reinterpret_cast<TextWidget>(w));
}

static void endDragAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;
	(void)event;

	if (reinterpret_cast<TextWidget>(w)->text.dragState == PRIMARY_BLOCK_DRAG)
		FinishBlockDrag(reinterpret_cast<TextWidget>(w));
	else
		endDrag(w);
}

static void exchangeAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	XButtonEvent *e = &event->xbutton;
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	TextBuffer *buf = textD->buffer;
	TextSelection *sec = &buf->secondary_, *primary = &buf->primary_;

	int newPrimaryStart, newPrimaryEnd, secWasRect;
	int dragState = reinterpret_cast<TextWidget>(w)->text.dragState; /* save before endDrag */
	int silent = hasKey("nobell", args, nArgs);

	endDrag(w);
	if (checkReadOnly(w))
		return;

	/* If there's no secondary selection here, or the primary and secondary
	   selection overlap, just beep and return */
	if (!sec->selected || (primary->selected && ((primary->start <= sec->start && primary->end > sec->start) || (sec->start <= primary->start && sec->end > primary->start)))) {
		buf->BufSecondaryUnselect();
		ringIfNecessary(silent, w);
		/* If there's no secondary selection, but the primary selection is
		   being dragged, we must not forget to finish the dragging.
		   Otherwise, modifications aren't recorded. */
		if (dragState == PRIMARY_BLOCK_DRAG)
			FinishBlockDrag(reinterpret_cast<TextWidget>(w));
		return;
	}

	/* if the primary selection is in another widget, use selection routines */
	if (!primary->selected) {
		ExchangeSelections(w, e->time);
		return;
	}

	/* Both primary and secondary are in this widget, do the exchange here */
	std::string primaryText = buf->BufGetSelectionTextEx();
	std::string secText = buf->BufGetSecSelectTextEx();
	secWasRect = sec->rectangular;
	buf->BufReplaceSecSelectEx(primaryText);
	newPrimaryStart = primary->start;
	buf->BufReplaceSelectedEx(secText);
	newPrimaryEnd = newPrimaryStart + secText.size();

	buf->BufSecondaryUnselect();
	if (secWasRect) {
		textD->TextDSetInsertPosition(buf->cursorPosHint_);
	} else {
		buf->BufSelect(newPrimaryStart, newPrimaryEnd);
		textD->TextDSetInsertPosition(newPrimaryEnd);
	}
	checkAutoShowInsertPos(w);
}

static void copyPrimaryAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	XKeyEvent *e = &event->xkey;
	auto tw = reinterpret_cast<TextWidget>(w);
	TextDisplay *textD = tw->text.textD;
	TextBuffer *buf = textD->buffer;
	TextSelection *primary = &buf->primary_;
	int rectangular = hasKey("rect", args, nArgs);
	int insertPos, col;

	cancelDrag(w);
	if (checkReadOnly(w))
		return;
	if (primary->selected && rectangular) {
		std::string textToCopy = buf->BufGetSelectionTextEx();
		insertPos = textD->TextDGetInsertPosition();
		col = buf->BufCountDispChars(buf->BufStartOfLine(insertPos), insertPos);
		buf->BufInsertColEx(col, insertPos, textToCopy, nullptr, nullptr);
		textD->TextDSetInsertPosition(buf->cursorPosHint_);

		checkAutoShowInsertPos(w);
	} else if (primary->selected) {
		std::string textToCopy = buf->BufGetSelectionTextEx();
		insertPos = textD->TextDGetInsertPosition();
		buf->BufInsertEx(insertPos, textToCopy);
		textD->TextDSetInsertPosition(insertPos + textToCopy.size());

		checkAutoShowInsertPos(w);
	} else if (rectangular) {
		if (!textD->TextDPositionToXY(textD->TextDGetInsertPosition(), &tw->text.btnDownCoord.x, &tw->text.btnDownCoord.y))
			return; /* shouldn't happen */
		InsertPrimarySelection(w, e->time, True);
	} else
		InsertPrimarySelection(w, e->time, False);
}

static void cutPrimaryAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	XKeyEvent *e = &event->xkey;
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	TextBuffer *buf = textD->buffer;
	TextSelection *primary = &buf->primary_;

	int rectangular = hasKey("rect", args, nArgs);
	int insertPos, col;

	cancelDrag(w);
	if (checkReadOnly(w))
		return;
	if (primary->selected && rectangular) {
		std::string textToCopy = buf->BufGetSelectionTextEx();
		insertPos = textD->TextDGetInsertPosition();
		col = buf->BufCountDispChars(buf->BufStartOfLine(insertPos), insertPos);
		buf->BufInsertColEx(col, insertPos, textToCopy, nullptr, nullptr);
		textD->TextDSetInsertPosition(buf->cursorPosHint_);

		buf->BufRemoveSelected();
		checkAutoShowInsertPos(w);
	} else if (primary->selected) {
		std::string textToCopy = buf->BufGetSelectionTextEx();
		insertPos = textD->TextDGetInsertPosition();
		buf->BufInsertEx(insertPos, textToCopy);
		textD->TextDSetInsertPosition(insertPos + textToCopy.size());
		
		buf->BufRemoveSelected();
		checkAutoShowInsertPos(w);
	} else if (rectangular) {
		if (!textD->TextDPositionToXY(textD->TextDGetInsertPosition(), &reinterpret_cast<TextWidget>(w)->text.btnDownCoord.x, &reinterpret_cast<TextWidget>(w)->text.btnDownCoord.y))
			return; /* shouldn't happen */
		MovePrimarySelection(w, e->time, True);
	} else {
		MovePrimarySelection(w, e->time, False);
	}
}

static void mousePanAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	XButtonEvent *e = &event->xbutton;
	auto tw = reinterpret_cast<TextWidget>(w);
	TextDisplay *textD = tw->text.textD;
	int lineHeight = textD->ascent + textD->descent;
	int topLineNum, horizOffset;
	static Cursor panCursor = 0;

	if (tw->text.dragState == MOUSE_PAN) {
		textD->TextDSetScroll((tw->text.btnDownCoord.y - e->y + lineHeight / 2) / lineHeight, tw->text.btnDownCoord.x - e->x);
	} else if (tw->text.dragState == NOT_CLICKED) {
		textD->TextDGetScroll(&topLineNum, &horizOffset);
		tw->text.btnDownCoord.x = e->x + horizOffset;
		tw->text.btnDownCoord.y = e->y + topLineNum * lineHeight;
		tw->text.dragState = MOUSE_PAN;
		if (!panCursor)
			panCursor = XCreateFontCursor(XtDisplay(w), XC_fleur);
		XGrabPointer(XtDisplay(w), XtWindow(w), False, ButtonMotionMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, panCursor, CurrentTime);
	} else
		cancelDrag(w);
}

static void pasteClipboardAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	if (hasKey("rect", args, nArgs))
		TextColPasteClipboard(w, event->xkey.time);
	else
		TextPasteClipboard(w, event->xkey.time);
}

static void copyClipboardAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	TextCopyClipboard(w, event->xkey.time);
}

static void cutClipboardAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	TextCutClipboard(w, event->xkey.time);
}

static void insertStringAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	smartIndentCBStruct smartIndent;
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;

	if (*nArgs == 0)
		return;
	cancelDrag(w);
	if (checkReadOnly(w))
		return;
	if (reinterpret_cast<TextWidget>(w)->text.smartIndent) {
		smartIndent.reason = CHAR_TYPED;
		smartIndent.pos = textD->TextDGetInsertPosition();
		smartIndent.indentRequest = 0;
		smartIndent.charsTyped = args[0];
		XtCallCallbacks(w, textNsmartIndentCallback, &smartIndent);
	}
	TextInsertAtCursorEx(w, args[0], event, True, True);
	reinterpret_cast<TextWidget>(w)->text.textD->buffer->BufUnselect();
}

static void selfInsertAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	Document *window = Document::WidgetToWindow(w);


	int status;
	XKeyEvent *e = &event->xkey;
	char chars[20];
	KeySym keysym;
	int nChars;
	smartIndentCBStruct smartIndent;
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;


	nChars = XmImMbLookupString(w, &event->xkey, chars, 19, &keysym, &status);
	if (nChars == 0 || status == XLookupNone || status == XLookupKeySym || status == XBufferOverflow)
		return;

	cancelDrag(w);
	if (checkReadOnly(w))
		return;
	TakeMotifDestination(w, e->time);
	chars[nChars] = '\0';

	if (!window->buffer_->BufSubstituteNullChars(chars, nChars)) {
		DialogF(DF_ERR, window->shell_, 1, "Error", "Too much binary data", "OK");
		return;
	}

	/* If smart indent is on, call the smart indent callback to check the
	   inserted character */
	if (reinterpret_cast<TextWidget>(w)->text.smartIndent) {
		smartIndent.reason        = CHAR_TYPED;
		smartIndent.pos           = textD->TextDGetInsertPosition();
		smartIndent.indentRequest = 0;
		smartIndent.charsTyped    = chars;
		XtCallCallbacks(w, textNsmartIndentCallback, &smartIndent);
	}
	TextInsertAtCursorEx(w, chars, event, True, True);
	textD->buffer->BufUnselect();
}

static void newlineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	if (reinterpret_cast<TextWidget>(w)->text.autoIndent || reinterpret_cast<TextWidget>(w)->text.smartIndent)
		newlineAndIndentAP(w, event, args, nArgs);
	else
		newlineNoIndentAP(w, event, args, nArgs);
}

static void newlineNoIndentAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	XKeyEvent *e = &event->xkey;

	cancelDrag(w);
	if (checkReadOnly(w))
		return;
	TakeMotifDestination(w, e->time);
	simpleInsertAtCursorEx(w, "\n", event, True);
	reinterpret_cast<TextWidget>(w)->text.textD->buffer->BufUnselect();
}

static void newlineAndIndentAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	XKeyEvent *e = &event->xkey;
	auto tw = reinterpret_cast<TextWidget>(w);
	TextDisplay *textD = tw->text.textD;
	TextBuffer *buf = textD->buffer;
	int column;

	if (checkReadOnly(w))
		return;
	cancelDrag(w);
	TakeMotifDestination(w, e->time);

	/* Create a string containing a newline followed by auto or smart
	   indent string */
	int cursorPos = textD->TextDGetInsertPosition();
	int lineStartPos = buf->BufStartOfLine(cursorPos);
	std::string indentStr = createIndentStringEx(tw, buf, 0, lineStartPos, cursorPos, nullptr, &column);

	/* Insert it at the cursor */
	simpleInsertAtCursorEx(w, indentStr, event, True);

	if (tw->text.emulateTabs > 0) {
		/*  If emulated tabs are on, make the inserted indent deletable by
		    tab. Round this up by faking the column a bit to the right to
		    let the user delete half-tabs with one keypress.  */
		column += tw->text.emulateTabs - 1;
		tw->text.emTabsBeforeCursor = column / tw->text.emulateTabs;
	}

	buf->BufUnselect();
}

static void processTabAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	TextBuffer *buf = textD->buffer;
	TextSelection *sel = &buf->primary_;
	int emTabDist = reinterpret_cast<TextWidget>(w)->text.emulateTabs;
	int emTabsBeforeCursor = reinterpret_cast<TextWidget>(w)->text.emTabsBeforeCursor;
	int insertPos, indent, startIndent, toIndent, lineStart, tabWidth;

	if (checkReadOnly(w))
		return;
	cancelDrag(w);
	TakeMotifDestination(w, event->xkey.time);

	/* If emulated tabs are off, just insert a tab */
	if (emTabDist <= 0) {
		TextInsertAtCursorEx(w, "\t", event, True, True);
		return;
	}

	/* Find the starting and ending indentation.  If the tab is to
	   replace an existing selection, use the start of the selection
	   instead of the cursor position as the indent.  When replacing
	   rectangular selections, tabs are automatically recalculated as
	   if the inserted text began at the start of the line */
	insertPos = pendingSelection(w) ? sel->start : textD->TextDGetInsertPosition();
	lineStart = buf->BufStartOfLine(insertPos);
	if (pendingSelection(w) && sel->rectangular)
		insertPos = buf->BufCountForwardDispChars(lineStart, sel->rectStart);
	startIndent = buf->BufCountDispChars(lineStart, insertPos);
	toIndent = startIndent + emTabDist - (startIndent % emTabDist);
	if (pendingSelection(w) && sel->rectangular) {
		toIndent -= startIndent;
		startIndent = 0;
	}

	/* Allocate a buffer assuming all the inserted characters will be spaces */
	std::string outStr;
	outStr.reserve(toIndent - startIndent);
		
	/* Add spaces and tabs to outStr until it reaches toIndent */
	auto outPtr = std::back_inserter(outStr);
	indent = startIndent;
	while (indent < toIndent) {
		tabWidth = TextBuffer::BufCharWidth('\t', indent, buf->tabDist_, buf->nullSubsChar_);
		if (buf->useTabs_ && tabWidth > 1 && indent + tabWidth <= toIndent) {
			*outPtr++ = '\t';
			indent += tabWidth;
		} else {
			*outPtr++ = ' ';
			indent++;
		}
	}

	/* Insert the emulated tab */
	TextInsertAtCursorEx(w, outStr, event, True, True);

	/* Restore and ++ emTabsBeforeCursor cleared by TextInsertAtCursorEx */
	reinterpret_cast<TextWidget>(w)->text.emTabsBeforeCursor = emTabsBeforeCursor + 1;

	buf->BufUnselect();
}

static void deleteSelectionAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	XKeyEvent *e = &event->xkey;

	cancelDrag(w);
	if (checkReadOnly(w))
		return;
	TakeMotifDestination(w, e->time);
	deletePendingSelection(w, event);
}

static void deletePreviousCharacterAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	XKeyEvent *e = &event->xkey;
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	int insertPos = textD->TextDGetInsertPosition();
	char c;
	int silent = hasKey("nobell", args, nArgs);

	cancelDrag(w);
	if (checkReadOnly(w))
		return;

	TakeMotifDestination(w, e->time);
	if (deletePendingSelection(w, event))
		return;

	if (insertPos == 0) {
		ringIfNecessary(silent, w);
		return;
	}

	if (deleteEmulatedTab(w, event))
		return;

	if (reinterpret_cast<TextWidget>(w)->text.overstrike) {
		c = textD->buffer->BufGetCharacter(insertPos - 1);
		if (c == '\n')
			textD->buffer->BufRemove(insertPos - 1, insertPos);
		else if (c != '\t')
			textD->buffer->BufReplaceEx(insertPos - 1, insertPos, " ");
	} else {
		textD->buffer->BufRemove(insertPos - 1, insertPos);
	}

	textD->TextDSetInsertPosition(insertPos - 1);
	checkAutoShowInsertPos(w);
	callCursorMovementCBs(w, event);
}

static void deleteNextCharacterAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	XKeyEvent *e = &event->xkey;
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	int insertPos = textD->TextDGetInsertPosition();
	int silent = hasKey("nobell", args, nArgs);

	cancelDrag(w);
	if (checkReadOnly(w))
		return;
	TakeMotifDestination(w, e->time);
	if (deletePendingSelection(w, event))
		return;
	if (insertPos == textD->buffer->BufGetLength()) {
		ringIfNecessary(silent, w);
		return;
	}
	textD->buffer->BufRemove(insertPos, insertPos + 1);
	checkAutoShowInsertPos(w);
	callCursorMovementCBs(w, event);
}

static void deletePreviousWordAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	XKeyEvent *e = &event->xkey;
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	int insertPos = textD->TextDGetInsertPosition();
	int pos, lineStart = textD->buffer->BufStartOfLine(insertPos);
	char *delimiters = reinterpret_cast<TextWidget>(w)->text.delimiters;
	int silent = hasKey("nobell", args, nArgs);

	cancelDrag(w);
	if (checkReadOnly(w)) {
		return;
	}

	TakeMotifDestination(w, e->time);
	if (deletePendingSelection(w, event)) {
		return;
	}

	if (insertPos == lineStart) {
		ringIfNecessary(silent, w);
		return;
	}

	pos = std::max<int>(insertPos - 1, 0);
	while (strchr(delimiters, textD->buffer->BufGetCharacter(pos)) != nullptr && pos != lineStart) {
		pos--;
	}

	pos = startOfWord(reinterpret_cast<TextWidget>(w), pos);
	textD->buffer->BufRemove(pos, insertPos);
	checkAutoShowInsertPos(w);
	callCursorMovementCBs(w, event);
}

static void deleteNextWordAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	XKeyEvent *e = &event->xkey;
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	int insertPos = textD->TextDGetInsertPosition();
	int pos, lineEnd = textD->buffer->BufEndOfLine(insertPos);
	char *delimiters = reinterpret_cast<TextWidget>(w)->text.delimiters;
	int silent = hasKey("nobell", args, nArgs);

	cancelDrag(w);
	if (checkReadOnly(w)) {
		return;
	}

	TakeMotifDestination(w, e->time);
	if (deletePendingSelection(w, event)) {
		return;
	}

	if (insertPos == lineEnd) {
		ringIfNecessary(silent, w);
		return;
	}

	pos = insertPos;
	while (strchr(delimiters, textD->buffer->BufGetCharacter(pos)) != nullptr && pos != lineEnd) {
		pos++;
	}

	pos = endOfWord(reinterpret_cast<TextWidget>(w), pos);
	textD->buffer->BufRemove(insertPos, pos);
	checkAutoShowInsertPos(w);
	callCursorMovementCBs(w, event);
}

static void deleteToEndOfLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	XKeyEvent *e = &event->xkey;
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	int insertPos = textD->TextDGetInsertPosition();
	int endOfLine;
	int silent = 0;

	silent = hasKey("nobell", args, nArgs);
	if (hasKey("absolute", args, nArgs))
		endOfLine = textD->buffer->BufEndOfLine(insertPos);
	else
		endOfLine = textD->TextDEndOfLine(insertPos, False);
	cancelDrag(w);
	if (checkReadOnly(w))
		return;
	TakeMotifDestination(w, e->time);
	if (deletePendingSelection(w, event))
		return;
	if (insertPos == endOfLine) {
		ringIfNecessary(silent, w);
		return;
	}
	textD->buffer->BufRemove(insertPos, endOfLine);
	checkAutoShowInsertPos(w);
	callCursorMovementCBs(w, event);
}

static void deleteToStartOfLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	XKeyEvent *e = &event->xkey;
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	int insertPos = textD->TextDGetInsertPosition();
	int startOfLine;
	int silent = 0;

	silent = hasKey("nobell", args, nArgs);
	if (hasKey("wrap", args, nArgs))
		startOfLine =textD->TextDStartOfLine(insertPos);
	else
		startOfLine = textD->buffer->BufStartOfLine(insertPos);
	cancelDrag(w);
	if (checkReadOnly(w))
		return;
	TakeMotifDestination(w, e->time);
	if (deletePendingSelection(w, event))
		return;
	if (insertPos == startOfLine) {
		ringIfNecessary(silent, w);
		return;
	}
	textD->buffer->BufRemove(startOfLine, insertPos);
	checkAutoShowInsertPos(w);
	callCursorMovementCBs(w, event);
}

static void forwardCharacterAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	int insertPos = reinterpret_cast<TextWidget>(w)->text.textD->TextDGetInsertPosition();
	int silent = hasKey("nobell", args, nArgs);

	cancelDrag(w);
	if (!reinterpret_cast<TextWidget>(w)->text.textD->TextDMoveRight()) {
		ringIfNecessary(silent, w);
	}
	checkMoveSelectionChange(w, event, insertPos, args, nArgs);
	checkAutoShowInsertPos(w);
	callCursorMovementCBs(w, event);
}

static void backwardCharacterAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	int insertPos = reinterpret_cast<TextWidget>(w)->text.textD->TextDGetInsertPosition();
	int silent = hasKey("nobell", args, nArgs);

	cancelDrag(w);
	if (!reinterpret_cast<TextWidget>(w)->text.textD->TextDMoveLeft()) {
		ringIfNecessary(silent, w);
	}
	checkMoveSelectionChange(w, event, insertPos, args, nArgs);
	checkAutoShowInsertPos(w);
	callCursorMovementCBs(w, event);
}

static void forwardWordAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	TextBuffer *buf = textD->buffer;
	int pos, insertPos = textD->TextDGetInsertPosition();
	char *delimiters = reinterpret_cast<TextWidget>(w)->text.delimiters;
	int silent = hasKey("nobell", args, nArgs);

	cancelDrag(w);
	if (insertPos == buf->BufGetLength()) {
		ringIfNecessary(silent, w);
		return;
	}
	pos = insertPos;

	if (hasKey("tail", args, nArgs)) {
		for (; pos < buf->BufGetLength(); pos++) {
			if (strchr(delimiters, buf->BufGetCharacter(pos)) == nullptr) {
				break;
			}
		}
		if (strchr(delimiters, buf->BufGetCharacter(pos)) == nullptr) {
			pos = endOfWord(reinterpret_cast<TextWidget>(w), pos);
		}
	} else {
		if (strchr(delimiters, buf->BufGetCharacter(pos)) == nullptr) {
			pos = endOfWord(reinterpret_cast<TextWidget>(w), pos);
		}
		for (; pos < buf->BufGetLength(); pos++) {
			if (strchr(delimiters, buf->BufGetCharacter(pos)) == nullptr) {
				break;
			}
		}
	}

	textD->TextDSetInsertPosition(pos);
	checkMoveSelectionChange(w, event, insertPos, args, nArgs);
	checkAutoShowInsertPos(w);
	callCursorMovementCBs(w, event);
}

static void backwardWordAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	TextBuffer *buf = textD->buffer;
	int pos, insertPos = textD->TextDGetInsertPosition();
	char *delimiters = reinterpret_cast<TextWidget>(w)->text.delimiters;
	int silent = hasKey("nobell", args, nArgs);

	cancelDrag(w);
	if (insertPos == 0) {
		ringIfNecessary(silent, w);
		return;
	}
	pos = std::max<int>(insertPos - 1, 0);
	while (strchr(delimiters, buf->BufGetCharacter(pos)) != nullptr && pos > 0)
		pos--;
	pos = startOfWord(reinterpret_cast<TextWidget>(w), pos);

	textD->TextDSetInsertPosition(pos);
	checkMoveSelectionChange(w, event, insertPos, args, nArgs);
	checkAutoShowInsertPos(w);
	callCursorMovementCBs(w, event);
}

static void forwardParagraphAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	int pos, insertPos = textD->TextDGetInsertPosition();
	TextBuffer *buf = textD->buffer;
	char c;
	static char whiteChars[] = " \t";
	int silent = hasKey("nobell", args, nArgs);

	cancelDrag(w);
	if (insertPos == buf->BufGetLength()) {
		ringIfNecessary(silent, w);
		return;
	}
	pos = std::min<int>(buf->BufEndOfLine(insertPos) + 1, buf->BufGetLength());
	while (pos < buf->BufGetLength()) {
		c = buf->BufGetCharacter(pos);
		if (c == '\n')
			break;
		if (strchr(whiteChars, c) != nullptr)
			pos++;
		else
			pos = std::min<int>(buf->BufEndOfLine(pos) + 1, buf->BufGetLength());
	}
	textD->TextDSetInsertPosition(std::min<int>(pos + 1, buf->BufGetLength()));
	checkMoveSelectionChange(w, event, insertPos, args, nArgs);
	checkAutoShowInsertPos(w);
	callCursorMovementCBs(w, event);
}

static void backwardParagraphAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	int parStart, pos, insertPos = textD->TextDGetInsertPosition();
	TextBuffer *buf = textD->buffer;
	char c;
	static char whiteChars[] = " \t";
	int silent = hasKey("nobell", args, nArgs);

	cancelDrag(w);
	if (insertPos == 0) {
		ringIfNecessary(silent, w);
		return;
	}
	parStart = buf->BufStartOfLine(std::max<int>(insertPos - 1, 0));
	pos = std::max<int>(parStart - 2, 0);
	while (pos > 0) {
		c = buf->BufGetCharacter(pos);
		if (c == '\n')
			break;
		if (strchr(whiteChars, c) != nullptr)
			pos--;
		else {
			parStart = buf->BufStartOfLine(pos);
			pos = std::max<int>(parStart - 2, 0);
		}
	}
	textD->TextDSetInsertPosition(parStart);
	checkMoveSelectionChange(w, event, insertPos, args, nArgs);
	checkAutoShowInsertPos(w);
	callCursorMovementCBs(w, event);
}

static void keySelectAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	int stat, insertPos = textD->TextDGetInsertPosition();
	int silent = hasKey("nobell", args, nArgs);

	cancelDrag(w);
	if (hasKey("left", args, nArgs))
		stat = textD->TextDMoveLeft();
	else if (hasKey("right", args, nArgs))
		stat = textD->TextDMoveRight();
	else if (hasKey("up", args, nArgs))
		stat = textD->TextDMoveUp(false);
	else if (hasKey("down", args, nArgs))
		stat = textD->TextDMoveDown(false);
	else {
		keyMoveExtendSelection(w, event, insertPos, hasKey("rect", args, nArgs));
		return;
	}
	if (!stat) {
		ringIfNecessary(silent, w);
	} else {
		keyMoveExtendSelection(w, event, insertPos, hasKey("rect", args, nArgs));
		checkAutoShowInsertPos(w);
		callCursorMovementCBs(w, event);
	}
}

static void processUpAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	int insertPos = reinterpret_cast<TextWidget>(w)->text.textD->TextDGetInsertPosition();
	int silent = hasKey("nobell", args, nArgs);
	int abs    = hasKey("absolute", args, nArgs);

	cancelDrag(w);
	if (!reinterpret_cast<TextWidget>(w)->text.textD->TextDMoveUp(abs))
		ringIfNecessary(silent, w);
	checkMoveSelectionChange(w, event, insertPos, args, nArgs);
	checkAutoShowInsertPos(w);
	callCursorMovementCBs(w, event);
}

static void processShiftUpAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	int insertPos = reinterpret_cast<TextWidget>(w)->text.textD->TextDGetInsertPosition();
	int silent = hasKey("nobell", args, nArgs);
	int abs    = hasKey("absolute", args, nArgs);

	cancelDrag(w);
	if (!reinterpret_cast<TextWidget>(w)->text.textD->TextDMoveUp(abs))
		ringIfNecessary(silent, w);
	keyMoveExtendSelection(w, event, insertPos, hasKey("rect", args, nArgs));
	checkAutoShowInsertPos(w);
	callCursorMovementCBs(w, event);
}

static void processDownAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	int insertPos = reinterpret_cast<TextWidget>(w)->text.textD->TextDGetInsertPosition();
	int silent = hasKey("nobell", args, nArgs);
	int abs    = hasKey("absolute", args, nArgs);

	cancelDrag(w);
	if (!reinterpret_cast<TextWidget>(w)->text.textD->TextDMoveDown(abs))
		ringIfNecessary(silent, w);
	checkMoveSelectionChange(w, event, insertPos, args, nArgs);
	checkAutoShowInsertPos(w);
	callCursorMovementCBs(w, event);
}

static void processShiftDownAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	int insertPos = reinterpret_cast<TextWidget>(w)->text.textD->TextDGetInsertPosition();
	int silent = hasKey("nobell", args, nArgs);
	int abs = hasKey("absolute", args, nArgs);

	cancelDrag(w);
	if (!reinterpret_cast<TextWidget>(w)->text.textD->TextDMoveDown(abs))
		ringIfNecessary(silent, w);
	keyMoveExtendSelection(w, event, insertPos, hasKey("rect", args, nArgs));
	checkAutoShowInsertPos(w);
	callCursorMovementCBs(w, event);
}

static void beginningOfLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	int insertPos = textD->TextDGetInsertPosition();

	cancelDrag(w);
	if (hasKey("absolute", args, nArgs))
		textD->TextDSetInsertPosition(textD->buffer->BufStartOfLine(insertPos));
	else
		textD->TextDSetInsertPosition(textD->TextDStartOfLine(insertPos));
	checkMoveSelectionChange(w, event, insertPos, args, nArgs);
	checkAutoShowInsertPos(w);
	callCursorMovementCBs(w, event);
	textD->cursorPreferredCol = 0;
}

static void endOfLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	int insertPos = textD->TextDGetInsertPosition();

	cancelDrag(w);
	if (hasKey("absolute", args, nArgs))
		textD->TextDSetInsertPosition(textD->buffer->BufEndOfLine(insertPos));
	else
		textD->TextDSetInsertPosition(textD->TextDEndOfLine(insertPos, False));
	checkMoveSelectionChange(w, event, insertPos, args, nArgs);
	checkAutoShowInsertPos(w);
	callCursorMovementCBs(w, event);
	textD->cursorPreferredCol = -1;
}

static void beginningOfFileAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	int insertPos = reinterpret_cast<TextWidget>(w)->text.textD->TextDGetInsertPosition();
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;

	cancelDrag(w);
	if (hasKey("scrollbar", args, nArgs)) {
		if (textD->topLineNum != 1) {
			textD->TextDSetScroll(1, textD->horizOffset);
		}
	} else {
		reinterpret_cast<TextWidget>(w)->text.textD->TextDSetInsertPosition(0);
		checkMoveSelectionChange(w, event, insertPos, args, nArgs);
		checkAutoShowInsertPos(w);
		callCursorMovementCBs(w, event);
	}
}

static void endOfFileAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	int insertPos = textD->TextDGetInsertPosition();
	int lastTopLine;

	cancelDrag(w);
	if (hasKey("scrollbar", args, nArgs)) {
		lastTopLine = std::max<int>(1, textD->nBufferLines - (textD->nVisibleLines - 2) + reinterpret_cast<TextWidget>(w)->text.cursorVPadding);
		if (lastTopLine != textD->topLineNum) {
			textD->TextDSetScroll(lastTopLine, textD->horizOffset);
		}
	} else {
		textD->TextDSetInsertPosition(textD->buffer->BufGetLength());
		checkMoveSelectionChange(w, event, insertPos, args, nArgs);
		checkAutoShowInsertPos(w);
		callCursorMovementCBs(w, event);
	}
}

static void nextPageAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	TextBuffer *buf = textD->buffer;
	int lastTopLine = std::max<int>(1, textD->nBufferLines - (textD->nVisibleLines - 2) + reinterpret_cast<TextWidget>(w)->text.cursorVPadding);
	int insertPos = textD->TextDGetInsertPosition();
	int column = 0, visLineNum, lineStartPos;
	int pos, targetLine;
	int pageForwardCount = std::max<int>(1, textD->nVisibleLines - 1);
	int maintainColumn = 0;
	int silent = hasKey("nobell", args, nArgs);

	maintainColumn = hasKey("column", args, nArgs);
	cancelDrag(w);
	if (hasKey("scrollbar", args, nArgs)) { /* scrollbar only */
		targetLine = std::min<int>(textD->topLineNum + pageForwardCount, lastTopLine);

		if (targetLine == textD->topLineNum) {
			ringIfNecessary(silent, w);
			return;
		}
		textD->TextDSetScroll(targetLine, textD->horizOffset);
	} else if (hasKey("stutter", args, nArgs)) { /* Mac style */
		/* move to bottom line of visible area */
		/* if already there, page down maintaining preferrred column */
		targetLine = std::max<int>(std::min<int>(textD->nVisibleLines - 1, textD->nBufferLines), 0);
		column = textD->TextDPreferredColumn(&visLineNum, &lineStartPos);
		if (lineStartPos == textD->lineStarts[targetLine]) {
			if (insertPos >= buf->BufGetLength() || textD->topLineNum == lastTopLine) {
				ringIfNecessary(silent, w);
				return;
			}
			targetLine = std::min<int>(textD->topLineNum + pageForwardCount, lastTopLine);
			pos = textD->TextDCountForwardNLines(insertPos, pageForwardCount, False);
			if (maintainColumn) {
				pos = textD->TextDPosOfPreferredCol(column, pos);
			}
			textD->TextDSetInsertPosition(pos);
			textD->TextDSetScroll(targetLine, textD->horizOffset);
		} else {
			pos = textD->lineStarts[targetLine];
			while (targetLine > 0 && pos == -1) {
				--targetLine;
				pos = textD->lineStarts[targetLine];
			}
			if (lineStartPos == pos) {
				ringIfNecessary(silent, w);
				return;
			}
			if (maintainColumn) {
				pos = textD->TextDPosOfPreferredCol(column, pos);
			}
			textD->TextDSetInsertPosition(pos);
		}
		checkMoveSelectionChange(w, event, insertPos, args, nArgs);
		checkAutoShowInsertPos(w);
		callCursorMovementCBs(w, event);
		if (maintainColumn) {
			textD->cursorPreferredCol = column;
		} else {
			textD->cursorPreferredCol = -1;
		}
	} else { /* "standard" */
		if (insertPos >= buf->BufGetLength() && textD->topLineNum == lastTopLine) {
			ringIfNecessary(silent, w);
			return;
		}
		if (maintainColumn) {
			column = textD->TextDPreferredColumn(&visLineNum, &lineStartPos);
		}
		targetLine = textD->topLineNum + textD->nVisibleLines - 1;
		if (targetLine < 1)
			targetLine = 1;
		if (targetLine > lastTopLine)
			targetLine = lastTopLine;
		pos = textD->TextDCountForwardNLines(insertPos, textD->nVisibleLines - 1, False);
		if (maintainColumn) {
			pos = textD->TextDPosOfPreferredCol(column, pos);
		}
		textD->TextDSetInsertPosition(pos);
		textD->TextDSetScroll(targetLine, textD->horizOffset);
		checkMoveSelectionChange(w, event, insertPos, args, nArgs);
		checkAutoShowInsertPos(w);
		callCursorMovementCBs(w, event);
		if (maintainColumn) {
			textD->cursorPreferredCol = column;
		} else {
			textD->cursorPreferredCol = -1;
		}
	}
}

static void previousPageAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	int insertPos = textD->TextDGetInsertPosition();
	int column = 0, visLineNum, lineStartPos;
	int pos, targetLine;
	int pageBackwardCount = std::max<int>(1, textD->nVisibleLines - 1);
	int maintainColumn = 0;
	int silent = hasKey("nobell", args, nArgs);

	maintainColumn = hasKey("column", args, nArgs);
	cancelDrag(w);
	if (hasKey("scrollbar", args, nArgs)) { /* scrollbar only */
		targetLine = std::max<int>(textD->topLineNum - pageBackwardCount, 1);

		if (targetLine == textD->topLineNum) {
			ringIfNecessary(silent, w);
			return;
		}
		textD->TextDSetScroll(targetLine, textD->horizOffset);
	} else if (hasKey("stutter", args, nArgs)) { /* Mac style */
		/* move to top line of visible area */
		/* if already there, page up maintaining preferrred column if required */
		targetLine = 0;
		column = textD->TextDPreferredColumn(&visLineNum, &lineStartPos);
		if (lineStartPos == textD->lineStarts[targetLine]) {
			if (textD->topLineNum == 1 && (maintainColumn || column == 0)) {
				ringIfNecessary(silent, w);
				return;
			}
			targetLine = std::max<int>(textD->topLineNum - pageBackwardCount, 1);
			pos = textD->TextDCountBackwardNLines(insertPos, pageBackwardCount);
			if (maintainColumn) {
				pos = textD->TextDPosOfPreferredCol(column, pos);
			}
			textD->TextDSetInsertPosition(pos);
			textD->TextDSetScroll(targetLine, textD->horizOffset);
		} else {
			pos = textD->lineStarts[targetLine];
			if (maintainColumn) {
				pos = textD->TextDPosOfPreferredCol(column, pos);
			}
			textD->TextDSetInsertPosition(pos);
		}
		checkMoveSelectionChange(w, event, insertPos, args, nArgs);
		checkAutoShowInsertPos(w);
		callCursorMovementCBs(w, event);
		if (maintainColumn) {
			textD->cursorPreferredCol = column;
		} else {
			textD->cursorPreferredCol = -1;
		}
	} else { /* "standard" */
		if (insertPos <= 0 && textD->topLineNum == 1) {
			ringIfNecessary(silent, w);
			return;
		}
		if (maintainColumn) {
			column = textD->TextDPreferredColumn(&visLineNum, &lineStartPos);
		}
		targetLine = textD->topLineNum - (textD->nVisibleLines - 1);
		if (targetLine < 1)
			targetLine = 1;
		pos = textD->TextDCountBackwardNLines(insertPos, textD->nVisibleLines - 1);
		if (maintainColumn) {
			pos = textD->TextDPosOfPreferredCol(column, pos);
		}
		textD->TextDSetInsertPosition(pos);
		textD->TextDSetScroll(targetLine, textD->horizOffset);
		checkMoveSelectionChange(w, event, insertPos, args, nArgs);
		checkAutoShowInsertPos(w);
		callCursorMovementCBs(w, event);
		if (maintainColumn) {
			textD->cursorPreferredCol = column;
		} else {
			textD->cursorPreferredCol = -1;
		}
	}
}

static void pageLeftAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	TextBuffer *buf = textD->buffer;
	int insertPos = textD->TextDGetInsertPosition();
	int maxCharWidth = textD->fontStruct->max_bounds.width;
	int lineStartPos, indent, pos;
	int horizOffset;
	int silent = hasKey("nobell", args, nArgs);

	cancelDrag(w);
	if (hasKey("scrollbar", args, nArgs)) {
		if (textD->horizOffset == 0) {
			ringIfNecessary(silent, w);
			return;
		}
		horizOffset = std::max<int>(0, textD->horizOffset - textD->width);
		textD->TextDSetScroll(textD->topLineNum, horizOffset);
	} else {
		lineStartPos = buf->BufStartOfLine(insertPos);
		if (insertPos == lineStartPos && textD->horizOffset == 0) {
			ringIfNecessary(silent, w);
			return;
		}
		indent = buf->BufCountDispChars(lineStartPos, insertPos);
		pos = buf->BufCountForwardDispChars(lineStartPos, std::max<int>(0, indent - textD->width / maxCharWidth));
		textD->TextDSetInsertPosition(pos);
		textD->TextDSetScroll(textD->topLineNum, std::max<int>(0, textD->horizOffset - textD->width));
		checkMoveSelectionChange(w, event, insertPos, args, nArgs);
		checkAutoShowInsertPos(w);
		callCursorMovementCBs(w, event);
	}
}

static void pageRightAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;
	(void)event;

	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	TextBuffer *buf = textD->buffer;
	int insertPos = textD->TextDGetInsertPosition();
	int maxCharWidth = textD->fontStruct->max_bounds.width;
	int oldHorizOffset = textD->horizOffset;
	int lineStartPos, indent, pos;
	int horizOffset, sliderSize, sliderMax;
	int silent = hasKey("nobell", args, nArgs);

	cancelDrag(w);
	if (hasKey("scrollbar", args, nArgs)) {
		XtVaGetValues(textD->hScrollBar, XmNmaximum, &sliderMax, XmNsliderSize, &sliderSize, nullptr);
		horizOffset = std::min<int>(textD->horizOffset + textD->width, sliderMax - sliderSize);
		if (textD->horizOffset == horizOffset) {
			ringIfNecessary(silent, w);
			return;
		}
		textD->TextDSetScroll(textD->topLineNum, horizOffset);
	} else {
		lineStartPos = buf->BufStartOfLine(insertPos);
		indent = buf->BufCountDispChars(lineStartPos, insertPos);
		pos = buf->BufCountForwardDispChars(lineStartPos, indent + textD->width / maxCharWidth);
		textD->TextDSetInsertPosition(pos);
		textD->TextDSetScroll(textD->topLineNum, textD->horizOffset + textD->width);
		if (textD->horizOffset == oldHorizOffset && insertPos == pos)
			ringIfNecessary(silent, w);
		checkMoveSelectionChange(w, event, insertPos, args, nArgs);
		checkAutoShowInsertPos(w);
		callCursorMovementCBs(w, event);
	}
}

static void toggleOverstrikeAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;
	(void)event;

	auto tw = reinterpret_cast<TextWidget>(w);

	if (tw->text.overstrike) {
		tw->text.overstrike = false;
		tw->text.textD->TextDSetCursorStyle(tw->text.heavyCursor ? HEAVY_CURSOR : NORMAL_CURSOR);
	} else {
		tw->text.overstrike = true;
		if (tw->text.textD->cursorStyle == NORMAL_CURSOR || tw->text.textD->cursorStyle == HEAVY_CURSOR)
			tw->text.textD->TextDSetCursorStyle(BLOCK_CURSOR);
	}
}

static void scrollUpAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)event;

	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	int topLineNum, horizOffset, nLines;

	if (*nArgs == 0 || sscanf(args[0], "%d", &nLines) != 1)
		return;
	if (*nArgs == 2) {
		/* Allow both 'page' and 'pages' */
		if (strncmp(args[1], "page", 4) == 0)
			nLines *= textD->nVisibleLines;

		/* 'line' or 'lines' is the only other valid possibility */
		else if (strncmp(args[1], "line", 4) != 0)
			return;
	}
	textD->TextDGetScroll(&topLineNum, &horizOffset);
	textD->TextDSetScroll(topLineNum - nLines, horizOffset);
}

static void scrollDownAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;
	(void)event;

	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	int topLineNum, horizOffset, nLines;

	if (*nArgs == 0 || sscanf(args[0], "%d", &nLines) != 1)
		return;
	if (*nArgs == 2) {
		/* Allow both 'page' and 'pages' */
		if (strncmp(args[1], "page", 4) == 0)
			nLines *= textD->nVisibleLines;

		/* 'line' or 'lines' is the only other valid possibility */
		else if (strncmp(args[1], "line", 4) != 0)
			return;
	}
	textD->TextDGetScroll(&topLineNum, &horizOffset);
	textD->TextDSetScroll(topLineNum + nLines, horizOffset);
}

static void scrollLeftAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;
	(void)event;

	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	int horizOffset, nPixels;
	int sliderMax, sliderSize;

	if (*nArgs == 0 || sscanf(args[0], "%d", &nPixels) != 1)
		return;
	XtVaGetValues(textD->hScrollBar, XmNmaximum, &sliderMax, XmNsliderSize, &sliderSize, nullptr);
	horizOffset = std::min<int>(std::max<int>(0, textD->horizOffset - nPixels), sliderMax - sliderSize);
	if (textD->horizOffset != horizOffset) {
		textD->TextDSetScroll(textD->topLineNum, horizOffset);
	}
}

static void scrollRightAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;
	(void)event;

	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	int horizOffset, nPixels;
	int sliderMax, sliderSize;

	if (*nArgs == 0 || sscanf(args[0], "%d", &nPixels) != 1)
		return;
	XtVaGetValues(textD->hScrollBar, XmNmaximum, &sliderMax, XmNsliderSize, &sliderSize, nullptr);
	horizOffset = std::min<int>(std::max<int>(0, textD->horizOffset + nPixels), sliderMax - sliderSize);
	if (textD->horizOffset != horizOffset) {
		textD->TextDSetScroll(textD->topLineNum, horizOffset);
	}
}

static void scrollToLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;
	(void)event;

	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	int topLineNum, horizOffset, lineNum;

	if (*nArgs == 0 || sscanf(args[0], "%d", &lineNum) != 1)
		return;
	textD->TextDGetScroll(&topLineNum, &horizOffset);
	textD->TextDSetScroll(lineNum, horizOffset);
}

static void selectAllAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;
	(void)event;

	TextBuffer *buf = reinterpret_cast<TextWidget>(w)->text.textD->buffer;

	cancelDrag(w);
	buf->BufSelect(0, buf->BufGetLength());
}

static void deselectAllAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;
	(void)event;

	cancelDrag(w);
	reinterpret_cast<TextWidget>(w)->text.textD->buffer->BufUnselect();
}

/*
**  Called on the Intrinsic FocusIn event.
**
**  Note that the widget has no internal state about the focus, ie. it does
**  not know whether it has the focus or not.
*/
static void focusInAP(Widget widget, XEvent *event, String *unused1, Cardinal *unused2) {

	(void)unused1;
	(void)unused2;

	TextWidget textwidget = (TextWidget)widget;
	TextDisplay *textD = textwidget->text.textD;

/* I don't entirely understand the traversal mechanism in Motif widgets,
   particularly, what leads to this widget getting a focus-in event when
   it does not actually have the input focus.  The temporary solution is
   to do the comparison below, and not show the cursor when Motif says
   we don't have focus, but keep looking for the real answer */
#if XmVersion >= 1002
	if (widget != XmGetFocusWidget(widget))
		return;
#endif

	/* If the timer is not already started, start it */
	if (textwidget->text.cursorBlinkRate != 0 && textwidget->text.cursorBlinkProcID == 0) {
		textwidget->text.cursorBlinkProcID = XtAppAddTimeOut(XtWidgetToApplicationContext(widget), textwidget->text.cursorBlinkRate, cursorBlinkTimerProc, widget);
	}

	/* Change the cursor to active style */
	if (textwidget->text.overstrike)
		textD->TextDSetCursorStyle(BLOCK_CURSOR);
	else
		textD->TextDSetCursorStyle((textwidget->text.heavyCursor ? HEAVY_CURSOR : NORMAL_CURSOR));
	textD->TextDUnblankCursor();

	/* Notify Motif input manager that widget has focus */
	XmImVaSetFocusValues(widget, nullptr);

	/* Call any registered focus-in callbacks */
	XtCallCallbacks((Widget)widget, textNfocusCallback, (XtPointer)event);
}

static void focusOutAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;

	/* Remove the cursor blinking timer procedure */
	if (reinterpret_cast<TextWidget>(w)->text.cursorBlinkProcID != 0)
		XtRemoveTimeOut(reinterpret_cast<TextWidget>(w)->text.cursorBlinkProcID);
	reinterpret_cast<TextWidget>(w)->text.cursorBlinkProcID = 0;

	/* Leave a dim or destination cursor */
	textD->TextDSetCursorStyle(reinterpret_cast<TextWidget>(w)->text.motifDestOwner ? CARET_CURSOR : DIM_CURSOR);
	textD->TextDUnblankCursor();

	/* If there's a calltip displayed, kill it. */
	TextDKillCalltip(textD, 0);

	/* Call any registered focus-out callbacks */
	XtCallCallbacks((Widget)w, textNlosingFocusCallback, (XtPointer)event);
}

/*
** For actions involving cursor movement, "extend" keyword means incorporate
** the new cursor position in the selection, and lack of an "extend" keyword
** means cancel the existing selection
*/
static void checkMoveSelectionChange(Widget w, XEvent *event, int startPos, String *args, Cardinal *nArgs) {
	if (hasKey("extend", args, nArgs)) {
		keyMoveExtendSelection(w, event, startPos, hasKey("rect", args, nArgs));
	} else {
		reinterpret_cast<TextWidget>(w)->text.textD->buffer->BufUnselect();
	}
}

/*
** If a selection change was requested via a keyboard command for moving
** the insertion cursor (usually with the "extend" keyword), adjust the
** selection to include the new cursor position, or begin a new selection
** between startPos and the new cursor position with anchor at startPos.
*/
static void keyMoveExtendSelection(Widget w, XEvent *event, int origPos, int rectangular) {
	XKeyEvent *e = &event->xkey;
	auto tw = reinterpret_cast<TextWidget>(w);
	TextDisplay *textD = tw->text.textD;
	TextBuffer *buf = textD->buffer;
	TextSelection *sel = &buf->primary_;
	int newPos = textD->TextDGetInsertPosition();
	int startPos, endPos, startCol, endCol, newCol, origCol;
	int anchor, rectAnchor, anchorLineStart;

	/* Moving the cursor does not take the Motif destination, but as soon as
	   the user selects something, grab it (I'm not sure if this distinction
	   actually makes sense, but it's what Motif was doing, back when their
	   secondary selections actually worked correctly) */
	TakeMotifDestination(w, e->time);

	if ((sel->selected || sel->zeroWidth) && sel->rectangular && rectangular) {
		/* rect -> rect */
		newCol = buf->BufCountDispChars(buf->BufStartOfLine(newPos), newPos);
		startCol = std::min<int>(tw->text.rectAnchor, newCol);
		endCol = std::max<int>(tw->text.rectAnchor, newCol);
		startPos = buf->BufStartOfLine(std::min<int>(tw->text.anchor, newPos));
		endPos = buf->BufEndOfLine(std::max<int>(tw->text.anchor, newPos));
		buf->BufRectSelect(startPos, endPos, startCol, endCol);
	} else if (sel->selected && rectangular) { /* plain -> rect */
		newCol = buf->BufCountDispChars(buf->BufStartOfLine(newPos), newPos);
		if (abs(newPos - sel->start) < abs(newPos - sel->end))
			anchor = sel->end;
		else
			anchor = sel->start;
		anchorLineStart = buf->BufStartOfLine(anchor);
		rectAnchor = buf->BufCountDispChars(anchorLineStart, anchor);
		tw->text.anchor = anchor;
		tw->text.rectAnchor = rectAnchor;
		buf->BufRectSelect(buf->BufStartOfLine(std::min<int>(anchor, newPos)), buf->BufEndOfLine(std::max<int>(anchor, newPos)), std::min<int>(rectAnchor, newCol), std::max<int>(rectAnchor, newCol));
	} else if (sel->selected && sel->rectangular) { /* rect -> plain */
		startPos = buf->BufCountForwardDispChars(buf->BufStartOfLine(sel->start), sel->rectStart);
		endPos = buf->BufCountForwardDispChars(buf->BufStartOfLine(sel->end), sel->rectEnd);
		if (abs(origPos - startPos) < abs(origPos - endPos))
			anchor = endPos;
		else
			anchor = startPos;
		buf->BufSelect(anchor, newPos);
	} else if (sel->selected) { /* plain -> plain */
		if (abs(origPos - sel->start) < abs(origPos - sel->end))
			anchor = sel->end;
		else
			anchor = sel->start;
		buf->BufSelect(anchor, newPos);
	} else if (rectangular) { /* no sel -> rect */
		origCol = buf->BufCountDispChars(buf->BufStartOfLine(origPos), origPos);
		newCol = buf->BufCountDispChars(buf->BufStartOfLine(newPos), newPos);
		startCol = std::min<int>(newCol, origCol);
		endCol = std::max<int>(newCol, origCol);
		startPos = buf->BufStartOfLine(std::min<int>(origPos, newPos));
		endPos = buf->BufEndOfLine(std::max<int>(origPos, newPos));
		tw->text.anchor = origPos;
		tw->text.rectAnchor = origCol;
		buf->BufRectSelect(startPos, endPos, startCol, endCol);
	} else { /* no sel -> plain */
		tw->text.anchor = origPos;
		tw->text.rectAnchor = buf->BufCountDispChars(buf->BufStartOfLine(origPos), origPos);
		buf->BufSelect(tw->text.anchor, newPos);
	}
}

static void checkAutoShowInsertPos(Widget w) {
	if (reinterpret_cast<TextWidget>(w)->text.autoShowInsertPos)
		reinterpret_cast<TextWidget>(w)->text.textD->TextDMakeInsertPosVisible();
}

static int checkReadOnly(Widget w) {
	if (reinterpret_cast<TextWidget>(w)->text.readOnly) {
		XBell(XtDisplay(w), 0);
		return True;
	}
	return False;
}


/*
** Insert text "chars" at the cursor position, as if the text had been
** typed.  Same as TextInsertAtCursorEx, but without the complicated auto-wrap
** scanning and re-formatting.
*/
static void simpleInsertAtCursorEx(Widget w, view::string_view chars, XEvent *event, int allowPendingDelete) {
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	TextBuffer *buf = textD->buffer;

	if (allowPendingDelete && pendingSelection(w)) {
		buf->BufReplaceSelectedEx(chars);
		textD->TextDSetInsertPosition(buf->cursorPosHint_);
	} else if (reinterpret_cast<TextWidget>(w)->text.overstrike) {
	
		size_t index = chars.find('\n');
		if(index != view::string_view::npos) {
			textD->TextDInsertEx(chars);
		} else {
			textD->TextDOverstrikeEx(chars);
		}
	} else {
		textD->TextDInsertEx(chars);
	}
	
	checkAutoShowInsertPos(w);
	callCursorMovementCBs(w, event);
}

/*
** If there's a selection, delete it and position the cursor where the
** selection was deleted.  (Called by routines which do deletion to check
** first for and do possible selection delete)
*/
static int deletePendingSelection(Widget w, XEvent *event) {
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	TextBuffer *buf = textD->buffer;

	if (reinterpret_cast<TextWidget>(w)->text.textD->buffer->primary_.selected) {
		buf->BufRemoveSelected();
		textD->TextDSetInsertPosition(buf->cursorPosHint_);
		checkAutoShowInsertPos(w);
		callCursorMovementCBs(w, event);
		return True;
	} else
		return False;
}

/*
** Return true if pending delete is on and there's a selection contiguous
** with the cursor ready to be deleted.  These criteria are used to decide
** if typing a character or inserting something should delete the selection
** first.
*/
static int pendingSelection(Widget w) {
	TextSelection *sel = &reinterpret_cast<TextWidget>(w)->text.textD->buffer->primary_;
	int pos = reinterpret_cast<TextWidget>(w)->text.textD->TextDGetInsertPosition();

	return reinterpret_cast<TextWidget>(w)->text.pendingDelete && sel->selected && pos >= sel->start && pos <= sel->end;
}

/*
** Check if tab emulation is on and if there are emulated tabs before the
** cursor, and if so, delete an emulated tab as a unit.  Also finishes up
** by calling checkAutoShowInsertPos and callCursorMovementCBs, so the
** calling action proc can just return (this is necessary to preserve
** emTabsBeforeCursor which is otherwise cleared by callCursorMovementCBs).
*/
static int deleteEmulatedTab(Widget w, XEvent *event) {
	TextDisplay *textD        = reinterpret_cast<TextWidget>(w)->text.textD;
	TextBuffer *buf        = reinterpret_cast<TextWidget>(w)->text.textD->buffer;
	int emTabDist          = reinterpret_cast<TextWidget>(w)->text.emulateTabs;
	int emTabsBeforeCursor = reinterpret_cast<TextWidget>(w)->text.emTabsBeforeCursor;
	int startIndent, toIndent, insertPos, startPos, lineStart;
	int pos, indent, startPosIndent;
	char c;

	if (emTabDist <= 0 || emTabsBeforeCursor <= 0)
		return False;

	/* Find the position of the previous tab stop */
	insertPos = textD->TextDGetInsertPosition();
	lineStart = buf->BufStartOfLine(insertPos);
	startIndent = buf->BufCountDispChars(lineStart, insertPos);
	toIndent = (startIndent - 1) - ((startIndent - 1) % emTabDist);

	/* Find the position at which to begin deleting (stop at non-whitespace
	   characters) */
	startPosIndent = indent = 0;
	startPos = lineStart;
	for (pos = lineStart; pos < insertPos; pos++) {
		c = buf->BufGetCharacter(pos);
		indent += TextBuffer::BufCharWidth(c, indent, buf->tabDist_, buf->nullSubsChar_);
		if (indent > toIndent)
			break;
		startPosIndent = indent;
		startPos = pos + 1;
	}

	/* Just to make sure, check that we're not deleting any non-white chars */
	for (pos = insertPos - 1; pos >= startPos; pos--) {
		c = buf->BufGetCharacter(pos);
		if (c != ' ' && c != '\t') {
			startPos = pos + 1;
			break;
		}
	}

	/* Do the text replacement and reposition the cursor.  If any spaces need
	   to be inserted to make up for a deleted tab, do a BufReplaceEx, otherwise,
	   do a BufRemove. */
	if (startPosIndent < toIndent) {
	
		std::string spaceString(toIndent - startPosIndent, ' ');
	
		buf->BufReplaceEx(startPos, insertPos, spaceString);
		textD->TextDSetInsertPosition(startPos + toIndent - startPosIndent);
	} else {
		buf->BufRemove(startPos, insertPos);
		textD->TextDSetInsertPosition(startPos);
	}

	/* The normal cursor movement stuff would usually be called by the action
	   routine, but this wraps around it to restore emTabsBeforeCursor */
	checkAutoShowInsertPos(w);
	callCursorMovementCBs(w, event);

	/* Decrement and restore the marker for consecutive emulated tabs, which
	   would otherwise have been zeroed by callCursorMovementCBs */
	reinterpret_cast<TextWidget>(w)->text.emTabsBeforeCursor = emTabsBeforeCursor - 1;
	return True;
}

/*
** Select the word or whitespace adjacent to the cursor, and move the cursor
** to its end.  pointerX is used as a tie-breaker, when the cursor is at the
** boundary between a word and some white-space.  If the cursor is on the
** left, the word or space on the left is used.  If it's on the right, that
** is used instead.
*/
static void selectWord(Widget w, int pointerX) {
	auto tw = reinterpret_cast<TextWidget>(w);
	TextBuffer *buf = tw->text.textD->buffer;
	int x, y, insertPos = tw->text.textD->TextDGetInsertPosition();

	TextPosToXY(w, insertPos, &x, &y);
	if (pointerX < x && insertPos > 0 && buf->BufGetCharacter(insertPos - 1) != '\n')
		insertPos--;
	buf->BufSelect(startOfWord(tw, insertPos), endOfWord(tw, insertPos));
}

static int startOfWord(TextWidget w, int pos) {
	int startPos;
	TextBuffer *buf = w->text.textD->buffer;
	char *delimiters = w->text.delimiters;
	char c = buf->BufGetCharacter(pos);

	if (c == ' ' || c == '\t') {
		if (!spanBackward(buf, pos, " \t", False, &startPos))
			return 0;
	} else if (strchr(delimiters, c)) {
		if (!spanBackward(buf, pos, delimiters, True, &startPos))
			return 0;
	} else {
		if (!buf->BufSearchBackwardEx(pos, delimiters, &startPos))
			return 0;
	}
	return std::min<int>(pos, startPos + 1);
}

static int endOfWord(TextWidget w, int pos) {
	int endPos;
	TextBuffer *buf = w->text.textD->buffer;
	char *delimiters = w->text.delimiters;
	char c = buf->BufGetCharacter(pos);

	if (c == ' ' || c == '\t') {
		if (!spanForward(buf, pos, " \t", False, &endPos))
			return buf->BufGetLength();
	} else if (strchr(delimiters, c)) {
		if (!spanForward(buf, pos, delimiters, True, &endPos))
			return buf->BufGetLength();
	} else {
		if (!buf->BufSearchForwardEx(pos, delimiters, &endPos))
			return buf->BufGetLength();
	}
	return endPos;
}

/*
** Search forwards in buffer "buf" for the first character NOT in
** "searchChars",  starting with the character "startPos", and returning the
** result in "foundPos" returns True if found, False if not. If ignoreSpace
** is set, then Space, Tab, and Newlines are ignored in searchChars.
*/
static int spanForward(TextBuffer *buf, int startPos, const char *searchChars, int ignoreSpace, int *foundPos) {
	int pos;
	const char *c;

	pos = startPos;
	while (pos < buf->BufGetLength()) {
		for (c = searchChars; *c != '\0'; c++)
			if (!(ignoreSpace && (*c == ' ' || *c == '\t' || *c == '\n')))
				if (buf->BufGetCharacter(pos) == *c)
					break;
		if (*c == 0) {
			*foundPos = pos;
			return True;
		}
		pos++;
	}
	*foundPos = buf->BufGetLength();
	return False;
}

/*
** Search backwards in buffer "buf" for the first character NOT in
** "searchChars",  starting with the character BEFORE "startPos", returning the
** result in "foundPos" returns True if found, False if not. If ignoreSpace is
** set, then Space, Tab, and Newlines are ignored in searchChars.
*/
static int spanBackward(TextBuffer *buf, int startPos, const char *searchChars, int ignoreSpace, int *foundPos) {
	int pos;
	const char *c;

	if (startPos == 0) {
		*foundPos = 0;
		return False;
	}
	pos = startPos == 0 ? 0 : startPos - 1;
	while (pos >= 0) {
		for (c = searchChars; *c != '\0'; c++)
			if (!(ignoreSpace && (*c == ' ' || *c == '\t' || *c == '\n')))
				if (buf->BufGetCharacter(pos) == *c)
					break;
		if (*c == 0) {
			*foundPos = pos;
			return True;
		}
		pos--;
	}
	*foundPos = 0;
	return False;
}

/*
** Select the line containing the cursor, including the terminating newline,
** and move the cursor to its end.
*/
static void selectLine(Widget w) {
	TextDisplay *textD = reinterpret_cast<TextWidget>(w)->text.textD;
	int insertPos = textD->TextDGetInsertPosition();
	int endPos, startPos;

	endPos = textD->buffer->BufEndOfLine(insertPos);
	startPos = textD->buffer->BufStartOfLine(insertPos);
	textD->buffer->BufSelect(startPos, std::min<int>(endPos + 1, textD->buffer->BufGetLength()));
	textD->TextDSetInsertPosition(endPos);
}

/*
** Given a new mouse pointer location, pass the position on to the
** autoscroll timer routine, and make sure the timer is on when it's
** needed and off when it's not.
*/
static void checkAutoScroll(TextWidget w, int x, int y) {
	int inWindow;

	/* Is the pointer in or out of the window? */
	inWindow = x >= w->text.textD->left && x < w->core.width - w->text.marginWidth && y >= w->text.marginHeight && y < w->core.height - w->text.marginHeight;

	/* If it's in the window, cancel the timer procedure */
	if (inWindow) {
		if (w->text.autoScrollProcID != 0)
			XtRemoveTimeOut(w->text.autoScrollProcID);
		;
		w->text.autoScrollProcID = 0;
		return;
	}

	/* If the timer is not already started, start it */
	if (w->text.autoScrollProcID == 0) {
		w->text.autoScrollProcID = XtAppAddTimeOut(XtWidgetToApplicationContext((Widget)w), 0, autoScrollTimerProc, w);
	}

	/* Pass on the newest mouse location to the autoscroll routine */
	w->text.mouseCoord = {x, y};
}

/*
** Reset drag state and cancel the auto-scroll timer
*/
static void endDrag(Widget w) {
	if (reinterpret_cast<TextWidget>(w)->text.autoScrollProcID != 0)
		XtRemoveTimeOut(reinterpret_cast<TextWidget>(w)->text.autoScrollProcID);
	reinterpret_cast<TextWidget>(w)->text.autoScrollProcID = 0;
	if (reinterpret_cast<TextWidget>(w)->text.dragState == MOUSE_PAN)
		XUngrabPointer(XtDisplay(w), CurrentTime);
	reinterpret_cast<TextWidget>(w)->text.dragState = NOT_CLICKED;
}

/*
** Cancel any drag operation that might be in progress.  Should be included
** in nearly every key event to cleanly end any dragging before edits are made
** which might change the insert position or the content of the buffer during
** a drag operation)
*/
static void cancelDrag(Widget w) {
	int dragState = reinterpret_cast<TextWidget>(w)->text.dragState;

	if (reinterpret_cast<TextWidget>(w)->text.autoScrollProcID != 0)
		XtRemoveTimeOut(reinterpret_cast<TextWidget>(w)->text.autoScrollProcID);
	if (dragState == SECONDARY_DRAG || dragState == SECONDARY_RECT_DRAG)
		reinterpret_cast<TextWidget>(w)->text.textD->buffer->BufSecondaryUnselect();
	if (dragState == PRIMARY_BLOCK_DRAG)
		CancelBlockDrag(reinterpret_cast<TextWidget>(w));
	if (dragState == MOUSE_PAN)
		XUngrabPointer(XtDisplay(w), CurrentTime);
	if (dragState != NOT_CLICKED)
		reinterpret_cast<TextWidget>(w)->text.dragState = DRAG_CANCELED;
}

/*
** Do operations triggered by cursor movement: Call cursor movement callback
** procedure(s), and cancel marker indicating that the cursor is after one or
** more just-entered emulated tabs (spaces to be deleted as a unit).
*/
static void callCursorMovementCBs(Widget w, XEvent *event) {
	reinterpret_cast<TextWidget>(w)->text.emTabsBeforeCursor = 0;
	XtCallCallbacks((Widget)w, textNcursorMovementCallback, (XtPointer)event);
}

/*
** Adjust the selection as the mouse is dragged to position: (x, y).
*/
static void adjustSelection(TextWidget tw, int x, int y) {
	TextDisplay *textD = tw->text.textD;
	TextBuffer *buf = textD->buffer;
	int row, col, startCol, endCol, startPos, endPos;
	int newPos = textD->TextDXYToPosition(Point{x, y});

	/* Adjust the selection */
	if (tw->text.dragState == PRIMARY_RECT_DRAG) {
		textD->TextDXYToUnconstrainedPosition(Point{x, y}, &row, &col);
		col = textD->TextDOffsetWrappedColumn(row, col);
		startCol = std::min<int>(tw->text.rectAnchor, col);
		endCol = std::max<int>(tw->text.rectAnchor, col);
		startPos = buf->BufStartOfLine(std::min<int>(tw->text.anchor, newPos));
		endPos = buf->BufEndOfLine(std::max<int>(tw->text.anchor, newPos));
		buf->BufRectSelect(startPos, endPos, startCol, endCol);
	} else if (tw->text.multiClickState == ONE_CLICK) {
		startPos = startOfWord(tw, std::min<int>(tw->text.anchor, newPos));
		endPos = endOfWord(tw, std::max<int>(tw->text.anchor, newPos));
		buf->BufSelect(startPos, endPos);
		newPos = newPos < tw->text.anchor ? startPos : endPos;
	} else if (tw->text.multiClickState == TWO_CLICKS) {
		startPos = buf->BufStartOfLine(std::min<int>(tw->text.anchor, newPos));
		endPos = buf->BufEndOfLine(std::max<int>(tw->text.anchor, newPos));
		buf->BufSelect(startPos, std::min<int>(endPos + 1, buf->BufGetLength()));
		newPos = newPos < tw->text.anchor ? startPos : endPos;
	} else
		buf->BufSelect(tw->text.anchor, newPos);

	/* Move the cursor */
	textD->TextDSetInsertPosition(newPos);
	callCursorMovementCBs((Widget)tw, nullptr);
}

static void adjustSecondarySelection(TextWidget tw, int x, int y) {
	TextDisplay *textD = tw->text.textD;
	TextBuffer *buf = textD->buffer;
	int row, col, startCol, endCol, startPos, endPos;
	int newPos = textD->TextDXYToPosition(Point{x, y});

	if (tw->text.dragState == SECONDARY_RECT_DRAG) {
		textD->TextDXYToUnconstrainedPosition(Point{x, y}, &row, &col);
		col = textD->TextDOffsetWrappedColumn(row, col);
		startCol = std::min<int>(tw->text.rectAnchor, col);
		endCol = std::max<int>(tw->text.rectAnchor, col);
		startPos = buf->BufStartOfLine(std::min<int>(tw->text.anchor, newPos));
		endPos = buf->BufEndOfLine(std::max<int>(tw->text.anchor, newPos));
		buf->BufSecRectSelect(startPos, endPos, startCol, endCol);
	} else
		textD->buffer->BufSecondarySelect(tw->text.anchor, newPos);
}

/*
** Wrap multi-line text in argument "text" to be inserted at the end of the
** text on line "startLine" and return the result.  If "breakBefore" is
** non-nullptr, allow wrapping to extend back into "startLine", in which case
** the returned text will include the wrapped part of "startLine", and
** "breakBefore" will return the number of characters at the end of
** "startLine" that were absorbed into the returned string.  "breakBefore"
** will return zero if no characters were absorbed into the returned string.
** The buffer offset of text in the widget's text buffer is needed so that
** smart indent (which can be triggered by wrapping) can search back farther
** in the buffer than just the text in startLine.
*/
static std::string wrapTextEx(TextWidget tw, view::string_view startLine, view::string_view text, int bufOffset, int wrapMargin, int *breakBefore) {
	TextBuffer *buf = tw->text.textD->buffer;
	int startLineLen = startLine.size();
	int colNum, pos, lineStartPos, limitPos, breakAt, charsAdded;
	int firstBreak = -1, tabDist = buf->tabDist_;
	char c;
	std::string wrappedText;

	/* Create a temporary text buffer and load it with the strings */
	auto wrapBuf = new TextBuffer;
	wrapBuf->BufInsertEx(0, startLine);
	wrapBuf->BufInsertEx(wrapBuf->BufGetLength(), text);

	/* Scan the buffer for long lines and apply wrapLine when wrapMargin is
	   exceeded.  limitPos enforces no breaks in the "startLine" part of the
	   string (if requested), and prevents re-scanning of long unbreakable
	   lines for each character beyond the margin */
	colNum = 0;
	pos = 0;
	lineStartPos = 0;
	limitPos = breakBefore == nullptr ? startLineLen : 0;
	while (pos < wrapBuf->BufGetLength()) {
		c = wrapBuf->BufGetCharacter(pos);
		if (c == '\n') {
			lineStartPos = limitPos = pos + 1;
			colNum = 0;
		} else {
			colNum += TextBuffer::BufCharWidth(c, colNum, tabDist, buf->nullSubsChar_);
			if (colNum > wrapMargin) {
				if (!wrapLine(tw, wrapBuf, bufOffset, lineStartPos, pos, limitPos, &breakAt, &charsAdded)) {
					limitPos = std::max<int>(pos, limitPos);
				} else {
					lineStartPos = limitPos = breakAt + 1;
					pos += charsAdded;
					colNum = wrapBuf->BufCountDispChars(lineStartPos, pos + 1);
					if (firstBreak == -1)
						firstBreak = breakAt;
				}
			}
		}
		pos++;
	}

	/* Return the wrapped text, possibly including part of startLine */
	if(!breakBefore) {
		wrappedText = wrapBuf->BufGetRangeEx(startLineLen, wrapBuf->BufGetLength());
	} else {
		*breakBefore = firstBreak != -1 && firstBreak < startLineLen ? startLineLen - firstBreak : 0;
		wrappedText = wrapBuf->BufGetRangeEx(startLineLen - *breakBefore, wrapBuf->BufGetLength());
	}
	delete wrapBuf;
	return wrappedText;
}

/*
** Wraps the end of a line beginning at lineStartPos and ending at lineEndPos
** in "buf", at the last white-space on the line >= limitPos.  (The implicit
** assumption is that just the last character of the line exceeds the wrap
** margin, and anywhere on the line we can wrap is correct).  Returns False if
** unable to wrap the line.  "breakAt", returns the character position at
** which the line was broken,
**
** Auto-wrapping can also trigger auto-indent.  The additional parameter
** bufOffset is needed when auto-indent is set to smart indent and the smart
** indent routines need to scan far back in the buffer.  "charsAdded" returns
** the number of characters added to acheive the auto-indent.  wrapMargin is
** used to decide whether auto-indent should be skipped because the indent
** string itself would exceed the wrap margin.
*/
static int wrapLine(TextWidget tw, TextBuffer *buf, int bufOffset, int lineStartPos, int lineEndPos, int limitPos, int *breakAt, int *charsAdded) {
	int p, length, column;


	/* Scan backward for whitespace or BOL.  If BOL, return False, no
	   whitespace in line at which to wrap */
	for (p = lineEndPos;; p--) {
		if (p < lineStartPos || p < limitPos) {
			return False;
		}
		
		char c = buf->BufGetCharacter(p);
		if (c == '\t' || c == ' ')
			break;
	}

	/* Create an auto-indent string to insert to do wrap.  If the auto
	   indent string reaches the wrap position, slice the auto-indent
	   back off and return to the left margin */
	std::string indentStr;
	if (tw->text.autoIndent || tw->text.smartIndent) {
		indentStr = createIndentStringEx(tw, buf, bufOffset, lineStartPos, lineEndPos, &length, &column);
		if (column >= p - lineStartPos) {
			indentStr.resize(1);
		}
	} else {
		indentStr = "\n";
		length = 1;
	}

	/* Replace the whitespace character with the auto-indent string
	   and return the stats */
	buf->BufReplaceEx(p, p + 1, indentStr);
	
	*breakAt = p;
	*charsAdded = length - 1;
	return True;
}

/*
** Create and return an auto-indent string to add a newline at lineEndPos to a
** line starting at lineStartPos in buf.  "buf" may or may not be the real
** text buffer for the widget.  If it is not the widget's text buffer it's
** offset position from the real buffer must be specified in "bufOffset" to
** allow the smart-indent routines to scan back as far as necessary. The
** string length is returned in "length" (or "length" can be passed as nullptr,
** and the indent column is returned in "column" (if non nullptr).
*/
static std::string createIndentStringEx(TextWidget tw, TextBuffer *buf, int bufOffset, int lineStartPos, int lineEndPos, int *length, int *column) {
	TextDisplay *textD = tw->text.textD;
	int pos, indent = -1, tabDist = textD->buffer->tabDist_;
	int i, useTabs = textD->buffer->useTabs_;
	char c;
	smartIndentCBStruct smartIndent;

	/* If smart indent is on, call the smart indent callback.  It is not
	   called when multi-line changes are being made (lineStartPos != 0),
	   because smart indent needs to search back an indeterminate distance
	   through the buffer, and reconciling that with wrapping changes made,
	   but not yet committed in the buffer, would make programming smart
	   indent more difficult for users and make everything more complicated */
	if (tw->text.smartIndent && (lineStartPos == 0 || buf == textD->buffer)) {
		smartIndent.reason = NEWLINE_INDENT_NEEDED;
		smartIndent.pos = lineEndPos + bufOffset;
		smartIndent.indentRequest = 0;
		smartIndent.charsTyped = nullptr;
		XtCallCallbacks((Widget)tw, textNsmartIndentCallback, &smartIndent);
		indent = smartIndent.indentRequest;
	}

	/* If smart indent wasn't used, measure the indent distance of the line */
	if (indent == -1) {
		indent = 0;
		for (pos = lineStartPos; pos < lineEndPos; pos++) {
			c = buf->BufGetCharacter(pos);
			if (c != ' ' && c != '\t')
				break;
			if (c == '\t')
				indent += tabDist - (indent % tabDist);
			else
				indent++;
		}
	}

	/* Allocate and create a string of tabs and spaces to achieve the indent */
	std::string indentStr;
	indentStr.reserve(indent + 2);
	
	auto indentPtr = std::back_inserter(indentStr);
	
	*indentPtr++ = '\n';
	if (useTabs) {
		for (i = 0; i < indent / tabDist; i++)
			*indentPtr++ = '\t';
		for (i = 0; i < indent % tabDist; i++)
			*indentPtr++ = ' ';
	} else {
		for (i = 0; i < indent; i++)
			*indentPtr++ = ' ';
	}

	/* Return any requested stats */
	if(length)
		*length = indentStr.size();
	if(column)
		*column = indent;

	return indentStr;
}

/*
** Xt timer procedure for autoscrolling
*/
static void autoScrollTimerProc(XtPointer clientData, XtIntervalId *id) {
	(void)id;

	TextWidget w = (TextWidget)clientData;
	TextDisplay *textD = w->text.textD;
	int topLineNum, horizOffset, newPos, cursorX, y;
	int fontWidth = textD->fontStruct->max_bounds.width;
	int fontHeight = textD->fontStruct->ascent + textD->fontStruct->descent;

	/* For vertical autoscrolling just dragging the mouse outside of the top
	   or bottom of the window is sufficient, for horizontal (non-rectangular)
	   scrolling, see if the position where the CURSOR would go is outside */
	newPos = textD->TextDXYToPosition(w->text.mouseCoord);
	if (w->text.dragState == PRIMARY_RECT_DRAG)
		cursorX = w->text.mouseCoord.x;
	else if (!textD->TextDPositionToXY(newPos, &cursorX, &y))
		cursorX = w->text.mouseCoord.x;

	/* Scroll away from the pointer, 1 character (horizontal), or 1 character
	   for each fontHeight distance from the mouse to the text (vertical) */
	textD->TextDGetScroll(&topLineNum, &horizOffset);
	if (cursorX >= (int)w->core.width - w->text.marginWidth)
		horizOffset += fontWidth;
	else if (w->text.mouseCoord.x < textD->left)
		horizOffset -= fontWidth;
	if (w->text.mouseCoord.y >= (int)w->core.height - w->text.marginHeight)
		topLineNum += 1 + ((w->text.mouseCoord.y - (int)w->core.height - w->text.marginHeight) / fontHeight) + 1;
	else if (w->text.mouseCoord.y < w->text.marginHeight)
		topLineNum -= 1 + ((w->text.marginHeight - w->text.mouseCoord.y) / fontHeight);
	textD->TextDSetScroll(topLineNum, horizOffset);

	/* Continue the drag operation in progress.  If none is in progress
	   (safety check) don't continue to re-establish the timer proc */
	if (w->text.dragState == PRIMARY_DRAG) {
		adjustSelection(w, w->text.mouseCoord.x, w->text.mouseCoord.y);
	} else if (w->text.dragState == PRIMARY_RECT_DRAG) {
		adjustSelection(w, w->text.mouseCoord.x, w->text.mouseCoord.y);
	} else if (w->text.dragState == SECONDARY_DRAG) {
		adjustSecondarySelection(w, w->text.mouseCoord.x, w->text.mouseCoord.y);
	} else if (w->text.dragState == SECONDARY_RECT_DRAG) {
		adjustSecondarySelection(w, w->text.mouseCoord.x, w->text.mouseCoord.y);
	} else if (w->text.dragState == PRIMARY_BLOCK_DRAG) {
		BlockDragSelection(w, w->text.mouseCoord, USE_LAST);
	} else {
		w->text.autoScrollProcID = 0;
		return;
	}

	/* re-establish the timer proc (this routine) to continue processing */
	w->text.autoScrollProcID =
	    XtAppAddTimeOut(XtWidgetToApplicationContext((Widget)w), w->text.mouseCoord.y >= w->text.marginHeight && w->text.mouseCoord.y < w->core.height - w->text.marginHeight ? (VERTICAL_SCROLL_DELAY * fontWidth) / fontHeight : VERTICAL_SCROLL_DELAY,
	                    autoScrollTimerProc, w);
}

/*
** Xt timer procedure for cursor blinking
*/
static void cursorBlinkTimerProc(XtPointer clientData, XtIntervalId *id) {
	(void)id;

	TextWidget w = (TextWidget)clientData;
	TextDisplay *textD = w->text.textD;

	/* Blink the cursor */
	if (textD->cursorOn)
		textD->TextDBlankCursor();
	else
		textD->TextDUnblankCursor();

	/* re-establish the timer proc (this routine) to continue processing */
	w->text.cursorBlinkProcID = XtAppAddTimeOut(XtWidgetToApplicationContext((Widget)w), w->text.cursorBlinkRate, cursorBlinkTimerProc, w);
}

/*
**  Sets the caret to on or off and restart the caret blink timer.
**  This could be used by other modules to modify the caret's blinking.
*/
void ResetCursorBlink(TextWidget textWidget, Boolean startsBlanked) {
	if (textWidget->text.cursorBlinkRate != 0) {
		if (textWidget->text.cursorBlinkProcID != 0) {
			XtRemoveTimeOut(textWidget->text.cursorBlinkProcID);
		}

		if (startsBlanked) {
			textWidget->text.textD->TextDBlankCursor();
		} else {
			textWidget->text.textD->TextDUnblankCursor();
		}

		textWidget->text.cursorBlinkProcID = XtAppAddTimeOut(XtWidgetToApplicationContext((Widget)textWidget), textWidget->text.cursorBlinkRate, cursorBlinkTimerProc, textWidget);
	}
}

/*
** look at an action procedure's arguments to see if argument "key" has been
** specified in the argument list
*/
static int hasKey(const char *key, const String *args, const Cardinal *nArgs) {
	int i;

	for (i = 0; i < (int)*nArgs; i++)
		if (!strCaseCmp(args[i], key))
			return True;
	return False;
}

/*
** strCaseCmp compares its arguments and returns 0 if the two strings
** are equal IGNORING case differences.  Otherwise returns 1.
*/

static int strCaseCmp(const char *str1, const char *str2) {
	unsigned const char *c1 = (unsigned const char *)str1;
	unsigned const char *c2 = (unsigned const char *)str2;

	for (; *c1 != '\0' && *c2 != '\0'; c1++, c2++)
		if (toupper(*c1) != toupper(*c2))
			return 1;
	if (*c1 == *c2) {
		return (0);
	} else {
		return (1);
	}
}

static void ringIfNecessary(Boolean silent, Widget w) {
	if (!silent)
		XBell(XtDisplay(w), 0);
}
