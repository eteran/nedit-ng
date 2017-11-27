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
#include "CommandRecorder.h"
#include "DialogPrompt.h"
#include "DialogPromptList.h"
#include "DialogPromptString.h"
#include "Direction.h"
#include "DocumentWidget.h"
#include "HighlightPattern.h"
#include "Input.h"
#include "MainWindow.h"
#include "RangesetTable.h"
#include "SearchType.h"
#include "SignalBlocker.h"
#include "TextArea.h"
#include "TextBuffer.h"
#include "WrapMode.h"
#include "highlight.h"
#include "highlightData.h"
#include "interpret.h"
#include "parse.h"
#include "preferences.h"
#include "search.h"
#include "smartIndent.h"
#include "util/fileUtils.h"
#include "util/utils.h"
#include "version.h"

#include <gsl/gsl_util>
#include <boost/optional.hpp>
#include <stack>
#include <fstream>

#include <QClipboard>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QMimeData>

#include <sys/param.h>

// The following definitions cause an exit from the macro with a message
// added if (1) to remove compiler warnings on solaris
#define M_FAILURE(s)     \
    do {                 \
        if(errMsg) {     \
            *errMsg = s; \
        }                \
        return false;    \
    } while (0)

static bool lengthMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool minMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool maxMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool focusWindowMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool getRangeMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool getCharacterMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool replaceRangeMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool replaceSelectionMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool getSelectionMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool validNumberMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool replaceInStringMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool replaceSubstringMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool readFileMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool writeFileMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool appendFileMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool writeOrAppendFile(bool append, DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool substringMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool toupperMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool tolowerMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool stringToClipboardMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool clipboardToStringMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool searchMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool searchStringMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool setCursorPosMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool beepMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool selectMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool selectRectangleMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool tPrintMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool getenvMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool shellCmdMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool dialogMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool stringDialogMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool calltipMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool killCalltipMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);

// T Balinski
static bool listDialogMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
// T Balinski End

static bool stringCompareMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool splitMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
#if 0 // DISASBLED for 5.4
static bool setBacklightStringMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
#endif
static bool cursorMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool lineMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool columnMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool fileNameMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool filePathMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool lengthMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool selectionStartMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool selectionEndMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool selectionLeftMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool selectionRightMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool statisticsLineMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool incSearchLineMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool showLineNumbersMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool autoIndentMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool wrapTextMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool highlightSyntaxMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool makeBackupCopyMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool incBackupMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool showMatchingMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool matchSyntaxBasedMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool overTypeModeMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool readOnlyMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool lockedMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool fileFormatMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool fontNameMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool fontNameItalicMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool fontNameBoldMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool fontNameBoldItalicMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool subscriptSepMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool minFontWidthMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool maxFontWidthMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool wrapMarginMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool topLineMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool numDisplayLinesMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool displayWidthMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool activePaneMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool nPanesMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool emptyArrayMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool serverNameMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool tabDistMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool emTabDistMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool useTabsMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool modifiedMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool languageModeMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool calltipIDMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool rangesetListMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool versionMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool rangesetCreateMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool rangesetDestroyMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool rangesetGetByNameMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool rangesetAddMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool rangesetSubtractMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool rangesetInvertMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool rangesetInfoMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool rangesetRangeMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool rangesetIncludesPosMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool rangesetSetColorMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool rangesetSetNameMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool rangesetSetModeMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool fillPatternResultEx(DataValue *result, const char **errMsg, DocumentWidget *document, const QString &patternName, bool includeName, const QString &styleName, int bufferPos);
static bool getPatternByNameMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool getPatternAtPosMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool fillStyleResultEx(DataValue *result, const char **errMsg, DocumentWidget *document, const QString &styleName, bool includeName, int patCode, int bufferPos);
static bool getStyleByNameMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool getStyleAtPosMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool filenameDialogMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);

// MainWindow scoped functions
static bool replaceAllInSelectionMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);
static bool replaceAllMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg);

static bool readSearchArgs(Arguments arguments, Direction *searchDirection, SearchType *searchType, WrapMode *wrap, const char **errMsg);

static bool readArgument(const DataValue &dv, int *result, const char **errMsg = nullptr);
static bool readArgument(const DataValue &dv, std::string *result, const char **errMsg = nullptr);
static bool readArgument(const DataValue &dv, QString *result, const char **errMsg = nullptr);

template <class T, class ...Ts>
bool readArguments(Arguments arguments, int index, const char **errMsg, T arg, Ts...args);

template <class T>
bool readArguments(Arguments arguments, int index, const char **errMsg, T arg);

struct SubRoutine {
    const char   *name;
    BuiltInSubrEx function;
};

namespace {

const char ArrayFull[]                   = "Too many elements in array in %s";
const char InvalidMode[]                 = "Invalid value for mode in %s";
const char InvalidArgument[]             = "%s called with invalid argument";
const char TooFewArguments[]             = "%s subroutine called with too few arguments";
const char TooManyArguments[]            = "%s subroutine called with too many arguments";
const char UnknownObject[]               = "%s called with unknown object";
const char NotAnInteger[]                = "%s called with non-integer argument";
const char NotAString[]                  = "%s not called with a string parameter";
const char InvalidContext[]              = "%s can't be called from non-suspendable context";
const char NeedsArguments[]              = "%s subroutine called with no arguments";
const char WrongNumberOfArguments[]      = "Wrong number of arguments to function %s";
const char UnrecognizedArgument[]        = "Unrecognized argument to %s";
const char InsertFailed[]                = "Array element failed to insert: %s";
const char RangesetDoesNotExist[]        = "Rangeset does not exist in %s";
const char Rangeset2DoesNotExist[]       = "Second rangeset does not exist in %s";
const char PathnameTooLong[]             = "Pathname too long in %s";
const char InvalidArrayKey[]             = "Invalid key in array in %s";
const char InvalidIndentStyle[]          = "Invalid indent style value encountered in %s";
const char InvalidWrapStyle[]            = "Invalid wrap style value encountered in %s";
const char EmptyList[]                   = "%s subroutine called with empty list data";
const char Param1InvalidRangesetLabel[]  = "First parameter is an invalid rangeset label in %s";
const char Param2InvalidRangesetLabel[]  = "Second parameter is an invalid rangeset label in %s";
const char InvalidRangesetLabel[]        = "Invalid rangeset label in %s";
const char InvalidRangesetLabelInArray[] = "Invalid rangeset label in array in %s";
const char Param1NotAString[]            = "First parameter is not a string in %s";
const char Param2NotAString[]            = "Second parameter is not a string in %s";
const char SelectionMissing[]            = "Selection missing or rectangular in call to %s";
const char FailedToAddRange[]            = "Failed to add range in %s";
const char FailedToInvertRangeset[]      = "Problem inverting rangeset in %s";
const char FailedToAddSelection[]        = "Failure to add selection in %s";
const char Param2CannotBeEmptyString[]   = "Second argument must be a non-empty string: %s";
const char InvalidSearchReplaceArgs[]    = "%s action requires search and replace string arguments";

}

/**
 * @brief flagsFromArguments
 * @param argList
 * @param nArgs
 * @param firstFlag
 * @param flags
 * @return true if all arguments were valid flags, false otherwise
 */
bool flagsFromArguments(Arguments arguments, int firstFlag, TextArea::EventFlags *flags) {
    TextArea::EventFlags f = TextArea::NoneFlag;
    for(int i = firstFlag; i < arguments.size(); ++i) {
        if(to_string(arguments[i]) == "absolute") {
            f |= TextArea::AbsoluteFlag;
        } else if(to_string(arguments[i]) == "column") {
            f |= TextArea::ColumnFlag;
        } else if(to_string(arguments[i]) == "copy") {
            f |= TextArea::CopyFlag;
        } else if(to_string(arguments[i]) == "down") {
            f |= TextArea::DownFlag;
        } else if(to_string(arguments[i]) == "extend") {
            f |= TextArea::ExtendFlag;
        } else if(to_string(arguments[i]) == "left") {
            f |= TextArea::LeftFlag;
        } else if(to_string(arguments[i]) == "overlay") {
            f |= TextArea::OverlayFlag;
        } else if(to_string(arguments[i]) == "rect") {
            f |= TextArea::RectFlag;
        } else if(to_string(arguments[i]) == "right") {
            f |= TextArea::RightFlag;
        } else if(to_string(arguments[i]) == "up") {
            f |= TextArea::UpFlag;
        } else if(to_string(arguments[i]) == "wrap") {
            f |= TextArea::WrapFlag;
        } else if(to_string(arguments[i]) == "tail") {
            f |= TextArea::TailFlag;
        } else if(to_string(arguments[i]) == "stutter") {
            f |= TextArea::StutterFlag;
        } else if(to_string(arguments[i]) == "scrollbar") {
            f |= TextArea::ScrollbarFlag;
        } else if(to_string(arguments[i]) == "nobell") {
            f |= TextArea::NoBellFlag;
        } else {
            return false;
        }
    }

    *flags = f;
    return true;
}


