
#include "ElidedLabel.h"
#include <QResizeEvent>

/**
 * @brief ElidedLabel::ElidedLabel
 * @param parent
 * @param f
 */
ElidedLabel::ElidedLabel(QWidget *parent, Qt::WindowFlags f) : QLabel(parent, f), elideMode_(Qt::ElideRight) {
}

/**
 * @brief ElidedLabel::ElidedLabel
 * @param text
 * @param parent
 * @param f
 */
ElidedLabel::ElidedLabel(const QString &text, QWidget *parent, Qt::WindowFlags f) : QLabel(text, parent, f), elideMode_(Qt::ElideRight), realString_(text) {
}

/**
 * @brief ElidedLabel::ElidedLabel
 * @param text
 * @param elideMode
 * @param parent
 * @param f
 */
ElidedLabel::ElidedLabel(const QString &text, Qt::TextElideMode elideMode, QWidget *parent, Qt::WindowFlags f) : QLabel(text, parent, f), elideMode_(elideMode), realString_(text) {
}

/**
 * @brief ElidedLabel::setText
 * @param text
 */
void ElidedLabel::setText(const QString &text) {
	realString_ = text;
    QLabel::setText(fontMetrics().elidedText(realString_, elideMode_, width(), Qt::TextShowMnemonic));
}

/**
 * @brief ElidedLabel::resizeEvent
 * @param event
 */
void ElidedLabel::resizeEvent(QResizeEvent *event) {
	QLabel::resizeEvent(event);
	QLabel::setText(fontMetrics().elidedText(realString_, elideMode_, width(), Qt::TextShowMnemonic));
}

/**
 * @brief ElidedLabel::setElideMode
 * @param elideMode
 */
void ElidedLabel::setElideMode(Qt::TextElideMode elideMode) {
	elideMode_ = elideMode;
	updateGeometry();
}

/**
 * @brief ElidedLabel::elideMode
 * @return
 */
Qt::TextElideMode ElidedLabel::elideMode() const {
	return elideMode_;
}
