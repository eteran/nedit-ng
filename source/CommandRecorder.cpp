
#include "CommandRecorder.h"
#include "TextEditEvent.h"
#include "WindowMenuEvent.h"
#include "DocumentWidget.h"
#include <QMutex>
#include <QtDebug>

namespace {

#if 0
// Arrays for translating escape characters in escapeStringChars
const char ReplaceChars[] = "\\\"ntbrfav";
#endif

// List of actions not useful when learning a macro sequence (also see below)
QLatin1String IgnoredActions[] = {
    QLatin1String("focusIn"),
    QLatin1String("focusOut")
};

/* List of actions intended to be attached to mouse buttons, which the user
   must be warned can't be recorded in a learn/replay sequence */
QLatin1String MouseActions[] = {
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
    QLatin1String("mouse_pan")
};

/* List of actions to not record because they
   generate further actions, more suitable for recording */
QLatin1String RedundantActions[] = {
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
    QLatin1String("start_incremental_find")
};

CommandRecorder *instance = nullptr;
QMutex instanceMutex;

}

/**
 * @brief CommandRecorder::quoteString
 * @param s
 * @return
 */
QString CommandRecorder::quoteString(const QString &s) {
    return tr("\"%1\"").arg(s);
}

/**
 * @brief CommandRecorder::escapeString
 * @param s
 * @return
 */
QString CommandRecorder::escapeString(const QString &s) {

    static const QString EscapeChars = QLatin1String("\\\"\n\t\b\r\f\a\v");

    QString r;
    r.reserve(s.size());
    for(QChar ch : s) {
        if(EscapeChars.contains(ch)) {
            r.append(QLatin1Char('\\'));
        }
        r.append(ch);
    }
    return r;
}

/**
 * @brief CommandRecorder::CommandRecorder
 * @param parent
 */
CommandRecorder::CommandRecorder(QObject *parent) : QObject(parent), macroRecordWindowEx(nullptr), isRecording_(false) {

}

/**
 * @brief CommandRecorder::getInstance
 * @return global unique instance
 */
CommandRecorder *CommandRecorder::getInstance() {

    QMutexLocker locker(&instanceMutex);
    if(!instance) {
        instance = new CommandRecorder;
    }

    return instance;
}

/**
 * @brief CommandRecorder::eventFilter
 * @param obj
 * @param event
 * @return
 */
bool CommandRecorder::eventFilter(QObject *obj, QEvent *event) {

    if(event->type() == TextEditEvent::eventType) {
        lastActionHook(obj, static_cast<TextEditEvent *>(event));
    } else if(event->type() == WindowMenuEvent::eventType) {
        lastActionHook(obj, static_cast<WindowMenuEvent *>(event));
    }

    return false;
}

void CommandRecorder::lastActionHook(QObject *obj, const WindowMenuEvent *ev) {
    Q_UNUSED(obj);
    Q_UNUSED(ev);

    /* The last action is recorded for the benefit of repeating the last
       action.  Don't record repeat_macro and wipe out the real action */
    if(ev->actionString() == QLatin1String("repeat_macro")) {
        return;
    }

    // Record the action and its parameters
    QString actionString = actionToString(ev);
    if (!actionString.isNull()) {
        lastCommand = actionString;

        if(isRecording_) {
            macroRecordBuffer.append(actionString);
            macroRecordBuffer.append(QLatin1Char('\n'));
        }
    }
}

void CommandRecorder::lastActionHook(QObject *obj, const TextEditEvent *ev) {

    Q_UNUSED(obj);

    /* The last action is recorded for the benefit of repeating the last
       action.  Don't record repeat_macro and wipe out the real action */
    if(ev->actionString() == QLatin1String("repeat_macro")) {
        return;
    }

    // Record the action and its parameters
    QString actionString = actionToString(ev);
    if (!actionString.isNull()) {
        lastCommand = actionString;

        if(isRecording_) {
            /* beep on un-recordable operations which require a mouse position, to
               remind the user that the action was not recorded */
            if (isMouseAction(ev)) {
                QApplication::beep();
                return;
            }

            macroRecordBuffer.append(actionString);
            macroRecordBuffer.append(QLatin1Char('\n'));
        }
    }
}

/**
 * @brief CommandRecorder::isMouseAction
 * @param ev
 * @return
 */
bool CommandRecorder::isMouseAction(const TextEditEvent *ev) const {

    for(const QLatin1String &action : MouseActions) {
        if (action == ev->actionString()) {
            return true;
        }
    }

    return false;
}

/**
 * @brief CommandRecorder::isRedundantAction
 * @param ev
 * @return
 */
bool CommandRecorder::isRedundantAction(const TextEditEvent *ev) const {

    for(const QLatin1String &action : RedundantActions) {
        if (action == ev->actionString()) {
            return true;
        }
    }

    return false;
}

/**
 * @brief CommandRecorder::isIgnoredAction
 * @param ev
 * @return
 */
bool CommandRecorder::isIgnoredAction(const TextEditEvent *ev) const {

    for(const QLatin1String &action : IgnoredActions) {
        if (action == ev->actionString()) {
            return true;
        }
    }

    return false;
}

/*
** Create a macro string to represent an invocation of an action routine.
** Returns nullptr for non-operational or un-recordable actions.
*/
QString CommandRecorder::actionToString(const TextEditEvent *ev) {

    if (isIgnoredAction(ev) || isRedundantAction(ev) || isMouseAction(ev)) {
        return QString();
    }

    return ev->toString();
}

/*
** Create a macro string to represent an invocation of an action routine.
** Returns nullptr for non-operational or un-recordable actions.
*/
QString CommandRecorder::actionToString(const WindowMenuEvent *ev) {
    return ev->toString();
}


/**
 * @brief CommandRecorder::startRecording
 */
void CommandRecorder::startRecording(DocumentWidget *document) {
    setRecording(true);
    macroRecordWindowEx = document;
}

/**
 * @brief CommandRecorder::stopRecording
 */
void CommandRecorder::stopRecording() {
    setRecording(false);
    macroRecordWindowEx = nullptr;
}

/**
 * stops recording user actions, but does NOT save the buffer
 * @brief CommandRecorder::cancelRecording
 */
void CommandRecorder::cancelRecording() {
    isRecording_ = false;
    macroRecordBuffer.clear();
    macroRecordWindowEx = nullptr;
}

/**
 * @brief CommandRecorder::isRecording
 * @return
 */
bool CommandRecorder::isRecording() const {
    return isRecording_;
}

/**
 * @brief CommandRecorder::setRecording
 * @param enabled
 */
void CommandRecorder::setRecording(bool enabled) {

    if(isRecording_ == enabled) {
        // no change in state, do nothing
    }

    if(!enabled) {
        // we've been asked to stop recording
        // Store the finished action for the replay menu item
        replayMacro = macroRecordBuffer;
    } else {
        // start recording
        macroRecordBuffer.clear();
    }

    isRecording_ = enabled;
}
