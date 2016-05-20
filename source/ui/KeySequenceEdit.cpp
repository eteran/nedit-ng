
#include "KeySequenceEdit.h"
#include <QBoxLayout>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QStyle>


/*!
    \class KeySequenceEdit
    \brief The KeySequenceEdit widget allows to input a QKeySequence.


    This widget lets the user choose a QKeySequence, which is usually used as
    a shortcut. The recording is initiated when the widget receives the focus
    and ends one second after the user releases the last key_.
*/

namespace {

//------------------------------------------------------------------------------
// Name: translateModifiers
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Name: resetState
//------------------------------------------------------------------------------
void KeySequenceEdit::resetState() {
    if (releaseTimer_) {
        killTimer(releaseTimer_);
        releaseTimer_ = 0;
    }
	
    prevKey_ = -1;
    lineEdit_->setText(keySequence_.toString(QKeySequence::NativeText));
    lineEdit_->setPlaceholderText(KeySequenceEdit::tr("Press shortcut"));
}



//------------------------------------------------------------------------------
// Name: finishEditing
//------------------------------------------------------------------------------
void KeySequenceEdit::finishEditing() {
    resetState();
    Q_EMIT keySequenceChanged(keySequence_);
    Q_EMIT editingFinished();
}

//------------------------------------------------------------------------------
// Name: KeySequenceEdit
// Desc: Constructor
//------------------------------------------------------------------------------
KeySequenceEdit::KeySequenceEdit(QWidget *parent) : QWidget(parent, 0), prevKey_(-1), releaseTimer_(0), maximumSequenceLength_(4), modifierRequired_(false) {
	
    lineEdit_ = new QLineEdit(this);	
	lineEdit_->setContextMenuPolicy(Qt::PreventContextMenu);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(lineEdit_);

    lineEdit_->setFocusProxy(this);
    lineEdit_->installEventFilter(this);
    resetState();

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_MacShowFocusRect, true);
    setAttribute(Qt::WA_InputMethodEnabled, false);

	// clear button
	buttonClear_ = new QToolButton(lineEdit_);
	buttonClear_->setIcon(QIcon::fromTheme(QLatin1String("edit-clear")));
	buttonClear_->setCursor(Qt::ArrowCursor);
	buttonClear_->setStyleSheet(QLatin1String("QToolButton { border: none; padding: 0px; }"));
	buttonClear_->hide();

	connect(buttonClear_, SIGNAL(clicked()), this, SLOT(clear()));
	connect(lineEdit_, SIGNAL(textChanged(const QString &)), this, SLOT(updateCloseButton(const QString &)));

	const int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);

	setStyleSheet(QString(QLatin1String("QLineEdit { padding-right: %1px; }")).arg(buttonClear_->sizeHint().width() + frameWidth + 1));
	QSize msz = minimumSizeHint();
	setMinimumSize(qMax(msz.width(), buttonClear_->sizeHint().height() + frameWidth * 2 + 2), qMax(msz.height(), buttonClear_->sizeHint().height() + frameWidth * 2 + 2));
}

//------------------------------------------------------------------------------
// Name: KeySequenceEdit
// Desc: Constructor
//------------------------------------------------------------------------------
KeySequenceEdit::KeySequenceEdit(const QKeySequence &keySequence, QWidget *parent) : KeySequenceEdit(parent) {
    setKeySequence(keySequence);
}

//------------------------------------------------------------------------------
// Name: ~KeySequenceEdit
// Desc: Destroys the KeySequenceEdit object.
//------------------------------------------------------------------------------
KeySequenceEdit::~KeySequenceEdit() {
}

//------------------------------------------------------------------------------
// Name: maximumSequenceLength
//------------------------------------------------------------------------------
int KeySequenceEdit::maximumSequenceLength() const {
	return maximumSequenceLength_;
}

//------------------------------------------------------------------------------
// Name: setMaximumSequenceLength
//------------------------------------------------------------------------------
void KeySequenceEdit::setMaximumSequenceLength(int maximum) {

	if(maximum < 1 || maximum > 4) {
		qDebug("Maximum sequence length must be an integer from 1 to 4");
		return;
	}

	maximumSequenceLength_ = maximum;
}


//------------------------------------------------------------------------------
// Name: keySequence
// Desc: This property contains the currently chosen key_ sequence.
//       The shortcut can be changed by the user or via setter function.
//------------------------------------------------------------------------------
QKeySequence KeySequenceEdit::keySequence() const {
    return keySequence_;
}

//------------------------------------------------------------------------------
// Name: setKeySequence
//------------------------------------------------------------------------------
void KeySequenceEdit::setKeySequence(const QKeySequence &keySequence) {
    resetState();

    if (keySequence_ == keySequence) {
        return;
	}

    keySequence_ = keySequence;

	keys_.clear();

    for (size_t i = 0; i < keySequence_.count(); ++i) {
        keys_.push_back(keySequence[i]);
	}

    lineEdit_->setText(keySequence_.toString(QKeySequence::NativeText));

    Q_EMIT keySequenceChanged(keySequence);
}


//------------------------------------------------------------------------------
// Name: clear
// Desc: Clears the current key sequence.
//------------------------------------------------------------------------------
void KeySequenceEdit::clear() {
    setKeySequence(QKeySequence());
}

//------------------------------------------------------------------------------
// Name: event
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Name: keyPressEvent
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Name: keyReleaseEvent
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Name: timerEvent
//------------------------------------------------------------------------------
void KeySequenceEdit::timerEvent(QTimerEvent *e) {
    if (e->timerId() == releaseTimer_) {
        finishEditing();
        return;
    }

    QWidget::timerEvent(e);
}

//------------------------------------------------------------------------------
// Name: modifierRequired
//------------------------------------------------------------------------------
bool KeySequenceEdit::modifierRequired() const {
	return modifierRequired_;
}

//------------------------------------------------------------------------------
// Name: setModifierRequired
//------------------------------------------------------------------------------
void KeySequenceEdit::setModifierRequired(bool required) {
	modifierRequired_ = required;
}

//------------------------------------------------------------------------------
// Name: resizeEvent
//------------------------------------------------------------------------------
void KeySequenceEdit::resizeEvent(QResizeEvent *e) {
	Q_UNUSED(e);
    QSize sz = buttonClear_->sizeHint();
    const int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
    buttonClear_->move(rect().right() - frameWidth - sz.width(), (rect().bottom() + 1 - sz.height()) / 2);
}

//------------------------------------------------------------------------------
// Name: updateCloseButton
//------------------------------------------------------------------------------
void KeySequenceEdit::updateCloseButton(const QString &text) {
	buttonClear_->setVisible(!text.isEmpty());
}
