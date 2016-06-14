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
#include "MultiClickStates.h"
#include "DragStates.h"
#include "nedit.h"
#include "Document.h"
#include "calltips.h"
#include "window.h"
#include "util/memory.h"
#include "TextHelper.h"

#include <climits>
#include <algorithm>

#include <X11/Intrinsic.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <Xm/Xm.h>
#include <Xm/XmP.h>
#include <Xm/PrimitiveP.h>

#define NEDIT_DEFAULT_CALLTIP_FG "black"
#define NEDIT_DEFAULT_CALLTIP_BG "LemonChiffon1"

static void initialize(Widget request, Widget new_widget, ArgList args, Cardinal *num_args);
static void handleHidePointer(Widget w, XtPointer unused, XEvent *event, Boolean *continue_to_dispatch);
static void redisplay(Widget w, XEvent *event, Region region);
static void redisplayGE(TextWidget w, XtPointer client_data, XEvent *event, Boolean *continue_to_dispatch_return);
static void destroy(Widget w);
static void resize(Widget w);
static Boolean setValues(Widget current, Widget request, Widget new_widget, ArgList args, Cardinal *num_args);
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
	{ textNhScrollBar,             textCHScrollBar,             XmRWidget,     sizeof(Widget),        XtOffset(TextWidget, text.P_hScrollBar),              XmRPointer, nullptr                                  },
	{ textNvScrollBar,             textCVScrollBar,             XmRWidget,     sizeof(Widget),        XtOffset(TextWidget, text.P_vScrollBar),              XmRPointer, nullptr                                  },
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
		destroy,			               // destroy
		resize, 			               // resize
		redisplay,  		               // expose
		setValues,                         // set_values
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
	
	/* NOTE(eteran):
	** request:   Specifies a copy of the widget with resource values as requested by the argument list, the resource database, and the widget defaults.
	** newWidget: Specifies the widget with the new values, both resource and nonresource, that are actually allowed.
	** args:      Specifies the argument list passed by the client, for computing derived resource values. If the client created the widget using a varargs form, any resources specified via XtVaTypedArg are converted to the widget representation and the list is transformed into the ArgList format.
	** num_args:  Specifies the number of entries in the argument list.
	*/

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
	Screen *screen = XtScreen(newWidget);
	Pixel white = WhitePixelOfScreen(screen);
	Pixel black = BlackPixelOfScreen(screen);

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

	const size_t n = strlen(tw.P_delimiters) + 4;
    char *delimiters = new char[n];
    snprintf(delimiters, n, "%s%s", " \t\n", tw.P_delimiters);
    tw.P_delimiters = delimiters;


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
static void destroy(Widget w) {

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

	XtRemoveAllCallbacks(w, textNfocusCallback);
	XtRemoveAllCallbacks(w, textNlosingFocusCallback);
	XtRemoveAllCallbacks(w, textNcursorMovementCallback);
	XtRemoveAllCallbacks(w, textNdragStartCallback);
	XtRemoveAllCallbacks(w, textNdragEndCallback);

	// Unregister the widget from the input manager
	XmImUnregister(w);
	

}

/*
** Widget resize method.  Called when the size of the widget changes
*/
static void resize(Widget w) {

	auto tw = reinterpret_cast<TextWidget>(w);

	XFontStruct *fs      = tw->text.P_fontStruct;
	int height           = tw->core.height;
	int width            = tw->core.width;
	int marginWidth      = tw->text.P_marginWidth;
	int marginHeight     = tw->text.P_marginHeight;
	int lineNumAreaWidth = tw->text.P_lineNumCols == 0 ? 0 : tw->text.P_marginWidth + fs->max_bounds.width * tw->text.P_lineNumCols;

	tw->text.P_columns = (width - marginWidth * 2 - lineNumAreaWidth) / fs->max_bounds.width;
	tw->text.P_rows    = (height - marginHeight * 2) / (fs->ascent + fs->descent);

	/* Reject widths and heights less than a character, which the text
	   display can't tolerate.  This is not strictly legal, but I've seen
	   it done in other widgets and it seems to do no serious harm.  NEdit
	   prevents panes from getting smaller than one line, but sometimes
	   splitting windows on Linux 2.0 systems (same Motif, why the change in
	   behavior?), causes one or two resize calls with < 1 line of height.
	   Fixing it here is 100x easier than re-designing TextDisplay.c */
	if (tw->text.P_columns < 1) {
		tw->text.P_columns = 1;
		tw->core.width = width = fs->max_bounds.width + marginWidth * 2 + lineNumAreaWidth;
	}
	
	if (tw->text.P_rows < 1) {
		tw->text.P_rows = 1;
		tw->core.height = height = fs->ascent + fs->descent + marginHeight * 2;
	}

	// Resize the text display that the widget uses to render text
	textD_of(w)->TextDResize(width - marginWidth * 2 - lineNumAreaWidth, height - marginHeight * 2);

	/* if the window became shorter or narrower, there may be text left
	   in the bottom or right margin area, which must be cleaned up */
	if (XtIsRealized(w)) {
		XClearArea(XtDisplay(w), XtWindow(w), 0, height - marginHeight, width, marginHeight, false);
		XClearArea(XtDisplay(w), XtWindow(w), width - marginWidth, 0, marginWidth, height, false);
	}
}

