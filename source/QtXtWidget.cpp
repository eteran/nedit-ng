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

#include <QtGui/QApplication>
#include <QtGui/QDesktopWidget>
#include <QtCore/QList>

#include "QtXtWidget.h"

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>

#include <QtGui/QX11Info>

typedef struct {
    int empty;
} QWidgetClassPart;

typedef struct _QWidgetClassRec {
    CoreClassPart	core_class;
    QWidgetClassPart	qwidget_class;
} QWidgetClassRec;

//static QWidgetClassRec qwidgetClassRec;

typedef struct {
    /* resources */
    /* (none) */
    /* private state */
    QtXtWidget* qxtwidget;
} QWidgetPart;

typedef struct _QWidgetRec {
    CorePart	core;
    QWidgetPart	qwidget;
} QWidgetRec;


static void reparentChildrenOf(QWidget* parent)
{
    const QObjectList children = parent->children();
    if (children.isEmpty())
        return; // nothing to do
    for (int i = 0; i < children.size(); ++i) {
	QWidget *widget = qobject_cast<QWidget *>(children.at(i));
	if (! widget)
            continue;
       	XReparentWindow(widget->x11Info().display(), widget->winId(),
                        parent->winId(), widget->x(), widget->y());
    }
}

void qwidget_realize(
	Widget                widget,
	XtValueMask*          mask,
	XSetWindowAttributes* attributes
    )
{
    widgetClassRec.core_class.realize(widget, mask, attributes);
    QtXtWidget* qxtw = ((QWidgetRec*)widget)->qwidget.qxtwidget;
    if (XtWindow(widget) != qxtw->winId()) {
	qxtw->create(XtWindow(widget), false, false);
	reparentChildrenOf(qxtw);
    }
    qxtw->show();
    XMapWindow( QX11Info::display(), qxtw->winId() );
}

static
QWidgetClassRec qwidgetClassRec = {
  { /* core fields */
    /* superclass		*/	(WidgetClass) &widgetClassRec,
    /* class_name		*/	(char*)"QWidget",
    /* widget_size		*/	sizeof(QWidgetRec),
    /* class_initialize		*/	0,
    /* class_part_initialize	*/	0,
    /* class_inited		*/	FALSE,
    /* initialize		*/	0,
    /* initialize_hook		*/	0,
    /* realize			*/	qwidget_realize,
    /* actions			*/	0,
    /* num_actions		*/	0,
    /* resources		*/	0,
    /* num_resources		*/	0,
    /* xrm_class		*/	NULLQUARK,
    /* compress_motion		*/	TRUE,
    /* compress_exposure	*/	TRUE,
    /* compress_enterleave	*/	TRUE,
    /* visible_interest		*/	FALSE,
    /* destroy			*/	0,
    /* resize			*/	XtInheritResize,
    /* expose			*/	XtInheritExpose,
    /* set_values		*/	0,
    /* set_values_hook		*/	0,
    /* set_values_almost	*/	XtInheritSetValuesAlmost,
    /* get_values_hook		*/	0,
    /* accept_focus		*/	XtInheritAcceptFocus,
    /* version			*/	XtVersion,
    /* callback_private		*/	0,
    /* tm_table			*/	XtInheritTranslations,
    /* query_geometry		*/	XtInheritQueryGeometry,
    /* display_accelerator	*/	XtInheritDisplayAccelerator,
    /* extension		*/	0
  },
  { /* qwidget fields */
    /* empty			*/	0
  }
};
static WidgetClass qWidgetClass = (WidgetClass)&qwidgetClassRec;


/*!
  \class QtXtWidget
  \brief The QtXtWidget class allows mixing of Xt/Motif and Qt widgets.
  \obsolete

  QtXtWidget acts as a bridge between Xt and Qt. When utilizing old Xt
  widgets, it can be a QWidget based on a Xt widget class. When
  including Qt widgets in an existing Xt/Motif application, it can be
  a special Xt widget class that is a QWidget. See the constructors
  for the different behaviors.

  \section1 Known Issues

  This class is unsupported and has many known problems and
  limitations. It is provided only to keep existing source working;
  it should not be used in new code. These problems will \e not be
  fixed in future releases.

  Below is an incomplete list of known issues:

  \list 1

  \o Keyboard focus navigation is impossible when using QtXtWidget.
  The mouse must be used to focus widgets in both Qt and Xt/Motif
  widgets.  For example, when embedding a QtXtWidget into an Xt/Motif
  widget, key events will go to the QtXtWidget (and its children) while
  the mouse is over the QtXtWidget, regardless of where Xt/Motif has
  placed the focus.

  \o Reparenting does not work.  You cannot use
  QWidget::reparent(). Since QWidget::showFullScreen()
  and QWidget::showNormal() rely on QWidget::reparent(), these
  functions will also not work as expected.
  \endlist
*/

