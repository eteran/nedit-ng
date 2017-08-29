/*******************************************************************************
*                                                                              *
* macro.c -- Macro file processing, learn/replay, and built-in macro           *
*            subroutines                                                       *
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
* April, 1997                                                                  *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/

#include "macro.h"
#include "version.h"
#include "DialogPrompt.h"
#include "DialogPromptList.h"
#include "DialogPromptString.h"
#include "DialogRepeat.h"
#include "DocumentWidget.h"
#include "HighlightPattern.h"
#include "IndentStyle.h"
#include "CommandRecorder.h"
#include "SignalBlocker.h"
#include "Input.h"
#include "CloseMode.h"
#include "MainWindow.h"
#include "RangesetTable.h"
#include "SearchDirection.h"
#include "Settings.h"
#include "TextArea.h"
#include "TextBuffer.h"
#include "WrapStyle.h"
#include "calltips.h"
#include "highlight.h"
#include "highlightData.h"
#include "interpret.h"
#include "parse.h"
#include "search.h"
#include "selection.h"
#include "smartIndent.h"
#include "string_view.h"
#include "tags.h"
#include "userCmds.h"
#include "util/fileUtils.h"
#include "util/utils.h"

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFuture>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QStack>
#include <QString>
#include <QTimer>
#include <QWidget>
#include <QtConcurrent>
#include <QtDebug>

#include <fstream>
#include <functional>
#include <memory>
#include <sys/param.h>
#include <sys/stat.h>
#include <type_traits>

#define NO_FLASH_STRING      "off"
#define FLASH_DELIMIT_STRING "delimiter"
#define FLASH_RANGE_STRING   "range"

namespace {

// How long to wait (msec) before putting up Macro Command banner
constexpr int BANNER_WAIT_TIME = 6000;

}


// The following definitions cause an exit from the macro with a message
// added if (1) to remove compiler warnings on solaris
#define M_FAILURE(s)   \
    do {               \
        *errMsg = s;   \
        return false;  \
    } while (0)

#define M_ARRAY_INSERT_FAILURE() M_FAILURE("array element failed to insert: %s")

static void cancelLearnEx();
static void runMacroEx(DocumentWidget *document, Program *prog);
static void finishMacroCmdExecutionEx(DocumentWidget *document);
static bool continueWorkProcEx(DocumentWidget *clientData);

static int lengthMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int minMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int maxMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int focusWindowMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int getRangeMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int getCharacterMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int replaceRangeMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int replaceSelectionMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int getSelectionMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int validNumberMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int replaceInStringMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int replaceSubstringMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int readFileMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int writeFileMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int appendFileMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int writeOrAppendFile(bool append, DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int substringMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int toupperMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int tolowerMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int stringToClipboardMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int clipboardToStringMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int searchMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int searchStringMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int setCursorPosMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int beepMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int selectMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int selectRectangleMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int tPrintMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int getenvMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int shellCmdMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int dialogMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int stringDialogMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int calltipMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int killCalltipMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);

// T Balinski
static int listDialogMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
// T Balinski End

static int stringCompareMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int splitMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
#if 0 // DISASBLED for 5.4
static int setBacklightStringMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
#endif
static int cursorMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int lineMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int columnMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int fileNameMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int filePathMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int lengthMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int selectionStartMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int selectionEndMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int selectionLeftMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int selectionRightMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int statisticsLineMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int incSearchLineMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int showLineNumbersMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int autoIndentMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int wrapTextMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int highlightSyntaxMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int makeBackupCopyMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int incBackupMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int showMatchingMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int matchSyntaxBasedMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int overTypeModeMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int readOnlyMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int lockedMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int fileFormatMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int fontNameMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int fontNameItalicMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int fontNameBoldMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int fontNameBoldItalicMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int subscriptSepMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int minFontWidthMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int maxFontWidthMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int wrapMarginMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int topLineMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int numDisplayLinesMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int displayWidthMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int activePaneMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int nPanesMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int emptyArrayMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int serverNameMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int tabDistMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int emTabDistMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int useTabsMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int modifiedMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int languageModeMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int calltipIDMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int rangesetListMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int versionMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int rangesetCreateMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int rangesetDestroyMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int rangesetGetByNameMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int rangesetAddMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int rangesetSubtractMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int rangesetInvertMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int rangesetInfoMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int rangesetRangeMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int rangesetIncludesPosMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int rangesetSetColorMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int rangesetSetNameMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int rangesetSetModeMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int fillPatternResultEx(DataValue *result, const char **errMsg, DocumentWidget *window, const char *patternName, bool includeName, char *styleName, int bufferPos);
static int getPatternByNameMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int getPatternAtPosMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int fillStyleResultEx(DataValue *result, const char **errMsg, DocumentWidget *document, const char *styleName, bool includeName, int patCode, int bufferPos);
static int getStyleByNameMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int getStyleAtPosMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int filenameDialogMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);

// MainWindow scoped functions
static int replaceAllInSelectionMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);
static int replaceAllMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg);

static int readSearchArgs(DataValue *argList, int nArgs, SearchDirection *searchDirection, SearchType *searchType, WrapMode *wrap, const char **errMsg);
static bool wrongNArgsErr(const char **errMsg);
static bool tooFewArgsErr(const char **errMsg);

static bool readArgument(DataValue dv, int *result, const char **errMsg = nullptr);
static bool readArgument(DataValue dv, std::string *result, const char **errMsg = nullptr);
static bool readArgument(DataValue dv, QString *result, const char **errMsg = nullptr);

template <class T, class ...Ts>
bool readArguments(DataValue *argList, int nArgs, int index, const char **errMsg, T arg, Ts...args);

template <class T>
bool readArguments(DataValue *argList, int nArgs, int index, const char **errMsg, T arg);

struct SubRoutine {
    const char   *name;
    BuiltInSubrEx function;
};

/**
 * @brief flagsFromArguments
 * @param argList
 * @param nArgs
 * @param firstFlag
 * @param flags
 * @return true if all arguments were valid flags, false otherwise
 */
bool flagsFromArguments(DataValue *argList, int nArgs, int firstFlag, TextArea::EventFlags *flags) {
    TextArea::EventFlags f = TextArea::NoneFlag;
    for(int i = firstFlag; i < nArgs; ++i) {
        if(strcmp(argList[i].val.str.rep, "absolute") == 0) {
            f |= TextArea::AbsoluteFlag;
        } else if(strcmp(argList[i].val.str.rep, "column") == 0) {
            f |= TextArea::ColumnFlag;
        } else if(strcmp(argList[i].val.str.rep, "copy") == 0) {
            f |= TextArea::CopyFlag;
        } else if(strcmp(argList[i].val.str.rep, "down") == 0) {
            f |= TextArea::DownFlag;
        } else if(strcmp(argList[i].val.str.rep, "extend") == 0) {
            f |= TextArea::ExtendFlag;
        } else if(strcmp(argList[i].val.str.rep, "left") == 0) {
            f |= TextArea::LeftFlag;
        } else if(strcmp(argList[i].val.str.rep, "overlay") == 0) {
            f |= TextArea::OverlayFlag;
        } else if(strcmp(argList[i].val.str.rep, "rect") == 0) {
            f |= TextArea::RectFlag;
        } else if(strcmp(argList[i].val.str.rep, "right") == 0) {
            f |= TextArea::RightFlag;
        } else if(strcmp(argList[i].val.str.rep, "up") == 0) {
            f |= TextArea::UpFlag;
        } else if(strcmp(argList[i].val.str.rep, "wrap") == 0) {
            f |= TextArea::WrapFlag;
        } else if(strcmp(argList[i].val.str.rep, "tail") == 0) {
            f |= TextArea::TailFlag;
        } else if(strcmp(argList[i].val.str.rep, "stutter") == 0) {
            f |= TextArea::StutterFlag;
        } else if(strcmp(argList[i].val.str.rep, "scrollbar") == 0) {
            f |= TextArea::ScrollbarFlag;
        } else if(strcmp(argList[i].val.str.rep, "nobell") == 0) {
            f |= TextArea::NoBellFlag;
        } else {
            return false;
        }
    }

    *flags = f;
    return true;
}


#define TEXT_EVENT(routineName, slotName)                                                                                 \
static int routineName(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) { \
                                                                                                                          \
    TextArea::EventFlags flags = TextArea::NoneFlag;                                                                      \
    if(!flagsFromArguments(argList, nArgs, 0, &flags)) {                                                                  \
        *errMsg = "%s called with invalid argument";                                                                      \
        return false;                                                                                                     \
    }                                                                                                                     \
                                                                                                                          \
    if(MainWindow *window = document->toWindow()) {                                                                       \
        if(TextArea *area = window->lastFocus_) {                                                                         \
            area->slotName(flags);                                                                                        \
        }                                                                                                                 \
    }                                                                                                                     \
                                                                                                                          \
    result->tag = NO_TAG;                                                                                                 \
    return true;                                                                                                          \
}

#define TEXT_EVENT_S(routineName, slotName)                                                                               \
static int routineName(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) { \
                                                                                                                          \
    if(nArgs < 1) {                                                                                                       \
        return wrongNArgsErr(errMsg);                                                                                     \
    }                                                                                                                     \
                                                                                                                          \
    QString string;                                                                                                       \
    if(!readArgument(argList[0], &string, errMsg)) {                                                                      \
        return false;                                                                                                     \
    }                                                                                                                     \
                                                                                                                          \
    TextArea::EventFlags flags = TextArea::NoneFlag;                                                                      \
    if(!flagsFromArguments(argList, nArgs, 1, &flags)) {                                                                  \
        *errMsg = "%s called with invalid argument";                                                                      \
        return false;                                                                                                     \
    }                                                                                                                     \
                                                                                                                          \
    if(MainWindow *window = document->toWindow()) {                                                                       \
        if(TextArea *area = window->lastFocus_) {                                                                         \
            area->slotName(string, flags);                                                                                \
        }                                                                                                                 \
    }                                                                                                                     \
                                                                                                                          \
    result->tag = NO_TAG;                                                                                                 \
    return true;                                                                                                          \
}

#define TEXT_EVENT_I(routineName, slotName)                                                                               \
static int routineName(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) { \
                                                                                                                          \
    if(nArgs < 1) {                                                                                                       \
        return wrongNArgsErr(errMsg);                                                                                     \
    }                                                                                                                     \
                                                                                                                          \
    int num;                                                                                                              \
    if(!readArgument(argList[0], &num, errMsg)) {                                                                         \
        return false;                                                                                                     \
    }                                                                                                                     \
                                                                                                                          \
    TextArea::EventFlags flags = TextArea::NoneFlag;                                                                      \
    if(!flagsFromArguments(argList, nArgs, 1, &flags)) {                                                                  \
        *errMsg = "%s called with invalid argument";                                                                      \
        return false;                                                                                                     \
    }                                                                                                                     \
                                                                                                                          \
    if(MainWindow *window = document->toWindow()) {                                                                       \
        if(TextArea *area = window->lastFocus_) {                                                                         \
            area->slotName(num, flags);                                                                                   \
        }                                                                                                                 \
    }                                                                                                                     \
                                                                                                                          \
    result->tag = NO_TAG;                                                                                                 \
    return true;                                                                                                          \
}

TEXT_EVENT_I(scrollToLineMS,          scrollToLineAP)
TEXT_EVENT_I(scrollLeftMS,            scrollLeftAP)
TEXT_EVENT_I(scrollRightMS,           scrollRightAP)
TEXT_EVENT_S(insertStringMS,          insertStringAP)

TEXT_EVENT(backwardWordMS,            backwardWordAP)
TEXT_EVENT(backwardCharacterMS,       backwardCharacterAP)
TEXT_EVENT(copyClipboardMS,           copyClipboardAP)
TEXT_EVENT(copyPrimaryMS,             copyPrimaryAP)
TEXT_EVENT(cutClipboardMS,            cutClipboardAP)
TEXT_EVENT(cutPrimaryMS,              cutPrimaryAP)
TEXT_EVENT(deleteNextCharacterMS,     deleteNextCharacterAP)
TEXT_EVENT(deletePreviousCharacterMS, deletePreviousCharacterAP)
TEXT_EVENT(deletePreviousWordMS,      deletePreviousWordAP)
TEXT_EVENT(deselectAllMS,             deselectAllAP)
TEXT_EVENT(forwardWordMS,             forwardWordAP)
TEXT_EVENT(keySelectMS,               keySelectAP)
TEXT_EVENT(newlineAndIndentMS,        newlineAndIndentAP)
TEXT_EVENT(newlineMS,                 newlineAP)
TEXT_EVENT(newlineNoIndentMS,         newlineNoIndentAP)
TEXT_EVENT(pasteClipboardMS,          pasteClipboardAP)
TEXT_EVENT(processCancelMS,           processCancelAP)
TEXT_EVENT(processDownMS,             processDownAP)
TEXT_EVENT(processShiftDownMS,        processShiftDownAP)
TEXT_EVENT(processShiftUpMS,          processShiftUpAP)
TEXT_EVENT(processTabMS,              processTabAP)
TEXT_EVENT(processUpMS,               processUpAP)
TEXT_EVENT(beginingOfLineMS,          beginningOfLineAP)
TEXT_EVENT(backwardParagraphMS,       backwardParagraphAP)
TEXT_EVENT(beginningOfFileMS,         beginningOfFileAP)
TEXT_EVENT(endOfFileMS,               endOfFileAP)
TEXT_EVENT(endOfLineMS,               endOfLineAP)
TEXT_EVENT(forwardParagraphMS,        forwardParagraphAP)
TEXT_EVENT(forwardCharacterMS,        forwardCharacterAP)
TEXT_EVENT(nextPageMS,                nextPageAP)
TEXT_EVENT(previousPageMS,            previousPageAP)
TEXT_EVENT(beginningOfSelectionMS,    beginningOfSelectionAP)
TEXT_EVENT(deleteSelectionMS,         deleteSelectionAP)
TEXT_EVENT(deleteNextWordMS,          deleteNextWordAP)
TEXT_EVENT(deleteToEndOfLineMS,       deleteToEndOfLineAP)
TEXT_EVENT(deleteToStartOfLineMS,     deleteToStartOfLineAP)
TEXT_EVENT(endOfSelectionMS,          endOfSelectionAP)
TEXT_EVENT(pageLeftMS,                pageLeftAP)
TEXT_EVENT(pageRightMS,               pageRightAP)

static const SubRoutine TextAreaSubrNames[] = {
    // Keyboard
    {"backward_character",        backwardCharacterMS},
    {"backward_paragraph",        backwardParagraphMS},
    {"backward_word",             backwardWordMS},
    {"beginning_of_file",         beginningOfFileMS},
    {"beginning_of_line",         beginingOfLineMS},
    {"beginning_of_selection",    beginningOfSelectionMS},
    {"copy_clipboard",            copyClipboardMS},
    {"copy_primary",              copyPrimaryMS},
    {"cut_clipboard",             cutClipboardMS},
    {"cut_primary",               cutPrimaryMS},
    {"delete_selection",          deleteSelectionMS},
    {"delete_next_character",     deleteNextCharacterMS},
    {"delete_previous_character", deletePreviousCharacterMS},
    {"delete_next_word",          deleteNextWordMS},
    {"delete_previous_word",      deletePreviousWordMS},
    {"delete_to_start_of_line",   deleteToStartOfLineMS},
    {"delete_to_end_of_line",     deleteToEndOfLineMS},
    {"deselect_all",              deselectAllMS},
    {"end_of_file",               endOfFileMS},
    {"end_of_line",               endOfLineMS},
    {"end_of_selection",          endOfSelectionMS},
    {"forward_character",         forwardCharacterMS},
    {"forward_paragraph",         forwardParagraphMS},
    {"forward_word",              forwardWordMS},
    {"insert_string",             insertStringMS},
    {"key_select",                keySelectMS},
    {"newline",                   newlineMS},
    {"newline_and_indent",        newlineAndIndentMS},
    {"newline_no_indent",         newlineNoIndentMS},
    {"next_page",                 nextPageMS},
    {"page_left",                 pageLeftMS},
    {"page_right",                pageRightMS},
    {"paste_clipboard",           pasteClipboardMS},
    {"previous_page",             previousPageMS},
    {"process_cancel",            processCancelMS},
    {"process_down",              processDownMS},
    {"process_return",            newlineMS},
    {"process_shift_down",        processShiftDownMS},
    {"process_shift_up",          processShiftUpMS},
    {"process_tab",               processTabMS},
    {"process_up",                processUpMS},
    {"scroll_down",               nullptr}, // TODO(eteran): implement this
    {"scroll_left",               scrollLeftMS},
    {"scroll_right",              scrollRightMS},
    {"scroll_up",                 nullptr}, // TODO(eteran): implement this
    {"scroll_to_line",            scrollToLineMS},
    {"self_insert",               insertStringMS},

#if 0 // NOTE(eteran): do these make sense to support
    {"focus_pane",                nullptr}, // NOTE(eteran): was from MainWindow in my code...
    {"raise_window",              nullptr}, // NOTE(eteran): was from MainWindow in my code...
#endif

#if 0 // NOTE(eteran): mouse event, no point in scripting...
    {"extend_end",                nullptr},
    {"grab_focus",                nullptr},
    {"extend_adjust",             nullptr},
    {"extend_start",              nullptr},
    {"copy_to",                   nullptr},
    {"copy_to_or_end_drag",       nullptr},
    {"secondary_or_drag_start",   nullptr},
    {"process_bdrag",             nullptr},
    {"secondary_or_drag_adjust",  nullptr},
    {"secondary_start",           nullptr},
    {"secondary_adjust",          nullptr},
    {"exchange",                  nullptr},
#endif

#if 0 // NOTE(eteran): duplicated by the main window event
    {"select_all",                selectAllMS},
#endif

#if 0 // NOTE(eteran): Not mentioned in the documentation
    {"end_drag",                  nullptr},
    {"process_home",              processHomeMS},
    {"toggle_overstrike",         toggleOverstrikeMS},
#endif
};


#define WINDOW_MENU_EVENT_S(routineName, slotName)                                                                            \
    static int routineName(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) { \
                                                                                                                              \
        /* ensure that we are dealing with the document which currently has the focus */                                      \
        document = MacroRunWindowEx();                                                                                        \
                                                                                                                              \
        QString string;                                                                                                       \
        if(!readArguments(argList, nArgs, 0, errMsg, &string)) {                                                              \
            return false;                                                                                                     \
        }                                                                                                                     \
                                                                                                                              \
        if(MainWindow *window = document->toWindow()) {                                                                       \
            window->slotName(string);                                                                                         \
        }                                                                                                                     \
                                                                                                                              \
        result->tag = NO_TAG;                                                                                                 \
        return true;                                                                                                          \
    }

#define WINDOW_MENU_EVENT(routineName, slotName)                                                                              \
    static int routineName(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) { \
        Q_UNUSED(argList);                                                                                                    \
                                                                                                                              \
        /* ensure that we are dealing with the document which currently has the focus */                                      \
        document = MacroRunWindowEx();                                                                                        \
                                                                                                                              \
        if(nArgs != 0) {                                                                                                      \
            return wrongNArgsErr(errMsg);                                                                                     \
        }                                                                                                                     \
                                                                                                                              \
        if(MainWindow *window = document->toWindow()) {                                                                       \
            window->slotName();                                                                                               \
        }                                                                                                                     \
                                                                                                                              \
        result->tag = NO_TAG;                                                                                                 \
        return true;                                                                                                          \
    }

// These emit functions to support calling them from macros, see WINDOW_MENU_EVENT for what
// these functions will look like
WINDOW_MENU_EVENT_S(includeFileMS,                   action_Include_File)
WINDOW_MENU_EVENT_S(gotoLineNumberMS,                action_Goto_Line_Number)
WINDOW_MENU_EVENT_S(openMS,                          action_Open)
WINDOW_MENU_EVENT_S(loadMacroFileMS,                 action_Load_Macro_File)
WINDOW_MENU_EVENT_S(loadTagsFileMS,                  action_Load_Tags_File)
WINDOW_MENU_EVENT_S(unloadTagsFileMS,                action_Unload_Tags_File)
WINDOW_MENU_EVENT_S(loadTipsFileMS,                  action_Load_Tips_File)
WINDOW_MENU_EVENT_S(unloadTipsFileMS,                action_Unload_Tips_File)
WINDOW_MENU_EVENT_S(markMS,                          action_Mark)
WINDOW_MENU_EVENT_S(filterSelectionMS,               action_Filter_Selection)
WINDOW_MENU_EVENT_S(executeCommandMS,                action_Execute_Command)
WINDOW_MENU_EVENT_S(shellMenuCommandMS,              action_Shell_Menu_Command)
WINDOW_MENU_EVENT_S(macroMenuCommandMS,              action_Macro_Menu_Command)

