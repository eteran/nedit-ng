/*
Copyright (C) 2006 - 2015 Evan Teran
                          evan.teran@gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "LineEdit.h"
#include <QToolButton>
#include <QStyle>

//------------------------------------------------------------------------------
// Name: LineEdit
// Desc:
//------------------------------------------------------------------------------
LineEdit::LineEdit(QWidget *parent) : QLineEdit(parent), buttonClear_(new QToolButton(this)) {

	buttonClear_->setIcon(QIcon::fromTheme(QLatin1String("edit-clear")));
	buttonClear_->setCursor(Qt::ArrowCursor);
	buttonClear_->setStyleSheet(QLatin1String("QToolButton { border: none; padding: 0px; }"));
	buttonClear_->hide();

	connect(buttonClear_, SIGNAL(clicked()), this, SLOT(clear()));
	connect(this, SIGNAL(textChanged(const QString &)), this, SLOT(updateCloseButton(const QString &)));

	const int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);

	setStyleSheet(QString(QLatin1String("QLineEdit { padding-right: %1px; }")).arg(buttonClear_->sizeHint().width() + frameWidth + 1));
	QSize msz = minimumSizeHint();
	setMinimumSize(qMax(msz.width(), buttonClear_->sizeHint().height() + frameWidth * 2 + 2), qMax(msz.height(), buttonClear_->sizeHint().height() + frameWidth * 2 + 2));
}

//------------------------------------------------------------------------------
// Name: resizeEvent
// Desc:
//------------------------------------------------------------------------------
void LineEdit::resizeEvent(QResizeEvent *) {
    QSize sz = buttonClear_->sizeHint();
    const int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
    buttonClear_->move(rect().right() - frameWidth - sz.width(), (rect().bottom() + 1 - sz.height()) / 2);
}

//------------------------------------------------------------------------------
// Name: updateCloseButton
// Desc:
//------------------------------------------------------------------------------
void LineEdit::updateCloseButton(const QString &text) {
    buttonClear_->setVisible(!text.isEmpty());
}


