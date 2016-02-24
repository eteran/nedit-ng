/****************************************************************************
** 
** Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
** 
** This file is part of a Qt Solutions component.
**
** Commercial Usage  
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Solutions Commercial License Agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and Nokia.
** 
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
** 
** In addition, as a special exception, Nokia gives you certain
** additional rights. These rights are described in the Nokia Qt LGPL
** Exception version 1.1, included in the file LGPL_EXCEPTION.txt in this
** package.
** 
** GNU General Public License Usage 
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
** 
** Please note Third Party Software included with Qt Solutions may impose
** additional restrictions and it is the user's responsibility to ensure
** that they have met the licensing requirements of the GPL, LGPL, or Qt
** Solutions Commercial license and the relevant license of the Third
** Party Software they are using.
** 
** If you are unsure which license is appropriate for your use, please
** contact Nokia at qt-info@nokia.com.
** 
****************************************************************************/

#ifndef QXTWIDGET_H
#define QXTWIDGET_H

#include <QtGui/qwidget.h>

#include <X11/Intrinsic.h>
#undef Bool
#undef Int


class QtXtWidget : public QWidget {
    Q_OBJECT
    Widget xtw;
    Widget xtparent;
    bool   need_reroot;
    void init(const char* name, WidgetClass widget_class,
              Widget parent, QWidget* qparent,
              ArgList args, Cardinal num_args,
              bool managed);
    friend void qwidget_realize( Widget widget, XtValueMask* mask,
                                 XSetWindowAttributes* attributes );

public:
    QtXtWidget(const char* name, Widget parent, bool managed=false);
    QtXtWidget(const char* name, WidgetClass widget_class,
              QWidget *parent=0, ArgList args=0, Cardinal num_args=0,
              bool managed=false);
    ~QtXtWidget();

    Widget xtWidget() const { return xtw; }
    bool isActiveWindow() const;
    void activateWindow();

protected:
    void moveEvent( QMoveEvent* );
    void resizeEvent( QResizeEvent* );
    bool x11Event( XEvent * );
};

#endif // QXTWIDGET_H
