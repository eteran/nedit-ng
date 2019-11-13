
#include "ElidedLabel.h"
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDesktopWidget>
#include <QMenu>
#include <QMimeData>
#include <QTextDocument>

/**
 * @brief ElidedLabel::ElidedLabel
 * @param text
 * @param parent
 */
ElidedLabel::ElidedLabel(const QString &text, QWidget *parent)
	: QLabel(parent) {

	setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
	fullText_  = text;
	elideMode_ = Qt::ElideMiddle;
	squeezeTextToLabel();
}

/**
 * @brief ElidedLabel::ElidedLabel
 * @param parent
 */
ElidedLabel::ElidedLabel(QWidget *parent)
	: QLabel(parent) {
	setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
	elideMode_ = Qt::ElideMiddle;
}

/**
 * @brief ElidedLabel::resizeEvent
 */
void ElidedLabel::resizeEvent(QResizeEvent *) {
	squeezeTextToLabel();
}

/**
 * @brief ElidedLabel::minimumSizeHint
 * @return
 */
QSize ElidedLabel::minimumSizeHint() const {
	QSize sh = QLabel::minimumSizeHint();
	sh.setWidth(-1);
	return sh;
}

/**
 * @brief ElidedLabel::sizeHint
 * @return
 */
QSize ElidedLabel::sizeHint() const {
	const int maxWidth = QApplication::desktop()->geometry().width() * 3 / 4;

	QFontMetrics fm(fontMetrics());

	int textWidth = fm.width(fullText_);
	if (textWidth > maxWidth) {
		textWidth = maxWidth;
	}

	return QSize(textWidth, QLabel::sizeHint().height());
}

/**
 * @brief ElidedLabel::setText
 * @param text
 */
void ElidedLabel::setText(const QString &text) {
	fullText_ = text;
	squeezeTextToLabel();
}

/**
 * @brief ElidedLabel::clear
 */
void ElidedLabel::clear() {
	fullText_.clear();
	QLabel::clear();
}

/**
 * @brief ElidedLabel::squeezeTextToLabel
 */
void ElidedLabel::squeezeTextToLabel() {

	QFontMetrics fm(fontMetrics());
	int labelWidth = size().width();
	QStringList squeezedLines;
	bool squeezed = false;

	for (const QString &line : fullText_.split(QLatin1Char('\n'))) {
		int lineWidth = fm.width(line);
		if (lineWidth > labelWidth) {
			squeezed = true;
			squeezedLines << fm.elidedText(line, elideMode_, labelWidth);
		} else {
			squeezedLines << line;
		}
	}

	if (squeezed) {
		QLabel::setText(squeezedLines.join(QLatin1String("\n")));
		setToolTip(fullText_);
	} else {
		QLabel::setText(fullText_);
		setToolTip(QString());
	}
}

/**
 * @brief ElidedLabel::setAlignment
 * @param alignment
 */
void ElidedLabel::setAlignment(Qt::Alignment alignment) {
	// save fullText and restore it
	QString tmpFull(fullText_);
	QLabel::setAlignment(alignment);
	fullText_ = tmpFull;
}

/**
 * @brief ElidedLabel::textElideMode
 * @return
 */
Qt::TextElideMode ElidedLabel::textElideMode() const {
	return elideMode_;
}

/**
 * @brief ElidedLabel::setTextElideMode
 * @param mode
 */
void ElidedLabel::setTextElideMode(Qt::TextElideMode mode) {
	elideMode_ = mode;
	squeezeTextToLabel();
}

/**
 * @brief ElidedLabel::fullText
 * @return
 */
QString ElidedLabel::fullText() const {
	return fullText_;
}

/**
 * @brief ElidedLabel::contextMenuEvent
 * @param ev
 */