#define TEXT_EVENT(routineName, slotName)                                                                                 \
static bool routineName(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {          \
                                                                                                                          \
    TextArea::EventFlags flags = TextArea::NoneFlag;                                                                      \
    if(!flagsFromArguments(arguments, 0, &flags)) {                                                                       \
        M_FAILURE(InvalidArgument);                                                                                       \
    }                                                                                                                     \
                                                                                                                          \
    if(auto window = MainWindow::fromDocument(document)) {                                                                \
        if(TextArea *area = window->lastFocus()) {                                                                        \
            area->slotName(flags | TextArea::SupressRecording);                                                           \
        }                                                                                                                 \
    }                                                                                                                     \
                                                                                                                          \
    *result = to_value();                                                                                                 \
    return true;                                                                                                          \
}

#define TEXT_EVENT_S(routineName, slotName)                                                                               \
static bool routineName(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {          \
                                                                                                                          \
    if(arguments.size() < 1) {                                                                                            \
        M_FAILURE(WrongNumberOfArguments);                                                                                \
    }                                                                                                                     \
                                                                                                                          \
    QString string;                                                                                                       \
    if(!readArgument(arguments[0], &string, errMsg)) {                                                                    \
        return false;                                                                                                     \
    }                                                                                                                     \
                                                                                                                          \
    TextArea::EventFlags flags = TextArea::NoneFlag;                                                                      \
    if(!flagsFromArguments(arguments, 1, &flags)) {                                                                       \
        M_FAILURE(InvalidArgument);                                                                                       \
    }                                                                                                                     \
                                                                                                                          \
    if(auto window = MainWindow::fromDocument(document)) {                                                                \
        if(TextArea *area = window->lastFocus()) {                                                                        \
            area->slotName(string, flags | TextArea::SupressRecording);                                                   \
        }                                                                                                                 \
    }                                                                                                                     \
                                                                                                                          \
    *result = to_value();                                                                                                 \
    return true;                                                                                                          \
}

#define TEXT_EVENT_I(routineName, slotName)                                                                               \
static bool routineName(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {          \
                                                                                                                          \
    if(arguments.size() < 1) {                                                                                            \
        M_FAILURE(WrongNumberOfArguments);                                                                                \
    }                                                                                                                     \
                                                                                                                          \
    int num;                                                                                                              \
    if(!readArgument(arguments[0], &num, errMsg)) {                                                                       \
        return false;                                                                                                     \
    }                                                                                                                     \
                                                                                                                          \
    TextArea::EventFlags flags = TextArea::NoneFlag;                                                                      \
    if(!flagsFromArguments(arguments, 1, &flags)) {                                                                       \
        M_FAILURE(InvalidArgument);                                                                                       \
    }                                                                                                                     \
                                                                                                                          \
    if(auto window = MainWindow::fromDocument(document)) {                                                                \
        if(TextArea *area = window->lastFocus()) {                                                                        \
            area->slotName(num, flags | TextArea::SupressRecording);                                                      \
        }                                                                                                                 \
    }                                                                                                                     \
                                                                                                                          \
    *result = to_value();                                                                                                 \
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

static bool scrollDownMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    int count;
    QString unitsString = QLatin1String("line");
    switch(arguments.size()) {
    case 2:
        if(!readArguments(arguments, 0, errMsg, &count, &unitsString)) {
            return false;
        }
        break;
    case 1:
        if(!readArguments(arguments, 0, errMsg, &count)) {
            return false;
        }
        break;
    default:
        M_FAILURE(WrongNumberOfArguments);
    }


    TextArea::ScrollUnits units;
    if(unitsString.startsWith(QLatin1String("page"))) {
        units = TextArea::ScrollUnits::Pages;
    } else if(unitsString.startsWith(QLatin1String("line"))) {
        units = TextArea::ScrollUnits::Lines;
    } else {
        return false;
    }

    if(auto window = MainWindow::fromDocument(document)) {
        if(TextArea *area = window->lastFocus()) {
            area->scrollDownAP(count, units, TextArea::SupressRecording);                                                                             \
        }
    }

    *result = to_value();
    return true;
}

static bool scrollUpMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    int count;
    QString unitsString = QLatin1String("line");
    switch(arguments.size()) {
    case 2:
        if(!readArguments(arguments, 0, errMsg, &count, &unitsString)) {
            return false;
        }
        break;
    case 1:
        if(!readArguments(arguments, 0, errMsg, &count)) {
            return false;
        }
        break;
    default:
        M_FAILURE(WrongNumberOfArguments);
    }


    TextArea::ScrollUnits units;
    if(unitsString.startsWith(QLatin1String("page"))) {
        units = TextArea::ScrollUnits::Pages;
    } else if(unitsString.startsWith(QLatin1String("line"))) {
        units = TextArea::ScrollUnits::Lines;
    } else {
        return false;
    }

    if(auto window = MainWindow::fromDocument(document)) {
        if(TextArea *area = window->lastFocus()) {
            area->scrollUpAP(count, units, TextArea::SupressRecording);                                                                             \
        }
    }

    *result = to_value();
    return true;
}

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
    {"scroll_down",               scrollDownMS},
    {"scroll_left",               scrollLeftMS},
    {"scroll_right",              scrollRightMS},
    {"scroll_up",                 scrollUpMS},
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
    static bool routineName(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {          \
                                                                                                                              \
        /* ensure that we are dealing with the document which currently has the focus */                                      \
        document = MacroRunDocumentEx();                                                                                      \
                                                                                                                              \
        QString string;                                                                                                       \
        if(!readArguments(arguments, 0, errMsg, &string)) {                                                                   \
            return false;                                                                                                     \
        }                                                                                                                     \
                                                                                                                              \
        if(auto window = MainWindow::fromDocument(document)) {                                                                \
            window->slotName(document, string);                                                                               \
        }                                                                                                                     \
                                                                                                                              \
        *result = to_value();                                                                                                 \
        return true;                                                                                                          \
    }

#define WINDOW_MENU_EVENT(routineName, slotName)                                                                                \
    static bool routineName(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {            \
                                                                                                                                \
        /* ensure that we are dealing with the document which currently has the focus */                                        \
        document = MacroRunDocumentEx();                                                                                        \
                                                                                                                                \
        if(!arguments.empty()) {                                                                                                \
            M_FAILURE(WrongNumberOfArguments);                                                                                  \
        }                                                                                                                       \
                                                                                                                                \
        if(auto window = MainWindow::fromDocument(document)) {                                                                  \
            window->slotName(document);                                                                                         \
        }                                                                                                                       \
                                                                                                                                \
        *result = to_value();                                                                                                   \
        return true;                                                                                                            \
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

WINDOW_MENU_EVENT(closePaneMS,                       action_Close_Pane)
WINDOW_MENU_EVENT(deleteMS,                          action_Delete)
WINDOW_MENU_EVENT(exitMS,                            action_Exit)
WINDOW_MENU_EVENT(fillParagraphMS,                   action_Fill_Paragraph)
WINDOW_MENU_EVENT(gotoLineNumberDialogMS,            action_Goto_Line_Number)
WINDOW_MENU_EVENT(gotoSelectedMS,                    action_Goto_Selected)
WINDOW_MENU_EVENT(gotoMatchingMS,                    action_Goto_Matching)
WINDOW_MENU_EVENT(includeFileDialogMS,               action_Include_File)
WINDOW_MENU_EVENT(insertControlCodeDialogMS,         action_Insert_Ctrl_Code)
WINDOW_MENU_EVENT(loadMacroFileDialogMS,             action_Load_Macro_File)
WINDOW_MENU_EVENT(loadTagsFileDialogMS,              action_Load_Tags_File)
WINDOW_MENU_EVENT(loadTipsFileDialogMS,              action_Load_Calltips_File)
WINDOW_MENU_EVENT(lowercaseMS,                       action_Lower_case)
WINDOW_MENU_EVENT(openSelectedMS,                    action_Open_Selected)
WINDOW_MENU_EVENT(printMS,                           action_Print)
WINDOW_MENU_EVENT(printSelectionMS,                  action_Print_Selection)
WINDOW_MENU_EVENT(redoMS,                            action_Redo)
WINDOW_MENU_EVENT(saveAsDialogMS,                    action_Save_As)
WINDOW_MENU_EVENT(saveMS,                            action_Save)
WINDOW_MENU_EVENT(selectAllMS,                       action_Select_All)
WINDOW_MENU_EVENT(shiftLeftMS,                       action_Shift_Left)
WINDOW_MENU_EVENT(shiftLeftTabMS,                    action_Shift_Left_Tabs)
WINDOW_MENU_EVENT(shiftRightMS,                      action_Shift_Right)
WINDOW_MENU_EVENT(shiftRightTabMS,                   action_Shift_Right_Tabs)
WINDOW_MENU_EVENT(splitPaneMS,                       action_Split_Pane)
WINDOW_MENU_EVENT(undoMS,                            action_Undo)
WINDOW_MENU_EVENT(uppercaseMS,                       action_Upper_case)
WINDOW_MENU_EVENT(openDialogMS,                      action_Open)
WINDOW_MENU_EVENT(revertToSavedDialogMS,             action_Revert_to_Saved)
WINDOW_MENU_EVENT(revertToSavedMS,                   action_Revert_to_Saved)
WINDOW_MENU_EVENT(markDialogMS,                      action_Mark)
WINDOW_MENU_EVENT(showTipMS,                         action_Show_Calltip)
WINDOW_MENU_EVENT(selectToMatchingMS,                action_Shift_Goto_Matching)
WINDOW_MENU_EVENT(filterSelectionDialogMS,           action_Filter_Selection)
WINDOW_MENU_EVENT(executeCommandDialogMS,            action_Execute_Command)
WINDOW_MENU_EVENT(executeCommandLineMS,              action_Execute_Command_Line)
WINDOW_MENU_EVENT(repeatMacroDialogMS,               action_Repeat)
WINDOW_MENU_EVENT(detachDocumentMS,                  action_Detach_Document)
WINDOW_MENU_EVENT(moveDocumentMS,                    action_Move_Tab_To)
WINDOW_MENU_EVENT(newOppositeMS,                     action_Close_Pane)

/*
** Scans action argument list for arguments "forward" or "backward" to
** determine search direction for search and replace actions.  "ignoreArgs"
** tells the routine how many required arguments there are to ignore before
** looking for keywords
*/
static Direction searchDirection(Arguments arguments, int index) {
    for(int i = index; i < arguments.size(); ++i) {
        QString arg;
        if (!readArgument(arguments[i], &arg)) {
            return Direction::Forward;
        }

        if (arg.compare(QLatin1String("forward"), Qt::CaseInsensitive) == 0)
            return Direction::Forward;

        if (arg.compare(QLatin1String("backward"), Qt::CaseInsensitive) == 0)
            return Direction::Backward;
    }

    return Direction::Forward;
}


/*
** Scans action argument list for arguments "keep" or "nokeep" to
** determine whether to keep dialogs up for search and replace.  "ignoreArgs"
** tells the routine how many required arguments there are to ignore before
** looking for keywords
*/
static int searchKeepDialogs(Arguments arguments, int index) {
    for(int i = index; i < arguments.size(); ++i) {
        QString arg;
        if (!readArgument(arguments[i], &arg)) {
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
static WrapMode searchWrap(Arguments arguments, int index) {
    for(int i = index; i < arguments.size(); ++i) {
        QString arg;
        if (!readArgument(arguments[i], &arg)) {
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
bool StringToSearchType(const QString &string, SearchType *searchType) {

    static const struct {
        QLatin1String name;
        SearchType type;
    } searchTypeStrings[] = {
        { QLatin1String("literal"),     SearchType::Literal },
        { QLatin1String("case"),        SearchType::CaseSense },
        { QLatin1String("regex"),       SearchType::Regex },
        { QLatin1String("word"),        SearchType::LiteralWord },
        { QLatin1String("caseWord"),    SearchType::CaseSenseWord },
        { QLatin1String("regexNoCase"), SearchType::RegexNoCase },
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
static SearchType searchType(Arguments arguments, int index) {

    for(int i = index; i < arguments.size(); ++i) {
        QString arg;

        if (!readArgument(arguments[i], &arg)) {
            return GetPrefSearch();
        }

        SearchType type;
        if(StringToSearchType(arg, &type)) {
            return type;
        }
    }

    return GetPrefSearch();
}

static bool closeMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunDocumentEx();

    if(arguments.size() > 1) {
        M_FAILURE(WrongNumberOfArguments);
    }

    CloseMode mode = CloseMode::Prompt;

    if(arguments.size() == 1) {
        QString string;
        if(!readArguments(arguments, 0, errMsg, &string)) {
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

    if(auto window = MainWindow::fromDocument(document)) {
        window->action_Close(document, mode);
    }

    *result = to_value();
    return true;
}

static bool newMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunDocumentEx();

    if(arguments.size() > 1) {
        M_FAILURE(WrongNumberOfArguments);
    }

    NewMode mode = NewMode::Prefs;

    if(arguments.size() == 1) {
        QString string;
        if(!readArguments(arguments, 0, errMsg, &string)) {
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

    if(auto window = MainWindow::fromDocument(document)) {
        window->action_New(document, mode);
    }

    *result = to_value();
    return true;
}

static bool saveAsMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunDocumentEx();

    if(arguments.size() > 2 || arguments.empty()) {
        M_FAILURE(WrongNumberOfArguments);
    }

    QString filename;
    if (!readArgument(arguments[0], &filename, errMsg)) {
        return false;
    }

    bool wrapped = false;

    // NOTE(eteran): "wrapped" optional argument is not documented
    if(arguments.size() == 2) {
        QString string;
        if(!readArgument(arguments[1], &string, errMsg)) {
            return false;
        }

        if(string.compare(QLatin1String("wrapped"), Qt::CaseInsensitive) == 0) {
            wrapped = true;
        }
    }

    if(auto window = MainWindow::fromDocument(document)) {
        window->action_Save_As(document, filename, wrapped);
    }

    *result = to_value();
    return true;
}

static bool findMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    // find( search-string [, search-direction] [, search-type] [, search-wrap] )

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunDocumentEx();

    if(arguments.size() > 4 || arguments.empty()) {
        M_FAILURE(WrongNumberOfArguments);
    }

    QString string;
    if(!readArguments(arguments, 0, errMsg, &string)) {
        return false;
    }

    Direction direction = searchDirection(arguments, 1);
    SearchType      type      = searchType(arguments, 1);
    WrapMode        wrap      = searchWrap(arguments, 1);

    if(auto window = MainWindow::fromDocument(document)) {
        window->action_Find(document, string, direction, type, wrap);
    }

    *result = to_value();
    return true;
}

static bool findDialogMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunDocumentEx();

    Direction direction = searchDirection(arguments, 0);
    SearchType      type      = searchType(arguments, 0);
    bool            keep      = searchKeepDialogs(arguments, 0);

    if(auto window = MainWindow::fromDocument(document)) {
        window->action_Find_Dialog(document, direction, type, keep);
    }

    *result = to_value();
    return true;
}

static bool findAgainMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunDocumentEx();

    Direction direction = searchDirection(arguments, 0);
    WrapMode wrap       = searchWrap(arguments, 0);

    if(auto window = MainWindow::fromDocument(document)) {
        window->action_Find_Again(document, direction, wrap);
    }

    *result = to_value();
    return true;
}

static bool findSelectionMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunDocumentEx();

    Direction direction = searchDirection(arguments, 0);
    SearchType type     = searchType(arguments, 0);
    WrapMode wrap       = searchWrap(arguments, 0);

    if(auto window = MainWindow::fromDocument(document)) {
        window->action_Find_Selection(document, direction, type, wrap);
    }

    *result = to_value();
    return true;
}

static bool replaceMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunDocumentEx();

    if(arguments.size() > 5 || arguments.size() < 2) {
        M_FAILURE(WrongNumberOfArguments);
    }

    QString searchString;
    QString replaceString;
    if(!readArguments(arguments, 0, errMsg, &searchString, &replaceString)) {
        return false;
    }

    Direction direction = searchDirection(arguments, 2);
    SearchType type     = searchType(arguments, 2);
    WrapMode   wrap     = searchWrap(arguments, 2);

    if(auto window = MainWindow::fromDocument(document)) {
        window->action_Replace(document, searchString, replaceString, direction, type, wrap);
    }

    *result = to_value();
    return true;
}

static bool replaceDialogMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunDocumentEx();

    Direction direction = searchDirection(arguments, 0);
    SearchType      type      = searchType(arguments, 0);
    bool            keep      = searchKeepDialogs(arguments, 0);

    if(auto window = MainWindow::fromDocument(document)) {
        window->action_Replace_Dialog(document, direction, type, keep);
    }

    *result = to_value();
    return true;
}

static bool replaceAgainMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunDocumentEx();

    if(arguments.size() != 2) {
        M_FAILURE(WrongNumberOfArguments);
    }

    WrapMode wrap       = searchWrap(arguments, 0);
    Direction direction = searchDirection(arguments, 0);

    if(auto window = MainWindow::fromDocument(document)) {
        window->action_Replace_Again(document, direction, wrap);
    }

    *result = to_value();

    return true;
}

static bool gotoMarkMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    // goto_mark( mark, [, extend] )

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunDocumentEx();

    if(arguments.size() > 2 || arguments.size() < 1) {
        M_FAILURE(WrongNumberOfArguments);
    }

    bool extend = false;

    QString mark;
    if(!readArguments(arguments, 0, errMsg, &mark)) {
        return false;
    }

    if(arguments.size() == 2) {
        QString argument;
        if(!readArgument(arguments[1], &argument, errMsg)) {
            return false;
        }

        if(argument.compare(QLatin1String("extend"), Qt::CaseInsensitive) == 0) {
            extend = true;
        }
    }

    if(auto window = MainWindow::fromDocument(document)) {
        window->action_Goto_Mark(document, mark, extend);
    }

    *result = to_value();
    return true;
}

static bool gotoMarkDialogMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    // goto_mark( mark, [, extend] )

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunDocumentEx();

    if(arguments.size() > 1) {
        M_FAILURE(WrongNumberOfArguments);
    }

    bool extend = false;

    if(arguments.size() == 1) {
        QString argument;
        if(!readArgument(arguments[1], &argument, errMsg)) {
            return false;
        }

        if(argument.compare(QLatin1String("extend"), Qt::CaseInsensitive) == 0) {
            extend = true;
        }
    }

    if(auto window = MainWindow::fromDocument(document)) {
        window->action_Goto_Mark_Dialog(document, extend);
    }

    *result = to_value();
    return true;
}

static bool findDefinitionMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    // find_definition( [ argument ] )

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunDocumentEx();

    if(arguments.size() > 1) {
        M_FAILURE(WrongNumberOfArguments);
    }

    QString argument;

    if(arguments.size() == 1) {
        if(!readArgument(arguments[1], &argument, errMsg)) {
            return false;
        }
    }

    if(auto window = MainWindow::fromDocument(document)) {
        window->action_Find_Definition(document, argument);
    }

    *result = to_value();
    return true;
}

static bool repeatMacroMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    // repeat_macro( how/count, method )

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunDocumentEx();

    QString howString;
    QString method;

    if(!readArguments(arguments, 0, errMsg, &howString, &method)) {
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

    if(auto window = MainWindow::fromDocument(document)) {
        window->action_Repeat_Macro(document, method, how);
    }

    *result = to_value();
    return true;
}

static bool detachDocumentDialogMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(arguments);
    Q_UNUSED(result);
    Q_UNUSED(errMsg);

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunDocumentEx();

    if(auto window = MainWindow::fromDocument(document)) {
        window->action_Detach_Document_Dialog(document);
    }

    return true;
}

static bool setAutoIndentMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    QString string;
    if(!readArguments(arguments, 0, errMsg, &string)) {
        qWarning("NEdit: set_auto_indent requires argument");
        return false;
    }

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunDocumentEx();

    if(string == QLatin1String("off")) {
        document->SetAutoIndent(IndentStyle::None);
    } else if(string == QLatin1String("on")) {
        document->SetAutoIndent(IndentStyle::Auto);
    } else if(string == QLatin1String("smart")) {
        document->SetAutoIndent(IndentStyle::Smart);
    } else {
        qWarning("NEdit: set_auto_indent invalid argument");
    }

    *result = to_value();
    return true;
}

static bool setEmTabDistMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    int number;
    if(!readArguments(arguments, 0, errMsg, &number)) {
        qWarning("NEdit: set_em_tab_dist requires argument");
        return false;
    }

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunDocumentEx();

    if (number < 1000) {
        if (number < 0) {
            number = 0;
        }
        document->SetEmTabDist(number);
    } else {
        qWarning("NEdit: set_em_tab_dist requires integer argument >= -1 and < 1000");
    }

    *result = to_value();
    return true;
}

static bool setFontsMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunDocumentEx();

    QString fontName;
    QString italicName;
    QString boldName;
    QString boldItalicName;
    if(!readArguments(arguments, 0, errMsg, &fontName, &italicName, &boldName, &boldItalicName)) {
        qWarning("NEdit: set_fonts requires 4 arguments");
        return false;
    }

    document->action_Set_Fonts(fontName, italicName, boldName, boldItalicName);

    *result = to_value();
    return true;
}

