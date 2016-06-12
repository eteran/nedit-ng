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

#include <QMessageBox>
#include <QApplication>
#include "text.h"
#include "textP.h"
#include "TextBuffer.h"
#include "TextDisplay.h"
#include "nedit.h"
#include "Document.h"
#include "calltips.h"
#include "window.h"
#include "memory.h"
#include "TextHelper.h"

#include <climits>
#include <algorithm>

#include <X11/Intrinsic.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/cursorfont.h>
#include <Xm/Xm.h>
#include <Xm/XmP.h>
#include <Xm/PrimitiveP.h>

namespace {

/* Number of pixels of motion from the initial (grab-focus) button press
   required to begin recognizing a mouse drag for the purpose of making a
   selection */
const int SELECT_THRESHOLD = 5;

// Length of delay in milliseconds for vertical autoscrolling
const int VERTICAL_SCROLL_DELAY = 50;

}

static void initialize(Widget request, Widget newWidget, ArgList args, Cardinal *num_args);
static void handleHidePointer(Widget w, XtPointer unused, XEvent *event, Boolean *continue_to_dispatch);
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


static void adjustSelection(TextWidget tw, int x, int y);
static void adjustSecondarySelection(TextWidget tw, int x, int y);
static void autoScrollTimerProc(XtPointer clientData, XtIntervalId *id);
static std::string createIndentStringEx(TextWidget tw, TextBuffer *buf, int bufOffset, int lineStartPos, int lineEndPos, int *length, int *column);
static void cursorBlinkTimerProc(XtPointer clientData, XtIntervalId *id);
static bool hasKey(const char *key, const String *args, const Cardinal *nArgs);
static int strCaseCmp(const char *str1, const char *str2);
static void ringIfNecessary(bool silent);

static char defaultTranslations[] =
    // Home
    "~Shift ~Ctrl Alt<Key>osfBeginLine: last_document()\n"

    // Backspace
    "Ctrl<KeyPress>osfBackSpace: delete_previous_word()\n"
    "<KeyPress>osfBackSpace: delete_previous_character()\n"

    // Delete
    "Alt Shift Ctrl<KeyPress>osfDelete: cut_primary(\"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfDelete: cut_primary(\"rect\")\n"
    "Shift Ctrl<KeyPress>osfDelete: cut_primary()\n"
    "Ctrl<KeyPress>osfDelete: delete_to_end_of_line()\n"
    "Shift<KeyPress>osfDelete: cut_clipboard()\n"
    "<KeyPress>osfDelete: delete_next_character()\n"

    // Insert
    "Alt Shift Ctrl<KeyPress>osfInsert: copy_primary(\"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfInsert: copy_primary(\"rect\")\n"
    "Shift Ctrl<KeyPress>osfInsert: copy_primary()\n"
    "Shift<KeyPress>osfInsert: paste_clipboard()\n"
    "Ctrl<KeyPress>osfInsert: copy_clipboard()\n"
    "~Shift ~Ctrl<KeyPress>osfInsert: set_overtype_mode()\n"

    // Cut/Copy/Paste
    "Shift Ctrl<KeyPress>osfCut: cut_primary()\n"
    "<KeyPress>osfCut: cut_clipboard()\n"
    "<KeyPress>osfCopy: copy_clipboard()\n"
    "<KeyPress>osfPaste: paste_clipboard()\n"
    "<KeyPress>osfPrimaryPaste: copy_primary()\n"

    // BeginLine
    "Alt Shift Ctrl<KeyPress>osfBeginLine: beginning_of_file(\"extend\", \"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfBeginLine: beginning_of_file(\"extend\" \"rect\")\n"
    "Alt Shift<KeyPress>osfBeginLine: beginning_of_line(\"extend\", \"rect\")\n"
    "Meta Shift<KeyPress>osfBeginLine: beginning_of_line(\"extend\", \"rect\")\n"
    "Shift Ctrl<KeyPress>osfBeginLine: beginning_of_file(\"extend\")\n"
    "Ctrl<KeyPress>osfBeginLine: beginning_of_file()\n"
    "Shift<KeyPress>osfBeginLine: beginning_of_line(\"extend\")\n"
    "~Alt~Shift~Ctrl~Meta<KeyPress>osfBeginLine: beginning_of_line()\n"

    // EndLine
    "Alt Shift Ctrl<KeyPress>osfEndLine: end_of_file(\"extend\", \"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfEndLine: end_of_file(\"extend\", \"rect\")\n"
    "Alt Shift<KeyPress>osfEndLine: end_of_line(\"extend\", \"rect\")\n"
    "Meta Shift<KeyPress>osfEndLine: end_of_line(\"extend\", \"rect\")\n"
    "Shift Ctrl<KeyPress>osfEndLine: end_of_file(\"extend\")\n"
    "Ctrl<KeyPress>osfEndLine: end_of_file()\n"
    "Shift<KeyPress>osfEndLine: end_of_line(\"extend\")\n"
    "~Alt~Shift~Ctrl~Meta<KeyPress>osfEndLine: end_of_line()\n"

    // Left
    "Alt Shift Ctrl<KeyPress>osfLeft: backward_word(\"extend\", \"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfLeft: backward_word(\"extend\", \"rect\")\n"
    "Alt Shift<KeyPress>osfLeft: key_select(\"left\", \"rect\")\n"
    "Meta Shift<KeyPress>osfLeft: key_select(\"left\", \"rect\")\n"
    "Shift Ctrl<KeyPress>osfLeft: backward_word(\"extend\")\n"
    "Ctrl<KeyPress>osfLeft: backward_word()\n"
    "Shift<KeyPress>osfLeft: key_select(\"left\")\n"
    "~Alt~Shift~Ctrl~Meta<KeyPress>osfLeft: backward_character()\n"

    // Right
    "Alt Shift Ctrl<KeyPress>osfRight: forward_word(\"extend\", \"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfRight: forward_word(\"extend\", \"rect\")\n"
    "Alt Shift<KeyPress>osfRight: key_select(\"right\", \"rect\")\n"
    "Meta Shift<KeyPress>osfRight: key_select(\"right\", \"rect\")\n"
    "Shift Ctrl<KeyPress>osfRight: forward_word(\"extend\")\n"
    "Ctrl<KeyPress>osfRight: forward_word()\n"
    "Shift<KeyPress>osfRight: key_select(\"right\")\n"
    "~Alt~Shift~Ctrl~Meta<KeyPress>osfRight: forward_character()\n"

    // Up
    "Alt Shift Ctrl<KeyPress>osfUp: backward_paragraph(\"extend\", \"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfUp: backward_paragraph(\"extend\", \"rect\")\n"
    "Alt Shift<KeyPress>osfUp: process_shift_up(\"rect\")\n"
    "Meta Shift<KeyPress>osfUp: process_shift_up(\"rect\")\n"
    "Shift Ctrl<KeyPress>osfUp: backward_paragraph(\"extend\")\n"
    "Ctrl<KeyPress>osfUp: backward_paragraph()\n"
    "Shift<KeyPress>osfUp: process_shift_up()\n"
    "~Alt~Shift~Ctrl~Meta<KeyPress>osfUp: process_up()\n"

    // Down
    "Alt Shift Ctrl<KeyPress>osfDown: forward_paragraph(\"extend\", \"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfDown: forward_paragraph(\"extend\", \"rect\")\n"
    "Alt Shift<KeyPress>osfDown: process_shift_down(\"rect\")\n"
    "Meta Shift<KeyPress>osfDown: process_shift_down(\"rect\")\n"
    "Shift Ctrl<KeyPress>osfDown: forward_paragraph(\"extend\")\n"
    "Ctrl<KeyPress>osfDown: forward_paragraph()\n"
    "Shift<KeyPress>osfDown: process_shift_down()\n"
    "~Alt~Shift~Ctrl~Meta<KeyPress>osfDown: process_down()\n"

    // PageUp
    "Alt Shift Ctrl<KeyPress>osfPageUp: page_left(\"extend\", \"rect\")\n"
    "Meta Shift Ctrl<KeyPress>osfPageUp: page_left(\"extend\", \"rect\")\n"
    "Alt Shift<KeyPress>osfPageUp: previous_page(\"extend\", \"rect\")\n"
    "Meta Shift<KeyPress>osfPageUp: previous_page(\"extend\", \"rect\")\n"
    "Shift Ctrl<KeyPress>osfPageUp: page_left(\"extend\")\n"
    "Ctrl<KeyPress>osfPageUp: previous_document()\n"
    "Shift<KeyPress>osfPageUp: previous_page(\"extend\")\n"
    "~Alt ~Shift ~Ctrl ~Meta<KeyPress>osfPageUp: previous_page()\n"

    // PageDown
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

    // PageLeft
    "Alt Shift<KeyPress>osfPageLeft: page_left(\"extend\", \"rect\")\n"
    "Meta Shift<KeyPress>osfPageLeft: page_left(\"extend\", \"rect\")\n"
    "Shift<KeyPress>osfPageLeft: page_left(\"extend\")\n"
    "~Alt ~Shift ~Ctrl ~Meta<KeyPress>osfPageLeft: page_left()\n"

    // PageRight
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
    // Support for mouse wheel in XFree86
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
	{ XmNhighlightThickness,       XmCHighlightThickness,       XmRDimension,  sizeof(Dimension),     XtOffset(TextWidget, primitive.highlight_thickness),  XmRInt,    0                                         },
	{ XmNshadowThickness,          XmCShadowThickness,          XmRDimension,  sizeof(Dimension),     XtOffset(TextWidget, primitive.shadow_thickness),     XmRInt,    0                                         },
	{ textNfont,                   textCFont,                   XmRFontStruct, sizeof(XFontStruct *), XtOffset(TextWidget, text.P_fontStruct),              XmRString, (String) "fixed"                          },
	{ textNselectForeground,       textCSelectForeground,       XmRPixel,      sizeof(Pixel),         XtOffset(TextWidget, text.P_selectFGPixel),           XmRString, (String)NEDIT_DEFAULT_SEL_FG              },
	{ textNselectBackground,       textCSelectBackground,       XmRPixel,      sizeof(Pixel),         XtOffset(TextWidget, text.P_selectBGPixel),           XmRString, (String)NEDIT_DEFAULT_SEL_BG              },
	{ textNhighlightForeground,    textCHighlightForeground,    XmRPixel,      sizeof(Pixel),         XtOffset(TextWidget, text.P_highlightFGPixel),        XmRString, (String)NEDIT_DEFAULT_HI_FG               },
	{ textNhighlightBackground,    textCHighlightBackground,    XmRPixel,      sizeof(Pixel),         XtOffset(TextWidget, text.P_highlightBGPixel),        XmRString, (String)NEDIT_DEFAULT_HI_BG               },
	{ textNlineNumForeground,      textCLineNumForeground,      XmRPixel,      sizeof(Pixel),         XtOffset(TextWidget, text.P_lineNumFGPixel),          XmRString, (String)NEDIT_DEFAULT_LINENO_FG           },
	{ textNcursorForeground,       textCCursorForeground,       XmRPixel,      sizeof(Pixel),         XtOffset(TextWidget, text.P_cursorFGPixel),           XmRString, (String)NEDIT_DEFAULT_CURSOR_FG           },
	{ textNcalltipForeground,      textCcalltipForeground,      XmRPixel,      sizeof(Pixel),         XtOffset(TextWidget, text.P_calltipFGPixel),          XmRString, (String)NEDIT_DEFAULT_CALLTIP_FG          },
	{ textNcalltipBackground,      textCcalltipBackground,      XmRPixel,      sizeof(Pixel),         XtOffset(TextWidget, text.P_calltipBGPixel),          XmRString, (String)NEDIT_DEFAULT_CALLTIP_BG          },
	{ textNbacklightCharTypes,     textCBacklightCharTypes,     XmRString,     sizeof(XmString),      XtOffset(TextWidget, text.P_backlightCharTypes),      XmRString, nullptr                                   },
	{ textNrows,                   textCRows,                   XmRInt,        sizeof(int),           XtOffset(TextWidget, text.P_rows),                    XmRString, (String) "24"                             },
	{ textNcolumns,                textCColumns,                XmRInt,        sizeof(int),           XtOffset(TextWidget, text.P_columns),                 XmRString, (String) "80"                             },
	{ textNmarginWidth,            textCMarginWidth,            XmRInt,        sizeof(int),           XtOffset(TextWidget, text.P_marginWidth),             XmRString, (String) "5"                              },
	{ textNmarginHeight,           textCMarginHeight,           XmRInt,        sizeof(int),           XtOffset(TextWidget, text.P_marginHeight),            XmRString, (String) "5"                              },
	{ textNpendingDelete,          textCPendingDelete,          XmRBoolean,    sizeof(Boolean),       XtOffset(TextWidget, text.P_pendingDelete),           XmRString, (String) "True"                           },
	{ textNautoWrap,               textCAutoWrap,               XmRBoolean,    sizeof(Boolean),       XtOffset(TextWidget, text.P_autoWrap),                XmRString, (String) "True"                           },
	{ textNcontinuousWrap,         textCContinuousWrap,         XmRBoolean,    sizeof(Boolean),       XtOffset(TextWidget, text.P_continuousWrap),          XmRString, (String) "True"                           },
	{ textNautoIndent,             textCAutoIndent,             XmRBoolean,    sizeof(Boolean),       XtOffset(TextWidget, text.P_autoIndent),              XmRString, (String) "True"                           },
	{ textNsmartIndent,            textCSmartIndent,            XmRBoolean,    sizeof(Boolean),       XtOffset(TextWidget, text.P_smartIndent),             XmRString, (String) "False"                          },
	{ textNoverstrike,             textCOverstrike,             XmRBoolean,    sizeof(Boolean),       XtOffset(TextWidget, text.P_overstrike),              XmRString, (String) "False"                          },
	{ textNheavyCursor,            textCHeavyCursor,            XmRBoolean,    sizeof(Boolean),       XtOffset(TextWidget, text.P_heavyCursor),             XmRString, (String) "False"                          },
	{ textNreadOnly,               textCReadOnly,               XmRBoolean,    sizeof(Boolean),       XtOffset(TextWidget, text.P_readOnly),                XmRString, (String) "False"                          },
	{ textNhidePointer,            textCHidePointer,            XmRBoolean,    sizeof(Boolean),       XtOffset(TextWidget, text.P_hidePointer),             XmRString, (String) "False"                          },
	{ textNwrapMargin,             textCWrapMargin,             XmRInt,        sizeof(int),           XtOffset(TextWidget, text.P_wrapMargin),              XmRString, (String) "0"                              },
	{ textNhScrollBar,             textCHScrollBar,             XmRWidget,     sizeof(Widget),        XtOffset(TextWidget, text.P_hScrollBar),              XmRString, (String) ""                               },
	{ textNvScrollBar,             textCVScrollBar,             XmRWidget,     sizeof(Widget),        XtOffset(TextWidget, text.P_vScrollBar),              XmRString, (String) ""                               },
	{ textNlineNumCols,            textCLineNumCols,            XmRInt,        sizeof(int),           XtOffset(TextWidget, text.P_lineNumCols),             XmRString, (String) "0"                              },
	{ textNautoShowInsertPos,      textCAutoShowInsertPos,      XmRBoolean,    sizeof(Boolean),       XtOffset(TextWidget, text.P_autoShowInsertPos),       XmRString, (String) "True"                           },
	{ textNautoWrapPastedText,     textCAutoWrapPastedText,     XmRBoolean,    sizeof(Boolean),       XtOffset(TextWidget, text.P_autoWrapPastedText),      XmRString, (String) "False"                          },
	{ textNwordDelimiters,         textCWordDelimiters,         XmRString,     sizeof(char *),        XtOffset(TextWidget, text.P_delimiters),              XmRString, (String) ".,/\\`'!@#%^&*()-=+{}[]\":;<>?" },
	{ textNblinkRate,              textCBlinkRate,              XmRInt,        sizeof(int),           XtOffset(TextWidget, text.P_cursorBlinkRate),         XmRString, (String) "500"                            },
	{ textNemulateTabs,            textCEmulateTabs,            XmRInt,        sizeof(int),           XtOffset(TextWidget, text.P_emulateTabs),             XmRString, (String) "0"                              },
	{ textNcursorVPadding,         textCCursorVPadding,         XtRCardinal,   sizeof(Cardinal),      XtOffset(TextWidget, text.P_cursorVPadding),          XmRString, (String) "0"                              },	
	{ textNfocusCallback,          textCFocusCallback,          XmRCallback,   sizeof(XtPointer),     XtOffset(TextWidget, text.P_focusInCB),               XtRCallback, nullptr								 },		
	{ textNlosingFocusCallback,    textCLosingFocusCallback,    XmRCallback,   sizeof(XtPointer),     XtOffset(TextWidget, text.P_focusOutCB),              XtRCallback, nullptr								 },		
	{ textNcursorMovementCallback, textCCursorMovementCallback, XmRCallback,   sizeof(XtPointer),     XtOffset(TextWidget, text.P_cursorCB),                XtRCallback, nullptr								 },		
	{ textNdragStartCallback,      textCDragStartCallback,      XmRCallback,   sizeof(XtPointer),     XtOffset(TextWidget, text.P_dragStartCB),             XtRCallback, nullptr								 },		
	{ textNdragEndCallback,        textCDragEndCallback,        XmRCallback,   sizeof(XtPointer),     XtOffset(TextWidget, text.P_dragEndCB),               XtRCallback, nullptr								 },		
	{ textNsmartIndentCallback,    textCSmartIndentCallback,    XmRCallback,   sizeof(XtPointer),     XtOffset(TextWidget, text.P_smartIndentCB),           XtRCallback, nullptr								 },	
};

