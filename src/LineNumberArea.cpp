
#include "LineNumberArea.h"
#include "TextArea.h"
#include "TextBuffer.h"

#include <QPaintEvent>
#include <QPainter>

// Uncomment this to enable highlighting of the current line in the line number area.
// #define ENABLE_HIGHLIGHT

/**
 * @brief Constructor for LineNumberArea.
 *
 * @param area The TextArea object associated with this line number area.
 */
LineNumberArea::LineNumberArea(TextArea *area)
	: QWidget(area), area_(area) {
	resize(0, 0);
}

/**
 * @brief Returns the size hint for the line number area.
 *
 * @return The size hint for the line number area.
 * @note The width is determined by the maximum number of digits in the line numbers.
 *       The height is set to 0, as it will be determined by the parent widget.
 */
QSize LineNumberArea::sizeHint() const {
	return QSize(area_->lineNumberAreaWidth(), 0);
}

/**
 * @brief Paints the line number area.
 *
 * @param event The paint event that triggered this function.
 */
void LineNumberArea::paintEvent(QPaintEvent *event) {

	const int lineHeight = area_->fixedFontHeight_;

	QPainter painter(this);

	painter.fillRect(event->rect(), area_->lineNumBGColor_);
	painter.setPen(area_->lineNumFGColor_);
	painter.setFont(area_->font_);

	// Draw the line numbers, aligned to the text
	int y        = area_->viewport()->contentsRect().top();
	int64_t line = area_->getAbsTopLineNum();

#ifdef ENABLE_HIGHLIGHT
	const TextCursor cursor = area_->cursorPos_;
#endif
	for (int visLine = 0; visLine < area_->nVisibleLines_; visLine++) {

		const TextCursor lineStart = area_->lineStarts_[visLine];
#ifdef ENABLE_HIGHLIGHT
		if(area_->visibleLineContainsCursor(visLine, cursor)) {
			painter.fillRect(0, y, width(), lineHeight, Qt::darkCyan);
		}
#endif
		if (lineStart != -1 && (lineStart == 0 || area_->buffer_->BufGetCharacter(lineStart - 1) == '\n')) {
			const auto number = QString::number(line);
			const QRect rect(Padding, y, width() - (Padding * 2), lineHeight);
			painter.drawText(rect, Qt::TextSingleLine | Qt::AlignVCenter | Qt::AlignRight, number);
			++line;
		} else {
			if (visLine == 0) {
				++line;
			}
		}

		y += lineHeight;
	}
}

/**
 * @brief Handles the context menu event for the line number area.
 *
 * @param event The context menu event that triggered this function.
 */
void LineNumberArea::contextMenuEvent(QContextMenuEvent *event) {
	area_->contextMenuEvent(event);
}

/**
 * @brief Handles the wheel event for the line number area.
 *
 * @param event The wheel event that triggered this function.
 */
void LineNumberArea::wheelEvent(QWheelEvent *event) {
	area_->wheelEvent(event);
}

/**
 * @brief Handles the mouse double-click event for the line number area.
 *
 * @param event The mouse event that triggered this function.
 */
void LineNumberArea::mouseDoubleClickEvent(QMouseEvent *event) {

	QMouseEvent e(event->type(),
				  QPoint(std::max(0, event->x() - width()), event->y()),
				  event->button(),
				  event->buttons(),
				  event->modifiers());

	area_->mouseDoubleClickEvent(&e);
}

/**
 * @brief Handles the mouse move event for the line number area.
 *
 * @param event The mouse event that triggered this function.
 */
void LineNumberArea::mouseMoveEvent(QMouseEvent *event) {
	QMouseEvent e(event->type(),
				  QPoint(std::max(0, event->x() - width()), event->y()),
				  event->button(),
				  event->buttons(),
				  event->modifiers());
	area_->mouseMoveEvent(&e);
}

/**
 * @brief Handles the mouse press event for the line number area.
 *
 * @param event The mouse event that triggered this function.
 */
void LineNumberArea::mousePressEvent(QMouseEvent *event) {
	QMouseEvent e(event->type(),
				  QPoint(std::max(0, event->x() - width()), event->y()),
				  event->button(),
				  event->buttons(),
				  event->modifiers());
	area_->mousePressEvent(&e);
}

/**
 * @brief Handles the mouse release event for the line number area.
 *
 * @param event The mouse event that triggered this function.
 */
void LineNumberArea::mouseReleaseEvent(QMouseEvent *event) {
	QMouseEvent e(event->type(),
				  QPoint(std::max(0, event->x() - width()), event->y()),
				  event->button(),
				  event->buttons(),
				  event->modifiers());
	area_->mouseReleaseEvent(&e);
}