boost::optional<int> toggle_or_bool(Arguments arguments, int previous, const char **errMsg, const char *actionName) {

    int next;
    switch(arguments.size()) {
    case 1:
        if(!readArguments(arguments, 0, errMsg, &next)) {
            return boost::none;
        }
        break;
    case 0:
        next = !previous;
        break;
    default:
        qWarning("NEdit: %s requires 0 or 1 arguments", actionName);
        return boost::none;
    }

    return next;
}


static bool setHighlightSyntaxMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    document = MacroRunDocumentEx();

    if(boost::optional<int> next = toggle_or_bool(arguments, document->GetHighlightSyntax(), errMsg, "set_highlight_syntax")) {
        document->SetHighlightSyntax(*next);
        *result = to_value();
        return true;
    }

    return false;
}

static bool setIncrementalBackupMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    document = MacroRunDocumentEx();

    if(boost::optional<int> next = toggle_or_bool(arguments, document->GetIncrementalBackup(), errMsg, "set_incremental_backup")) {
        document->SetIncrementalBackup(*next);
        *result = to_value();
        return true;
    }

    return false;
}

static bool setIncrementalSearchLineMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    document = MacroRunDocumentEx();

    if(auto win = MainWindow::fromDocument(document)) {
        if(boost::optional<int> next = toggle_or_bool(arguments, win->GetIncrementalSearchLineMS(), errMsg, "set_incremental_search_line")) {
            win->SetIncrementalSearchLineMS(*next);
            *result = to_value();
            return true;
        }
    }

    return false;
}

static bool setMakeBackupCopyMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    document = MacroRunDocumentEx();

    if(boost::optional<int> next = toggle_or_bool(arguments, document->GetMakeBackupCopy(), errMsg, "set_make_backup_copy")) {
        document->SetMakeBackupCopy(*next);
        *result = to_value();
        return true;
    }

    return false;
}

static bool setLockedMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    document = MacroRunDocumentEx();

    if(boost::optional<int> next = toggle_or_bool(arguments, document->GetUserLocked(), errMsg, "set_locked")) {
        document->SetUserLocked(*next);
        *result = to_value();
        return true;
    }

    return false;
}

static bool setLanguageModeMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    document = MacroRunDocumentEx();

    QString languageMode;
    if(!readArguments(arguments, 0, errMsg, &languageMode)) {
        qWarning("NEdit: set_language_mode requires argument");
        return false;
    }

    document->action_Set_Language_Mode(languageMode, false);
    *result = to_value();
    return true;
}

static bool setOvertypeModeMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    document = MacroRunDocumentEx();

    if(boost::optional<int> next = toggle_or_bool(arguments, document->GetOverstrike(), errMsg, "set_overtype_mode")) {
        document->SetOverstrike(*next);
        *result = to_value();
        return true;
    }

    return false;
}

static bool setShowLineNumbersMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    document = MacroRunDocumentEx();

    if(auto win = MainWindow::fromDocument(document)) {
        if(boost::optional<int> next = toggle_or_bool(arguments, win->GetShowLineNumbers(), errMsg, "set_show_line_numbers")) {
            win->SetShowLineNumbers(*next);
            *result = to_value();
            return true;
        }
    }

    return false;
}

static bool setShowMatchingMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    document = MacroRunDocumentEx();

    if (arguments.size() > 0) {
        QString arg;
        if(!readArgument(arguments[0], &arg, errMsg)) {
            return false;
        }

        if (arg == QLatin1String("off")) {
            document->SetShowMatching(ShowMatchingStyle::None);
        } else if (arg == QLatin1String("delimiter")) {
            document->SetShowMatching(ShowMatchingStyle::Delimeter);
        } else if (arg == QLatin1String("range")) {
            document->SetShowMatching(ShowMatchingStyle::Range);
        }
        /* For backward compatibility with pre-5.2 versions, we also
           accept 0 and 1 as aliases for None and Delimeter.
           It is quite unlikely, though, that anyone ever used this
           action procedure via the macro language or a key binding,
           so this can probably be left out safely. */
        else if (arg == QLatin1String("0")) {
           document->SetShowMatching(ShowMatchingStyle::None);
        } else if (arg == QLatin1String("1")) {
           document->SetShowMatching(ShowMatchingStyle::Delimeter);
        } else {
            qWarning("NEdit: Invalid argument for set_show_matching");
        }
    } else {
        qWarning("NEdit: set_show_matching requires argument");
    }


    *result = to_value();
    return true;
}

static bool setMatchSyntaxBasedMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    document = MacroRunDocumentEx();

    if(boost::optional<int> next = toggle_or_bool(arguments, document->GetMatchSyntaxBased(), errMsg, "set_match_syntax_based")) {
        document->SetMatchSyntaxBased(*next);
        *result = to_value();
        return true;
    }

    return false;
}

static bool setStatisticsLineMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    document = MacroRunDocumentEx();

    if(boost::optional<int> next = toggle_or_bool(arguments, document->GetShowStatisticsLine(), errMsg, "set_statistics_line")) {
        document->SetShowStatisticsLine(*next);
        *result = to_value();
        return true;
    }

    return false;
}

static bool setTabDistMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    document = MacroRunDocumentEx();

    if (arguments.size() > 0) {
        int newTabDist = 0;
        if(!readArguments(arguments, 0, errMsg, &newTabDist) && newTabDist > 0 && newTabDist <= TextBuffer::MAX_EXP_CHAR_LEN) {
            document->SetTabDist(newTabDist);
        } else {
            qWarning("NEdit: set_tab_dist requires integer argument > 0 and <= %d", TextBuffer::MAX_EXP_CHAR_LEN);
        }
    } else {
        qWarning("NEdit: set_tab_dist requires argument");
    }

    *result = to_value();
    return true;
}

static bool setUseTabsMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    document = MacroRunDocumentEx();

    if(boost::optional<int> next = toggle_or_bool(arguments, document->GetUseTabs(), errMsg, "set_use_tabs")) {
        document->SetUseTabs(*next);
        *result = to_value();
        return true;
    }

    return false;
}

static bool setWrapMarginMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    document = MacroRunDocumentEx();

    if (arguments.size() > 0) {
        int newMargin = 0;
        if(!readArguments(arguments, 0, errMsg, &newMargin) && newMargin > 0 && newMargin <= 1000) {

            const std::vector<TextArea *> panes = document->textPanes();

            for(TextArea *area : panes) {
                area->setWrapMargin(newMargin);
            }
        } else {
            qWarning("NEdit: set_wrap_margin requires integer argument >= 0 and < 1000");
        }
    } else {
        qWarning("NEdit: set_wrap_margin requires argument");
    }

    *result = to_value();
    return true;
}

static bool setWrapTextMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    document = MacroRunDocumentEx();

    if (arguments.size() > 0) {

        QString arg;
        if(!readArgument(arguments[0], &arg, errMsg)) {
            return false;
        }

        if (arg == QLatin1String("none")) {
            document->SetAutoWrap(WrapStyle::None);
        } else if (arg == QLatin1String("auto")) {
            document->SetAutoWrap(WrapStyle::Newline);
        } else if (arg == QLatin1String("continuous")) {
           document->SetAutoWrap(WrapStyle::Continuous);
        } else {
            qWarning("NEdit: set_wrap_text invalid argument");
        }
    } else {
        qWarning("NEdit: set_wrap_text requires argument");
    }

    *result = to_value();
    return true;
}

static bool findIncrMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    document = MacroRunDocumentEx();

    auto win = MainWindow::fromDocument(document);

    if(!win) {
        return false;
    }

    int i;
    bool continued = false;

    QString arg;
    if(!readArgument(arguments[0], &arg, errMsg)) {
        qWarning("NEdit: find action requires search string argument");
        return false;
    }

    for (i = 1; i < arguments.size(); ++i)  {
        QString arg2;
        if(!readArgument(arguments[0], &arg2, errMsg)) {
            return false;
        }

        if(arg2.compare(QLatin1String("continued"), Qt::CaseInsensitive) == 0) {
            continued = true;
        }
    }

    win->action_Find_Incremental(
        document,
        arg,
        searchDirection(arguments, 1),
        searchType(arguments, 1),
        searchWrap(arguments, 1),
        continued);

    *result = to_value();
    return true;
}

static bool startIncrFindMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);

    document = MacroRunDocumentEx();

    auto win = MainWindow::fromDocument(document);

    if(!win) {
        return false;
    }

    win->BeginISearchEx(searchDirection(arguments, 0));

    *result = to_value();
    return true;
}

static bool replaceFindMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    document = MacroRunDocumentEx();

    auto win = MainWindow::fromDocument(document);

    if(!win) {
        return false;
    }

    if (document->CheckReadOnly()) {
        return false;
    }

    QString searchString;
    QString replaceString;
    if(!readArguments(arguments, 0, errMsg, &searchString, &replaceString)) {
        M_FAILURE(InvalidSearchReplaceArgs);
    }

    win->action_Replace_Find(
                document,
                searchString,
                replaceString,
                searchDirection(arguments, 2),
                searchType(arguments, 2),
                searchWrap(arguments, 0));

    *result = to_value();
    return true;

}

static bool replaceFindSameMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);

    document = MacroRunDocumentEx();

    auto win = MainWindow::fromDocument(document);

    if(!win) {
        return false;
    }

    if (document->CheckReadOnly()) {
        return false;
    }

    win->action_Replace_Find_Again(
                document,
                searchDirection(arguments, 0),
                searchWrap(arguments, 0));

    *result = to_value();
    return true;
}

static bool nextDocumentMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(arguments);
    Q_UNUSED(errMsg);

    document = MacroRunDocumentEx();

    auto win = MainWindow::fromDocument(document);

    if(!win) {
        return false;
    }

    win->action_Next_Document();

    *result = to_value();
    return true;
}

static bool prevDocumentMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(arguments);
    Q_UNUSED(errMsg);

    document = MacroRunDocumentEx();

    auto win = MainWindow::fromDocument(document);

    if(!win) {
        return false;
    }

    win->action_Prev_Document();

    *result = to_value();
    return true;
}

static bool lastDocumentMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(arguments);
    Q_UNUSED(errMsg);

    document = MacroRunDocumentEx();

    auto win = MainWindow::fromDocument(document);

    if(!win) {
        return false;
    }

    win->action_Last_Document();

    *result = to_value();
    return true;
}

static bool backgroundMenuCommandMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(arguments);
    Q_UNUSED(errMsg);

    document = MacroRunDocumentEx();

    auto win = MainWindow::fromDocument(document);

    if(!win) {
        return false;
    }

    QString name;
    if(!readArguments(arguments, 0, errMsg, &name)) {
        qWarning("NEdit: bg_menu_command requires item-name argument\n");
        return false;
    }

    win->DoNamedBGMenuCmd(document, win->lastFocus(), name, true);

    *result = to_value();
    return true;
}

static const SubRoutine MenuMacroSubrNames[] = {
    // File
    { "new",                          newMS },
    { "new_opposite",                 newOppositeMS },
    { "open",                         openMS },
    { "open_dialog",                  openDialogMS },
    { "open_selected",                openSelectedMS },
    { "close",                        closeMS },
    { "save",                         saveMS },
    { "save_as",                      saveAsMS },
    { "save_as_dialog",               saveAsDialogMS },
    { "revert_to_saved_dialog",       revertToSavedDialogMS },
    { "revert_to_saved",              revertToSavedMS },
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

    // These aren't mentioned in the documentation...
    { "find_incremental",             findIncrMS },
    { "start_incremental_find",       startIncrFindMS },
    { "replace_find",                 replaceFindMS },
    { "replace_find_same",            replaceFindSameMS },
    { "replace_find_again",           replaceFindSameMS },
    { "next_document",                nextDocumentMS },
    { "previous_document",            prevDocumentMS },
    { "last_document",                lastDocumentMS },
    { "bg_menu_command",              backgroundMenuCommandMS },
#if 0 // NOTE(eteran): what are these for? are they needed?
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
        subrPtr = to_value(routine.function);
        InstallSymbol(routine.name, C_FUNCTION_SYM, subrPtr);
    }

    for(const SubRoutine &routine : SpecialVars) {
        subrPtr = to_value(routine.function);
        InstallSymbol(routine.name, PROC_VALUE_SYM, subrPtr);
    }

    // NOTE(eteran): things that were in the menu action list
    for(const SubRoutine &routine : MenuMacroSubrNames) {
        subrPtr = to_value(routine.function);
        InstallSymbol(routine.name, C_FUNCTION_SYM, subrPtr);
    }

    // NOTE(eteran): things that were in the text widget action list
    for(const SubRoutine &routine : TextAreaSubrNames) {
        subrPtr = to_value(routine.function);
        InstallSymbol(routine.name, C_FUNCTION_SYM, subrPtr);
    }

    /* Define global variables used for return values, remember their
       locations so they can be set without a LookupSymbol call */
    for (unsigned int i = 0; i < N_RETURN_GLOBALS; i++)
        ReturnGlobals[i] = InstallSymbol(ReturnGlobalNames[i], GLOBAL_SYM, noValue);
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
    *stoppedAt = gsl::narrow<int>(e - ptr);
    return p;
}