static TextClassRec textClassRec = {
	// CoreClassPart
	{
		(WidgetClass)&xmPrimitiveClassRec, // superclass
		(String) "Text",                   // class_name
		sizeof(TextRec),                   // widget_size
		nullptr,                           // class_initialize
		nullptr,                           // class_part_initialize
		FALSE,                             // class_inited
		initialize,                        // initialize
		nullptr,                           // initialize_hook
		realize,                           // realize
		actionsList,                       // actions
		XtNumber(actionsList),             // num_actions
		resources,                         // resources
		XtNumber(resources),               // num_resources
		NULLQUARK,                         // xrm_class
		true,                              // compress_motion
		true,                              // compress_exposure
		true,                              // compress_enterleave
		false,                             // visible_interest
		(XtWidgetProc)destroy,             // destroy
		(XtWidgetProc)resize,              // resize
		(XtExposeProc)redisplay,           // expose
		(XtSetValuesFunc)setValues,        // set_values
		nullptr,                           // set_values_hook
		XtInheritSetValuesAlmost,          // set_values_almost
		nullptr,                           // get_values_hook
		nullptr,                           // accept_focus
		XtVersion,                         // version
		nullptr,                           // callback private
		defaultTranslations,               // tm_table
		queryGeometry,                     // query_geometry
		nullptr,                           // display_accelerator
		nullptr,                           // extension
	},
	// Motif primitive class fields
	{
		(XtWidgetProc)_XtInherit,           // Primitive border_highlight
		(XtWidgetProc)_XtInherit,           // Primitive border_unhighlight
		nullptr, /*XtInheritTranslations,*/ // translations
		nullptr,                            // arm_and_activate
		nullptr,                            // get resources
		0,                                  // num get_resources
		nullptr,                            // extension
	},
	// Text class part
	{
		0, // ignored
	}
};

WidgetClass textWidgetClass = reinterpret_cast<WidgetClass>(&textClassRec);

namespace {
const int NEDIT_HIDE_CURSOR_MASK = (KeyPressMask);
const int NEDIT_SHOW_CURSOR_MASK = (FocusChangeMask | PointerMotionMask | ButtonMotionMask | ButtonPressMask | ButtonReleaseMask);
char empty_bits[] = {0x00, 0x00, 0x00, 0x00};
Cursor empty_cursor = 0;

}

/*
** Widget initialize method
*/
static void initialize(Widget request, Widget newWidget, ArgList args, Cardinal *num_args) {

	(void)args;
	(void)num_args;

	auto new_widget = reinterpret_cast<TextWidget>(newWidget);
	auto& tw = text_of(new_widget);

	XFontStruct *fs = tw.P_fontStruct;
	int charWidth   = fs->max_bounds.width;
	int marginWidth = tw.P_marginWidth;
	int lineNumCols = tw.P_lineNumCols;

	// Set the initial window size based on the rows and columns resources
	if (request->core.width == 0) {
		new_widget->core.width = charWidth * tw.P_columns + marginWidth * 2 + (lineNumCols == 0 ? 0 : marginWidth + charWidth * lineNumCols);
	}

	if (request->core.height == 0) {
		new_widget->core.height = (fs->ascent + fs->descent) * tw.P_rows + tw.P_marginHeight * 2;
	}

	/* The default colors work for B&W as well as color, except for
	   selectFGPixel and selectBGPixel, where color highlighting looks
	   much better without reverse video, so if we get here, and the
	   selection is totally unreadable because of the bad default colors,
	   swap the colors and make the selection reverse video */
	Pixel white = WhitePixelOfScreen(XtScreen(newWidget));
	Pixel black = BlackPixelOfScreen(XtScreen(newWidget));

	if (tw.P_selectBGPixel == white && new_widget->core.background_pixel == white && tw.P_selectFGPixel == black && new_widget->primitive.foreground == black) {
		tw.P_selectBGPixel = black;
		tw.P_selectFGPixel = white;
	}

	/* Create the initial text buffer for the widget to display (which can
	   be replaced later with TextSetBuffer) */
	auto buf = new TextBuffer;

	// Create and initialize the text-display part of the widget
	int textLeft = tw.P_marginWidth + (lineNumCols == 0 ? 0 : marginWidth + charWidth * lineNumCols);

	auto textD = new TextDisplay(
		newWidget,
		tw.P_hScrollBar,
		tw.P_vScrollBar,
		textLeft,
		tw.P_marginHeight,
		new_widget->core.width - marginWidth - textLeft,
	    new_widget->core.height - tw.P_marginHeight * 2,
		lineNumCols == 0 ? 0 : marginWidth,
		lineNumCols == 0 ? 0 : lineNumCols * charWidth,
		buf,
		tw.P_fontStruct,
		new_widget->core.background_pixel,
	    new_widget->primitive.foreground,
		tw.P_selectFGPixel,
		tw.P_selectBGPixel,
		tw.P_highlightFGPixel,
		tw.P_highlightBGPixel,
		tw.P_cursorFGPixel,
	    tw.P_lineNumFGPixel,
		tw.P_continuousWrap,
		tw.P_wrapMargin,
		tw.P_backlightCharTypes,
		tw.P_calltipFGPixel,
		tw.P_calltipBGPixel);
		
	textD_of(new_widget) = textD;

	/* Add mandatory delimiters blank, tab, and newline to the list of
	   delimiters.  The memory use scheme here is that new values are
	   always copied, and can therefore be safely freed on subsequent
	   set-values calls or destroy */

	size_t n = strlen(tw.P_delimiters) + 4;
    char *delimiters = new char[n];
    snprintf(delimiters, n, "%s%s", " \t\n", tw.P_delimiters);
    tw.P_delimiters = delimiters;

	/* Start with the cursor blanked (widgets don't have focus on creation,
	   the initial FocusIn event will unblank it and get blinking started) */
	textD->cursorOn = false;

	// Initialize the widget variables
	textD->autoScrollProcID   = 0;
	textD->cursorBlinkProcID  = 0;
	textD->dragState          = NOT_CLICKED;
	textD->multiClickState    = NO_CLICKS;
	textD->lastBtnDown        = 0;
	textD->selectionOwner     = false;
	textD->motifDestOwner     = false;
	textD->emTabsBeforeCursor = 0;

	// Register the widget to the input manager
	XmImRegister(newWidget, 0);
	// In case some Resources for the IC need to be set, add them below
	XmImVaSetValues(newWidget, nullptr);

	XtAddEventHandler(newWidget, GraphicsExpose, true, (XtEventHandler)redisplayGE, nullptr);

	if (tw.P_hidePointer) {
		Pixmap empty_pixmap;
		XColor black_color;
		// Set up the empty Cursor
		if (empty_cursor == 0) {
			Display *theDisplay = XtDisplay(newWidget);
			empty_pixmap = XCreateBitmapFromData(theDisplay, RootWindowOfScreen(XtScreen(newWidget)), empty_bits, 1, 1);
			XParseColor(theDisplay, DefaultColormapOfScreen(XtScreen(newWidget)), "black", &black_color);
			empty_cursor = XCreatePixmapCursor(theDisplay, empty_pixmap, empty_pixmap, &black_color, &black_color, 0, 0);
		}

		// Add event handler to hide the pointer on keypresses
		XtAddEventHandler(newWidget, NEDIT_HIDE_CURSOR_MASK, false, handleHidePointer, nullptr);
	}
}

// Hide the pointer while the user is typing
static void handleHidePointer(Widget w, XtPointer unused, XEvent *event, Boolean *continue_to_dispatch) {
	(void)unused;
	(void)event;
	(void)continue_to_dispatch;

	auto textD = textD_of(w);
	textD->ShowHidePointer(true);
}

/*
** Widget destroy method
*/
static void destroy(TextWidget w) {

	/* Free the text display and possibly the attached buffer.  The buffer
	   is freed only if after removing all of the modify procs (by calling
	   StopHandlingXSelections and TextDFree) there are no modify procs
	   left */
	
	auto textD = textD_of(w);
	
	textD->StopHandlingXSelections();
	TextBuffer *buf = textD->textBuffer();
	
	
	if (buf->modifyProcs_.empty()) {
		delete buf;
	}

	if (textD->cursorBlinkProcID != 0) {
		XtRemoveTimeOut(textD->cursorBlinkProcID);
	}
	
	// NOTE(eteran): the delete was originally right below the TextBuffer line
	//               but I moved it here, so the above cursor stuff was valid
	//               maybe move that code to the textDisplay destructor? 
	delete textD;

	XtRemoveAllCallbacks((Widget)w, textNfocusCallback);
	XtRemoveAllCallbacks((Widget)w, textNlosingFocusCallback);
	XtRemoveAllCallbacks((Widget)w, textNcursorMovementCallback);
	XtRemoveAllCallbacks((Widget)w, textNdragStartCallback);
	XtRemoveAllCallbacks((Widget)w, textNdragEndCallback);

	// Unregister the widget from the input manager
	XmImUnregister((Widget)w);
	

}

