
#include "KeySequenceEdit.h"

#include <QAction>
#include <QBoxLayout>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QStyle>

namespace {

/**
 * @brief Extracts the keyboard modifiers from a QKeyEvent.
 *
 * @param e The QKeyEvent from which to extract the modifiers.
 * @return An integer representing the extracted modifiers.
 *         The result is a bitwise OR of the modifier flags (Qt::SHIFT, Qt::CTRL, Qt::META, Qt::ALT).
 */
int ExtractModifiers(QKeyEvent *e) {

	const Qt::KeyboardModifiers state = e->modifiers();
	const QString &text               = e->text();

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
 * @brief Resets the state of the KeySequenceEdit widget.
 */
void KeySequenceEdit::resetState() {
	if (releaseTimer_) {
		killTimer(releaseTimer_);
		releaseTimer_ = 0;
	}

	prevKey_ = -1;
	lineEdit_->setText(keySequence_.toString(QKeySequence::NativeText));
	lineEdit_->setPlaceholderText(tr("Press shortcut"));

	// hook the clear button...
	if (auto action = lineEdit_->findChild<QAction *>()) {
		connect(action, &QAction::triggered, this, &KeySequenceEdit::clear, Qt::QueuedConnection);
	}
}

/**
 * @brief Finishes the editing of the key sequence.
 */
void KeySequenceEdit::finishEditing() {
	resetState();
	Q_EMIT keySequenceChanged(keySequence_);
	Q_EMIT editingFinished();
}

/**
 * @brief Constructor for KeySequenceEdit.
 *
 * @param parent The parent widget, defaults to nullptr.
 * @param f The window flags, defaults to Qt::WindowFlags().
 */
KeySequenceEdit::KeySequenceEdit(QWidget *parent, Qt::WindowFlags f)
	: QWidget(parent, f) {

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
 * @brief Constructor for KeySequenceEdit with an initial key sequence.
 *
 * @param keySequence The initial key sequence to set.
 * @param parent The parent widget, defaults to nullptr.
 * @param f The window flags, defaults to Qt::WindowFlags().
 */
KeySequenceEdit::KeySequenceEdit(const QKeySequence &keySequence, QWidget *parent, Qt::WindowFlags f)
	: KeySequenceEdit(parent, f) {
	setKeySequence(keySequence);
}

/**
 * @brief Returns the maximum length of the key sequence.
 *
 * @return The maximum length of the key sequence, which is an integer between 1 and 4.
 */
int KeySequenceEdit::maximumSequenceLength() const {
	return maximumSequenceLength_;
}

/**
 * @brief Sets the maximum length of the key sequence.
 *
 * @param maximum The maximum length to set for the key sequence.
 */
void KeySequenceEdit::setMaximumSequenceLength(int maximum) {

	if (maximum < 1 || maximum > 4) {
		qWarning("Maximum sequence length must be an integer from 1 to 4");
		return;
	}

	maximumSequenceLength_ = maximum;

	Q_EMIT maximumSequenceLengthChanged(maximum);
}

/**
 * @brief Returns the current key sequence.
 *
 * @return The current key sequence as a QKeySequence object.
 */
QKeySequence KeySequenceEdit::keySequence() const {
	return keySequence_;
}

/**
 * @brief Sets the key sequence for the KeySequenceEdit widget.
 *
 * @param keySequence The key sequence to set.
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
 * @brief Clears the current key sequence.
 */
void KeySequenceEdit::clear() {
	setKeySequence(QKeySequence());
}

/**
 * @brief Handles events for the KeySequenceEdit widget.
 *
 * @param e The event to handle.
 * @return `true` if the event was handled, `false` otherwise.
 */
bool KeySequenceEdit::event(QEvent *e) {
	switch (e->type()) {
	case QEvent::Shortcut:
		return true;
	case QEvent::ShortcutOverride:
		e->accept();
		return true;
	default:
		break;
	}

	return QWidget::event(e);
}

/**
 * @brief Handles key press events for the KeySequenceEdit widget.
 *
 * @param e The key event to handle.
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

	if (modifierRequired_ && e->modifiers() == Qt::NoModifier) {
		return;
	}

	const QString selectedText = lineEdit_->selectedText();
	if (!selectedText.isEmpty() && selectedText == lineEdit_->text()) {
		clear();
		if (nextKey == Qt::Key_Backspace) {
			return;
		}
	}

	if (keys_.size() >= maximumSequenceLength_) {
		return;
	}

	nextKey |= ExtractModifiers(e);

	keys_.push_back(nextKey);

	switch (keys_.size()) {
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
 * @brief Handles key release events for the KeySequenceEdit widget.
 *
 * @param e The key event to handle.
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
 * @brief Handles timer events for the KeySequenceEdit widget.
 *
 * @param e The timer event to handle.
 */
void KeySequenceEdit::timerEvent(QTimerEvent *e) {
	if (e->timerId() == releaseTimer_) {
		finishEditing();
		return;
	}

	QWidget::timerEvent(e);
}

/**
 * @brief Returns whether a modifier key is required for the key sequence.
 *
 * @return `true` if a modifier key is required, `false` otherwise.
 */
bool KeySequenceEdit::modifierRequired() const {
	return modifierRequired_;
}

/**
 * @brief Sets whether a modifier key is required for the key sequence.
 *
 * @param required If `true`, a modifier key is required; if `false`, it is not required.
 */
void KeySequenceEdit::setModifierRequired(bool required) {
	modifierRequired_ = required;
	Q_EMIT modifierRequiredChanged(required);
}