bool readCheckMacroStringEx(QWidget *dialogParent, const QString &string, DocumentWidget *runDocument, const QString &errIn, int *errPos) {

    Input in(&string);

    DataValue subrPtr;
    std::stack<Program *> progStack;

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
            if (runDocument) {
                Symbol *sym = LookupSymbolEx(subrName);
                if(!sym) {
                    subrPtr = to_value(prog);
                    sym = InstallSymbolEx(subrName, MACRO_FUNCTION_SYM, subrPtr);
                } else {
                    if (sym->type == MACRO_FUNCTION_SYM) {
                        FreeProgram(to_program(sym->value));
                    } else {
                        sym->type = MACRO_FUNCTION_SYM;
                    }

                    sym->value = to_value(prog);
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

            if (runDocument) {

                if (!runDocument->macroCmdData_) {
                    runDocument->runMacroEx(prog);
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
** Do garbage collection of strings if there are no macros currently
** executing.  NEdit's macro language GC strategy is to call this routine
** whenever a macro completes.  If other macros are still running (preempted
** or waiting for a shell command or dialog), this does nothing and therefore
** defers GC to the completion of the last macro out.
*/
void SafeGC() {

    for(DocumentWidget *document : DocumentWidget::allDocuments()) {
        if (document->macroCmdData_ || document->InSmartIndentMacrosEx()) {
            return;
        }
    }

    GarbageCollectStrings();
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
** Built-in macro subroutine for getting the length of a string
*/
static bool lengthMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(document);

    QString string;
    if(!readArguments(arguments, 0, errMsg, &string)) {
        return false;
    }

    *result = to_value(string.size());
    return true;
}

/*
** Built-in macro subroutines for min and max
*/
static bool minMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(document);

    if (arguments.size() == 1) {
        M_FAILURE(TooFewArguments);
    }

    int minVal;
    if (!readArgument(arguments[0], &minVal, errMsg)) {
        return false;
    }

    for (const DataValue &dv : arguments) {
        int value;
        if (!readArgument(dv, &value, errMsg)) {
            return false;
        }

        minVal = std::min(minVal, value);
    }

    *result = to_value(minVal);
    return true;
}

static bool maxMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(document);

    if (arguments.size() == 1) {
        M_FAILURE(TooFewArguments);
    }

    int maxVal;
    if (!readArgument(arguments[0], &maxVal, errMsg)) {
        return false;
    }

    for (const DataValue &dv : arguments) {
        int value;
        if (!readArgument(dv, &value, errMsg)) {
            return false;
        }

        maxVal = std::max(maxVal, value);
    }

    *result = to_value(maxVal);
    return true;
}

static bool focusWindowMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {


    /* Read the argument representing the window to focus to, and translate
       it into a pointer to a real DocumentWidget */
    if (arguments.size() != 1) {
        M_FAILURE(WrongNumberOfArguments);
    }

    QString string;
    if (!readArgument(arguments[0], &string, errMsg)) {
        return false;
    }

    std::vector<DocumentWidget *> documents = DocumentWidget::allDocuments();
    std::vector<DocumentWidget *>::iterator it;

    if (string == QLatin1String("last")) {
        it = documents.begin();
    } else if (string == QLatin1String("next")) {

        auto curr = std::find_if(documents.begin(), documents.end(), [document](DocumentWidget *doc) {
            return doc == document;
        });

        if(curr != documents.end()) {
            it = std::next(curr);
        }
    } else if (string.size() >= MAXPATHLEN) {
        M_FAILURE(PathnameTooLong);
    } else {
        // just use the plain name as supplied
        it = std::find_if(documents.begin(), documents.end(), [&string](DocumentWidget *doc) {
            QString fullname = doc->FullPath();
            return fullname == string;
        });

        // didn't work? try normalizing the string passed in
        if(it == documents.end()) {

            QString normalizedString = NormalizePathnameEx(string);
            if(normalizedString.isNull()) {
                //  Something is broken with the input pathname.
                M_FAILURE(PathnameTooLong);
            }

            it = std::find_if(documents.begin(), documents.end(), [&normalizedString](DocumentWidget *win) {
                QString fullname = win->FullPath();
                return fullname == normalizedString;
            });
        }
    }

    // If no matching window was found, return empty string and do nothing
    if(it == documents.end()) {
        *result = to_value(std::string());
        return true;
    }

    DocumentWidget *const target_document = *it;

    // Change the focused window to the requested one
    SetMacroFocusWindowEx(target_document);

    // turn on syntax highlight that might have been deferred
    if (target_document->highlightSyntax_ && !target_document->highlightData_) {
        target_document->StartHighlightingEx(false);
    }

    // Return the name of the window
    *result = to_value(QString(QLatin1String("%1%2")).arg(target_document->path_, target_document->filename_));
    return true;
}

/*
** Built-in macro subroutine for getting text from the current window's text
** buffer
*/
static bool getRangeMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    int from;
    int to;
    TextBuffer *buf = document->buffer_;

    // Validate arguments and convert to int
    if(!readArguments(arguments, 0, errMsg, &from, &to)) {
        return false;
    }

    from = qBound(0, from, buf->BufGetLength());
    to   = qBound(0, to,   buf->BufGetLength());

    if (from > to) {
        std::swap(from, to);
    }

    /* Copy text from buffer (this extra copy could be avoided if TextBuffer.c
       provided a routine for writing into a pre-allocated string) */

    std::string rangeText = buf->BufGetRangeEx(from, to);

    *result = to_value(rangeText);
    return true;
}

/*
** Built-in macro subroutine for getting a single character at the position
** given, from the current window
*/
static bool getCharacterMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    int pos;
    TextBuffer *buf = document->buffer_;

    // Validate arguments and convert to int
    if(!readArguments(arguments, 0, errMsg, &pos)) {
        return false;
    }

    pos = qBound(0, pos, buf->BufGetLength());

    // Return the character in a pre-allocated string)
    std::string str(1, buf->BufGetCharacter(pos));

    *result = to_value(str);
    return true;
}

/*
** Built-in macro subroutine for replacing text in the current window's text
** buffer
*/
static bool replaceRangeMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    int from;
    int to;
    TextBuffer *buf = document->buffer_;
    std::string string;

    // Validate arguments and convert to int
    if(!readArguments(arguments, 0, errMsg, &from, &to, &string)) {
        return false;
    }

    from = qBound(0, from, buf->BufGetLength());
    to   = qBound(0, to,   buf->BufGetLength());

    if (from > to) {
        std::swap(from, to);
    }

    // Don't allow modifications if the window is read-only
    if (document->lockReasons_.isAnyLocked()) {
        QApplication::beep();
        *result = to_value();
        return true;
    }

    // Do the replace
    buf->BufReplaceEx(from, to, string);
    *result = to_value();
    return true;
}

/*
** Built-in macro subroutine for replacing the primary-selection selected
** text in the current window's text buffer
*/
static bool replaceSelectionMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    std::string string;

    // Validate argument and convert to string
    if(!readArguments(arguments, 0, errMsg, &string)) {
        return false;
    }

    // Don't allow modifications if the window is read-only
    if (document->lockReasons_.isAnyLocked()) {
        QApplication::beep();
        *result = to_value();
        return true;
    }

    // Do the replace
    document->buffer_->BufReplaceSelectedEx(string);
    *result = to_value();
    return true;
}

/*
** Built-in macro subroutine for getting the text currently selected by
** the primary selection in the current window's text buffer, or in any
** part of screen if "any" argument is given
*/
static bool getSelectionMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    /* Read argument list to check for "any" keyword, and get the appropriate
       selection */
    if (!arguments.empty() && arguments.size() != 1) {
        M_FAILURE(WrongNumberOfArguments);
    }

    if (arguments.size() == 1) {
        if (!is_string(arguments[0]) || to_string(arguments[0]) != "any") {
            M_FAILURE(UnrecognizedArgument);
        }

        QString text = document->GetAnySelectionEx();
        if (text.isNull()) {
            text = QLatin1String("");
        }

        // Return the text as an allocated string
        *result = to_value(text);
    } else {
        std::string selText = document->buffer_->BufGetSelectionTextEx();

        // Return the text as an allocated string
        *result = to_value(selText);
    }

    return true;
}

/*
** Built-in macro subroutine for determining if implicit conversion of
** a string to number will succeed or fail
*/
static bool validNumberMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    Q_UNUSED(document);

    std::string string;

    if(!readArguments(arguments, 0, errMsg, &string)) {
        return false;
    }

    *result = to_value(StringToNum(string, nullptr));
    return true;
}

/*
** Built-in macro subroutine for replacing a substring within another string
*/
static bool replaceSubstringMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(document);

    int from;
    int to;
    QString string;
    QString replStr;

    // Validate arguments and convert to int
    if(!readArguments(arguments, 0, errMsg, &string, &from, &to, &replStr)) {
        return false;
    }

    const int length = string.size();

    from = qBound(0, from, length);
    to   = qBound(0, to,   length);

    if (from > to) {
        std::swap(from, to);
    }

    // Allocate a new string and do the replacement
    string.replace(from, to - from, replStr);
    *result = to_value(string);

    return true;
}

/*
** Built-in macro subroutine for getting a substring of a string.
** Called as substring(string, from [, to])
*/
static bool substringMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(document);

    // Validate arguments and convert to int
    if (arguments.size() != 2 && arguments.size() != 3) {
        M_FAILURE(WrongNumberOfArguments);
    }

    int from;
    QString string;

    if(!readArguments(arguments, 0, errMsg, &string, &from)) {
        return false;
    }

    int length = string.size();
    int to     = string.size();

    if (arguments.size() == 3) {
        if (!readArgument(arguments[2], &to, errMsg)) {
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
    *result = to_value(string.mid(from, to - from));
    return true;
}

static bool toupperMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(document);
    std::string string;

    // Validate arguments and convert to int
    if(!readArguments(arguments, 0, errMsg, &string)) {
        return false;
    }

    // Allocate a new string and copy an uppercased version of the string it
    for(char &ch : string) {
        ch = safe_ctype<toupper>(ch);
    }

    *result = to_value(string);
    return true;
}

static bool tolowerMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(document);
    std::string string;

    // Validate arguments and convert to int
    if(!readArguments(arguments, 0, errMsg, &string)) {
        return false;
    }

    // Allocate a new string and copy an uppercased version of the string it
    for(char &ch : string) {
        ch = safe_ctype<tolower>(ch);
    }

    *result = to_value(string);
    return true;
}

static bool stringToClipboardMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(document);

    QString string;

    // Get the string argument
    if(!readArguments(arguments, 0, errMsg, &string)) {
        return false;
    }

    QApplication::clipboard()->setText(string, QClipboard::Clipboard);

    *result = to_value();
    return true;
}

static bool clipboardToStringMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(document);

    // Should have no arguments
    if (!arguments.empty()) {
        M_FAILURE(WrongNumberOfArguments);
    }

    // Ask if there's a string in the clipboard, and get its length
    const QMimeData *data = QApplication::clipboard()->mimeData(QClipboard::Clipboard);
    if(!data->hasText()) {
        *result = to_value(std::string());
    } else {
        // Allocate a new string to hold the data
        *result = to_value(data->text());
    }

    return true;
}

/*
** Built-in macro subroutine for reading the contents of a text file into
** a string.  On success, returns 1 in $readStatus, and the contents of the
** file as a string in the subroutine return value.  On failure, returns
** the empty string "" and an 0 $readStatus.
*/
static bool readFileMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    Q_UNUSED(document);

    std::string name;

    // Validate arguments
    if(!readArguments(arguments, 0, errMsg, &name)) {
        return false;
    }

    // Read the whole file into an allocated string
    std::ifstream file(name, std::ios::binary);
    if(file) {
        std::string contents(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});

        *result = to_value(contents);

        // Return the results
        ReturnGlobals[READ_STATUS]->value = to_value(true);
        return true;
    }

    ReturnGlobals[READ_STATUS]->value = to_value(false);

    *result = to_value(std::string());
    return true;
}

/*
** Built-in macro subroutines for writing or appending a string (parameter $1)
** to a file named in parameter $2. Returns 1 on successful write, or 0 if
** unsuccessful.
*/
static bool writeFileMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    return writeOrAppendFile(false, document, arguments, result, errMsg);
}

static bool appendFileMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    return writeOrAppendFile(true, document, arguments, result, errMsg);
}

static bool writeOrAppendFile(bool append, DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(document);

    std::string name;
    std::string string;

    // Validate argument
    if(!readArguments(arguments, 0, errMsg, &string, &name)) {
        return false;
    }

    // open the file
    FILE *fp = ::fopen(name.c_str(), append ? "a" : "w");
    if (!fp) {
        *result = to_value(false);
        return true;
    }

    auto _ = gsl::finally([fp] { ::fclose(fp); });

    // write the string to the file
    ::fwrite(string.data(), 1, string.size(), fp);
    if (::ferror(fp)) {
        *result = to_value(false);
        return true;
    }

    // return the status
    *result = to_value(true);
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
static bool searchMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    DataValue newArgList[9];

    /* Use the search string routine, by adding the buffer contents as the
     * string argument */
    if (arguments.size() > 8)
        M_FAILURE(WrongNumberOfArguments);

    /* we remove constness from BufAsString() result since we know
     * searchStringMS will not modify the result. NOTE(eteran):
     * We do this instead of using a string_view, because this version of
     * to_value() doesn't make a copy!
     */
    auto str = const_cast<char *>(document->buffer_->BufAsString());
    int size = document->buffer_->BufGetLength();
    newArgList[0] = to_value(str, size);

    // copy other arguments to the new argument list
    std::copy(arguments.begin(), arguments.end(), &newArgList[1]);

    return searchStringMS(
                document,
                Arguments(newArgList, arguments.size() + 1),
                result,
                errMsg);
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
static bool searchStringMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    int beginPos;
    WrapMode wrap;
    bool found     = false;
    int foundStart = -1;
    int foundEnd   = 0;
    SearchType type;
    bool skipSearch = false;
    std::string string;
    QString searchStr;
    Direction direction;

    // Validate arguments and convert to proper types
    if (arguments.size() < 3)
        M_FAILURE(TooFewArguments);
    if (!readArgument(arguments[0], &string, errMsg))
        return false;
    if (!readArgument(arguments[1], &searchStr, errMsg))
        return false;
    if (!readArgument(arguments[2], &beginPos, errMsg))
        return false;
    if (!readSearchArgs(arguments.subspan(3), &direction, &type, &wrap, errMsg))
        return false;

    int len = gsl::narrow<int>(to_string(arguments[0]).size());
    if (beginPos > len) {
        if (direction == Direction::Forward) {
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
        if (direction == Direction::Backward) {
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
                    document->GetWindowDelimitersEx());
    }

    // Return the results
    ReturnGlobals[SEARCH_END]->value = to_value(found ? foundEnd : 0);
    *result = to_value(found ? foundStart : -1);
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
static bool replaceInStringMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    std::string string;
    QString searchStr;
    QString replaceStr;
    auto searchType = SearchType::Literal;
    int copyStart;
    int copyEnd;
    bool force = false;
    int i;

    // Validate arguments and convert to proper types
    if (arguments.size() < 3 || arguments.size() > 5)
        M_FAILURE(WrongNumberOfArguments);
    if (!readArgument(arguments[0], &string, errMsg))
        return false;
    if (!readArgument(arguments[1], &searchStr, errMsg))
        return false;
    if (!readArgument(arguments[2], &replaceStr, errMsg))
        return false;

    for (i = 3; i < arguments.size(); i++) {
        // Read the optional search type and force arguments
        QString argStr;

        if (!readArgument(arguments[i], &argStr, errMsg))
            return false;

        if (!StringToSearchType(argStr, &searchType)) {
            // It's not a search type.  is it "copy"?
            if (argStr == QLatin1String("copy")) {
                force = true;
            } else {
                M_FAILURE(UnrecognizedArgument);
            }
        }
    }

    // Do the replace
    bool ok;
    std::string replacedStr = ReplaceAllInStringEx(
                string,
                searchStr,
                replaceStr,
                searchType,
                &copyStart,
                &copyEnd,
                document->GetWindowDelimitersEx(),
                &ok);

    // Return the results

    if(!ok) {
        if (force) {
            *result = to_value(string);
        } else {
            *result = to_value(std::string());
        }
    } else {
        std::string new_string;
        new_string.reserve(string.size() + replacedStr.size());

        new_string.append(string.substr(0, copyStart));
        new_string.append(replacedStr);
        new_string.append(string.substr(copyEnd));

        *result = to_value(new_string);
    }

    return true;
}

static bool readSearchArgs(Arguments arguments, Direction *searchDirection, SearchType *searchType, WrapMode *wrap, const char **errMsg) {

    QString argStr;

    *wrap            = WrapMode::NoWrap;
    *searchDirection = Direction::Forward;
    *searchType      = SearchType::Literal;

    for (const DataValue &dv : arguments) {
        if (!readArgument(dv, &argStr, errMsg))
            return false;
        else if (argStr == QLatin1String("wrap"))
            *wrap = WrapMode::Wrap;
        else if (argStr == QLatin1String("nowrap"))
            *wrap = WrapMode::NoWrap;
        else if (argStr == QLatin1String("backward"))
            *searchDirection = Direction::Backward;
        else if (argStr == QLatin1String("forward"))
            *searchDirection = Direction::Forward;
        else if (!StringToSearchType(argStr, searchType)) {
            M_FAILURE(UnrecognizedArgument);
        }
    }
    return true;
}

static bool setCursorPosMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    int pos;

    // Get argument and convert to int
    if(!readArguments(arguments, 0, errMsg, &pos)) {
        return false;
    }

    // Set the position
    TextArea *area = MainWindow::fromDocument(document)->lastFocus();
    area->TextSetCursorPos(pos);
    *result = to_value();
    return true;
}