/*
** Widget redisplay method
*/
static void redisplay(Widget w, XEvent *event, Region region) {

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
static Boolean setValues(Widget current, Widget request, Widget new_widget, ArgList args, Cardinal *num_args) {

	/* NOTE(eteran):
	** current:    Specifies a copy of the widget as it was before the XtSetValues call.
	** request:    Specifies a copy of the widget with all values changed as asked for by the XtSetValues call before any class set_values procedures have been called.
	** new_widget: Specifies the widget with the new values that are actually allowed.
	*/

	(void)request;
	(void)args;
	(void)num_args;

	bool redraw = false;
	bool reconfigure = false;
	
	auto &newText = text_of(new_widget);
	auto &curText = text_of(current);
	TextDisplay *newD = textD_of(new_widget);
	TextDisplay *curD = textD_of(current);

	// did overstrike change?
	if (newText.P_overstrike != curText.P_overstrike) {
	
		switch(curD->cursorStyle) {
		case BLOCK_CURSOR:
			curD->TextDSetCursorStyle(curText.P_heavyCursor ? HEAVY_CURSOR : NORMAL_CURSOR);
			break;
		case NORMAL_CURSOR:
		case HEAVY_CURSOR:
			curD->TextDSetCursorStyle(BLOCK_CURSOR);
		default:
			// NOTE(eteran): wasn't handled in the original code
			break;
		}
	}

	// did the font change?
	if (newText.P_fontStruct != curText.P_fontStruct) {
		
		if (newText.P_lineNumCols != 0) {
			reconfigure = true;
		}
		
		curD->TextDSetFont(newText.P_fontStruct);
	}

	// did the wrap change?
	if (newText.P_wrapMargin != curText.P_wrapMargin || newText.P_continuousWrap != curText.P_continuousWrap) {
		curD->TextDSetWrapMode(newText.P_continuousWrap, newText.P_wrapMargin);
	}

	/* When delimiters are changed, copy the memory, so that the caller
	   doesn't have to manage it, and add mandatory delimiters blank,
	   tab, and newline to the list */
	if (newText.P_delimiters != curText.P_delimiters) {

		const size_t n = strlen(newText.P_delimiters) + 4;
		char *delimiters = new char[n];	
		delete [] curText.P_delimiters;
		snprintf(delimiters, n, "%s%s", " \t\n", newText.P_delimiters);
		newText.P_delimiters = delimiters;
	}

	/* Setting the lineNumCols resource tells the text widget to hide or
	   show, or change the number of columns of the line number display,
	   which requires re-organizing the x coordinates of both the line
	   number display and the main text display */
	if (newText.P_lineNumCols != curText.P_lineNumCols || reconfigure) {

		int marginWidth = newText.P_marginWidth;
		int charWidth   = newText.P_fontStruct->max_bounds.width;
		int lineNumCols = newText.P_lineNumCols;

		if (lineNumCols == 0) {
			newD->TextDSetLineNumberArea(0, 0, marginWidth);
			newText.P_columns = (new_widget->core.width - marginWidth * 2) / charWidth;
		} else {
			newD->TextDSetLineNumberArea(marginWidth, charWidth * lineNumCols, 2 * marginWidth + charWidth * lineNumCols);
			newText.P_columns = (new_widget->core.width - marginWidth * 3 - charWidth * lineNumCols) / charWidth;
		}
	}

	// did the backlight change?
	if (newText.P_backlightCharTypes != curText.P_backlightCharTypes) {
		TextDisplay::TextDSetupBGClasses((Widget)new_widget, newText.P_backlightCharTypes, &newD->bgClassPixel, &newD->bgClass, newD->bgPixel);
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
		answer->height = std::max(1, ((propHeight - 2 * marginHeight) / fontHeight)) * fontHeight + 2 * marginHeight;
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
	textD_of(w)->grabFocusAP(event, args, nArgs);
}

static void moveDestinationAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->moveDestinationAP(event, args, nArgs);
}

static void extendAdjustAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->extendAdjustAP(event, args, nArgs);
}

static void extendStartAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->extendStartAP(event, args, nArgs);
}

static void extendEndAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->extendEndAP(event, args, nArgs);
}

static void processCancelAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->processCancelAP(event, args, nArgs);
}

static void secondaryStartAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->secondaryStartAP(event, args, nArgs);
}

static void secondaryOrDragStartAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->secondaryOrDragStartAP(event, args, nArgs);
}