/*
** Widget resize method.  Called when the size of the widget changes
*/
static void resize(TextWidget w) {
	XFontStruct *fs = text_of(w).P_fontStruct;
	int height = w->core.height, width = w->core.width;
	int marginWidth = text_of(w).P_marginWidth, marginHeight = text_of(w).P_marginHeight;
	int lineNumAreaWidth = text_of(w).P_lineNumCols == 0 ? 0 : text_of(w).P_marginWidth + fs->max_bounds.width * text_of(w).P_lineNumCols;

	text_of(w).P_columns = (width - marginWidth * 2 - lineNumAreaWidth) / fs->max_bounds.width;
	text_of(w).P_rows    = (height - marginHeight * 2) / (fs->ascent + fs->descent);

	/* Reject widths and heights less than a character, which the text
	   display can't tolerate.  This is not strictly legal, but I've seen
	   it done in other widgets and it seems to do no serious harm.  NEdit
	   prevents panes from getting smaller than one line, but sometimes
	   splitting windows on Linux 2.0 systems (same Motif, why the change in
	   behavior?), causes one or two resize calls with < 1 line of height.
	   Fixing it here is 100x easier than re-designing TextDisplay.c */
	if (text_of(w).P_columns < 1) {
		text_of(w).P_columns = 1;
		w->core.width = width = fs->max_bounds.width + marginWidth * 2 + lineNumAreaWidth;
	}
	if (text_of(w).P_rows < 1) {
		text_of(w).P_rows = 1;
		w->core.height = height = fs->ascent + fs->descent + marginHeight * 2;
	}

	// Resize the text display that the widget uses to render text
	textD_of(w)->TextDResize(width - marginWidth * 2 - lineNumAreaWidth, height - marginHeight * 2);

	/* if the window became shorter or narrower, there may be text left
	   in the bottom or right margin area, which must be cleaned up */
	if (XtIsRealized((Widget)w)) {
		XClearArea(XtDisplay(w), XtWindow(w), 0, height - marginHeight, width, marginHeight, false);
		XClearArea(XtDisplay(w), XtWindow(w), width - marginWidth, 0, marginWidth, height, false);
	}
}

/*
** Widget redisplay method
*/
static void redisplay(TextWidget w, XEvent *event, Region region) {

	(void)region;

	XExposeEvent *e = &event->xexpose;

	textD_of(w)->TextDRedisplayRect(e->x, e->y, e->width, e->height);
}

static void redisplayGE(TextWidget w, XtPointer client_data, XEvent *event, Boolean *continue_to_dispatch_return) {

	(void)client_data;
	(void)continue_to_dispatch_return;

	if (event->type == GraphicsExpose || event->type == NoExpose) {
		TextDisplay *textD = textD_of(w);	
		textD->HandleAllPendingGraphicsExposeNoExposeEvents(event);
	}
}

/*
** Widget setValues method
*/
static Boolean setValues(TextWidget current, TextWidget request, TextWidget new_widget) {

	(void)request;

	bool redraw = false;
	bool reconfigure = false;

	if (text_of(new_widget).P_overstrike != text_of(current).P_overstrike) {
		if (textD_of(current)->cursorStyle == BLOCK_CURSOR)
			textD_of(current)->TextDSetCursorStyle(text_of(current).P_heavyCursor ? HEAVY_CURSOR : NORMAL_CURSOR);
		else if (textD_of(current)->cursorStyle == NORMAL_CURSOR || textD_of(current)->cursorStyle == HEAVY_CURSOR)
			textD_of(current)->TextDSetCursorStyle(BLOCK_CURSOR);
	}

	if (text_of(new_widget).P_fontStruct != text_of(current).P_fontStruct) {
		
		if (text_of(new_widget).P_lineNumCols != 0) {
			reconfigure = true;
		}
		
		textD_of(current)->TextDSetFont(text_of(new_widget).P_fontStruct);
	}

	if (text_of(new_widget).P_wrapMargin != text_of(current).P_wrapMargin || text_of(new_widget).P_continuousWrap != text_of(current).P_continuousWrap)
		textD_of(current)->TextDSetWrapMode(text_of(new_widget).P_continuousWrap, text_of(new_widget).P_wrapMargin);

	/* When delimiters are changed, copy the memory, so that the caller
	   doesn't have to manage it, and add mandatory delimiters blank,
	   tab, and newline to the list */
	if (text_of(new_widget).P_delimiters != text_of(current).P_delimiters) {

		size_t n = strlen(text_of(new_widget).P_delimiters) + 4;
		char *delimiters = new char[n];
		delete [] text_of(current).P_delimiters;
		snprintf(delimiters, n, "%s%s", " \t\n", text_of(new_widget).P_delimiters);
		text_of(new_widget).P_delimiters = delimiters;
	}

	/* Setting the lineNumCols resource tells the text widget to hide or
	   show, or change the number of columns of the line number display,
	   which requires re-organizing the x coordinates of both the line
	   number display and the main text display */
	if (text_of(new_widget).P_lineNumCols != text_of(current).P_lineNumCols || reconfigure) {

		int marginWidth = text_of(new_widget).P_marginWidth;
		int charWidth   = text_of(new_widget).P_fontStruct->max_bounds.width;
		int lineNumCols = text_of(new_widget).P_lineNumCols;

		if (lineNumCols == 0) {
			textD_of(new_widget)->TextDSetLineNumberArea(0, 0, marginWidth);
			text_of(new_widget).P_columns = (new_widget->core.width - marginWidth * 2) / charWidth;
		} else {
			textD_of(new_widget)->TextDSetLineNumberArea(marginWidth, charWidth * lineNumCols, 2 * marginWidth + charWidth * lineNumCols);
			text_of(new_widget).P_columns = (new_widget->core.width - marginWidth * 3 - charWidth * lineNumCols) / charWidth;
		}
	}

	if (text_of(new_widget).P_backlightCharTypes != text_of(current).P_backlightCharTypes) {
		TextDisplay::TextDSetupBGClasses((Widget)new_widget, text_of(new_widget).P_backlightCharTypes, &textD_of(new_widget)->bgClassPixel, &textD_of(new_widget)->bgClass, textD_of(new_widget)->bgPixel);
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

	// Continue with realize method from superclass
	(xmPrimitiveClassRec.core_class.realize)(w, valueMask, attributes);
}

/*
** Widget query geometry method ... unless asked to negotiate a different size simply return current size.
*/
static XtGeometryResult queryGeometry(Widget w, XtWidgetGeometry *proposed, XtWidgetGeometry *answer) {
	auto tw = reinterpret_cast<TextWidget>(w);

	int curHeight    = tw->core.height;
	int curWidth     = tw->core.width;
	XFontStruct *fs  = textD_of(tw)->fontStruct;
	int fontWidth    = fs->max_bounds.width;
	int fontHeight   = fs->ascent + fs->descent;
	int marginHeight = text_of(tw).P_marginHeight;
	int propWidth    = (proposed->request_mode & CWWidth)  ? proposed->width  : 0;
	int propHeight   = (proposed->request_mode & CWHeight) ? proposed->height : 0;

	answer->request_mode = CWHeight | CWWidth;

	if (proposed->request_mode & CWWidth)
		// Accept a width no smaller than 10 chars
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
	TextDisplay *textD = textD_of(tw);
	Time lastBtnDown = textD->lastBtnDown;
	int row;
	int column;

	/* Indicate state for future events, PRIMARY_CLICKED indicates that
	   the proper initialization has been done for primary dragging and/or
	   multi-clicking.  Also record the timestamp for multi-click processing */
	textD->dragState = PRIMARY_CLICKED;
	textD->lastBtnDown = e->time;

	/* Become owner of the MOTIF_DESTINATION selection, making this widget
	   the designated recipient of secondary quick actions in Motif XmText
	   widgets and in other NEdit text widgets */
	textD->TakeMotifDestination(e->time);

	// Check for possible multi-click sequence in progress
	if (textD->multiClickState != NO_CLICKS) {
		if (e->time < lastBtnDown + XtGetMultiClickTime(XtDisplay(w))) {
			if (textD->multiClickState == ONE_CLICK) {
				selectWord(w, e->x);
				textD->callCursorMovementCBs(event);
				return;
			} else if (textD->multiClickState == TWO_CLICKS) {
				selectLine(w);
				textD->callCursorMovementCBs(event);
				return;
			} else if (textD->multiClickState == THREE_CLICKS) {
				textD->textBuffer()->BufSelect(0, textD->textBuffer()->BufGetLength());
				return;
			} else if (textD->multiClickState > THREE_CLICKS)
				textD->multiClickState = NO_CLICKS;
		} else
			textD->multiClickState = NO_CLICKS;
	}

	// Clear any existing selections
	textD->textBuffer()->BufUnselect();

	// Move the cursor to the pointer location
	moveDestinationAP(w, event, args, nArgs);

	/* Record the site of the initial button press and the initial character
	   position so subsequent motion events and clicking can decide when and
	   where to begin a primary selection */
	textD->btnDownCoord = Point{e->x, e->y};
	textD->anchor = textD->TextDGetInsertPosition();
	textD->TextDXYToUnconstrainedPosition(Point{e->x, e->y}, &row, &column);
	column = textD->TextDOffsetWrappedColumn(row, column);
	textD->rectAnchor = column;
}

static void moveDestinationAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;
	(void)event;

	XButtonEvent *e = &event->xbutton;
	auto textD = textD_of(w);

	// Get input focus
	XmProcessTraversal(w, XmTRAVERSE_CURRENT);

	// Move the cursor
	textD->TextDSetInsertPosition(textD->TextDXYToPosition(Point{e->x, e->y}));
	textD->checkAutoShowInsertPos();
	textD->callCursorMovementCBs(event);
}

static void extendAdjustAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;
	(void)event;

	auto tw = reinterpret_cast<TextWidget>(w);
	XMotionEvent *e = &event->xmotion;
	int dragState = textD_of(tw)->dragState;
	int rectDrag = hasKey("rect", args, nArgs);

	// Make sure the proper initialization was done on mouse down
	if (dragState != PRIMARY_DRAG && dragState != PRIMARY_CLICKED && dragState != PRIMARY_RECT_DRAG)
		return;

	/* If the selection hasn't begun, decide whether the mouse has moved
	   far enough from the initial mouse down to be considered a drag */
	if (textD_of(tw)->dragState == PRIMARY_CLICKED) {
		if (abs(e->x - textD_of(tw)->btnDownCoord.x) > SELECT_THRESHOLD || abs(e->y - textD_of(tw)->btnDownCoord.y) > SELECT_THRESHOLD)
			textD_of(tw)->dragState = rectDrag ? PRIMARY_RECT_DRAG : PRIMARY_DRAG;
		else
			return;
	}

	/* If "rect" argument has appeared or disappeared, keep dragState up
	   to date about which type of drag this is */
	textD_of(tw)->dragState = rectDrag ? PRIMARY_RECT_DRAG : PRIMARY_DRAG;

	/* Record the new position for the autoscrolling timer routine, and
	   engage or disengage the timer if the mouse is in/out of the window */
	checkAutoScroll(tw, e->x, e->y);

	// Adjust the selection and move the cursor
	adjustSelection(tw, e->x, e->y);
}

static void extendStartAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	XMotionEvent *e = &event->xmotion;
	auto textD = textD_of(w);
	TextBuffer *buf = textD->buffer;
	TextSelection *sel = &buf->primary_;
	int anchor, rectAnchor, anchorLineStart, newPos, row, column;

	// Find the new anchor point for the rest of this drag operation
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
	textD_of(w)->anchor = anchor;
	textD_of(w)->rectAnchor = rectAnchor;

	// Make the new selection
	if (hasKey("rect", args, nArgs))
		buf->BufRectSelect(buf->BufStartOfLine(std::min<int>(anchor, newPos)), buf->BufEndOfLine(std::max<int>(anchor, newPos)), std::min<int>(rectAnchor, column), std::max<int>(rectAnchor, column));
	else
		buf->BufSelect(std::min<int>(anchor, newPos), std::max<int>(anchor, newPos));

	/* Never mind the motion threshold, go right to dragging since
	   extend-start is unambiguously the start of a selection */
	textD_of(w)->dragState = PRIMARY_DRAG;

	// Don't do by-word or by-line adjustment, just by character
	textD_of(w)->multiClickState = NO_CLICKS;

	// Move the cursor
	textD->TextDSetInsertPosition(newPos);
	textD->callCursorMovementCBs(event);
}

static void extendEndAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;
	(void)event;

	XButtonEvent *e = &event->xbutton;
	auto tw = reinterpret_cast<TextWidget>(w);

	if (textD_of(tw)->dragState == PRIMARY_CLICKED && textD_of(tw)->lastBtnDown <= e->time + XtGetMultiClickTime(XtDisplay(w)))
		textD_of(tw)->multiClickState++;
	endDrag(w);
}

static void processCancelAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;
	(void)event;

	int dragState = textD_of(w)->dragState;
	auto textD = textD_of(w);
	TextBuffer *buf = textD->buffer;
	
	// If there's a calltip displayed, kill it.
	textD->TextDKillCalltip(0);

	if (dragState == PRIMARY_DRAG || dragState == PRIMARY_RECT_DRAG) {
		buf->BufUnselect();
	}
	
	textD->cancelDrag();
}