static bool selectMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    int start;
    int end;

    // Get arguments and convert to int
    if(!readArguments(arguments, 0, errMsg, &start, &end)) {
        return false;
    }

    // Verify integrity of arguments
    if (start > end) {
        std::swap(start, end);
    }

    start = qBound(0, start, document->buffer_->BufGetLength());
    end   = qBound(0, end,   document->buffer_->BufGetLength());

    // Make the selection
    document->buffer_->BufSelect(start, end);
    *result = to_value();
    return true;
}

static bool selectRectangleMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    int start, end, left, right;

    // Get arguments and convert to int
    if(!readArguments(arguments, 0, errMsg, &start, &end, &left, &right)) {
        return false;
    }

    // Make the selection
    document->buffer_->BufRectSelect(start, end, left, right);
    *result = to_value();
    return true;
}

/*
** Macro subroutine to ring the bell
*/
static bool beepMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(document);

    if (!arguments.empty()) {
        M_FAILURE(WrongNumberOfArguments);
    }

    QApplication::beep();
    *result = to_value();
    return true;
}

static bool tPrintMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(document);

    std::string string;

    if (arguments.empty()) {
        M_FAILURE(TooFewArguments);
    }

    for (int i = 0; i < arguments.size(); i++) {
        if (!readArgument(arguments[i], &string, errMsg)) {
            return false;
        }

        printf("%s%s", string.c_str(), i == arguments.size() - 1 ? "" : " ");
    }

    fflush(stdout);
    *result = to_value();
    return true;
}

/*
** Built-in macro subroutine for getting the value of an environment variable
*/
static bool getenvMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(document);

    std::string name;

    // Get name of variable to get
    if(!readArguments(arguments, 0, errMsg, &name)) {
        M_FAILURE(NotAString);
    }

    QByteArray value = qgetenv(name.c_str());

    // Return the text as an allocated string
    *result = to_value(QString::fromLocal8Bit(value));
    return true;
}

static bool shellCmdMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    QString cmdString;
    QString inputString;

    if(!readArguments(arguments, 0, errMsg, &cmdString, &inputString)) {
        return false;
    }

    /* Shell command execution requires that the macro be suspended, so
       this subroutine can't be run if macro execution can't be interrupted */
    if (!MacroRunDocumentEx()->macroCmdData_) {
        M_FAILURE(InvalidContext);
    }

    document->ShellCmdToMacroStringEx(cmdString, inputString);
    *result = to_value(0);
    return true;
}

/*
** Method used by ShellCmdToMacroString (called by shellCmdMS), for returning
** macro string and exit status after the execution of a shell command is
** complete.  (Sorry about the poor modularity here, it's just not worth
** teaching other modules about macro return globals, since other than this,
** they're not used outside of macro.c)
*/
void ReturnShellCommandOutputEx(DocumentWidget *document, const QString &outText, int status) {

    if(const std::shared_ptr<MacroCommandData> &cmdData = document->macroCmdData_) {

        DataValue retVal = to_value(outText);

        ModifyReturnedValueEx(cmdData->context, retVal);

        ReturnGlobals[SHELL_CMD_STATUS]->value = to_value(status);
    }
}

static bool dialogMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    QString btnLabel;
    QString message;
    int i;

    /* Ignore the focused window passed as the function argument and put
       the dialog up over the window which is executing the macro */
    document = MacroRunDocumentEx();
    const std::shared_ptr<MacroCommandData> &cmdData = document->macroCmdData_;

    /* Dialogs require macro to be suspended and interleaved with other macros.
       This subroutine can't be run if macro execution can't be interrupted */
    if (!cmdData) {
        M_FAILURE(InvalidContext);
    }

    /* Read and check the arguments.  The first being the dialog message,
       and the rest being the button labels */
    if (arguments.empty()) {
        M_FAILURE(NeedsArguments);
    }

    if (!readArgument(arguments[0], &message, errMsg)) {
        return false;
    }

    // check that all button labels can be read
    for (i = 1; i < arguments.size(); i++) {
        if (!readArgument(arguments[i], &btnLabel, errMsg)) {
            return false;
        }
    }

    // Stop macro execution until the dialog is complete
    PreemptMacro();

    // Return placeholder result.  Value will be changed by button callback
    *result = to_value(0);

    // NOTE(eteran): passing document here breaks things...
    auto prompt = std::make_unique<DialogPrompt>(nullptr);
    prompt->setMessage(message);
    if (arguments.size() == 1) {
        prompt->addButton(QDialogButtonBox::Ok);
    } else {
        for(int i = 1; i < arguments.size(); ++i) {
            readArgument(arguments[i], &btnLabel, errMsg);
            prompt->addButton(btnLabel);
        }
    }
    prompt->exec();

    result->val.n = prompt->result();
    ModifyReturnedValueEx(cmdData->context, *result);

    document->ResumeMacroExecutionEx();
    return true;
}

static bool stringDialogMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    QString btnLabel;
    QString message;
    long i;

    /* Ignore the focused window passed as the function argument and put
       the dialog up over the window which is executing the macro */
    document = MacroRunDocumentEx();
    const std::shared_ptr<MacroCommandData> &cmdData = document->macroCmdData_;

    /* Dialogs require macro to be suspended and interleaved with other macros.
       This subroutine can't be run if macro execution can't be interrupted */
    if (!cmdData) {
        M_FAILURE(InvalidContext);
    }

    /* Read and check the arguments.  The first being the dialog message,
       and the rest being the button labels */
    if (arguments.empty()) {
        M_FAILURE(NeedsArguments);
    }
    if (!readArgument(arguments[0], &message, errMsg)) {
        return false;
    }

    // check that all button labels can be read
    for (i = 1; i < arguments.size(); i++) {
        if (!readArgument(arguments[i], &btnLabel, errMsg)) {
            return false;
        }
    }

    // Stop macro execution until the dialog is complete
    PreemptMacro();

    // Return placeholder result.  Value will be changed by button callback
    *result = to_value(0);

    // NOTE(eteran): passing document here breaks things...
    auto prompt = std::make_unique<DialogPromptString>(nullptr);
    prompt->setMessage(message);
    if (arguments.size() == 1) {
        prompt->addButton(QDialogButtonBox::Ok);
    } else {
        for(int i = 1; i < arguments.size(); ++i) {
            readArgument(arguments[i], &btnLabel, errMsg);
            prompt->addButton(btnLabel);
        }
    }
    prompt->exec();

    // Return the button number in the global variable $string_dialog_button
    ReturnGlobals[STRING_DIALOG_BUTTON]->value = to_value(prompt->result());

    *result = to_value(prompt->text());
    ModifyReturnedValueEx(cmdData->context, *result);

    document->ResumeMacroExecutionEx();
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
static bool calltipMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    QString tipText;
    std::string txtArg;
    bool anchored = false;
    bool lookup = true;
    int i;
    int anchorPos;
    auto mode      = TagSearchMode::None;
    auto hAlign    = TipHAlignMode::Left;
    auto vAlign    = TipVAlignMode::Below;
    auto alignMode = TipAlignStrict::Sloppy;

    // Read and check the string
    if (arguments.size() < 1) {
        M_FAILURE(TooFewArguments);
    }
    if (arguments.size() > 6) {
        M_FAILURE(TooManyArguments);
    }

    // Read the tip text or key
    if (!readArgument(arguments[0], &tipText, errMsg))
        return false;

    // Read the anchor position (-1 for unanchored)
    if (arguments.size() > 1) {
        if (!readArgument(arguments[1], &anchorPos, errMsg))
            return false;
    } else {
        anchorPos = -1;
    }
    if (anchorPos >= 0)
        anchored = true;

    // Any further args are directives for relative positioning
    for (i = 2; i < arguments.size(); ++i) {
        if (!readArgument(arguments[i], &txtArg, errMsg)) {
            return false;
        }
        switch (txtArg[0]) {
        case 'c':
            if (txtArg == "center")
                M_FAILURE(UnrecognizedArgument);
            hAlign = TipHAlignMode::Center;
            break;
        case 'r':
            if (txtArg == "right")
                M_FAILURE(UnrecognizedArgument);
            hAlign = TipHAlignMode::Right;
            break;
        case 'a':
            if (txtArg == "above")
                M_FAILURE(UnrecognizedArgument);
            vAlign = TipVAlignMode::Above;
            break;
        case 's':
            if (txtArg == "strict")
                M_FAILURE(UnrecognizedArgument);
            alignMode = TipAlignStrict::Strict;
            break;
        case 't':
            if (txtArg == "tipText") {
                mode = TagSearchMode::None;
            } else if (txtArg == "tipKey") {
                mode = TagSearchMode::TIP;
            } else if (txtArg == "tagKey") {
                mode = TagSearchMode::TIP_FROM_TAG;
            } else {
                M_FAILURE(UnrecognizedArgument);
            }
            break;
        default:
            M_FAILURE(UnrecognizedArgument);
        }
    }

    if (mode == TagSearchMode::None) {
        lookup = false;
    }

    // Look up (maybe) a calltip and display it
    *result = to_value(ShowTipStringEx(
                           document,
                           tipText,
                           anchored,
                           anchorPos,
                           lookup,
                           mode,
                           hAlign,
                           vAlign,
                           alignMode));

    return true;
}

/*
** A subroutine to kill the current calltip
*/
static bool killCalltipMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    int calltipID = 0;

    if (arguments.size() > 1) {
        M_FAILURE(TooManyArguments);
    }
    if (arguments.size() > 0) {
        if (!readArgument(arguments[0], &calltipID, errMsg))
            return false;
    }

    MainWindow::fromDocument(document)->lastFocus()->TextDKillCalltip(calltipID);

    *result = to_value();
    return true;
}

/*
 * A subroutine to get the ID of the current calltip, or 0 if there is none.
 */
static bool calltipIDMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(arguments);

    *result = to_value(MainWindow::fromDocument(document)->lastFocus()->TextDGetCalltipID(0));
    return true;
}

static bool replaceAllInSelectionMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    // replace_in_selection( search-string, replace-string [, search-type] )

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunDocumentEx();

    //  Get the argument list.
    QString searchString;
    QString replaceString;

    if(!readArguments(arguments, 0, errMsg, &searchString, &replaceString)) {
        return false;
    }

    SearchType type = searchType(arguments, 2);

    if(auto window = MainWindow::fromDocument(document)) {
        window->action_Replace_In_Selection(document, searchString, replaceString, type);
    }

    *result = to_value(0);
    return true;
}

static bool replaceAllMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    // replace_all( search-string, replace-string [, search-type] )

    // ensure that we are dealing with the document which currently has the focus
    document = MacroRunDocumentEx();

    if (arguments.size() < 2 || arguments.size() > 3) {
        M_FAILURE(WrongNumberOfArguments);
    }

    //  Get the argument list.
    QString searchString;
    QString replaceString;

    if(!readArguments(arguments, 0, errMsg, &searchString, &replaceString)) {
        return false;
    }

    SearchType type = searchType(arguments, 2);

    if(auto window = MainWindow::fromDocument(document)) {
        window->action_Replace_All(document, searchString, replaceString, type);
    }

    *result = to_value(0);
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
static bool filenameDialogMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    QString title           = QLatin1String("Choose Filename");
    QString mode            = QLatin1String("exist");
    QString defaultPath;
    QString defaultFilter;
    QString defaultName;


    /* Ignore the focused window passed as the function argument and put
       the dialog up over the window which is executing the macro */
    document = MacroRunDocumentEx();

    /* Dialogs require macro to be suspended and interleaved with other macros.
       This subroutine can't be run if macro execution can't be interrupted */
    if (!document->macroCmdData_) {
        M_FAILURE(InvalidContext);
    }

    //  Get the argument list.
    if (arguments.size() > 0 && !readArgument(arguments[0], &title, errMsg)) {
        return false;
    }

    if (arguments.size() > 1 && !readArgument(arguments[1], &mode, errMsg)) {
        return false;
    }
    if ((mode != QLatin1String("exist")) != 0 && (mode != QLatin1String("new"))) {
        M_FAILURE(InvalidMode);
    }

    if (arguments.size() > 2 && !readArgument(arguments[2], &defaultPath, errMsg)) {
        return false;
    }

    if (arguments.size() > 3 && !readArgument(arguments[3], &defaultFilter,  errMsg)) {
        return false;
    }

    if (arguments.size() > 4 && !readArgument(arguments[4], &defaultName, errMsg)) {
        return false;
    }

    if (arguments.size() > 5) {
        M_FAILURE(TooManyArguments);
    }

    //  Set default directory
    if (defaultPath.isEmpty()) {
        defaultPath = document->path_;
    }

    //  Set default filter
    if(defaultFilter.isEmpty()) {
        defaultFilter = QLatin1String("*");
    }

    bool gfnResult;
    QString filename;
    if (mode == QLatin1String("exist")) {
        // NOTE(eteran); filters probably don't work quite the same with Qt's dialog
        QString existingFile = QFileDialog::getOpenFileName(document, title, defaultPath, defaultFilter, nullptr);
        if(!existingFile.isNull()) {
            filename = existingFile;
            gfnResult = true;
        } else {
            gfnResult = false;
        }
    } else {
        // NOTE(eteran); filters probably don't work quite the same with Qt's dialog
        QString newFile = QFileDialog::getSaveFileName(document, title, defaultPath, defaultFilter, nullptr);
        if(!newFile.isNull()) {
            filename  = newFile;
            gfnResult = true;
        } else {
            gfnResult = false;
        }
    } //  Invalid values are weeded out above.



    if (gfnResult == true) {
        //  Got a string, copy it to the result
        *result = to_value(filename);
    } else {
        // User cancelled.  Return ""
        *result = to_value(std::string());
    }

    return true;
}