WINDOW_MENU_EVENT(closePaneMS,                       on_action_Close_Pane_triggered)
WINDOW_MENU_EVENT(deleteMS,                          on_action_Delete_triggered)
WINDOW_MENU_EVENT(exitMS,                            on_action_Exit_triggered)
WINDOW_MENU_EVENT(fillParagraphMS,                   on_action_Fill_Paragraph_triggered)
WINDOW_MENU_EVENT(gotoLineNumberDialogMS,            on_action_Goto_Line_Number_triggered)
WINDOW_MENU_EVENT(gotoSelectedMS,                    on_action_Goto_Selected_triggered)
WINDOW_MENU_EVENT(gotoMatchingMS,                    on_action_Goto_Matching_triggered)
WINDOW_MENU_EVENT(includeFileDialogMS,               on_action_Include_File_triggered)
WINDOW_MENU_EVENT(insertControlCodeDialogMS,         on_action_Insert_Ctrl_Code_triggered)
WINDOW_MENU_EVENT(loadMacroFileDialogMS,             on_action_Load_Macro_File_triggered)
WINDOW_MENU_EVENT(loadTagsFileDialogMS,              on_action_Load_Tags_File_triggered)
WINDOW_MENU_EVENT(loadTipsFileDialogMS,              on_action_Load_Calltips_File_triggered)
WINDOW_MENU_EVENT(lowercaseMS,                       on_action_Lower_case_triggered)
WINDOW_MENU_EVENT(openSelectedMS,                    on_action_Open_Selected_triggered)
WINDOW_MENU_EVENT(printMS,                           on_action_Print_triggered)
WINDOW_MENU_EVENT(printSelectionMS,                  on_action_Print_Selection_triggered)
WINDOW_MENU_EVENT(redoMS,                            on_action_Redo_triggered)
WINDOW_MENU_EVENT(saveAsDialogMS,                    on_action_Save_As_triggered)
WINDOW_MENU_EVENT(saveMS,                            on_action_Save_triggered)
WINDOW_MENU_EVENT(selectAllMS,                       on_action_Select_All_triggered)
WINDOW_MENU_EVENT(shiftLeftMS,                       on_action_Shift_Left_triggered)
WINDOW_MENU_EVENT(shiftLeftTabMS,                    action_Shift_Left_Tabs)
WINDOW_MENU_EVENT(shiftRightMS,                      on_action_Shift_Right_triggered)
WINDOW_MENU_EVENT(shiftRightTabMS,                   action_Shift_Right_Tabs)
WINDOW_MENU_EVENT(splitPaneMS,                       on_action_Split_Pane_triggered)
WINDOW_MENU_EVENT(undoMS,                            on_action_Undo_triggered)
WINDOW_MENU_EVENT(uppercaseMS,                       on_action_Upper_case_triggered)
WINDOW_MENU_EVENT(openDialogMS,                      on_action_Open_triggered)
WINDOW_MENU_EVENT(revertToSavedDialogMS,             on_action_Revert_to_Saved_triggered)
WINDOW_MENU_EVENT(markDialogMS,                      on_action_Mark_triggered)
WINDOW_MENU_EVENT(showTipMS,                         on_action_Show_Calltip_triggered)
WINDOW_MENU_EVENT(selectToMatchingMS,                action_Shift_Goto_Matching_triggered)
WINDOW_MENU_EVENT(filterSelectionDialogMS,           on_action_Filter_Selection_triggered)
WINDOW_MENU_EVENT(executeCommandDialogMS,            on_action_Execute_Command_triggered)
WINDOW_MENU_EVENT(executeCommandLineMS,              on_action_Execute_Command_Line_triggered)
WINDOW_MENU_EVENT(repeatMacroDialogMS,               on_action_Repeat_triggered)
WINDOW_MENU_EVENT(detachDocumentMS,                  on_action_Detach_Tab_triggered)
WINDOW_MENU_EVENT(moveDocumentMS,                    on_action_Move_Tab_To_triggered)

/*
** Scans action argument list for arguments "forward" or "backward" to
** determine search direction for search and replace actions.  "ignoreArgs"
** tells the routine how many required arguments there are to ignore before
** looking for keywords
*/
static SearchDirection searchDirection(DataValue *argList, int nArgs, int index) {
    for(int i = index; i < nArgs; ++i) {
        QString arg;
        if (!readArgument(argList[i], &arg)) {
            return SEARCH_FORWARD;
        }

        if (arg.compare(QLatin1String("forward"), Qt::CaseInsensitive) == 0)
            return SEARCH_FORWARD;

        if (arg.compare(QLatin1String("backward"), Qt::CaseInsensitive) == 0)
            return SEARCH_BACKWARD;
    }

    return SEARCH_FORWARD;
}


/*
** Scans action argument list for arguments "keep" or "nokeep" to
** determine whether to keep dialogs up for search and replace.  "ignoreArgs"
** tells the routine how many required arguments there are to ignore before
** looking for keywords
*/
static int searchKeepDialogs(DataValue *argList, int nArgs, int index) {
    for(int i = index; i < nArgs; ++i) {
        QString arg;
        if (!readArgument(argList[i], &arg)) {
            return GetPrefKeepSearchDlogs();
        }

        if (arg.compare(QLatin1String("keep"), Qt::CaseInsensitive) == 0)
            return true;

        if (arg.compare(QLatin1String("nokeep"), Qt::CaseInsensitive) == 0)
            return false;
    }

    return GetPrefKeepSearchDlogs();
}


/*
** Scans action argument list for arguments "wrap" or "nowrap" to
** determine search direction for search and replace actions.  "ignoreArgs"
** tells the routine how many required arguments there are to ignore before
** looking for keywords
*/
static WrapMode searchWrap(DataValue *argList, int nArgs, int index) {
    for(int i = index; i < nArgs; ++i) {
        QString arg;
        if (!readArgument(argList[i], &arg)) {
            return GetPrefSearchWraps();
        }

        if (arg.compare(QLatin1String("wrap"), Qt::CaseInsensitive) == 0)
            return WrapMode::Wrap;

        if (arg.compare(QLatin1String("nowrap"), Qt::CaseInsensitive) == 0)
            return WrapMode::NoWrap;
    }

    return GetPrefSearchWraps();
}

/*
** Parses a search type description string. If the string contains a valid
** search type description, returns TRUE and writes the corresponding
** SearchType in searchType. Returns FALSE and leaves searchType untouched
** otherwise. (Originally written by Markus Schwarzenberg; slightly adapted).
*/
// TODO(eteran): this is redundant to the "searchType" function below
// let's factor it out...
bool StringToSearchType(const QString &string, SearchType *searchType) {

    static const struct {
        QString name;
        SearchType type;
    } searchTypeStrings[] = {
        { QLatin1String("literal"),     SearchType::SEARCH_LITERAL },
        { QLatin1String("case"),        SearchType::SEARCH_CASE_SENSE },
        { QLatin1String("regex"),       SearchType::SEARCH_REGEX },
        { QLatin1String("word"),        SearchType::SEARCH_LITERAL_WORD },
        { QLatin1String("caseWord"),    SearchType::SEARCH_CASE_SENSE_WORD },
        { QLatin1String("regexNoCase"), SearchType::SEARCH_REGEX_NOCASE },
    };

    for(const auto &entry : searchTypeStrings) {
        if (string.compare(entry.name, Qt::CaseInsensitive) == 0) {
            *searchType = entry.type;
            return true;
        }
    }

    *searchType = GetPrefSearch();
    return false;
}


/*
** Scans action argument list for arguments "literal", "case" or "regex" to
** determine search type for search and replace actions.  "ignoreArgs"
** tells the routine how many required arguments there are to ignore before
** looking for keywords
*/
static SearchType searchType(DataValue *argList, int nArgs, int index) {
    const char *errMsg = nullptr;
    for(int i = index; i < nArgs; ++i) {
        QString arg;
        if (!readArgument(argList[i], &arg, &errMsg)) {
            return GetPrefSearch();
        }

        if (arg.compare(QLatin1String("literal"), Qt::CaseInsensitive) == 0)
            return SearchType::SEARCH_LITERAL;
        if (arg.compare(QLatin1String("case"), Qt::CaseInsensitive) == 0)
            return SearchType::SEARCH_CASE_SENSE;
        if (arg.compare(QLatin1String("regex"), Qt::CaseInsensitive) == 0)
            return SearchType::SEARCH_REGEX;
        if (arg.compare(QLatin1String("word"), Qt::CaseInsensitive) == 0)
            return SearchType::SEARCH_LITERAL_WORD;
        if (arg.compare(QLatin1String("caseWord"), Qt::CaseInsensitive) == 0)
            return SearchType::SEARCH_CASE_SENSE_WORD;
        if (arg.compare(QLatin1String("regexNoCase"), Qt::CaseInsensitive) == 0)
            return SearchType::SEARCH_REGEX_NOCASE;
    }

    return GetPrefSearch();
}

static int closeMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunWindowEx();

    if(nArgs > 1) {
        return wrongNArgsErr(errMsg);
    }

    CloseMode mode = CloseMode::Prompt;

    if(nArgs == 1) {
        QString string;
        if(!readArguments(argList, nArgs, 0, errMsg, &string)) {
            return false;
        }

        static const struct {
            QLatin1String name;
            CloseMode type;
        } openTypeStrings [] = {
            { QLatin1String("prompt"), CloseMode::Prompt },
            { QLatin1String("save"),   CloseMode::Save },
            { QLatin1String("nosave"), CloseMode::NoSave },
        };

        for(const auto &entry : openTypeStrings) {
            if (string.compare(entry.name, Qt::CaseInsensitive) == 0) {
                mode = entry.type;
                break;
            }
        }
    }

    if(MainWindow *window = document->toWindow()) {
        window->action_Close(mode);
    }

    result->tag = NO_TAG;
    return true;
}

static int newMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunWindowEx();

    if(nArgs > 1) {
        return wrongNArgsErr(errMsg);
    }

    NewMode mode = NewMode::Prefs;

    if(nArgs == 1) {
        QString string;
        if(!readArguments(argList, nArgs, 0, errMsg, &string)) {
            return false;
        }

        static const struct {
            QLatin1String name;
            NewMode type;
        } openTypeStrings [] = {
            { QLatin1String("tab"),      NewMode::Tab },
            { QLatin1String("window"),   NewMode::Window },
            { QLatin1String("prefs"),    NewMode::Prefs },
            { QLatin1String("opposite"), NewMode::Opposite }
        };

        for(const auto &entry : openTypeStrings) {
            if (string.compare(entry.name, Qt::CaseInsensitive) == 0) {
                mode = entry.type;
                break;
            }
        }
    }

    if(MainWindow *window = document->toWindow()) {
        window->action_New(mode);
    }

    result->tag = NO_TAG;
    return true;
}

static int saveAsMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunWindowEx();

    if(nArgs > 2 || nArgs == 0) {
        return wrongNArgsErr(errMsg);
    }

    QString filename;
    if (!readArgument(argList[0], &filename, errMsg)) {
        return false;
    }

    bool wrapped = false;

    // NOTE(eteran): "wrapped" optional argument is not documented
    if(nArgs == 2) {
        QString string;
        if(!readArgument(argList[1], &string, errMsg)) {
            return false;
        }

        if(string.compare(QLatin1String("wrapped"), Qt::CaseInsensitive) == 0) {
            wrapped = true;
        }
    }

    if(MainWindow *window = document->toWindow()) {
        window->action_Save_As(filename, wrapped);
    }

    result->tag = NO_TAG;
    return true;
}

static int findMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    // find( search-string [, search-direction] [, search-type] [, search-wrap] )

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunWindowEx();

    if(nArgs > 4 || nArgs == 0) {
        return wrongNArgsErr(errMsg);
    }

    QString string;
    if(!readArguments(argList, nArgs, 0, errMsg, &string)) {
        return false;
    }

    SearchDirection direction = searchDirection(argList, nArgs, 1);
    SearchType      type      = searchType(argList, nArgs, 1);
    WrapMode        wrap      = searchWrap(argList, nArgs, 1);

    if(MainWindow *window = document->toWindow()) {
        window->action_Find(string, direction, type, wrap);
    }

    result->tag = NO_TAG;
    return true;
}

static int findDialogMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunWindowEx();

    SearchDirection direction = searchDirection(argList, nArgs, 0);
    SearchType      type      = searchType(argList, nArgs, 0);
    bool            keep      = searchKeepDialogs(argList, nArgs, 0);

    if(MainWindow *window = document->toWindow()) {
        window->action_Find_Dialog(direction, type, keep);
    }

    result->tag = NO_TAG;
    return true;
}

static int findAgainMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunWindowEx();

    SearchDirection direction = searchDirection(argList, nArgs, 0);
    WrapMode        wrap      = searchWrap(argList, nArgs, 0);

    if(MainWindow *window = document->toWindow()) {
        window->action_Find_Again(direction, wrap);
    }

    result->tag = NO_TAG;
    return true;
}

static int findSelectionMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunWindowEx();

    SearchDirection direction = searchDirection(argList, nArgs, 0);
    SearchType      type      = searchType(argList, nArgs, 0);
    WrapMode        wrap      = searchWrap(argList, nArgs, 0);

    if(MainWindow *window = document->toWindow()) {
        window->action_Find_Selection(direction, type, wrap);
    }

    result->tag = NO_TAG;
    return true;
}

static int replaceMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunWindowEx();

    if(nArgs > 5 || nArgs < 2) {
        return wrongNArgsErr(errMsg);
    }

    QString searchString;
    QString replaceString;
    if(!readArguments(argList, nArgs, 0, errMsg, &searchString, &replaceString)) {
        return false;
    }

    SearchDirection direction = searchDirection(argList, nArgs, 2);
    SearchType      type      = searchType(argList, nArgs, 2);
    WrapMode        wrap      = searchWrap(argList, nArgs, 2);

    if(MainWindow *window = document->toWindow()) {
        window->action_Replace(direction, searchString, replaceString, type, wrap);
    }

    result->tag = NO_TAG;
    return true;
}

static int replaceDialogMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunWindowEx();

    SearchDirection direction = searchDirection(argList, nArgs, 0);
    SearchType      type      = searchType(argList, nArgs, 0);
    bool            keep      = searchKeepDialogs(argList, nArgs, 0);

    if(MainWindow *window = document->toWindow()) {
        window->action_Replace_Dialog(direction, type, keep);
    }

    result->tag = NO_TAG;
    return true;
}

static int replaceAgainMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    // replace_in_selection( search-string, replace-string [, search-type] )

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunWindowEx();

    if(nArgs != 2) {
        return wrongNArgsErr(errMsg);
    }

    WrapMode wrap             = searchWrap(argList, nArgs, 0);
    SearchDirection direction = searchDirection(argList, nArgs, 0);

    if(MainWindow *window = document->toWindow()) {
        window->action_Replace_Again(direction, wrap);
    }

    result->tag = NO_TAG;

    return true;
}

static int gotoMarkMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    // goto_mark( mark, [, extend] )

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunWindowEx();

    if(nArgs > 2 || nArgs < 1) {
        return wrongNArgsErr(errMsg);
    }

    bool extend = false;

    QString mark;
    if(!readArguments(argList, nArgs, 0, errMsg, &mark)) {
        return false;
    }

    if(nArgs == 2) {
        QString argument;
        if(!readArgument(argList[1], &argument, errMsg)) {
            return false;
        }

        if(argument.compare(QLatin1String("extend"), Qt::CaseInsensitive) == 0) {
            extend = true;
        }
    }

    if(MainWindow *window = document->toWindow()) {
        window->action_Goto_Mark(mark, extend);
    }

    result->tag = NO_TAG;
    return true;
}

static int gotoMarkDialogMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    // goto_mark( mark, [, extend] )

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunWindowEx();

    if(nArgs > 1) {
        return wrongNArgsErr(errMsg);
    }

    bool extend = false;

    if(nArgs == 1) {
        QString argument;
        if(!readArgument(argList[1], &argument, errMsg)) {
            return false;
        }

        if(argument.compare(QLatin1String("extend"), Qt::CaseInsensitive) == 0) {
            extend = true;
        }
    }

    if(MainWindow *window = document->toWindow()) {
        window->action_Goto_Mark_Dialog(extend);
    }

    result->tag = NO_TAG;
    return true;
}

static int findDefinitionMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    // find_definition( [ argument ] )

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunWindowEx();

    if(nArgs > 1) {
        return wrongNArgsErr(errMsg);
    }

    QString argument;

    if(nArgs == 1) {
        if(!readArgument(argList[1], &argument, errMsg)) {
            return false;
        }
    }

    if(MainWindow *window = document->toWindow()) {
        window->action_Find_Definition(argument);
    }

    result->tag = NO_TAG;
    return true;
}

static int repeatMacroMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    // repeat_macro( how/count, method )

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunWindowEx();

    QString howString;
    QString method;

    if(!readArguments(argList, nArgs, 0, errMsg, &howString, &method)) {
        return false;
    }

    int how;

    if(howString.compare(QLatin1String("in_selection"), Qt::CaseInsensitive) == 0) {
        how = REPEAT_IN_SEL;
    } else if(howString.compare(QLatin1String("to_end"), Qt::CaseInsensitive) == 0) {
        how = REPEAT_TO_END;
    } else {
        bool ok;
        how = howString.toInt(&ok);
        if(!ok) {
            qWarning("NEdit: repeat_macro requires method/count");
            return false;
        }
    }

    if(MainWindow *window = document->toWindow()) {
        window->action_Repeat_Macro(method, how);
    }

    result->tag = NO_TAG;
    return true;
}

static int detachDocumentDialogMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(argList);
    Q_UNUSED(nArgs);
    Q_UNUSED(result);
    Q_UNUSED(errMsg);

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunWindowEx();

    if(MainWindow *window = document->toWindow()) {
        if(window->TabCount() < 2) {
            return false;
        }

        int r = QMessageBox::question(
                    nullptr,
                    QLatin1String("Detach Tab"),
                    QString(QLatin1String("Detach %1?")).arg(document->filename_), QMessageBox::Yes | QMessageBox::No);

        if(r == QMessageBox::Yes) {
            window->action_Detach_Document(document);
        }
    }

    return true;
}

static int setAutoIndentMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    QString string;
    if(!readArguments(argList, nArgs, 0, errMsg, &string)) {
        qWarning("NEdit: set_auto_indent requires argument");
        return false;
    }

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunWindowEx();

    if(string == QLatin1String("off")) {
        document->SetAutoIndent(NO_AUTO_INDENT);
    } else if(string == QLatin1String("on")) {
        document->SetAutoIndent(AUTO_INDENT);
    } else if(string == QLatin1String("smart")) {
        document->SetAutoIndent(SMART_INDENT);
    } else {
        qWarning("NEdit: set_auto_indent invalid argument");
    }

    result->tag = NO_TAG;
    return true;
}

static int setEmTabDistMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    int number;
    if(!readArguments(argList, nArgs, 0, errMsg, &number)) {
        qWarning("NEdit: set_em_tab_dist requires argument");
        return false;
    }

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunWindowEx();

    if (number < 1000) {
        if (number < 0) {
            number = 0;
        }
        document->SetEmTabDist(number);
    } else {
        qWarning("NEdit: set_em_tab_dist requires integer argument >= -1 and < 1000");
    }

    result->tag = NO_TAG;
    return true;
}

static int setFontsMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunWindowEx();

    QString fontName;
    QString italicName;
    QString boldName;
    QString boldItalicName;
    if(!readArguments(argList, nArgs, 0, errMsg, &fontName, &italicName, &boldName, &boldItalicName)) {
        qWarning("NEdit: set_fonts requires 4 arguments");
        return false;
    }

    document->SetFonts(fontName, italicName, boldName, boldItalicName);

    result->tag = NO_TAG;
    return true;
}


#define ACTION_BOOL_PARAM_OR_TOGGLE(newState, nArgs, argList, oValue, actionName) \
    do {                                                                          \
        switch(nArgs) {                                                           \
        case 1:                                                                   \
            if(!readArguments(argList, nArgs, 0, errMsg, &newState)) {            \
                return false;                                                     \
            }                                                                     \
            break;                                                                \
        case 0:                                                                   \
            newState = !(oValue);                                                 \
            break;                                                                \
        default:                                                                  \
            qWarning("NEdit: %s requires 0 or 1 arguments", actionName);             \
            return false;                                                         \
        }                                                                         \
    } while(0)



static int setHighlightSyntaxMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    document = MacroRunWindowEx();
    int newState;

    ACTION_BOOL_PARAM_OR_TOGGLE(newState, nArgs, argList, document->highlightSyntax_, "set_highlight_syntax");

    document->highlightSyntax_ = newState;

    if(document->IsTopDocument()) {
        if(auto win = document->toWindow()) {
            no_signals(win->ui.action_Highlight_Syntax)->setChecked(newState);
        }
    }

    if (document->highlightSyntax_) {
        StartHighlightingEx(document, true);
    } else {
        document->StopHighlightingEx();
    }

    result->tag = NO_TAG;
    return true;
}

static int setIncrementalBackupMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    document = MacroRunWindowEx();
    int newState;

    ACTION_BOOL_PARAM_OR_TOGGLE(newState, nArgs, argList, document->autoSave_, "set_incremental_backup");

    document->autoSave_ = newState;

    if(document->IsTopDocument()) {
        if(auto win = document->toWindow()) {
            no_signals(win->ui.action_Highlight_Syntax)->setChecked(newState);
        }
    }

    result->tag = NO_TAG;
    return true;
}

static int setIncrementalSearchLineMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    document = MacroRunWindowEx();

    MainWindow *win = document->toWindow();
    if(!win) {
        return false;
    }

    int newState;
    ACTION_BOOL_PARAM_OR_TOGGLE(newState, nArgs, argList, win->showISearchLine_, "set_incremental_search_line");

    win->ui.action_Incremental_Search_Line->setChecked(newState);

    result->tag = NO_TAG;
    return true;
}

static int setMakeBackupCopyMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    document = MacroRunWindowEx();
    int newState;

    ACTION_BOOL_PARAM_OR_TOGGLE(newState, nArgs, argList, document->saveOldVersion_, "set_make_backup_copy");

    if (document->IsTopDocument()) {
        if(auto win = document->toWindow()) {
            no_signals(win->ui.action_Make_Backup_Copy)->setChecked(newState);
        }
    }

    document->saveOldVersion_ = newState;

    result->tag = NO_TAG;
    return true;
}

static int setLockedMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    document = MacroRunWindowEx();
    int newState;

    ACTION_BOOL_PARAM_OR_TOGGLE(newState, nArgs, argList, document->lockReasons_.isUserLocked(), "set_locked");

    document->lockReasons_.setUserLocked(newState);

    if(document->IsTopDocument()) {
        if(auto win = document->toWindow()) {
            no_signals(win->ui.action_Read_Only)->setChecked(document->lockReasons_.isAnyLocked());
            win->UpdateWindowTitle(document);
            win->UpdateWindowReadOnly(document);
        }
    }

    result->tag = NO_TAG;
    return true;
}

static int setLanguageModeMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    document = MacroRunWindowEx();

    QString languageMode;
    if(!readArguments(argList, nArgs, 0, errMsg, &languageMode)) {
        qWarning("NEdit: set_language_mode requires argument");
        return false;
    }

    document->SetLanguageMode(FindLanguageMode(languageMode), false);
    result->tag = NO_TAG;
    return true;
}

static int setOvertypeModeMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    document = MacroRunWindowEx();
    int newState;

    ACTION_BOOL_PARAM_OR_TOGGLE(newState, nArgs, argList, document->saveOldVersion_, "set_overtype_mode");

    if(document->IsTopDocument()) {
        if(auto win = document->toWindow()) {
            no_signals(win->ui.action_Overtype)->setChecked(newState);
        }
    }

    document->SetOverstrike(newState);
    result->tag = NO_TAG;
    return true;
}

