
#include "macro.h"
#include "CommandRecorder.h"
#include "DialogPrompt.h"
#include "DialogPromptList.h"
#include "DialogPromptString.h"
#include "Direction.h"
#include "DocumentWidget.h"
#include "Highlight.h"
#include "HighlightPattern.h"
#include "MainWindow.h"
#include "Preferences.h"
#include "RangesetTable.h"
#include "Search.h"
#include "SearchType.h"
#include "SignalBlocker.h"
#include "SmartIndent.h"
#include "TextArea.h"
#include "TextBuffer.h"
#include "WrapMode.h"
#include "interpret.h"
#include "parse.h"
#include "Util/FileSystem.h"
#include "Util/Input.h"
#include "Util/utils.h"
#include "Util/version.h"

#include <boost/optional.hpp>
#include <stack>
#include <fstream>

#include <QClipboard>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QMimeData>

// DISABLED for 5.4
//#define ENABLE_BACKLIGHT_STRING

static std::error_code lengthMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code minMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code maxMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code focusWindowMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code getRangeMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code getCharacterMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code replaceRangeMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code replaceSelectionMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code getSelectionMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code validNumberMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code replaceInStringMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code replaceSubstringMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code readFileMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code writeFileMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code appendFileMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code writeOrAppendFile(bool append, DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code substringMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code toupperMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code tolowerMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code stringToClipboardMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code clipboardToStringMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code searchMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code searchStringMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code setCursorPosMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code beepMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code selectMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code selectRectangleMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code tPrintMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code getenvMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code shellCmdMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code dialogMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code stringDialogMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code calltipMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code killCalltipMS(DocumentWidget *document, Arguments arguments, DataValue *result);

// T Balinski
static std::error_code listDialogMS(DocumentWidget *document, Arguments arguments, DataValue *result);
// T Balinski End

static std::error_code stringCompareMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code splitMS(DocumentWidget *document, Arguments arguments, DataValue *result);
#if defined(ENABLE_BACKLIGHT_STRING)
static std::error_code setBacklightStringMS(DocumentWidget *document, Arguments arguments, DataValue *result);
#endif
static std::error_code cursorMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code lineMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code columnMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code fileNameMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code filePathMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code lengthMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code selectionStartMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code selectionEndMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code selectionLeftMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code selectionRightMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code statisticsLineMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code incSearchLineMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code showLineNumbersMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code autoIndentMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code wrapTextMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code highlightSyntaxMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code makeBackupCopyMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code incBackupMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code showMatchingMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code matchSyntaxBasedMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code overTypeModeMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code readOnlyMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code lockedMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code fileFormatMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code fontNameMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code fontNameItalicMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code fontNameBoldMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code fontNameBoldItalicMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code subscriptSepMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code minFontWidthMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code maxFontWidthMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code wrapMarginMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code topLineMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code numDisplayLinesMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code displayWidthMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code activePaneMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code nPanesMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code emptyArrayMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code serverNameMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code tabDistMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code emTabDistMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code useTabsMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code modifiedMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code languageModeMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code calltipIDMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code rangesetListMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code versionMV(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code rangesetCreateMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code rangesetDestroyMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code rangesetGetByNameMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code rangesetAddMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code rangesetSubtractMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code rangesetInvertMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code rangesetInfoMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code rangesetRangeMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code rangesetIncludesPosMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code rangesetSetColorMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code rangesetSetNameMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code rangesetSetModeMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code getPatternByNameMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code getPatternAtPosMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code getStyleByNameMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code getStyleAtPosMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code filenameDialogMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code replaceAllInSelectionMS(DocumentWidget *document, Arguments arguments, DataValue *result);
static std::error_code replaceAllMS(DocumentWidget *document, Arguments arguments, DataValue *result);

static std::error_code fillPatternResult(DataValue *result, DocumentWidget *document, const QString &patternName, bool includeName, const QString &styleName, TextCursor bufferPos);
static std::error_code fillStyleResult(DataValue *result, DocumentWidget *document, const QString &styleName, bool includeName, size_t patCode, TextCursor bufferPos);

static std::error_code readSearchArgs(Arguments arguments, Direction *searchDirection, SearchType *searchType, WrapMode *wrap);
static std::error_code readArgument(const DataValue &dv, int *result);
static std::error_code readArgument(const DataValue &dv, int64_t *result);
static std::error_code readArgument(const DataValue &dv, std::string *result);
static std::error_code readArgument(const DataValue &dv, QString *result);

template <class T, class ...Ts>
std::error_code readArguments(Arguments arguments, int index, T arg, Ts...args);

template <class T>
std::error_code readArguments(Arguments arguments, int index, T arg);

struct SubRoutine {
	const char *name;
	LibraryRoutine function;
};

namespace {

enum class MacroErrorCode {
	Success = 0,
	ArrayFull,
	InvalidMode,
	InvalidArgument,
	TooFewArguments,
	TooManyArguments,
	UnknownObject,
	NotAnInteger,
	NotAString,
	InvalidContext,
	NeedsArguments,
	WrongNumberOfArguments,
	UnrecognizedArgument,
	InsertFailed,
	RangesetDoesNotExist,
	Rangeset2DoesNotExist,
	PathnameTooLong,
	InvalidArrayKey,
	InvalidIndentStyle,
	InvalidWrapStyle,
	EmptyList,
	Param1InvalidRangesetLabel,
	Param2InvalidRangesetLabel,
	InvalidRangesetLabel,
	InvalidRangesetLabelInArray,
	Param1NotAString,
	Param2NotAString,
	SelectionMissing,
	FailedToAddRange,
	FailedToInvertRangeset,
	FailedToAddSelection,
	Param2CannotBeEmptyString,
	InvalidSearchReplaceArgs,
	InvalidRepeatArg,
	ReadOnly,
};

struct MacroErrorCategory : std::error_category {
	const char* name() const noexcept override { return "macro_error"; }
	std::string message(int ev) const override;
};

std::string MacroErrorCategory::message(int ev) const {
	switch (static_cast<MacroErrorCode>(ev)) {
	case MacroErrorCode::Success:                     return "";
	case MacroErrorCode::ArrayFull:                   return "Too many elements in array in %s";
	case MacroErrorCode::InvalidMode:                 return "Invalid value for mode in %s";
	case MacroErrorCode::InvalidArgument:             return "%s called with invalid argument";
	case MacroErrorCode::TooFewArguments:             return "%s subroutine called with too few arguments";
	case MacroErrorCode::TooManyArguments:            return "%s subroutine called with too many arguments";
	case MacroErrorCode::UnknownObject:               return "%s called with unknown object";
	case MacroErrorCode::NotAnInteger:                return "%s called with non-integer argument";
	case MacroErrorCode::NotAString:                  return "%s not called with a string parameter";
	case MacroErrorCode::InvalidContext:              return "%s can't be called from non-suspendable context";
	case MacroErrorCode::NeedsArguments:              return "%s subroutine called with no arguments";
	case MacroErrorCode::WrongNumberOfArguments:      return "Wrong number of arguments to function %s";
	case MacroErrorCode::UnrecognizedArgument:        return "Unrecognized argument to %s";
	case MacroErrorCode::InsertFailed:                return "Array element failed to insert: %s";
	case MacroErrorCode::RangesetDoesNotExist:        return "Rangeset does not exist in %s";
	case MacroErrorCode::Rangeset2DoesNotExist:       return "Second rangeset does not exist in %s";
	case MacroErrorCode::PathnameTooLong:             return "Pathname too long in %s";
	case MacroErrorCode::InvalidArrayKey:             return "Invalid key in array in %s";
	case MacroErrorCode::InvalidIndentStyle:          return "Invalid indent style value encountered in %s";
	case MacroErrorCode::InvalidWrapStyle:            return "Invalid wrap style value encountered in %s";
	case MacroErrorCode::EmptyList:                   return "%s subroutine called with empty list data";
	case MacroErrorCode::Param1InvalidRangesetLabel:  return "First parameter is an invalid rangeset label in %s";
	case MacroErrorCode::Param2InvalidRangesetLabel:  return "Second parameter is an invalid rangeset label in %s";
	case MacroErrorCode::InvalidRangesetLabel:        return "Invalid rangeset label in %s";
	case MacroErrorCode::InvalidRangesetLabelInArray: return "Invalid rangeset label in array in %s";
	case MacroErrorCode::Param1NotAString:            return "First parameter is not a string in %s";
	case MacroErrorCode::Param2NotAString:            return "Second parameter is not a string in %s";
	case MacroErrorCode::SelectionMissing:            return "Selection missing or rectangular in call to %s";
	case MacroErrorCode::FailedToAddRange:            return "Failed to add range in %s";
	case MacroErrorCode::FailedToInvertRangeset:      return "Problem inverting rangeset in %s";
	case MacroErrorCode::FailedToAddSelection:        return "Failure to add selection in %s";
	case MacroErrorCode::Param2CannotBeEmptyString:   return "Second argument must be a non-empty string: %s";
	case MacroErrorCode::InvalidSearchReplaceArgs:    return "%s action requires search and replace string arguments";
	case MacroErrorCode::InvalidRepeatArg:            return "%s requires method/count";
	case MacroErrorCode::ReadOnly:                    return "%s cannot alter a read-only document";
	}

	Q_UNREACHABLE();
}


std::error_code make_error_code(MacroErrorCode e) {
	// NOTE(eteran): error_code uses the address of this,
	// so it needs global lifetime
	static MacroErrorCategory category;
	return { static_cast<int>(e), category };
}

}

namespace std {

template <>
struct is_error_code_enum<MacroErrorCode> : true_type {};

}


/**
 * @brief flagsFromArguments
 * @param arguments
 * @param firstFlag
 * @return The flags if all arguments were valid, otherwise, nothing
 */
static boost::optional<TextArea::EventFlags> flagsFromArguments(Arguments arguments, int firstFlag) {

	TextArea::EventFlags f = TextArea::NoneFlag;
	for(int i = firstFlag; i < arguments.size(); ++i) {

		const std::string s = to_string(arguments[i]);

		if(s == "absolute") {
			f |= TextArea::AbsoluteFlag;
		} else if(s == "column") {
			f |= TextArea::ColumnFlag;
		} else if(s == "copy") {
			f |= TextArea::CopyFlag;
		} else if(s == "down") {
			f |= TextArea::DownFlag;
		} else if(s == "extend") {
			f |= TextArea::ExtendFlag;
		} else if(s == "left") {
			f |= TextArea::LeftFlag;
		} else if(s == "overlay") {
			f |= TextArea::OverlayFlag;
		} else if(s == "rect") {
			f |= TextArea::RectFlag;
		} else if(s == "right") {
			f |= TextArea::RightFlag;
		} else if(s == "up") {
			f |= TextArea::UpFlag;
		} else if(s == "wrap") {
			f |= TextArea::WrapFlag;
		} else if(s == "tail") {
			f |= TextArea::TailFlag;
		} else if(s == "stutter") {
			f |= TextArea::StutterFlag;
		} else if(s == "scrollbar") {
			f |= TextArea::ScrollbarFlag;
		} else if(s == "nobell") {
			f |= TextArea::NoBellFlag;
		} else {
			return boost::none;
		}
	}

	return f;
}

template <void(TextArea::*Func)(TextArea::EventFlags)>
std::error_code textEvent(DocumentWidget *document, Arguments arguments, DataValue *result) {

	boost::optional<TextArea::EventFlags> flags = flagsFromArguments(arguments, 0);
	if(!flags) {
		return MacroErrorCode::InvalidArgument;
	}

	if(auto window = MainWindow::fromDocument(document)) {
		if(TextArea *area = window->lastFocus()) {
			(area->*Func)(*flags | TextArea::SupressRecording);
		}
	}

	*result = make_value();
	return MacroErrorCode::Success;
}

template <class T, void(TextArea::*Func)(T, TextArea::EventFlags)>
std::error_code textEventArg(DocumentWidget *document, Arguments arguments, DataValue *result) {

	if(arguments.size() < 1) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	typename std::decay<T>::type argument;
	if(std::error_code ec = readArgument(arguments[0], &argument)) {
		return ec;
	}

	boost::optional<TextArea::EventFlags> flags = flagsFromArguments(arguments, 1);
	if(!flags) {
		return MacroErrorCode::InvalidArgument;
	}

	if(auto window = MainWindow::fromDocument(document)) {
		if(TextArea *area = window->lastFocus()) {
			(area->*Func)(argument, *flags | TextArea::SupressRecording);
		}
	}

	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code scrollDownMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	int count;
	QString unitsString = QLatin1String("line");
	switch(arguments.size()) {
	case 2:
		if(std::error_code ec = readArguments(arguments, 0, &count, &unitsString)) {
			return ec;
		}
		break;
	case 1:
		if(std::error_code ec = readArguments(arguments, 0, &count)) {
			return ec;
		}
		break;
	default:
		return MacroErrorCode::WrongNumberOfArguments;
	}


	TextArea::ScrollUnits units;
	if(unitsString.startsWith(QLatin1String("page"))) {
		units = TextArea::ScrollUnits::Pages;
	} else if(unitsString.startsWith(QLatin1String("line"))) {
		units = TextArea::ScrollUnits::Lines;
	} else {
		return MacroErrorCode::InvalidArgument;
	}

	if(auto window = MainWindow::fromDocument(document)) {
		if(TextArea *area = window->lastFocus()) {
			area->scrollDownAP(count, units, TextArea::SupressRecording);
		}
	}

	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code scrollUpMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	int count;
	QString unitsString = QLatin1String("line");
	switch(arguments.size()) {
	case 2:
		if(std::error_code ec = readArguments(arguments, 0, &count, &unitsString)) {
			return ec;
		}
		break;
	case 1:
		if(std::error_code ec = readArguments(arguments, 0, &count)) {
			return ec;
		}
		break;
	default:
		return MacroErrorCode::WrongNumberOfArguments;
	}


	TextArea::ScrollUnits units;
	if(unitsString.startsWith(QLatin1String("page"))) {
		units = TextArea::ScrollUnits::Pages;
	} else if(unitsString.startsWith(QLatin1String("line"))) {
		units = TextArea::ScrollUnits::Lines;
	} else {
		return MacroErrorCode::InvalidArgument;
	}

	if(auto window = MainWindow::fromDocument(document)) {
		if(TextArea *area = window->lastFocus()) {
			area->scrollUpAP(count, units, TextArea::SupressRecording);
		}
	}

	*result = make_value();
	return MacroErrorCode::Success;
}

static const SubRoutine TextAreaSubrNames[] = {
	// Keyboard
	{"backward_character",        textEvent<&TextArea::backwardCharacter> },
	{"backward_paragraph",        textEvent<&TextArea::backwardParagraphAP> },
	{"backward_word",             textEvent<&TextArea::backwardWordAP> },
	{"beginning_of_file",         textEvent<&TextArea::beginningOfFileAP> },
	{"beginning_of_line",         textEvent<&TextArea::beginningOfLine> },
	{"beginning_of_selection",    textEvent<&TextArea::beginningOfSelectionAP> },
	{"copy_clipboard",            textEvent<&TextArea::copyClipboard> },
	{"copy_primary",              textEvent<&TextArea::copyPrimaryAP> },
	{"cut_clipboard",             textEvent<&TextArea::cutClipboard> },
	{"cut_primary",               textEvent<&TextArea::cutPrimaryAP> },
	{"delete_selection",          textEvent<&TextArea::deleteSelectionAP> },
	{"delete_next_character",     textEvent<&TextArea::deleteNextCharacter> },
	{"delete_previous_character", textEvent<&TextArea::deletePreviousCharacter> },
	{"delete_next_word",          textEvent<&TextArea::deleteNextWordAP> },
	{"delete_previous_word",      textEvent<&TextArea::deletePreviousWord> },
	{"delete_to_start_of_line",   textEvent<&TextArea::deleteToStartOfLineAP> },
	{"delete_to_end_of_line",     textEvent<&TextArea::deleteToEndOfLineAP> },
	{"deselect_all",              textEvent<&TextArea::deselectAllAP> },
	{"end_of_file",               textEvent<&TextArea::endOfFileAP> },
	{"end_of_line",               textEvent<&TextArea::endOfLine> },
	{"end_of_selection",          textEvent<&TextArea::endOfSelectionAP> },
	{"forward_character",         textEvent<&TextArea::forwardCharacter> },
	{"forward_paragraph",         textEvent<&TextArea::forwardParagraphAP> },
	{"forward_word",              textEvent<&TextArea::forwardWordAP> },
	{"insert_string",             textEventArg<const QString &, &TextArea::insertStringAP> },
	{"key_select",                textEvent<&TextArea::keySelectAP> },
	{"newline",                   textEvent<&TextArea::newline> },
	{"newline_and_indent",        textEvent<&TextArea::newlineAndIndentAP> },
	{"newline_no_indent",         textEvent<&TextArea::newlineNoIndentAP> },
	{"next_page",                 textEvent<&TextArea::nextPageAP> },
	{"page_left",                 textEvent<&TextArea::pageLeftAP> },
	{"page_right",                textEvent<&TextArea::pageRightAP> },
	{"paste_clipboard",           textEvent<&TextArea::pasteClipboard> },
	{"previous_page",             textEvent<&TextArea::previousPageAP> },
	{"process_cancel",            textEvent<&TextArea::processCancel> },
	{"process_down",              textEvent<&TextArea::processDown> },
	{"process_return",            textEvent<&TextArea::newline> },
	{"process_shift_down",        textEvent<&TextArea::processShiftDownAP> },
	{"process_shift_up",          textEvent<&TextArea::processShiftUpAP> },
	{"process_tab",               textEvent<&TextArea::processTabAP> },
	{"process_up",                textEvent<&TextArea::processUp> },
	{"scroll_down",               scrollDownMS},
	{"scroll_left",               textEventArg<int, &TextArea::scrollLeftAP> },
	{"scroll_right",              textEventArg<int, &TextArea::scrollRightAP> },
	{"scroll_up",                 scrollUpMS},
	{"scroll_to_line",            textEventArg<int, &TextArea::scrollToLineAP> },
	{"self_insert",               textEventArg<const QString &, &TextArea::insertStringAP> },

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

template <void(MainWindow::*Func)(DocumentWidget *, const QString &, CommandSource)>
std::error_code menuEventSM(DocumentWidget *document, Arguments arguments, DataValue *result) {

	/* ensure that we are dealing with the document which currently has the focus */
	document = MacroFocusDocument();

	QString string;
	if(std::error_code ec = readArguments(arguments, 0, &string)) {
		return ec;
	}

	if(auto win = MainWindow::fromDocument(document)) {
		(win->*Func)(document, string, CommandSource::Macro);
	}

	*result = make_value();
	return MacroErrorCode::Success;
}

template <void(MainWindow::*Func)(DocumentWidget *, const QString &)>
std::error_code menuEventSU(DocumentWidget *document, Arguments arguments, DataValue *result) {

	/* ensure that we are dealing with the document which currently has the focus */
	document = MacroFocusDocument();

	QString string;
	if(std::error_code ec = readArguments(arguments, 0, &string)) {
		return ec;
	}

	if(auto win = MainWindow::fromDocument(document)) {
		(win->*Func)(document, string);
	}

	*result = make_value();
	return MacroErrorCode::Success;
}

template <void(MainWindow::*Func)(DocumentWidget *, CommandSource)>
std::error_code menuEventM(DocumentWidget *document, Arguments arguments, DataValue *result) {

	/* ensure that we are dealing with the document which currently has the focus */
	document = MacroFocusDocument();

	if(!arguments.empty()) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	if(auto win = MainWindow::fromDocument(document)) {
		(win->*Func)(document, CommandSource::Macro);
	}

	*result = make_value();
	return MacroErrorCode::Success;
}

template <void(MainWindow::*Func)(DocumentWidget *)>
std::error_code menuEventU(DocumentWidget *document, Arguments arguments, DataValue *result) {

	/* ensure that we are dealing with the document which currently has the focus */
	document = MacroFocusDocument();

	if(!arguments.empty()) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	if(auto win = MainWindow::fromDocument(document)) {
		(win->*Func)(document);
	}

	*result = make_value();
	return MacroErrorCode::Success;
}

/*
** Scans action argument list for arguments "forward" or "backward" to
** determine search direction for search and replace actions.  "index"
** tells the routine how many required arguments there are to ignore before
** looking for keywords
*/
static Direction searchDirection(Arguments arguments, int index) {
	for(int i = index; i < arguments.size(); ++i) {
		QString arg;
		if (std::error_code ec = readArgument(arguments[i], &arg)) {
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
** determine whether to keep dialogs up for search and replace.  "index"
** tells the routine how many required arguments there are to ignore before
** looking for keywords
*/
static bool searchKeepDialogs(Arguments arguments, int index) {
	for(int i = index; i < arguments.size(); ++i) {
		QString arg;
		if (std::error_code ec = readArgument(arguments[i], &arg)) {
			return Preferences::GetPrefKeepSearchDlogs();
		}

		if (arg.compare(QLatin1String("keep"), Qt::CaseInsensitive) == 0)
			return true;

		if (arg.compare(QLatin1String("nokeep"), Qt::CaseInsensitive) == 0)
			return false;
	}

	return Preferences::GetPrefKeepSearchDlogs();
}


/*
** Scans action argument list for arguments "wrap" or "nowrap" to
** determine search direction for search and replace actions.  "index"
** tells the routine how many required arguments there are to ignore before
** looking for keywords
*/
static WrapMode searchWrap(Arguments arguments, int index) {
	for(int i = index; i < arguments.size(); ++i) {
		QString arg;
		if (std::error_code ec = readArgument(arguments[i], &arg)) {
			return Preferences::GetPrefSearchWraps();
		}

		if (arg.compare(QLatin1String("wrap"), Qt::CaseInsensitive) == 0)
			return WrapMode::Wrap;

		if (arg.compare(QLatin1String("nowrap"), Qt::CaseInsensitive) == 0)
			return WrapMode::NoWrap;
	}

	return Preferences::GetPrefSearchWraps();
}

/*
** Parses a search type description string. If the string contains a valid
** search type description, returns true and writes the corresponding
** SearchType in searchType. Returns false and leaves searchType untouched
** otherwise. (Originally written by Markus Schwarzenberg; slightly adapted).
*/
static bool StringToSearchType(const QString &string, SearchType *searchType) {

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

	// NOTE(eteran): leave searchType unchanged (issue #97)
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

		if (std::error_code ec = readArgument(arguments[i], &arg)) {
			return Preferences::GetPrefSearch();
		}

		SearchType type;
		if(StringToSearchType(arg, &type)) {
			return type;
		}
	}

	return Preferences::GetPrefSearch();
}

static std::error_code closeMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	// ensure that we are dealing with the document which currently has the focus
	document = MacroRunDocument();

	if(arguments.size() > 1) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	CloseMode mode = CloseMode::Prompt;

	if(arguments.size() == 1) {
		QString string;
		if(std::error_code ec = readArguments(arguments, 0, &string)) {
			return ec;
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

	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code newMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	// ensure that we are dealing with the document which currently has the focus
	document = MacroRunDocument();

	if(arguments.size() > 1) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	NewMode mode = NewMode::Prefs;

	if(arguments.size() == 1) {
		QString string;
		if(std::error_code ec = readArguments(arguments, 0, &string)) {
			return ec;
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

	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code newOppositeMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	// ensure that we are dealing with the document which currently has the focus
	document = MacroRunDocument();

	if(arguments.size() > 0) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	if(auto window = MainWindow::fromDocument(document)) {
		window->action_New(document, NewMode::Opposite);
	}

	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code saveAsMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	// ensure that we are dealing with the document which currently has the focus
	document = MacroRunDocument();

	if(arguments.size() > 2 || arguments.empty()) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	QString filename;
	if (std::error_code ec = readArgument(arguments[0], &filename)) {
		return ec;
	}

	bool wrapped = false;

	// NOTE(eteran): "wrapped" optional argument is not documented
	if(arguments.size() == 2) {
		QString string;
		if(std::error_code ec = readArgument(arguments[1], &string)) {
			return ec;
		}

		if(string.compare(QLatin1String("wrapped"), Qt::CaseInsensitive) == 0) {
			wrapped = true;
		}
	}

	if(auto window = MainWindow::fromDocument(document)) {
		window->action_Save_As(document, filename, wrapped);
	}

	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code findMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	// find( search-string [, search-direction] [, search-type] [, search-wrap] )

	// ensure that we are dealing with the document which currently has the focus
	document = MacroRunDocument();

	if(arguments.size() > 4 || arguments.empty()) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	QString string;
	if(std::error_code ec = readArguments(arguments, 0, &string)) {
		return ec;
	}

	Direction direction = searchDirection(arguments, 1);
	SearchType      type      = searchType(arguments, 1);
	WrapMode        wrap      = searchWrap(arguments, 1);

	if(auto window = MainWindow::fromDocument(document)) {
		window->action_Find(document, string, direction, type, wrap);
	}

	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code findDialogMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	// ensure that we are dealing with the document which currently has the focus
	document = MacroRunDocument();

	Direction direction = searchDirection(arguments, 0);
	SearchType      type      = searchType(arguments, 0);
	bool            keep      = searchKeepDialogs(arguments, 0);

	if(auto window = MainWindow::fromDocument(document)) {
		window->action_Find_Dialog(document, direction, type, keep);
	}

	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code findAgainMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	// ensure that we are dealing with the document which currently has the focus
	document = MacroRunDocument();

	Direction direction = searchDirection(arguments, 0);
	WrapMode wrap       = searchWrap(arguments, 0);

	if(auto window = MainWindow::fromDocument(document)) {
		window->action_Find_Again(document, direction, wrap);
	}

	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code findSelectionMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	// ensure that we are dealing with the document which currently has the focus
	document = MacroRunDocument();

	Direction direction = searchDirection(arguments, 0);
	SearchType type     = searchType(arguments, 0);
	WrapMode wrap       = searchWrap(arguments, 0);

	if(auto window = MainWindow::fromDocument(document)) {
		window->action_Find_Selection(document, direction, type, wrap);
	}

	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code replaceMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	// ensure that we are dealing with the document which currently has the focus
	document = MacroRunDocument();

	if(arguments.size() > 5 || arguments.size() < 2) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	QString searchString;
	QString replaceString;
	if(std::error_code ec = readArguments(arguments, 0, &searchString, &replaceString)) {
		return ec;
	}

	Direction direction = searchDirection(arguments, 2);
	SearchType type     = searchType(arguments, 2);
	WrapMode   wrap     = searchWrap(arguments, 2);

	if(auto window = MainWindow::fromDocument(document)) {
		window->action_Replace(document, searchString, replaceString, direction, type, wrap);
	}

	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code replaceDialogMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	// ensure that we are dealing with the document which currently has the focus
	document = MacroRunDocument();

	Direction direction = searchDirection(arguments, 0);
	SearchType      type      = searchType(arguments, 0);
	bool            keep      = searchKeepDialogs(arguments, 0);

	if(auto window = MainWindow::fromDocument(document)) {
		window->action_Replace_Dialog(document, direction, type, keep);
	}

	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code replaceAgainMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	// ensure that we are dealing with the document which currently has the focus
	document = MacroRunDocument();

	if(arguments.size() != 2) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	WrapMode wrap       = searchWrap(arguments, 0);
	Direction direction = searchDirection(arguments, 0);

	if(auto window = MainWindow::fromDocument(document)) {
		window->action_Replace_Again(document, direction, wrap);
	}

	*result = make_value();

	return MacroErrorCode::Success;
}

static std::error_code gotoMarkMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	// goto_mark( mark, [, extend] )

	// ensure that we are dealing with the document which currently has the focus
	document = MacroRunDocument();

	if(arguments.size() > 2 || arguments.size() < 1) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	bool extend = false;

	QString mark;
	if(std::error_code ec = readArguments(arguments, 0, &mark)) {
		return ec;
	}

	if(arguments.size() == 2) {
		QString argument;
		if(std::error_code ec = readArgument(arguments[1], &argument)) {
			return ec;
		}

		if(argument.compare(QLatin1String("extend"), Qt::CaseInsensitive) == 0) {
			extend = true;
		}
	}

	if(auto window = MainWindow::fromDocument(document)) {
		window->action_Goto_Mark(document, mark, extend);
	}

	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code gotoMarkDialogMS(DocumentWidget *document, Arguments arguments, DataValue *result) {
	// goto_mark( mark, [, extend] )

	// ensure that we are dealing with the document which currently has the focus
	document = MacroRunDocument();

	if(arguments.size() > 1) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	bool extend = false;

	if(arguments.size() == 1) {
		QString argument;
		if(std::error_code ec = readArgument(arguments[1], &argument)) {
			return ec;
		}

		if(argument.compare(QLatin1String("extend"), Qt::CaseInsensitive) == 0) {
			extend = true;
		}
	}

	if(auto window = MainWindow::fromDocument(document)) {
		window->action_Goto_Mark_Dialog(document, extend);
	}

	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code findDefinitionMS(DocumentWidget *document, Arguments arguments, DataValue *result) {
	// find_definition( [ argument ] )

	// ensure that we are dealing with the document which currently has the focus
	document = MacroRunDocument();

	if(arguments.size() > 1) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	QString argument;

	if(arguments.size() == 1) {
		if(std::error_code ec = readArgument(arguments[1], &argument)) {
			return ec;
		}
	}

	if(auto window = MainWindow::fromDocument(document)) {
		window->action_Find_Definition(document, argument);
	}

	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code repeatMacroMS(DocumentWidget *document, Arguments arguments, DataValue *result) {
	// repeat_macro( how/count, method )

	// ensure that we are dealing with the document which currently has the focus
	document = MacroRunDocument();

	QString howString;
	QString method;

	if(std::error_code ec = readArguments(arguments, 0, &howString, &method)) {
		return ec;
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
			return MacroErrorCode::InvalidRepeatArg;
		}
	}

	if(auto window = MainWindow::fromDocument(document)) {
		window->action_Repeat_Macro(document, method, how);
	}

	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code detachDocumentDialogMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(arguments)
	Q_UNUSED(result)

	// ensure that we are dealing with the document which currently has the focus
	document = MacroRunDocument();

	if(auto window = MainWindow::fromDocument(document)) {
		window->action_Detach_Document_Dialog(document);
	}

	return MacroErrorCode::Success;
}

static std::error_code setAutoIndentMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	QString string;
	if(std::error_code ec = readArguments(arguments, 0, &string)) {
		qWarning("NEdit: set_auto_indent requires argument");
		return ec;
	}

	// ensure that we are dealing with the document which currently has the focus
	document = MacroRunDocument();

	if(string == QLatin1String("off")) {
		document->setAutoIndent(IndentStyle::None);
	} else if(string == QLatin1String("on")) {
		document->setAutoIndent(IndentStyle::Auto);
	} else if(string == QLatin1String("smart")) {
		document->setAutoIndent(IndentStyle::Smart);
	} else {
		qWarning("NEdit: set_auto_indent invalid argument");
	}

	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code setEmTabDistMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	int number;
	if(std::error_code ec = readArguments(arguments, 0, &number)) {
		qWarning("NEdit: set_em_tab_dist requires argument");
		return ec;
	}

	// ensure that we are dealing with the document which currently has the focus
	document = MacroRunDocument();

	if (number < 1000) {
		if (number < 0) {
			number = 0;
		}
		document->setEmTabDistance(number);
	} else {
		qWarning("NEdit: set_em_tab_dist requires integer argument >= -1 and < 1000");
	}

	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code setFontsMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	// ensure that we are dealing with the document which currently has the focus
	document = MacroRunDocument();

	QString fontName;

	if(arguments.size() == 1) {
		if(std::error_code ec = readArguments(arguments, 0, &fontName)) {
			qWarning("NEdit: set_fonts requires a string argument");
			return ec;
		}
	} else {
		QString italicName;
		QString boldName;
		QString boldItalicName;

		if(std::error_code ec = readArguments(arguments, 0, &fontName, &italicName, &boldName, &boldItalicName)) {
			qWarning("NEdit: set_fonts requires 4 arguments");
			return ec;
		}

		qWarning("NEdit: support for independent fonts for styles is no longer support, arguments 2-4 are ignored");
	}

	document->action_Set_Fonts(fontName);

	*result = make_value();
	return MacroErrorCode::Success;
}

static boost::optional<int> toggle_or_bool(Arguments arguments, int previous, std::error_code *errMsg, const char *actionName) {

	int next;
	switch(arguments.size()) {
	case 1:
		if(std::error_code ec = readArguments(arguments, 0, &next)) {
			*errMsg = ec;
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


static std::error_code setHighlightSyntaxMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	document = MacroRunDocument();

	std::error_code ec;
	if(boost::optional<int> next = toggle_or_bool(arguments, document->highlightSyntax(), &ec, "set_highlight_syntax")) {
		document->setHighlightSyntax(*next);
		*result = make_value();
		return MacroErrorCode::Success;
	}

	return ec;
}

static std::error_code setIncrementalBackupMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	document = MacroRunDocument();

	std::error_code ec;
	if(boost::optional<int> next = toggle_or_bool(arguments, document->incrementalBackup(), &ec, "set_incremental_backup")) {
		document->setIncrementalBackup(*next);
		*result = make_value();
		return MacroErrorCode::Success;
	}

	return ec;
}

static std::error_code setIncrementalSearchLineMS(DocumentWidget *document, Arguments arguments, DataValue *result) {
	document = MacroRunDocument();

	auto win = MainWindow::fromDocument(document);
	Q_ASSERT(win);

	std::error_code ec;
	if(boost::optional<int> next = toggle_or_bool(arguments, win->getIncrementalSearchLine(), &ec, "set_incremental_search_line")) {
		win->setIncrementalSearchLine(*next);
		*result = make_value();
		return MacroErrorCode::Success;
	}

	return ec;
}

static std::error_code setMakeBackupCopyMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	document = MacroRunDocument();

	std::error_code ec;
	if(boost::optional<int> next = toggle_or_bool(arguments, document->makeBackupCopy(), &ec, "set_make_backup_copy")) {
		document->setMakeBackupCopy(*next);
		*result = make_value();
		return MacroErrorCode::Success;
	}

	return ec;
}

static std::error_code setLockedMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	document = MacroRunDocument();

	std::error_code ec;
	if(boost::optional<int> next = toggle_or_bool(arguments, document->userLocked(), &ec, "set_locked")) {
		document->setUserLocked(*next);
		*result = make_value();
		return MacroErrorCode::Success;
	}

	return ec;
}

static std::error_code setLanguageModeMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	document = MacroRunDocument();

	QString languageMode;
	if(std::error_code ec = readArguments(arguments, 0, &languageMode)) {
		qWarning("NEdit: set_language_mode requires argument");
		return ec;
	}

	document->action_Set_Language_Mode(languageMode, false);
	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code setOvertypeModeMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	document = MacroRunDocument();

	std::error_code ec;
	if(boost::optional<int> next = toggle_or_bool(arguments, document->overstrike(), &ec, "set_overtype_mode")) {
		document->setOverstrike(*next);
		*result = make_value();
		return MacroErrorCode::Success;
	}

	return ec;
}

static std::error_code setShowLineNumbersMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	document = MacroRunDocument();

	auto win = MainWindow::fromDocument(document);
	Q_ASSERT(win);

	std::error_code ec;
	if(boost::optional<int> next = toggle_or_bool(arguments, win->getShowLineNumbers(), &ec, "set_show_line_numbers")) {
		win->setShowLineNumbers(*next);
		*result = make_value();
		return MacroErrorCode::Success;
	}

	return ec;
}

static std::error_code setShowMatchingMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	document = MacroRunDocument();

	if (arguments.size() > 0) {
		QString arg;
		if(std::error_code ec = readArgument(arguments[0], &arg)) {
			return ec;
		}

		if (arg == QLatin1String("off")) {
			document->setShowMatching(ShowMatchingStyle::None);
		} else if (arg == QLatin1String("delimiter")) {
			document->setShowMatching(ShowMatchingStyle::Delimiter);
		} else if (arg == QLatin1String("range")) {
			document->setShowMatching(ShowMatchingStyle::Range);
		}
		/* For backward compatibility with pre-5.2 versions, we also
		   accept 0 and 1 as aliases for None and Delimeter.
		   It is quite unlikely, though, that anyone ever used this
		   action procedure via the macro language or a key binding,
		   so this can probably be left out safely. */
		else if (arg == QLatin1String("0")) {
		   document->setShowMatching(ShowMatchingStyle::None);
		} else if (arg == QLatin1String("1")) {
		   document->setShowMatching(ShowMatchingStyle::Delimiter);
		} else {
			qWarning("NEdit: Invalid argument for set_show_matching");
		}
	} else {
		qWarning("NEdit: set_show_matching requires argument");
	}


	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code setMatchSyntaxBasedMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	document = MacroRunDocument();

	std::error_code ec;
	if(boost::optional<int> next = toggle_or_bool(arguments, document->matchSyntaxBased(), &ec, "set_match_syntax_based")) {
		document->setMatchSyntaxBased(*next);
		*result = make_value();
		return MacroErrorCode::Success;
	}

	return ec;
}

static std::error_code setStatisticsLineMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	document = MacroRunDocument();

	std::error_code ec;
	if(boost::optional<int> next = toggle_or_bool(arguments, document->showStatisticsLine(), &ec, "set_statistics_line")) {
		document->setShowStatisticsLine(*next);
		*result = make_value();
		return MacroErrorCode::Success;
	}

	return ec;
}

static std::error_code setTabDistMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	document = MacroRunDocument();

	if (arguments.size() > 0) {
		int newTabDist = 0;

		std::error_code ec = readArguments(arguments, 0, &newTabDist);
		if(ec && newTabDist > 0 && newTabDist <= TextBuffer::MAX_EXP_CHAR_LEN) {
			document->setTabDistance(newTabDist);
		} else {
			qWarning("NEdit: set_tab_dist requires integer argument > 0 and <= %d", TextBuffer::MAX_EXP_CHAR_LEN);
		}
	} else {
		qWarning("NEdit: set_tab_dist requires argument");
	}

	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code setUseTabsMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	document = MacroRunDocument();

	std::error_code ec;
	if(boost::optional<int> next = toggle_or_bool(arguments, document->useTabs(), &ec, "set_use_tabs")) {
		document->setUseTabs(*next);
		*result = make_value();
		return MacroErrorCode::Success;
	}

	return ec;
}

static std::error_code setWrapMarginMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	document = MacroRunDocument();

	if (arguments.size() > 0) {
		int newMargin = 0;

		std::error_code ec = readArguments(arguments, 0, &newMargin);
		if(ec && newMargin > 0 && newMargin <= 1000) {

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

	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code setWrapTextMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	document = MacroRunDocument();

	if (arguments.size() > 0) {

		QString arg;
		if(std::error_code ec = readArgument(arguments[0], &arg)) {
			return ec;
		}

		if (arg == QLatin1String("none")) {
			document->setAutoWrap(WrapStyle::None);
		} else if (arg == QLatin1String("auto")) {
			document->setAutoWrap(WrapStyle::Newline);
		} else if (arg == QLatin1String("continuous")) {
		   document->setAutoWrap(WrapStyle::Continuous);
		} else {
			qWarning("NEdit: set_wrap_text invalid argument");
		}
	} else {
		qWarning("NEdit: set_wrap_text requires argument");
	}

	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code findIncrMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	document = MacroRunDocument();

	auto win = MainWindow::fromDocument(document);
	Q_ASSERT(win);

	int i;
	bool continued = false;

	QString arg;
	if(std::error_code ec = readArgument(arguments[0], &arg)) {
		qWarning("NEdit: find action requires search string argument");
		return ec;
	}

	for (i = 1; i < arguments.size(); ++i)  {
		QString arg2;
		if(std::error_code ec = readArgument(arguments[0], &arg2)) {
			return ec;
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

	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code startIncrFindMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	document = MacroRunDocument();

	auto win = MainWindow::fromDocument(document);
	Q_ASSERT(win);

	win->beginISearch(searchDirection(arguments, 0));

	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code replaceFindMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	document = MacroRunDocument();

	auto win = MainWindow::fromDocument(document);
	Q_ASSERT(win);

	if (document->checkReadOnly()) {
		// NOTE(eteran): previously was a silent failure
		return MacroErrorCode::ReadOnly;
	}

	QString searchString;
	QString replaceString;
	if(std::error_code ec = readArguments(arguments, 0, &searchString, &replaceString)) {
		return MacroErrorCode::InvalidSearchReplaceArgs;
	}

	win->action_Replace_Find(
				document,
				searchString,
				replaceString,
				searchDirection(arguments, 2),
				searchType(arguments, 2),
				searchWrap(arguments, 0));

	*result = make_value();
	return MacroErrorCode::Success;

}

static std::error_code replaceFindSameMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	document = MacroRunDocument();

	auto win = MainWindow::fromDocument(document);
	Q_ASSERT(win);

	if (document->checkReadOnly()) {
		// NOTE(eteran): previously was a silent failure
		return MacroErrorCode::ReadOnly;
	}

	win->action_Replace_Find_Again(
				document,
				searchDirection(arguments, 0),
				searchWrap(arguments, 0));

	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code nextDocumentMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(arguments)

	document = MacroRunDocument();

	auto win = MainWindow::fromDocument(document);
	Q_ASSERT(win);

	win->action_Next_Document();

	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code prevDocumentMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(arguments)

	document = MacroRunDocument();

	auto win = MainWindow::fromDocument(document);
	Q_ASSERT(win);

	win->action_Prev_Document();

	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code lastDocumentMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(arguments)

	document = MacroRunDocument();

	auto win = MainWindow::fromDocument(document);
	Q_ASSERT(win);

	win->action_Last_Document();

	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code backgroundMenuCommandMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	document = MacroRunDocument();

	auto win = MainWindow::fromDocument(document);
	Q_ASSERT(win);

	QString name;
	if(std::error_code ec = readArguments(arguments, 0, &name)) {
		qWarning("NEdit: bg_menu_command requires item-name argument\n");
		return ec;
	}

	win->execNamedBGMenuCmd(document, win->lastFocus(), name, CommandSource::Macro);

	*result = make_value();
	return MacroErrorCode::Success;
}

static const SubRoutine MenuMacroSubrNames[] = {
	// File
	{ "new",                          newMS },
	{ "new_opposite",                 newOppositeMS },
	{ "open",                         menuEventSU<&MainWindow::action_Open> },
	{ "open_dialog",                  menuEventU<&MainWindow::action_Open> },
	{ "open_selected",                menuEventU<&MainWindow::action_Open_Selected> },
	{ "close",                        closeMS },
	{ "save",                         menuEventU<&MainWindow::action_Save> },
	{ "save_as",                      saveAsMS },
	{ "save_as_dialog",               menuEventU<&MainWindow::action_Save_As> },
	{ "revert_to_saved_dialog",       menuEventU<&MainWindow::action_Revert_to_Saved> },
	{ "revert_to_saved",              menuEventU<&MainWindow::action_Revert_to_Saved> },
	{ "include_file",                 menuEventSU<&MainWindow::action_Include_File> },
	{ "include_file_dialog",          menuEventU<&MainWindow::action_Include_File> },
	{ "load_macro_file",              menuEventSU<&MainWindow::action_Load_Macro_File> },
	{ "load_macro_file_dialog",       menuEventU<&MainWindow::action_Load_Macro_File> },
	{ "load_tags_file",               menuEventSU<&MainWindow::action_Load_Tags_File> },
	{ "load_tags_file_dialog",        menuEventU<&MainWindow::action_Load_Tags_File> },
	{ "unload_tags_file",             menuEventSU<&MainWindow::action_Unload_Tags_File> },
	{ "load_tips_file",               menuEventSU<&MainWindow::action_Load_Tips_File> },
	{ "load_tips_file_dialog",        menuEventU<&MainWindow::action_Load_Calltips_File> },
	{ "unload_tips_file",             menuEventSU<&MainWindow::action_Unload_Tips_File> },
	{ "print",                        menuEventU<&MainWindow::action_Print> },
	{ "print_selection",              menuEventU<&MainWindow::action_Print_Selection> },
	{ "exit",                         menuEventU<&MainWindow::action_Exit> },

	// Edit
	{ "undo",                         menuEventU<&MainWindow::action_Undo> },
	{ "redo",                         menuEventU<&MainWindow::action_Redo> },
	{ "delete",                       menuEventU<&MainWindow::action_Delete> },
	{ "select_all",                   menuEventU<&MainWindow::action_Select_All> },
	{ "shift_left",                   menuEventU<&MainWindow::action_Shift_Left> },
	{ "shift_left_by_tab",            menuEventU<&MainWindow::action_Shift_Left_Tabs> },
	{ "shift_right",                  menuEventU<&MainWindow::action_Shift_Right> },
	{ "shift_right_by_tab",           menuEventU<&MainWindow::action_Shift_Right_Tabs> },
	{ "uppercase",                    menuEventU<&MainWindow::action_Upper_case> },
	{ "lowercase",                    menuEventU<&MainWindow::action_Lower_case> },
	{ "fill_paragraph",               menuEventU<&MainWindow::action_Fill_Paragraph> },
	{ "control_code_dialog",          menuEventU<&MainWindow::action_Insert_Ctrl_Code> },

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
	{ "goto_line_number",             menuEventSU<&MainWindow::action_Goto_Line_Number> },
	{ "goto_line_number_dialog",      menuEventU<&MainWindow::action_Goto_Line_Number> },
	{ "goto_selected",                menuEventU<&MainWindow::action_Goto_Selected> },
	{ "mark",                         menuEventSU<&MainWindow::action_Mark> },
	{ "mark_dialog",                  menuEventU<&MainWindow::action_Mark> },
	{ "goto_mark",                    gotoMarkMS },
	{ "goto_mark_dialog",             gotoMarkDialogMS },
	{ "goto_matching",                menuEventU<&MainWindow::action_Goto_Matching> },
	{ "select_to_matching",           menuEventU<&MainWindow::action_Shift_Goto_Matching> },
	{ "find_definition",              findDefinitionMS },
	{ "show_tip",                     menuEventU<&MainWindow::action_Show_Calltip> },

	// Shell
	{ "filter_selection_dialog",      menuEventM<&MainWindow::action_Filter_Selection> },
	{ "filter_selection",             menuEventSM<&MainWindow::action_Filter_Selection> },
	{ "execute_command",              menuEventSU<&MainWindow::action_Execute_Command> },
	{ "execute_command_dialog",       menuEventU<&MainWindow::action_Execute_Command> },
	{ "execute_command_line",         menuEventU<&MainWindow::action_Execute_Command_Line> },
	{ "shell_menu_command",           menuEventSU<&MainWindow::action_Shell_Menu_Command> },

	// Macro
	{ "macro_menu_command",           menuEventSU<&MainWindow::action_Macro_Menu_Command> },
	{ "repeat_macro",                 repeatMacroMS },
	{ "repeat_dialog",                menuEventU<&MainWindow::action_Repeat> },

	// Windows
	{ "split_pane",                   menuEventU<&MainWindow::action_Split_Pane> },
	{ "close_pane",                   menuEventU<&MainWindow::action_Close_Pane> },
	{ "detach_document",              menuEventU<&MainWindow::action_Detach_Document> },
	{ "detach_document_dialog",       detachDocumentDialogMS },
	{ "move_document_dialog",         menuEventU<&MainWindow::action_Move_Tab_To> },

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
	{ "match",                        menuEventU<&MainWindow::action_Shift_Goto_Matching> },

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
#if defined(ENABLE_BACKLIGHT_STRING)
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
#if defined(ENABLE_BACKLIGHT_STRING)
	{ "$backlight_string",        backlightStringMV},
#endif
	{ "$rangeset_list",           rangesetListMV },
	{ "$VERSION",                 versionMV }
};



// Global symbols for returning values from built-in functions
constexpr int N_RETURN_GLOBALS = 5;

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

	/* Install symbols for built-in routines and variables, with pointers
	   to the appropriate c routines to do the work */
	for(const SubRoutine &routine : MacroSubrs) {
		DataValue subrPtr = make_value(routine.function);
		InstallSymbol(routine.name, C_FUNCTION_SYM, subrPtr);
	}

	for(const SubRoutine &routine : SpecialVars) {
		DataValue subrPtr = make_value(routine.function);
		InstallSymbol(routine.name, PROC_VALUE_SYM, subrPtr);
	}

	for(const SubRoutine &routine : MenuMacroSubrNames) {
		DataValue subrPtr = make_value(routine.function);
		InstallSymbol(routine.name, C_FUNCTION_SYM, subrPtr);
	}

	for(const SubRoutine &routine : TextAreaSubrNames) {
		DataValue subrPtr = make_value(routine.function);
		InstallSymbol(routine.name, C_FUNCTION_SYM, subrPtr);
	}

	/* Define global variables used for return values, remember their
	   locations so they can be set without a LookupSymbol call */
	for (unsigned int i = 0; i < N_RETURN_GLOBALS; i++) {
		ReturnGlobals[i] = InstallSymbol(ReturnGlobalNames[i], GLOBAL_SYM, make_value());
	}
}


/*
** Check a macro string containing definitions for errors.  Returns true
** if macro compiled successfully.  Returns false and puts up
** a dialog explaining if macro did not compile successfully.
*/
bool CheckMacroString(QWidget *dialogParent, const QString &string, const QString &errIn, int *errPos) {
	return readCheckMacroString(dialogParent, string, nullptr, errIn, errPos);
}

bool readCheckMacroString(QWidget *dialogParent, const QString &string, DocumentWidget *runDocument, const QString &errIn, int *errPos) {

	Input in(&string);
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
		static const QRegularExpression defineRE(QLatin1String("define[ \t]"));
		if (in.match(defineRE)) {
			in.skipWhitespace();

			QString routineName;
			static const QRegularExpression identRE(QLatin1String("[A-Za-z0-9_]+"));
			if(!in.match(identRE, &routineName)) {
				if(errPos) {
					*errPos = in.index();
				}
				return Preferences::reportError(
							dialogParent,
							*in.string(),
							in.index(),
							errIn,
							QLatin1String("expected identifier"));
			}

			in.skipWhitespaceNL();

			if (*in != QLatin1Char('{')) {
				if(errPos) {
					*errPos = in.index();
				}
				return Preferences::reportError(
							dialogParent,
							*in.string(),
							in.index(),
							errIn,
							QLatin1String("expected '{'"));
			}

			QString code = in.mid();

			int stoppedAt;
			QString errMsg;
			Program *const prog = compileMacro(code, &errMsg, &stoppedAt);
			if(!prog) {
				if (errPos) {
					*errPos = in.index() + stoppedAt;
				}

				return Preferences::reportError(
							dialogParent,
							code,
							stoppedAt,
							errIn,
							errMsg);
			}

			if (runDocument) {
				if(Symbol *const sym = LookupSymbolEx(routineName)) {

					if (sym->type == MACRO_FUNCTION_SYM) {
						delete to_program(sym->value);
					} else {
						sym->type = MACRO_FUNCTION_SYM;
					}

					sym->value = make_value(prog);
				} else {
					DataValue subrPtr = make_value(prog);
					InstallSymbolEx(routineName, MACRO_FUNCTION_SYM, subrPtr);
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
			Program *const prog = compileMacro(code, &errMsg, &stoppedAt);
			if(!prog) {
				if (errPos) {
					*errPos = in.index() + stoppedAt;
				}

				return Preferences::reportError(
							dialogParent,
							code,
							stoppedAt,
							errIn,
							errMsg);
			}

			if (runDocument) {

				if (!runDocument->macroCmdData_) {
					runDocument->runMacro(prog);
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

		// TODO(eteran): OK, I'm pretty sure this is a memory leak. We CAN'T delete prog quite yet,
		// because despite the name, "RunMacroAsSubrCall" doesn't run anything, it just sets up the
		// code for execution... but no one is tracking this pointer anymore :-/.
	}

	return true;
}

/*
** Built-in macro subroutine for getting the length of a string
*/
static std::error_code lengthMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(document)

	QString string;
	if(std::error_code ec = readArguments(arguments, 0, &string)) {
		return ec;
	}

	*result = make_value(string.size());
	return MacroErrorCode::Success;
}

/*
** Built-in macro subroutines for min and max
*/
static std::error_code minMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(document)

	if (arguments.size() < 2) {
		return MacroErrorCode::TooFewArguments;
	}

	int minVal;
	if (std::error_code ec = readArgument(arguments[0], &minVal)) {
		return ec;
	}

	for (const DataValue &dv : arguments) {
		int value;
		if (std::error_code ec = readArgument(dv, &value)) {
			return ec;
		}

		minVal = std::min(minVal, value);
	}

	*result = make_value(minVal);
	return MacroErrorCode::Success;
}

static std::error_code maxMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(document)

	if (arguments.size() < 2) {
		return MacroErrorCode::TooFewArguments;
	}

	int maxVal;
	if (std::error_code ec = readArgument(arguments[0], &maxVal)) {
		return ec;
	}

	for (const DataValue &dv : arguments) {
		int value;
		if (std::error_code ec = readArgument(dv, &value)) {
			return ec;
		}

		maxVal = std::max(maxVal, value);
	}

	*result = make_value(maxVal);
	return MacroErrorCode::Success;
}

static std::error_code focusWindowMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	/* Read the argument representing the window to focus to, and translate
	   it into a pointer to a real DocumentWidget */
	if (arguments.size() != 1) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	QString string;
	if (std::error_code ec = readArgument(arguments[0], &string)) {
		return ec;
	}

	std::vector<DocumentWidget *> documents = DocumentWidget::allDocuments();
	std::vector<DocumentWidget *>::iterator it;

	if (string == QLatin1String("last")) {

		it = std::find(documents.begin(), documents.end(), DocumentWidget::LastCreated);

	} else if (string == QLatin1String("next")) {

		it = std::find_if(documents.begin(), documents.end(), [document](DocumentWidget *doc) {
			return doc == document;
		});

		if(it != documents.end()) {
			it = std::next(it);
		}
	} else {
		// just use the plain name as supplied
		it = std::find_if(documents.begin(), documents.end(), [&string](DocumentWidget *doc) {
			return doc->fullPath() == string;
		});

		// didn't work? try normalizing the string passed in
		if(it == documents.end()) {

			const QString normalizedString = NormalizePathname(string);
			if(normalizedString.isNull()) {
				//  Something is broken with the input pathname.
				return MacroErrorCode::PathnameTooLong;
			}

			it = std::find_if(documents.begin(), documents.end(), [&normalizedString](DocumentWidget *doc) {
				return doc->fullPath() == normalizedString;
			});
		}
	}

	// If no matching window was found, return empty string and do nothing
	if(it == documents.end()) {
		*result = make_value(std::string());
		return MacroErrorCode::Success;
	}

	DocumentWidget *const target = *it;

	// Change the focused window to the requested one
	SetMacroFocusDocument(target);

	// turn on syntax highlight that might have been deferred
	if (target->highlightSyntax_ && !target->highlightData_) {
		target->startHighlighting(Verbosity::Silent);
	}

	// Return the name of the window
	*result = make_value(QString(QLatin1String("%1%2")).arg(target->path(), target->filename()));
	return MacroErrorCode::Success;
}

/*
** Built-in macro subroutine for getting text from the current window's text
** buffer
*/
static std::error_code getRangeMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	int64_t from;
	int64_t to;
	TextBuffer *buf = document->buffer();

	// Validate arguments and convert to int
	if(std::error_code ec = readArguments(arguments, 0, &from, &to)) {
		return ec;
	}

	from = qBound<int64_t>(0, from, buf->length());
	to   = qBound<int64_t>(0, to,   buf->length());

	if (from > to) {
		std::swap(from, to);
	}

	std::string rangeText = buf->BufGetRange(TextCursor(from), TextCursor(to));

	*result = make_value(rangeText);
	return MacroErrorCode::Success;
}

/*
** Built-in macro subroutine for getting a single character at the position
** given, from the current window
*/
static std::error_code getCharacterMS(DocumentWidget *document, Arguments arguments, DataValue *result) {


	int64_t pos;
	TextBuffer *buf = document->buffer();

	// Validate arguments and convert to int
	if(std::error_code ec = readArguments(arguments, 0, &pos)) {
		return ec;
	}

	pos = qBound<int64_t>(0, pos, buf->length());

	// Return the character in a pre-allocated string)
	std::string str(1, buf->BufGetCharacter(TextCursor(pos)));

	*result = make_value(str);
	return MacroErrorCode::Success;
}

/*
** Built-in macro subroutine for replacing text in the current window's text
** buffer
*/
static std::error_code replaceRangeMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	document = MacroFocusDocument();

	int64_t from;
	int64_t to;
	TextBuffer *buf = document->buffer();
	std::string string;

	// Validate arguments and convert to int
	if(std::error_code ec = readArguments(arguments, 0, &from, &to, &string)) {
		return ec;
	}

	from = qBound<int64_t>(0, from, buf->length());
	to   = qBound<int64_t>(0, to,   buf->length());

	if (from > to) {
		std::swap(from, to);
	}

	// Don't allow modifications if the window is read-only
	if (document->lockReasons().isAnyLocked()) {
		QApplication::beep();
		*result = make_value();
		return MacroErrorCode::Success;
	}

	// Do the replace
	buf->BufReplace(TextCursor(from), TextCursor(to), string);
	*result = make_value();
	return MacroErrorCode::Success;
}

/*
** Built-in macro subroutine for replacing the primary-selection selected
** text in the current window's text buffer
*/
static std::error_code replaceSelectionMS(DocumentWidget *document, Arguments arguments, DataValue *result) {
	std::string string;

	// Validate argument and convert to string
	if(std::error_code ec = readArguments(arguments, 0, &string)) {
		return ec;
	}

	// Don't allow modifications if the window is read-only
	if (document->lockReasons().isAnyLocked()) {
		QApplication::beep();
		*result = make_value();
		return MacroErrorCode::Success;
	}

	// Do the replace
	document->buffer()->BufReplaceSelected(string);
	*result = make_value();
	return MacroErrorCode::Success;
}

/*
** Built-in macro subroutine for getting the text currently selected by
** the primary selection in the current window's text buffer, or in any
** part of screen if "any" argument is given
*/
static std::error_code getSelectionMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	/* Read argument list to check for "any" keyword, and get the appropriate
	   selection */
	if (!arguments.empty() && arguments.size() != 1) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	if (arguments.size() == 1) {
		if (!is_string(arguments[0]) || to_string(arguments[0]) != "any") {
			return MacroErrorCode::UnrecognizedArgument;
		}

		QString text = document->getAnySelection(ErrorSound::Beep);
		if (text.isNull()) {
			text = QLatin1String("");
		}

		// Return the text as an allocated string
		*result = make_value(text);
	} else {
		std::string selText = document->buffer()->BufGetSelectionText();

		// Return the text as an allocated string
		*result = make_value(selText);
	}

	return MacroErrorCode::Success;
}