void QtXtWidget::init(const char* name, WidgetClass widget_class,
		    Widget parent, QWidget* qparent,
		    ArgList args, Cardinal num_args,
		    bool managed)
{
    need_reroot=false;
    xtparent = 0;
    if ( parent ) {
	Q_ASSERT(!qparent);
	xtw = XtCreateWidget(name, widget_class, parent, args, num_args);
	if ( widget_class == qWidgetClass )
	    ((QWidgetRec*)xtw)->qwidget.qxtwidget = this;
	xtparent = parent;
	if (managed)
	    XtManageChild(xtw);
    } else {
	Q_ASSERT(!managed);

	String n, c;
	XtGetApplicationNameAndClass(QX11Info::display(), &n, &c);
	xtw = XtAppCreateShell(n, c, widget_class, QX11Info::display(),
			       args, num_args);
	if ( widget_class == qWidgetClass )
	    ((QWidgetRec*)xtw)->qwidget.qxtwidget = this;
    }

    if ( qparent ) {
	XtResizeWidget( xtw, 100, 100, 0 );
	XtSetMappedWhenManaged(xtw, False);
	XtRealizeWidget(xtw);
	XSync(QX11Info::display(), False);    // I want all windows to be created now
	XReparentWindow(QX11Info::display(), XtWindow(xtw), qparent->winId(), x(), y());
	XtSetMappedWhenManaged(xtw, True);
	need_reroot=true;
    }

    Arg reqargs[20];
    Cardinal nargs=0;
    XtSetArg(reqargs[nargs], XtNx, x());	nargs++;
    XtSetArg(reqargs[nargs], XtNy, y());	nargs++;
    XtSetArg(reqargs[nargs], XtNwidth, width());	nargs++;
    XtSetArg(reqargs[nargs], XtNheight, height());	nargs++;
    //XtSetArg(reqargs[nargs], "mappedWhenManaged", False);	nargs++;
    XtSetValues(xtw, reqargs, nargs);

    //#### destroy();   MLK

    if (!parent || XtIsRealized(parent))
	XtRealizeWidget(xtw);
}

/*!
  Constructs a QtXtWidget with the given \a name and \a parent
  of the special Xt widget class known as "QWidget" to the
  resource manager.

  Use this constructor to utilize Qt widgets in an Xt/Motif
  application. The QtXtWidget is a QWidget, so you can create
  subwidgets, layouts, and use other Qt features.

  If the \a managed parameter is true and \a parent is not null,
  XtManageChild is used to manage the child; otherwise it is
  unmanaged.
*/
QtXtWidget::QtXtWidget(const char* name, Widget parent, bool managed)
    : QWidget( 0 ), xtw( 0 )
{
    setObjectName(QString::fromUtf8(name));
    init(name, qWidgetClass, parent, 0, 0, 0, managed);
    Arg reqargs[20];
    Cardinal nargs=0;
    XtSetArg(reqargs[nargs], XtNborderWidth, 0);            nargs++;
    XtSetValues(xtw, reqargs, nargs);
}

/*!
  Constructs a QtXtWidget of the given \a widget_class called \a name.

  Use this constructor to utilize Xt or Motif widgets in a Qt
  application. The QtXtWidget looks and behaves like an Xt widget,
  but can be used like any QWidget.

  Note that Xt requires that the top level Xt widget is a shell.
  This means that if \a parent is a QtXtWidget, any kind of
  \a widget_class can be used. However, if there is no parent, or
  the parent is just a normal QWidget, \a widget_class should be
  something like \c topLevelShellWidgetClass.

  The \a args and \a num_args arguments are passed on to XtCreateWidget.

  If \a managed is true and \a parent is not null, XtManageChild is
  used to manage the child; otherwise it is unmanaged.
*/
QtXtWidget::QtXtWidget(const char* name, WidgetClass widget_class,
		     QWidget *parent, ArgList args, Cardinal num_args,
		     bool managed)
    : QWidget( parent ), xtw( 0 )
{
    setObjectName(QString::fromUtf8(name));
    if ( !parent )
	init(name, widget_class, 0, 0, args, num_args, managed);
    else if ( parent->inherits("QtXtWidget") )
	init(name, widget_class, ( (QtXtWidget*)parent)->xtw , 0, args, num_args, managed);
    else
	init(name, widget_class, 0, parent, args, num_args, managed);
    create(XtWindow(xtw), false, false);
}