static int setShowLineNumbersMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    document = MacroRunWindowEx();

    auto win = document->toWindow();

    if(!win) {
        return false;
    }

    int newState;

    ACTION_BOOL_PARAM_OR_TOGGLE(newState, nArgs, argList, win->showLineNumbers_, "set_overtype_mode");

    no_signals(win->ui.action_Show_Line_Numbers)->setChecked(newState);

    win->showLineNumbers_ = newState;
    result->tag = NO_TAG;
    return true;
}

static int setShowMatchingMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    document = MacroRunWindowEx();

    if (nArgs > 0) {
        QString arg;
        if(!readArgument(argList[0], &arg, errMsg)) {
            return false;
        }

        if (arg == QLatin1String(NO_FLASH_STRING)) {
            document->SetShowMatching(NO_FLASH);
        } else if (arg == QLatin1String(FLASH_DELIMIT_STRING)) {
            document->SetShowMatching(FLASH_DELIMIT);
        } else if (arg == QLatin1String(FLASH_RANGE_STRING)) {
            document->SetShowMatching(FLASH_RANGE);
        }
        /* For backward compatibility with pre-5.2 versions, we also
           accept 0 and 1 as aliases for NO_FLASH and FLASH_DELIMIT.
           It is quite unlikely, though, that anyone ever used this
           action procedure via the macro language or a key binding,
           so this can probably be left out safely. */
        else if (arg == QLatin1String("0")) {
           document->SetShowMatching(NO_FLASH);
        } else if (arg == QLatin1String("1")) {
           document->SetShowMatching(FLASH_DELIMIT);
        } else {
            qWarning("NEdit: Invalid argument for set_show_matching");
        }
    } else {
        qWarning("NEdit: set_show_matching requires argument");
    }


    result->tag = NO_TAG;
    return true;
}

static int setMatchSyntaxBasedMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    document = MacroRunWindowEx();

    int newState;

    ACTION_BOOL_PARAM_OR_TOGGLE(newState, nArgs, argList, document->matchSyntaxBased_, "set_match_syntax_based");

    if(document->IsTopDocument()) {
        if(auto win = document->toWindow()) {
            no_signals(win->ui.action_Matching_Syntax)->setChecked(newState);
        }
    }

    document->matchSyntaxBased_ = newState;
    result->tag = NO_TAG;
    return true;
}

static int setStatisticsLineMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    document = MacroRunWindowEx();

    int newState;

    ACTION_BOOL_PARAM_OR_TOGGLE(newState, nArgs, argList, document->showStats_, "set_statistics_line");

    // stats line is a shell-level item, so we toggle the button state
    // regardless of it's 'topness'
    if(auto win = document->toWindow()) {
        no_signals(win->ui.action_Statistics_Line)->setChecked(newState);
    }

    document->showStats_ = newState;
    result->tag = NO_TAG;
    return true;
}

static int setTabDistMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    document = MacroRunWindowEx();

    if (nArgs > 0) {
        int newTabDist = 0;
        if(!readArguments(argList, nArgs, 0, errMsg, &newTabDist) && newTabDist > 0 && newTabDist <= MAX_EXP_CHAR_LEN) {
            document->SetTabDist(newTabDist);
        } else {
            qWarning("NEdit: set_tab_dist requires integer argument > 0 and <= %d", MAX_EXP_CHAR_LEN);
        }
    } else {
        qWarning("NEdit: set_tab_dist requires argument");
    }

    result->tag = NO_TAG;
    return true;
}

static int setUseTabsMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    document = MacroRunWindowEx();

    int newState;
    ACTION_BOOL_PARAM_OR_TOGGLE(newState, nArgs, argList, document->buffer_->useTabs_, "set_use_tabs");

    document->buffer_->useTabs_ = newState;
    result->tag = NO_TAG;
    return true;
}

static int setWrapMarginMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    document = MacroRunWindowEx();

    if (nArgs > 0) {
        int newMargin = 0;
        if(!readArguments(argList, nArgs, 0, errMsg, &newMargin) && newMargin > 0 && newMargin <= 1000) {

            QList<TextArea *> panes = document->textPanes();

            for(TextArea *area : panes) {
                area->setWrapMargin(newMargin);
            }
        } else {
            qWarning("NEdit: set_wrap_margin requires integer argument >= 0 and < 1000");
        }
    } else {
        qWarning("NEdit: set_wrap_margin requires argument");
    }

    result->tag = NO_TAG;
    return true;
}

static int setWrapTextMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    document = MacroRunWindowEx();

    if (nArgs > 0) {

        QString arg;
        if(!readArgument(argList[0], &arg, errMsg)) {
            return false;
        }

        if (arg == QLatin1String("none")) {
            document->SetAutoWrap(NO_WRAP);
        } else if (arg == QLatin1String("auto")) {
            document->SetAutoWrap(NEWLINE_WRAP);
        } else if (arg == QLatin1String("continuous")) {
           document->SetAutoWrap(CONTINUOUS_WRAP);
        } else {
            qWarning("NEdit: set_wrap_text invalid argument");
        }
    } else {
        qWarning("NEdit: set_wrap_text requires argument");
    }

    result->tag = NO_TAG;
    return true;
}

static int findIncrMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    document = MacroRunWindowEx();

    auto win = document->toWindow();

    if(!win) {
        return false;
    }

    int i;
    bool continued = false;

    QString arg;
    if(!readArgument(argList[0], &arg, errMsg)) {
        qWarning("NEdit: find action requires search string argument");
        return false;
    }


    for (i = 1; i < nArgs; ++i)  {
        QString arg2;
        if(!readArgument(argList[0], &arg2, errMsg)) {
            return false;
        }

        if(arg2.compare(QLatin1String("continued"), Qt::CaseInsensitive) == 0) {
            continued = true;
        }
    }

    SearchAndSelectIncrementalEx(
        win,
        document,
        win->lastFocus_,
        searchDirection(argList, nArgs, 1),
        arg,
        searchType(argList, nArgs, 1),
        searchWrap(argList, nArgs, 1),
        continued);

    result->tag = NO_TAG;
    return true;
}



static const SubRoutine MenuMacroSubrNames[] = {
    // File
    { "new",                          newMS },
    { "open",                         openMS },
    { "open_dialog",                  openDialogMS },
    { "open_selected",                openSelectedMS },
    { "close",                        closeMS },
    { "save",                         saveMS },
    { "save_as",                      saveAsMS },
    { "save_as_dialog",               saveAsDialogMS },
    { "revert_to_saved_dialog",       revertToSavedDialogMS },
    { "include_file",                 includeFileMS },
    { "include_file_dialog",          includeFileDialogMS },
    { "load_macro_file",              loadMacroFileMS },
    { "load_macro_file_dialog",       loadMacroFileDialogMS },
    { "load_tags_file",               loadTagsFileMS },
    { "load_tags_file_dialog",        loadTagsFileDialogMS },
    { "unload_tags_file",             unloadTagsFileMS },
    { "load_tips_file",               loadTipsFileMS },
    { "load_tips_file_dialog",        loadTipsFileDialogMS },
    { "unload_tips_file",             unloadTipsFileMS },
    { "print",                        printMS },
    { "print_selection",              printSelectionMS },
    { "exit",                         exitMS },

    // Edit
    { "undo",                         undoMS },
    { "redo",                         redoMS },
    { "delete",                       deleteMS },
    { "select_all",                   selectAllMS },
    { "shift_left",                   shiftLeftMS },
    { "shift_left_by_tab",            shiftLeftTabMS },
    { "shift_right",                  shiftRightMS },
    { "shift_right_by_tab",           shiftRightTabMS },
    { "uppercase",                    uppercaseMS },
    { "lowercase",                    lowercaseMS },
    { "fill_paragraph",               fillParagraphMS },
    { "control_code_dialog",          insertControlCodeDialogMS },

    // Search
    { "find",                         findMS },
    { "find_dialog",                  findDialogMS },
    { "find_again",                   findAgainMS },
    { "find_selection",               findSelectionMS },
    { "replace",                      replaceMS },
    { "replace_dialog",               replaceDialogMS },
    { "replace_all",                  replaceAllMS },
    { "replace_in_selection",         replaceAllInSelectionMS },
    { "replace_again",                replaceAgainMS },
    { "goto_line_number",             gotoLineNumberMS },
    { "goto_line_number_dialog",      gotoLineNumberDialogMS },
    { "goto_selected",                gotoSelectedMS },
    { "mark",                         markMS },
    { "mark_dialog",                  markDialogMS },
    { "goto_mark",                    gotoMarkMS },
    { "goto_mark_dialog",             gotoMarkDialogMS },
    { "goto_matching",                gotoMatchingMS },
    { "select_to_matching",           selectToMatchingMS },
    { "find_definition",              findDefinitionMS },
    { "show_tip",                     showTipMS },

    // Shell
    { "filter_selection_dialog",      filterSelectionDialogMS },
    { "filter_selection",             filterSelectionMS },
    { "execute_command",              executeCommandMS },
    { "execute_command_dialog",       executeCommandDialogMS },
    { "execute_command_line",         executeCommandLineMS },
    { "shell_menu_command",           shellMenuCommandMS },

    // Macro
    { "macro_menu_command",           macroMenuCommandMS },
    { "repeat_macro",                 repeatMacroMS },
    { "repeat_dialog",                repeatMacroDialogMS },

    // Windows
    { "split_pane",                   splitPaneMS },
    { "close_pane",                   closePaneMS },
    { "detach_document",              detachDocumentMS },
    { "detach_document_dialog",       detachDocumentDialogMS },
    { "move_document_dialog",         moveDocumentMS },

    // Preferences
    { "set_auto_indent",              setAutoIndentMS },
    { "set_em_tab_dist",              setEmTabDistMS },
    { "set_fonts",                    setFontsMS },
    { "set_highlight_syntax",         setHighlightSyntaxMS },
    { "set_incremental_backup",       setIncrementalBackupMS },
    { "set_incremental_search_line",  setIncrementalSearchLineMS },
    { "set_language_mode",            setLanguageModeMS },
    { "set_locked",                   setLockedMS },
    { "set_make_backup_copy",         setMakeBackupCopyMS },
    { "set_overtype_mode",            setOvertypeModeMS },
    { "set_show_line_numbers",        setShowLineNumbersMS },
    { "set_show_matching",            setShowMatchingMS },
    { "set_match_syntax_based",       setMatchSyntaxBasedMS },
    { "set_statistics_line",          setStatisticsLineMS },
    { "set_tab_dist",                 setTabDistMS },
    { "set_use_tabs",                 setUseTabsMS },
    { "set_wrap_margin",              setWrapMarginMS },
    { "set_wrap_text",                setWrapTextMS },

    // Deprecated
    { "match",                        selectToMatchingMS },

#if 1 // These aren't mentioned in the documentation...
    { "find_incremental",             findIncrMS },
    { "start_incremental_find",       nullptr }, // NOTE(eteran): here
    { "replace_find",                 nullptr },
    { "replace_find_same",            nullptr },
    { "replace_find_again",           nullptr },
    { "next_document",                nullptr },
    { "previous_document",            nullptr },
    { "last_document",                nullptr },
    { "bg_menu_command",              nullptr },
    { "post_window_bg_menu",          nullptr },
    { "post_tab_context_menu",        nullptr },
#endif

};


// Built-in subroutines and variables for the macro language
static const SubRoutine MacroSubrs[] = {
    { "length",                  lengthMS },
    { "get_range",               getRangeMS },
    { "t_print",                 tPrintMS },
    { "dialog",                  dialogMS },
    { "string_dialog",           stringDialogMS },
    { "replace_range",           replaceRangeMS },
    { "replace_selection",       replaceSelectionMS },
    { "set_cursor_pos",          setCursorPosMS },
    { "get_character",           getCharacterMS },
    { "min",                     minMS },
    { "max",                     maxMS },
    { "search",                  searchMS },
    { "search_string",           searchStringMS },
    { "substring",               substringMS },
    { "replace_substring",       replaceSubstringMS },
    { "read_file",               readFileMS },
    { "write_file",              writeFileMS },
    { "append_file",             appendFileMS },
    { "beep",                    beepMS },
    { "get_selection",           getSelectionMS },
    { "valid_number",            validNumberMS },
    { "replace_in_string",       replaceInStringMS },
    { "select",                  selectMS },
    { "select_rectangle",        selectRectangleMS },
    { "focus_window",            focusWindowMS },
    { "shell_command",           shellCmdMS },
    { "string_to_clipboard",     stringToClipboardMS },
    { "clipboard_to_string",     clipboardToStringMS },
    { "toupper",                 toupperMS },
    { "tolower",                 tolowerMS },
    { "list_dialog",             listDialogMS },
    { "getenv",                  getenvMS },
    { "string_compare",          stringCompareMS },
    { "split",                   splitMS },
    { "calltip",                 calltipMS },
    { "kill_calltip",            killCalltipMS },
#if 0 // DISABLED for 5.4
    { "set_backlight_string",    setBacklightStringMS },
#endif
    { "rangeset_create",         rangesetCreateMS },
    { "rangeset_destroy",        rangesetDestroyMS },
    { "rangeset_add",            rangesetAddMS },
    { "rangeset_subtract",       rangesetSubtractMS },
    { "rangeset_invert",         rangesetInvertMS },
    { "rangeset_info",           rangesetInfoMS },
    { "rangeset_range",          rangesetRangeMS },
    { "rangeset_includes",       rangesetIncludesPosMS },
    { "rangeset_set_color",      rangesetSetColorMS },
    { "rangeset_set_name",       rangesetSetNameMS },
    { "rangeset_set_mode",       rangesetSetModeMS },
    { "rangeset_get_by_name",    rangesetGetByNameMS },
    { "get_pattern_by_name",     getPatternByNameMS },
    { "get_pattern_at_pos",      getPatternAtPosMS },
    { "get_style_by_name",       getStyleByNameMS },
    { "get_style_at_pos",        getStyleAtPosMS },
    { "filename_dialog",         filenameDialogMS },
};

static SubRoutine SpecialVars[] = {
    { "$cursor",                  cursorMV },
    { "$line",                    lineMV },
    { "$column",                  columnMV },
    { "$file_name",               fileNameMV },
    { "$file_path",               filePathMV },
    { "$text_length",             lengthMV },
    { "$selection_start",         selectionStartMV },
    { "$selection_end",           selectionEndMV },
    { "$selection_left",          selectionLeftMV },
    { "$selection_right",         selectionRightMV },
    { "$wrap_margin",             wrapMarginMV },
    { "$tab_dist",                tabDistMV },
    { "$em_tab_dist",             emTabDistMV },
    { "$use_tabs",                useTabsMV },
    { "$language_mode",           languageModeMV },
    { "$modified",                modifiedMV },
    { "$statistics_line",         statisticsLineMV },
    { "$incremental_search_line", incSearchLineMV },
    { "$show_line_numbers",       showLineNumbersMV },
    { "$auto_indent",             autoIndentMV },
    { "$wrap_text",               wrapTextMV },
    { "$highlight_syntax",        highlightSyntaxMV },
    { "$make_backup_copy",        makeBackupCopyMV },
    { "$incremental_backup",      incBackupMV },
    { "$show_matching",           showMatchingMV },
    { "$match_syntax_based",      matchSyntaxBasedMV },
    { "$overtype_mode",           overTypeModeMV },
    { "$read_only",               readOnlyMV },
    { "$locked",                  lockedMV },
    { "$file_format",             fileFormatMV },
    { "$font_name",               fontNameMV },
    { "$font_name_italic",        fontNameItalicMV },
    { "$font_name_bold",          fontNameBoldMV },
    { "$font_name_bold_italic",   fontNameBoldItalicMV },
    { "$sub_sep",                 subscriptSepMV },
    { "$min_font_width",          minFontWidthMV },
    { "$max_font_width",          maxFontWidthMV },
    { "$top_line",                topLineMV },
    { "$n_display_lines",         numDisplayLinesMV },
    { "$display_width",           displayWidthMV },
    { "$active_pane",             activePaneMV },
    { "$n_panes",                 nPanesMV },
    { "$empty_array",             emptyArrayMV },
    { "$server_name",             serverNameMV },
    { "$calltip_ID",              calltipIDMV },
#if 0 // // DISABLED for 5.4
    { "$backlight_string",        backlightStringMV},
#endif
    { "$rangeset_list",           rangesetListMV },
    { "$VERSION",                 versionMV }
};



// Global symbols for returning values from built-in functions
#define N_RETURN_GLOBALS 5
enum retGlobalSyms {
    STRING_DIALOG_BUTTON,
    SEARCH_END,
    READ_STATUS,
    SHELL_CMD_STATUS,
    LIST_DIALOG_BUTTON
};

static const char *ReturnGlobalNames[N_RETURN_GLOBALS] = {
    "$string_dialog_button",
    "$search_end",
    "$read_status",
    "$shell_cmd_status",
    "$list_dialog_button"
};
static Symbol *ReturnGlobals[N_RETURN_GLOBALS];

/*
** Install built-in macro subroutines and special variables for accessing
** editor information
*/
void RegisterMacroSubroutines() {
    static DataValue subrPtr = INIT_DATA_VALUE;
    static DataValue noValue = INIT_DATA_VALUE;

    /* Install symbols for built-in routines and variables, with pointers
       to the appropriate c routines to do the work */
    for(const SubRoutine &routine : MacroSubrs) {
        subrPtr.val.subr = routine.function;
        InstallSymbol(routine.name, C_FUNCTION_SYM, subrPtr);
    }

    for(const SubRoutine &routine : SpecialVars) {
        subrPtr.val.subr = routine.function;
        InstallSymbol(routine.name, PROC_VALUE_SYM, subrPtr);
    }

    // NOTE(eteran): things that were in the menu action list
    for(const SubRoutine &routine : MenuMacroSubrNames) {
        subrPtr.val.subr = routine.function;
        InstallSymbol(routine.name, C_FUNCTION_SYM, subrPtr);
    }

    // NOTE(eteran): things that were in the text widget action list
    for(const SubRoutine &routine : TextAreaSubrNames) {
        subrPtr.val.subr = routine.function;
        InstallSymbol(routine.name, C_FUNCTION_SYM, subrPtr);
    }

    /* Define global variables used for return values, remember their
       locations so they can be set without a LookupSymbol call */
    for (unsigned int i = 0; i < N_RETURN_GLOBALS; i++)
        ReturnGlobals[i] = InstallSymbol(ReturnGlobalNames[i], GLOBAL_SYM, noValue);
}

void FinishLearnEx() {

    // If we're not in learn mode, return
    if(!CommandRecorder::getInstance()->isRecording()) {
        return;
    }

    DocumentWidget *document = CommandRecorder::getInstance()->macroRecordWindowEx;
    Q_ASSERT(document);

    CommandRecorder::getInstance()->stopRecording();

    // Undim the menu items dimmed during learn
    for(MainWindow *window : MainWindow::allWindows()) {
        window->ui.action_Learn_Keystrokes->setEnabled(true);
    }

    if (document->IsTopDocument()) {
        if(MainWindow *window = document->toWindow()) {
            window->ui.action_Finish_Learn->setEnabled(false);
            window->ui.action_Cancel_Learn->setEnabled(false);
        }
    }

    // Undim the replay and paste-macro buttons
    for(MainWindow *window : MainWindow::allWindows()) {
        window->ui.action_Replay_Keystrokes->setEnabled(true);
    }

    MainWindow::DimPasteReplayBtns(true);

    // Clear learn-mode banner
    document->ClearModeMessageEx();
}

/*
** Cancel Learn mode, or macro execution (they're bound to the same menu item)
*/
void CancelMacroOrLearnEx(DocumentWidget *document) {
    if(CommandRecorder::getInstance()->isRecording()) {
        cancelLearnEx();
    } else if (document->macroCmdData_) {
        AbortMacroCommandEx(document);
    }
}

static void cancelLearnEx() {
    // If we're not in learn mode, return
    if(!CommandRecorder::getInstance()->isRecording()) {
        return;
    }

    DocumentWidget *document = CommandRecorder::getInstance()->macroRecordWindowEx;

    // Undim the menu items dimmed during learn
    for(MainWindow *window : MainWindow::allWindows()) {
        window->ui.action_Learn_Keystrokes->setEnabled(true);
    }

    if (document->IsTopDocument()) {
        MainWindow *win = document->toWindow();
        win->ui.action_Finish_Learn->setEnabled(false);
        win->ui.action_Cancel_Learn->setEnabled(false);
    }

    // Clear learn-mode banner
    document->ClearModeMessageEx();
}

/*
** Execute the learn/replay sequence stored in "window"
*/
void ReplayEx(DocumentWidget *window) {

    QString replayMacro = CommandRecorder::getInstance()->replayMacro;

    // Verify that a replay macro exists and it's not empty and that
    // we're not already running a macro
    if (!replayMacro.isEmpty() && window->macroCmdData_ == nullptr) {

        /* Parse the replay macro (it's stored in text form) and compile it into
           an executable program "prog" */
        QString errMsg;
        int stoppedAt;

        Program *prog = ParseMacroEx(replayMacro, &errMsg, &stoppedAt);
        if(!prog) {
            qWarning("NEdit: internal error, learn/replay macro syntax error: %s", qPrintable(errMsg));
            return;
        }

        // run the executable program
        runMacroEx(window, prog);
    }
}

/*
**  Read the initial NEdit macro file if one exists.
*/
void ReadMacroInitFileEx(DocumentWidget *window) {

    const QString autoloadName = Settings::autoLoadMacroFile();
    if(autoloadName.isNull()) {
        return;
    }

    static bool initFileLoaded = false;

    if (!initFileLoaded) {
        window->ReadMacroFileEx(autoloadName, false);
        initFileLoaded = true;
    }
}

/*
** Parse and execute a macro string including macro definitions.  Report
** parsing errors in a dialog posted over window->shell_.
*/
int ReadMacroStringEx(DocumentWidget *window, const QString &string, const QString &errIn) {
    return readCheckMacroStringEx(window, string, window, errIn, nullptr);
}

/*
** Check a macro string containing definitions for errors.  Returns True
** if macro compiled successfully.  Returns False and puts up
** a dialog explaining if macro did not compile successfully.
*/
bool CheckMacroStringEx(QWidget *dialogParent, const QString &string, const QString &errIn, int *errPos) {
    Q_ASSERT(errPos);
    return readCheckMacroStringEx(dialogParent, string, nullptr, errIn, errPos);
}