static void secondaryStartAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	XMotionEvent *e = &event->xmotion;
	auto textD = textD_of(w);
	TextBuffer *buf = textD->buffer;
	TextSelection *sel = &buf->secondary_;
	int anchor, pos, row, column;

	// Find the new anchor point and make the new selection
	pos = textD->TextDXYToPosition(Point{e->x, e->y});
	if (sel->selected) {
		if (abs(pos - sel->start) < abs(pos - sel->end))
			anchor = sel->end;
		else
			anchor = sel->start;
		buf->BufSecondarySelect(anchor, pos);
	} else {
		anchor = pos;
	}

	/* Record the site of the initial button press and the initial character
	   position so subsequent motion events can decide when to begin a
	   selection, (and where the selection began) */
	textD_of(w)->btnDownCoord = Point{e->x, e->y};
	textD_of(w)->anchor       = pos;

	textD->TextDXYToUnconstrainedPosition(Point{e->x, e->y}, &row, &column);

	column = textD->TextDOffsetWrappedColumn(row, column);
	textD_of(w)->rectAnchor = column;
	textD_of(w)->dragState = SECONDARY_CLICKED;
}

static void secondaryOrDragStartAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	XMotionEvent *e = &event->xmotion;
	auto textD = textD_of(w);
	TextBuffer *buf = textD->buffer;

	/* If the click was outside of the primary selection, this is not
	   a drag, start a secondary selection */
	if (!buf->primary_.selected || !textD->TextDInSelection(Point{e->x, e->y})) {
		secondaryStartAP(w, event, args, nArgs);
		return;
	}

	if (textD->checkReadOnly())
		return;

	/* Record the site of the initial button press and the initial character
	   position so subsequent motion events can decide when to begin a
	   drag, and where to drag to */
	textD_of(w)->btnDownCoord = Point{e->x, e->y};
	textD_of(w)->dragState    = CLICKED_IN_SELECTION;
}

static void secondaryAdjustAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	auto tw = reinterpret_cast<TextWidget>(w);
	XMotionEvent *e = &event->xmotion;
	int dragState = textD_of(tw)->dragState;
	int rectDrag = hasKey("rect", args, nArgs);

	// Make sure the proper initialization was done on mouse down
	if (dragState != SECONDARY_DRAG && dragState != SECONDARY_RECT_DRAG && dragState != SECONDARY_CLICKED)
		return;

	/* If the selection hasn't begun, decide whether the mouse has moved
	   far enough from the initial mouse down to be considered a drag */
	if (textD_of(tw)->dragState == SECONDARY_CLICKED) {
		if (abs(e->x - textD_of(tw)->btnDownCoord.x) > SELECT_THRESHOLD || abs(e->y - textD_of(tw)->btnDownCoord.y) > SELECT_THRESHOLD)
			textD_of(tw)->dragState = rectDrag ? SECONDARY_RECT_DRAG : SECONDARY_DRAG;
		else
			return;
	}

	/* If "rect" argument has appeared or disappeared, keep dragState up
	   to date about which type of drag this is */
	textD_of(tw)->dragState = rectDrag ? SECONDARY_RECT_DRAG : SECONDARY_DRAG;

	/* Record the new position for the autoscrolling timer routine, and
	   engage or disengage the timer if the mouse is in/out of the window */
	checkAutoScroll(tw, e->x, e->y);

	// Adjust the selection
	adjustSecondarySelection(tw, e->x, e->y);
}

static void secondaryOrDragAdjustAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	auto tw = reinterpret_cast<TextWidget>(w);
	TextDisplay *textD = textD_of(tw);
	
	XMotionEvent *e = &event->xmotion;
	int dragState = textD_of(tw)->dragState;

	/* Only dragging of blocks of text is handled in this action proc.
	   Otherwise, defer to secondaryAdjust to handle the rest */
	if (dragState != CLICKED_IN_SELECTION && dragState != PRIMARY_BLOCK_DRAG) {
		secondaryAdjustAP(w, event, args, nArgs);
		return;
	}

	/* Decide whether the mouse has moved far enough from the
	   initial mouse down to be considered a drag */
	if (textD_of(tw)->dragState == CLICKED_IN_SELECTION) {
		if (abs(e->x - textD_of(tw)->btnDownCoord.x) > SELECT_THRESHOLD || abs(e->y - textD_of(tw)->btnDownCoord.y) > SELECT_THRESHOLD) {
			textD->BeginBlockDrag();
		} else {
			return;
		}
	}

	/* Record the new position for the autoscrolling timer routine, and
	   engage or disengage the timer if the mouse is in/out of the window */
	checkAutoScroll(tw, e->x, e->y);

	// Adjust the selection
	textD->BlockDragSelection(
		Point{e->x, e->y}, 
		hasKey("overlay", args, nArgs) ? 
			(hasKey("copy", args, nArgs) ? DRAG_OVERLAY_COPY : DRAG_OVERLAY_MOVE) : 
			(hasKey("copy", args, nArgs) ? DRAG_COPY         : DRAG_MOVE));
}

static void copyToAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	XButtonEvent *e = &event->xbutton;
	auto tw = reinterpret_cast<TextWidget>(w);
	TextDisplay *textD = textD_of(tw);
	int dragState = textD_of(tw)->dragState;
	TextBuffer *buf = textD->buffer;
	TextSelection *secondary = &buf->secondary_, *primary = &buf->primary_;
	int rectangular = secondary->rectangular;
	int insertPos, lineStart, column;

	endDrag(w);
	if (!((dragState == SECONDARY_DRAG && secondary->selected) || (dragState == SECONDARY_RECT_DRAG && secondary->selected) || dragState == SECONDARY_CLICKED || dragState == NOT_CLICKED))
		return;
	if (!(secondary->selected && !textD_of(w)->motifDestOwner)) {
		if (textD->checkReadOnly()) {
			buf->BufSecondaryUnselect();
			return;
		}
	}
	if (secondary->selected) {
		if (textD->motifDestOwner) {
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
				textD->TextInsertAtCursorEx(textToCopy, event, true, text_of(tw).P_autoWrapPastedText);

			buf->BufSecondaryUnselect();
			textD->TextDUnblankCursor();
		} else
			textD->SendSecondarySelection(e->time, false);
	} else if (primary->selected) {
		std::string textToCopy = buf->BufGetSelectionTextEx();
		textD->TextDSetInsertPosition(textD->TextDXYToPosition(Point{e->x, e->y}));
		textD->TextInsertAtCursorEx(textToCopy, event, false, text_of(tw).P_autoWrapPastedText);
	} else {
		textD->TextDSetInsertPosition(textD->TextDXYToPosition(Point{e->x, e->y}));
		textD->InsertPrimarySelection(e->time, false);
	}
}

static void copyToOrEndDragAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	int dragState = textD_of(w)->dragState;

	if (dragState != PRIMARY_BLOCK_DRAG) {
		copyToAP(w, event, args, nArgs);
		return;
	}
	
	textD_of(w)->FinishBlockDrag();
}

static void moveToAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	XButtonEvent *e = &event->xbutton;
	auto textD = textD_of(w);
	int dragState = textD_of(w)->dragState;
	TextBuffer *buf = textD->buffer;
	TextSelection *secondary = &buf->secondary_, *primary = &buf->primary_;
	int insertPos, rectangular = secondary->rectangular;
	int column, lineStart;

	endDrag(w);
	if (!((dragState == SECONDARY_DRAG && secondary->selected) || (dragState == SECONDARY_RECT_DRAG && secondary->selected) || dragState == SECONDARY_CLICKED || dragState == NOT_CLICKED))
		return;
	if (textD->checkReadOnly()) {
		buf->BufSecondaryUnselect();
		return;
	}

	if (secondary->selected) {
		if (textD_of(w)->motifDestOwner) {
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
				textD->TextInsertAtCursorEx(textToCopy, event, true, text_of(w).P_autoWrapPastedText);

			buf->BufRemoveSecSelect();
			buf->BufSecondaryUnselect();
		} else
			textD->SendSecondarySelection(e->time, true);
	} else if (primary->selected) {
		std::string textToCopy = buf->BufGetRangeEx(primary->start, primary->end);
		textD->TextDSetInsertPosition(textD->TextDXYToPosition(Point{e->x, e->y}));
		textD->TextInsertAtCursorEx(textToCopy, event, false, text_of(w).P_autoWrapPastedText);

		buf->BufRemoveSelected();
		buf->BufUnselect();
	} else {
		textD->TextDSetInsertPosition(textD->TextDXYToPosition(Point{e->x, e->y}));
		textD->MovePrimarySelection(e->time, false);
	}
}

static void moveToOrEndDragAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	int dragState = textD_of(w)->dragState;

	if (dragState != PRIMARY_BLOCK_DRAG) {
		moveToAP(w, event, args, nArgs);
		return;
	}

	textD_of(w)->FinishBlockDrag();
}

static void endDragAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;
	(void)event;

	if (textD_of(w)->dragState == PRIMARY_BLOCK_DRAG) {
		textD_of(w)->FinishBlockDrag();
	} else {
		endDrag(w);
	}
}

static void exchangeAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	XButtonEvent *e = &event->xbutton;
	auto textD = textD_of(w);
	TextBuffer *buf = textD->buffer;
	TextSelection *sec = &buf->secondary_, *primary = &buf->primary_;

	int newPrimaryStart, newPrimaryEnd, secWasRect;
	int dragState = textD_of(w)->dragState; // save before endDrag
	int silent = hasKey("nobell", args, nArgs);

	endDrag(w);
	if (textD->checkReadOnly())
		return;

	/* If there's no secondary selection here, or the primary and secondary
	   selection overlap, just beep and return */
	if (!sec->selected || (primary->selected && ((primary->start <= sec->start && primary->end > sec->start) || (sec->start <= primary->start && sec->end > primary->start)))) {
		buf->BufSecondaryUnselect();
		ringIfNecessary(silent);
		/* If there's no secondary selection, but the primary selection is
		   being dragged, we must not forget to finish the dragging.
		   Otherwise, modifications aren't recorded. */
		if (dragState == PRIMARY_BLOCK_DRAG)
			textD->FinishBlockDrag();
		return;
	}

	// if the primary selection is in another widget, use selection routines
	if (!primary->selected) {
		textD->ExchangeSelections(e->time);
		return;
	}

	// Both primary and secondary are in this widget, do the exchange here
	std::string primaryText = buf->BufGetSelectionTextEx();
	std::string secText     = buf->BufGetSecSelectTextEx();
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
	textD->checkAutoShowInsertPos();
}

static void copyPrimaryAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	XKeyEvent *e = &event->xkey;
	auto tw = reinterpret_cast<TextWidget>(w);
	TextDisplay *textD = textD_of(tw);
	TextBuffer *buf = textD->buffer;
	TextSelection *primary = &buf->primary_;
	int rectangular = hasKey("rect", args, nArgs);
	int insertPos, col;

	textD->cancelDrag();
	if (textD->checkReadOnly())
		return;
	if (primary->selected && rectangular) {
		std::string textToCopy = buf->BufGetSelectionTextEx();
		insertPos = textD->TextDGetInsertPosition();
		col = buf->BufCountDispChars(buf->BufStartOfLine(insertPos), insertPos);
		buf->BufInsertColEx(col, insertPos, textToCopy, nullptr, nullptr);
		textD->TextDSetInsertPosition(buf->cursorPosHint_);

		textD->checkAutoShowInsertPos();
	} else if (primary->selected) {
		std::string textToCopy = buf->BufGetSelectionTextEx();
		insertPos = textD->TextDGetInsertPosition();
		buf->BufInsertEx(insertPos, textToCopy);
		textD->TextDSetInsertPosition(insertPos + textToCopy.size());

		textD->checkAutoShowInsertPos();
	} else if (rectangular) {
		if (!textD->TextDPositionToXY(textD->TextDGetInsertPosition(), &textD_of(tw)->btnDownCoord.x, &textD_of(tw)->btnDownCoord.y)) {
			return; // shouldn't happen
		}
		
		textD->InsertPrimarySelection(e->time, true);
	} else {
		textD->InsertPrimarySelection(e->time, false);
	}
}

static void cutPrimaryAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	XKeyEvent *e = &event->xkey;
	auto textD = textD_of(w);
	TextBuffer *buf = textD->buffer;
	TextSelection *primary = &buf->primary_;

	int rectangular = hasKey("rect", args, nArgs);
	int insertPos, col;

	textD->cancelDrag();
	if (textD->checkReadOnly())
		return;
	if (primary->selected && rectangular) {
		std::string textToCopy = buf->BufGetSelectionTextEx();
		insertPos = textD->TextDGetInsertPosition();
		col = buf->BufCountDispChars(buf->BufStartOfLine(insertPos), insertPos);
		buf->BufInsertColEx(col, insertPos, textToCopy, nullptr, nullptr);
		textD->TextDSetInsertPosition(buf->cursorPosHint_);

		buf->BufRemoveSelected();
		textD->checkAutoShowInsertPos();
	} else if (primary->selected) {
		std::string textToCopy = buf->BufGetSelectionTextEx();
		insertPos = textD->TextDGetInsertPosition();
		buf->BufInsertEx(insertPos, textToCopy);
		textD->TextDSetInsertPosition(insertPos + textToCopy.size());

		buf->BufRemoveSelected();
		textD->checkAutoShowInsertPos();
	} else if (rectangular) {
		if (!textD->TextDPositionToXY(textD->TextDGetInsertPosition(), &textD_of(w)->btnDownCoord.x, &textD_of(w)->btnDownCoord.y))
			return; // shouldn't happen
		textD->MovePrimarySelection(e->time, true);
	} else {
		textD->MovePrimarySelection(e->time, false);
	}
}

