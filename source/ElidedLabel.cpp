
#include "ElidedLabel.h"
#include <QResizeEvent>

// TODO(eteran): 2.0, maybe support scrolling the text if you select it with the
//               mouse and pan left and right?

ElidedLabel::ElidedLabel(QWidget *parent, Qt::WindowFlags f) : QLabel(parent, f), elideMode_(Qt::ElideRight) {
}

ElidedLabel::ElidedLabel(const QString &text, QWidget *parent, Qt::WindowFlags f) : QLabel(text, parent, f), elideMode_(Qt::ElideRight), realString_(text) {
}

ElidedLabel::ElidedLabel(const QString &text, Qt::TextElideMode elideMode, QWidget *parent, Qt::WindowFlags f) : QLabel(text, parent, f), elideMode_(elideMode), realString_(text) {
}

void ElidedLabel::setText(const QString &text) {
	realString_ = text;
    QLabel::setText(fontMetrics().elidedText(realString_, elideMode_, width(), Qt::TextShowMnemonic));
}

void ElidedLabel::resizeEvent(QResizeEvent *event) {
	QLabel::resizeEvent(event);
	QLabel::setText(fontMetrics().elidedText(realString_, elideMode_, width(), Qt::TextShowMnemonic));
}

void ElidedLabel::setElideMode(Qt::TextElideMode elideMode) {
	elideMode_ = elideMode;
	updateGeometry();
}

Qt::TextElideMode ElidedLabel::elideMode() const {
	return elideMode_;
}