/*
** Parse and optionally execute a macro string including macro definitions.
** Report parsing errors in a dialog posted over dialogParent, using the
** string errIn to identify the entity being parsed (filename, macro string,
** etc.).  If runWindow is specified, runs the macro against the window.  If
** runWindow is passed as nullptr, does parse only.  If errPos is non-null,
** returns a pointer to the error location in the string.
*/
Program *ParseMacroEx(const QString &expr, QString *message, int *stoppedAt) {
    QByteArray str = expr.toLatin1();
    const char *ptr = str.data();

    const char *msg = nullptr;
    const char *e = nullptr;
    Program *p = ParseMacro(ptr, &msg, &e);

    *message = QString::fromLatin1(msg);
    *stoppedAt = (e - ptr);
    return p;
}

int readCheckMacroStringEx(QWidget *dialogParent, const QString &string, DocumentWidget *runWindow, const QString &errIn, int *errPos) {

    Input in(&string);

    DataValue subrPtr;
    QStack<Program *> progStack;

    while (!in.atEnd()) {

        // skip over white space and comments
        while (*in == QLatin1Char(' ') || *in == QLatin1Char('\t') || *in == QLatin1Char('\n') || *in == QLatin1Char('#')) {
            if (*in == QLatin1Char('#')) {
                while (!in.atEnd() && *in != QLatin1Char('\n')) {
                    ++in;
                }
            } else {
                ++in;
            }
        }

        if (in.atEnd()) {
            break;
        }

        // look for define keyword, and compile and store defined routines
        if (in.match(QLatin1String("define"))  && (in[6] == QLatin1Char(' ') || in[6] == QLatin1Char('\t'))) {
            in += 6;
            in.skipWhitespace();

            QString subrName;
            auto namePtr = std::back_inserter(subrName);

            while ((*in).isLetterOrNumber() || *in == QLatin1Char('_')) {
                *namePtr++ = *in++;
            }

            if ((*in).isLetterOrNumber() || *in == QLatin1Char('_')) {
                return ParseErrorEx(
                            dialogParent,
                            *in.string(),
                            in.index(),
                            errIn,
                            QLatin1String("subroutine name too long"));
            }

            in.skipWhitespaceNL();

            if (*in != QLatin1Char('{')) {
                if(errPos) {
                    *errPos = in.index();
                }
                return ParseErrorEx(
                            dialogParent,
                            *in.string(),
                            in.index(),
                            errIn,
                            QLatin1String("expected '{'"));
            }

            QString code = in.mid();
            int stoppedAt;
            QString errMsg;
            Program *const prog = ParseMacroEx(code, &errMsg, &stoppedAt);
            if(!prog) {
                if(errPos) {
                    *errPos = in.index() + stoppedAt;
                }

                return ParseErrorEx(
                            dialogParent,
                            code,
                            stoppedAt,
                            errIn,
                            errMsg);
            }
            if (runWindow) {
                Symbol *sym = LookupSymbol(subrName.toStdString());
                if(!sym) {
                    subrPtr.val.prog = prog;
                    subrPtr.tag = NO_TAG;
                    sym = InstallSymbol(subrName.toStdString(), MACRO_FUNCTION_SYM, subrPtr);
                } else {
                    if (sym->type == MACRO_FUNCTION_SYM) {
                        FreeProgram(sym->value.val.prog);
                    } else {
                        sym->type = MACRO_FUNCTION_SYM;
                    }

                    sym->value.val.prog = prog;
                }
            }

            in += stoppedAt;

            /* Parse and execute immediate (outside of any define) macro commands
               and WAIT for them to finish executing before proceeding.  Note that
               the code below is not perfect.  If you interleave code blocks with
               definitions in a file which is loaded from another macro file, it
               will probably run the code blocks in reverse order! */
        } else {
            QString code = in.mid();
            int stoppedAt;
            QString errMsg;
            Program *const prog = ParseMacroEx(code, &errMsg, &stoppedAt);
            if(!prog) {
                if (errPos) {
                    *errPos = in.index() + stoppedAt;
                }

                return ParseErrorEx(
                            dialogParent,
                            code,
                            stoppedAt,
                            errIn,
                            errMsg);
            }

            if (runWindow) {

                if (!runWindow->macroCmdData_) {
                    runMacroEx(runWindow, prog);

#if 0
                    // TODO(eteran): is this the same as like QApplication::processEvents() ?
                    XEvent nextEvent;
                    while (runWindow->macroCmdData_) {
                        XtAppNextEvent(XtWidgetToApplicationContext(runWindow->shell_), &nextEvent);
                        ServerDispatchEvent(&nextEvent);
                    }
#endif
                } else {
                    /*  If we come here this means that the string was parsed
                        from within another macro via load_macro_file(). In
                        this case, plain code segments outside of define
                        blocks are rolled into one Program each and put on
                        the stack. At the end, the stack is unrolled, so the
                        plain Programs would be executed in the wrong order.

                        So we don't hand the Programs over to the interpreter
                        just yet (via RunMacroAsSubrCall()), but put it on a
                        stack of our own, reversing order once again.   */
                    progStack.push(prog);
                }
            }
            in += stoppedAt;
        }
    }

    //  Unroll reversal stack for macros loaded from macros.
    while (!progStack.empty()) {

        Program *const prog = progStack.top();
        progStack.pop();

        RunMacroAsSubrCall(prog);
    }

    return true;
}

/*
** Run a pre-compiled macro, changing the interface state to reflect that
** a macro is running, and handling preemption, resumption, and cancellation.
** frees prog when macro execution is complete;
*/
static void runMacroEx(DocumentWidget *document, Program *prog) {
    DataValue result;
    const char *errMsg;
    int stat;

    /* If a macro is already running, just call the program as a subroutine,
       instead of starting a new one, so we don't have to keep a separate
       context, and the macros will serialize themselves automatically */
    if (document->macroCmdData_) {
        RunMacroAsSubrCall(prog);
        return;
    }

    // put up a watch cursor over the waiting window
    document->setCursor(Qt::BusyCursor);

    // enable the cancel menu item
    if(MainWindow *win = document->toWindow()) {
        win->ui.action_Cancel_Learn->setText(QLatin1String("Cancel Macro"));
        win->ui.action_Cancel_Learn->setEnabled(true);
    }

    /* Create a data structure for passing macro execution information around
       amongst the callback routines which will process i/o and completion */
    auto cmdData = new macroCmdInfoEx;
    document->macroCmdData_       = cmdData;
    cmdData->bannerIsUp         = false;
    cmdData->closeOnCompletion  = false;
    cmdData->program            = prog;
    cmdData->context            = nullptr;

    // Set up timer proc for putting up banner when macro takes too long
    cmdData->bannerTimeoutID = new QTimer(document);
    QObject::connect(cmdData->bannerTimeoutID, SIGNAL(timeout()), document, SLOT(bannerTimeoutProc()));
    cmdData->bannerTimeoutID->setSingleShot(true);
    cmdData->bannerTimeoutID->start(BANNER_WAIT_TIME);

    // Begin macro execution
    stat = ExecuteMacroEx(document, prog, 0, nullptr, &result, &cmdData->context, &errMsg);

    if (stat == MACRO_ERROR) {
        finishMacroCmdExecutionEx(document);
        QMessageBox::critical(document, QLatin1String("Macro Error"), QString(QLatin1String("Error executing macro: %1")).arg(QString::fromLatin1(errMsg)));
        return;
    }

    if (stat == MACRO_DONE) {
        finishMacroCmdExecutionEx(document);
        return;
    }
    if (stat == MACRO_TIME_LIMIT) {
        ResumeMacroExecutionEx(document);
        return;
    }
    // (stat == MACRO_PREEMPT) Macro was preempted
}

/*
** Continue with macro execution after preemption.  Called by the routines
** whose actions cause preemption when they have completed their lengthy tasks.
** Re-establishes macro execution work proc.  Window must be the window in
** which the macro is executing (the window to which macroCmdData is attached),
** and not the window to which operations are focused.
*/
void ResumeMacroExecutionEx(DocumentWidget *window) {
    auto cmdData = static_cast<macroCmdInfoEx *>(window->macroCmdData_);

    if(cmdData) {
        cmdData->continueWorkProcID = QtConcurrent::run([window]() {
            return continueWorkProcEx(window);
        });
    }
}

/*
** Cancel the macro command in progress (user cancellation via GUI)
*/
void AbortMacroCommandEx(DocumentWidget *document) {
    if (!document->macroCmdData_)
        return;

    /* If there's both a macro and a shell command executing, the shell command
       must have been called from the macro.  When called from a macro, shell
       commands don't put up cancellation controls of their own, but rely
       instead on the macro cancellation mechanism (here) */
    if (document->shellCmdData_) {
        document->AbortShellCommandEx();
    }

    // Free the continuation
    FreeRestartDataEx((static_cast<macroCmdInfoEx *>(document->macroCmdData_))->context);

    // Kill the macro command
    finishMacroCmdExecutionEx(document);
}

/*
** Call this before closing a window, to clean up macro references to the
** window, stop any macro which might be running from it, free associated
** memory, and check that a macro is not attempting to close the window from
** which it is run.  If this is being called from a macro, and the window
** this routine is examining is the window from which the macro was run, this
** routine will return False, and the caller must NOT CLOSE THE WINDOW.
** Instead, empty it and make it Untitled, and let the macro completion
** process close the window when the macro is finished executing.
*/
int MacroWindowCloseActionsEx(DocumentWidget *document) {
    auto cmdData = static_cast<macroCmdInfoEx *>(document->macroCmdData_);

    auto recorder = CommandRecorder::getInstance();

    if (recorder->isRecording() && recorder->macroRecordWindowEx == document) {
        FinishLearnEx();
    }

    /* If no macro is executing in the window, allow the close, but check
       if macros executing in other windows have it as focus.  If so, set
       their focus back to the window from which they were originally run */
    if(!cmdData) {
        for(DocumentWidget *w : DocumentWidget::allDocuments()) {
            auto mcd = static_cast<macroCmdInfoEx *>(w->macroCmdData_);
            if (w == MacroRunWindowEx() && MacroFocusWindowEx() == document) {
                SetMacroFocusWindowEx(MacroRunWindowEx());
            } else if (mcd != nullptr && mcd->context->focusWindow == document) {
                mcd->context->focusWindow = mcd->context->runWindow;
            }
        }

        return true;
    }

    /* If the macro currently running (and therefore calling us, because
       execution must otherwise return to the main loop to execute any
       commands), is running in this window, tell the caller not to close,
       and schedule window close on completion of macro */
    if (document == MacroRunWindowEx()) {
        cmdData->closeOnCompletion = true;
        return false;
    }

    // Free the continuation
    FreeRestartDataEx(cmdData->context);

    // Kill the macro command
    finishMacroCmdExecutionEx(document);
    return true;
}

/*
** Clean up after the execution of a macro command: free memory, and restore
** the user interface state.
*/
static void finishMacroCmdExecutionEx(DocumentWidget *document) {
    auto cmdData = static_cast<macroCmdInfoEx *>(document->macroCmdData_);
    bool closeOnCompletion = cmdData->closeOnCompletion;

    // Cancel pending timeout and work proc
    cmdData->bannerTimeoutID->stop();
    cmdData->continueWorkProcID.cancel();

    // Clean up waiting-for-macro-command-to-complete mode
    document->setCursor(Qt::ArrowCursor);

    // enable the cancel menu item
    if(MainWindow *win = document->toWindow()) {
        win->ui.action_Cancel_Learn->setText(QLatin1String("Cancel Learn"));
        win->ui.action_Cancel_Learn->setEnabled(false);
    }

    if (cmdData->bannerIsUp) {
        document->ClearModeMessageEx();
    }

    // Free execution information
    FreeProgram(cmdData->program);
    delete cmdData;
    document->macroCmdData_ = nullptr;

    /* If macro closed its own window, window was made empty and untitled,
       but close was deferred until completion.  This is completion, so if
       the window is still empty, do the close */
    if (closeOnCompletion && !document->filenameSet_ && !document->fileChanged_) {
        document->CloseWindow();
    }

    // If no other macros are executing, do garbage collection
    SafeGC();

    /* In processing the .neditmacro file (and possibly elsewhere), there
       is an event loop which waits for macro completion.  Send an event
       to wake up that loop, otherwise execution will stall until the user
       does something to the window. */
    if (!closeOnCompletion) {
#if 0
        XClientMessageEvent event;
        // TODO(eteran): find the equivalent to this...
        event.format = 8;
        event.type = ClientMessage;
        XSendEvent(XtDisplay(window->shell_), XtWindow(window->shell_), False, NoEventMask, (XEvent *)&event);
#endif
    }
}

/*
** Do garbage collection of strings if there are no macros currently
** executing.  NEdit's macro language GC strategy is to call this routine
** whenever a macro completes.  If other macros are still running (preempted
** or waiting for a shell command or dialog), this does nothing and therefore
** defers GC to the completion of the last macro out.
*/
void SafeGC() {

    for(DocumentWidget *document : DocumentWidget::allDocuments()) {
        if (document->macroCmdData_ != nullptr || InSmartIndentMacrosEx(document)) {
            return;
        }
    }

    GarbageCollectStrings();
}

/*
** Executes macro string "macro" using the lastFocus pane in "window".
** Reports errors via a dialog posted over "window", integrating the name
** "errInName" into the message to help identify the source of the error.
*/
void DoMacroEx(DocumentWidget *document, const QString &macro, const char *errInName) {

    /* Add a terminating newline (which command line users are likely to omit
       since they are typically invoking a single routine) */
    QString qMacro = macro + QLatin1Char('\n');
    QString errMsg;

    // Parse the macro and report errors if it fails
    int stoppedAt;
    Program *const prog = ParseMacroEx(qMacro, &errMsg, &stoppedAt);
    if(!prog) {
        ParseErrorEx(document, qMacro, stoppedAt, QString::fromLatin1(errInName), errMsg);
        return;
    }

    // run the executable program (prog is freed upon completion)
    runMacroEx(document, prog);
}

/*
** Dispatches a macro to which repeats macro command in "command", either
** an integer number of times ("how" == positive integer), or within a
** selected range ("how" == REPEAT_IN_SEL), or to the end of the window
** ("how == REPEAT_TO_END).
**
** Note that as with most macro routines, this returns BEFORE the macro is
** finished executing
*/
void RepeatMacroEx(DocumentWidget *document, const char *command, int how) {

    const char *errMsg;
    const char *stoppedAt;
    const char *loopMacro;

    if(!command) {
        return;
    }

    // Wrap a for loop and counter/tests around the command
    switch(how) {
    case REPEAT_TO_END:
        loopMacro = "lastCursor=-1\nstartPos=$cursor\n"
                    "while($cursor>=startPos&&$cursor!=lastCursor){\nlastCursor=$cursor\n%s\n}\n";
        break;
    case REPEAT_IN_SEL:
        loopMacro = "selStart = $selection_start\nif (selStart == -1)\nreturn\n"
                    "selEnd = $selection_end\nset_cursor_pos(selStart)\nselect(0,0)\n"
                    "boundText = get_range(selEnd, selEnd+10)\n"
                    "while($cursor >= selStart && $cursor < selEnd && \\\n"
                    "get_range(selEnd, selEnd+10) == boundText) {\n"
                    "startLength = $text_length\n%s\n"
                    "selEnd += $text_length - startLength\n}\n";
        break;
    default:
        loopMacro = "for(i=0;i<%d;i++){\n%s\n}\n";
        break;
    }


    auto loopedCmd = std::make_unique<char[]>(strlen(command) + strlen(loopMacro) + 25);

    if (how == REPEAT_TO_END || how == REPEAT_IN_SEL) {
        sprintf(&loopedCmd[0], loopMacro, command);
    } else {
        sprintf(&loopedCmd[0], loopMacro, how, command);
    }

    // Parse the resulting macro into an executable program "prog"
    Program *const prog = ParseMacro(&loopedCmd[0], &errMsg, &stoppedAt);
    if(!prog) {
        qWarning("NEdit: internal error, repeat macro syntax wrong: %s", errMsg);
        return;
    }

    // run the executable program
    runMacroEx(document, prog);
}


/*
** Macro recording action hook for Learn/Replay, added temporarily during
** learn.
*/
#if 0
void learnActionHook(Widget w, XtPointer clientData, String actionName, XEvent *event, String *params, Cardinal *numParams) {

    int i;
    char *actionString;

    /* Select only actions in text panes in the curr for which this
       action hook is recording macros (from clientData). */
    auto curr = WindowList.begin();
    for (; curr != WindowList.end(); ++curr) {

        Document *const window = *curr;

        if (window->textArea_ == w)
            break;
        for (i = 0; i < window->textPanes_.size(); i++) {
            if (window->textPanes_[i] == w)
                break;
        }
        if (i < window->textPanes_.size())
            break;
    }

    if (curr == WindowList.end() || *curr != static_cast<Document *>(clientData))
        return;

    /* beep on un-recordable operations which require a mouse position, to
       remind the user that the action was not recorded */
    if (isMouseAction(actionName)) {
        QApplication::beep();
        return;
    }

    // Record the action and its parameters
    actionString = actionToString(w, actionName, event, params, *numParams);
    if (actionString) {
        MacroRecordBuf->BufAppendEx(actionString);
        XtFree(actionString);
    }
}
#endif

/*
** Work proc for continuing execution of a preempted macro.
**
** Xt WorkProcs are designed to run first-in first-out, which makes them
** very bad at sharing time between competing tasks.  For this reason, it's
** usually bad to use work procs anywhere where their execution is likely to
** overlap.  Using a work proc instead of a timer proc (which I usually
** prefer) here means macros will probably share time badly, but we're more
** interested in making the macros cancelable, and in continuing other work
** than having users run a bunch of them at once together.
*/
// NOTE(eteran): we are using a QFuture to simulate this, but I'm not 100% sure
//               that it is a perfect match. We can also try something like a
//               sinle shot QTimer with a timeout of zero.
bool continueWorkProcEx(DocumentWidget *window) {

    auto cmdData = static_cast<macroCmdInfoEx *>(window->macroCmdData_);

    const char *errMsg;
    DataValue result;
    const int stat = ContinueMacroEx(cmdData->context, &result, &errMsg);

    if (stat == MACRO_ERROR) {
        finishMacroCmdExecutionEx(window);
        QMessageBox::critical(window, QLatin1String("Macro Error"), QString(QLatin1String("Error executing macro: %1")).arg(QString::fromLatin1(errMsg)));
        return true;
    } else if (stat == MACRO_DONE) {
        finishMacroCmdExecutionEx(window);
        return true;
    } else if (stat == MACRO_PREEMPT) {
        cmdData->continueWorkProcID = QFuture<bool>();
        return true;
    }

    // Macro exceeded time slice, re-schedule it
    if (stat != MACRO_TIME_LIMIT) {
        return true; // shouldn't happen
    }

    return false;
}


/*
** Built-in macro subroutine for getting the length of a string
*/
static int lengthMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(document);

    std::string string;

    if(!readArguments(argList, nArgs, 0, errMsg, &string)) {
        return false;
    }

    result->tag   = INT_TAG;
    result->val.n = static_cast<int>(string.size());
    return true;
}

/*
** Built-in macro subroutines for min and max
*/
static int minMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(document);

    int minVal;
    int value;

    if (nArgs == 1) {
        return tooFewArgsErr(errMsg);
    }

    if (!readArgument(argList[0], &minVal, errMsg)) {
        return false;
    }

    for (int i = 0; i < nArgs; i++) {
        if (!readArgument(argList[i], &value, errMsg)) {
            return false;
        }

        minVal = std::min(minVal, value);
    }

    result->tag   = INT_TAG;
    result->val.n = minVal;
    return true;
}

static int maxMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    Q_UNUSED(document);

    int maxVal;
    int value;

    if (nArgs == 1) {
        return tooFewArgsErr(errMsg);
    }

    if (!readArgument(argList[0], &maxVal, errMsg)) {
        return false;
    }

    for (int i = 0; i < nArgs; i++) {
        if (!readArgument(argList[i], &value, errMsg)) {
            return false;
        }

        maxVal = std::max(maxVal, value);
    }

    result->tag   = INT_TAG;
    result->val.n = maxVal;
    return true;
}

static int focusWindowMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    QString string;

    /* Read the argument representing the window to focus to, and translate
       it into a pointer to a real DocumentWidget */
    if (nArgs != 1) {
        return wrongNArgsErr(errMsg);
    }

    QList<DocumentWidget *> documents = DocumentWidget::allDocuments();
    QList<DocumentWidget *>::iterator w;

    if (!readArgument(argList[0], &string, errMsg)) {
        return false;
    } else if (string == QLatin1String("last")) {
        w = documents.begin();
    } else if (string == QLatin1String("next")) {

        auto curr = std::find_if(documents.begin(), documents.end(), [window](DocumentWidget *doc) {
            return doc == window;
        });

        if(curr != documents.end()) {
            w = std::next(curr);
        }
    } else if (string.size() >= MAXPATHLEN) {
        *errMsg = "Pathname too long in focus_window()";
        return false;
    } else {
        // just use the plain name as supplied
        w = std::find_if(documents.begin(), documents.end(), [&string](DocumentWidget *doc) {
            QString fullname = doc->FullPath();
            return fullname == string;
        });

        // didn't work? try normalizing the string passed in
        if(w == documents.end()) {

            QString normalizedString = NormalizePathnameEx(string);
            if(normalizedString.isNull()) {
                //  Something is broken with the input pathname.
                *errMsg = "Pathname too long in focus_window()";
                return false;
            }

            w = std::find_if(documents.begin(), documents.end(), [&normalizedString](DocumentWidget *win) {
                QString fullname = win->FullPath();
                return fullname == normalizedString;
            });
        }
    }

    // If no matching window was found, return empty string and do nothing
    if(w == documents.end()) {
        result->tag         = STRING_TAG;
        result->val.str.rep = PERM_ALLOC_STR("");
        result->val.str.len = 0;
        return true;
    }

    DocumentWidget *const document = *w;

    // Change the focused window to the requested one
    SetMacroFocusWindowEx(document);

    // turn on syntax highlight that might have been deferred
    if ((document)->highlightSyntax_ && !(document)->highlightData_) {
        StartHighlightingEx(document, false);
    }

    // Return the name of the window
    result->tag     = STRING_TAG;
    result->val.str = AllocNStringCpyEx(QString(QLatin1String("%1%2")).arg(document->path_, document->filename_));
    return true;
}

