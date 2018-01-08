
#include "KeySequenceEdit.h"
#include <QBoxLayout>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QAction>
#include <QStyle>

/*!
    \class KeySequenceEdit
    \brief The KeySequenceEdit widget allows to input a QKeySequence.


    This widget lets the user choose a QKeySequence, which is usually used as
    a shortcut. The recording is initiated when the widget receives the focus
    and ends one second after the user releases the last key_.
*/

namespace {

/**
 * @brief translateModifiers
 * @param state
 * @param text
 * @return
 */
int translateModifiers(Qt::KeyboardModifiers state, const QString &text) {
    int result = 0;
    // The shift modifier only counts when it is not used to type a symbol
    // that is only reachable using the shift key_ anyway
    if ((state & Qt::ShiftModifier) && (text.isEmpty() || !text.at(0).isPrint() || text.at(0).isLetterOrNumber() || text.at(0).isSpace())) {
        result |= Qt::SHIFT;
	}

    if (state & Qt::ControlModifier) {
        result |= Qt::CTRL;
	}
	
    if (state & Qt::MetaModifier) {
        result |= Qt::META;
	}
	
    if (state & Qt::AltModifier) {
        result |= Qt::ALT;
	}

    return result;
}

}

/**
 * @brief KeySequenceEdit::resetState
 */
void KeySequenceEdit::resetState() {
    if (releaseTimer_) {
        killTimer(releaseTimer_);
        releaseTimer_ = 0;
    }
	
    prevKey_ = -1;
    lineEdit_->setText(keySequence_.toString(QKeySequence::NativeText));
    lineEdit_->setPlaceholderText(KeySequenceEdit::tr("Press shortcut"));

    // hook the clear button...
    if(auto action = lineEdit_->findChild<QAction *>()) {
        connect(action, &QAction::triggered, this, [this]() {
            setKeySequence(QKeySequence());
        }, Qt::QueuedConnection);
    }
}



/**
 * @brief KeySequenceEdit::finishEditing
 */
void KeySequenceEdit::finishEditing() {
    resetState();
    Q_EMIT keySequenceChanged(keySequence_);
    Q_EMIT editingFinished();
}

/**
 * @brief KeySequenceEdit::KeySequenceEdit
 * @param parent
 * @param f
 */
KeySequenceEdit::KeySequenceEdit(QWidget *parent, Qt::WindowFlags f) : QWidget(parent, f), prevKey_(-1), releaseTimer_(0), maximumSequenceLength_(4), modifierRequired_(false) {
	
    lineEdit_ = new QLineEdit(this);	
	lineEdit_->setContextMenuPolicy(Qt::PreventContextMenu);
    lineEdit_->setClearButtonEnabled(true);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(lineEdit_);

    lineEdit_->setFocusProxy(this);
    lineEdit_->installEventFilter(this);
    resetState();

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_MacShowFocusRect, true);
    setAttribute(Qt::WA_InputMethodEnabled, false);
}

/**
 * @brief KeySequenceEdit::KeySequenceEdit
 * @param keySequence
 * @param parent
 * @param f
 */
KeySequenceEdit::KeySequenceEdit(const QKeySequence &keySequence, QWidget *parent, Qt::WindowFlags f) : KeySequenceEdit(parent, f) {
    setKeySequence(keySequence);
}

/**
 * @brief KeySequenceEdit::maximumSequenceLength
 * @return
 */
int KeySequenceEdit::maximumSequenceLength() const {
	return maximumSequenceLength_;
}

/**
 * @brief KeySequenceEdit::setMaximumSequenceLength
 * @param maximum
 */
void KeySequenceEdit::setMaximumSequenceLength(int maximum) {

	if(maximum < 1 || maximum > 4) {
		qWarning("Maximum sequence length must be an integer from 1 to 4");
		return;
	}

	maximumSequenceLength_ = maximum;

    Q_EMIT maximumSequenceLengthChanged(maximum);
}


/**
 * This property contains the currently chosen key_ sequence. The shortcut can
 * be changed by the user or via setter function.
 *
 * @brief KeySequenceEdit::keySequence
 * @return
 */
QKeySequence KeySequenceEdit::keySequence() const {
    return keySequence_;
}

