
#include "CommandRecorder.h"
#include "TextEditEvent.h"
#include "DocumentWidget.h"
#include <QMutex>
#include <QtDebug>

namespace {

#if 0
// Arrays for translating escape characters in escapeStringChars
const char ReplaceChars[] = "\\\"ntbrfav";
const char EscapeChars[] = "\\\"\n\t\b\r\f\a\v";
#endif

// List of actions not useful when learning a macro sequence (also see below)
const char *IgnoredActions[] = {
    "focusIn",
    "focusOut"
};

/* List of actions intended to be attached to mouse buttons, which the user
   must be warned can't be recorded in a learn/replay sequence */
const char *MouseActions[] = {
    "grab_focus",
    "extend_adjust",
    "extend_start",
    "extend_end",
    "secondary_or_drag_adjust",
    "secondary_adjust",
    "secondary_or_drag_start",
    "secondary_start",
    "move_destination",
    "move_to",
    "move_to_or_end_drag",
    "copy_to",
    "copy_to_or_end_drag",
    "exchange",
    "process_bdrag",
    "mouse_pan"
};

/* List of actions to not record because they
   generate further actions, more suitable for recording */
const char *RedundantActions[] = {
    "open_dialog",
    "save_as_dialog",
    "revert_to_saved_dialog",
    "include_file_dialog",
    "load_macro_file_dialog",
    "load_tags_file_dialog",
    "find_dialog",
    "replace_dialog",
    "goto_line_number_dialog",
    "mark_dialog",
    "goto_mark_dialog",
    "control_code_dialog",
    "filter_selection_dialog",
    "execute_command_dialog",
    "repeat_dialog",
    "start_incremental_find"
};


CommandRecorder *instance = nullptr;
QMutex instanceMutex;

}

/**
 * @brief CommandRecorder::CommandRecorder
 * @param parent
 */
CommandRecorder::CommandRecorder(QObject *parent) : QObject(parent), isRecording_(false) {

}

/**
 * @brief CommandRecorder::getInstance
 * @return global unique instance
 */
CommandRecorder *CommandRecorder::getInstance() {

    instanceMutex.lock();
    if(!instance) {
        instance = new CommandRecorder;
    }
    instanceMutex.unlock();

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
    }

    return false;
}

void CommandRecorder::lastActionHook(QObject *obj, const TextEditEvent *ev) {
    qDebug("Text Event! : %s", ev->toString().toLatin1().data());

    int i;

    // Find the curr to which this action belongs
    QList<DocumentWidget *> documents = DocumentWidget::allDocuments();
    auto curr = documents.begin();
    for (; curr != documents.end(); ++curr) {

        DocumentWidget *const document = *curr;
        QList<TextArea *> textPanes = document->textPanes();

        for (i = 0; i < textPanes.size(); i++) {
            if (textPanes[i] == obj) {
                break;
            }
        }

        if (i < textPanes.size()) {
            break;
        }
    }

    if(curr == documents.end()) {
        return;
    }

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



#if 0
    WindowInfo *window;
    int i;
    char *actionString;

    /* Select only actions in text panes in the window for which this
       action hook is recording macros (from clientData). */
    for (window=WindowList; window!=NULL; window=window->next) {
    if (window->textArea == w)
        break;
    for (i=0; i<window->nPanes; i++) {
            if (window->textPanes[i] == w)
                break;
    }
    if (i < window->nPanes)
        break;
    }
    if (window == NULL || window != (WindowInfo *)clientData)
        return;

    /* beep on un-recordable operations which require a mouse position, to
       remind the user that the action was not recorded */
    if (isMouseAction(actionName)) {
        XBell(XtDisplay(w), 0);
        return;
    }

    /* Record the action and its parameters */
    actionString = actionToString(w, actionName, event, params, *numParams);
    if (actionString != NULL) {
    BufInsert(MacroRecordBuf, MacroRecordBuf->length, actionString);
    }
#endif
}

bool CommandRecorder::isMouseAction(const TextEditEvent *ev) const {

    for(const char *action : MouseActions) {
        if (!strcmp(action, ev->actionString().toLatin1().data())) {
            return true;
        }
    }

    return false;
}

bool CommandRecorder::isRedundantAction(const TextEditEvent *ev) const {

    for(const char *action : RedundantActions) {
        if (!strcmp(action, ev->actionString().toLatin1().data())) {
            return true;
        }
    }

    return false;
}

bool CommandRecorder::isIgnoredAction(const TextEditEvent *ev) const {

    for(const char *action : IgnoredActions) {
        if (!strcmp(action, ev->actionString().toLatin1().data())) {
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

/**
 * @brief CommandRecorder::startRecording
 */
void CommandRecorder::startRecording() {
    setRecording(true);
}

/**
 * @brief CommandRecorder::stopRecording
 */
void CommandRecorder::stopRecording() {
    setRecording(false);
}

/**
 * stops recording user actions, but does NOT save the buffer
 * @brief CommandRecorder::cancelRecording
 */
void CommandRecorder::cancelRecording() {
    isRecording_ = false;
    macroRecordBuffer.clear();
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