/*
** Built-in macro subroutine for getting text from the current window's text
** buffer
*/
static int getRangeMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    int from;
    int to;
    TextBuffer *buf = window->buffer_;

    // Validate arguments and convert to int
    if(!readArguments(argList, nArgs, 0, errMsg, &from, &to)) {
        return false;
    }

    from = qBound(0, from, buf->BufGetLength());
    to   = qBound(0, to,   buf->BufGetLength());

    if (from > to) {
        qSwap(from, to);
    }

    /* Copy text from buffer (this extra copy could be avoided if TextBuffer.c
       provided a routine for writing into a pre-allocated string) */
    result->tag = STRING_TAG;

    std::string rangeText = buf->BufGetRangeEx(from, to);
    buf->BufUnsubstituteNullCharsEx(rangeText);

    result->val.str = AllocNStringCpyEx(rangeText);

    /* Note: after the un-substitution, it is possible that strlen() != len,
       but that's because strlen() can't deal with 0-characters. */

    return true;
}

/*
** Built-in macro subroutine for getting a single character at the position
** given, from the current window
*/
static int getCharacterMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    int pos;
    TextBuffer *buf = window->buffer_;

    // Validate arguments and convert to int
    if(!readArguments(argList, nArgs, 0, errMsg, &pos)) {
        return false;
    }

    pos = qBound(0, pos, buf->BufGetLength());

    // Return the character in a pre-allocated string)
    result->tag = STRING_TAG;
    AllocNString(&result->val.str, 2);
    result->val.str.rep[0] = buf->BufGetCharacter(pos);

    buf->BufUnsubstituteNullChars(result->val.str.rep, result->val.str.len);
    /* Note: after the un-substitution, it is possible that strlen() != len,
       but that's because strlen() can't deal with 0-characters. */

    return true;
}

/*
** Built-in macro subroutine for replacing text in the current window's text
** buffer
*/
static int replaceRangeMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    int from;
    int to;
    TextBuffer *buf = window->buffer_;
    std::string string;

    // Validate arguments and convert to int
    if(!readArguments(argList, nArgs, 0, errMsg, &from, &to, &string)) {
        return false;
    }

    from = qBound(0, from, buf->BufGetLength());
    to   = qBound(0, to,   buf->BufGetLength());

    if (from > to) {
        qSwap(from, to);
    }

    // Don't allow modifications if the window is read-only
    if (window->lockReasons_.isAnyLocked()) {
        QApplication::beep();
        result->tag = NO_TAG;
        return true;
    }

    /* There are no null characters in the string (because macro strings
       still have null termination), but if the string contains the
       character used by the buffer for null substitution, it could
       theoretically become a null.  In the highly unlikely event that
       all of the possible substitution characters in the buffer are used
       up, stop the macro and tell the user of the failure */
    if (!window->buffer_->BufSubstituteNullCharsEx(string)) {
        *errMsg = "Too much binary data in file";
        return false;
    }

    // Do the replace
    buf->BufReplaceEx(from, to, string);
    result->tag = NO_TAG;
    return true;
}

/*
** Built-in macro subroutine for replacing the primary-selection selected
** text in the current window's text buffer
*/
static int replaceSelectionMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    std::string string;

    // Validate argument and convert to string
    if(!readArguments(argList, nArgs, 0, errMsg, &string)) {
        return false;
    }

    // Don't allow modifications if the window is read-only
    if (window->lockReasons_.isAnyLocked()) {
        QApplication::beep();
        result->tag = NO_TAG;
        return true;
    }

    /* There are no null characters in the string (because macro strings
       still have null termination), but if the string contains the
       character used by the buffer for null substitution, it could
       theoretically become a null.  In the highly unlikely event that
       all of the possible substitution characters in the buffer are used
       up, stop the macro and tell the user of the failure */
    if (!window->buffer_->BufSubstituteNullCharsEx(string)) {
        *errMsg = "Too much binary data in file";
        return false;
    }

    // Do the replace
    window->buffer_->BufReplaceSelectedEx(string);
    result->tag = NO_TAG;
    return true;
}

/*
** Built-in macro subroutine for getting the text currently selected by
** the primary selection in the current window's text buffer, or in any
** part of screen if "any" argument is given
*/
static int getSelectionMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    /* Read argument list to check for "any" keyword, and get the appropriate
       selection */
    if (nArgs != 0 && nArgs != 1) {
        return wrongNArgsErr(errMsg);
    }

    if (nArgs == 1) {
        if (argList[0].tag != STRING_TAG || strcmp(argList[0].val.str.rep, "any")) {
            *errMsg = "Unrecognized argument to %s";
            return false;
        }

        QString text = GetAnySelectionEx(window);
        if (text.isNull()) {
            text = QLatin1String("");
        }

        // Return the text as an allocated string
        result->tag = STRING_TAG;
        result->val.str = AllocNStringCpyEx(text);
    } else {
        std::string selText = window->buffer_->BufGetSelectionTextEx();
        window->buffer_->BufUnsubstituteNullCharsEx(selText);

        // Return the text as an allocated string
        result->tag = STRING_TAG;
        result->val.str = AllocNStringCpyEx(selText);
    }

    return true;
}

/*
** Built-in macro subroutine for determining if implicit conversion of
** a string to number will succeed or fail
*/
static int validNumberMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    Q_UNUSED(window);

    std::string string;

    if(!readArguments(argList, nArgs, 0, errMsg, &string)) {
        return false;
    }

    result->tag = INT_TAG;
    result->val.n = StringToNum(string, nullptr);

    return true;
}

/*
** Built-in macro subroutine for replacing a substring within another string
*/
static int replaceSubstringMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(window);

    int from;
    int to;
    std::string string;
    std::string replStr;

    // Validate arguments and convert to int
    if(!readArguments(argList, nArgs, 0, errMsg, &string, &from, &to, &replStr)) {
        return false;
    }

    const int length = string.size();

    from = qBound(0, from, length);
    to   = qBound(0, to,   length);

    if (from > to) {
        qSwap(from, to);
    }

    // Allocate a new string and do the replacement
    result->tag = STRING_TAG;

    string.replace(from, to - from, replStr);
    result->val.str = AllocNStringCpyEx(string);

    return true;
}

/*
** Built-in macro subroutine for getting a substring of a string.
** Called as substring(string, from [, to])
*/
static int substringMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(window);

    // Validate arguments and convert to int
    if (nArgs != 2 && nArgs != 3) {
        return wrongNArgsErr(errMsg);
    }

    int from;
    QString string;

    if(!readArguments(argList, nArgs, 0, errMsg, &string, &from)) {
        return false;
    }

    int length = string.size();
    int to     = string.size();

    if (nArgs == 3) {
        if (!readArgument(argList[2], &to, errMsg)) {
            return false;
        }
    }

    if (from < 0)      from += length;
    if (from < 0)      from = 0;
    if (from > length) from = length;
    if (to < 0)        to += length;
    if (to < 0)        to = 0;
    if (to > length)   to = length;
    if (from > to)     to = from;

    // Allocate a new string and copy the sub-string into it
    result->tag     = STRING_TAG;
    result->val.str = AllocNStringCpyEx(string.mid(from, to - from));
    return true;
}

static int toupperMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(window);
    std::string string;

    // Validate arguments and convert to int
    if(!readArguments(argList, nArgs, 0, errMsg, &string)) {
        return false;
    }

    // Allocate a new string and copy an uppercased version of the string it
    for(char &ch : string) {
        ch = toupper(static_cast<uint8_t>(ch));
    }

    result->tag = STRING_TAG;
    result->val.str = AllocNStringCpyEx(string);

    return true;
}

static int tolowerMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(window);
    std::string string;

    // Validate arguments and convert to int
    if(!readArguments(argList, nArgs, 0, errMsg, &string)) {
        return false;
    }

    // Allocate a new string and copy an uppercased version of the string it
    for(char &ch : string) {
        ch = tolower(static_cast<uint8_t>(ch));
    }

    result->tag = STRING_TAG;
    result->val.str = AllocNStringCpyEx(string);

    return true;
}

static int stringToClipboardMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(window);

    QString string;

    // Get the string argument
    if(!readArguments(argList, nArgs, 0, errMsg, &string)) {
        return false;
    }

    result->tag = NO_TAG;
    QApplication::clipboard()->setText(string, QClipboard::Clipboard);
    return true;
}

static int clipboardToStringMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(window);
    Q_UNUSED(argList);

    // Should have no arguments
    if (nArgs != 0) {
        return wrongNArgsErr(errMsg);
    }

    // Ask if there's a string in the clipboard, and get its length
    const QMimeData *data = QApplication::clipboard()->mimeData(QClipboard::Clipboard);
    if(!data->hasText()) {
        result->tag = STRING_TAG;
        result->val.str.rep = PERM_ALLOC_STR("");
        result->val.str.len = 0;
    } else {
        // Allocate a new string to hold the data
        result->tag = STRING_TAG;
        result->val.str = AllocNStringCpyEx(data->text());
    }

    return true;
}

/*
** Built-in macro subroutine for reading the contents of a text file into
** a string.  On success, returns 1 in $readStatus, and the contents of the
** file as a string in the subroutine return value.  On failure, returns
** the empty string "" and an 0 $readStatus.
*/
static int readFileMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    Q_UNUSED(window);

    std::string name;

    // Validate arguments
    if(!readArguments(argList, nArgs, 0, errMsg, &name)) {
        return false;
    }

    // Read the whole file into an allocated string
    std::ifstream file(name, std::ios::binary);
    if(file) {
        std::string contents(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});
        result->tag = STRING_TAG;
        result->val.str = AllocNStringCpyEx(contents);

        // Return the results
        ReturnGlobals[READ_STATUS]->value.tag = INT_TAG;
        ReturnGlobals[READ_STATUS]->value.val.n = true;
        return true;
    }

    ReturnGlobals[READ_STATUS]->value.tag = INT_TAG;
    ReturnGlobals[READ_STATUS]->value.val.n = false;
    result->tag = STRING_TAG;
    result->val.str.rep = PERM_ALLOC_STR("");
    result->val.str.len = 0;
    return true;
}

/*
** Built-in macro subroutines for writing or appending a string (parameter $1)
** to a file named in parameter $2. Returns 1 on successful write, or 0 if
** unsuccessful.
*/
static int writeFileMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    return writeOrAppendFile(false, window, argList, nArgs, result, errMsg);
}

static int appendFileMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    return writeOrAppendFile(true, window, argList, nArgs, result, errMsg);
}

static int writeOrAppendFile(bool append, DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(window);

    std::string name;
    std::string string;
    FILE *fp;

    // Validate argument
    if(!readArguments(argList, nArgs, 0, errMsg, &string, &name)) {
        return false;
    }

    // open the file
    if ((fp = fopen(name.c_str(), append ? "a" : "w")) == nullptr) {
        result->tag = INT_TAG;
        result->val.n = false;
        return true;
    }

    // write the string to the file
    fwrite(string.data(), 1, string.size(), fp);
    if (ferror(fp)) {
        fclose(fp);
        result->tag = INT_TAG;
        result->val.n = false;
        return true;
    }
    fclose(fp);

    // return the status
    result->tag = INT_TAG;
    result->val.n = true;
    return true;
}

/*
** Built-in macro subroutine for searching silently in a window without
** dialogs, beeps, or changes to the selection.  Arguments are: $1: string to
** search for, $2: starting position. Optional arguments may include the
** strings: "wrap" to make the search wrap around the beginning or end of the
** string, "backward" or "forward" to change the search direction ("forward" is
** the default), "literal", "case" or "regex" to change the search type
** (default is "literal").
**
** Returns the starting position of the match, or -1 if nothing matched.
** also returns the ending position of the match in $searchEndPos
*/
static int searchMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    DataValue newArgList[9];

    /* Use the search string routine, by adding the buffer contents as
       the string argument */
    if (nArgs > 8)
        return wrongNArgsErr(errMsg);

    /* we remove constness from BufAsStringEx() result since we know
       searchStringMS will not modify the result */
    newArgList[0].tag = STRING_TAG;
    newArgList[0].val.str.rep = const_cast<char *>(window->buffer_->BufAsString());
    newArgList[0].val.str.len = window->buffer_->BufGetLength();

    // copy other arguments to the new argument list
    memcpy(&newArgList[1], argList, nArgs * sizeof(DataValue));

    return searchStringMS(window, newArgList, nArgs + 1, result, errMsg);
}

/*
** Built-in macro subroutine for searching a string.  Arguments are $1:
** string to search in, $2: string to search for, $3: starting position.
** Optional arguments may include the strings: "wrap" to make the search
** wrap around the beginning or end of the string, "backward" or "forward"
** to change the search direction ("forward" is the default), "literal",
** "case" or "regex" to change the search type (default is "literal").
**
** Returns the starting position of the match, or -1 if nothing matched.
** also returns the ending position of the match in $searchEndPos
*/
static int searchStringMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    int beginPos;
    WrapMode wrap;
    bool found = false;
    int foundStart;
    int foundEnd;
    SearchType type;
    bool skipSearch = false;
    std::string string;
    QString searchStr;
    SearchDirection direction;

    // Validate arguments and convert to proper types
    if (nArgs < 3)
        return tooFewArgsErr(errMsg);
    if (!readArgument(argList[0], &string, errMsg))
        return false;
    if (!readArgument(argList[1], &searchStr, errMsg))
        return false;
    if (!readArgument(argList[2], &beginPos, errMsg))
        return false;
    if (!readSearchArgs(&argList[3], nArgs - 3, &direction, &type, &wrap, errMsg))
        return false;

    int len = argList[0].val.str.len;
    if (beginPos > len) {
        if (direction == SEARCH_FORWARD) {
            if (wrap == WrapMode::Wrap) {
                beginPos = 0; // Wrap immediately
            } else {
                found = false;
                skipSearch = true;
            }
        } else {
            beginPos = len;
        }
    } else if (beginPos < 0) {
        if (direction == SEARCH_BACKWARD) {
            if (wrap == WrapMode::Wrap) {
                beginPos = len; // Wrap immediately
            } else {
                found = false;
                skipSearch = true;
            }
        } else {
            beginPos = 0;
        }
    }

    if (!skipSearch) {
        found = SearchString(
                    string,
                    searchStr,
                    direction,
                    type,
                    wrap,
                    beginPos,
                    &foundStart,
                    &foundEnd,
                    nullptr,
                    nullptr,
                    GetWindowDelimitersEx(window).toLatin1().data());
    }

    // Return the results
    ReturnGlobals[SEARCH_END]->value.tag = INT_TAG;
    ReturnGlobals[SEARCH_END]->value.val.n = found ? foundEnd : 0;
    result->tag = INT_TAG;
    result->val.n = found ? foundStart : -1;
    return true;
}

/*
** Built-in macro subroutine for replacing all occurences of a search string in
** a string with a replacement string.  Arguments are $1: string to search in,
** $2: string to search for, $3: replacement string. Also takes an optional
** search type: one of "literal", "case" or "regex" (default is "literal"), and
** an optional "copy" argument.
**
** Returns a new string with all of the replacements done.  If no replacements
** were performed and "copy" was specified, returns a copy of the original
** string.  Otherwise returns an empty string ("").
*/
static int replaceInStringMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    std::string string;
    QString searchStr;
    std::string replaceStr;
    SearchType searchType = SEARCH_LITERAL;
    int copyStart;
    int copyEnd;
    bool force = false;
    int i;

    // Validate arguments and convert to proper types
    if (nArgs < 3 || nArgs > 5)
        return wrongNArgsErr(errMsg);
    if (!readArgument(argList[0], &string, errMsg))
        return false;
    if (!readArgument(argList[1], &searchStr, errMsg))
        return false;
    if (!readArgument(argList[2], &replaceStr, errMsg))
        return false;

    for (i = 3; i < nArgs; i++) {
        // Read the optional search type and force arguments
        QString argStr;

        if (!readArgument(argList[i], &argStr, errMsg))
            return false;

        if (!StringToSearchType(argStr, &searchType)) {
            // It's not a search type.  is it "copy"?
            if (argStr == QLatin1String("copy")) {
                force = true;
            } else {
                *errMsg = "unrecognized argument to %s";
                return false;
            }
        }
    }

    // Do the replace
    bool ok;
    std::string replacedStr = ReplaceAllInStringEx(
                string,
                searchStr,
                replaceStr.c_str(),
                searchType,
                &copyStart,
                &copyEnd,
                GetWindowDelimitersEx(window).toLatin1().data(),
                &ok);

    // Return the results
    result->tag = STRING_TAG;
    if(!ok) {
        if (force) {
            result->val.str = AllocNStringCpyEx(string);
        } else {
            result->val.str.rep = PERM_ALLOC_STR("");
            result->val.str.len = 0;
        }
    } else {
        std::string new_string;
        new_string.reserve(string.size() + replacedStr.size());

        new_string.append(string.substr(0, copyStart));
        new_string.append(replacedStr);
        new_string.append(string.substr(copyEnd));
        result->val.str = AllocNStringCpyEx(new_string);
    }

    return true;
}

static int readSearchArgs(DataValue *argList, int nArgs, SearchDirection *searchDirection, SearchType *searchType, WrapMode *wrap, const char **errMsg) {

    QString argStr;

    *wrap            = WrapMode::NoWrap;
    *searchDirection = SEARCH_FORWARD;
    *searchType      = SEARCH_LITERAL;

    for (int i = 0; i < nArgs; i++) {
        if (!readArgument(argList[i], &argStr, errMsg))
            return false;
        else if (argStr == QLatin1String("wrap"))
            *wrap = WrapMode::Wrap;
        else if (argStr == QLatin1String("nowrap"))
            *wrap = WrapMode::NoWrap;
        else if (argStr == QLatin1String("backward"))
            *searchDirection = SEARCH_BACKWARD;
        else if (argStr == QLatin1String("forward"))
            *searchDirection = SEARCH_FORWARD;
        else if (!StringToSearchType(argStr, searchType)) {
            *errMsg = "Unrecognized argument to %s";
            return false;
        }
    }
    return true;
}

static int setCursorPosMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    int pos;

    // Get argument and convert to int
    if(!readArguments(argList, nArgs, 0, errMsg, &pos)) {
        return false;
    }

    // Set the position
    auto textD = window->toWindow()->lastFocus_;
    textD->TextSetCursorPos(pos);
    result->tag = NO_TAG;
    return true;
}

static int selectMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    int start;
    int end;

    // Get arguments and convert to int
    if(!readArguments(argList, nArgs, 0, errMsg, &start, &end)) {
        return false;
    }

    // Verify integrity of arguments
    if (start > end) {
        qSwap(start, end);
    }

    start = qBound(0, start, window->buffer_->BufGetLength());
    end   = qBound(0, end,   window->buffer_->BufGetLength());

    // Make the selection
    window->buffer_->BufSelect(start, end);
    result->tag = NO_TAG;
    return true;
}

static int selectRectangleMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    int start, end, left, right;

    // Get arguments and convert to int
    if(!readArguments(argList, nArgs, 0, errMsg, &start, &end, &left, &right)) {
        return false;
    }

    // Make the selection
    window->buffer_->BufRectSelect(start, end, left, right);
    result->tag = NO_TAG;
    return true;
}

/*
** Macro subroutine to ring the bell
*/
static int beepMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(argList);
    Q_UNUSED(window);

    if (nArgs != 0) {
        return wrongNArgsErr(errMsg);
    }

    QApplication::beep();
    result->tag = NO_TAG;
    return true;
}

static int tPrintMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(window);

    std::string string;

    if (nArgs == 0) {
        return tooFewArgsErr(errMsg);
    }

    for (int i = 0; i < nArgs; i++) {
        if (!readArgument(argList[i], &string, errMsg)) {
            return false;
        }

        printf("%s%s", string.c_str(), i == nArgs - 1 ? "" : " ");
    }

    fflush(stdout);
    result->tag = NO_TAG;
    return true;
}

/*
** Built-in macro subroutine for getting the value of an environment variable
*/
static int getenvMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(window);

    std::string name;

    // Get name of variable to get
    if(!readArguments(argList, nArgs, 0, errMsg, &name)) {
        *errMsg = "argument to %s must be a string";
        return false;
    }

    QByteArray value = qgetenv(name.c_str());

    // Return the text as an allocated string
    result->tag = STRING_TAG;
    result->val.str = AllocNStringCpyEx(QString::fromLocal8Bit(value));
    return true;
}

static int shellCmdMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    std::string cmdString;
    std::string inputString;

    if(!readArguments(argList, nArgs, 0, errMsg, &cmdString, &inputString)) {
        return false;
    }

    /* Shell command execution requires that the macro be suspended, so
       this subroutine can't be run if macro execution can't be interrupted */
    if (!MacroRunWindowEx()->macroCmdData_) {
        *errMsg = "%s can't be called from non-suspendable context";
        return false;
    }

    document->ShellCmdToMacroStringEx(cmdString, inputString);
    result->tag = INT_TAG;
    result->val.n = 0;
    return true;
}

/*
** Method used by ShellCmdToMacroString (called by shellCmdMS), for returning
** macro string and exit status after the execution of a shell command is
** complete.  (Sorry about the poor modularity here, it's just not worth
** teaching other modules about macro return globals, since other than this,
** they're not used outside of macro.c)
*/
void ReturnShellCommandOutputEx(DocumentWidget *window, const QString &outText, int status) {

    auto cmdData = static_cast<macroCmdInfoEx *>(window->macroCmdData_);
    if(!cmdData) {
        return;
    }

    DataValue retVal;
    retVal.tag = STRING_TAG;
    retVal.val.str = AllocNStringCpyEx(outText);
    ModifyReturnedValueEx(cmdData->context, retVal);
    ReturnGlobals[SHELL_CMD_STATUS]->value.tag = INT_TAG;
    ReturnGlobals[SHELL_CMD_STATUS]->value.val.n = status;
}

