
#include "ElidedLabel.h"
#include "Font.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QMenu>
#include <QRegularExpression>
#include <QScreen>
#include <QTextDocument>

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
#include <QWindow>
#endif

/**
 * @brief Constructor for ElidedLabel.
 *
 * @param text The text to display in the label.
 * @param parent The parent widget for this label.
 */
ElidedLabel::ElidedLabel(const QString &text, QWidget *parent)
	: QLabel(parent) {

	setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
	fullText_  = text;
	elideMode_ = Qt::ElideMiddle;
	squeezeTextToLabel();
}

/**
 * @brief Constructor for ElidedLabel.
 *
 * @param parent The parent widget for this label.
 */
ElidedLabel::ElidedLabel(QWidget *parent)
	: QLabel(parent) {
	setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
	elideMode_ = Qt::ElideMiddle;
}

/**
 * @brief Reimplemented resizeEvent to adjust the text when the label is resized.
 */
void ElidedLabel::resizeEvent(QResizeEvent *event) {
	Q_UNUSED(event);
	squeezeTextToLabel();
}

/**
 * @brief Returns the minimum size hint for the label.
 *
 * @return The minimum size hint for the label.
 */
QSize ElidedLabel::minimumSizeHint() const {
	QSize sh = QLabel::minimumSizeHint();
	sh.setWidth(-1);
	return sh;
}

/**
 * @brief Returns the size hint for the label.
 *
 * @return The size hint for the label.
 */
QSize ElidedLabel::sizeHint() const {
	QScreen *currentScreen = QGuiApplication::primaryScreen();

	const int maxWidth = currentScreen->geometry().width() * 3 / 4;
	const QFontMetrics fm(fontMetrics());

	int textWidth = Font::stringWidth(fm, fullText_);
	if (textWidth > maxWidth) {
		textWidth = maxWidth;
	}

	return QSize(textWidth, QLabel::sizeHint().height());
}

/**
 * @brief Sets the text for the label.
 *
 * @param text The new text to set for the label.
 */
void ElidedLabel::setText(const QString &text) {
	fullText_ = text;
	squeezeTextToLabel();
}

/**
 * @brief Clears the text in the label.
 */
void ElidedLabel::clear() {
	fullText_.clear();
	QLabel::clear();
}

/**
 * @brief Squeezes the text to fit within the label's width, eliding it if necessary.
 */
void ElidedLabel::squeezeTextToLabel() {

	const QFontMetrics fm(fontMetrics());
	const int labelWidth = size().width();
	QStringList squeezedLines;
	bool squeezed = false;

	for (const QString &line : fullText_.split(QLatin1Char('\n'))) {
		const int lineWidth = Font::stringWidth(fm, line);
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
 * @brief Sets the alignment for the label.
 *
 * @param alignment The alignment to set for the label.
 */
void ElidedLabel::setAlignment(Qt::Alignment alignment) {
	// save fullText and restore it
	QString tmpFull(std::move(fullText_));
	QLabel::setAlignment(alignment);
	fullText_ = std::move(tmpFull);
}

/**
 * @brief Returns the text elide mode used by the label.
 *
 * @return The text elide mode used by the label.
 */
Qt::TextElideMode ElidedLabel::textElideMode() const {
	return elideMode_;
}

/**
 * @brief Sets the text elide mode for the label.
 *
 * @param mode The text elide mode to set for the label.
 */
void ElidedLabel::setTextElideMode(Qt::TextElideMode mode) {
	elideMode_ = mode;
	squeezeTextToLabel();
}

/**
 * @brief Returns the full text set via setText.
 *
 * @return The full, un-elided text of the label.
 */
QString ElidedLabel::fullText() const {
	return fullText_;
}

/**
 * @brief Reimplemented contextMenuEvent to provide a custom context menu
 *
 * @param ev The context menu event that triggered this function.
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

	const bool showCustomPopup = text() != fullText_;
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
 * @brief Reimplemented keyPressEvent to handle the Copy action.
 * This function allows the user to copy the full text of the label.
 *
 * @param event The key event that triggered this function.
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
			// final selection length = 26 - 2 - 1 = 23
			const int start         = selectionStart();
			int charsAfterSelection = text().length() - start - selectedText().length();
			txt                     = fullText_;

			// Strip markup tags
			if (textFormat() == Qt::RichText || (textFormat() == Qt::AutoText && Qt::mightBeRichText(txt))) {
				txt.replace(QRegularExpression(QLatin1String("<[^>]*>")), QString());
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
 * @brief Reimplemented mouseReleaseEvent to handle the selection and copying of text.
 * This function allows the user to select the full text of the label.
 *
 * @param event The mouse event that triggered this function.
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
			// final selection length = 26 - 2 - 1 = 23
			const int start         = selectionStart();
			int charsAfterSelection = text().length() - start - selectedText().length();
			txt                     = fullText_;

			// Strip markup tags
			if (textFormat() == Qt::RichText || (textFormat() == Qt::AutoText && Qt::mightBeRichText(txt))) {
				txt.replace(QRegularExpression(QLatin1String("<[^>]*>")), QString());
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
