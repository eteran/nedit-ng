
#include "CommandRecorder.h"
#include "DocumentWidget.h"
#include "TextEditEvent.h"
#include "WindowMenuEvent.h"

#include <QtDebug>

namespace {

/* List of actions intended to be attached to mouse buttons, which the user
   must be warned can't be recorded in a learn/replay sequence */
const QLatin1String MouseActions[] = {
	QLatin1String("grab_focus"),
	QLatin1String("extend_adjust"),
	QLatin1String("extend_start"),
	QLatin1String("extend_end"),
	QLatin1String("secondary_or_drag_adjust"),
	QLatin1String("secondary_adjust"),
	QLatin1String("secondary_or_drag_start"),
	QLatin1String("secondary_start"),
	QLatin1String("move_destination"),
	QLatin1String("move_to"),
	QLatin1String("move_to_or_end_drag"),
	QLatin1String("copy_to"),
	QLatin1String("copy_to_or_end_drag"),
	QLatin1String("exchange"),
	QLatin1String("process_bdrag"),
	QLatin1String("mouse_pan")};

/* List of actions to not record because they
   generate further actions, more suitable for recording */
const QLatin1String RedundantActions[] = {
	QLatin1String("open_dialog"),
	QLatin1String("save_as_dialog"),
	QLatin1String("revert_to_saved_dialog"),
	QLatin1String("include_file_dialog"),
	QLatin1String("load_macro_file_dialog"),
	QLatin1String("load_tags_file_dialog"),
	QLatin1String("find_dialog"),
	QLatin1String("replace_dialog"),
	QLatin1String("goto_line_number_dialog"),
	QLatin1String("mark_dialog"),
	QLatin1String("goto_mark_dialog"),
	QLatin1String("control_code_dialog"),
	QLatin1String("filter_selection_dialog"),
	QLatin1String("execute_command_dialog"),
	QLatin1String("repeat_dialog"),
	QLatin1String("start_incremental_find")};

/**
 * @brief Checks if the event is a mouse action.
 *
 * @param ev The event to check
 * @return `true` if the event is a mouse action, false otherwise
 */
template <class Event>
bool IsMouseAction(const Event *ev) {

	return std::any_of(std::begin(MouseActions), std::end(MouseActions), [ev](const QLatin1String &action) {
		return action == ev->actionString();
	});
}

/**
 * @brief Checks if the action is redundant.
 *
 * @param ev The event to check
 * @return `true` if the action is redundant, false otherwise
 */
template <class Event>
bool IsRedundantAction(const Event *ev) {

	return std::any_of(std::begin(RedundantActions), std::end(RedundantActions), [ev](const QLatin1String &action) {
		return action == ev->actionString();
	});
}

/**
 * @brief Create a macro string to represent an invocation of an action routine.
 *
 * @param ev The event to convert to a string.
 * @return QString representing the action, or QString() if the action is not recordable.
 */
template <class Event>
QString ActionToString(const Event *ev) {

	if (IsRedundantAction(ev) || IsMouseAction(ev)) {
		return QString();
	}

	return ev->toString();
}

}

/**
 * @brief Quote a string for use in a command.
 *
 * @param s The string to quote.
 * @return The string quoted with double quotes.
 *
 * @note If the string contains double quotes, they will NOT be escaped.
 *       This is intended for use in strings which have already been
 *       escaped using escapeString().
 */
QString CommandRecorder::quoteString(const QString &s) {
	return QStringLiteral("\"%1\"").arg(s);
}

/**
 * @brief Escape a string for use in a command.
 *
 * @param s The string to escape.
 * @return The string with special characters escaped.
 */
QString CommandRecorder::escapeString(const QString &s) {

	static const auto EscapeChars = QStringLiteral("\\\"\n\t\b\r\f\a\v");

	QString r;
	r.reserve(s.size());
	for (const QChar ch : s) {
		if (EscapeChars.contains(ch)) {
			r.append(QLatin1Char('\\'));
		}
		r.append(ch);
	}
	return r;
}

/**
 * @brief CommandRecorder constructor.
 *
 * @param parent The parent object.
 */
CommandRecorder::CommandRecorder(QObject *parent)
	: QObject(parent) {
}

/**
 * @brief Get the global unique instance of CommandRecorder.
 *
 * @return Global unique instance.
 */
CommandRecorder *CommandRecorder::instance() {
	static CommandRecorder instance;
	return &instance;
}