static int dialogMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    QString btnLabel;
    QString message;
    long i;

    /* Ignore the focused window passed as the function argument and put
       the dialog up over the window which is executing the macro */
    document = MacroRunWindowEx();
    auto cmdData = static_cast<macroCmdInfoEx *>(document->macroCmdData_);

    /* Dialogs require macro to be suspended and interleaved with other macros.
       This subroutine can't be run if macro execution can't be interrupted */
    if (!cmdData) {
        *errMsg = "%s can't be called from non-suspendable context";
        return false;
    }

    /* Read and check the arguments.  The first being the dialog message,
       and the rest being the button labels */
    if (nArgs == 0) {
        *errMsg = "%s subroutine called with no arguments";
        return false;
    }
    if (!readArgument(argList[0], &message, errMsg)) {
        return false;
    }

    // check that all button labels can be read
    for (i = 1; i < nArgs; i++) {
        if (!readArgument(argList[i], &btnLabel, errMsg)) {
            return false;
        }
    }

    // Stop macro execution until the dialog is complete
    PreemptMacro();

    // Return placeholder result.  Value will be changed by button callback
    result->tag = INT_TAG;
    result->val.n = 0;

    auto prompt = new DialogPrompt(nullptr /*parent*/);
    prompt->setMessage(message);
    if (nArgs == 1) {
        prompt->addButton(QDialogButtonBox::Ok);
    } else {
        for(int i = 1; i < nArgs; ++i) {
            readArgument(argList[i], &btnLabel, errMsg);
            prompt->addButton(btnLabel);
        }
    }
    prompt->exec();
    result->val.n = prompt->result();
    ModifyReturnedValueEx(cmdData->context, *result);
    delete prompt;

    ResumeMacroExecutionEx(document);
    return true;
}

static int stringDialogMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    QString btnLabel;
    QString message;
    long i;

    /* Ignore the focused window passed as the function argument and put
       the dialog up over the window which is executing the macro */
    document = MacroRunWindowEx();
    auto cmdData = static_cast<macroCmdInfoEx *>(document->macroCmdData_);

    /* Dialogs require macro to be suspended and interleaved with other macros.
       This subroutine can't be run if macro execution can't be interrupted */
    if (!cmdData) {
        *errMsg = "%s can't be called from non-suspendable context";
        return false;
    }

    /* Read and check the arguments.  The first being the dialog message,
       and the rest being the button labels */
    if (nArgs == 0) {
        *errMsg = "%s subroutine called with no arguments";
        return false;
    }
    if (!readArgument(argList[0], &message, errMsg)) {
        return false;
    }

    // check that all button labels can be read
    for (i = 1; i < nArgs; i++) {
        if (!readArgument(argList[i], &btnLabel, errMsg)) {
            return false;
        }
    }

    // Stop macro execution until the dialog is complete
    PreemptMacro();

    // Return placeholder result.  Value will be changed by button callback
    result->tag = INT_TAG;
    result->val.n = 0;

    auto prompt = std::make_unique<DialogPromptString>(nullptr /*parent*/);
    prompt->setMessage(message);
    if (nArgs == 1) {
        prompt->addButton(QDialogButtonBox::Ok);
    } else {
        for(int i = 1; i < nArgs; ++i) {
            readArgument(argList[i], &btnLabel, errMsg);
            prompt->addButton(btnLabel);
        }
    }
    prompt->exec();

    // Return the button number in the global variable $string_dialog_button
    ReturnGlobals[STRING_DIALOG_BUTTON]->value.tag = INT_TAG;
    ReturnGlobals[STRING_DIALOG_BUTTON]->value.val.n = prompt->result();

    result->tag = STRING_TAG;
    result->val.str = AllocNStringCpyEx(prompt->text());
    ModifyReturnedValueEx(cmdData->context, *result);

    ResumeMacroExecutionEx(document);
    return true;
}

/*
** A subroutine to put up a calltip
** First arg is either text to be displayed or a key for tip/tag lookup.
** Optional second arg is the buffer position beneath which to display the
**      upper-left corner of the tip.  Default (or -1) puts it under the cursor.
** Additional optional arguments:
**      "tipText": (default) Indicates first arg is text to be displayed in tip.
**      "tipKey":   Indicates first arg is key in calltips database.  If key
**                  is not found in tip database then the tags database is also
**                  searched.
**      "tagKey":   Indicates first arg is key in tags database.  (Skips
**                  search in calltips database.)
**      "center":   Horizontally center the calltip at the position
**      "right":    Put the right edge of the calltip at the position
**                  "center" and "right" cannot both be specified.
**      "above":    Place the calltip above the position
**      "strict":   Don't move the calltip to keep it on-screen and away
**                  from the cursor's line.
**
** Returns the new calltip's ID on success, 0 on failure.
**
** Does this need to go on IgnoredActions?  I don't think so, since
** showing a calltip may be part of the action you want to learn.
*/
static int calltipMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    std::string tipText;
    std::string txtArg;
    bool anchored = false;
    bool lookup = true;
    int mode = -1;
    int i;
    int anchorPos;
    int hAlign = TIP_LEFT;
    int vAlign = TIP_BELOW;
    int alignMode = TIP_SLOPPY;

    // Read and check the string
    if (nArgs < 1) {
        *errMsg = "%s subroutine called with too few arguments";
        return false;
    }
    if (nArgs > 6) {
        *errMsg = "%s subroutine called with too many arguments";
        return false;
    }

    // Read the tip text or key
    if (!readArgument(argList[0], &tipText, errMsg))
        return false;

    // Read the anchor position (-1 for unanchored)
    if (nArgs > 1) {
        if (!readArgument(argList[1], &anchorPos, errMsg))
            return false;
    } else {
        anchorPos = -1;
    }
    if (anchorPos >= 0)
        anchored = true;

    // Any further args are directives for relative positioning
    for (i = 2; i < nArgs; ++i) {
        if (!readArgument(argList[i], &txtArg, errMsg)) {
            return false;
        }
        switch (txtArg[0]) {
        case 'c':
            if (txtArg == "center")
                goto bad_arg;
            hAlign = TIP_CENTER;
            break;
        case 'r':
            if (txtArg == "right")
                goto bad_arg;
            hAlign = TIP_RIGHT;
            break;
        case 'a':
            if (txtArg == "above")
                goto bad_arg;
            vAlign = TIP_ABOVE;
            break;
        case 's':
            if (txtArg == "strict")
                goto bad_arg;
            alignMode = TIP_STRICT;
            break;
        case 't':
            if (txtArg == "tipText") {
                mode = -1;
            } else if (txtArg == "tipKey") {
                mode = TIP;
            } else if (txtArg == "tagKey") {
                mode = TIP_FROM_TAG;
            } else {
                goto bad_arg;
            }
            break;
        default:
            goto bad_arg;
        }
    }

    result->tag = INT_TAG;
    if (mode < 0) {
        lookup = false;
    }
    // Look up (maybe) a calltip and display it
    result->val.n = ShowTipStringEx(document, tipText.c_str(), anchored, anchorPos, lookup, mode, hAlign, vAlign, alignMode);

    return true;

bad_arg:
    /* This is how the (more informative) global var. version would work,
        assuming there was a global buffer called msg.  */
    /* sprintf(msg, "unrecognized argument to %%s: \"%s\"", txtArg);
    *errMsg = msg; */
    *errMsg = "unrecognized argument to %s";
    return false;
}

/*
** A subroutine to kill the current calltip
*/
static int killCalltipMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    int calltipID = 0;

    if (nArgs > 1) {
        *errMsg = "%s subroutine called with too many arguments";
        return false;
    }
    if (nArgs > 0) {
        if (!readArgument(argList[0], &calltipID, errMsg))
            return false;
    }

    document->toWindow()->lastFocus_->TextDKillCalltip(calltipID);

    result->tag = NO_TAG;
    return true;
}

/*
 * A subroutine to get the ID of the current calltip, or 0 if there is none.
 */
static int calltipIDMV(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    result->tag = INT_TAG;
    result->val.n = document->toWindow()->lastFocus_->TextDGetCalltipID(0);
    return true;
}

static int replaceAllInSelectionMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    // replace_in_selection( search-string, replace-string [, search-type] )

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunWindowEx();

    //  Get the argument list.
    QString searchString;
    QString replaceString;

    if(!readArguments(argList, nArgs, 0, errMsg, &searchString, &replaceString)) {
        return false;
    }

    SearchType type = searchType(argList, nArgs, 2);

    result->tag = INT_TAG;
    result->val.n = 0;
    document->replaceInSelAP(searchString, replaceString, type);
    return true;
}

static int replaceAllMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    // replace_all( search-string, replace-string [, search-type] )

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunWindowEx();

    if (nArgs < 2 || nArgs > 3) {
        return wrongNArgsErr(errMsg);
    }

    //  Get the argument list.
    QString searchString;
    QString replaceString;

    if(!readArguments(argList, nArgs, 0, errMsg, &searchString, &replaceString)) {
        return false;
    }

    SearchType type = searchType(argList, nArgs, 2);

    result->tag = INT_TAG;
    result->val.n = 0;
    document->replaceAllAP(searchString, replaceString, type);
    return true;
}

/*
**  filename_dialog([title[, mode[, defaultPath[, filter[, defaultName]]]]])
**
**  Presents a FileSelectionDialog to the user prompting for a new file.
**
**  Options are:
**  title       - will be the title of the dialog, defaults to "Choose file".
**  mode        - if set to "exist" (default), the "New File Name" TextField
**                of the FSB will be unmanaged. If "new", the TextField will
**                be managed.
**  defaultPath - is the default path to use. Default (or "") will use the
**                active document's directory.
**  filter      - the file glob which determines which files to display.
**                Is set to "*" if filter is "" and by default.
**  defaultName - is the default filename that is filled in automatically.
**
** Returns "" if the user cancelled the dialog, otherwise returns the path to
** the file that was selected
**
** Note that defaultName doesn't work on all *tifs.  :-(
*/
static int filenameDialogMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    QString title           = QLatin1String("Choose Filename");
    QString mode            = QLatin1String("exist");
    QString defaultPath;
    QString defaultFilter;
    QString defaultName;


    /* Ignore the focused window passed as the function argument and put
       the dialog up over the window which is executing the macro */
    window = MacroRunWindowEx();

    /* Dialogs require macro to be suspended and interleaved with other macros.
       This subroutine can't be run if macro execution can't be interrupted */
    if (!window->macroCmdData_) {
        M_FAILURE("%s can't be called from non-suspendable context");
    }

    //  Get the argument list.
    if (nArgs > 0 && !readArgument(argList[0], &title, errMsg)) {
        return false;
    }

    if (nArgs > 1 && !readArgument(argList[1], &mode, errMsg)) {
        return false;
    }
    if ((mode != QLatin1String("exist")) != 0 && (mode != QLatin1String("new"))) {
        M_FAILURE("Invalid value for mode in %s");
    }

    if (nArgs > 2 && !readArgument(argList[2], &defaultPath, errMsg)) {
        return false;
    }

    if (nArgs > 3 && !readArgument(argList[3], &defaultFilter,  errMsg)) {
        return false;
    }

    if (nArgs > 4 && !readArgument(argList[4], &defaultName, errMsg)) {
        return false;
    }

    if (nArgs > 5) {
        M_FAILURE("%s called with too many arguments. Expects at most 5 arguments.");
    }

    //  Set default directory
    if (defaultPath.isEmpty()) {
        defaultPath = window->path_;
    }

    //  Set default filter
    if(defaultFilter.isEmpty()) {
        defaultFilter = QLatin1String("*");
    }

    bool gfnResult;
    QString filename;
    if (mode == QLatin1String("exist")) {
        // NOTE(eteran); filters probably don't work quite the same with Qt's dialog
        QString existingFile = QFileDialog::getOpenFileName(/*this*/ nullptr, title, defaultPath, defaultFilter, nullptr);
        if(!existingFile.isNull()) {
            filename = existingFile;
            gfnResult = true;
        } else {
            gfnResult = false;
        }
    } else {
        // NOTE(eteran); filters probably don't work quite the same with Qt's dialog
        QString newFile = QFileDialog::getSaveFileName(/*this*/ nullptr, title, defaultPath, defaultFilter, nullptr);
        if(!newFile.isNull()) {
            filename  = newFile;
            gfnResult = true;
        } else {
            gfnResult = false;
        }
    } //  Invalid values are weeded out above.


    result->tag = STRING_TAG;
    if (gfnResult == true) {
        //  Got a string, copy it to the result
        result->val.str = AllocNStringCpyEx(filename);
    } else {
        // User cancelled.  Return ""
        result->val.str.rep = PERM_ALLOC_STR("");
        result->val.str.len = 0;
    }

    return true;
}

// T Balinski
static int listDialogMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    QString btnLabel;
    QString message;
    QString text;
    long i;

    /* Ignore the focused window passed as the function argument and put
       the dialog up over the window which is executing the macro */
    window = MacroRunWindowEx();
    auto cmdData = static_cast<macroCmdInfoEx *>(window->macroCmdData_);

    /* Dialogs require macro to be suspended and interleaved with other macros.
       This subroutine can't be run if macro execution can't be interrupted */
    if (!cmdData) {
        *errMsg = "%s can't be called from non-suspendable context";
        return false;
    }

    /* Read and check the arguments.  The first being the dialog message,
       and the rest being the button labels */
    if (nArgs < 2) {
        *errMsg = "%s subroutine called with no message, string or arguments";
        return false;
    }

    if (!readArgument(argList[0], &message, errMsg))
        return false;

    if (!readArgument(argList[1], &text, errMsg))
        return false;

    if (text.isEmpty()) {
        *errMsg = "%s subroutine called with empty list data";
        return false;
    }

    // check that all button labels can be read
    for (i = 2; i < nArgs; i++) {
        if (!readArgument(argList[i], &btnLabel, errMsg)) {
            return false;
        }
    }

    // Stop macro execution until the dialog is complete
    PreemptMacro();

    // Return placeholder result.  Value will be changed by button callback
    result->tag = INT_TAG;
    result->val.n = 0;

    auto prompt = std::make_unique<DialogPromptList>(nullptr /*parent*/);
    prompt->setMessage(message);
    prompt->setList(text);
    if (nArgs == 2) {
        prompt->addButton(QDialogButtonBox::Ok);
    } else {
        for(int i = 2; i < nArgs; ++i) {
            readArgument(argList[i], &btnLabel, errMsg);
            prompt->addButton(btnLabel);
        }
    }

    prompt->exec();

    // Return the button number in the global variable $string_dialog_button
    ReturnGlobals[STRING_DIALOG_BUTTON]->value.tag = INT_TAG;
    ReturnGlobals[STRING_DIALOG_BUTTON]->value.val.n = prompt->result();

    result->tag = STRING_TAG;
    result->val.str = AllocNStringCpyEx(prompt->text());
    ModifyReturnedValueEx(cmdData->context, *result);

    ResumeMacroExecutionEx(window);
    return true;
}

static int stringCompareMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(window);

    QString leftStr;
    QString rightStr;
    QString argStr;
    bool considerCase = true;
    int i;
    int compareResult;


    if (nArgs < 2) {
        return (wrongNArgsErr(errMsg));
    }

    if (!readArgument(argList[0], &leftStr, errMsg))
        return false;

    if (!readArgument(argList[1], &rightStr, errMsg))
        return false;

    for (i = 2; i < nArgs; ++i) {
        if (!readArgument(argList[i], &argStr, errMsg))
            return false;
        else if (argStr == QLatin1String("case"))
            considerCase = true;
        else if (argStr == QLatin1String("nocase"))
            considerCase = false;
        else {
            *errMsg = "Unrecognized argument to %s";
            return false;
        }
    }

    if (considerCase) {
        compareResult = leftStr.compare(rightStr, Qt::CaseSensitive);
    } else {
        compareResult = leftStr.compare(rightStr, Qt::CaseInsensitive);
    }

    compareResult = qBound(-1, compareResult, 1);

    result->tag = INT_TAG;
    result->val.n = compareResult;
    return true;
}

/*
** This function is intended to split strings into an array of substrings
** Importatnt note: It should always return at least one entry with key 0
** split("", ",") result[0] = ""
** split("1,2", ",") result[0] = "1" result[1] = "2"
** split("1,2,", ",") result[0] = "1" result[1] = "2" result[2] = ""
**
** This behavior is specifically important when used to break up
** array sub-scripts
*/

static int splitMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    std::string sourceStr;
    QString splitStr;
    bool validSplit = true;
    SearchType searchType;
    int foundStart;
    int foundEnd;
    int elementEnd;
    char indexStr[TYPE_INT_STR_SIZE(int)];
    char *allocIndexStr;
    DataValue element;
    int elementLen;

    if (nArgs < 2) {
        return (wrongNArgsErr(errMsg));
    }
    if (!readArgument(argList[0], &sourceStr, errMsg)) {
        *errMsg = "first argument must be a string: %s";
        return false;
    }
    if (!readArgument(argList[1], &splitStr, errMsg)) {
        validSplit = false;
    } else {
        if (splitStr.isEmpty()) {
            validSplit = false;
        }
    }
    if(!validSplit) {
        *errMsg = "second argument must be a non-empty string: %s";
        return false;
    }

    QString typeSplitStr;
    if (nArgs > 2 && readArgument(argList[2], &typeSplitStr, errMsg)) {
        if (!StringToSearchType(typeSplitStr, &searchType)) {
            *errMsg = "unrecognized argument to %s";
            return false;
        }
    } else {
        searchType = SEARCH_LITERAL;
    }

    result->tag = ARRAY_TAG;
    result->val.arrayPtr = ArrayNew();

    int beginPos  = 0;
    int lastEnd   = 0;
    int indexNum  = 0;
    int strLength = sourceStr.size();
    bool found    = true;
    while (found && beginPos < strLength) {
        sprintf(indexStr, "%d", indexNum);
        allocIndexStr = AllocString(strlen(indexStr) + 1);
        if (!allocIndexStr) {
            *errMsg = "array element failed to allocate key: %s";
            return false;
        }
        strcpy(allocIndexStr, indexStr);

        found = SearchString(
                    sourceStr,
                    splitStr,
                    SEARCH_FORWARD,
                    searchType,
                    WrapMode::NoWrap,
                    beginPos,
                    &foundStart,
                    &foundEnd,
                    nullptr,
                    nullptr,
                    GetWindowDelimitersEx(window).toLatin1().data());

        elementEnd = found ? foundStart : strLength;
        elementLen = elementEnd - lastEnd;
        element.tag = STRING_TAG;
        if (!AllocNStringNCpy(&element.val.str, &sourceStr[lastEnd], elementLen)) {
            *errMsg = "failed to allocate element value: %s";
            return false;
        }

        if (!ArrayInsert(result, allocIndexStr, &element)) {
            M_ARRAY_INSERT_FAILURE();
        }

        if (found) {
            if (foundStart == foundEnd) {
                beginPos = foundEnd + 1; // Avoid endless loop for 0-width match
            } else {
                beginPos = foundEnd;
            }
        } else {
            beginPos = strLength; // Break the loop
        }
        lastEnd = foundEnd;
        ++indexNum;
    }
    if (found) {
        sprintf(indexStr, "%d", indexNum);
        allocIndexStr = AllocString(strlen(indexStr) + 1);
        if (!allocIndexStr) {
            *errMsg = "array element failed to allocate key: %s";
            return false;
        }
        strcpy(allocIndexStr, indexStr);
        element.tag = STRING_TAG;
        if (lastEnd == strLength) {
            // The pattern mathed the end of the string. Add an empty chunk.
            element.val.str.rep = PERM_ALLOC_STR("");
            element.val.str.len = 0;

            if (!ArrayInsert(result, allocIndexStr, &element)) {
                M_ARRAY_INSERT_FAILURE();
            }
        } else {
            /* We skipped the last character to prevent an endless loop.
               Add it to the list. */
            elementLen = strLength - lastEnd;
            if (!AllocNStringNCpy(&element.val.str, &sourceStr[lastEnd], elementLen)) {
                *errMsg = "failed to allocate element value: %s";
                return false;
            }

            if (!ArrayInsert(result, allocIndexStr, &element)) {
                M_ARRAY_INSERT_FAILURE();
            }

            /* If the pattern can match zero-length strings, we may have to
               add a final empty chunk.
               For instance:  split("abc\n", "$", "regex")
                 -> matches before \n and at end of string
                 -> expected output: "abc", "\n", ""
               The '\n' gets added in the lines above, but we still have to
               verify whether the pattern also matches the end of the string,
               and add an empty chunk in case it does. */
            found = SearchString(
                        sourceStr,
                        splitStr,
                        SEARCH_FORWARD,
                        searchType,
                        WrapMode::NoWrap,
                        strLength,
                        &foundStart,
                        &foundEnd,
                        nullptr,
                        nullptr,
                        GetWindowDelimitersEx(window).toLatin1().data());
            if (found) {
                ++indexNum;
                sprintf(indexStr, "%d", indexNum);
                allocIndexStr = AllocString(strlen(indexStr) + 1);
                if (!allocIndexStr) {
                    *errMsg = "array element failed to allocate key: %s";
                    return false;
                }
                strcpy(allocIndexStr, indexStr);
                element.tag = STRING_TAG;
                element.val.str.rep = PERM_ALLOC_STR("");
                element.val.str.len = 0;

                if (!ArrayInsert(result, allocIndexStr, &element)) {
                    M_ARRAY_INSERT_FAILURE();
                }
            }
        }
    }
    return true;
}

/*
** Set the backlighting string resource for the current window. If no parameter
** is passed or the value "default" is passed, it attempts to set the preference
** value of the resource. If the empty string is passed, the backlighting string
** will be cleared, turning off backlighting.
*/
#if 0 // DISABLED for 5.4
static int setBacklightStringMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(result);

    QString backlightString;

    if (nArgs == 0) {
      backlightString = GetPrefBacklightCharTypes();
    } else if (nArgs == 1) {
      if (argList[0].tag != STRING_TAG) {
          *errMsg = "%s not called with a string parameter";
          return false;
      }
      backlightString = QString::fromLatin1(argList[0].val.str.rep);
    } else {
      return wrongNArgsErr(errMsg);
    }

    if (backlightString == QLatin1String("default"))
      backlightString = GetPrefBacklightCharTypes();

    if (backlightString.isEmpty())  /* empty string param */
      backlightString = QString();  /* turns of backlighting */

    window->SetBacklightChars(backlightString);
    return true;
}
#endif

static int cursorMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    auto textD    = window->toWindow()->lastFocus_;
    result->tag   = INT_TAG;
    result->val.n = textD->TextGetCursorPos();
    return true;
}

static int lineMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    int line;
    int colNum;

    auto textD  = window->toWindow()->lastFocus_;
    result->tag = INT_TAG;
    int cursorPos   = textD->TextGetCursorPos();

    if (!textD->TextDPosToLineAndCol(cursorPos, &line, &colNum)) {
        line = window->buffer_->BufCountLines(0, cursorPos) + 1;
    }

    result->val.n = line;
    return true;
}