/**
 * @brief KeySequenceEdit::setKeySequence
 * @param keySequence
 */
void KeySequenceEdit::setKeySequence(const QKeySequence &keySequence) {
    resetState();

    if (keySequence_ == keySequence) {
        return;
	}

    keySequence_ = keySequence;

	keys_.clear();

    for (int i = 0; i < keySequence_.count(); ++i) {
        keys_.push_back(keySequence[static_cast<uint>(i)]);
	}

    lineEdit_->setText(keySequence_.toString(QKeySequence::NativeText));

    Q_EMIT keySequenceChanged(keySequence);
}


/**
 * Clears the current key sequence.
 *
 * @brief KeySequenceEdit::clear
 */
void KeySequenceEdit::clear() {
    setKeySequence(QKeySequence());
}

/**
 * @brief KeySequenceEdit::event
 * @param e
 * @return
 */
bool KeySequenceEdit::event(QEvent *e) {
    switch (e->type()) {
    case QEvent::Shortcut:
        return true;
    case QEvent::ShortcutOverride:
        e->accept();
        return true;
    default :
        break;
    }

    return QWidget::event(e);
}

/**
 * @brief KeySequenceEdit::keyPressEvent
 * @param e
 */
void KeySequenceEdit::keyPressEvent(QKeyEvent *e) {
    int nextKey = e->key();

    if (prevKey_ == -1) {
        clear();
        prevKey_ = nextKey;
    }

    lineEdit_->setPlaceholderText(QString());
    if (nextKey == Qt::Key_Control || nextKey == Qt::Key_Shift || nextKey == Qt::Key_Meta || nextKey == Qt::Key_Alt) {
        return;
    }
	
	if(modifierRequired_ && e->modifiers() == Qt::NoModifier) {
		return;
	}

    QString selectedText = lineEdit_->selectedText();
    if (!selectedText.isEmpty() && selectedText == lineEdit_->text()) {
        clear();
        if (nextKey == Qt::Key_Backspace)
            return;
    }

    if (keys_.size() >= maximumSequenceLength_)
        return;

    nextKey |= translateModifiers(e->modifiers(), e->text());

    keys_.push_back(nextKey);

	switch(keys_.size()) {
	case 1:
		keySequence_ = QKeySequence(keys_[0]);
		break;
	case 2:
		keySequence_ = QKeySequence(keys_[0], keys_[1]);
		break;
	case 3:
		keySequence_ = QKeySequence(keys_[0], keys_[1], keys_[2]);
		break;	
	case 4:
		keySequence_ = QKeySequence(keys_[0], keys_[1], keys_[2], keys_[3]);
		break;
	default:
		keySequence_ = QKeySequence();
		break;	
	}

    QString text = keySequence_.toString(QKeySequence::NativeText);
    if (keys_.size() < maximumSequenceLength_) {
        //: This text is an "unfinished" shortcut, expands like "Ctrl+A, ..."
        text = tr("%1, ...").arg(text);
    }
	
    lineEdit_->setText(text);
    e->accept();
}

/**
 * @brief KeySequenceEdit::keyReleaseEvent
 * @param e
 */
void KeySequenceEdit::keyReleaseEvent(QKeyEvent *e) {
    if (prevKey_ == e->key()) {
        if (keys_.size() < maximumSequenceLength_) {
            releaseTimer_ = startTimer(1000);
        } else {
            finishEditing();
		}
    }
    e->accept();
}

/**
 * @brief KeySequenceEdit::timerEvent
 * @param e
 */
void KeySequenceEdit::timerEvent(QTimerEvent *e) {
    if (e->timerId() == releaseTimer_) {
        finishEditing();
        return;
    }

    QWidget::timerEvent(e);
}

/**
 * @brief KeySequenceEdit::modifierRequired
 * @return
 */
bool KeySequenceEdit::modifierRequired() const {
	return modifierRequired_;
}

/**
 * @brief KeySequenceEdit::setModifierRequired
 * @param required
 */
void KeySequenceEdit::setModifierRequired(bool required) {
	modifierRequired_ = required;
    Q_EMIT modifierRequiredChanged(required);
}