/*
** Built-in macro subroutine for determining if implicit conversion of
** a string to number will succeed or fail
*/
static std::error_code validNumberMS(DocumentWidget *document, Arguments arguments, DataValue *result) {
	Q_UNUSED(document)

	std::string string;

	if(std::error_code ec = readArguments(arguments, 0, &string)) {
		return ec;
	}

	*result = make_value(StringToNum(string, nullptr));
	return MacroErrorCode::Success;
}

/*
** Built-in macro subroutine for replacing a substring within another string
*/
static std::error_code replaceSubstringMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(document)

	int from;
	int to;
	QString string;
	QString replStr;

	// Validate arguments and convert to int
	if(std::error_code ec = readArguments(arguments, 0, &string, &from, &to, &replStr)) {
		return ec;
	}

	const int length = string.size();

	from = qBound(0, from, length);
	to   = qBound(0, to,   length);

	if (from > to) {
		std::swap(from, to);
	}

	// Allocate a new string and do the replacement
	string.replace(from, to - from, replStr);
	*result = make_value(string);

	return MacroErrorCode::Success;
}

/*
** Built-in macro subroutine for getting a substring of a string.
** Called as substring(string, from [, to])
*/
static std::error_code substringMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(document)

	// Validate arguments and convert to int
	if (arguments.size() != 2 && arguments.size() != 3) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	int from;
	QString string;

	if(std::error_code ec = readArguments(arguments, 0, &string, &from)) {
		return ec;
	}

	int length = string.size();
	int to     = string.size();

	if (arguments.size() == 3) {
		if (std::error_code ec = readArgument(arguments[2], &to)) {
			return ec;
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
	*result = make_value(string.mid(from, to - from));
	return MacroErrorCode::Success;
}

static std::error_code toupperMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(document)
	std::string string;

	// Validate arguments and convert to int
	if(std::error_code ec = readArguments(arguments, 0, &string)) {
		return ec;
	}

	// Allocate a new string and copy an uppercased version of the string it
	for(char &ch : string) {
		ch = static_cast<char>(safe_ctype<toupper>(ch));
	}

	*result = make_value(string);
	return MacroErrorCode::Success;
}