void ElidedLabel::contextMenuEvent(QContextMenuEvent *ev) {
	// We want to reimplement "Copy" to include the elided text.
	// But this means reimplementing the full popup menu, so no more
	// copy-link-address or copy-selection support anymore, since we
	// have no access to the QTextDocument.
	// Maybe we should have a boolean flag in ElidedLabel itself for
	// whether to show the "Copy Full Text" custom popup?
	// For now I chose to show it when the text is squeezed; when it's not, the
	// standard popup menu can do the job (select all, copy).

	const bool squeezed        = text() != fullText_;
	const bool showCustomPopup = squeezed;
	if (showCustomPopup) {
		QMenu menu(this);

		auto act = new QAction(tr("&Copy Full Text"), &menu);
		connect(act, &QAction::triggered, this, [this]() {
			QApplication::clipboard()->setText(fullText_);
		});
		menu.addAction(act);

		ev->accept();
		menu.exec(ev->globalPos());
	} else {
		QLabel::contextMenuEvent(ev);
	}
}

/**
 * @brief ElidedLabel::keyPressEvent
 * @param event
 */
void ElidedLabel::keyPressEvent(QKeyEvent *event) {
	if (event == QKeySequence::Copy) {
		// Expand "..." when selecting with the mouse
		QString txt = selectedText();
		const QChar ellipsisChar(0x2026); // from qtextengine.cpp
		const int dotsPos = txt.indexOf(ellipsisChar);
		if (dotsPos > -1) {
			// Ex: abcde...yz, selecting de...y  (selectionStart=3)
			// charsBeforeSelection = selectionStart = 2 (ab)
			// charsAfterSelection = 1 (z)
			// final selection length= 26 - 2 - 1 = 23
			const int start         = selectionStart();
			int charsAfterSelection = text().length() - start - selectedText().length();
			txt                     = fullText_;

			// Strip markup tags
			if (textFormat() == Qt::RichText || (textFormat() == Qt::AutoText && Qt::mightBeRichText(txt))) {
				txt.replace(QRegExp(QLatin1String("<[^>]*>")), QString());
				// account for stripped characters
				charsAfterSelection -= fullText_.length() - txt.length();
			}

			txt = txt.mid(selectionStart(), txt.length() - start - charsAfterSelection);
		}

		QApplication::clipboard()->setText(txt, QClipboard::Clipboard);
	} else {
		QLabel::keyPressEvent(event);
	}
}

/**
 * @brief ElidedLabel::mouseReleaseEvent
 * @param event
 */
void ElidedLabel::mouseReleaseEvent(QMouseEvent *event) {

	if (QApplication::clipboard()->supportsSelection() && textInteractionFlags() != Qt::NoTextInteraction && event->button() == Qt::LeftButton && !fullText_.isEmpty() && hasSelectedText()) {
		// Expand "..." when selecting with the mouse
		QString txt = selectedText();
		const QChar ellipsisChar(0x2026); // from qtextengine.cpp
		const int dotsPos = txt.indexOf(ellipsisChar);
		if (dotsPos > -1) {
			// Ex: abcde...yz, selecting de...y  (selectionStart=3)
			// charsBeforeSelection = selectionStart = 2 (ab)
			// charsAfterSelection = 1 (z)
			// final selection length= 26 - 2 - 1 = 23
			const int start         = selectionStart();
			int charsAfterSelection = text().length() - start - selectedText().length();
			txt                     = fullText_;

			// Strip markup tags
			if (textFormat() == Qt::RichText || (textFormat() == Qt::AutoText && Qt::mightBeRichText(txt))) {
				txt.replace(QRegExp(QLatin1String("<[^>]*>")), QString());
				// account for stripped characters
				charsAfterSelection -= fullText_.length() - txt.length();
			}

			txt = txt.mid(selectionStart(), txt.length() - start - charsAfterSelection);
		}

		QApplication::clipboard()->setText(txt, QClipboard::Selection);
	} else {
		QLabel::mouseReleaseEvent(event);
	}
}