static void mousePanAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	XButtonEvent *e = &event->xbutton;
	auto tw = reinterpret_cast<TextWidget>(w);
	TextDisplay *textD = textD_of(tw);
	int lineHeight = textD->ascent + textD->descent;
	int topLineNum, horizOffset;
	static Cursor panCursor = 0;

	if (textD_of(tw)->dragState == MOUSE_PAN) {
		textD->TextDSetScroll((textD_of(tw)->btnDownCoord.y - e->y + lineHeight / 2) / lineHeight, textD_of(tw)->btnDownCoord.x - e->x);
	} else if (textD_of(tw)->dragState == NOT_CLICKED) {
		textD->TextDGetScroll(&topLineNum, &horizOffset);
		textD_of(tw)->btnDownCoord.x = e->x + horizOffset;
		textD_of(tw)->btnDownCoord.y = e->y + topLineNum * lineHeight;
		textD_of(tw)->dragState = MOUSE_PAN;
		if (!panCursor)
			panCursor = XCreateFontCursor(XtDisplay(w), XC_fleur);
		XGrabPointer(XtDisplay(w), XtWindow(w), false, ButtonMotionMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, panCursor, CurrentTime);
	} else
		textD->cancelDrag();
}

static void pasteClipboardAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	auto tw = reinterpret_cast<TextWidget>(w);
	TextDisplay *textD = textD_of(tw);

	if (hasKey("rect", args, nArgs))
		textD->TextColPasteClipboard(event->xkey.time);
	else
		textD->TextPasteClipboard(event->xkey.time);
}

static void copyClipboardAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	auto textD = textD_of(w);
	textD->TextCopyClipboard(event->xkey.time);
}

static void cutClipboardAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	auto textD = textD_of(w);
	textD->TextCutClipboard(event->xkey.time);
}

static void insertStringAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	smartIndentCBStruct smartIndent;
	auto textD = textD_of(w);

	if (*nArgs == 0)
		return;
	textD->cancelDrag();
	if (textD->checkReadOnly())
		return;
	if (text_of(w).P_smartIndent) {
		smartIndent.reason = CHAR_TYPED;
		smartIndent.pos = textD->TextDGetInsertPosition();
		smartIndent.indentRequest = 0;
		smartIndent.charsTyped = args[0];
		XtCallCallbacks(w, textNsmartIndentCallback, &smartIndent);
	}
	textD->TextInsertAtCursorEx(args[0], event, true, true);
	textD->textBuffer()->BufUnselect();
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
	auto textD = textD_of(w);


	nChars = XmImMbLookupString(w, &event->xkey, chars, 19, &keysym, &status);
	if (nChars == 0 || status == XLookupNone || status == XLookupKeySym || status == XBufferOverflow)
		return;

	textD->cancelDrag();
	if (textD->checkReadOnly())
		return;
	textD->TakeMotifDestination(e->time);
	chars[nChars] = '\0';

	if (!window->buffer_->BufSubstituteNullChars(chars, nChars)) {
		QMessageBox::critical(nullptr /*parent*/, QLatin1String("Error"), QLatin1String("Too much binary data"));
		return;
	}

	/* If smart indent is on, call the smart indent callback to check the
	   inserted character */
	if (text_of(w).P_smartIndent) {
		smartIndent.reason        = CHAR_TYPED;
		smartIndent.pos           = textD->TextDGetInsertPosition();
		smartIndent.indentRequest = 0;
		smartIndent.charsTyped    = chars;
		XtCallCallbacks(w, textNsmartIndentCallback, &smartIndent);
	}
	textD->TextInsertAtCursorEx(chars, event, true, true);
	textD->textBuffer()->BufUnselect();
}

static void newlineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	if (text_of(w).P_autoIndent || text_of(w).P_smartIndent) {
		newlineAndIndentAP(w, event, args, nArgs);
	} else {
		newlineNoIndentAP(w, event, args, nArgs);
	}
}

static void newlineNoIndentAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	XKeyEvent *e = &event->xkey;
	
	auto textD = textD_of(w);

	textD->cancelDrag();
	if (textD->checkReadOnly())
		return;
	textD->TakeMotifDestination(e->time);
	simpleInsertAtCursorEx(w, "\n", event, true);
	textD_of(w)->textBuffer()->BufUnselect();
}

static void newlineAndIndentAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	XKeyEvent *e = &event->xkey;
	auto tw = reinterpret_cast<TextWidget>(w);
	TextDisplay *textD = textD_of(tw);
	TextBuffer *buf = textD->buffer;
	int column;

	if (textD->checkReadOnly()) {
		return;
	}
	
	textD->cancelDrag();
	textD->TakeMotifDestination(e->time);

	/* Create a string containing a newline followed by auto or smart
	   indent string */
	int cursorPos = textD->TextDGetInsertPosition();
	int lineStartPos = buf->BufStartOfLine(cursorPos);
	std::string indentStr = createIndentStringEx(tw, buf, 0, lineStartPos, cursorPos, nullptr, &column);

	// Insert it at the cursor
	simpleInsertAtCursorEx(w, indentStr, event, true);

	if (text_of(tw).P_emulateTabs > 0) {
		/*  If emulated tabs are on, make the inserted indent deletable by
		    tab. Round this up by faking the column a bit to the right to
		    let the user delete half-tabs with one keypress.  */
		column += text_of(tw).P_emulateTabs - 1;
		textD_of(tw)->emTabsBeforeCursor = column / text_of(tw).P_emulateTabs;
	}

	buf->BufUnselect();
}

static void processTabAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	auto textD = textD_of(w);
	TextBuffer *buf = textD->buffer;
	TextSelection *sel = &buf->primary_;
	int emTabDist = text_of(w).P_emulateTabs;
	int emTabsBeforeCursor = textD_of(w)->emTabsBeforeCursor;
	int insertPos, indent, startIndent, toIndent, lineStart, tabWidth;

	if (textD->checkReadOnly())
		return;
	textD->cancelDrag();
	textD->TakeMotifDestination(event->xkey.time);

	// If emulated tabs are off, just insert a tab
	if (emTabDist <= 0) {
		textD->TextInsertAtCursorEx("\t", event, true, true);
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

	// Allocate a buffer assuming all the inserted characters will be spaces
	std::string outStr;
	outStr.reserve(toIndent - startIndent);

	// Add spaces and tabs to outStr until it reaches toIndent
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

	// Insert the emulated tab
	textD->TextInsertAtCursorEx(outStr, event, true, true);

	// Restore and ++ emTabsBeforeCursor cleared by TextInsertAtCursorEx
	textD_of(w)->emTabsBeforeCursor = emTabsBeforeCursor + 1;

	buf->BufUnselect();
}

static void deleteSelectionAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	XKeyEvent *e = &event->xkey;

	auto textD = textD_of(w);

	textD->cancelDrag();
	if (textD->checkReadOnly())
		return;
	textD->TakeMotifDestination(e->time);
	deletePendingSelection(w, event);
}

static void deletePreviousCharacterAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	XKeyEvent *e = &event->xkey;
	auto textD = textD_of(w);
	int insertPos = textD->TextDGetInsertPosition();
	char c;
	int silent = hasKey("nobell", args, nArgs);

	textD->cancelDrag();
	if (textD->checkReadOnly())
		return;

	textD->TakeMotifDestination(e->time);
	if (deletePendingSelection(w, event))
		return;

	if (insertPos == 0) {
		ringIfNecessary(silent);
		return;
	}

	if (deleteEmulatedTab(w, event))
		return;

	if (text_of(w).P_overstrike) {
		c = textD->textBuffer()->BufGetCharacter(insertPos - 1);
		if (c == '\n')
			textD->textBuffer()->BufRemove(insertPos - 1, insertPos);
		else if (c != '\t')
			textD->textBuffer()->BufReplaceEx(insertPos - 1, insertPos, " ");
	} else {
		textD->textBuffer()->BufRemove(insertPos - 1, insertPos);
	}

	textD->TextDSetInsertPosition(insertPos - 1);
	textD->checkAutoShowInsertPos();
	textD->callCursorMovementCBs(event);
}

static void deleteNextCharacterAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	XKeyEvent *e = &event->xkey;
	auto textD = textD_of(w);
	int insertPos = textD->TextDGetInsertPosition();
	int silent = hasKey("nobell", args, nArgs);

	textD->cancelDrag();
	if (textD->checkReadOnly())
		return;
	textD->TakeMotifDestination(e->time);
	if (deletePendingSelection(w, event))
		return;
	if (insertPos == textD->textBuffer()->BufGetLength()) {
		ringIfNecessary(silent);
		return;
	}
	textD->textBuffer()->BufRemove(insertPos, insertPos + 1);
	textD->checkAutoShowInsertPos();
	textD->callCursorMovementCBs(event);
}

static void deletePreviousWordAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	XKeyEvent *e = &event->xkey;
	auto textD = textD_of(w);
	int insertPos = textD->TextDGetInsertPosition();
	int pos, lineStart = textD->textBuffer()->BufStartOfLine(insertPos);
	const char *delimiters = text_of(w).P_delimiters;
	int silent = hasKey("nobell", args, nArgs);

	textD->cancelDrag();
	if (textD->checkReadOnly()) {
		return;
	}

	textD->TakeMotifDestination(e->time);
	if (deletePendingSelection(w, event)) {
		return;
	}

	if (insertPos == lineStart) {
		ringIfNecessary(silent);
		return;
	}

	pos = std::max<int>(insertPos - 1, 0);
	while (strchr(delimiters, textD->textBuffer()->BufGetCharacter(pos)) != nullptr && pos != lineStart) {
		pos--;
	}

	pos = startOfWord(reinterpret_cast<TextWidget>(w), pos);
	textD->textBuffer()->BufRemove(pos, insertPos);
	textD->checkAutoShowInsertPos();
	textD->callCursorMovementCBs(event);
}

static void deleteNextWordAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	XKeyEvent *e = &event->xkey;
	auto textD = textD_of(w);
	int insertPos = textD->TextDGetInsertPosition();
	int pos, lineEnd = textD->textBuffer()->BufEndOfLine(insertPos);
	const char *delimiters = text_of(w).P_delimiters;
	int silent = hasKey("nobell", args, nArgs);

	textD->cancelDrag();
	if (textD->checkReadOnly()) {
		return;
	}

	textD->TakeMotifDestination(e->time);
	if (deletePendingSelection(w, event)) {
		return;
	}

	if (insertPos == lineEnd) {
		ringIfNecessary(silent);
		return;
	}

	pos = insertPos;
	while (strchr(delimiters, textD->textBuffer()->BufGetCharacter(pos)) != nullptr && pos != lineEnd) {
		pos++;
	}

	pos = endOfWord(reinterpret_cast<TextWidget>(w), pos);
	textD->textBuffer()->BufRemove(insertPos, pos);
	textD->checkAutoShowInsertPos();
	textD->callCursorMovementCBs(event);
}

static void deleteToEndOfLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	XKeyEvent *e = &event->xkey;
	auto textD = textD_of(w);
	int insertPos = textD->TextDGetInsertPosition();
	int endOfLine;
	int silent = 0;

	silent = hasKey("nobell", args, nArgs);
	if (hasKey("absolute", args, nArgs))
		endOfLine = textD->textBuffer()->BufEndOfLine(insertPos);
	else
		endOfLine = textD->TextDEndOfLine(insertPos, false);
	textD->cancelDrag();
	if (textD->checkReadOnly())
		return;
	textD->TakeMotifDestination(e->time);
	if (deletePendingSelection(w, event))
		return;
	if (insertPos == endOfLine) {
		ringIfNecessary(silent);
		return;
	}
	textD->textBuffer()->BufRemove(insertPos, endOfLine);
	textD->checkAutoShowInsertPos();
	textD->callCursorMovementCBs(event);
}

static void deleteToStartOfLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	XKeyEvent *e = &event->xkey;
	auto textD = textD_of(w);
	int insertPos = textD->TextDGetInsertPosition();
	int startOfLine;
	int silent = 0;

	silent = hasKey("nobell", args, nArgs);
	if (hasKey("wrap", args, nArgs))
		startOfLine =textD->TextDStartOfLine(insertPos);
	else
		startOfLine = textD->textBuffer()->BufStartOfLine(insertPos);
	textD->cancelDrag();
	if (textD->checkReadOnly())
		return;
	textD->TakeMotifDestination(e->time);
	if (deletePendingSelection(w, event))
		return;
	if (insertPos == startOfLine) {
		ringIfNecessary(silent);
		return;
	}
	textD->textBuffer()->BufRemove(startOfLine, insertPos);
	textD->checkAutoShowInsertPos();
	textD->callCursorMovementCBs(event);
}

static void forwardCharacterAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	auto textD = textD_of(w);
	int insertPos = textD_of(w)->TextDGetInsertPosition();
	int silent = hasKey("nobell", args, nArgs);

	textD->cancelDrag();
	if (!textD_of(w)->TextDMoveRight()) {
		ringIfNecessary(silent);
	}
	checkMoveSelectionChange(w, event, insertPos, args, nArgs);
	textD->checkAutoShowInsertPos();
	textD->callCursorMovementCBs(event);
}

static void backwardCharacterAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	auto textD = textD_of(w);
	int insertPos = textD_of(w)->TextDGetInsertPosition();
	int silent = hasKey("nobell", args, nArgs);

	textD->cancelDrag();
	if (!textD_of(w)->TextDMoveLeft()) {
		ringIfNecessary(silent);
	}
	checkMoveSelectionChange(w, event, insertPos, args, nArgs);
	textD->checkAutoShowInsertPos();
	textD->callCursorMovementCBs(event);
}