/**
 * @brief Event filter for CommandRecorder.
 *
 * @param obj The object that received the event.
 * @param event The event that was received.
 * @return `true` if the event was handled, `false` otherwise.
 */
bool CommandRecorder::eventFilter(QObject *obj, QEvent *event) {

	Q_UNUSED(obj)

	if (event->type() == TextEditEvent::eventType) {
		lastActionHook(static_cast<TextEditEvent *>(event));
	} else if (event->type() == WindowMenuEvent::eventType) {
		lastActionHook(static_cast<WindowMenuEvent *>(event));
	}

	return false;
}

/**
 * @brief Hook for the last action performed.
 *
 * @param ev The event containing the action performed.
 */
void CommandRecorder::lastActionHook(const WindowMenuEvent *ev) {

	/* The last action is recorded for the benefit of repeating the last
	   action.  Don't record repeat_macro and wipe out the real action */
	if (ev->actionString() == QStringLiteral("repeat_macro")) {
		return;
	}

	// Record the action and its parameters
	const QString actionString = ActionToString(ev);
	if (!actionString.isNull()) {
		lastCommand_ = actionString;

		if (isRecording_) {
			/* beep on un-recordable operations which require a mouse position, to
			   remind the user that the action was not recorded */
			if (IsMouseAction(ev)) {
				QApplication::beep();
				return;
			}

			macroRecordBuffer_.append(actionString);
			macroRecordBuffer_.append(QLatin1Char('\n'));
		}
	}
}

/**
 * @brief Hook for the last action performed in a text edit event.
 *
 * @param ev The event containing the action performed.
 */
void CommandRecorder::lastActionHook(const TextEditEvent *ev) {

	/* The last action is recorded for the benefit of repeating the last
	   action.  Don't record repeat_macro and wipe out the real action */
	if (ev->actionString() == QStringLiteral("repeat_macro")) {
		return;
	}

	// Record the action and its parameters
	const QString actionString = ActionToString(ev);
	if (!actionString.isNull()) {
		lastCommand_ = actionString;

		if (isRecording_) {
			/* beep on un-recordable operations which require a mouse position, to
			   remind the user that the action was not recorded */
			if (IsMouseAction(ev)) {
				QApplication::beep();
				return;
			}

			macroRecordBuffer_.append(actionString);
			macroRecordBuffer_.append(QLatin1Char('\n'));
		}
	}
}

/**
 * @brief Starts recording user actions.
 *
 * @param document The document widget where the recording will take place.
 */
void CommandRecorder::startRecording(DocumentWidget *document) {
	setRecording(true);
	macroRecordDocument_ = document;
}

/**
 * @brief Stops recording user actions and clears the macro record buffer.
 */
void CommandRecorder::stopRecording() {
	setRecording(false);
	macroRecordDocument_ = nullptr;
}

/**
 * @brief Returns the document where the macro is being recorded.
 *
 * @return The document widget where the macro is being recorded.
 */
QPointer<DocumentWidget> CommandRecorder::macroRecordDocument() const {
	return macroRecordDocument_;
}

/**
 * @brief Cancels the current recording session.
 * But does NOT save the buffer
 */
void CommandRecorder::cancelRecording() {
	isRecording_ = false;
	macroRecordBuffer_.clear();
	macroRecordDocument_ = nullptr;
}

/**
 * @brief Checks if the recorder is currently recording user actions.
 *
 * @return `true` if recording is active, `false` otherwise.
 */
bool CommandRecorder::isRecording() const {
	return isRecording_;
}

/**
 * @brief Sets the recording state of the CommandRecorder.
 *
 * @param enabled If `true`, starts recording; if false, stops recording.
 */
void CommandRecorder::setRecording(bool enabled) {

	if (isRecording_ == enabled) {
		// no change in state, do nothing
		return;
	}

	if (!enabled) {
		// we've been asked to stop recording
		// Store the finished action for the replay menu item
		replayMacro_ = macroRecordBuffer_;
	} else {
		// start recording
		macroRecordBuffer_.clear();
	}

	isRecording_ = enabled;
}

/**
 * @brief Returns the last command executed.
 *
 * @return The last command.
 */
QString CommandRecorder::lastCommand() const {
	return lastCommand_;
}

/**
 * @brief Returns the macro string that can be replayed.
 *
 * @return The macro string to replay.
 */
QString CommandRecorder::replayMacro() const {
	return replayMacro_;
}
