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

#ifndef QMOTIFDIALOG_H
#define QMOTIFDIALOG_H

#include <QtGui/QDialog>

#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#undef Bool
#undef Int

class QtMotifDialogPrivate;

class QtMotifDialog : public QDialog
{
    Q_OBJECT
    QtMotifDialogPrivate *d;

public:
    QtMotifDialog(Widget parent, Qt::WFlags flags = 0);
    QtMotifDialog(const char *name, Widget parent, Qt::WFlags flags = 0);
    QtMotifDialog(QWidget *parent, Qt::WFlags flags = 0);
    QtMotifDialog(const char *name, QWidget *parent, Qt::WFlags flags = 0);
    ~QtMotifDialog();

    Widget shell() const;
    Widget dialog() const;

    static void acceptCallback(Widget, XtPointer, XtPointer);
    static void rejectCallback(Widget, XtPointer, XtPointer);

public Q_SLOTS:
    void accept();
    void reject();

protected:
    bool event(QEvent *);
    void showEvent(QShowEvent *event);
    void hideEvent(QHideEvent *event);

private:
    void init(const char* name = 0, Widget parent = NULL, ArgList args = NULL, Cardinal argcount = 0);

    void realize(Widget w);
    void insertChild(Widget w);
    void deleteChild(Widget w);

    friend void qmotif_dialog_realize(Widget, XtValueMask *, XSetWindowAttributes *);
    friend void qmotif_dialog_insert_child(Widget);
    friend void qmotif_dialog_delete_child(Widget);
    friend void qmotif_dialog_change_managed(Widget);
};

#endif // QMOTIFDIALOG_H