static void secondaryAdjustAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->secondaryAdjustAP(event, args, nArgs);
}

static void secondaryOrDragAdjustAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->secondaryOrDragAdjustAP(event, args, nArgs);
}

static void copyToAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->copyToAP(event, args, nArgs);
}

static void copyToOrEndDragAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->copyToOrEndDragAP(event, args, nArgs);
}

static void moveToAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->moveToAP(event, args, nArgs);
}

static void moveToOrEndDragAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->moveToOrEndDragAP(event, args, nArgs);
}

static void endDragAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->endDragAP(event, args, nArgs);
}

static void exchangeAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->exchangeAP(event, args, nArgs);
}

static void copyPrimaryAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->copyPrimaryAP(event, args, nArgs);
}

static void cutPrimaryAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->cutPrimaryAP(event, args, nArgs);
}

static void mousePanAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->mousePanAP(event, args, nArgs);
}

static void pasteClipboardAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->pasteClipboardAP(event, args, nArgs);
}

static void copyClipboardAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->copyClipboardAP(event, args, nArgs);
}

static void cutClipboardAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->cutClipboardAP(event, args, nArgs);
}

static void insertStringAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->insertStringAP(event, args, nArgs);
}

static void selfInsertAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->selfInsertAP(event, args, nArgs);
}

static void newlineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->newlineAP(event, args, nArgs);
}

static void newlineNoIndentAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->newlineNoIndentAP(event, args, nArgs);
}

static void newlineAndIndentAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->newlineAndIndentAP(event, args, nArgs);
}

static void processTabAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->processTabAP(event, args, nArgs);
}

static void deleteSelectionAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->deleteSelectionAP(event, args, nArgs);
}

static void deletePreviousCharacterAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->deletePreviousCharacterAP(event, args, nArgs);
}

static void deleteNextCharacterAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->deleteNextCharacterAP(event, args, nArgs);
}

static void deletePreviousWordAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->deletePreviousWordAP(event, args, nArgs);
}

static void deleteNextWordAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->deleteNextWordAP(event, args, nArgs);
}

static void deleteToEndOfLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->deleteToEndOfLineAP(event, args, nArgs);
}

static void deleteToStartOfLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->deleteToStartOfLineAP(event, args, nArgs);
}

static void forwardCharacterAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->forwardCharacterAP(event, args, nArgs);
}

static void backwardCharacterAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->backwardCharacterAP(event, args, nArgs);
}

static void forwardWordAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->forwardWordAP(event, args, nArgs);
}

static void backwardWordAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->backwardWordAP(event, args, nArgs);
}

static void forwardParagraphAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->forwardParagraphAP(event, args, nArgs);
}

static void backwardParagraphAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->backwardParagraphAP(event, args, nArgs);
}

static void keySelectAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->keySelectAP(event, args, nArgs);
}

static void processUpAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->processUpAP(event, args, nArgs);
}

static void processShiftUpAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->processShiftUpAP(event, args, nArgs);
}

static void processDownAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->processDownAP(event, args, nArgs);
}

static void processShiftDownAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->processShiftDownAP(event, args, nArgs);
}

static void beginningOfLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->beginningOfLineAP(event, args, nArgs);
}

static void endOfLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->endOfLineAP(event, args, nArgs);
}

static void beginningOfFileAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->beginningOfFileAP(event, args, nArgs);
}

static void endOfFileAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->endOfFileAP(event, args, nArgs);
}

static void nextPageAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->nextPageAP(event, args, nArgs);
}

static void previousPageAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->previousPageAP(event, args, nArgs);
}

static void pageLeftAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->pageLeftAP(event, args, nArgs);
}

static void pageRightAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->pageRightAP(event, args, nArgs);
}

static void toggleOverstrikeAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->toggleOverstrikeAP(event, args, nArgs);
}

static void scrollUpAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->scrollUpAP(event, args, nArgs);
}

static void scrollDownAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->scrollDownAP(event, args, nArgs);
}

static void scrollLeftAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->scrollLeftAP(event, args, nArgs);
}

static void scrollRightAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->scrollRightAP(event, args, nArgs);
}

static void scrollToLineAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->scrollToLineAP(event, args, nArgs);
}

static void selectAllAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->selectAllAP(event, args, nArgs);
}

static void deselectAllAP(Widget w, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(w)->deselectAllAP(event, args, nArgs);
}

/*
**  Called on the Intrinsic FocusIn event.
**
**  Note that the widget has no internal state about the focus, ie. it does
**  not know whether it has the focus or not.
*/
static void focusInAP(Widget widget, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(widget)->focusInAP(event, args, nArgs);
}

static void focusOutAP(Widget widget, XEvent *event, String *args, Cardinal *nArgs) {
	textD_of(widget)->focusOutAP(event, args, nArgs);
}