static std::error_code tolowerMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(document)
	std::string string;

	// Validate arguments and convert to int
	if(std::error_code ec = readArguments(arguments, 0, &string)) {
		return ec;
	}

	// Allocate a new string and copy an uppercased version of the string it
	for(char &ch : string) {
		ch = static_cast<char>(safe_ctype<tolower>(ch));
	}

	*result = make_value(string);
	return MacroErrorCode::Success;
}

static std::error_code stringToClipboardMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(document)

	QString string;

	// Get the string argument
	if(std::error_code ec = readArguments(arguments, 0, &string)) {
		return ec;
	}

	QApplication::clipboard()->setText(string, QClipboard::Clipboard);

	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code clipboardToStringMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(document)

	// Should have no arguments
	if (!arguments.empty()) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	// Ask if there's a string in the clipboard, and get its length
	const QMimeData *data = QApplication::clipboard()->mimeData(QClipboard::Clipboard);
	if(!data->hasText()) {
		*result = make_value(std::string());
	} else {
		// Allocate a new string to hold the data
		*result = make_value(data->text());
	}

	return MacroErrorCode::Success;
}

/*
** Built-in macro subroutine for reading the contents of a text file into
** a string.  On success, returns 1 in $readStatus, and the contents of the
** file as a string in the subroutine return value.  On failure, returns
** the empty string "" and an 0 $readStatus.
*/
static std::error_code readFileMS(DocumentWidget *document, Arguments arguments, DataValue *result) {
	Q_UNUSED(document)

	std::string name;

	// Validate arguments
	if(std::error_code ec = readArguments(arguments, 0, &name)) {
		return ec;
	}

	// Read the whole file into an allocated string
	try {
		std::ifstream file(name, std::ios::binary);
		if(file) {
			std::string contents(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});

			*result = make_value(contents);

			// Return the results
			ReturnGlobals[READ_STATUS]->value = make_value(true);
			return MacroErrorCode::Success;
		}
	} catch(const std::ios_base::failure &ex) {
		qWarning("NEdit: Error while reading file. %s", ex.what());
	}

	ReturnGlobals[READ_STATUS]->value = make_value(false);

	*result = make_value(std::string());
	return MacroErrorCode::Success;
}