static void forwardWordAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	auto textD = textD_of(w);
	TextBuffer *buf = textD->buffer;
	int pos, insertPos = textD->TextDGetInsertPosition();
	const char *delimiters = text_of(w).P_delimiters;
	int silent = hasKey("nobell", args, nArgs);

	textD->cancelDrag();
	if (insertPos == buf->BufGetLength()) {
		ringIfNecessary(silent);
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
	textD->checkAutoShowInsertPos();
	textD->callCursorMovementCBs(event);
}

static void backwardWordAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	auto textD = textD_of(w);
	TextBuffer *buf = textD->buffer;
	int pos, insertPos = textD->TextDGetInsertPosition();
	const char *delimiters = text_of(w).P_delimiters;
	int silent = hasKey("nobell", args, nArgs);

	textD->cancelDrag();
	if (insertPos == 0) {
		ringIfNecessary(silent);
		return;
	}
	pos = std::max<int>(insertPos - 1, 0);
	while (strchr(delimiters, buf->BufGetCharacter(pos)) != nullptr && pos > 0)
		pos--;
	pos = startOfWord(reinterpret_cast<TextWidget>(w), pos);

	textD->TextDSetInsertPosition(pos);
	checkMoveSelectionChange(w, event, insertPos, args, nArgs);
	textD->checkAutoShowInsertPos();
	textD->callCursorMovementCBs(event);
}

static void forwardParagraphAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	auto textD = textD_of(w);
	int pos, insertPos = textD->TextDGetInsertPosition();
	TextBuffer *buf = textD->buffer;
	char c;
	static char whiteChars[] = " \t";
	int silent = hasKey("nobell", args, nArgs);

	textD->cancelDrag();
	if (insertPos == buf->BufGetLength()) {
		ringIfNecessary(silent);
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
	textD->checkAutoShowInsertPos();
	textD->callCursorMovementCBs(event);
}

static void backwardParagraphAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	auto textD = textD_of(w);
	int parStart, pos, insertPos = textD->TextDGetInsertPosition();
	TextBuffer *buf = textD->buffer;
	char c;
	static char whiteChars[] = " \t";
	int silent = hasKey("nobell", args, nArgs);

	textD->cancelDrag();
	if (insertPos == 0) {
		ringIfNecessary(silent);
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
	textD->checkAutoShowInsertPos();
	textD->callCursorMovementCBs(event);
}

static void keySelectAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	auto textD = textD_of(w);
	int stat, insertPos = textD->TextDGetInsertPosition();
	int silent = hasKey("nobell", args, nArgs);

	textD->cancelDrag();
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
		ringIfNecessary(silent);
	} else {
		keyMoveExtendSelection(w, event, insertPos, hasKey("rect", args, nArgs));
		textD->checkAutoShowInsertPos();
		textD->callCursorMovementCBs(event);
	}
}

static void processUpAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	auto textD = textD_of(w);
	int insertPos = textD_of(w)->TextDGetInsertPosition();
	int silent = hasKey("nobell", args, nArgs);
	int abs    = hasKey("absolute", args, nArgs);

	textD->cancelDrag();
	if (!textD_of(w)->TextDMoveUp(abs))
		ringIfNecessary(silent);
	checkMoveSelectionChange(w, event, insertPos, args, nArgs);
	textD->checkAutoShowInsertPos();
	textD->callCursorMovementCBs(event);
}

static void processShiftUpAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	auto textD = textD_of(w);
	int insertPos = textD_of(w)->TextDGetInsertPosition();
	int silent = hasKey("nobell", args, nArgs);
	int abs    = hasKey("absolute", args, nArgs);

	textD->cancelDrag();
	if (!textD_of(w)->TextDMoveUp(abs))
		ringIfNecessary(silent);
	keyMoveExtendSelection(w, event, insertPos, hasKey("rect", args, nArgs));
	textD->checkAutoShowInsertPos();
	textD->callCursorMovementCBs(event);
}

static void processDownAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	auto textD = textD_of(w);
	int insertPos = textD_of(w)->TextDGetInsertPosition();
	int silent = hasKey("nobell", args, nArgs);
	int abs    = hasKey("absolute", args, nArgs);

	textD->cancelDrag();
	if (!textD_of(w)->TextDMoveDown(abs))
		ringIfNecessary(silent);
	checkMoveSelectionChange(w, event, insertPos, args, nArgs);
	textD->checkAutoShowInsertPos();
	textD->callCursorMovementCBs(event);
}

static void processShiftDownAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	auto textD = textD_of(w);
	int insertPos = textD_of(w)->TextDGetInsertPosition();
	int silent = hasKey("nobell", args, nArgs);
	int abs = hasKey("absolute", args, nArgs);

	textD->cancelDrag();
	if (!textD_of(w)->TextDMoveDown(abs))
		ringIfNecessary(silent);
	keyMoveExtendSelection(w, event, insertPos, hasKey("rect", args, nArgs));
	textD->checkAutoShowInsertPos();
	textD->callCursorMovementCBs(event);
}

static void beginningOfLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	auto textD = textD_of(w);
	int insertPos = textD->TextDGetInsertPosition();

	textD->cancelDrag();
	if (hasKey("absolute", args, nArgs))
		textD->TextDSetInsertPosition(textD->textBuffer()->BufStartOfLine(insertPos));
	else
		textD->TextDSetInsertPosition(textD->TextDStartOfLine(insertPos));
	checkMoveSelectionChange(w, event, insertPos, args, nArgs);
	textD->checkAutoShowInsertPos();
	textD->callCursorMovementCBs(event);
	textD->cursorPreferredCol = 0;
}

static void endOfLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	auto textD = textD_of(w);
	int insertPos = textD->TextDGetInsertPosition();

	textD->cancelDrag();
	if (hasKey("absolute", args, nArgs))
		textD->TextDSetInsertPosition(textD->textBuffer()->BufEndOfLine(insertPos));
	else
		textD->TextDSetInsertPosition(textD->TextDEndOfLine(insertPos, false));
	checkMoveSelectionChange(w, event, insertPos, args, nArgs);
	textD->checkAutoShowInsertPos();
	textD->callCursorMovementCBs(event);
	textD->cursorPreferredCol = -1;
}

static void beginningOfFileAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	int insertPos = textD_of(w)->TextDGetInsertPosition();
	auto textD = textD_of(w);

	textD->cancelDrag();
	if (hasKey("scrollbar", args, nArgs)) {
		if (textD->topLineNum != 1) {
			textD->TextDSetScroll(1, textD->horizOffset);
		}
	} else {
		textD_of(w)->TextDSetInsertPosition(0);
		checkMoveSelectionChange(w, event, insertPos, args, nArgs);
		textD->checkAutoShowInsertPos();
		textD->callCursorMovementCBs(event);
	}
}

static void endOfFileAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	auto textD = textD_of(w);
	int insertPos = textD->TextDGetInsertPosition();
	int lastTopLine;

	textD->cancelDrag();
	if (hasKey("scrollbar", args, nArgs)) {
		lastTopLine = std::max<int>(1, textD->getBufferLinesCount() - (textD->nVisibleLines - 2) + text_of(w).P_cursorVPadding);
		if (lastTopLine != textD->topLineNum) {
			textD->TextDSetScroll(lastTopLine, textD->horizOffset);
		}
	} else {
		textD->TextDSetInsertPosition(textD->textBuffer()->BufGetLength());
		checkMoveSelectionChange(w, event, insertPos, args, nArgs);
		textD->checkAutoShowInsertPos();
		textD->callCursorMovementCBs(event);
	}
}

static void nextPageAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	auto textD = textD_of(w);
	TextBuffer *buf = textD->buffer;
	int lastTopLine = std::max<int>(1, textD->getBufferLinesCount() - (textD->nVisibleLines - 2) + text_of(w).P_cursorVPadding);
	int insertPos = textD->TextDGetInsertPosition();
	int column = 0, visLineNum, lineStartPos;
	int pos, targetLine;
	int pageForwardCount = std::max<int>(1, textD->nVisibleLines - 1);
	int maintainColumn = 0;
	int silent = hasKey("nobell", args, nArgs);

	maintainColumn = hasKey("column", args, nArgs);
	textD->cancelDrag();
	if (hasKey("scrollbar", args, nArgs)) { // scrollbar only
		targetLine = std::min<int>(textD->topLineNum + pageForwardCount, lastTopLine);

		if (targetLine == textD->topLineNum) {
			ringIfNecessary(silent);
			return;
		}
		textD->TextDSetScroll(targetLine, textD->horizOffset);
	} else if (hasKey("stutter", args, nArgs)) { // Mac style
		// move to bottom line of visible area
		// if already there, page down maintaining preferrred column
		targetLine = std::max<int>(std::min<int>(textD->nVisibleLines - 1, textD->getBufferLinesCount()), 0);
		column = textD->TextDPreferredColumn(&visLineNum, &lineStartPos);
		if (lineStartPos == textD->lineStarts[targetLine]) {
			if (insertPos >= buf->BufGetLength() || textD->topLineNum == lastTopLine) {
				ringIfNecessary(silent);
				return;
			}
			targetLine = std::min<int>(textD->topLineNum + pageForwardCount, lastTopLine);
			pos = textD->TextDCountForwardNLines(insertPos, pageForwardCount, false);
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
				ringIfNecessary(silent);
				return;
			}
			if (maintainColumn) {
				pos = textD->TextDPosOfPreferredCol(column, pos);
			}
			textD->TextDSetInsertPosition(pos);
		}
		checkMoveSelectionChange(w, event, insertPos, args, nArgs);
		textD->checkAutoShowInsertPos();
		textD->callCursorMovementCBs(event);
		if (maintainColumn) {
			textD->cursorPreferredCol = column;
		} else {
			textD->cursorPreferredCol = -1;
		}
	} else { // "standard"
		if (insertPos >= buf->BufGetLength() && textD->topLineNum == lastTopLine) {
			ringIfNecessary(silent);
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
		pos = textD->TextDCountForwardNLines(insertPos, textD->nVisibleLines - 1, false);
		if (maintainColumn) {
			pos = textD->TextDPosOfPreferredCol(column, pos);
		}
		textD->TextDSetInsertPosition(pos);
		textD->TextDSetScroll(targetLine, textD->horizOffset);
		checkMoveSelectionChange(w, event, insertPos, args, nArgs);
		textD->checkAutoShowInsertPos();
		textD->callCursorMovementCBs(event);
		if (maintainColumn) {
			textD->cursorPreferredCol = column;
		} else {
			textD->cursorPreferredCol = -1;
		}
	}
}

static void previousPageAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	auto textD = textD_of(w);
	int insertPos = textD->TextDGetInsertPosition();
	int column = 0, visLineNum, lineStartPos;
	int pos, targetLine;
	int pageBackwardCount = std::max<int>(1, textD->nVisibleLines - 1);
	int maintainColumn = 0;
	int silent = hasKey("nobell", args, nArgs);

	maintainColumn = hasKey("column", args, nArgs);
	textD->cancelDrag();
	if (hasKey("scrollbar", args, nArgs)) { // scrollbar only
		targetLine = std::max<int>(textD->topLineNum - pageBackwardCount, 1);

		if (targetLine == textD->topLineNum) {
			ringIfNecessary(silent);
			return;
		}
		textD->TextDSetScroll(targetLine, textD->horizOffset);
	} else if (hasKey("stutter", args, nArgs)) { // Mac style
		// move to top line of visible area
		// if already there, page up maintaining preferrred column if required
		targetLine = 0;
		column = textD->TextDPreferredColumn(&visLineNum, &lineStartPos);
		if (lineStartPos == textD->lineStarts[targetLine]) {
			if (textD->topLineNum == 1 && (maintainColumn || column == 0)) {
				ringIfNecessary(silent);
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
		textD->checkAutoShowInsertPos();
		textD->callCursorMovementCBs(event);
		if (maintainColumn) {
			textD->cursorPreferredCol = column;
		} else {
			textD->cursorPreferredCol = -1;
		}
	} else { // "standard"
		if (insertPos <= 0 && textD->topLineNum == 1) {
			ringIfNecessary(silent);
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
		textD->checkAutoShowInsertPos();
		textD->callCursorMovementCBs(event);
		if (maintainColumn) {
			textD->cursorPreferredCol = column;
		} else {
			textD->cursorPreferredCol = -1;
		}
	}
}

static void pageLeftAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	auto textD = textD_of(w);
	TextBuffer *buf = textD->buffer;
	int insertPos = textD->TextDGetInsertPosition();
	int maxCharWidth = textD->fontStruct->max_bounds.width;
	int lineStartPos, indent, pos;
	int horizOffset;
	int silent = hasKey("nobell", args, nArgs);

	textD->cancelDrag();
	if (hasKey("scrollbar", args, nArgs)) {
		if (textD->horizOffset == 0) {
			ringIfNecessary(silent);
			return;
		}
		horizOffset = std::max<int>(0, textD->horizOffset - textD->rect.width);
		textD->TextDSetScroll(textD->topLineNum, horizOffset);
	} else {
		lineStartPos = buf->BufStartOfLine(insertPos);
		if (insertPos == lineStartPos && textD->horizOffset == 0) {
			ringIfNecessary(silent);
			return;
		}
		indent = buf->BufCountDispChars(lineStartPos, insertPos);
		pos = buf->BufCountForwardDispChars(lineStartPos, std::max<int>(0, indent - textD->rect.width / maxCharWidth));
		textD->TextDSetInsertPosition(pos);
		textD->TextDSetScroll(textD->topLineNum, std::max<int>(0, textD->horizOffset - textD->rect.width));
		checkMoveSelectionChange(w, event, insertPos, args, nArgs);
		textD->checkAutoShowInsertPos();
		textD->callCursorMovementCBs(event);
	}
}

static void pageRightAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;
	(void)event;

	auto textD = textD_of(w);
	TextBuffer *buf = textD->buffer;
	int insertPos = textD->TextDGetInsertPosition();
	int maxCharWidth = textD->fontStruct->max_bounds.width;
	int oldHorizOffset = textD->horizOffset;
	int lineStartPos, indent, pos;
	int horizOffset, sliderSize, sliderMax;
	int silent = hasKey("nobell", args, nArgs);

	textD->cancelDrag();
	if (hasKey("scrollbar", args, nArgs)) {
		XtVaGetValues(textD->hScrollBar, XmNmaximum, &sliderMax, XmNsliderSize, &sliderSize, nullptr);
		horizOffset = std::min<int>(textD->horizOffset + textD->rect.width, sliderMax - sliderSize);
		if (textD->horizOffset == horizOffset) {
			ringIfNecessary(silent);
			return;
		}
		textD->TextDSetScroll(textD->topLineNum, horizOffset);
	} else {
		lineStartPos = buf->BufStartOfLine(insertPos);
		indent = buf->BufCountDispChars(lineStartPos, insertPos);
		pos = buf->BufCountForwardDispChars(lineStartPos, indent + textD->rect.width / maxCharWidth);
		textD->TextDSetInsertPosition(pos);
		textD->TextDSetScroll(textD->topLineNum, textD->horizOffset + textD->rect.width);
		if (textD->horizOffset == oldHorizOffset && insertPos == pos)
			ringIfNecessary(silent);
		checkMoveSelectionChange(w, event, insertPos, args, nArgs);
		textD->checkAutoShowInsertPos();
		textD->callCursorMovementCBs(event);
	}
}

