
#include "LineNumberArea.h"
#include "TextArea.h"
#include "TextBuffer.h"

#include <QPaintEvent>
#include <QPainter>

/**
 * @brief LineNumberArea::LineNumberArea
 * @param editor
 */
LineNumberArea::LineNumberArea(TextArea *area)
	: QWidget(area), area_(area) {
	resize(0, 0);
}

/**
 * @brief LineNumberArea::sizeHint
 * @return
 */
QSize LineNumberArea::sizeHint() const {
	return QSize(area_->lineNumberAreaWidth(), 0);
}

/**
 * @brief LineNumberArea::paintEvent
 * @param event
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

#if 0
	const TextCursor cursor = area_->cursorPos_;
#endif
	for (int visLine = 0; visLine < area_->nVisibleLines_; visLine++) {

		const TextCursor lineStart = area_->lineStarts_[visLine];
#if 0
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
 * @brief LineNumberArea::contextMenuEvent
 * @param event
 */
void LineNumberArea::contextMenuEvent(QContextMenuEvent *event) {
	area_->contextMenuEvent(event);
}

/**
 * @brief LineNumberArea::wheelEvent
 * @param event
 */
void LineNumberArea::wheelEvent(QWheelEvent *event) {
	area_->wheelEvent(event);
}

/**
 * @brief LineNumberArea::mouseDoubleClickEvent
 * @param event
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
 * @brief LineNumberArea::mouseMoveEvent
 * @param event
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
 * @brief LineNumberArea::mousePressEvent
 * @param event
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
 * @brief LineNumberArea::mouseReleaseEvent
 * @param event
 */
void LineNumberArea::mouseReleaseEvent(QMouseEvent *event) {
	QMouseEvent e(event->type(),
				  QPoint(std::max(0, event->x() - width()), event->y()),
				  event->button(),
				  event->buttons(),
				  event->modifiers());
	area_->mouseReleaseEvent(&e);
}