/*
** Built-in macro subroutines for writing or appending a string (parameter $1)
** to a file named in parameter $2. Returns 1 on successful write, or 0 if
** unsuccessful.
*/
static std::error_code writeFileMS(DocumentWidget *document, Arguments arguments, DataValue *result) {
	return writeOrAppendFile(false, document, arguments, result);
}

static std::error_code appendFileMS(DocumentWidget *document, Arguments arguments, DataValue *result) {
	return writeOrAppendFile(true, document, arguments, result);
}

static std::error_code writeOrAppendFile(bool append, DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(document)

	std::string name;
	std::string string;

	// Validate argument
	if(std::error_code ec = readArguments(arguments, 0, &string, &name)) {
		return ec;
	}

	std::ofstream file(name, append ? std::ios::app | std::ios::out : std::ios::out);
	if(!file) {
		*result = make_value(false);
		return MacroErrorCode::Success;
	}

	if(!file.write(string.data(), static_cast<std::streamsize>(string.size()))) {
		*result = make_value(false);
		return MacroErrorCode::Success;
	}

	// return the status
	*result = make_value(true);
	return MacroErrorCode::Success;
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
static std::error_code searchMS(DocumentWidget *document, Arguments arguments, DataValue *result) {
	DataValue newArgList[9];

	/* Use the search string routine, by adding the buffer contents as the
	 * string argument */
	if (arguments.size() > 8)
		return MacroErrorCode::WrongNumberOfArguments;

	// NOTE(eteran): the original version of this code was copy-free
	// in the interest of making the macro code simpler
	// we just make a copy here. If we duplicate the code in searchStringMS
	// here and thus don't have to pass things in a DataValue, we can once again
	// make it copy free
	newArgList[0] = make_value(document->buffer()->BufAsString());

	// copy other arguments to the new argument list
	std::copy(arguments.begin(), arguments.end(), &newArgList[1]);

	return searchStringMS(
				document,
				Arguments(newArgList, arguments.size() + 1),
				result);
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
static std::error_code searchStringMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	int64_t     beginPos;
	WrapMode    wrap;
	SearchType  type;
	QString     searchStr;
	Direction   direction;
	std::string string;

	bool found      = false;
	bool skipSearch = false;

	// Validate arguments and convert to proper types
	if (arguments.size() < 3) {
		return MacroErrorCode::TooFewArguments;
	}

	if (std::error_code ec = readArguments(arguments, 0, &string, &searchStr, &beginPos)) {
		return ec;
	}

	if (std::error_code ec = readSearchArgs(arguments.subspan(3), &direction, &type, &wrap)) {
		return ec;
	}

	auto len = static_cast<int64_t>(to_string(arguments[0]).size());
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

	Search::Result searchResult = { -1, 0, 0, 0 };

	if (!skipSearch) {
		found = Search::SearchString(
					string,
					searchStr,
					direction,
					type,
					wrap,
					beginPos,
					&searchResult,
					document->getWindowDelimiters());
	}

	// Return the results
	ReturnGlobals[SEARCH_END]->value = make_value(found ? searchResult.end : 0);
	*result = make_value(found ? searchResult.start : -1);
	return MacroErrorCode::Success;
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
static std::error_code replaceInStringMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	std::string string;
	QString searchStr;
	QString replaceStr;
	auto searchType = SearchType::Literal;
	int64_t copyStart;
	int64_t copyEnd;
	bool force = false;
	int i;

	// Validate arguments and convert to proper types
	if (arguments.size() < 3 || arguments.size() > 5) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	if (std::error_code ec = readArguments(arguments, 0, &string, &searchStr, &replaceStr)) {
		return ec;
	}

	for (i = 3; i < arguments.size(); i++) {
		// Read the optional search type and force arguments
		QString argStr;

		if (std::error_code ec = readArgument(arguments[i], &argStr)) {
			return ec;
		}

		if (!StringToSearchType(argStr, &searchType)) {
			// It's not a search type.  is it "copy"?
			if (argStr == QLatin1String("copy")) {
				force = true;
			} else {
				return MacroErrorCode::UnrecognizedArgument;
			}
		}
	}

	// Do the replace
	boost::optional<std::string> replacedStr = Search::ReplaceAllInString(
				string,
				searchStr,
				replaceStr,
				searchType,
				&copyStart,
				&copyEnd,
				document->getWindowDelimiters());

	// Return the results

	if(!replacedStr) {
		if (force) {
			*result = make_value(string);
		} else {
			*result = make_value(std::string());
		}
	} else {
		std::string new_string;
		new_string.reserve(string.size() + replacedStr->size());

		new_string.append(string.substr(0, static_cast<size_t>(copyStart)));
		new_string.append(*replacedStr);
		new_string.append(string.substr(static_cast<size_t>(copyEnd)));

		*result = make_value(new_string);
	}

	return MacroErrorCode::Success;
}

static std::error_code readSearchArgs(Arguments arguments, Direction *searchDirection, SearchType *searchType, WrapMode *wrap) {

	*wrap            = WrapMode::NoWrap;
	*searchDirection = Direction::Forward;
	*searchType      = SearchType::Literal;

	for (const DataValue &dv : arguments) {

		QString argStr;
		if (std::error_code ec = readArgument(dv, &argStr))
			return ec;
		else if (argStr == QLatin1String("wrap"))
			*wrap = WrapMode::Wrap;
		else if (argStr == QLatin1String("nowrap"))
			*wrap = WrapMode::NoWrap;
		else if (argStr == QLatin1String("backward"))
			*searchDirection = Direction::Backward;
		else if (argStr == QLatin1String("forward"))
			*searchDirection = Direction::Forward;
		else if (!StringToSearchType(argStr, searchType)) {
			return MacroErrorCode::UnrecognizedArgument;
		}
	}
	return MacroErrorCode::Success;
}

static std::error_code setCursorPosMS(DocumentWidget *document, Arguments arguments, DataValue *result) {
	int pos;

	// Get argument and convert to int
	if(std::error_code ec = readArguments(arguments, 0, &pos)) {
		return ec;
	}

	// Set the position
	TextArea *area = MainWindow::fromDocument(document)->lastFocus();
	area->TextSetCursorPos(TextCursor(pos));
	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code selectMS(DocumentWidget *document, Arguments arguments, DataValue *result) {
	int64_t start;
	int64_t end;

	// Get arguments and convert to int
	if(std::error_code ec = readArguments(arguments, 0, &start, &end)) {
		return ec;
	}

	// Verify integrity of arguments
	if (start > end) {
		std::swap(start, end);
	}

	start = qBound<int64_t>(0, start, document->buffer()->length());
	end   = qBound<int64_t>(0, end,   document->buffer()->length());

	// Make the selection
	document->buffer()->BufSelect(TextCursor(start), TextCursor(end));
	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code selectRectangleMS(DocumentWidget *document, Arguments arguments, DataValue *result) {
	int64_t start;
	int64_t end;
	int left;
	int right;

	// Get arguments and convert to int
	if(std::error_code ec = readArguments(arguments, 0, &start, &end, &left, &right)) {
		return ec;
	}

	// Make the selection
	document->buffer()->BufRectSelect(TextCursor(start), TextCursor(end), left, right);
	*result = make_value();
	return MacroErrorCode::Success;
}

/*
** Macro subroutine to ring the bell
*/
static std::error_code beepMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(document)

	if (!arguments.empty()) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	QApplication::beep();
	*result = make_value();
	return MacroErrorCode::Success;
}

static std::error_code tPrintMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(document)

	std::string string;

	if (arguments.empty()) {
		return MacroErrorCode::TooFewArguments;
	}

	for (int i = 0; i < arguments.size(); i++) {
		if (std::error_code ec = readArgument(arguments[i], &string)) {
			return ec;
		}

		printf("%s%s", string.c_str(), i == arguments.size() - 1 ? "" : " ");
	}

	fflush(stdout);
	*result = make_value();
	return MacroErrorCode::Success;
}

/*
** Built-in macro subroutine for getting the value of an environment variable
*/
static std::error_code getenvMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(document)

	std::string name;

	// Get name of variable to get
	if(std::error_code ec = readArguments(arguments, 0, &name)) {
		return MacroErrorCode::NotAString;
	}

	QByteArray value = qgetenv(name.c_str());

	// Return the text as an allocated string
	*result = make_value(QString::fromLocal8Bit(value));
	return MacroErrorCode::Success;
}