static int columnMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    TextBuffer *buf = window->buffer_;

    auto textD    = window->toWindow()->lastFocus_;
    result->tag   = INT_TAG;
    int cursorPos = textD->TextGetCursorPos();
    result->val.n = buf->BufCountDispChars(buf->BufStartOfLine(cursorPos), cursorPos);
    return true;
}

static int fileNameMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    result->tag     = STRING_TAG;
    result->val.str = AllocNStringCpyEx(window->filename_);
    return true;
}

static int filePathMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    result->tag     = STRING_TAG;
    result->val.str = AllocNStringCpyEx(window->path_);
    return true;
}

static int lengthMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    result->tag = INT_TAG;
    result->val.n = window->buffer_->BufGetLength();
    return true;
}

static int selectionStartMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    result->tag = INT_TAG;
    result->val.n = window->buffer_->primary_.selected ? window->buffer_->primary_.start : -1;
    return true;
}

static int selectionEndMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    result->tag = INT_TAG;
    result->val.n = window->buffer_->primary_.selected ? window->buffer_->primary_.end : -1;
    return true;
}

static int selectionLeftMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    TextSelection *sel = &window->buffer_->primary_;

    result->tag = INT_TAG;
    result->val.n = sel->selected && sel->rectangular ? sel->rectStart : -1;
    return true;
}

static int selectionRightMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    TextSelection *sel = &window->buffer_->primary_;

    result->tag = INT_TAG;
    result->val.n = sel->selected && sel->rectangular ? sel->rectEnd : -1;
    return true;
}

static int wrapMarginMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    int margin = window->firstPane()->getWrapMargin();
    int nCols  = window->firstPane()->getColumns();

    result->tag = INT_TAG;
    result->val.n = (margin == 0) ? nCols : margin;
    return true;
}

static int statisticsLineMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    result->tag = INT_TAG;
    result->val.n = window->showStats_ ? 1 : 0;
    return true;
}

static int incSearchLineMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    result->tag = INT_TAG;
    result->val.n = window->toWindow()->showISearchLine_ ? 1 : 0;
    return true;
}

static int showLineNumbersMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    result->tag = INT_TAG;
    result->val.n = window->toWindow()->showLineNumbers_ ? 1 : 0;
    return true;
}

static int autoIndentMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    char *res = nullptr;

    switch (window->indentStyle_) {
    case NO_AUTO_INDENT:
        res = PERM_ALLOC_STR("off");
        break;
    case AUTO_INDENT:
        res = PERM_ALLOC_STR("on");
        break;
    case SMART_INDENT:
        res = PERM_ALLOC_STR("smart");
        break;
    case DEFAULT_INDENT:
        *errMsg = "Invalid indent style value encountered in %s";
        return false;
    }

    result->tag = STRING_TAG;
    result->val.str.rep = res;
    result->val.str.len = strlen(res);
    return true;
}

static int wrapTextMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    char *res = nullptr;

    switch (window->wrapMode_) {
    case NO_WRAP:
        res = PERM_ALLOC_STR("none");
        break;
    case NEWLINE_WRAP:
        res = PERM_ALLOC_STR("auto");
        break;
    case CONTINUOUS_WRAP:
        res = PERM_ALLOC_STR("continuous");
        break;
    case DEFAULT_WRAP:
        *errMsg = "Invalid wrap style value encountered in %s";
        return false;
    }

    result->tag = STRING_TAG;
    result->val.str.rep = res;
    result->val.str.len = strlen(res);
    return true;
}

static int highlightSyntaxMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    result->tag = INT_TAG;
    result->val.n = window->highlightSyntax_ ? 1 : 0;
    return true;
}

static int makeBackupCopyMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    result->tag = INT_TAG;
    result->val.n = window->saveOldVersion_ ? 1 : 0;
    return true;
}

static int incBackupMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    result->tag = INT_TAG;
    result->val.n = window->autoSave_ ? 1 : 0;
    return true;
}

static int showMatchingMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    char *res = nullptr;

    switch (window->showMatchingStyle_) {
    case NO_FLASH:
        res = PERM_ALLOC_STR(NO_FLASH_STRING);
        break;
    case FLASH_DELIMIT:
        res = PERM_ALLOC_STR(FLASH_DELIMIT_STRING);
        break;
    case FLASH_RANGE:
        res = PERM_ALLOC_STR(FLASH_RANGE_STRING);
        break;
    default:
        *errMsg = "Invalid match flashing style value encountered in %s";
        return false;
        break;
    }
    result->tag = STRING_TAG;
    result->val.str.rep = res;
    result->val.str.len = strlen(res);
    return true;
}

static int matchSyntaxBasedMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    result->tag = INT_TAG;
    result->val.n = window->matchSyntaxBased_ ? 1 : 0;
    return true;
}

static int overTypeModeMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    result->tag = INT_TAG;
    result->val.n = window->overstrike_ ? 1 : 0;
    return true;
}

static int readOnlyMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    result->tag = INT_TAG;
    result->val.n = (window->lockReasons_.isAnyLocked()) ? 1 : 0;
    return true;
}

static int lockedMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    result->tag = INT_TAG;
    result->val.n = (window->lockReasons_.isUserLocked()) ? 1 : 0;
    return true;
}

static int fileFormatMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    QString res;

    switch (window->fileFormat_) {
    case UNIX_FILE_FORMAT:
        res = QLatin1String("unix");
        break;
    case DOS_FILE_FORMAT:
        res = QLatin1String("dos");
        break;
    case MAC_FILE_FORMAT:
        res = QLatin1String("macintosh");
        break;
    default:
        *errMsg = "Invalid linefeed style value encountered in %s";
        return false;
    }

    result->tag     = STRING_TAG;
    result->val.str = AllocNStringCpyEx(res);
    return true;
}

static int fontNameMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    result->tag = STRING_TAG;
    result->val.str = AllocNStringCpyEx(window->fontName_);
    return true;
}

static int fontNameItalicMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    result->tag = STRING_TAG;
    result->val.str = AllocNStringCpyEx(window->italicFontName_);
    return true;
}

static int fontNameBoldMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    result->tag = STRING_TAG;
    result->val.str = AllocNStringCpyEx(window->boldFontName_);
    return true;
}

static int fontNameBoldItalicMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    result->tag = STRING_TAG;
    result->val.str = AllocNStringCpyEx(window->boldItalicFontName_);
    return true;
}

static int subscriptSepMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    Q_UNUSED(window);
    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    result->tag = STRING_TAG;
    result->val.str.rep = PERM_ALLOC_STR(ARRAY_DIM_SEP);
    result->val.str.len = strlen(result->val.str.rep);
    return true;
}

static int minFontWidthMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    auto textD = window->firstPane();
    result->tag = INT_TAG;
    result->val.n = textD->TextDMinFontWidth(window->highlightSyntax_);
    return true;
}

static int maxFontWidthMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    auto textD = window->firstPane();
    result->tag = INT_TAG;
    result->val.n = textD->TextDMaxFontWidth(window->highlightSyntax_);
    return true;
}

static int topLineMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    result->tag = INT_TAG;

    auto textD = window->toWindow()->lastFocus_;
    result->val.n = textD->TextFirstVisibleLine();
    return true;
}

static int numDisplayLinesMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    auto textD    = window->toWindow()->lastFocus_;
    result->tag   = INT_TAG;
    result->val.n = textD->TextNumVisibleLines();
    return true;
}

static int displayWidthMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    auto textD    = window->toWindow()->lastFocus_;
    result->tag   = INT_TAG;
    result->val.n = textD->TextVisibleWidth();
    return true;
}

static int activePaneMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(nArgs);
    Q_UNUSED(argList);
    Q_UNUSED(errMsg);

    result->tag   = INT_TAG;
    result->val.n = window->WidgetToPaneIndex(window->toWindow()->lastFocus_);
    return true;
}

static int nPanesMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(nArgs);
    Q_UNUSED(argList);
    Q_UNUSED(errMsg);

    result->tag   = INT_TAG;
    result->val.n = window->textPanesCount();
    return true;
}

static int emptyArrayMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(window);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);
    Q_UNUSED(errMsg);

    result->tag          = ARRAY_TAG;
    result->val.arrayPtr = nullptr;
    return true;
}

static int serverNameMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(window);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);
    Q_UNUSED(errMsg);

    result->tag     = STRING_TAG;
    result->val.str = AllocNStringCpyEx(GetPrefServerName());
    return true;
}

static int tabDistMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(nArgs);
    Q_UNUSED(argList);
    Q_UNUSED(errMsg);

    result->tag   = INT_TAG;
    result->val.n = window->buffer_->tabDist_;
    return true;
}

static int emTabDistMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(nArgs);
    Q_UNUSED(argList);
    Q_UNUSED(errMsg);

    int dist = window->firstPane()->getEmulateTabs();

    result->tag   = INT_TAG;
    result->val.n = dist;
    return true;
}

static int useTabsMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);
    Q_UNUSED(errMsg);

    result->tag = INT_TAG;
    result->val.n = window->buffer_->useTabs_;
    return true;
}

static int modifiedMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(nArgs);
    Q_UNUSED(argList);
    Q_UNUSED(errMsg);

    result->tag = INT_TAG;
    result->val.n = window->fileChanged_;
    return true;
}

static int languageModeMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(nArgs);
    Q_UNUSED(argList);
    Q_UNUSED(errMsg);

    QString lmName = LanguageModeName(window->languageMode_);

    if(lmName.isNull()) {
        lmName = QLatin1String("Plain");
    }

    result->tag = STRING_TAG;
    result->val.str = AllocNStringCpyEx(lmName);
    return true;
}

// --------------------------------------------------------------------------

/*
** Range set macro variables and functions
*/
static int rangesetListMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(nArgs);
    Q_UNUSED(argList);

    RangesetTable *rangesetTable = window->buffer_->rangesetTable_;
    DataValue element;

    result->tag = ARRAY_TAG;
    result->val.arrayPtr = ArrayNew();

    if(!rangesetTable) {
        return true;
    }

    uint8_t *rangesetList = RangesetTable::RangesetGetList(rangesetTable);
    int nRangesets = strlen((char *)rangesetList);
    for (int i = 0; i < nRangesets; i++) {
        element.tag = INT_TAG;
        element.val.n = rangesetList[i];

        if (!ArrayInsert(result, AllocStringCpyEx(std::to_string(nRangesets - i - 1)), &element)) {
            M_FAILURE("Failed to insert array element in %s");
        }
    }

    return true;
}

/*
**  Returns the version number of the current macro language implementation.
**  For releases, this is the same number as NEdit's major.minor version
**  number to keep things simple. For developer versions this could really
**  be anything.
**
**  Note that the current way to build $VERSION builds the same value for
**  different point revisions. This is done because the macro interface
**  does not change for the same version.
*/
static int versionMV(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(nArgs);
    Q_UNUSED(argList);
    Q_UNUSED(window);

    static const unsigned version = NEDIT_VERSION;

    result->tag = INT_TAG;
    result->val.n = version;
    return true;
}

/*
** Built-in macro subroutine to create a new rangeset or rangesets.
** If called with one argument: $1 is the number of rangesets required and
** return value is an array indexed 0 to n, with the rangeset labels as values;
** (or an empty array if the requested number of rangesets are not available).
** If called with no arguments, returns a single rangeset label (not an array),
** or an empty string if there are no rangesets available.
*/
static int rangesetCreateMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    int label;
    int i;
    int nRangesetsRequired;
    DataValue element;

    RangesetTable *rangesetTable = window->buffer_->rangesetTable_;

    if (nArgs > 1)
        return wrongNArgsErr(errMsg);

    if(!rangesetTable) {
        rangesetTable = new RangesetTable(window->buffer_);
        window->buffer_->rangesetTable_ = rangesetTable;
    }

    if (nArgs == 0) {
        label = rangesetTable->RangesetCreate();

        result->tag = INT_TAG;
        result->val.n = label;
        return true;
    } else {
        if (!readArgument(argList[0], &nRangesetsRequired, errMsg))
            return false;

        result->tag = ARRAY_TAG;
        result->val.arrayPtr = ArrayNew();

        if (nRangesetsRequired > rangesetTable->nRangesetsAvailable())
            return true;

        for (i = 0; i < nRangesetsRequired; i++) {
            element.tag = INT_TAG;
            element.val.n = rangesetTable->RangesetCreate();

            ArrayInsert(result, AllocStringCpyEx(std::to_string(i)), &element);
        }

        return true;
    }
}

/*
** Built-in macro subroutine for forgetting a range set.
*/
static int rangesetDestroyMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    RangesetTable *rangesetTable = window->buffer_->rangesetTable_;
    DataValue *array;
    DataValue element;
    char keyString[TYPE_INT_STR_SIZE(int)];
    int deleteLabels[N_RANGESETS];
    int i, arraySize;
    int label = 0;

    if (nArgs != 1) {
        return wrongNArgsErr(errMsg);
    }

    if (argList[0].tag == ARRAY_TAG) {
        array = &argList[0];
        arraySize = ArraySize(array);

        if (arraySize > N_RANGESETS) {
            M_FAILURE("Too many elements in array in %s");
        }

        for (i = 0; i < arraySize; i++) {
            sprintf(keyString, "%d", i);

            if (!ArrayGet(array, keyString, &element)) {
                M_FAILURE("Invalid key in array in %s");
            }

            if (!readArgument(element, &label, errMsg) || !RangesetTable::RangesetLabelOK(label)) {
                M_FAILURE("Invalid rangeset label in array in %s");
            }

            deleteLabels[i] = label;
        }

        for (i = 0; i < arraySize; i++) {
            rangesetTable->RangesetForget(deleteLabels[i]);
        }
    } else {
        if (!readArgument(argList[0], &label, errMsg) || !RangesetTable::RangesetLabelOK(label)) {
            M_FAILURE("Invalid rangeset label in %s");
        }

        if (rangesetTable) {
            rangesetTable->RangesetForget(label);
        }
    }

    // set up result
    result->tag = NO_TAG;
    return true;
}

/*
** Built-in macro subroutine for getting all range sets with a specfic name.
** Arguments are $1: range set name.
** return value is an array indexed 0 to n, with the rangeset labels as values;
*/
static int rangesetGetByNameMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    Rangeset *rangeset;
    int label;
    std::string name;
    RangesetTable *rangesetTable = window->buffer_->rangesetTable_;
    uint8_t *rangesetList;
    int insertIndex = 0;
    DataValue element;

    if(!readArguments(argList, nArgs, 0, errMsg, &name)) {
        M_FAILURE("First parameter is not a name string in %s");
    }

    result->tag = ARRAY_TAG;
    result->val.arrayPtr = ArrayNew();

    if(!rangesetTable) {
        return true;
    }

    rangesetList = RangesetTable::RangesetGetList(rangesetTable);
    int nRangesets = strlen((char *)rangesetList);
    for (int i = 0; i < nRangesets; ++i) {
        label = rangesetList[i];
        rangeset = rangesetTable->RangesetFetch(label);
        if (rangeset) {
            QString rangeset_name = rangeset->RangesetGetName();

            if(rangeset_name == QString::fromStdString(name)) {

                element.tag = INT_TAG;
                element.val.n = label;

                if (!ArrayInsert(result, AllocStringCpyEx(std::to_string(insertIndex)), &element)) {
                    M_FAILURE("Failed to insert array element in %s");
                }

                ++insertIndex;
            }
        }
    }

    return true;
}

/*
** Built-in macro subroutine for adding to a range set. Arguments are $1: range
** set label (one integer), then either (a) $2: source range set label,
** (b) $2: int start-range, $3: int end-range, (c) nothing (use selection
** if any to specify range to add - must not be rectangular). Returns the
** index of the newly added range (cases b and c), or 0 (case a).
*/
static int rangesetAddMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    TextBuffer *buffer = window->buffer_;
    RangesetTable *rangesetTable = buffer->rangesetTable_;
    Rangeset *sourceRangeset;
    int start, end, rectStart, rectEnd, maxpos, index;
    bool isRect;
    int label = 0;

    if (nArgs < 1 || nArgs > 3)
        return wrongNArgsErr(errMsg);

    if (!readArgument(argList[0], &label, errMsg) || !RangesetTable::RangesetLabelOK(label)) {
        M_FAILURE("First parameter is an invalid rangeset label in %s");
    }

    if(!rangesetTable) {
        M_FAILURE("Rangeset does not exist in %s");
    }

    Rangeset *targetRangeset = rangesetTable->RangesetFetch(label);

    if(!targetRangeset) {
        M_FAILURE("Rangeset does not exist in %s");
    }

    start = end = -1;

    if (nArgs == 1) {
        // pick up current selection in this window
        if (!buffer->BufGetSelectionPos(&start, &end, &isRect, &rectStart, &rectEnd) || isRect) {
            M_FAILURE("Selection missing or rectangular in call to %s");
        }
        if (!targetRangeset->RangesetAddBetween(start, end)) {
            M_FAILURE("Failure to add selection in %s");
        }
    }

    if (nArgs == 2) {
        // add ranges taken from a second set
        if (!readArgument(argList[1], &label, errMsg) || !RangesetTable::RangesetLabelOK(label)) {
            M_FAILURE("Second parameter is an invalid rangeset label in %s");
        }

        sourceRangeset = rangesetTable->RangesetFetch(label);
        if(!sourceRangeset) {
            M_FAILURE("Second rangeset does not exist in %s");
        }

        targetRangeset->RangesetAdd(sourceRangeset);
    }

    if (nArgs == 3) {
        // add a range bounded by the start and end positions in $2, $3
        if (!readArgument(argList[1], &start, errMsg)) {
            return false;
        }
        if (!readArgument(argList[2], &end, errMsg)) {
            return false;
        }

        // make sure range is in order and fits buffer size
        maxpos = buffer->BufGetLength();
        if (start < 0)
            start = 0;
        if (start > maxpos)
            start = maxpos;
        if (end < 0)
            end = 0;
        if (end > maxpos)
            end = maxpos;
        if (start > end) {
            qSwap(start, end);
        }

        if ((start != end) && !targetRangeset->RangesetAddBetween(start, end)) {
            M_FAILURE("Failed to add range in %s");
        }
    }

    // (to) which range did we just add?
    if (nArgs != 2 && start >= 0) {
        start = (start + end) / 2; // "middle" of added range
        index = 1 + targetRangeset->RangesetFindRangeOfPos(start, false);
    } else {
        index = 0;
    }

    // set up result
    result->tag = INT_TAG;
    result->val.n = index;
    return true;
}

/*
** Built-in macro subroutine for removing from a range set. Almost identical to
** rangesetAddMS() - only changes are from RangesetAdd()/RangesetAddBetween()
** to RangesetSubtract()/RangesetSubtractBetween(), the handling of an
** undefined destination range, and that it returns no value.
*/
static int rangesetSubtractMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    TextBuffer *buffer = window->buffer_;
    RangesetTable *rangesetTable = buffer->rangesetTable_;
    Rangeset *targetRangeset, *sourceRangeset;
    int start, end, rectStart, rectEnd, maxpos;
    bool isRect;
    int label = 0;

    if (nArgs < 1 || nArgs > 3) {
        return wrongNArgsErr(errMsg);
    }

    if (!readArgument(argList[0], &label, errMsg) || !RangesetTable::RangesetLabelOK(label)) {
        M_FAILURE("First parameter is an invalid rangeset label in %s");
    }

    if(!rangesetTable) {
        M_FAILURE("Rangeset does not exist in %s");
    }

    targetRangeset = rangesetTable->RangesetFetch(label);
    if(!targetRangeset) {
        M_FAILURE("Rangeset does not exist in %s");
    }

    if (nArgs == 1) {
        // remove current selection in this window
        if (!buffer->BufGetSelectionPos(&start, &end, &isRect, &rectStart, &rectEnd) || isRect) {
            M_FAILURE("Selection missing or rectangular in call to %s");
        }
        targetRangeset->RangesetRemoveBetween(start, end);
    }

    if (nArgs == 2) {
        // remove ranges taken from a second set
        if (!readArgument(argList[1], &label, errMsg) || !RangesetTable::RangesetLabelOK(label)) {
            M_FAILURE("Second parameter is an invalid rangeset label in %s");
        }

        sourceRangeset = rangesetTable->RangesetFetch(label);
        if(!sourceRangeset) {
            M_FAILURE("Second rangeset does not exist in %s");
        }
        targetRangeset->RangesetRemove(sourceRangeset);
    }

    if (nArgs == 3) {
        // remove a range bounded by the start and end positions in $2, $3
        if (!readArgument(argList[1], &start, errMsg))
            return false;
        if (!readArgument(argList[2], &end, errMsg))
            return false;

        // make sure range is in order and fits buffer size
        maxpos = buffer->BufGetLength();
        if (start < 0)
            start = 0;
        if (start > maxpos)
            start = maxpos;
        if (end < 0)
            end = 0;
        if (end > maxpos)
            end = maxpos;
        if (start > end) {
            qSwap(start, end);
        }

        targetRangeset->RangesetRemoveBetween(start, end);
    }

    // set up result
    result->tag = NO_TAG;
    return true;
}

/*
** Built-in macro subroutine to invert a range set. Argument is $1: range set
** label (one alphabetic character). Returns nothing. Fails if range set
** undefined.
*/
static int rangesetInvertMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    RangesetTable *rangesetTable = window->buffer_->rangesetTable_;
    int label;

    if(!readArguments(argList, nArgs, 0, errMsg, &label)) {
        return false;
    }

    if (!RangesetTable::RangesetLabelOK(label)) {
        M_FAILURE("First parameter is an invalid rangeset label in %s");
    }

    if(!rangesetTable) {
        M_FAILURE("Rangeset does not exist in %s");
    }

    Rangeset *rangeset = rangesetTable->RangesetFetch(label);
    if(!rangeset) {
        M_FAILURE("Rangeset does not exist in %s");
    }

    if (rangeset->RangesetInverse() < 0) {
        M_FAILURE("Problem inverting rangeset in %s");
    }

    // set up result
    result->tag = NO_TAG;
    return true;
}