static void toggleOverstrikeAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;
	(void)event;

	auto tw = reinterpret_cast<TextWidget>(w);

	if (text_of(tw).P_overstrike) {
		text_of(tw).P_overstrike = false;
		textD_of(tw)->TextDSetCursorStyle(text_of(tw).P_heavyCursor ? HEAVY_CURSOR : NORMAL_CURSOR);
	} else {
		text_of(tw).P_overstrike = true;
		if (textD_of(tw)->cursorStyle == NORMAL_CURSOR || textD_of(tw)->cursorStyle == HEAVY_CURSOR)
			textD_of(tw)->TextDSetCursorStyle(BLOCK_CURSOR);
	}
}

static void scrollUpAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)event;

	auto textD = textD_of(w);
	int topLineNum, horizOffset, nLines;

	if (*nArgs == 0 || sscanf(args[0], "%d", &nLines) != 1)
		return;
	if (*nArgs == 2) {
		// Allow both 'page' and 'pages'
		if (strncmp(args[1], "page", 4) == 0)
			nLines *= textD->nVisibleLines;

		// 'line' or 'lines' is the only other valid possibility
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

	auto textD = textD_of(w);
	int topLineNum, horizOffset, nLines;

	if (*nArgs == 0 || sscanf(args[0], "%d", &nLines) != 1)
		return;
	if (*nArgs == 2) {
		// Allow both 'page' and 'pages'
		if (strncmp(args[1], "page", 4) == 0)
			nLines *= textD->nVisibleLines;

		// 'line' or 'lines' is the only other valid possibility
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

	auto textD = textD_of(w);
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

	auto textD = textD_of(w);
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

	auto textD = textD_of(w);
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

	auto textD = textD_of(w);
	TextBuffer *buf = textD_of(w)->textBuffer();

	textD->cancelDrag();
	buf->BufSelect(0, buf->BufGetLength());
}

static void deselectAllAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;
	(void)event;

	auto textD = textD_of(w);
	textD->cancelDrag();
	textD->textBuffer()->BufUnselect();
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

	auto tw = reinterpret_cast<TextWidget>(widget);
	TextDisplay *textD = textD_of(tw);

/* I don't entirely understand the traversal mechanism in Motif widgets,
   particularly, what leads to this widget getting a focus-in event when
   it does not actually have the input focus.  The temporary solution is
   to do the comparison below, and not show the cursor when Motif says
   we don't have focus, but keep looking for the real answer */
	if (widget != XmGetFocusWidget(widget))
		return;

	// If the timer is not already started, start it
	if (text_of(tw).P_cursorBlinkRate != 0 && textD->getCursorBlinkProcID() == 0) {
		textD->cursorBlinkProcID = XtAppAddTimeOut(XtWidgetToApplicationContext(widget), text_of(tw).P_cursorBlinkRate, cursorBlinkTimerProc, widget);
	}

	// Change the cursor to active style
	if (text_of(tw).P_overstrike) {
		textD->TextDSetCursorStyle(BLOCK_CURSOR);
	} else {
		textD->TextDSetCursorStyle((text_of(tw).P_heavyCursor ? HEAVY_CURSOR : NORMAL_CURSOR));
	}
	
	textD->TextDUnblankCursor();

	// Notify Motif input manager that widget has focus
	XmImVaSetFocusValues(widget, nullptr);

	// Call any registered focus-in callbacks
	XtCallCallbacks((Widget)widget, textNfocusCallback, (XtPointer)event);
}

static void focusOutAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {

	(void)args;
	(void)nArgs;

	auto textD = textD_of(w);

	// Remove the cursor blinking timer procedure
	if (textD->getCursorBlinkProcID() != 0) {
		XtRemoveTimeOut(textD->getCursorBlinkProcID());
	}
	
	textD->cursorBlinkProcID = 0;

	// Leave a dim or destination cursor
	textD->TextDSetCursorStyle(textD_of(w)->motifDestOwner ? CARET_CURSOR : DIM_CURSOR);
	textD->TextDUnblankCursor();

	// If there's a calltip displayed, kill it.
	textD->TextDKillCalltip(0);

	// Call any registered focus-out callbacks
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
		textD_of(w)->textBuffer()->BufUnselect();
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
	TextDisplay *textD = textD_of(tw);
	TextBuffer *buf = textD->buffer;
	TextSelection *sel = &buf->primary_;
	int newPos = textD->TextDGetInsertPosition();
	int startPos, endPos, startCol, endCol, newCol, origCol;
	int anchor, rectAnchor, anchorLineStart;

	/* Moving the cursor does not take the Motif destination, but as soon as
	   the user selects something, grab it (I'm not sure if this distinction
	   actually makes sense, but it's what Motif was doing, back when their
	   secondary selections actually worked correctly) */
	textD->TakeMotifDestination(e->time);

	if ((sel->selected || sel->zeroWidth) && sel->rectangular && rectangular) {
		// rect -> rect
		newCol = buf->BufCountDispChars(buf->BufStartOfLine(newPos), newPos);
		startCol = std::min<int>(textD_of(tw)->rectAnchor, newCol);
		endCol = std::max<int>(textD_of(tw)->rectAnchor, newCol);
		startPos = buf->BufStartOfLine(std::min<int>(textD_of(tw)->getAnchor(), newPos));
		endPos = buf->BufEndOfLine(std::max<int>(textD_of(tw)->anchor, newPos));
		buf->BufRectSelect(startPos, endPos, startCol, endCol);
	} else if (sel->selected && rectangular) { // plain -> rect
		newCol = buf->BufCountDispChars(buf->BufStartOfLine(newPos), newPos);
		if (abs(newPos - sel->start) < abs(newPos - sel->end))
			anchor = sel->end;
		else
			anchor = sel->start;
		anchorLineStart = buf->BufStartOfLine(anchor);
		rectAnchor = buf->BufCountDispChars(anchorLineStart, anchor);
		textD_of(tw)->anchor = anchor;
		textD_of(tw)->rectAnchor = rectAnchor;
		buf->BufRectSelect(buf->BufStartOfLine(std::min<int>(anchor, newPos)), buf->BufEndOfLine(std::max<int>(anchor, newPos)), std::min<int>(rectAnchor, newCol), std::max<int>(rectAnchor, newCol));
	} else if (sel->selected && sel->rectangular) { // rect -> plain
		startPos = buf->BufCountForwardDispChars(buf->BufStartOfLine(sel->start), sel->rectStart);
		endPos = buf->BufCountForwardDispChars(buf->BufStartOfLine(sel->end), sel->rectEnd);
		if (abs(origPos - startPos) < abs(origPos - endPos))
			anchor = endPos;
		else
			anchor = startPos;
		buf->BufSelect(anchor, newPos);
	} else if (sel->selected) { // plain -> plain
		if (abs(origPos - sel->start) < abs(origPos - sel->end))
			anchor = sel->end;
		else
			anchor = sel->start;
		buf->BufSelect(anchor, newPos);
	} else if (rectangular) { // no sel -> rect
		origCol = buf->BufCountDispChars(buf->BufStartOfLine(origPos), origPos);
		newCol = buf->BufCountDispChars(buf->BufStartOfLine(newPos), newPos);
		startCol = std::min<int>(newCol, origCol);
		endCol = std::max<int>(newCol, origCol);
		startPos = buf->BufStartOfLine(std::min<int>(origPos, newPos));
		endPos = buf->BufEndOfLine(std::max<int>(origPos, newPos));
		textD_of(tw)->anchor = origPos;
		textD_of(tw)->rectAnchor = origCol;
		buf->BufRectSelect(startPos, endPos, startCol, endCol);
	} else { // no sel -> plain
		textD_of(tw)->anchor = origPos;
		textD_of(tw)->rectAnchor = buf->BufCountDispChars(buf->BufStartOfLine(origPos), origPos);
		buf->BufSelect(textD_of(tw)->anchor, newPos);
	}
}



/*
** Insert text "chars" at the cursor position, as if the text had been
** typed.  Same as TextInsertAtCursorEx, but without the complicated auto-wrap
** scanning and re-formatting.
*/
static void simpleInsertAtCursorEx(Widget w, view::string_view chars, XEvent *event, int allowPendingDelete) {
	auto textD = textD_of(w);
	TextBuffer *buf = textD->buffer;

	if (allowPendingDelete && pendingSelection(w)) {
		buf->BufReplaceSelectedEx(chars);
		textD->TextDSetInsertPosition(buf->cursorPosHint_);
	} else if (text_of(w).P_overstrike) {

		size_t index = chars.find('\n');
		if(index != view::string_view::npos) {
			textD->TextDInsertEx(chars);
		} else {
			textD->TextDOverstrikeEx(chars);
		}
	} else {
		textD->TextDInsertEx(chars);
	}

	textD->checkAutoShowInsertPos();
	textD->callCursorMovementCBs(event);
}

/*
** If there's a selection, delete it and position the cursor where the
** selection was deleted.  (Called by routines which do deletion to check
** first for and do possible selection delete)
*/
static int deletePendingSelection(Widget w, XEvent *event) {
	auto textD = textD_of(w);
	TextBuffer *buf = textD->buffer;

	if (textD_of(w)->textBuffer()->primary_.selected) {
		buf->BufRemoveSelected();
		textD->TextDSetInsertPosition(buf->cursorPosHint_);
		textD->checkAutoShowInsertPos();
		textD->callCursorMovementCBs(event);
		return true;
	} else
		return false;
}

/*
** Return true if pending delete is on and there's a selection contiguous
** with the cursor ready to be deleted.  These criteria are used to decide
** if typing a character or inserting something should delete the selection
** first.
*/
static int pendingSelection(Widget w) {
	TextSelection *sel = &textD_of(w)->textBuffer()->primary_;
	int pos = textD_of(w)->TextDGetInsertPosition();

	return text_of(w).P_pendingDelete && sel->selected && pos >= sel->start && pos <= sel->end;
}

/*
** Check if tab emulation is on and if there are emulated tabs before the
** cursor, and if so, delete an emulated tab as a unit.  Also finishes up
** by calling checkAutoShowInsertPos and callCursorMovementCBs, so the
** calling action proc can just return (this is necessary to preserve
** emTabsBeforeCursor which is otherwise cleared by callCursorMovementCBs).
*/
static int deleteEmulatedTab(Widget w, XEvent *event) {
	TextDisplay *textD        = textD_of(w);
	TextBuffer *buf        = textD_of(w)->textBuffer();
	int emTabDist          = text_of(w).P_emulateTabs;
	int emTabsBeforeCursor = textD_of(w)->emTabsBeforeCursor;
	int startIndent, toIndent, insertPos, startPos, lineStart;
	int pos, indent, startPosIndent;
	char c;

	if (emTabDist <= 0 || emTabsBeforeCursor <= 0)
		return False;

	// Find the position of the previous tab stop
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

	// Just to make sure, check that we're not deleting any non-white chars
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
	textD->checkAutoShowInsertPos();
	textD->callCursorMovementCBs(event);

	/* Decrement and restore the marker for consecutive emulated tabs, which
	   would otherwise have been zeroed by callCursorMovementCBs */
	textD_of(w)->emTabsBeforeCursor = emTabsBeforeCursor - 1;
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
	TextBuffer *buf = textD_of(tw)->buffer;
	int x, y, insertPos = textD_of(tw)->TextDGetInsertPosition();

	textD_of(tw)->TextDPositionToXY(insertPos, &x, &y);
	if (pointerX < x && insertPos > 0 && buf->BufGetCharacter(insertPos - 1) != '\n')
		insertPos--;
	buf->BufSelect(startOfWord(tw, insertPos), endOfWord(tw, insertPos));
}