static std::error_code shellCmdMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	QString cmdString;
	QString inputString;

	if(std::error_code ec = readArguments(arguments, 0, &cmdString, &inputString)) {
		return ec;
	}

	/* Shell command execution requires that the macro be suspended, so
	   this subroutine can't be run if macro execution can't be interrupted */
	if (!MacroRunDocument()->macroCmdData_) {
		return MacroErrorCode::InvalidContext;
	}

	document->shellCmdToMacroString(cmdString, inputString);
	*result = make_value(0);
	return MacroErrorCode::Success;
}

/*
** Method used by ShellCmdToMacroString (called by shellCmdMS), for returning
** macro string and exit status after the execution of a shell command is
** complete.  (Sorry about the poor modularity here, it's just not worth
** teaching other modules about macro return globals, since other than this,
** they're not used outside of macro.c)
*/
void returnShellCommandOutput(DocumentWidget *document, const QString &outText, int status) {

	if(const std::shared_ptr<MacroCommandData> &cmdData = document->macroCmdData_) {

		DataValue retVal = make_value(outText);

		modifyReturnedValue(cmdData->context, retVal);

		ReturnGlobals[SHELL_CMD_STATUS]->value = make_value(status);
	}
}

static std::error_code dialogMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	QString btnLabel;
	QString message;

	/* Ignore the focused window passed as the function argument and put
	   the dialog up over the window which is executing the macro */
	document = MacroRunDocument();
	const std::shared_ptr<MacroCommandData> cmdData = document->macroCmdData_;

	/* Dialogs require macro to be suspended and interleaved with other macros.
	   This subroutine can't be run if macro execution can't be interrupted */
	if (!cmdData) {
		return MacroErrorCode::InvalidContext;
	}

	/* Read and check the arguments.  The first being the dialog message,
	   and the rest being the button labels */
	if (arguments.empty()) {
		return MacroErrorCode::NeedsArguments;
	}

	if (std::error_code ec = readArgument(arguments[0], &message)) {
		return ec;
	}

	// check that all button labels can be read
	for (int i = 1; i < arguments.size(); i++) {
		if (std::error_code ec = readArgument(arguments[i], &btnLabel)) {
			return ec;
		}
	}

	// Stop macro execution until the dialog is complete
	preemptMacro();

	// Return placeholder result. Value will be changed by button callback
	*result = make_value(0);

	// NOTE(eteran): passing document here breaks things...
	auto prompt = std::make_shared<DialogPrompt>(nullptr);
	prompt->setMessage(message);
	if (arguments.size() == 1) {
		prompt->addButton(QDialogButtonBox::Ok);
	} else {
		for(int i = 1; i < arguments.size(); ++i) {
			if(std::error_code ec = readArgument(arguments[i], &btnLabel)) {
				// NOTE(eteran): does not report
			}
			prompt->addButton(btnLabel);
		}
	}

	QObject::connect(prompt.get(), &QDialog::finished, [prompt, document, cmdData](int) {
		auto result = make_value(prompt->result());
		modifyReturnedValue(cmdData->context, result);

		document->resumeMacroExecution();
	});

	prompt->setWindowModality(Qt::NonModal);
	prompt->show();
	return MacroErrorCode::Success;
}

static std::error_code stringDialogMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	QString btnLabel;
	QString message;
	long i;

	/* Ignore the focused window passed as the function argument and put
	   the dialog up over the window which is executing the macro */
	document = MacroRunDocument();
	const std::shared_ptr<MacroCommandData> &cmdData = document->macroCmdData_;

	/* Dialogs require macro to be suspended and interleaved with other macros.
	   This subroutine can't be run if macro execution can't be interrupted */
	if (!cmdData) {
		return MacroErrorCode::InvalidContext;
	}

	/* Read and check the arguments.  The first being the dialog message,
	   and the rest being the button labels */
	if (arguments.empty()) {
		return MacroErrorCode::NeedsArguments;
	}

	if (std::error_code ec = readArgument(arguments[0], &message)) {
		return ec;
	}

	// check that all button labels can be read
	for (i = 1; i < arguments.size(); i++) {
		if (std::error_code ec = readArgument(arguments[i], &btnLabel)) {
			return ec;
		}
	}

	// Stop macro execution until the dialog is complete
	preemptMacro();

	// Return placeholder result.  Value will be changed by button callback
	*result = make_value(0);

	// NOTE(eteran): passing document here breaks things...
	auto prompt = std::make_shared<DialogPromptString>(nullptr);
	prompt->setMessage(message);
	if (arguments.size() == 1) {
		prompt->addButton(QDialogButtonBox::Ok);
	} else {
		for(int i = 1; i < arguments.size(); ++i) {
			if(std::error_code ec = readArgument(arguments[i], &btnLabel)) {
				// NOTE(eteran): does not report
			}
			prompt->addButton(btnLabel);
		}
	}

	QObject::connect(prompt.get(), &QDialog::finished, [prompt, document, cmdData](int) {
		// Return the button number in the global variable $string_dialog_button
		ReturnGlobals[STRING_DIALOG_BUTTON]->value = make_value(prompt->result());

		auto result = make_value(prompt->text());
		modifyReturnedValue(cmdData->context, result);

		document->resumeMacroExecution();
	});

	prompt->setWindowModality(Qt::NonModal);
	prompt->show();
	return MacroErrorCode::Success;
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
static std::error_code calltipMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	QString tipText;
	std::string txtArg;
	bool anchored = false;
	bool lookup = true;
	int i;
	int anchorPos;
	auto mode      = Tags::SearchMode::None;
	auto hAlign    = TipHAlignMode::Left;
	auto vAlign    = TipVAlignMode::Below;
	auto alignMode = TipAlignMode::Sloppy;

	// Read and check the string
	if (arguments.size() < 1) {
		return MacroErrorCode::TooFewArguments;
	}
	if (arguments.size() > 6) {
		return MacroErrorCode::TooManyArguments;
	}

	// Read the tip text or key
	if (std::error_code ec = readArgument(arguments[0], &tipText)) {
		return ec;
	}

	// Read the anchor position (-1 for unanchored)
	if (arguments.size() > 1) {
		if (std::error_code ec = readArgument(arguments[1], &anchorPos)) {
			return ec;
		}
	} else {
		anchorPos = -1;
	}
	if (anchorPos >= 0)
		anchored = true;

	// Any further args are directives for relative positioning
	for (i = 2; i < arguments.size(); ++i) {
		if (std::error_code ec = readArgument(arguments[i], &txtArg)) {
			return ec;
		}

		switch (txtArg[0]) {
		case 'c':
			if (txtArg == "center")
				return MacroErrorCode::UnrecognizedArgument;
			hAlign = TipHAlignMode::Center;
			break;
		case 'r':
			if (txtArg == "right")
				return MacroErrorCode::UnrecognizedArgument;
			hAlign = TipHAlignMode::Right;
			break;
		case 'a':
			if (txtArg == "above")
				return MacroErrorCode::UnrecognizedArgument;
			vAlign = TipVAlignMode::Above;
			break;
		case 's':
			if (txtArg == "strict")
				return MacroErrorCode::UnrecognizedArgument;
			alignMode = TipAlignMode::Strict;
			break;
		case 't':
			if (txtArg == "tipText") {
				mode = Tags::SearchMode::None;
			} else if (txtArg == "tipKey") {
				mode = Tags::SearchMode::TIP;
			} else if (txtArg == "tagKey") {
				mode = Tags::SearchMode::TIP_FROM_TAG;
			} else {
				return MacroErrorCode::UnrecognizedArgument;
			}
			break;
		default:
			return MacroErrorCode::UnrecognizedArgument;
		}
	}

	if (mode == Tags::SearchMode::None) {
		lookup = false;
	}

	// Look up (maybe) a calltip and display it
	*result = make_value(document->showTipString(
						   tipText,
						   anchored,
						   anchorPos,
						   lookup,
						   mode,
						   hAlign,
						   vAlign,
						   alignMode));

	return MacroErrorCode::Success;
}

/*
** A subroutine to kill the current calltip
*/
static std::error_code killCalltipMS(DocumentWidget *document, Arguments arguments, DataValue *result) {
	int calltipID = 0;

	if (arguments.size() > 1) {
		return MacroErrorCode::TooManyArguments;
	}
	if (arguments.size() > 0) {
		if (std::error_code ec = readArgument(arguments[0], &calltipID)) {
			return ec;
		}
	}

	MainWindow::fromDocument(document)->lastFocus()->TextDKillCalltip(calltipID);

	*result = make_value();
	return MacroErrorCode::Success;
}

/*
 * A subroutine to get the ID of the current calltip, or 0 if there is none.
 */
static std::error_code calltipIDMV(DocumentWidget *document, Arguments arguments, DataValue *result) {
	Q_UNUSED(arguments)

	*result = make_value(MainWindow::fromDocument(document)->lastFocus()->TextDGetCalltipID(0));
	return MacroErrorCode::Success;
}

static std::error_code replaceAllInSelectionMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	// replace_in_selection( search-string, replace-string [, search-type] )

	// ensure that we are dealing with the document which currently has the focus
	document = MacroRunDocument();

	//  Get the argument list.
	QString searchString;
	QString replaceString;

	if(std::error_code ec = readArguments(arguments, 0, &searchString, &replaceString)) {
		return ec;
	}

	SearchType type = searchType(arguments, 2);

	if(auto window = MainWindow::fromDocument(document)) {
		window->action_Replace_In_Selection(document, searchString, replaceString, type);
	}

	*result = make_value(0);
	return MacroErrorCode::Success;
}

static std::error_code replaceAllMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	// replace_all( search-string, replace-string [, search-type] )

	// ensure that we are dealing with the document which currently has the focus
	document = MacroRunDocument();

	if (arguments.size() < 2 || arguments.size() > 3) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	//  Get the argument list.
	QString searchString;
	QString replaceString;

	if(std::error_code ec = readArguments(arguments, 0, &searchString, &replaceString)) {
		return ec;
	}

	SearchType type = searchType(arguments, 2);

	if(auto window = MainWindow::fromDocument(document)) {
		window->action_Replace_All(document, searchString, replaceString, type);
	}

	*result = make_value(0);
	return MacroErrorCode::Success;
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
static std::error_code filenameDialogMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	QString title           = QLatin1String("Choose Filename");
	QString mode            = QLatin1String("exist");
	QString defaultPath;
	QString defaultFilter;
	QString defaultName;

	/* Ignore the focused window passed as the function argument and put
	   the dialog up over the window which is executing the macro */
	document = MacroRunDocument();

	/* Dialogs require macro to be suspended and interleaved with other macros.
	   This subroutine can't be run if macro execution can't be interrupted */
	if (!document->macroCmdData_) {
		return MacroErrorCode::InvalidContext;
	}

	//  Get the argument list.
	std::error_code ec;
	switch(arguments.size()) {
	case 0:
		break;
	case 1:
		ec = readArguments(arguments, 0, &title);
		break;
	case 2:
		ec = readArguments(arguments, 0, &title, &mode);
		break;
	case 3:
		ec = readArguments(arguments, 0, &title, &mode, &defaultPath);
		break;
	case 4:
		ec = readArguments(arguments, 0, &title, &mode, &defaultPath, &defaultFilter);
		break;
	case 5:
		ec = readArguments(arguments, 0, &title, &mode, &defaultPath, &defaultFilter, &defaultName);
		break;
	default:
		ec = MacroErrorCode::TooManyArguments;
	}

	if (ec) {
		return ec;
	}

	if ((mode != QLatin1String("exist")) != 0 && (mode != QLatin1String("new"))) {
		return MacroErrorCode::InvalidMode;
	}

	//  Set default directory
	if (defaultPath.isEmpty()) {
		defaultPath = document->path();
	}

	//  Set default filter
	if(defaultFilter.isEmpty()) {
		defaultFilter = QLatin1String("*");
	}

	QString filename;
	if (mode == QLatin1String("exist")) {
		// NOTE(eteran); filters probably don't work quite the same with Qt's dialog
		filename = QFileDialog::getOpenFileName(nullptr, title, defaultPath, defaultFilter, nullptr);
	} else {
		// NOTE(eteran); filters probably don't work quite the same with Qt's dialog
		filename = QFileDialog::getSaveFileName(nullptr, title, defaultPath, defaultFilter, nullptr);
	}

	if (!filename.isNull()) {
		*result = make_value(filename);
	} else {
		// User cancelled.  Return ""
		*result = make_value(std::string());
	}

	return MacroErrorCode::Success;
}