// T Balinski
static bool listDialogMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    QString btnLabel;
    QString message;
    QString text;
    long i;

    /* Ignore the focused window passed as the function argument and put
       the dialog up over the window which is executing the macro */
    document = MacroRunDocumentEx();
    const std::shared_ptr<MacroCommandData> &cmdData = document->macroCmdData_;

    /* Dialogs require macro to be suspended and interleaved with other macros.
       This subroutine can't be run if macro execution can't be interrupted */
    if (!cmdData) {
        M_FAILURE(InvalidContext);
    }

    /* Read and check the arguments.  The first being the dialog message,
       and the rest being the button labels */
    if (arguments.size() < 2) {
        M_FAILURE(TooFewArguments);
    }

    if (!readArgument(arguments[0], &message, errMsg)) {
        return false;
    }

    if (!readArgument(arguments[1], &text, errMsg)) {
        return false;
    }

    if (text.isEmpty()) {
        M_FAILURE(EmptyList);
    }

    // check that all button labels can be read
    for (i = 2; i < arguments.size(); i++) {
        if (!readArgument(arguments[i], &btnLabel, errMsg)) {
            return false;
        }
    }

    // Stop macro execution until the dialog is complete
    PreemptMacro();

    // Return placeholder result.  Value will be changed by button callback
    *result = to_value(0);

    // NOTE(eteran): passing document here breaks things...
    auto prompt = std::make_unique<DialogPromptList>(nullptr);
    prompt->setMessage(message);
    prompt->setList(text);

    if (arguments.size() == 2) {
        prompt->addButton(QDialogButtonBox::Ok);
    } else {
        for(int i = 2; i < arguments.size(); ++i) {
            readArgument(arguments[i], &btnLabel, errMsg);
            prompt->addButton(btnLabel);
        }
    }

    prompt->exec();

    // Return the button number in the global variable $string_dialog_button
    ReturnGlobals[STRING_DIALOG_BUTTON]->value = to_value(prompt->result());

    *result = to_value(prompt->text());
    ModifyReturnedValueEx(cmdData->context, *result);

    document->ResumeMacroExecutionEx();
    return true;
}

static bool stringCompareMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(document);

    QString leftStr;
    QString rightStr;
    QString argStr;
    bool considerCase = true;
    int i;
    int compareResult;

    if (arguments.size() < 2) {
        M_FAILURE(WrongNumberOfArguments);
    }

    if (!readArgument(arguments[0], &leftStr, errMsg))
        return false;

    if (!readArgument(arguments[1], &rightStr, errMsg))
        return false;

    for (i = 2; i < arguments.size(); ++i) {
        if (!readArgument(arguments[i], &argStr, errMsg))
            return false;
        else if (argStr == QLatin1String("case"))
            considerCase = true;
        else if (argStr == QLatin1String("nocase"))
            considerCase = false;
        else {
            M_FAILURE(UnrecognizedArgument);
        }
    }

    if (considerCase) {
        compareResult = leftStr.compare(rightStr, Qt::CaseSensitive);
    } else {
        compareResult = leftStr.compare(rightStr, Qt::CaseInsensitive);
    }

    compareResult = qBound(-1, compareResult, 1);

    *result = to_value(compareResult);
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

static bool splitMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    std::string sourceStr;
    QString splitStr;
    SearchType searchType;
    int foundStart;
    int foundEnd;
    DataValue element;

    if (arguments.size() < 2) {
        M_FAILURE(WrongNumberOfArguments);
    }

    if (!readArgument(arguments[0], &sourceStr, errMsg)) {
        M_FAILURE(Param1NotAString);
    }

    if (!readArgument(arguments[1], &splitStr, errMsg) || splitStr.isEmpty()) {
        M_FAILURE(Param2CannotBeEmptyString);
    }

    QString typeSplitStr;
    if (arguments.size() > 2 && readArgument(arguments[2], &typeSplitStr, errMsg)) {
        if (!StringToSearchType(typeSplitStr, &searchType)) {
            M_FAILURE(UnrecognizedArgument);
        }
    } else {
        searchType = SearchType::Literal;
    }

    *result = to_value(array_new());

    int beginPos  = 0;
    int lastEnd   = 0;
    int indexNum  = 0;
    int strLength = sourceStr.size();
    bool found    = true;
    while (found && beginPos < strLength) {

        auto indexStr      = std::to_string(indexNum);
        auto allocIndexStr = AllocStringCpyEx(indexStr);

        found = SearchString(
                    sourceStr,
                    splitStr,
                    Direction::Forward,
                    searchType,
                    WrapMode::NoWrap,
                    beginPos,
                    &foundStart,
                    &foundEnd,
                    nullptr,
                    nullptr,
                    document->GetWindowDelimitersEx());

        int elementEnd = found ? foundStart : strLength;
        int elementLen = elementEnd - lastEnd;

        std::string str(&sourceStr[lastEnd], elementLen);

        element = to_value(str);
        if (!ArrayInsert(result, allocIndexStr, &element)) {
            M_FAILURE(InsertFailed);
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

        auto indexStr = std::to_string(indexNum);
        auto allocIndexStr = AllocStringCpyEx(indexStr);

        if (lastEnd == strLength) {
            // The pattern mathed the end of the string. Add an empty chunk.
            element = to_value(std::string());

            if (!ArrayInsert(result, allocIndexStr, &element)) {
                M_FAILURE(InsertFailed);
            }
        } else {
            /* We skipped the last character to prevent an endless loop.
               Add it to the list. */
            int elementLen = strLength - lastEnd;
            std::string str(&sourceStr[lastEnd], elementLen);

            element = to_value(str);
            if (!ArrayInsert(result, allocIndexStr, &element)) {
                M_FAILURE(InsertFailed);
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
                        Direction::Forward,
                        searchType,
                        WrapMode::NoWrap,
                        strLength,
                        &foundStart,
                        &foundEnd,
                        nullptr,
                        nullptr,
                        document->GetWindowDelimitersEx());

            if (found) {
                ++indexNum;

                auto indexStr = std::to_string(indexNum);
                auto allocIndexStr = AllocStringCpyEx(indexStr);

                element = to_value();
                if (!ArrayInsert(result, allocIndexStr, &element)) {
                    M_FAILURE(InsertFailed);
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
static bool setBacklightStringMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    QString backlightString;

    if (arguments.empty()) {
        backlightString = GetPrefBacklightCharTypes();
    } else if (arguments.size() == 1) {
        if (!is_string(arguments[0])) {
            M_FAILURE(NotAString);
        }
        backlightString = to_qstring(arguments[0]);
    } else {
        M_FAILURE(WrongNumberOfArguments);
    }

    if (backlightString == QLatin1String("default")) {
        backlightString = GetPrefBacklightCharTypes();
    }

    if (backlightString.isEmpty()) {
        backlightString = QString();  /* turns off backlighting */
    }

    document->SetBacklightChars(backlightString);
    *result = to_value();
    return true;
}
#endif

static bool cursorMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    if(!arguments.empty()) {
        M_FAILURE(TooManyArguments);
    }

    TextArea *area  = MainWindow::fromDocument(document)->lastFocus();
    *result         = to_value(area->TextGetCursorPos());
    return true;
}

static bool lineMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    if(!arguments.empty()) {
        M_FAILURE(TooManyArguments);
    }

    int line;
    int colNum;

    TextBuffer *buf = document->buffer_;
    TextArea *area  = MainWindow::fromDocument(document)->lastFocus();
    int cursorPos   = area->TextGetCursorPos();

    if (!area->TextDPosToLineAndCol(cursorPos, &line, &colNum)) {
        line = buf->BufCountLines(0, cursorPos) + 1;
    }

    *result = to_value(line);
    return true;
}

static bool columnMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    if(!arguments.empty()) {
        M_FAILURE(TooManyArguments);
    }

    TextBuffer *buf = document->buffer_;
    TextArea *area  = MainWindow::fromDocument(document)->lastFocus();
    int cursorPos   = area->TextGetCursorPos();

    *result = to_value(buf->BufCountDispChars(buf->BufStartOfLine(cursorPos), cursorPos));
    return true;
}

static bool fileNameMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    if(!arguments.empty()) {
        M_FAILURE(TooManyArguments);
    }

    *result = to_value(document->filename_);
    return true;
}

static bool filePathMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    if(!arguments.empty()) {
        M_FAILURE(TooManyArguments);
    }

    *result = to_value(document->path_);
    return true;
}

static bool lengthMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    if(!arguments.empty()) {
        M_FAILURE(TooManyArguments);
    }

    *result = to_value(document->buffer_->BufGetLength());
    return true;
}

static bool selectionStartMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    if(!arguments.empty()) {
        M_FAILURE(TooManyArguments);
    }

    *result = to_value(document->buffer_->BufGetPrimary().selected ? document->buffer_->BufGetPrimary().start : -1);
    return true;
}

static bool selectionEndMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    if(!arguments.empty()) {
        M_FAILURE(TooManyArguments);
    }

    *result = to_value(document->buffer_->BufGetPrimary().selected ? document->buffer_->BufGetPrimary().end : -1);
    return true;
}

static bool selectionLeftMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    if(!arguments.empty()) {
        M_FAILURE(TooManyArguments);
    }

    const TextSelection *sel = &document->buffer_->BufGetPrimary();

    *result = to_value(sel->selected && sel->rectangular ? sel->rectStart : -1);
    return true;
}

static bool selectionRightMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    if(!arguments.empty()) {
        M_FAILURE(TooManyArguments);
    }

    const TextSelection *sel = &document->buffer_->BufGetPrimary();

    *result = to_value(sel->selected && sel->rectangular ? sel->rectEnd : -1);
    return true;
}

static bool wrapMarginMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    if(!arguments.empty()) {
        M_FAILURE(TooManyArguments);
    }

    int margin = document->firstPane()->getWrapMargin();
    int nCols  = document->firstPane()->getColumns();

    *result = to_value((margin == 0) ? nCols : margin);
    return true;
}

static bool statisticsLineMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    if(!arguments.empty()) {
        M_FAILURE(TooManyArguments);
    }

    *result = to_value(document->showStats_ ? 1 : 0);
    return true;
}

static bool incSearchLineMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    if(!arguments.empty()) {
        M_FAILURE(TooManyArguments);
    }

    *result = to_value(MainWindow::fromDocument(document)->showISearchLine_ ? 1 : 0);
    return true;
}

static bool showLineNumbersMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    if(!arguments.empty()) {
        M_FAILURE(TooManyArguments);
    }

    *result = to_value(MainWindow::fromDocument(document)->showLineNumbers_ ? 1 : 0);
    return true;
}

static bool autoIndentMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    if(!arguments.empty()) {
        M_FAILURE(TooManyArguments);
    }

    if(document->indentStyle_ == IndentStyle::Default) {
        M_FAILURE(InvalidIndentStyle);
    }

    QLatin1String res = to_string(document->indentStyle_);
    *result = to_value(res);
    return true;
}

static bool wrapTextMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    if(!arguments.empty()) {
        M_FAILURE(TooManyArguments);
    }

    if(document->wrapMode_ == WrapStyle::Default) {
        M_FAILURE(InvalidWrapStyle);
    }

    QLatin1String res = to_string(document->wrapMode_);
    *result = to_value(res);
    return true;
}

static bool highlightSyntaxMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(arguments);

    *result = to_value(document->highlightSyntax_ ? 1 : 0);
    return true;
}

static bool makeBackupCopyMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(arguments);

    *result = to_value(document->saveOldVersion_ ? 1 : 0);
    return true;
}

static bool incBackupMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(arguments);

    *result = to_value(document->autoSave_ ? 1 : 0);
    return true;
}

static bool showMatchingMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(arguments);

    QLatin1String res;

    switch (document->showMatchingStyle_) {
    case ShowMatchingStyle::None:
        res = QLatin1String("off");
        break;
    case ShowMatchingStyle::Delimeter:
        res = QLatin1String("delimiter");
        break;
    case ShowMatchingStyle::Range:
        res = QLatin1String("range");
        break;
    }

    *result = to_value(res);
    return true;
}

static bool matchSyntaxBasedMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(arguments);

    *result = to_value(document->matchSyntaxBased_ ? 1 : 0);
    return true;
}

static bool overTypeModeMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(arguments);

    *result = to_value(document->overstrike_ ? 1 : 0);
    return true;
}

static bool readOnlyMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(arguments);

    *result = to_value((document->lockReasons_.isAnyLocked()) ? 1 : 0);
    return true;
}

static bool lockedMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(arguments);

    *result = to_value((document->lockReasons_.isUserLocked()) ? 1 : 0);
    return true;
}

static bool fileFormatMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(arguments);

    QLatin1String res;

    switch (document->fileFormat_) {
    case FileFormats::Unix:
        res = QLatin1String("unix");
        break;
    case FileFormats::Dos:
        res = QLatin1String("dos");
        break;
    case FileFormats::Mac:
        res = QLatin1String("macintosh");
        break;
    }

    *result = to_value(res);
    return true;
}

static bool fontNameMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    Q_UNUSED(errMsg);
    Q_UNUSED(arguments);

    *result = to_value(document->fontName_);
    return true;
}

static bool fontNameItalicMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    Q_UNUSED(errMsg);
    Q_UNUSED(arguments);

    *result = to_value(document->italicFontName_);
    return true;
}

static bool fontNameBoldMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    Q_UNUSED(errMsg);
    Q_UNUSED(arguments);

    *result = to_value(document->boldFontName_);
    return true;
}

static bool fontNameBoldItalicMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    Q_UNUSED(errMsg);
    Q_UNUSED(arguments);

    *result = to_value(document->boldItalicFontName_);
    return true;
}

static bool subscriptSepMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    Q_UNUSED(document);
    Q_UNUSED(errMsg);
    Q_UNUSED(arguments);

    *result = to_value(view::string_view(ARRAY_DIM_SEP, 1));
    return true;
}

static bool minFontWidthMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    Q_UNUSED(errMsg);
    Q_UNUSED(arguments);

    TextArea *area = document->firstPane();
    *result = to_value(area->TextDMinFontWidth(document->highlightSyntax_));
    return true;
}

static bool maxFontWidthMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    Q_UNUSED(errMsg);
    Q_UNUSED(arguments);

    TextArea *area = document->firstPane();
    *result = to_value(area->TextDMaxFontWidth(document->highlightSyntax_));
    return true;
}

static bool topLineMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    Q_UNUSED(errMsg);
    Q_UNUSED(arguments);

    TextArea *area = MainWindow::fromDocument(document)->lastFocus();
    *result = to_value(area->TextFirstVisibleLine());
    return true;
}