/*
** Built-in macro subroutine for finding out info about a rangeset.  Takes one
** argument of a rangeset label.  Returns an array with the following keys:
**    defined, count, color, mode.
*/
static int rangesetInfoMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    RangesetTable *rangesetTable = window->buffer_->rangesetTable_;
    Rangeset *rangeset = nullptr;
    DataValue element;
    int label;

    if(!readArguments(argList, nArgs, 0, errMsg, &label)) {
        return false;
    }

    if (!RangesetTable::RangesetLabelOK(label)) {
        M_FAILURE("First parameter is an invalid rangeset label in %s");
    }

    if (rangesetTable) {
        rangeset = rangesetTable->RangesetFetch(label);
    }

    int count;
    bool defined;
    QString color;
    QString name;
    const char *mode;

    if(rangeset) {
        rangeset->RangesetGetInfo(&defined, &label, &count, &color, &name, &mode);
    } else {
        defined = false;
        label = 0;
        count = 0;
        mode  = "";
    }

    // set up result
    result->tag = ARRAY_TAG;
    result->val.arrayPtr = ArrayNew();

    element.tag = INT_TAG;
    element.val.n = defined;
    if (!ArrayInsert(result, PERM_ALLOC_STR("defined"), &element))
        M_FAILURE("Failed to insert array element \"defined\" in %s");

    element.tag = INT_TAG;
    element.val.n = count;
    if (!ArrayInsert(result, PERM_ALLOC_STR("count"), &element))
        M_FAILURE("Failed to insert array element \"count\" in %s");

    element.tag = STRING_TAG;
    element.val.str = AllocNStringCpyEx(color);

    if (!ArrayInsert(result, PERM_ALLOC_STR("color"), &element))
        M_FAILURE("Failed to insert array element \"color\" in %s");

    element.tag = STRING_TAG;
    element.val.str = AllocNStringCpyEx(name);

    if (!ArrayInsert(result, PERM_ALLOC_STR("name"), &element)) {
        M_FAILURE("Failed to insert array element \"name\" in %s");
    }

    element.tag = STRING_TAG;
    if (!AllocNStringCpy(&element.val.str, mode))
        M_FAILURE("Failed to allocate array value \"mode\" in %s");
    if (!ArrayInsert(result, PERM_ALLOC_STR("mode"), &element))
        M_FAILURE("Failed to insert array element \"mode\" in %s");

    return true;
}

/*
** Built-in macro subroutine for finding the extent of a range in a set.
** If only one parameter is supplied, use the spanning range of all
** ranges, otherwise select the individual range specified.  Returns
** an array with the keys "start" and "end" and values
*/
static int rangesetRangeMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    TextBuffer *buffer = window->buffer_;
    RangesetTable *rangesetTable = buffer->rangesetTable_;
    Rangeset *rangeset;
    int start, end, dummy, rangeIndex, ok;
    DataValue element;
    int label = 0;

    if (nArgs < 1 || nArgs > 2) {
        return wrongNArgsErr(errMsg);
    }

    if (!readArgument(argList[0], &label, errMsg) || !RangesetTable::RangesetLabelOK(label)) {
        M_FAILURE("First parameter is an invalid rangeset label in %s");
    }

    if(!rangesetTable) {
        M_FAILURE("Rangeset does not exist in %s");
    }

    ok = false;
    rangeset = rangesetTable->RangesetFetch(label);
    if (rangeset) {
        if (nArgs == 1) {
            rangeIndex = rangeset->RangesetGetNRanges() - 1;
            ok  = rangeset->RangesetFindRangeNo(0, &start, &dummy);
            ok &= rangeset->RangesetFindRangeNo(rangeIndex, &dummy, &end);
            rangeIndex = -1;
        } else if (nArgs == 2) {
            if (!readArgument(argList[1], &rangeIndex, errMsg)) {
                return false;
            }
            ok = rangeset->RangesetFindRangeNo(rangeIndex - 1, &start, &end);
        }
    }

    // set up result
    result->tag = ARRAY_TAG;
    result->val.arrayPtr = ArrayNew();

    if (!ok)
        return true;

    element.tag = INT_TAG;
    element.val.n = start;
    if (!ArrayInsert(result, PERM_ALLOC_STR("start"), &element))
        M_FAILURE("Failed to insert array element \"start\" in %s");

    element.tag = INT_TAG;
    element.val.n = end;
    if (!ArrayInsert(result, PERM_ALLOC_STR("end"), &element))
        M_FAILURE("Failed to insert array element \"end\" in %s");

    return true;
}

/*
** Built-in macro subroutine for checking a position against a range. If only
** one parameter is supplied, the current cursor position is used. Returns
** false (zero) if not in a range, range index (1-based) if in a range;
** fails if parameters were bad.
*/
static int rangesetIncludesPosMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    TextBuffer *buffer = document->buffer_;
    RangesetTable *rangesetTable = buffer->rangesetTable_;
    int rangeIndex, maxpos;
    int label = 0;

    if (nArgs < 1 || nArgs > 2) {
        return wrongNArgsErr(errMsg);
    }

    if (!readArgument(argList[0], &label, errMsg) || !RangesetTable::RangesetLabelOK(label)) {
        M_FAILURE("First parameter is an invalid rangeset label in %s");
    }

    if(!rangesetTable) {
        M_FAILURE("Rangeset does not exist in %s");
    }

    Rangeset *rangeset = rangesetTable->RangesetFetch(label);
    if(!rangeset) {
        M_FAILURE("Rangeset does not exist in %s");
    }

    int pos = 0;
    if (nArgs == 1) {
        auto textD = document->toWindow()->lastFocus_;
        pos = textD->TextGetCursorPos();
    } else if (nArgs == 2) {
        if (!readArgument(argList[1], &pos, errMsg))
            return false;
    }

    maxpos = buffer->BufGetLength();
    if (pos < 0 || pos > maxpos) {
        rangeIndex = 0;
    } else {
        rangeIndex = rangeset->RangesetFindRangeOfPos(pos, false) + 1;
    }

    // set up result
    result->tag = INT_TAG;
    result->val.n = rangeIndex;
    return true;
}

/*
** Set the color of a range set's ranges. it is ignored if the color cannot be
** found/applied. If no color is applied, any current color is removed. Returns
** true if the rangeset is valid.
*/
static int rangesetSetColorMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    TextBuffer *buffer = window->buffer_;
    RangesetTable *rangesetTable = buffer->rangesetTable_;
    int label = 0;

    if (nArgs != 2) {
        return wrongNArgsErr(errMsg);
    }

    if (!readArgument(argList[0], &label, errMsg) || !RangesetTable::RangesetLabelOK(label)) {
        M_FAILURE("First parameter is an invalid rangeset label in %s");
    }

    if(!rangesetTable) {
        M_FAILURE("Rangeset does not exist in %s");
    }

    Rangeset *rangeset = rangesetTable->RangesetFetch(label);
    if(!rangeset) {
        M_FAILURE("Rangeset does not exist in %s");
    }

    std::string color_name;
    if (!readArgument(argList[1], &color_name, errMsg)) {
        M_FAILURE("Second parameter is not a color name string in %s");
    }

    rangeset->RangesetAssignColorName(color_name);

    // set up result
    result->tag = NO_TAG;
    return true;
}

/*
** Set the name of a range set's ranges. Returns
** true if the rangeset is valid.
*/
static int rangesetSetNameMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    TextBuffer *buffer = window->buffer_;
    RangesetTable *rangesetTable = buffer->rangesetTable_;
    int label = 0;

    if (nArgs != 2) {
        return wrongNArgsErr(errMsg);
    }

    if (!readArgument(argList[0], &label, errMsg) || !RangesetTable::RangesetLabelOK(label)) {
        M_FAILURE("First parameter is an invalid rangeset label in %s");
    }

    if(!rangesetTable) {
        M_FAILURE("Rangeset does not exist in %s");
    }

    Rangeset *rangeset = rangesetTable->RangesetFetch(label);
    if(!rangeset) {
        M_FAILURE("Rangeset does not exist in %s");
    }

    std::string name;
    if (!readArgument(argList[1], &name, errMsg)) {
        M_FAILURE("Second parameter is not a valid name string in %s");
    }

    rangeset->RangesetAssignName(name);

    // set up result
    result->tag = NO_TAG;
    return true;
}

/*
** Change a range's modification response. Returns true if the rangeset is
** valid and the response type name is valid.
*/
static int rangesetSetModeMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    TextBuffer *buffer = window->buffer_;
    RangesetTable *rangesetTable = buffer->rangesetTable_;
    Rangeset *rangeset;
    int label = 0;

    if (nArgs < 1 || nArgs > 2) {
        return wrongNArgsErr(errMsg);
    }

    if (!readArgument(argList[0], &label, errMsg) || !RangesetTable::RangesetLabelOK(label)) {
        M_FAILURE("First parameter is an invalid rangeset label in %s");
    }

    if(!rangesetTable) {
        M_FAILURE("Rangeset does not exist in %s");
    }

    rangeset = rangesetTable->RangesetFetch(label);
    if(!rangeset) {
        M_FAILURE("Rangeset does not exist in %s");
    }

    std::string update_fn_name;
    if (nArgs == 2) {
        if (!readArgument(argList[1], &update_fn_name,  errMsg)) {
            M_FAILURE("Second parameter is not a string in %s");
        }
    }

    int ok = rangeset->RangesetChangeModifyResponse(update_fn_name.c_str());
    if (!ok) {
        M_FAILURE("Second parameter is not a valid mode in %s");
    }

    // set up result
    result->tag = NO_TAG;
    return true;
}

// --------------------------------------------------------------------------

/*
** Routines to get details directly from the window.
*/

/*
** Sets up an array containing information about a style given its name or
** a buffer position (bufferPos >= 0) and its highlighting pattern code
** (patCode >= 0).
** From the name we obtain:
**      ["color"]       Foreground color name of style
**      ["background"]  Background color name of style if specified
**      ["bold"]        '1' if style is bold, '0' otherwise
**      ["italic"]      '1' if style is italic, '0' otherwise
** Given position and pattern code we obtain:
**      ["rgb"]         RGB representation of foreground color of style
**      ["back_rgb"]    RGB representation of background color of style
**      ["extent"]      Forward distance from position over which style applies
** We only supply the style name if the includeName parameter is set:
**      ["style"]       Name of style
**
*/
static int fillStyleResultEx(DataValue *result, const char **errMsg, DocumentWidget *document, const char *styleName, bool includeName, int patCode, int bufferPos) {
    DataValue DV;

    // initialize array
    result->tag = ARRAY_TAG;
    result->val.arrayPtr = ArrayNew();

    // the following array entries will be strings
    DV.tag = STRING_TAG;

    auto styleNameStr = QString::fromLatin1(styleName);

    if (includeName) {

        // insert style name
        DV.val.str = AllocNStringCpyEx(styleNameStr);

        if (!ArrayInsert(result, PERM_ALLOC_STR("style"), &DV)) {
            M_ARRAY_INSERT_FAILURE();
        }
    }

    // insert color name
    DV.val.str = AllocNStringCpyEx(ColorOfNamedStyleEx(styleNameStr));
    if (!ArrayInsert(result, PERM_ALLOC_STR("color"), &DV)) {
        M_ARRAY_INSERT_FAILURE();
    }

    /* Prepare array element for color value
       (only possible if we pass through the dynamic highlight pattern tables
       in other words, only if we have a pattern code) */
    if (patCode) {
        QColor color = HighlightColorValueOfCodeEx(document, patCode);
        DV.val.str = AllocNStringCpyEx(color.name());

        if (!ArrayInsert(result, PERM_ALLOC_STR("rgb"), &DV)) {
            M_ARRAY_INSERT_FAILURE();
        }
    }

    // Prepare array element for background color name
    DV.val.str = AllocNStringCpyEx(BgColorOfNamedStyleEx(QString::fromLatin1(styleName)));
    if (!ArrayInsert(result, PERM_ALLOC_STR("background"), &DV)) {
        M_ARRAY_INSERT_FAILURE();
    }

    /* Prepare array element for background color value
       (only possible if we pass through the dynamic highlight pattern tables
       in other words, only if we have a pattern code) */
    if (patCode) {
        QColor color = GetHighlightBGColorOfCodeEx(document, patCode);
        DV.val.str = AllocNStringCpyEx(color.name());

        if (!ArrayInsert(result, PERM_ALLOC_STR("back_rgb"), &DV)) {
            M_ARRAY_INSERT_FAILURE();
        }
    }

    // the following array entries will be integers
    DV.tag = INT_TAG;

    // Put boldness value in array
    DV.val.n = FontOfNamedStyleIsBold(QString::fromLatin1(styleName));
    if (!ArrayInsert(result, PERM_ALLOC_STR("bold"), &DV)) {
        M_ARRAY_INSERT_FAILURE();
    }

    // Put italicity value in array
    DV.val.n = FontOfNamedStyleIsItalic(QString::fromLatin1(styleName));
    if (!ArrayInsert(result, PERM_ALLOC_STR("italic"), &DV)) {
        M_ARRAY_INSERT_FAILURE();
    }

    if (bufferPos >= 0) {
        // insert extent
        DV.val.n = StyleLengthOfCodeFromPosEx(document, bufferPos);
        if (!ArrayInsert(result, PERM_ALLOC_STR("extent"), &DV)) {
            M_ARRAY_INSERT_FAILURE();
        }
    }
    return true;
}

/*
** Returns an array containing information about the style of name $1
**      ["color"]       Foreground color name of style
**      ["background"]  Background color name of style if specified
**      ["bold"]        '1' if style is bold, '0' otherwise
**      ["italic"]      '1' if style is italic, '0' otherwise
**
*/
static int getStyleByNameMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    std::string styleName;

    // Validate number of arguments
    if(!readArguments(argList, nArgs, 0, errMsg, &styleName)) {
        M_FAILURE("First parameter is not a string in %s");
    }

    // Prepare result
    result->tag = ARRAY_TAG;
    result->val.arrayPtr = nullptr;

    if (!NamedStyleExists(QString::fromStdString(styleName))) {
        // if the given name is invalid we just return an empty array.
        return true;
    }

    return fillStyleResultEx(
                result,
                errMsg,
                window,
                styleName.c_str(),
                false,
                0,
                -1);
}

/*
** Returns an array containing information about the style of position $1
**      ["style"]       Name of style
**      ["color"]       Foreground color name of style
**      ["background"]  Background color name of style if specified
**      ["bold"]        '1' if style is bold, '0' otherwise
**      ["italic"]      '1' if style is italic, '0' otherwise
**      ["rgb"]         RGB representation of foreground color of style
**      ["back_rgb"]    RGB representation of background color of style
**      ["extent"]      Forward distance from position over which style applies
**
*/
static int getStyleAtPosMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    int patCode;
    int bufferPos;
    TextBuffer *buf = window->buffer_;

    // Validate number of arguments
    if(!readArguments(argList, nArgs, 0, errMsg, &bufferPos)) {
        return false;
    }

    // Prepare result
    result->tag = ARRAY_TAG;
    result->val.arrayPtr = nullptr;

    //  Verify sane buffer position
    if ((bufferPos < 0) || (bufferPos >= buf->BufGetLength())) {
        /*  If the position is not legal, we cannot guess anything about
            the style, so we return an empty array. */
        return true;
    }

    // Determine pattern code
    patCode = HighlightCodeOfPosEx(window, bufferPos);
    if (patCode == 0) {
        // if there is no pattern we just return an empty array.
        return true;
    }

    return fillStyleResultEx(
        result,
        errMsg,
        window,
        HighlightStyleOfCodeEx(window, patCode).toLatin1().data(),
        true,
        patCode,
        bufferPos);
}

/*
** Sets up an array containing information about a pattern given its name or
** a buffer position (bufferPos >= 0).
** From the name we obtain:
**      ["style"]       Name of style
**      ["extent"]      Forward distance from position over which style applies
** We only supply the pattern name if the includeName parameter is set:
**      ["pattern"]     Name of pattern
**
*/
static int fillPatternResultEx(DataValue *result, const char **errMsg, DocumentWidget *window, const char *patternName, bool includeName, char *styleName, int bufferPos) {

    Q_UNUSED(errMsg);

    DataValue DV;

    // initialize array
    result->tag = ARRAY_TAG;
    result->val.arrayPtr = ArrayNew();

    // the following array entries will be strings
    DV.tag = STRING_TAG;

    if (includeName) {
        // insert pattern name
        DV.val.str = AllocNStringCpyEx(QString::fromLatin1(patternName));

        if (!ArrayInsert(result, PERM_ALLOC_STR("pattern"), &DV)) {
            M_ARRAY_INSERT_FAILURE();
        }
    }

    // insert style name
    DV.val.str = AllocNStringCpyEx(QString::fromLatin1(styleName));

    if (!ArrayInsert(result, PERM_ALLOC_STR("style"), &DV)) {
        M_ARRAY_INSERT_FAILURE();
    }

    // the following array entries will be integers
    DV.tag = INT_TAG;

    if (bufferPos >= 0) {
        // insert extent
        int checkCode = 0;
        DV.val.n = HighlightLengthOfCodeFromPosEx(window, bufferPos, &checkCode);
        if (!ArrayInsert(result, PERM_ALLOC_STR("extent"), &DV)) {
            M_ARRAY_INSERT_FAILURE();
        }
    }

    return true;
}

/*
** Returns an array containing information about a highlighting pattern. The
** single parameter contains the pattern name for which this information is
** requested.
** The returned array looks like this:
**      ["style"]       Name of style
*/
static int getPatternByNameMS(DocumentWidget *document, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {

    std::string patternName;

    // Begin of building the result.
    result->tag = ARRAY_TAG;
    result->val.arrayPtr = nullptr;

    // Validate number of arguments
    if(!readArguments(argList, nArgs, 0, errMsg, &patternName)) {
        M_FAILURE("First parameter is not a string in %s");
    }

    HighlightPattern *pattern = FindPatternOfWindowEx(document, QString::fromStdString(patternName));
    if(!pattern) {
        // The pattern's name is unknown.
        return true;
    }

    return fillPatternResultEx(
                result,
                errMsg,
                document,
                patternName.c_str(),
                false,
                pattern->style.toLatin1().data(),
                -1);
}

/*
** Returns an array containing information about the highlighting pattern
** applied at a given position, passed as the only parameter.
** The returned array looks like this:
**      ["pattern"]     Name of pattern
**      ["style"]       Name of style
**      ["extent"]      Distance from position over which this pattern applies
*/
static int getPatternAtPosMS(DocumentWidget *window, DataValue *argList, int nArgs, DataValue *result, const char **errMsg) {
    int bufferPos;
    TextBuffer *buffer = window->buffer_;

    // Begin of building the result.
    result->tag = ARRAY_TAG;
    result->val.arrayPtr = nullptr;

    // Validate number of arguments
    if(!readArguments(argList, nArgs, 0, errMsg, &bufferPos)) {
        return false;
    }

    /* The most straightforward case: Get a pattern, style and extent
       for a buffer position. */

    /*  Verify sane buffer position
     *  You would expect that buffer->length would be among the sane
     *  positions, but we have n characters and n+1 buffer positions. */
    if ((bufferPos < 0) || (bufferPos >= buffer->BufGetLength())) {
        /*  If the position is not legal, we cannot guess anything about
            the highlighting pattern, so we return an empty array. */
        return true;
    }

    // Determine the highlighting pattern used
    int patCode = HighlightCodeOfPosEx(window, bufferPos);
    if (patCode == 0) {
        // if there is no highlighting pattern we just return an empty array.
        return true;
    }

    return fillPatternResultEx(
        result,
        errMsg,
        window,
        HighlightNameOfCodeEx(window, patCode).toLatin1().data(),
        true,
        HighlightStyleOfCodeEx(window, patCode).toLatin1().data(),
        bufferPos);
}

static bool wrongNArgsErr(const char **errMsg) {
    *errMsg = "Wrong number of arguments to function %s";
    return false;
}

static bool tooFewArgsErr(const char **errMsg) {
    *errMsg = "Too few arguments to function %s";
    return false;
}

/*
** Get an integer value from a tagged DataValue structure.  Return True
** if conversion succeeded, and store result in *result, otherwise
** return False with an error message in *errMsg.
*/
static bool readArgument(DataValue dv, int *result, const char **errMsg) {

    if (dv.tag == INT_TAG) {
        *result = dv.val.n;
        return true;
    } else if (dv.tag == STRING_TAG) {

        char *p = dv.val.str.rep;
        char *endp;
        long val = strtol(p, &endp, 10);
        if (endp == p || *endp != '\0' || ((val == LONG_MIN || val == LONG_MAX) && errno == ERANGE)) {
            if(errMsg) {
                *errMsg = "%s called with non-integer argument";
            }
            return false;
        }

        *result = static_cast<int>(val);
        return true;
    }

    if(errMsg) {
        *errMsg = "%s called with unknown object";
    }
    return false;
}

/*
** Get an string value from a tagged DataValue structure.  Return True
** if conversion succeeded, and store result in *result, otherwise
** return false with an error message in *errMsg.  If an integer value
** is converted, write the string in the space provided by "stringStorage",
** which must be large enough to handle ints of the maximum size.
*/
static bool readArgument(DataValue dv, std::string *result, const char **errMsg) {

    switch(dv.tag) {
    case STRING_TAG:
        *result = dv.val.str.rep;
        return true;
    case INT_TAG:
        *result = std::to_string(dv.val.n);
        return true;
    default:
        if(errMsg) {
            *errMsg = "%s called with unknown object";
        }
        return false;
    }
}

static bool readArgument(DataValue dv, QString *result, const char **errMsg) {

    switch(dv.tag) {
    case STRING_TAG:
        *result = QString::fromLatin1(dv.val.str.rep, dv.val.str.len);
        return true;
    case INT_TAG:
        *result = QString::number(dv.val.n);
        return true;
    default:
        if(errMsg) {
            *errMsg = "%s called with unknown object";
        }
        return false;
    }
}


template <class T, class ...Ts>
bool readArguments(DataValue *argList, int nArgs, int index, const char **errMsg, T arg, Ts...args) {

    static_assert(std::is_pointer<T>::value, "Argument is not a pointer");

    if((nArgs - index) != (sizeof...(args)) + 1) {
        return wrongNArgsErr(errMsg);
    }

    bool ret = readArgument(argList[index], arg, errMsg);
    if(!ret) {
        return false;
    }

    return readArguments(argList, nArgs, index + 1, errMsg, args...);
}

template <class T>
bool readArguments(DataValue *argList, int nArgs, int index, const char **errMsg, T arg) {

    static_assert(std::is_pointer<T>::value, "Argument is not a pointer");

    if((nArgs - index) != 1) {
        return wrongNArgsErr(errMsg);
    }

    return readArgument(argList[index], arg, errMsg);
}