// T Balinski
static std::error_code listDialogMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	QString btnLabel;
	QString message;
	QString text;
	long i;

	/* Ignore the focused window passed as the function argument and put
	   the dialog up over the window which is executing the macro */
	document = MacroRunDocument();
	const std::shared_ptr<MacroCommandData> &cmdData = document->macroCmdData_;

	/* Dialogs require macro to be suspended and interleaved with other macros.
	   This subroutine can't be run if macro execution can't be interrupted */
	if (!cmdData) {
		return MacroErrorCode::InvalidContext;
	}

	/* Read and check the arguments.  The first being the dialog message,
	   and the rest being the button labels */
	if (arguments.size() < 2) {
		return MacroErrorCode::TooFewArguments;
	}

	if (std::error_code ec = readArgument(arguments[0], &message)) {
		return ec;
	}

	if (std::error_code ec = readArgument(arguments[1], &text)) {
		return ec;
	}

	if (text.isEmpty()) {
		return MacroErrorCode::EmptyList;
	}

	// check that all button labels can be read
	for (i = 2; i < arguments.size(); i++) {
		if (std::error_code ec = readArgument(arguments[i], &btnLabel)) {
			return ec;
		}
	}

	// Stop macro execution until the dialog is complete
	preemptMacro();

	// Return placeholder result.  Value will be changed by button callback
	*result = make_value(0);

	// NOTE(eteran): passing document here breaks things...
	auto prompt = std::make_shared<DialogPromptList>(nullptr);
	prompt->setMessage(message);
	prompt->setList(text);

	if (arguments.size() == 2) {
		prompt->addButton(QDialogButtonBox::Ok);
	} else {
		for(int i = 2; i < arguments.size(); ++i) {
			if(std::error_code ec = readArgument(arguments[i], &btnLabel)) {
				// NOTE(eteran): does not report
			}
			prompt->addButton(btnLabel);
		}
	}

	QObject::connect(prompt.get(), &QDialog::finished, [prompt, document, cmdData](int) {
		// Return the button number in the global variable $list_dialog_button
		ReturnGlobals[LIST_DIALOG_BUTTON]->value = make_value(prompt->result());

		auto result = make_value(prompt->text());
		modifyReturnedValue(cmdData->context, result);

		document->resumeMacroExecution();
	});

	prompt->setWindowModality(Qt::NonModal);
	prompt->show();
	return MacroErrorCode::Success;
}

static std::error_code stringCompareMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(document)

	QString leftStr;
	QString rightStr;
	QString argStr;
	bool considerCase = true;
	int i;
	int compareResult;

	if (arguments.size() < 2) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	if (std::error_code ec = readArguments(arguments, 0, &leftStr, &rightStr)) {
		return ec;
	}

	for (i = 2; i < arguments.size(); ++i) {
		if (std::error_code ec = readArgument(arguments[i], &argStr)) {
			return ec;
		} else if (argStr == QLatin1String("case")) {
			considerCase = true;
		} else if (argStr == QLatin1String("nocase")) {
			considerCase = false;
		} else {
			return MacroErrorCode::UnrecognizedArgument;
		}
	}

	if (considerCase) {
		compareResult = leftStr.compare(rightStr, Qt::CaseSensitive);
	} else {
		compareResult = leftStr.compare(rightStr, Qt::CaseInsensitive);
	}

	compareResult = qBound(-1, compareResult, 1);

	*result = make_value(compareResult);
	return MacroErrorCode::Success;
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

static std::error_code splitMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	std::string sourceStr;
	QString splitStr;
	SearchType searchType = SearchType::Literal;
	DataValue element;
	std::error_code ec;

	if (arguments.size() < 2) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	if ((ec = readArgument(arguments[0], &sourceStr))) {
		return MacroErrorCode::Param1NotAString;
	}

	if ((ec = readArgument(arguments[1], &splitStr)) || splitStr.isEmpty()) {
		return MacroErrorCode::Param2CannotBeEmptyString;
	}

	QString typeSplitStr;
	if (arguments.size() > 2) {
		if ((ec = readArgument(arguments[2], &typeSplitStr)) || !StringToSearchType(typeSplitStr, &searchType)) {
			return MacroErrorCode::UnrecognizedArgument;
		}
	}

	*result = make_value(std::make_shared<Array>());

	int64_t beginPos = 0;
	int64_t lastEnd  = 0;
	int indexNum     = 0;
	auto strLength   = static_cast<int64_t>(sourceStr.size());
	bool found       = true;

	Search::Result searchResult;

	while (found && beginPos < strLength) {

		auto indexStr = std::to_string(indexNum);

		found = Search::SearchString(
					sourceStr,
					splitStr,
					Direction::Forward,
					searchType,
					WrapMode::NoWrap,
					beginPos,
					&searchResult,
					document->getWindowDelimiters());

		int64_t elementEnd = found ? searchResult.start : strLength;
		int64_t elementLen = elementEnd - lastEnd;

		view::string_view str(
					&sourceStr[static_cast<size_t>(lastEnd)],
					static_cast<size_t>(elementLen));

		element = make_value(str);
		if (!ArrayInsert(result, indexStr, &element)) {
			return MacroErrorCode::InsertFailed;
		}

		if (found) {
			if (searchResult.start == searchResult.end) {
				beginPos = searchResult.end + 1; // Avoid endless loop for 0-width match
			} else {
				beginPos = searchResult.end;
			}
		} else {
			beginPos = strLength; // Break the loop
		}

		lastEnd = searchResult.end;
		++indexNum;
	}

	if (found) {

		auto indexStr = std::to_string(indexNum);

		if (lastEnd == strLength) {
			// The pattern mathed the end of the string. Add an empty chunk.
			element = make_value(std::string());

			if (!ArrayInsert(result, indexStr, &element)) {
				return MacroErrorCode::InsertFailed;
			}
		} else {
			/* We skipped the last character to prevent an endless loop.
			   Add it to the list. */
			int64_t elementLen = strLength - lastEnd;
			view::string_view str(&sourceStr[static_cast<size_t>(lastEnd)], static_cast<size_t>(elementLen));

			element = make_value(str);
			if (!ArrayInsert(result, indexStr, &element)) {
				return MacroErrorCode::InsertFailed;
			}

			/* If the pattern can match zero-length strings, we may have to
			   add a final empty chunk.
			   For instance:  split("abc\n", "$", "regex")
				 -> matches before \n and at end of string
				 -> expected output: "abc", "\n", ""
			   The '\n' gets added in the lines above, but we still have to
			   verify whether the pattern also matches the end of the string,
			   and add an empty chunk in case it does. */
			found = Search::SearchString(
						sourceStr,
						splitStr,
						Direction::Forward,
						searchType,
						WrapMode::NoWrap,
						strLength,
						&searchResult,
						document->getWindowDelimiters());

			if (found) {
				++indexNum;

				auto indexStr = std::to_string(indexNum);

				element = make_value();
				if (!ArrayInsert(result, indexStr, &element)) {
					return MacroErrorCode::InsertFailed;
				}
			}
		}
	}
	return MacroErrorCode::Success;
}

/*
** Set the backlighting string resource for the current window. If no parameter
** is passed or the value "default" is passed, it attempts to set the preference
** value of the resource. If the empty string is passed, the backlighting string
** will be cleared, turning off backlighting.
*/
#if defined(ENABLE_BACKLIGHT_STRING)
static std::error_code setBacklightStringMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	QString backlightString;

	if (arguments.empty()) {
		backlightString = GetPrefBacklightCharTypes();
	} else if (arguments.size() == 1) {
		if (!is_string(arguments[0])) {
			return MacroErrorCode::NotAString;
		}
		backlightString = QString::fromStdString(to_string(arguments[0]));
	} else {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	if (backlightString == QLatin1String("default")) {
		backlightString = GetPrefBacklightCharTypes();
	}

	if (backlightString.isEmpty()) {
		backlightString = QString();  /* turns off backlighting */
	}

	document->SetBacklightChars(backlightString);
	*result = to_value();
	return MacroErrorCode::Success;
}
#endif

static std::error_code cursorMV(DocumentWidget *document, Arguments arguments, DataValue *result) {

	if(!arguments.empty()) {
		return MacroErrorCode::TooManyArguments;
	}

	TextArea *area  = MainWindow::fromDocument(document)->lastFocus();
	*result         = make_value(to_integer(area->TextGetCursorPos()));
	return MacroErrorCode::Success;
}

static std::error_code lineMV(DocumentWidget *document, Arguments arguments, DataValue *result) {

	if(!arguments.empty()) {
		return MacroErrorCode::TooManyArguments;
	}

	TextBuffer *buf      = document->buffer();
	TextArea *area       = MainWindow::fromDocument(document)->lastFocus();
	TextCursor cursorPos = area->TextGetCursorPos();

	if(const boost::optional<Location> loc = area->TextDPosToLineAndCol(cursorPos)) {
		*result = make_value(loc->line);
	} else {
		*result = make_value(buf->BufCountLines(buf->BufStartOfBuffer(), cursorPos) + 1);
	}

	return MacroErrorCode::Success;
}

static std::error_code columnMV(DocumentWidget *document, Arguments arguments, DataValue *result) {

	if(!arguments.empty()) {
		return MacroErrorCode::TooManyArguments;
	}

	TextBuffer *buf      = document->buffer();
	TextArea *area       = MainWindow::fromDocument(document)->lastFocus();
	TextCursor cursorPos = area->TextGetCursorPos();

	*result = make_value(buf->BufCountDispChars(buf->BufStartOfLine(cursorPos), cursorPos));
	return MacroErrorCode::Success;
}

static std::error_code fileNameMV(DocumentWidget *document, Arguments arguments, DataValue *result) {

	if(!arguments.empty()) {
		return MacroErrorCode::TooManyArguments;
	}

	*result = make_value(document->filename());
	return MacroErrorCode::Success;
}

static std::error_code filePathMV(DocumentWidget *document, Arguments arguments, DataValue *result) {

	if(!arguments.empty()) {
		return MacroErrorCode::TooManyArguments;
	}

	*result = make_value(document->path());
	return MacroErrorCode::Success;
}

static std::error_code lengthMV(DocumentWidget *document, Arguments arguments, DataValue *result) {

	if(!arguments.empty()) {
		return MacroErrorCode::TooManyArguments;
	}

	*result = make_value(document->buffer()->length());
	return MacroErrorCode::Success;
}

static std::error_code selectionStartMV(DocumentWidget *document, Arguments arguments, DataValue *result) {

	if(!arguments.empty()) {
		return MacroErrorCode::TooManyArguments;
	}

	*result = make_value(document->buffer()->primary.hasSelection() ? to_integer(document->buffer()->primary.start()) : -1);
	return MacroErrorCode::Success;
}

static std::error_code selectionEndMV(DocumentWidget *document, Arguments arguments, DataValue *result) {

	if(!arguments.empty()) {
		return MacroErrorCode::TooManyArguments;
	}

	*result = make_value(document->buffer()->primary.hasSelection() ? to_integer(document->buffer()->primary.end()) : -1);
	return MacroErrorCode::Success;
}

static std::error_code selectionLeftMV(DocumentWidget *document, Arguments arguments, DataValue *result) {

	if(!arguments.empty()) {
		return MacroErrorCode::TooManyArguments;
	}

	const TextBuffer::Selection *sel = &document->buffer()->primary;

	*result = make_value(sel->hasSelection() && sel->isRectangular() ? sel->rectStart() : -1);
	return MacroErrorCode::Success;
}

static std::error_code selectionRightMV(DocumentWidget *document, Arguments arguments, DataValue *result) {

	if(!arguments.empty()) {
		return MacroErrorCode::TooManyArguments;
	}

	const TextBuffer::Selection *sel = &document->buffer()->primary;

	*result = make_value(sel->hasSelection() && sel->isRectangular() ? sel->rectEnd() : -1);
	return MacroErrorCode::Success;
}

static std::error_code wrapMarginMV(DocumentWidget *document, Arguments arguments, DataValue *result) {

	if(!arguments.empty()) {
		return MacroErrorCode::TooManyArguments;
	}

	int margin = document->firstPane()->getWrapMargin();
	int nCols  = document->firstPane()->getColumns();

	*result = make_value((margin == 0) ? nCols : margin);
	return MacroErrorCode::Success;
}

static std::error_code statisticsLineMV(DocumentWidget *document, Arguments arguments, DataValue *result) {

	if(!arguments.empty()) {
		return MacroErrorCode::TooManyArguments;
	}

	*result = make_value(document->showStats_ ? 1 : 0);
	return MacroErrorCode::Success;
}

static std::error_code incSearchLineMV(DocumentWidget *document, Arguments arguments, DataValue *result) {

	if(!arguments.empty()) {
		return MacroErrorCode::TooManyArguments;
	}

	*result = make_value(MainWindow::fromDocument(document)->getIncrementalSearchLine() ? 1 : 0);
	return MacroErrorCode::Success;
}

static std::error_code showLineNumbersMV(DocumentWidget *document, Arguments arguments, DataValue *result) {

	if(!arguments.empty()) {
		return MacroErrorCode::TooManyArguments;
	}

	*result = make_value(MainWindow::fromDocument(document)->getShowLineNumbers() ? 1 : 0);
	return MacroErrorCode::Success;
}

static std::error_code autoIndentMV(DocumentWidget *document, Arguments arguments, DataValue *result) {

	if(!arguments.empty()) {
		return MacroErrorCode::TooManyArguments;
	}

	if(document->autoIndentStyle() == IndentStyle::Default) {
		return MacroErrorCode::InvalidIndentStyle;
	}

	QLatin1String res = to_string(document->autoIndentStyle());
	*result = make_value(res);
	return MacroErrorCode::Success;
}

static std::error_code wrapTextMV(DocumentWidget *document, Arguments arguments, DataValue *result) {

	if(!arguments.empty()) {
		return MacroErrorCode::TooManyArguments;
	}

	if(document->wrapMode() == WrapStyle::Default) {
		return MacroErrorCode::InvalidWrapStyle;
	}

	QLatin1String res = to_string(document->wrapMode());
	*result = make_value(res);
	return MacroErrorCode::Success;
}

static std::error_code highlightSyntaxMV(DocumentWidget *document, Arguments arguments, DataValue *result) {
	Q_UNUSED(arguments)

	*result = make_value(document->highlightSyntax_ ? 1 : 0);
	return MacroErrorCode::Success;
}

static std::error_code makeBackupCopyMV(DocumentWidget *document, Arguments arguments, DataValue *result) {
	Q_UNUSED(arguments)

	*result = make_value(document->makeBackupCopy() ? 1 : 0);
	return MacroErrorCode::Success;
}

static std::error_code incBackupMV(DocumentWidget *document, Arguments arguments, DataValue *result) {
	Q_UNUSED(arguments)

	*result = make_value(document->incrementalBackup() ? 1 : 0);
	return MacroErrorCode::Success;
}

static std::error_code showMatchingMV(DocumentWidget *document, Arguments arguments, DataValue *result) {
	Q_UNUSED(arguments)

	QLatin1String res = to_string(document->showMatchingStyle());
	*result = make_value(res);
	return MacroErrorCode::Success;
}

static std::error_code matchSyntaxBasedMV(DocumentWidget *document, Arguments arguments, DataValue *result) {
	Q_UNUSED(arguments)

	*result = make_value(document->matchSyntaxBased() ? 1 : 0);
	return MacroErrorCode::Success;
}

static std::error_code overTypeModeMV(DocumentWidget *document, Arguments arguments, DataValue *result) {
	Q_UNUSED(arguments)

	*result = make_value(document->overstrike() ? 1 : 0);
	return MacroErrorCode::Success;
}

static std::error_code readOnlyMV(DocumentWidget *document, Arguments arguments, DataValue *result) {
	Q_UNUSED(arguments)

	*result = make_value((document->lockReasons().isAnyLocked()) ? 1 : 0);
	return MacroErrorCode::Success;
}

static std::error_code lockedMV(DocumentWidget *document, Arguments arguments, DataValue *result) {
	Q_UNUSED(arguments)

	*result = make_value((document->lockReasons().isUserLocked()) ? 1 : 0);
	return MacroErrorCode::Success;
}

static std::error_code fileFormatMV(DocumentWidget *document, Arguments arguments, DataValue *result) {
	Q_UNUSED(arguments)

	QLatin1String res = to_string(document->fileFormat());
	*result = make_value(res);
	return MacroErrorCode::Success;
}

static std::error_code fontNameMV(DocumentWidget *document, Arguments arguments, DataValue *result) {
	Q_UNUSED(arguments)

	*result = make_value(document->fontName_);
	return MacroErrorCode::Success;
}

static std::error_code fontNameItalicMV(DocumentWidget *document, Arguments arguments, DataValue *result) {
	Q_UNUSED(arguments)

	qWarning("NEdit: seperate italic fonts are not longer supported");

	// NOTE(eteran): used to be italicFontName_
	*result = make_value(document->fontName_);
	return MacroErrorCode::Success;
}

static std::error_code fontNameBoldMV(DocumentWidget *document, Arguments arguments, DataValue *result) {
	Q_UNUSED(arguments)

	qWarning("NEdit: seperate bold fonts are not longer supported");

	// NOTE(eteran): used to be boldFontName_
	*result = make_value(document->fontName_);
	return MacroErrorCode::Success;
}

static std::error_code fontNameBoldItalicMV(DocumentWidget *document, Arguments arguments, DataValue *result) {
	Q_UNUSED(arguments)

	qWarning("NEdit: seperate bold-italic fonts are not longer supported");

	// NOTE(eteran): used to be boldItalicFontName_
	*result = make_value(document->fontName_);
	return MacroErrorCode::Success;
}

static std::error_code subscriptSepMV(DocumentWidget *document, Arguments arguments, DataValue *result) {
	Q_UNUSED(document)
	Q_UNUSED(arguments)

	*result = make_value(view::string_view(ARRAY_DIM_SEP, 1));
	return MacroErrorCode::Success;
}