static int startOfWord(TextWidget w, int pos) {
	int startPos;
	TextBuffer *buf = textD_of(w)->textBuffer();
	const char *delimiters = text_of(w).P_delimiters;
	char c = buf->BufGetCharacter(pos);

	if (c == ' ' || c == '\t') {
		if (!spanBackward(buf, pos, " \t", false, &startPos))
			return 0;
	} else if (strchr(delimiters, c)) {
		if (!spanBackward(buf, pos, delimiters, true, &startPos))
			return 0;
	} else {
		if (!buf->BufSearchBackwardEx(pos, delimiters, &startPos))
			return 0;
	}
	return std::min<int>(pos, startPos + 1);
}

static int endOfWord(TextWidget w, int pos) {
	int endPos;
	TextBuffer *buf = textD_of(w)->textBuffer();
	const char *delimiters = text_of(w).P_delimiters;
	char c = buf->BufGetCharacter(pos);

	if (c == ' ' || c == '\t') {
		if (!spanForward(buf, pos, " \t", false, &endPos))
			return buf->BufGetLength();
	} else if (strchr(delimiters, c)) {
		if (!spanForward(buf, pos, delimiters, true, &endPos))
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
** result in "foundPos" returns True if found, false if not. If ignoreSpace
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
** result in "foundPos" returns True if found, false if not. If ignoreSpace is
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
	auto textD = textD_of(w);
	int insertPos = textD->TextDGetInsertPosition();
	int endPos, startPos;

	endPos = textD->textBuffer()->BufEndOfLine(insertPos);
	startPos = textD->textBuffer()->BufStartOfLine(insertPos);
	textD->textBuffer()->BufSelect(startPos, std::min<int>(endPos + 1, textD->textBuffer()->BufGetLength()));
	textD->TextDSetInsertPosition(endPos);
}

/*
** Given a new mouse pointer location, pass the position on to the
** autoscroll timer routine, and make sure the timer is on when it's
** needed and off when it's not.
*/
static void checkAutoScroll(TextWidget w, int x, int y) {
	int inWindow;

	// Is the pointer in or out of the window?
	inWindow = x >= textD_of(w)->rect.left && x < w->core.width - text_of(w).P_marginWidth && y >= text_of(w).P_marginHeight && y < w->core.height - text_of(w).P_marginHeight;

	// If it's in the window, cancel the timer procedure
	if (inWindow) {
		if (textD_of(w)->autoScrollProcID != 0)
			XtRemoveTimeOut(textD_of(w)->autoScrollProcID);
		;
		textD_of(w)->autoScrollProcID = 0;
		return;
	}

	// If the timer is not already started, start it
	if (textD_of(w)->autoScrollProcID == 0) {
		textD_of(w)->autoScrollProcID = XtAppAddTimeOut(XtWidgetToApplicationContext((Widget)w), 0, autoScrollTimerProc, w);
	}

	// Pass on the newest mouse location to the autoscroll routine
	textD_of(w)->getMouseCoord() = {x, y};
}

/*
** Reset drag state and cancel the auto-scroll timer
*/
static void endDrag(Widget w) {

	if (textD_of(w)->autoScrollProcID != 0)
		XtRemoveTimeOut(textD_of(w)->autoScrollProcID);

	textD_of(w)->autoScrollProcID = 0;

	if (textD_of(w)->dragState == MOUSE_PAN)
		XUngrabPointer(XtDisplay(w), CurrentTime);

	textD_of(w)->dragState = NOT_CLICKED;
}





/*
** Adjust the selection as the mouse is dragged to position: (x, y).
*/
static void adjustSelection(TextWidget tw, int x, int y) {
	TextDisplay *textD = textD_of(tw);
	TextBuffer *buf = textD->buffer;
	int row, col, startCol, endCol, startPos, endPos;
	int newPos = textD->TextDXYToPosition(Point{x, y});

	// Adjust the selection
	if (textD_of(tw)->dragState == PRIMARY_RECT_DRAG) {
	
		textD->TextDXYToUnconstrainedPosition(Point{x, y}, &row, &col);
		col      = textD->TextDOffsetWrappedColumn(row, col);
		startCol = std::min<int>(textD_of(tw)->rectAnchor, col);
		endCol   = std::max<int>(textD_of(tw)->rectAnchor, col);
		startPos = buf->BufStartOfLine(std::min<int>(textD_of(tw)->getAnchor(), newPos));
		endPos   = buf->BufEndOfLine(std::max<int>(textD_of(tw)->getAnchor(), newPos));
		buf->BufRectSelect(startPos, endPos, startCol, endCol);
		
	} else if (textD_of(tw)->multiClickState == ONE_CLICK) {
		startPos = startOfWord(tw, std::min<int>(textD_of(tw)->getAnchor(), newPos));
		endPos   = endOfWord(tw, std::max<int>(textD_of(tw)->getAnchor(), newPos));
		buf->BufSelect(startPos, endPos);
		newPos   = newPos < textD_of(tw)->getAnchor() ? startPos : endPos;
		
	} else if (textD_of(tw)->multiClickState == TWO_CLICKS) {
		startPos = buf->BufStartOfLine(std::min<int>(textD_of(tw)->getAnchor(), newPos));
		endPos   = buf->BufEndOfLine(std::max<int>(textD_of(tw)->getAnchor(), newPos));
		buf->BufSelect(startPos, std::min<int>(endPos + 1, buf->BufGetLength()));
		newPos   = newPos < textD_of(tw)->getAnchor() ? startPos : endPos;
	} else
		buf->BufSelect(textD_of(tw)->getAnchor(), newPos);

	// Move the cursor
	textD->TextDSetInsertPosition(newPos);
	textD->callCursorMovementCBs(nullptr);
}

static void adjustSecondarySelection(TextWidget tw, int x, int y) {
	TextDisplay *textD = textD_of(tw);
	TextBuffer *buf = textD->buffer;
	int row, col, startCol, endCol, startPos, endPos;
	int newPos = textD->TextDXYToPosition(Point{x, y});

	if (textD_of(tw)->dragState == SECONDARY_RECT_DRAG) {
		textD->TextDXYToUnconstrainedPosition(Point{x, y}, &row, &col);
		col      = textD->TextDOffsetWrappedColumn(row, col);
		startCol = std::min(textD_of(tw)->rectAnchor, col);
		endCol   = std::max(textD_of(tw)->rectAnchor, col);
		startPos = buf->BufStartOfLine(std::min(textD_of(tw)->getAnchor(), newPos));
		endPos   = buf->BufEndOfLine(std::max(textD_of(tw)->getAnchor(), newPos));
		buf->BufSecRectSelect(startPos, endPos, startCol, endCol);
	} else {
		textD->textBuffer()->BufSecondarySelect(textD_of(tw)->getAnchor(), newPos);
	}
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
	TextDisplay *textD = textD_of(tw);
	int pos, indent = -1, tabDist = textD->textBuffer()->tabDist_;
	int i, useTabs = textD->textBuffer()->useTabs_;
	char c;
	smartIndentCBStruct smartIndent;

	/* If smart indent is on, call the smart indent callback.  It is not
	   called when multi-line changes are being made (lineStartPos != 0),
	   because smart indent needs to search back an indeterminate distance
	   through the buffer, and reconciling that with wrapping changes made,
	   but not yet committed in the buffer, would make programming smart
	   indent more difficult for users and make everything more complicated */
	if (text_of(tw).P_smartIndent && (lineStartPos == 0 || buf == textD->buffer)) {
		smartIndent.reason = NEWLINE_INDENT_NEEDED;
		smartIndent.pos = lineEndPos + bufOffset;
		smartIndent.indentRequest = 0;
		smartIndent.charsTyped = nullptr;
		XtCallCallbacks((Widget)tw, textNsmartIndentCallback, &smartIndent);
		indent = smartIndent.indentRequest;
	}

	// If smart indent wasn't used, measure the indent distance of the line
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

	// Allocate and create a string of tabs and spaces to achieve the indent
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

	// Return any requested stats
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

	TextWidget w = static_cast<TextWidget>(clientData);
	TextDisplay *textD = textD_of(w);
	int topLineNum;
	int horizOffset;
	int cursorX;
	int y;
	int fontWidth  = textD->fontStruct->max_bounds.width;
	int fontHeight = textD->fontStruct->ascent + textD->fontStruct->descent;

	/* For vertical autoscrolling just dragging the mouse outside of the top
	   or bottom of the window is sufficient, for horizontal (non-rectangular)
	   scrolling, see if the position where the CURSOR would go is outside */
	int newPos = textD->TextDXYToPosition(textD_of(w)->getMouseCoord());
	
	if (textD_of(w)->dragState == PRIMARY_RECT_DRAG) {
		cursorX = textD_of(w)->getMouseCoord().x;
	} else if (!textD->TextDPositionToXY(newPos, &cursorX, &y)) {
		cursorX = textD_of(w)->getMouseCoord().x;
	}

	/* Scroll away from the pointer, 1 character (horizontal), or 1 character
	   for each fontHeight distance from the mouse to the text (vertical) */
	textD->TextDGetScroll(&topLineNum, &horizOffset);
	
	if (cursorX >= (int)w->core.width - text_of(w).P_marginWidth) {
		horizOffset += fontWidth;
	} else if (textD_of(w)->getMouseCoord().x < textD->rect.left) {
		horizOffset -= fontWidth;
	}
		
	if (textD_of(w)->getMouseCoord().y >= (int)w->core.height - text_of(w).P_marginHeight) {
		topLineNum += 1 + ((textD_of(w)->getMouseCoord().y - (int)w->core.height - text_of(w).P_marginHeight) / fontHeight) + 1;
	} else if (textD_of(w)->getMouseCoord().y < text_of(w).P_marginHeight) {
		topLineNum -= 1 + ((text_of(w).P_marginHeight - textD_of(w)->getMouseCoord().y) / fontHeight);
	}
	
	textD->TextDSetScroll(topLineNum, horizOffset);

	/* Continue the drag operation in progress.  If none is in progress
	   (safety check) don't continue to re-establish the timer proc */
	switch(textD_of(w)->dragState) {
	case PRIMARY_DRAG:
		adjustSelection(w, textD_of(w)->getMouseCoord().x, textD_of(w)->getMouseCoord().y);
		break;
	case PRIMARY_RECT_DRAG:
		adjustSelection(w, textD_of(w)->getMouseCoord().x, textD_of(w)->getMouseCoord().y);
		break;
	case SECONDARY_DRAG:
		adjustSecondarySelection(w, textD_of(w)->getMouseCoord().x, textD_of(w)->getMouseCoord().y);
		break;
	case SECONDARY_RECT_DRAG:
		adjustSecondarySelection(w, textD_of(w)->getMouseCoord().x, textD_of(w)->getMouseCoord().y);
		break;
	case PRIMARY_BLOCK_DRAG:
		textD->BlockDragSelection(textD_of(w)->getMouseCoord(), USE_LAST);
		break;
	default:
		textD_of(w)->autoScrollProcID = 0;
		return;
	}
	   
	// re-establish the timer proc (this routine) to continue processing
	textD_of(w)->autoScrollProcID = XtAppAddTimeOut(
		XtWidgetToApplicationContext(
		(Widget)w), 
		textD_of(w)->getMouseCoord().y >= text_of(w).P_marginHeight && textD_of(w)->getMouseCoord().y < w->core.height - text_of(w).P_marginHeight ? (VERTICAL_SCROLL_DELAY * fontWidth) / fontHeight : VERTICAL_SCROLL_DELAY,
		autoScrollTimerProc, w);
}

/*
** Xt timer procedure for cursor blinking
*/
static void cursorBlinkTimerProc(XtPointer clientData, XtIntervalId *id) {
	(void)id;

	TextWidget w = static_cast<TextWidget>(clientData);
	TextDisplay *textD = textD_of(w);

	// Blink the cursor
	if (textD->cursorOn) {
		textD->TextDBlankCursor();
	} else {
		textD->TextDUnblankCursor();
	}

	// re-establish the timer proc (this routine) to continue processing
	textD->cursorBlinkProcID = XtAppAddTimeOut(XtWidgetToApplicationContext((Widget)w), text_of(w).P_cursorBlinkRate, cursorBlinkTimerProc, w);
}



/*
** look at an action procedure's arguments to see if argument "key" has been
** specified in the argument list
*/
static bool hasKey(const char *key, const String *args, const Cardinal *nArgs) {

	for (int i = 0; i < (int)*nArgs; i++) {
		if (!strCaseCmp(args[i], key)) {
			return true;
		}
	}
	return false;
}

/*
** strCaseCmp compares its arguments and returns 0 if the two strings
** are equal IGNORING case differences.  Otherwise returns 1.
*/

static int strCaseCmp(const char *str1, const char *str2) {
	unsigned const char *c1 = (unsigned const char *)str1;
	unsigned const char *c2 = (unsigned const char *)str2;

	for (; *c1 != '\0' && *c2 != '\0'; c1++, c2++) {
		if (toupper(*c1) != toupper(*c2)) {
			return 1;
		}
	}
	
	if (*c1 == *c2) {
		return 0;
	} else {
		return 1;
	}
}

static void ringIfNecessary(bool silent) {
	if (!silent) {
		QApplication::beep();
	}
}