static bool numDisplayLinesMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    Q_UNUSED(errMsg);
    Q_UNUSED(arguments);

    TextArea *area    = MainWindow::fromDocument(document)->lastFocus();
    *result = to_value(area->TextNumVisibleLines());
    return true;
}

static bool displayWidthMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(arguments);

    TextArea *area    = MainWindow::fromDocument(document)->lastFocus();
    *result = to_value(area->TextVisibleWidth());
    return true;
}

static bool activePaneMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(arguments);
    Q_UNUSED(errMsg);

    *result = to_value(document->WidgetToPaneIndex(MainWindow::fromDocument(document)->lastFocus()));
    return true;
}

static bool nPanesMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(arguments);
    Q_UNUSED(errMsg);

    *result = to_value(document->textPanesCount());
    return true;
}

static bool emptyArrayMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(document);
    Q_UNUSED(arguments);
    Q_UNUSED(errMsg);

    *result = to_value(array_empty());
    return true;
}

static bool serverNameMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(document);
    Q_UNUSED(arguments);
    Q_UNUSED(errMsg);

    *result = to_value(GetPrefServerName());
    return true;
}

static bool tabDistMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(arguments);
    Q_UNUSED(errMsg);

    *result = to_value(document->buffer_->BufGetTabDist());
    return true;
}

static bool emTabDistMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(arguments);
    Q_UNUSED(errMsg);

    int dist = document->firstPane()->getEmulateTabs();

    *result = to_value(dist);
    return true;
}

static bool useTabsMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(arguments);
    Q_UNUSED(errMsg);

    *result = to_value(document->buffer_->BufGetUseTabs());
    return true;
}

static bool modifiedMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(arguments);
    Q_UNUSED(errMsg);

    *result = to_value(document->fileChanged_);
    return true;
}

static bool languageModeMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(arguments);
    Q_UNUSED(errMsg);

    QString lmName = LanguageModeName(document->languageMode_);

    if(lmName.isNull()) {
        lmName = QLatin1String("Plain");
    }

    *result = to_value(lmName);
    return true;
}

// --------------------------------------------------------------------------

/*
** Range set macro variables and functions
*/
static bool rangesetListMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(arguments);

    const std::shared_ptr<RangesetTable> &rangesetTable = document->rangesetTable_;
    DataValue element;

    *result = to_value(array_new());

    if(!rangesetTable) {
        return true;
    }

    uint8_t *rangesetList = RangesetTable::RangesetGetList(rangesetTable);
    size_t nRangesets = strlen(reinterpret_cast<char *>(rangesetList));
    for (size_t i = 0; i < nRangesets; i++) {

        element = to_value(rangesetList[i]);

        if (!ArrayInsert(result, AllocStringCpyEx(std::to_string(nRangesets - i - 1)), &element)) {
            M_FAILURE(InsertFailed);
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
static bool versionMV(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Q_UNUSED(errMsg);
    Q_UNUSED(arguments);
    Q_UNUSED(document);

    *result = to_value(NEDIT_VERSION);
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
static bool rangesetCreateMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    int nRangesetsRequired;
    DataValue element;

    std::shared_ptr<RangesetTable> &rangesetTable = document->rangesetTable_;

    if (arguments.size() > 1)
        M_FAILURE(WrongNumberOfArguments);

    if(!rangesetTable) {
        rangesetTable = std::make_shared<RangesetTable>(document->buffer_);
    }

    if (arguments.empty()) {
        int label = rangesetTable->RangesetCreate();
        *result = to_value(label);
        return true;
    } else {
        if (!readArgument(arguments[0], &nRangesetsRequired, errMsg))
            return false;

        *result = to_value(array_new());

        if (nRangesetsRequired > rangesetTable->nRangesetsAvailable())
            return true;

        for (int i = 0; i < nRangesetsRequired; i++) {
            element = to_value(rangesetTable->RangesetCreate());

            ArrayInsert(result, AllocStringCpyEx(std::to_string(i)), &element);
        }

        return true;
    }
}

/*
** Built-in macro subroutine for forgetting a range set.
*/
static bool rangesetDestroyMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    const std::shared_ptr<RangesetTable> &rangesetTable = document->rangesetTable_;
    DataValue element;
    int label = 0;

    if (arguments.size() != 1) {
        M_FAILURE(WrongNumberOfArguments);
    }

    if (is_array(arguments[0])) {
        DataValue *array = &arguments[0];
        int arraySize = ArraySize(array);

        if (arraySize > N_RANGESETS) {
            M_FAILURE(ArrayFull);
        }

        int deleteLabels[N_RANGESETS];

        for (int i = 0; i < arraySize; i++) {

            char keyString[TYPE_INT_STR_SIZE<int>];
            sprintf(keyString, "%d", i);

            if (!ArrayGet(array, keyString, &element)) {
                M_FAILURE(InvalidArrayKey);
            }

            if (!readArgument(element, &label) || !RangesetTable::RangesetLabelOK(label)) {
                M_FAILURE(InvalidRangesetLabelInArray);
            }

            deleteLabels[i] = label;
        }

        for (int i = 0; i < arraySize; i++) {
            rangesetTable->RangesetForget(deleteLabels[i]);
        }
    } else {
        if (!readArgument(arguments[0], &label) || !RangesetTable::RangesetLabelOK(label)) {
            M_FAILURE(InvalidRangesetLabel);
        }

        if (rangesetTable) {
            rangesetTable->RangesetForget(label);
        }
    }

    // set up result
    *result = to_value();
    return true;
}

/*
** Built-in macro subroutine for getting all range sets with a specfic name.
** Arguments are $1: range set name.
** return value is an array indexed 0 to n, with the rangeset labels as values;
*/
static bool rangesetGetByNameMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    Rangeset *rangeset;
    QString name;
    const std::shared_ptr<RangesetTable> &rangesetTable = document->rangesetTable_;
    uint8_t *rangesetList;
    int insertIndex = 0;
    DataValue element;

    if(!readArguments(arguments, 0, errMsg, &name)) {
        M_FAILURE(Param1NotAString);
    }

    *result = to_value(array_new());

    if(!rangesetTable) {
        return true;
    }

    rangesetList = RangesetTable::RangesetGetList(rangesetTable);
    size_t nRangesets = strlen(reinterpret_cast<char *>(rangesetList));
    for (size_t i = 0; i < nRangesets; ++i) {
        int label = rangesetList[i];
        rangeset = rangesetTable->RangesetFetch(label);
        if (rangeset) {
            QString rangeset_name = rangeset->RangesetGetName();

            if(rangeset_name == name) {

                element = to_value(label);

                if (!ArrayInsert(result, AllocStringCpyEx(std::to_string(insertIndex)), &element)) {
                    M_FAILURE(InsertFailed);
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
static bool rangesetAddMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    TextBuffer *buffer = document->buffer_;
    const std::shared_ptr<RangesetTable> &rangesetTable = document->rangesetTable_;
    int start;
    int end;
    int rectStart;
    int rectEnd;
    int index;
    bool isRect;
    int label = 0;

    if (arguments.size() < 1 || arguments.size() > 3)
        M_FAILURE(WrongNumberOfArguments);

    if (!readArgument(arguments[0], &label, errMsg) || !RangesetTable::RangesetLabelOK(label)) {
        M_FAILURE(Param1InvalidRangesetLabel);
    }

    if(!rangesetTable) {
        M_FAILURE(RangesetDoesNotExist);
    }

    Rangeset *targetRangeset = rangesetTable->RangesetFetch(label);

    if(!targetRangeset) {
        M_FAILURE(RangesetDoesNotExist);
    }

    start = end = -1;

    if (arguments.size() == 1) {
        // pick up current selection in this window
        if (!buffer->BufGetSelectionPos(&start, &end, &isRect, &rectStart, &rectEnd) || isRect) {
            M_FAILURE(SelectionMissing);
        }
        if (!targetRangeset->RangesetAddBetween(buffer, start, end)) {
            M_FAILURE(FailedToAddSelection);
        }
    }

    if (arguments.size() == 2) {
        // add ranges taken from a second set
        if (!readArgument(arguments[1], &label, errMsg) || !RangesetTable::RangesetLabelOK(label)) {
            M_FAILURE(Param2InvalidRangesetLabel);
        }

        Rangeset *sourceRangeset = rangesetTable->RangesetFetch(label);
        if(!sourceRangeset) {
            M_FAILURE(Rangeset2DoesNotExist);
        }

        targetRangeset->RangesetAdd(buffer, sourceRangeset);
    }

    if (arguments.size() == 3) {
        // add a range bounded by the start and end positions in $2, $3
        if (!readArgument(arguments[1], &start, errMsg)) {
            return false;
        }
        if (!readArgument(arguments[2], &end, errMsg)) {
            return false;
        }

        // make sure range is in order and fits buffer size
        int maxpos = buffer->BufGetLength();
        start = qBound(0, start, maxpos);
        end   = qBound(0, end, maxpos);

        if (start > end) {
            std::swap(start, end);
        }

        if ((start != end) && !targetRangeset->RangesetAddBetween(buffer, start, end)) {
            M_FAILURE(FailedToAddRange);
        }
    }

    // (to) which range did we just add?
    if (arguments.size() != 2 && start >= 0) {
        start = (start + end) / 2; // "middle" of added range
        index = 1 + targetRangeset->RangesetFindRangeOfPos(start, false);
    } else {
        index = 0;
    }

    // set up result
    *result = to_value(index);
    return true;
}

/*
** Built-in macro subroutine for removing from a range set. Almost identical to
** rangesetAddMS() - only changes are from RangesetAdd()/RangesetAddBetween()
** to RangesetSubtract()/RangesetSubtractBetween(), the handling of an
** undefined destination range, and that it returns no value.
*/
static bool rangesetSubtractMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    TextBuffer *buffer = document->buffer_;
    const std::shared_ptr<RangesetTable> &rangesetTable = document->rangesetTable_;
    int start, end, rectStart, rectEnd;
    bool isRect;
    int label = 0;

    if (arguments.size() < 1 || arguments.size() > 3) {
        M_FAILURE(WrongNumberOfArguments);
    }

    if (!readArgument(arguments[0], &label, errMsg) || !RangesetTable::RangesetLabelOK(label)) {
        M_FAILURE(Param1InvalidRangesetLabel);
    }

    if(!rangesetTable) {
        M_FAILURE(RangesetDoesNotExist);
    }

    Rangeset *targetRangeset = rangesetTable->RangesetFetch(label);
    if(!targetRangeset) {
        M_FAILURE(RangesetDoesNotExist);
    }

    if (arguments.size() == 1) {
        // remove current selection in this window
        if (!buffer->BufGetSelectionPos(&start, &end, &isRect, &rectStart, &rectEnd) || isRect) {
            M_FAILURE(SelectionMissing);
        }
        targetRangeset->RangesetRemoveBetween(buffer, start, end);
    }

    if (arguments.size() == 2) {
        // remove ranges taken from a second set
        if (!readArgument(arguments[1], &label, errMsg) || !RangesetTable::RangesetLabelOK(label)) {
            M_FAILURE(Param2InvalidRangesetLabel);
        }

        Rangeset *sourceRangeset = rangesetTable->RangesetFetch(label);
        if(!sourceRangeset) {
            M_FAILURE(Rangeset2DoesNotExist);
        }
        targetRangeset->RangesetRemove(buffer, sourceRangeset);
    }

    if (arguments.size() == 3) {
        // remove a range bounded by the start and end positions in $2, $3
        if (!readArgument(arguments[1], &start, errMsg))
            return false;
        if (!readArgument(arguments[2], &end, errMsg))
            return false;

        // make sure range is in order and fits buffer size
        int maxpos = buffer->BufGetLength();
        start = qBound(0, start, maxpos);
        end   = qBound(0, end, maxpos);

        if (start > end) {
            std::swap(start, end);
        }

        targetRangeset->RangesetRemoveBetween(buffer, start, end);
    }

    // set up result
    *result = to_value();
    return true;
}

/*
** Built-in macro subroutine to invert a range set. Argument is $1: range set
** label (one alphabetic character). Returns nothing. Fails if range set
** undefined.
*/
static bool rangesetInvertMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    TextBuffer *buffer = document->buffer_;
    const std::shared_ptr<RangesetTable> &rangesetTable = document->rangesetTable_;
    int label;

    if(!readArguments(arguments, 0, errMsg, &label)) {
        return false;
    }

    if (!RangesetTable::RangesetLabelOK(label)) {
        M_FAILURE(Param1InvalidRangesetLabel);
    }

    if(!rangesetTable) {
        M_FAILURE(RangesetDoesNotExist);
    }

    Rangeset *rangeset = rangesetTable->RangesetFetch(label);
    if(!rangeset) {
        M_FAILURE(RangesetDoesNotExist);
    }

    if (rangeset->RangesetInverse(buffer) < 0) {
        M_FAILURE(FailedToInvertRangeset);
    }

    // set up result
    *result = to_value();
    return true;
}

/*
** Built-in macro subroutine for finding out info about a rangeset.  Takes one
** argument of a rangeset label.  Returns an array with the following keys:
**    defined, count, color, mode.
*/
static bool rangesetInfoMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    const std::shared_ptr<RangesetTable> &rangesetTable = document->rangesetTable_;
    Rangeset *rangeset = nullptr;
    DataValue element;
    int label;

    if(!readArguments(arguments, 0, errMsg, &label)) {
        return false;
    }

    if (!RangesetTable::RangesetLabelOK(label)) {
        M_FAILURE(Param1InvalidRangesetLabel);
    }

    if (rangesetTable) {
        rangeset = rangesetTable->RangesetFetch(label);
    }

    int count;
    bool defined;
    QString color;
    QString name;
    QString mode;

    if(rangeset) {
        rangeset->RangesetGetInfo(&defined, &label, &count, &color, &name, &mode);
    } else {
        defined = false;
        label = 0;
        count = 0;
    }

    // set up result
    *result = to_value(array_new());

    element = to_value(defined);
    if (!ArrayInsert(result, AllocStringCpyEx("defined"), &element))
        M_FAILURE(InsertFailed);

    element = to_value(count);
    if (!ArrayInsert(result, AllocStringCpyEx("count"), &element))
        M_FAILURE(InsertFailed);

    element = to_value(color);
    if (!ArrayInsert(result, AllocStringCpyEx("color"), &element))
        M_FAILURE(InsertFailed);

    element = to_value(name);
    if (!ArrayInsert(result, AllocStringCpyEx("name"), &element)) {
        M_FAILURE(InsertFailed);
    }

    element = to_value(mode);
    if (!ArrayInsert(result, AllocStringCpyEx("mode"), &element))
        M_FAILURE(InsertFailed);

    return true;
}

/*
** Built-in macro subroutine for finding the extent of a range in a set.
** If only one parameter is supplied, use the spanning range of all
** ranges, otherwise select the individual range specified.  Returns
** an array with the keys "start" and "end" and values
*/
static bool rangesetRangeMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    const std::shared_ptr<RangesetTable> &rangesetTable = document->rangesetTable_;
    Rangeset *rangeset;
    int start = 0;
    int end = 0;
    int dummy;
    int rangeIndex;
    DataValue element;
    int label = 0;

    if (arguments.size() < 1 || arguments.size() > 2) {
        M_FAILURE(WrongNumberOfArguments);
    }

    if (!readArgument(arguments[0], &label, errMsg) || !RangesetTable::RangesetLabelOK(label)) {
        M_FAILURE(Param1InvalidRangesetLabel);
    }

    if(!rangesetTable) {
        M_FAILURE(RangesetDoesNotExist);
    }

    bool ok = false;
    rangeset = rangesetTable->RangesetFetch(label);
    if (rangeset) {
        if (arguments.size() == 1) {
            rangeIndex = rangeset->RangesetGetNRanges() - 1;
            ok  = rangeset->RangesetFindRangeNo(0, &start, &dummy);
            ok &= rangeset->RangesetFindRangeNo(rangeIndex, &dummy, &end);
            rangeIndex = -1;
        } else if (arguments.size() == 2) {
            if (!readArgument(arguments[1], &rangeIndex, errMsg)) {
                return false;
            }
            ok = rangeset->RangesetFindRangeNo(rangeIndex - 1, &start, &end);
        }
    }

    *result = to_value(array_new());

    if (!ok)
        return true;

    element = to_value(start);
    if (!ArrayInsert(result, AllocStringCpyEx("start"), &element))
        M_FAILURE(InsertFailed);

    element = to_value(end);
    if (!ArrayInsert(result, AllocStringCpyEx("end"), &element))
        M_FAILURE(InsertFailed);

    return true;
}