static std::error_code minFontWidthMV(DocumentWidget *document, Arguments arguments, DataValue *result) {
	Q_UNUSED(arguments)

	TextArea *area = document->firstPane();
	*result = make_value(area->TextDMinFontWidth());
	return MacroErrorCode::Success;
}

static std::error_code maxFontWidthMV(DocumentWidget *document, Arguments arguments, DataValue *result) {
	Q_UNUSED(arguments)

	TextArea *area = document->firstPane();
	*result = make_value(area->TextDMaxFontWidth());
	return MacroErrorCode::Success;
}

static std::error_code topLineMV(DocumentWidget *document, Arguments arguments, DataValue *result) {
	Q_UNUSED(arguments)

	TextArea *area = MainWindow::fromDocument(document)->lastFocus();
	*result = make_value(area->TextFirstVisibleLine());
	return MacroErrorCode::Success;
}

static std::error_code numDisplayLinesMV(DocumentWidget *document, Arguments arguments, DataValue *result) {
	Q_UNUSED(arguments)

	TextArea *area    = MainWindow::fromDocument(document)->lastFocus();
	*result = make_value(area->TextNumVisibleLines());
	return MacroErrorCode::Success;
}

static std::error_code displayWidthMV(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(arguments)

	TextArea *area    = MainWindow::fromDocument(document)->lastFocus();
	*result = make_value(area->TextVisibleWidth());
	return MacroErrorCode::Success;
}

static std::error_code activePaneMV(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(arguments)

	*result = make_value(document->widgetToPaneIndex(MainWindow::fromDocument(document)->lastFocus()));
	return MacroErrorCode::Success;
}

static std::error_code nPanesMV(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(arguments)

	*result = make_value(document->textPanesCount());
	return MacroErrorCode::Success;
}

static std::error_code emptyArrayMV(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(document)
	Q_UNUSED(arguments)

	*result = make_value(ArrayPtr());
	return MacroErrorCode::Success;
}

static std::error_code serverNameMV(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(document)
	Q_UNUSED(arguments)

	*result = make_value(Preferences::GetPrefServerName());
	return MacroErrorCode::Success;
}

static std::error_code tabDistMV(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(arguments)

	*result = make_value(document->buffer()->BufGetTabDistance());
	return MacroErrorCode::Success;
}

static std::error_code emTabDistMV(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(arguments)

	int dist = document->firstPane()->getEmulateTabs();

	*result = make_value(dist);
	return MacroErrorCode::Success;
}

static std::error_code useTabsMV(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(arguments)

	*result = make_value(document->buffer()->BufGetUseTabs());
	return MacroErrorCode::Success;
}

static std::error_code modifiedMV(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(arguments)

	*result = make_value(document->fileChanged());
	return MacroErrorCode::Success;
}

static std::error_code languageModeMV(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(arguments)

	QString lmName = Preferences::LanguageModeName(document->getLanguageMode());

	if(lmName.isNull()) {
		lmName = QLatin1String("Plain");
	}

	*result = make_value(lmName);
	return MacroErrorCode::Success;
}

// --------------------------------------------------------------------------

/*
** Range set macro variables and functions
*/
static std::error_code rangesetListMV(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(arguments)

	const std::shared_ptr<RangesetTable> &rangesetTable = document->rangesetTable_;
	DataValue element;

	*result = make_value(std::make_shared<Array>());

	if(!rangesetTable) {
		return MacroErrorCode::Success;
	}

	std::vector<uint8_t> rangesetList = rangesetTable->labels();
	size_t nRangesets = rangesetList.size();
	for (size_t i = 0; i < nRangesets; i++) {

		element = make_value(rangesetList[i]);

		if (!ArrayInsert(result, std::to_string(nRangesets - i - 1), &element)) {
			return MacroErrorCode::InsertFailed;
		}
	}

	return MacroErrorCode::Success;
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
static std::error_code versionMV(DocumentWidget *document, Arguments arguments, DataValue *result) {

	Q_UNUSED(arguments)
	Q_UNUSED(document)

	*result = make_value(NEDIT_VERSION);
	return MacroErrorCode::Success;
}

/*
** Built-in macro subroutine to create a new rangeset or rangesets.
** If called with one argument: $1 is the number of rangesets required and
** return value is an array indexed 0 to n, with the rangeset labels as values;
** (or an empty array if the requested number of rangesets are not available).
** If called with no arguments, returns a single rangeset label (not an array),
** or an empty string if there are no rangesets available.
*/
static std::error_code rangesetCreateMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	std::shared_ptr<RangesetTable> &rangesetTable = document->rangesetTable_;

	if (arguments.size() > 1)
		return MacroErrorCode::WrongNumberOfArguments;

	if(!rangesetTable) {
		rangesetTable = std::make_shared<RangesetTable>(document->buffer());
	}

	if (arguments.empty()) {
		int label = rangesetTable->RangesetCreate();
		*result = make_value(label);
		return MacroErrorCode::Success;
	} else {

		int nRangesetsRequired;
		if (std::error_code ec = readArgument(arguments[0], &nRangesetsRequired)) {
			return ec;
		}

		*result = make_value(std::make_shared<Array>());

		if (nRangesetsRequired > rangesetTable->rangesetsAvailable())
			return MacroErrorCode::Success;

		for (int i = 0; i < nRangesetsRequired; i++) {
			DataValue element = make_value(rangesetTable->RangesetCreate());
			(void)ArrayInsert(result, std::to_string(i), &element);
		}

		return MacroErrorCode::Success;
	}
}

/*
** Built-in macro subroutine for forgetting a range set.
*/
static std::error_code rangesetDestroyMS(DocumentWidget *document, Arguments arguments, DataValue *result) {
	const std::shared_ptr<RangesetTable> &rangesetTable = document->rangesetTable_;
	DataValue element;
	int label = 0;

	if (arguments.size() != 1) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	if (is_array(arguments[0])) {
		DataValue *array = &arguments[0];
		int arraySize = ArraySize(array);

		if (arraySize > N_RANGESETS) {
			return MacroErrorCode::ArrayFull;
		}

		int deleteLabels[N_RANGESETS];

		for (int i = 0; i < arraySize; i++) {

			auto keyString = std::to_string(i);

			if (!ArrayGet(array, keyString, &element)) {
				return MacroErrorCode::InvalidArrayKey;
			}

			std::error_code ec;
			if ((ec = readArgument(element, &label)) || !RangesetTable::LabelOK(label)) {
				return MacroErrorCode::InvalidRangesetLabelInArray;
			}

			deleteLabels[i] = label;
		}

		for (int i = 0; i < arraySize; i++) {
			rangesetTable->forgetLabel(deleteLabels[i]);
		}
	} else {
		std::error_code ec;
		if ((ec = readArgument(arguments[0], &label)) || !RangesetTable::LabelOK(label)) {
			return MacroErrorCode::InvalidRangesetLabel;
		}

		if (rangesetTable) {
			rangesetTable->forgetLabel(label);
		}
	}

	// set up result
	*result = make_value();
	return MacroErrorCode::Success;
}

/*
** Built-in macro subroutine for getting all range sets with a specfic name.
** Arguments are $1: range set name.
** return value is an array indexed 0 to n, with the rangeset labels as values;
*/
static std::error_code rangesetGetByNameMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	QString name;
	const std::shared_ptr<RangesetTable> &rangesetTable = document->rangesetTable_;
	int insertIndex = 0;
	DataValue element;

	if(std::error_code ec = readArguments(arguments, 0, &name)) {
		return MacroErrorCode::Param1NotAString;
	}

	*result = make_value(std::make_shared<Array>());

	if(!rangesetTable) {
		return MacroErrorCode::Success;
	}

	std::vector<uint8_t> rangesetList = rangesetTable->labels();
	size_t nRangesets = rangesetList.size();
	for (size_t i = 0; i < nRangesets; ++i) {
		int label = rangesetList[i];
		if (Rangeset *rangeset = rangesetTable->RangesetFetch(label)) {
			QString rangeset_name = rangeset->name();

			if(rangeset_name == name) {

				element = make_value(label);

				if (!ArrayInsert(result, std::to_string(insertIndex), &element)) {
					return MacroErrorCode::InsertFailed;
				}

				++insertIndex;
			}
		}
	}

	return MacroErrorCode::Success;
}

/*
** Built-in macro subroutine for adding to a range set. Arguments are $1: range
** set label (one integer), then either (a) $2: source range set label,
** (b) $2: int start-range, $3: int end-range, (c) nothing (use selection
** if any to specify range to add - must not be rectangular). Returns the
** index of the newly added range (cases b and c), or 0 (case a).
*/
static std::error_code rangesetAddMS(DocumentWidget *document, Arguments arguments, DataValue *result) {
	TextBuffer *buffer = document->buffer();
	const std::shared_ptr<RangesetTable> &rangesetTable = document->rangesetTable_;
	TextCursor start;
	TextCursor end;
	int64_t rectStart;
	int64_t rectEnd;
	int64_t index;
	bool isRect;
	int label = 0;

	if (arguments.size() < 1 || arguments.size() > 3)
		return MacroErrorCode::WrongNumberOfArguments;

	std::error_code ec;
	if ((ec = readArgument(arguments[0], &label)) || !RangesetTable::LabelOK(label)) {
		return MacroErrorCode::Param1InvalidRangesetLabel;
	}

	if(!rangesetTable) {
		return MacroErrorCode::RangesetDoesNotExist;
	}

	Rangeset *targetRangeset = rangesetTable->RangesetFetch(label);
	if(!targetRangeset) {
		return MacroErrorCode::RangesetDoesNotExist;
	}

	start = TextCursor(-1);
	end   = TextCursor(-1);

	if (arguments.size() == 1) {
		// pick up current selection in this window
		if (!buffer->BufGetSelectionPos(&start, &end, &isRect, &rectStart, &rectEnd) || isRect) {
			return MacroErrorCode::SelectionMissing;
		}
		if (!targetRangeset->RangesetAdd({ start, end })) {
			return MacroErrorCode::FailedToAddSelection;
		}
	}

	if (arguments.size() == 2) {
		// add ranges taken from a second set
		std::error_code ec;
		if ((ec = readArgument(arguments[1], &label)) || !RangesetTable::LabelOK(label)) {
			return MacroErrorCode::Param2InvalidRangesetLabel;
		}

		Rangeset *sourceRangeset = rangesetTable->RangesetFetch(label);
		if(!sourceRangeset) {
			return MacroErrorCode::Rangeset2DoesNotExist;
		}

		targetRangeset->RangesetAdd(*sourceRangeset);
	}

	if (arguments.size() == 3) {
		// add a range bounded by the start and end positions in $2, $3
		int64_t tmp_start;
		int64_t tmp_end;
		if (std::error_code ec = readArguments(arguments, 1, &tmp_start, &tmp_end)) {
			return ec;
		}

		start = TextCursor(tmp_start);
		end   = TextCursor(tmp_end);

		// make sure range is in order and fits buffer size
		const TextCursor maxpos = TextCursor(buffer->length());
		start = qBound(TextCursor(), start, maxpos);
		end   = qBound(TextCursor(), end,   maxpos);

		if (start > end) {
			std::swap(start, end);
		}

		if ((start != end) && !targetRangeset->RangesetAdd({ start, end })) {
			return MacroErrorCode::FailedToAddRange;
		}
	}

	// (to) which range did we just add?
	if (arguments.size() != 2 && start >= 0) {
		start = (start + to_integer(end)) / 2; // "middle" of added range
		index = 1 + targetRangeset->RangesetFindRangeOfPos(start, /*incl_end=*/false);
	} else {
		index = 0;
	}

	// set up result
	*result = make_value(index);
	return MacroErrorCode::Success;
}

/*
** Built-in macro subroutine for removing from a range set. Almost identical to
** rangesetAddMS() - only changes are from RangesetAdd() to RangesetSubtract(),
** the handling of an undefined destination range, and that it returns no value.
*/
static std::error_code rangesetSubtractMS(DocumentWidget *document, Arguments arguments, DataValue *result) {
	TextBuffer *buffer = document->buffer();
	const std::shared_ptr<RangesetTable> &rangesetTable = document->rangesetTable_;
	int64_t start;
	int64_t end;
	int64_t rectStart;
	int64_t rectEnd;
	bool isRect;
	int label = 0;

	if (arguments.size() < 1 || arguments.size() > 3) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	std::error_code ec;
	if ((ec = readArgument(arguments[0], &label)) || !RangesetTable::LabelOK(label)) {
		return MacroErrorCode::Param1InvalidRangesetLabel;
	}

	if(!rangesetTable) {
		return MacroErrorCode::RangesetDoesNotExist;
	}

	Rangeset *targetRangeset = rangesetTable->RangesetFetch(label);
	if(!targetRangeset) {
		return MacroErrorCode::RangesetDoesNotExist;
	}

	if (arguments.size() == 1) {
		// remove current selection in this window
		TextCursor tmp_start;
		TextCursor tmp_end;
		if (!buffer->BufGetSelectionPos(&tmp_start, &tmp_end, &isRect, &rectStart, &rectEnd) || isRect) {
			return MacroErrorCode::SelectionMissing;
		}

		start = to_integer(tmp_start);
		end   = to_integer(tmp_end);

		targetRangeset->RangesetRemove({ tmp_start, tmp_end });
	}

	if (arguments.size() == 2) {
		// remove ranges taken from a second set
		if ((ec = readArgument(arguments[1], &label)) || !RangesetTable::LabelOK(label)) {
			return MacroErrorCode::Param2InvalidRangesetLabel;
		}

		Rangeset *sourceRangeset = rangesetTable->RangesetFetch(label);
		if(!sourceRangeset) {
			return MacroErrorCode::Rangeset2DoesNotExist;
		}
		targetRangeset->RangesetRemove(*sourceRangeset);
	}

	if (arguments.size() == 3) {
		// remove a range bounded by the start and end positions in $2, $3
		if (std::error_code ec = readArgument(arguments[1], &start)) {
			return ec;
		}

		if (std::error_code ec = readArgument(arguments[2], &end)) {
			return ec;
		}

		// make sure range is in order and fits buffer size
		const int64_t maxpos = buffer->length();
		start = qBound<int64_t>(0, start, maxpos);
		end   = qBound<int64_t>(0, end, maxpos);

		if (start > end) {
			std::swap(start, end);
		}

		targetRangeset->RangesetRemove({ TextCursor(start), TextCursor(end) });
	}

	// set up result
	*result = make_value();
	return MacroErrorCode::Success;
}

/*
** Built-in macro subroutine to invert a range set. Argument is $1: range set
** label (one alphabetic character). Returns nothing. Fails if range set
** undefined.
*/
static std::error_code rangesetInvertMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	const std::shared_ptr<RangesetTable> &rangesetTable = document->rangesetTable_;
	int label;

	if(std::error_code ec = readArguments(arguments, 0, &label)) {
		return ec;
	}

	if (!RangesetTable::LabelOK(label)) {
		return MacroErrorCode::Param1InvalidRangesetLabel;
	}

	if(!rangesetTable) {
		return MacroErrorCode::RangesetDoesNotExist;
	}

	Rangeset *rangeset = rangesetTable->RangesetFetch(label);
	if(!rangeset) {
		return MacroErrorCode::RangesetDoesNotExist;
	}

	if (rangeset->RangesetInverse() < 0) {
		return MacroErrorCode::FailedToInvertRangeset;
	}

	// set up result
	*result = make_value();
	return MacroErrorCode::Success;
}

/*
** Built-in macro subroutine for finding out info about a rangeset.  Takes one
** argument of a rangeset label.  Returns an array with the following keys:
**    defined, count, color, mode.
*/
static std::error_code rangesetInfoMS(DocumentWidget *document, Arguments arguments, DataValue *result) {
	const std::shared_ptr<RangesetTable> &rangesetTable = document->rangesetTable_;
	DataValue element;
	int label;

	if(std::error_code ec = readArguments(arguments, 0, &label)) {
		return ec;
	}

	if (!RangesetTable::LabelOK(label)) {
		return MacroErrorCode::Param1InvalidRangesetLabel;
	}

	RangesetInfo rangeset_info;

	if (rangesetTable) {
		if(Rangeset *rangeset = rangesetTable->RangesetFetch(label)) {
			rangeset_info = rangeset->RangesetGetInfo();
		}
	}


	// set up result
	*result = make_value(std::make_shared<Array>());

	element = make_value(rangeset_info.defined);
	if (!ArrayInsert(result, "defined", &element)) {
		return MacroErrorCode::InsertFailed;
	}

	element = make_value(rangeset_info.count);
	if (!ArrayInsert(result, "count", &element)) {
		return MacroErrorCode::InsertFailed;
	}

	element = make_value(rangeset_info.color);
	if (!ArrayInsert(result, "color", &element)) {
		return MacroErrorCode::InsertFailed;
	}

	element = make_value(rangeset_info.name);
	if (!ArrayInsert(result, "name", &element)) {
		return MacroErrorCode::InsertFailed;
	}

	element = make_value(rangeset_info.mode);
	if (!ArrayInsert(result, "mode", &element)) {
		return MacroErrorCode::InsertFailed;
	}

	return MacroErrorCode::Success;
}