/*!
  Destroys the QtXtWidget.
*/
QtXtWidget::~QtXtWidget()
{
    // Delete children first, as Xt will destroy their windows
    QList<QWidget *> list = qFindChildren<QWidget *>(0);
    for (int i = 0; i < list.size(); ++i) {
	QWidget *c;
        while ((c = qobject_cast<QWidget*>(list.at(i))) != 0)
            delete c;
    }

    if ( need_reroot ) {
	hide();
	XReparentWindow(QX11Info::display(), winId(), qApp->desktop()->winId(), x(), y());
    }

    XtDestroyWidget(xtw);
    destroy( false, false );
}

/*!
  \fn Widget QtXtWidget::xtWidget() const

  Returns the underlying Xt widget.
*/



/*!
  Reimplemented to produce the Xt effect of getting focus when the
  mouse enters the widget. The event is passed in \a e.
*/
bool QtXtWidget::x11Event( XEvent * e )
{
    if ( e->type == EnterNotify ) {
	if  ( xtparent )
	    activateWindow();
    }
    return QWidget::x11Event( e );
}


/*!
  Activates the widget. Implements a degree of focus handling for Xt widgets.
*/
void QtXtWidget::activateWindow()
{
    if  ( xtparent ) {
	if ( !QWidget::isActiveWindow() && isActiveWindow() ) {
	    XFocusChangeEvent e;
	    e.type = FocusIn;
	    e.window = winId();
	    e.mode = NotifyNormal;
	    e.detail = NotifyInferior;
	    XSendEvent( QX11Info::display(), e.window, TRUE, NoEventMask, (XEvent*)&e );
	}
    } else {
	QWidget::activateWindow();
    }
}

/*!
  Returns true if the widget is the active window; otherwise returns false.
  \omit
  Different from QWidget::isActiveWindow()
  \endomit
 */
bool QtXtWidget::isActiveWindow() const
{
    Window win;
    int revert;
    XGetInputFocus( QX11Info::display(), &win, &revert );

    if ( win == None) return false;

    QWidget *w = find( (WId)win );
    if ( w ) {
	// We know that window
	return w->window() == window();
    } else {
	// Window still may be a parent (if top-level is foreign window)
	Window root, parent;
	Window cursor = winId();
	Window *ch;
	unsigned int nch;
	while ( XQueryTree(QX11Info::display(), cursor, &root, &parent, &ch, &nch) ) {
	    if (ch) XFree( (char*)ch);
	    if ( parent == win ) return true;
	    if ( parent == root ) return false;
	    cursor = parent;
	}
	return false;
    }
}

/*!\reimp
 */
void QtXtWidget::moveEvent( QMoveEvent* )
{
    if ( xtparent || !xtw )
	return;
    XConfigureEvent c;
    c.type = ConfigureNotify;
    c.event = winId();
    c.window = winId();
    c.x = geometry().x();
    c.y = geometry().y();
    c.width = width();
    c.height = height();
    c.border_width = 0;
    XSendEvent( QX11Info::display(), c.event, TRUE, NoEventMask, (XEvent*)&c );
    XtMoveWidget( xtw, x(), y() );
}

/*!\reimp
 */
void QtXtWidget::resizeEvent( QResizeEvent* )
{
    if ( xtparent || !xtw )
	return;
    XtWidgetGeometry preferred;
    (void ) XtQueryGeometry( xtw, 0, &preferred );
    XConfigureEvent c;
    c.type = ConfigureNotify;
    c.event = winId();
    c.window = winId();
    c.x = geometry().x();
    c.y = geometry().y();
    c.width = width();
    c.height = height();
    c.border_width = 0;
    XSendEvent( QX11Info::display(), c.event, TRUE, NoEventMask, (XEvent*)&c );
    XtResizeWidget( xtw, width(), height(), preferred.border_width );
}