/*
** Built-in macro subroutine for checking a position against a range. If only
** one parameter is supplied, the current cursor position is used. Returns
** false (zero) if not in a range, range index (1-based) if in a range;
** fails if parameters were bad.
*/
static bool rangesetIncludesPosMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    TextBuffer *buffer = document->buffer_;
    const std::shared_ptr<RangesetTable> &rangesetTable = document->rangesetTable_;
    int rangeIndex;
    int label = 0;

    if (arguments.size() < 1 || arguments.size() > 2) {
        M_FAILURE(WrongNumberOfArguments);
    }

    if (!readArgument(arguments[0], &label, errMsg) || !RangesetTable::RangesetLabelOK(label)) {
        M_FAILURE(Param1InvalidRangesetLabel);
    }

    if(!rangesetTable) {
        M_FAILURE(RangesetDoesNotExist);
    }

    Rangeset *rangeset = rangesetTable->RangesetFetch(label);
    if(!rangeset) {
        M_FAILURE(RangesetDoesNotExist);
    }

    int pos = 0;
    if (arguments.size() == 1) {
        TextArea *area = MainWindow::fromDocument(document)->lastFocus();
        pos = area->TextGetCursorPos();
    } else if (arguments.size() == 2) {
        if (!readArgument(arguments[1], &pos, errMsg)) {
            return false;
        }
    }

    int maxpos = buffer->BufGetLength();
    if (pos < 0 || pos > maxpos) {
        rangeIndex = 0;
    } else {
        rangeIndex = rangeset->RangesetFindRangeOfPos(pos, false) + 1;
    }

    // set up result
    *result = to_value(rangeIndex);
    return true;
}

/*
** Set the color of a range set's ranges. it is ignored if the color cannot be
** found/applied. If no color is applied, any current color is removed. Returns
** true if the rangeset is valid.
*/
static bool rangesetSetColorMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    TextBuffer *buffer = document->buffer_;
    const std::shared_ptr<RangesetTable> &rangesetTable = document->rangesetTable_;
    int label = 0;

    if (arguments.size() != 2) {
        M_FAILURE(WrongNumberOfArguments);
    }

    if (!readArgument(arguments[0], &label, errMsg) || !RangesetTable::RangesetLabelOK(label)) {
        M_FAILURE(Param1InvalidRangesetLabel);
    }

    if(!rangesetTable) {
        M_FAILURE(RangesetDoesNotExist);
    }

    Rangeset *rangeset = rangesetTable->RangesetFetch(label);
    if(!rangeset) {
        M_FAILURE(RangesetDoesNotExist);
    }

    QString color_name;
    if (!readArgument(arguments[1], &color_name, errMsg)) {
        M_FAILURE(Param2NotAString);
    }

    rangeset->RangesetAssignColorName(buffer, color_name);

    // set up result
    *result = to_value();
    return true;
}

/*
** Set the name of a range set's ranges. Returns
** true if the rangeset is valid.
*/
static bool rangesetSetNameMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    const std::shared_ptr<RangesetTable> &rangesetTable = document->rangesetTable_;
    int label = 0;

    if (arguments.size() != 2) {
        M_FAILURE(WrongNumberOfArguments);
    }

    if (!readArgument(arguments[0], &label, errMsg) || !RangesetTable::RangesetLabelOK(label)) {
        M_FAILURE(Param1InvalidRangesetLabel);
    }

    if(!rangesetTable) {
        M_FAILURE(RangesetDoesNotExist);
    }

    Rangeset *rangeset = rangesetTable->RangesetFetch(label);
    if(!rangeset) {
        M_FAILURE(RangesetDoesNotExist);
    }

    QString name;
    if (!readArgument(arguments[1], &name, errMsg)) {
        M_FAILURE(Param2NotAString);
    }

    rangeset->RangesetAssignName(name);

    // set up result
    *result = to_value();
    return true;
}

/*
** Change a range's modification response. Returns true if the rangeset is
** valid and the response type name is valid.
*/
static bool rangesetSetModeMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    const std::shared_ptr<RangesetTable> &rangesetTable = document->rangesetTable_;
    Rangeset *rangeset;
    int label = 0;

    if (arguments.size() < 1 || arguments.size() > 2) {
        M_FAILURE(WrongNumberOfArguments);
    }

    if (!readArgument(arguments[0], &label, errMsg) || !RangesetTable::RangesetLabelOK(label)) {
        M_FAILURE(Param1InvalidRangesetLabel);
    }

    if(!rangesetTable) {
        M_FAILURE(RangesetDoesNotExist);
    }

    rangeset = rangesetTable->RangesetFetch(label);
    if(!rangeset) {
        M_FAILURE(RangesetDoesNotExist);
    }

    QString update_fn_name;
    if (arguments.size() == 2) {
        if (!readArgument(arguments[1], &update_fn_name,  errMsg)) {
            M_FAILURE(Param2NotAString);
        }
    }

    bool ok = rangeset->RangesetChangeModifyResponse(update_fn_name);
    if (!ok) {
        M_FAILURE(InvalidMode);
    }

    // set up result
    *result = to_value();
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
static bool fillStyleResultEx(DataValue *result, const char **errMsg, DocumentWidget *document, const QString &styleName, bool includeName, int patCode, int bufferPos) {
    DataValue DV;

    *result = to_value(array_new());

    if (includeName) {

        // insert style name
        DV = to_value(styleName);

        if (!ArrayInsert(result, AllocStringCpyEx("style"), &DV)) {
            M_FAILURE(InsertFailed);
        }
    }

    // insert color name
    DV = to_value(ColorOfNamedStyleEx(styleName));
    if (!ArrayInsert(result, AllocStringCpyEx("color"), &DV)) {
        M_FAILURE(InsertFailed);
    }

    /* Prepare array element for color value
       (only possible if we pass through the dynamic highlight pattern tables
       in other words, only if we have a pattern code) */
    if (patCode) {
        QColor color = document->HighlightColorValueOfCodeEx(patCode);
        DV = to_value(color.name());

        if (!ArrayInsert(result, AllocStringCpyEx("rgb"), &DV)) {
            M_FAILURE(InsertFailed);
        }
    }

    // Prepare array element for background color name
    DV = to_value(BgColorOfNamedStyleEx(styleName));
    if (!ArrayInsert(result, AllocStringCpyEx("background"), &DV)) {
        M_FAILURE(InsertFailed);
    }

    /* Prepare array element for background color value
       (only possible if we pass through the dynamic highlight pattern tables
       in other words, only if we have a pattern code) */
    if (patCode) {
        QColor color = document->GetHighlightBGColorOfCodeEx(patCode);
        DV = to_value(color.name());

        if (!ArrayInsert(result, AllocStringCpyEx("back_rgb"), &DV)) {
            M_FAILURE(InsertFailed);
        }
    }

    // Put boldness value in array
    DV = to_value(FontOfNamedStyleIsBold(styleName));
    if (!ArrayInsert(result, AllocStringCpyEx("bold"), &DV)) {
        M_FAILURE(InsertFailed);
    }

    // Put italicity value in array
    DV = to_value(FontOfNamedStyleIsItalic(styleName));
    if (!ArrayInsert(result, AllocStringCpyEx("italic"), &DV)) {
        M_FAILURE(InsertFailed);
    }

    if (bufferPos >= 0) {
        // insert extent
        DV = to_value(document->StyleLengthOfCodeFromPosEx(bufferPos));
        if (!ArrayInsert(result, AllocStringCpyEx("extent"), &DV)) {
            M_FAILURE(InsertFailed);
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
static bool getStyleByNameMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    QString styleName;

    // Validate number of arguments
    if(!readArguments(arguments, 0, errMsg, &styleName)) {
        M_FAILURE(Param1NotAString);
    }

    *result = to_value(array_empty());

    if (!NamedStyleExists(styleName)) {
        // if the given name is invalid we just return an empty array.
        return true;
    }

    return fillStyleResultEx(
                result,
                errMsg,
                document,
                styleName,
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
static bool getStyleAtPosMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    int bufferPos;
    TextBuffer *buf = document->buffer_;

    // Validate number of arguments
    if(!readArguments(arguments, 0, errMsg, &bufferPos)) {
        return false;
    }

    *result = to_value(array_empty());

    //  Verify sane buffer position
    if ((bufferPos < 0) || (bufferPos >= buf->BufGetLength())) {
        /*  If the position is not legal, we cannot guess anything about
            the style, so we return an empty array. */
        return true;
    }

    // Determine pattern code
    int patCode = document->HighlightCodeOfPosEx(bufferPos);
    if (patCode == 0) {
        // if there is no pattern we just return an empty array.
        return true;
    }

    return fillStyleResultEx(
        result,
        errMsg,
        document,
        document->HighlightStyleOfCodeEx(patCode),
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
bool fillPatternResultEx(DataValue *result, const char **errMsg, DocumentWidget *document, const QString &patternName, bool includeName, const QString &styleName, int bufferPos) {

    Q_UNUSED(errMsg);

    DataValue DV;

    *result = to_value(array_new());

    // the following array entries will be strings

    if (includeName) {
        // insert pattern name
        DV = to_value(patternName);
        if (!ArrayInsert(result, AllocStringCpyEx("pattern"), &DV)) {
            M_FAILURE(InsertFailed);
        }
    }

    // insert style name
    DV = to_value(styleName);
    if (!ArrayInsert(result, AllocStringCpyEx("style"), &DV)) {
        M_FAILURE(InsertFailed);
    }

    if (bufferPos >= 0) {
        // insert extent
        int checkCode = 0;
        DV = to_value(document->HighlightLengthOfCodeFromPosEx(bufferPos, &checkCode));
        if (!ArrayInsert(result, AllocStringCpyEx("extent"), &DV)) {
            M_FAILURE(InsertFailed);
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
static bool getPatternByNameMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {

    QString patternName;

    *result = to_value(array_empty());

    // Validate number of arguments
    if(!readArguments(arguments, 0, errMsg, &patternName)) {
        M_FAILURE(Param1NotAString);
    }

    HighlightPattern *pattern = document->FindPatternOfWindowEx(patternName);
    if(!pattern) {
        // The pattern's name is unknown.
        return true;
    }

    return fillPatternResultEx(
                result,
                errMsg,
                document,
                patternName,
                false,
                pattern->style,
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
static bool getPatternAtPosMS(DocumentWidget *document, Arguments arguments, DataValue *result, const char **errMsg) {
    int bufferPos;
    TextBuffer *buffer = document->buffer_;

    *result = to_value(array_empty());

    // Validate number of arguments
    if(!readArguments(arguments, 0, errMsg, &bufferPos)) {
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
    int patCode = document->HighlightCodeOfPosEx(bufferPos);
    if (patCode == 0) {
        // if there is no highlighting pattern we just return an empty array.
        return true;
    }

    return fillPatternResultEx(
        result,
        errMsg,
        document,
        document->HighlightNameOfCodeEx(patCode),
        true,
        document->HighlightStyleOfCodeEx(patCode),
        bufferPos);
}

/*
** Get an integer value from a tagged DataValue structure.  Return True
** if conversion succeeded, and store result in *result, otherwise
** return False with an error message in *errMsg.
*/
static bool readArgument(const DataValue &dv, int *result, const char **errMsg) {

    if(is_integer(dv)) {
        *result = to_integer(dv);
        return true;
    }

    if(is_string(dv)) {
        auto s = to_qstring(dv);
        bool ok;
        int val = s.toInt(&ok);
        if(!ok) {
           M_FAILURE(NotAnInteger);
        }

        *result = val;
        return true;
    }

    M_FAILURE(UnknownObject);

}

/*
** Get an string value from a tagged DataValue structure.  Return True
** if conversion succeeded, and store result in *result, otherwise
** return false with an error message in *errMsg.  If an integer value
** is converted, write the string in the space provided by "stringStorage",
** which must be large enough to handle ints of the maximum size.
*/
static bool readArgument(const DataValue &dv, std::string *result, const char **errMsg) {

    if(is_string(dv)) {
        *result = to_string(dv).to_string();
        return true;
    }

    if(is_integer(dv)) {
        *result = std::to_string(to_integer(dv));
        return true;
    }

    M_FAILURE(UnknownObject);
}

static bool readArgument(const DataValue &dv, QString *result, const char **errMsg) {

    if(is_string(dv)) {
        *result = to_qstring(dv);
        return true;
    }

    if(is_integer(dv)) {
        *result = QString::number(to_integer(dv));
        return true;
    }

    M_FAILURE(UnknownObject);
}


template <class T, class ...Ts>
bool readArguments(Arguments arguments, int index, const char **errMsg, T arg, Ts...args) {

    static_assert(std::is_pointer<T>::value, "Argument is not a pointer");

    if(static_cast<size_t>(arguments.size() - index) < (sizeof...(args)) + 1) {
        M_FAILURE(WrongNumberOfArguments);
    }

    bool ret = readArgument(arguments[index], arg, errMsg);
    if(!ret) {
        return false;
    }

    return readArguments(arguments, index + 1, errMsg, args...);
}

template <class T>
bool readArguments(Arguments arguments, int index, const char **errMsg, T arg) {

    static_assert(std::is_pointer<T>::value, "Argument is not a pointer");

    if(static_cast<size_t>(arguments.size() - index) < 1) {
        M_FAILURE(WrongNumberOfArguments);
    }

    return readArgument(arguments[index], arg, errMsg);
}