/*
** Built-in macro subroutine for finding the extent of a range in a set.
** If only one parameter is supplied, use the spanning range of all
** ranges, otherwise select the individual range specified.  Returns
** an array with the keys "start" and "end" and values
*/
static std::error_code rangesetRangeMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	const std::shared_ptr<RangesetTable> &rangesetTable = document->rangesetTable_;
	DataValue element;
	int label = 0;

	if (arguments.size() < 1 || arguments.size() > 2) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	std::error_code ec;
	if ((ec = readArgument(arguments[0], &label)) || !RangesetTable::LabelOK(label)) {
		return MacroErrorCode::Param1InvalidRangesetLabel;
	}

	if(!rangesetTable) {
		return MacroErrorCode::RangesetDoesNotExist;
	}

QT_WARNING_PUSH
QT_WARNING_DISABLE_GCC("-Wmaybe-uninitialized") // NOTE(eteran): GCC 7+ false positive

	boost::optional<TextRange> range;

	if (Rangeset *rangeset = rangesetTable->RangesetFetch(label)) {
		if (arguments.size() == 1) {
			range = rangeset->RangesetSpan();
		} else if (arguments.size() == 2) {
			int rangeIndex;
			if (std::error_code ec = readArgument(arguments[1], &rangeIndex)) {
				return ec;
			}

			range = rangeset->RangesetFindRangeNo(rangeIndex - 1);
		}
	}

	*result = make_value(std::make_shared<Array>());

	// NOTE(eteran): this test here prevents uninitialized usage, I'm not sure
	// why GCC's code flow analysis can't see this
	if (!range) {
		return MacroErrorCode::Success;
	}

	element = make_value(to_integer(range->start));
	if (!ArrayInsert(result, "start", &element)) {
		return MacroErrorCode::InsertFailed;
	}

	element = make_value(to_integer(range->end));
	if (!ArrayInsert(result, "end", &element)) {
		return MacroErrorCode::InsertFailed;
	}

QT_WARNING_POP

	return MacroErrorCode::Success;
}

/*
** Built-in macro subroutine for checking a position against a range. If only
** one parameter is supplied, the current cursor position is used. Returns
** false (zero) if not in a range, range index (1-based) if in a range;
** fails if parameters were bad.
*/
static std::error_code rangesetIncludesPosMS(DocumentWidget *document, Arguments arguments, DataValue *result) {
	TextBuffer *buffer = document->buffer();
	const std::shared_ptr<RangesetTable> &rangesetTable = document->rangesetTable_;
	int label = 0;

	if (arguments.size() < 1 || arguments.size() > 2) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	std::error_code ec;
	if ((ec = readArgument(arguments[0], &label)) || !RangesetTable::LabelOK(label)) {
		return MacroErrorCode::Param1InvalidRangesetLabel;
	}

	if(!rangesetTable) {
		return MacroErrorCode::RangesetDoesNotExist;
	}

	Rangeset *rangeset = rangesetTable->RangesetFetch(label);
	if(!rangeset) {
		return MacroErrorCode::RangesetDoesNotExist;
	}

	int64_t pos = 0;
	if (arguments.size() == 1) {
		TextArea *area = MainWindow::fromDocument(document)->lastFocus();
		pos = to_integer(area->TextGetCursorPos());
	} else if (arguments.size() == 2) {
		if (std::error_code ec = readArgument(arguments[1], &pos)) {
			return ec;
		}
	}

	int64_t maxpos = buffer->length();
	int rangeIndex;
	if (pos < 0 || pos > maxpos) {
		rangeIndex = 0;
	} else {
		rangeIndex = rangeset->RangesetFindRangeOfPos(TextCursor(pos), /*incl_end=*/false) + 1;
	}

	// set up result
	*result = make_value(rangeIndex);
	return MacroErrorCode::Success;
}

/*
** Set the color of a range set's ranges. it is ignored if the color cannot be
** found/applied. If no color is applied, any current color is removed. Returns
** true if the rangeset is valid.
*/
static std::error_code rangesetSetColorMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	TextBuffer *buffer = document->buffer();
	const std::shared_ptr<RangesetTable> &rangesetTable = document->rangesetTable_;
	int label = 0;

	if (arguments.size() != 2) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	std::error_code ec;
	if ((ec = readArgument(arguments[0], &label)) || !RangesetTable::LabelOK(label)) {
		return MacroErrorCode::Param1InvalidRangesetLabel;
	}

	if(!rangesetTable) {
		return MacroErrorCode::RangesetDoesNotExist;
	}

	Rangeset *rangeset = rangesetTable->RangesetFetch(label);
	if(!rangeset) {
		return MacroErrorCode::RangesetDoesNotExist;
	}

	QString color_name;
	if (std::error_code ec = readArgument(arguments[1], &color_name)) {
		return MacroErrorCode::Param2NotAString;
	}

	rangeset->setColor(buffer, color_name);

	// set up result
	*result = make_value();
	return MacroErrorCode::Success;
}

/*
** Set the name of a range set's ranges. Returns
** true if the rangeset is valid.
*/
static std::error_code rangesetSetNameMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	const std::shared_ptr<RangesetTable> &rangesetTable = document->rangesetTable_;
	int label = 0;

	if (arguments.size() != 2) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	std::error_code ec;
	if ((ec = readArgument(arguments[0], &label)) || !RangesetTable::LabelOK(label)) {
		return MacroErrorCode::Param1InvalidRangesetLabel;
	}

	if(!rangesetTable) {
		return MacroErrorCode::RangesetDoesNotExist;
	}

	Rangeset *rangeset = rangesetTable->RangesetFetch(label);
	if(!rangeset) {
		return MacroErrorCode::RangesetDoesNotExist;
	}

	QString name;
	if (std::error_code ec = readArgument(arguments[1], &name)) {
		return MacroErrorCode::Param2NotAString;
	}

	rangeset->setName(name);

	// set up result
	*result = make_value();
	return MacroErrorCode::Success;
}

/*
** Change a range's modification response. Returns true if the rangeset is
** valid and the response type name is valid.
*/
static std::error_code rangesetSetModeMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	const std::shared_ptr<RangesetTable> &rangesetTable = document->rangesetTable_;
	int label = 0;

	if (arguments.size() < 1 || arguments.size() > 2) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	std::error_code ec;
	if ((ec = readArgument(arguments[0], &label)) || !RangesetTable::LabelOK(label)) {
		return MacroErrorCode::Param1InvalidRangesetLabel;
	}

	if(!rangesetTable) {
		return MacroErrorCode::RangesetDoesNotExist;
	}

	Rangeset *rangeset = rangesetTable->RangesetFetch(label);
	if(!rangeset) {
		return MacroErrorCode::RangesetDoesNotExist;
	}

	QString update_fn_name;
	if (arguments.size() == 2) {
		if (std::error_code ec = readArgument(arguments[1], &update_fn_name)) {
			return MacroErrorCode::Param2NotAString;
		}
	}

	bool ok = rangeset->setMode(update_fn_name);
	if (!ok) {
		return MacroErrorCode::InvalidMode;
	}

	// set up result
	*result = make_value();
	return MacroErrorCode::Success;
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
static std::error_code fillStyleResult(DataValue *result, DocumentWidget *document, const QString &styleName, bool includeName, size_t patCode, TextCursor bufferPos) {
	DataValue DV;

	*result = make_value(std::make_shared<Array>());

	if (includeName) {

		// insert style name
		DV = make_value(styleName);

		if (!ArrayInsert(result, "style", &DV)) {
			return MacroErrorCode::InsertFailed;
		}
	}

	// insert color name
	DV = make_value(Highlight::FgColorOfNamedStyle(styleName));
	if (!ArrayInsert(result, "color", &DV)) {
		return MacroErrorCode::InsertFailed;
	}

	/* Prepare array element for color value
	   (only possible if we pass through the dynamic highlight pattern tables
	   in other words, only if we have a pattern code) */
	if (patCode) {
		QColor color = document->highlightColorValueOfCode(patCode);
		DV = make_value(color.name());

		if (!ArrayInsert(result, "rgb", &DV)) {
			return MacroErrorCode::InsertFailed;
		}
	}

	// Prepare array element for background color name
	DV = make_value(Highlight::BgColorOfNamedStyle(styleName));
	if (!ArrayInsert(result, "background", &DV)) {
		return MacroErrorCode::InsertFailed;
	}

	/* Prepare array element for background color value
	   (only possible if we pass through the dynamic highlight pattern tables
	   in other words, only if we have a pattern code) */
	if (patCode) {
		QColor color = document->highlightBGColorOfCode(patCode);
		DV = make_value(color.name());

		if (!ArrayInsert(result, "back_rgb", &DV)) {
			return MacroErrorCode::InsertFailed;
		}
	}

	// Put boldness value in array
	DV = make_value(Highlight::FontOfNamedStyleIsBold(styleName));
	if (!ArrayInsert(result, "bold", &DV)) {
		return MacroErrorCode::InsertFailed;
	}

	// Put italicity value in array
	DV = make_value(Highlight::FontOfNamedStyleIsItalic(styleName));
	if (!ArrayInsert(result, "italic", &DV)) {
		return MacroErrorCode::InsertFailed;
	}

	if (bufferPos >= 0) {
		// insert extent
		DV = make_value(document->styleLengthOfCodeFromPos(bufferPos));
		if (!ArrayInsert(result, "extent", &DV)) {
			return MacroErrorCode::InsertFailed;
		}
	}
	return MacroErrorCode::Success;
}

/*
** Returns an array containing information about the style of name $1
**      ["color"]       Foreground color name of style
**      ["background"]  Background color name of style if specified
**      ["bold"]        '1' if style is bold, '0' otherwise
**      ["italic"]      '1' if style is italic, '0' otherwise
**
*/
static std::error_code getStyleByNameMS(DocumentWidget *document, Arguments arguments, DataValue *result) {
	QString styleName;

	// Validate number of arguments
	if(std::error_code ec = readArguments(arguments, 0, &styleName)) {
		return MacroErrorCode::Param1NotAString;
	}

	*result = make_value(ArrayPtr());

	if (!Highlight::NamedStyleExists(styleName)) {
		// if the given name is invalid we just return an empty array.
		return MacroErrorCode::Success;
	}

	return fillStyleResult(
				result,
				document,
				styleName,
				false,
				0,
				TextCursor(-1));
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
static std::error_code getStyleAtPosMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	int64_t bufferPos;
	TextBuffer *buf = document->buffer();

	// Validate number of arguments
	if(std::error_code ec = readArguments(arguments, 0, &bufferPos)) {
		return ec;
	}

	*result = make_value(ArrayPtr());

	//  Verify sane buffer position
	if ((bufferPos < 0) || (bufferPos >= buf->length())) {
		/*  If the position is not legal, we cannot guess anything about
			the style, so we return an empty array. */
		return MacroErrorCode::Success;
	}

	// Determine pattern code
	size_t patCode = document->highlightCodeOfPos(TextCursor(bufferPos));
	if (patCode == 0) {
		// if there is no pattern we just return an empty array.
		return MacroErrorCode::Success;
	}

	return fillStyleResult(
		result,
		document,
		document->highlightStyleOfCode(patCode),
		true,
		patCode,
		TextCursor(bufferPos));
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
std::error_code fillPatternResult(DataValue *result, DocumentWidget *document, const QString &patternName, bool includeName, const QString &styleName, TextCursor bufferPos) {

	DataValue DV;

	*result = make_value(std::make_shared<Array>());

	// the following array entries will be strings

	if (includeName) {
		// insert pattern name
		DV = make_value(patternName);
		if (!ArrayInsert(result, "pattern", &DV)) {
			return MacroErrorCode::InsertFailed;
		}
	}

	// insert style name
	DV = make_value(styleName);
	if (!ArrayInsert(result, "style", &DV)) {
		return MacroErrorCode::InsertFailed;
	}

	if (bufferPos >= 0) {
		// insert extent
		DV = make_value(document->highlightLengthOfCodeFromPos(bufferPos));
		if (!ArrayInsert(result, "extent", &DV)) {
			return MacroErrorCode::InsertFailed;
		}
	}

	return MacroErrorCode::Success;
}

/*
** Returns an array containing information about a highlighting pattern. The
** single parameter contains the pattern name for which this information is
** requested.
** The returned array looks like this:
**      ["style"]       Name of style
*/
static std::error_code getPatternByNameMS(DocumentWidget *document, Arguments arguments, DataValue *result) {

	QString patternName;

	*result = make_value(ArrayPtr());

	// Validate number of arguments
	if(std::error_code ec = readArguments(arguments, 0, &patternName)) {
		return MacroErrorCode::Param1NotAString;
	}

	HighlightPattern *pattern = document->findPatternOfWindow(patternName);
	if(!pattern) {
		// The pattern's name is unknown.
		return MacroErrorCode::Success;
	}

	return fillPatternResult(
				result,
				document,
				patternName,
				false,
				pattern->style,
				TextCursor(-1));
}

/*
** Returns an array containing information about the highlighting pattern
** applied at a given position, passed as the only parameter.
** The returned array looks like this:
**      ["pattern"]     Name of pattern
**      ["style"]       Name of style
**      ["extent"]      Distance from position over which this pattern applies
*/
static std::error_code getPatternAtPosMS(DocumentWidget *document, Arguments arguments, DataValue *result) {
	int64_t bufferPos;
	TextBuffer *buffer = document->buffer();

	*result = make_value(ArrayPtr());

	// Validate number of arguments
	if(std::error_code ec = readArguments(arguments, 0, &bufferPos)) {
		return ec;
	}

	/* The most straightforward case: Get a pattern, style and extent
	   for a buffer position. */

	/*  Verify sane buffer position
	 *  You would expect that buffer->length would be among the sane
	 *  positions, but we have n characters and n+1 buffer positions. */
	if ((bufferPos < 0) || (bufferPos >= buffer->length())) {
		/*  If the position is not legal, we cannot guess anything about
			the highlighting pattern, so we return an empty array. */
		return MacroErrorCode::Success;
	}

	// Determine the highlighting pattern used
	size_t patCode = document->highlightCodeOfPos(TextCursor(bufferPos));
	if (patCode == 0) {
		// if there is no highlighting pattern we just return an empty array.
		return MacroErrorCode::Success;
	}

	return fillPatternResult(
		result,
		document,
		document->highlightNameOfCode(patCode),
		true,
		document->highlightStyleOfCode(patCode),
		TextCursor(bufferPos));
}

/*
** Get an integer value from a tagged DataValue structure.
*/
static std::error_code readArgument(const DataValue &dv, int64_t *result) {

	if(is_integer(dv)) {
		*result = to_integer(dv);
		return MacroErrorCode::Success;
	}

	if(is_string(dv)) {
		auto s = QString::fromStdString(to_string(dv));
		bool ok;
		int64_t val = s.toLongLong(&ok);
		if(!ok) {
		   return MacroErrorCode::NotAnInteger;
		}

		*result = val;
		return MacroErrorCode::Success;
	}

	return MacroErrorCode::UnknownObject;

}

static std::error_code readArgument(const DataValue &dv, int *result) {

	if(is_integer(dv)) {
		*result = to_integer(dv);
		return MacroErrorCode::Success;
	}

	if(is_string(dv)) {
		auto s = QString::fromStdString(to_string(dv));
		bool ok;
		int val = s.toInt(&ok);
		if(!ok) {
		   return MacroErrorCode::NotAnInteger;
		}

		*result = val;
		return MacroErrorCode::Success;
	}

	return MacroErrorCode::UnknownObject;

}

/*
** Get an string value from a tagged DataValue structure.
*/
static std::error_code readArgument(const DataValue &dv, std::string *result) {

	if(is_string(dv)) {
		*result = to_string(dv);
		return MacroErrorCode::Success;
	}

	if(is_integer(dv)) {
		*result = std::to_string(to_integer(dv));
		return MacroErrorCode::Success;
	}

	return MacroErrorCode::UnknownObject;
}

static std::error_code readArgument(const DataValue &dv, QString *result) {

	if(is_string(dv)) {
		*result = QString::fromStdString(to_string(dv));
		return MacroErrorCode::Success;
	}

	if(is_integer(dv)) {
		*result = QString::number(to_integer(dv));
		return MacroErrorCode::Success;
	}

	return MacroErrorCode::UnknownObject;
}


template <class T, class ...Ts>
std::error_code readArguments(Arguments arguments, int index, T arg, Ts...args) {

	static_assert(std::is_pointer<T>::value, "Argument is not a pointer");

	if(static_cast<size_t>(arguments.size() - index) < (sizeof...(args)) + 1) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	if(std::error_code ec = readArgument(arguments[index], arg)) {
		return ec;
	}

	return readArguments(arguments, index + 1, args...);
}

template <class T>
std::error_code readArguments(Arguments arguments, int index, T arg) {

	static_assert(std::is_pointer<T>::value, "Argument is not a pointer");

	if(static_cast<size_t>(arguments.size() - index) < 1) {
		return MacroErrorCode::WrongNumberOfArguments;
	}

	return readArgument(arguments[index], arg);
}
