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
#include <QtGui/QShowEvent>
#include <QtGui/QHideEvent>

#include <QtGui/QWidget>

#include <QtGui/QX11Info>

#include "QtMotifWidget.h"
#include "QtMotif.h"

#include <X11/StringDefs.h>
#include <X11/IntrinsicP.h>
#include <X11/ShellP.h>
#include <X11/Xatom.h>

const int XFocusOut = FocusOut;
const int XFocusIn = FocusIn;
#undef FocusOut
#undef FocusIn

const int XKeyPress = KeyPress;
const int XKeyRelease = KeyRelease;
#undef KeyPress
#undef KeyRelease

void qmotif_widget_shell_destroy(Widget w);
void qmotif_widget_shell_realize( Widget w, XtValueMask *mask,
				  XSetWindowAttributes *attr );
void qmotif_widget_shell_change_managed( Widget w );

// TopLevelShell subclass to wrap toplevel motif widgets into QWidgets

typedef struct {
    QtMotifWidget *widget;
} QTopLevelShellPart;

typedef struct _QTopLevelShellRec
{
    // full instance record declaration
    CorePart			core;
    CompositePart		composite;
    ShellPart			shell;
    WMShellPart			wmshell;
    VendorShellPart		vendorshell;
    TopLevelShellPart		toplevelshell;
    QTopLevelShellPart		qtoplevelshell;
} QTopLevelShellRec;

typedef struct
{
    // extension record
    XtPointer extension;
} QTopLevelShellClassPart;

typedef struct _QTopLevelShellClassRec {
    CoreClassPart		core_class;
    CompositeClassPart		composite_class;
    ShellClassPart		shell_class;
    WMShellClassPart		wmshell_class;
    VendorShellClassPart	vendorshell_class;
    TopLevelShellClassPart	toplevelshell_class;
    QTopLevelShellClassPart	qtoplevelshell_class;
} QTopLevelShellClassRec;

externalref QTopLevelShellClassRec 	qtoplevelShellClassRec;
externalref WidgetClass 		qtoplevelShellWidgetClass;
typedef struct _QTopLevelShellClassRec	*QTopLevelShellWidgetClass;
typedef struct _QTopLevelShellRec 	*QTopLevelShellWidget;

externaldef(qtoplevelshellclassrec)
    QTopLevelShellClassRec qtoplevelShellClassRec = {
	// core
	{
	    (WidgetClass) &topLevelShellClassRec,	// superclass
	    const_cast<char*>("QTopLevelShell"),	// class name
	    sizeof(QTopLevelShellRec),		        // widget size
	    NULL,					/* class_initialize proc */
	    NULL,					/* class_part_initialize proc */
	    FALSE,					/* class_inited flag */
	    NULL,					/* instance initialize proc */
	    NULL,					/* init_hook proc */
	    qmotif_widget_shell_realize,		/* realize widget proc */
	    NULL,					/* action table for class */
	    0,						/* num_actions */
	    NULL,					/* resource list of class */
	    0,						/* num_resources in list */
	    NULLQUARK,					/* xrm_class ? */
	    FALSE,					/* don't compress_motion */
	    XtExposeCompressSeries,			/* compressed exposure */
	    FALSE,					/* do compress enter-leave */
	    FALSE,					/* do have visible_interest */
	    NULL,					/* destroy widget proc */
	    XtInheritResize,				/* resize widget proc */
	    NULL,					/* expose proc */
	    NULL,					/* set_values proc */
	    NULL,					/* set_values_hook proc */
	    XtInheritSetValuesAlmost,			/* set_values_almost proc */
	    NULL,					/* get_values_hook */
	    NULL,					/* accept_focus proc */
	    XtVersion,					/* current version */
	    NULL,					/* callback offset    */
	    XtInheritTranslations,			/* default translation table */
	    XtInheritQueryGeometry,			/* query geometry widget proc */
	    NULL,					/* display accelerator	  */
	    NULL					/* extension record	 */
	},
	// composite
	{
	    XtInheritGeometryManager,			/* geometry_manager */
	    qmotif_widget_shell_change_managed,		// change managed
	    XtInheritInsertChild,			// insert_child
	    XtInheritDeleteChild,			// delete_child
	    NULL					// extension record
	},
	// shell extension record
	{ NULL },
	// wmshell extension record
	{ NULL },
	// vendorshell extension record
	{ NULL },
	// toplevelshell extension record
	{ NULL },
	// qtoplevelshell extension record
	{ NULL }
    };

externaldef(qtoplevelshellwidgetclass)
    WidgetClass qtoplevelShellWidgetClass = (WidgetClass)&qtoplevelShellClassRec;

// ApplicationShell subclass to wrap toplevel motif widgets into QWidgets

typedef struct {
    QtMotifWidget *widget;
} QApplicationShellPart;

typedef struct _QApplicationShellRec
{
    // full instance record declaration
    CorePart			core;
    CompositePart		composite;
    ShellPart			shell;
    WMShellPart			wmshell;
    VendorShellPart		vendorshell;
    TopLevelShellPart		toplevelshell;
    ApplicationShellPart   	applicationshell;
    QApplicationShellPart	qapplicationshell;
} QApplicationShellRec;

typedef struct
{
    // extension record
    XtPointer extension;
} QApplicationShellClassPart;

typedef struct _QApplicationShellClassRec {
    CoreClassPart		core_class;
    CompositeClassPart		composite_class;
    ShellClassPart		shell_class;
    WMShellClassPart		wmshell_class;
    VendorShellClassPart	vendorshell_class;
    TopLevelShellClassPart	toplevelshell_class;
    ApplicationShellClassPart   applicationshell_class;
    QApplicationShellClassPart	qapplicationshell_class;
} QApplicationShellClassRec;

externalref QApplicationShellClassRec		qapplicationShellClassRec;
externalref WidgetClass				qapplicationShellWidgetClass;
typedef struct _QApplicationShellClassRec	*QApplicationShellWidgetClass;
typedef struct _QApplicationShellRec		*QApplicationShellWidget;

externaldef(qapplicationshellclassrec)
    QApplicationShellClassRec qapplicationShellClassRec = {
	// core
	{
	    (WidgetClass) &applicationShellClassRec,	// superclass
	    const_cast<char*>("QApplicationShell"),	// class name
	    sizeof(QApplicationShellRec),		// widget size
	    NULL,					/* class_initialize proc */
	    NULL,					/* class_part_initialize proc */
	    FALSE,					/* class_inited flag */
	    NULL,					/* instance initialize proc */
	    NULL,					/* init_hook proc */
	    qmotif_widget_shell_realize,		/* realize widget proc */
	    NULL,					/* action table for class */
	    0,						/* num_actions */
	    NULL,					/* resource list of class */
	    0,						/* num_resources in list */
	    NULLQUARK,					/* xrm_class ? */
	    FALSE,					/* don't compress_motion */
	    XtExposeCompressSeries,			/* compressed exposure */
	    FALSE,					/* do compress enter-leave */
	    FALSE,					/* do have visible_interest */
	    qmotif_widget_shell_destroy,		/* destroy widget proc */
	    XtInheritResize,				/* resize widget proc */
	    NULL,					/* expose proc */
	    NULL,					/* set_values proc */
	    NULL,					/* set_values_hook proc */
	    XtInheritSetValuesAlmost,			/* set_values_almost proc */
	    NULL,					/* get_values_hook */
	    NULL,					/* accept_focus proc */
	    XtVersion,					/* current version */
	    NULL,					/* callback offset    */
	    XtInheritTranslations,			/* default translation table */
	    XtInheritQueryGeometry,			/* query geometry widget proc */
	    NULL,					/* display accelerator	  */
	    NULL					/* extension record	 */
	},
	// composite
	{
	    XtInheritGeometryManager,			/* geometry_manager */
	    qmotif_widget_shell_change_managed,		// change managed
	    XtInheritInsertChild,			// insert_child
	    XtInheritDeleteChild,			// delete_child
	    NULL					// extension record
	},
	// shell extension record
	{ NULL },
	// wmshell extension record
	{ NULL },
	// vendorshell extension record
	{ NULL },
	// toplevelshell extension record
	{ NULL },
	// applicationshell extension record
	{ NULL },
	// qapplicationshell extension record
	{ NULL }
    };

externaldef(qapplicationshellwidgetclass)
    WidgetClass qapplicationShellWidgetClass = (WidgetClass)&qapplicationShellClassRec;


class QtMotifWidgetPrivate
{
public:
    QtMotifWidgetPrivate()
        : widget( NULL ), shell( NULL ),
          filter1(0), filter2(0),
          isDestructing(false)
    { }

    Widget widget, shell;
    QObject *filter1, *filter2;
    bool isDestructing;
};

/*!
    \class QtMotifWidget
    \brief The QtMotifWidget class provides the QWidget API for Xt/Motif widgets.

    QtMotifWidget provides a QWidget that can act as a parent
    for any Xt/Motif widget. Since QtMotifWidget is a proper QWidget,
    it can be used as a top-level widget (specify 0 as the widget's
    parent) or as a child of any other QWidget.

    \bold{Note:} Since QtMotifWidget acts as a parent for Xt/Motif
    widgets, you should not create QWidgets with a QtMotifWidget parent
    because the child widgets will not interact as expected with their Motif
    siblings.

    An Xt/Motif widget, with a top-level QtMotifWidget parent, can begin
    using the standard Qt dialogs and custom QDialogs while keeping
    the main Xt/Motif interface of the application. Using a
    QtMotifWidget as the parent for the various QDialogs will ensure
    that modality and stacking works properly throughout the entire
    application.

    Applications moving to Qt may have custom Xt/Motif widgets that
    will take time to rewrite with Qt. Such applications can use
    these custom widgets as QtMotifWidgets with QWidget parents. This
    allows the application's interface to be replaced gradually.

    \warning QtMotifWidget uses the X11 window ID of the Motif widget
    directly instead of creating its own. As a result,
    QWidget::reparent() will not work. Since QWidget::showFullScreen()
    and QWidget::showNormal() rely on QWidget::reparent(), these
    functions will also not work as expected.
*/

/*!
    Constructs a QtMotifWidget of the given \a widgetClass as a child
    of \a parent, with the given \a name and widget flags specified by
    \a flags.

    The \a args and \a argCount arguments are passed on to
    XtCreateWidget.

    The resulting Xt/Motif widget can be accessed using the
    motifWidget() function.  The widget can be used as a parent for
    any other Xt/Motif widget.

    If \a parent is a QtMotifWidget, the Xt/Motif widget is created as
    a child of the parent's motifWidget(). If \a parent is 0 or a
    normal QWidget, the Xt/Motif widget is created as a child of a
    special \c TopLevelShell widget. Xt/Motif widgets can use this
    \c TopLevelShell as the parent for existing Xt/Motif dialogs or
    QtMotifDialogs.
*/
QtMotifWidget::QtMotifWidget(const char *name, WidgetClass widgetClass, QWidget *parent,
                           ArgList args, Cardinal argCount, Qt::WFlags flags)
    : QWidget(parent, flags)
{
    d = new QtMotifWidgetPrivate;

    setFocusPolicy( Qt::StrongFocus );

    Widget motifparent = NULL;
    if ( parent ) {
	if ( parent->inherits( "QtMotifWidget" ) ) {
	    // not really supported, but might be possible
	    motifparent = ( (QtMotifWidget *) parent )->motifWidget();
	} else {
	    // keep an eye on the position of the toplevel widget, so that we
	    // can tell our hidden shell its real on-screen position
            QWidget *tlw = parent;
            while (!tlw->isWindow()) {
                if (tlw->parentWidget()->inherits("QWorkspace")) {
                    d->filter2 = tlw;
                    d->filter2->installEventFilter(this);
                } else if (tlw->parentWidget()->inherits("QAbstractScrollAreaWidget")
                           || tlw->parentWidget()->inherits("QAbstractScrollAreaHelper")) {
                    break;
                }
                tlw = tlw->parentWidget();
                Q_ASSERT(tlw != 0);
            }
            d->filter1 = tlw;
            d->filter1->installEventFilter(this);

	}
    }

    bool isShell = widgetClass == applicationShellWidgetClass || widgetClass == topLevelShellWidgetClass;
    if (!motifparent || isShell) {
	ArgList realargs = new Arg[argCount + 3];
	Cardinal nargs = argCount;
	memcpy( realargs, args, sizeof( Arg ) * argCount );

	int screen = x11Info().screen();
	if (!QX11Info::appDefaultVisual(screen)) {
	    // make Motif use the same visual/colormap/depth as Qt (if
	    // Qt is not using the default)
	    XtSetArg(realargs[nargs], XtNvisual, QX11Info::appVisual(screen));
	    ++nargs;
	    XtSetArg(realargs[nargs], XtNcolormap, QX11Info::appColormap(screen));
	    ++nargs;
	    XtSetArg(realargs[nargs], XtNdepth, QX11Info::appDepth(screen));
	    ++nargs;
	}

	if (widgetClass == applicationShellWidgetClass) {
	    d->shell = XtAppCreateShell( name, name, qapplicationShellWidgetClass,
					 QtMotif::display(), realargs, nargs );
	    ( (QApplicationShellWidget) d->shell )->qapplicationshell.widget = this;
	} else {
	    d->shell = XtAppCreateShell( name, name, qtoplevelShellWidgetClass,
					 QtMotif::display(), realargs, nargs );
	    ( (QTopLevelShellWidget) d->shell )->qtoplevelshell.widget = this;
	}
	motifparent = d->shell;

	delete [] realargs;
	realargs = 0;
    }

    if (isShell)
	d->widget = d->shell;
    else
	d->widget = XtCreateWidget( name, widgetClass, motifparent, args, argCount );

#if QT_VERSION >= 0x040100
    setAttribute(Qt::WA_NoX11EventCompression, true);
#endif
}

/*!
    Destroys the QtMotifWidget. The special \c TopLevelShell is also
    destroyed, if it was created during construction.
*/
QtMotifWidget::~QtMotifWidget()
{
    d->isDestructing = true;
    QtMotif::unregisterWidget( this );

    // make sure we don't have any pending requests for the window we
    // are about to destroy
    XSync(x11Info().display(), FALSE);
    XSync(QtMotif::display(), FALSE);

    destroy( false );

    if (d->widget && !d->widget->core.being_destroyed)
        XtDestroyWidget( d->widget );

    if ( d->shell && d->shell != d->widget) {
        if (XtIsSubclass(d->shell, qapplicationShellWidgetClass)) {
            ( (QApplicationShellWidget) d->shell )->qapplicationshell.widget = 0;
        } else {
            Q_ASSERT(XtIsSubclass(d->shell, qtoplevelShellWidgetClass));
            ( (QTopLevelShellWidget) d->shell )->qtoplevelshell.widget = 0;
        }
        if (!d->shell->core.being_destroyed)
            XtDestroyWidget( d->shell );
    }

    // Make sure everything is finished before ~QWidget() runs
    XSync(x11Info().display(), FALSE);
    XSync(QtMotif::display(), FALSE);

    delete d;
}

/*!
    Returns the embedded Xt/Motif widget. If a \c Shell widget was
    created by the constructor, you can access it with XtParent().
*/
Widget QtMotifWidget::motifWidget() const
{
    return d->widget;
}

/*!
    Responds to the given \a event by ensuring that
    the embedded Xt/Motif widget is managed and shown.
*/
void QtMotifWidget::showEvent(QShowEvent *event)
{
    if (!event->spontaneous()) {
        if (d->widget && d->widget != d->shell )
            XtManageChild(d->widget);

        if ( d->shell ) {
            if ( ! XtIsRealized( d->shell ) )
                XtRealizeWidget( d->shell );

            XSync(x11Info().display(), FALSE);
            XSync(QtMotif::display(), FALSE);

            XtMapWidget(d->shell);
        }
    }
    QWidget::showEvent(event);
}

/*!
    Responds to the given \a event by ensuring that
    the embedded Xt/Motif widget is unmanaged and hidden.
*/
void QtMotifWidget::hideEvent(QHideEvent *event)
{
    if (!event->spontaneous()) {
        if (d->widget && d->widget != d->shell )
            XtUnmanageChild(d->widget);
        if ( d->shell )
            XtUnmapWidget(d->shell);
    }
    QWidget::hideEvent(event);
}


/*!
    \internal
    Wraps the Motif widget by setting the X window for the
    QtMotifWidget to the X window id of the widget shell.
*/
void QtMotifWidget::realize( Widget w )
{
    // use the winid of the dialog shell, reparent any children we
    // have
    if ( XtWindow( w ) != winId() ) {
	// flush both command queues to make sure that all windows
	// have been created
	XSync(x11Info().display(), FALSE);
	XSync(QtMotif::display(), FALSE);

	// save the geometry of the motif widget, since it has the
	// geometry we want
	QRect save( w->core.x, w->core.y, w->core.width, w->core.height );

	// save the window title
	QString wtitle = windowTitle();
	if (wtitle.isEmpty()) {
	    char *t = 0;
	    XtVaGetValues(w, XtNtitle, &t, NULL);
	    wtitle = QString::fromLocal8Bit(t);
	}
        setWindowTitle(QString()); // make sure setWindowTitle() works below

        QString icontext = windowIconText();
        if (icontext.isEmpty()) {
 	    char *iconName = 0;
 	    XtVaGetValues(w, XtNiconName, &iconName, NULL);
 	    icontext = QString::fromLocal8Bit(iconName);
        }
        setWindowIconText(QString()); // make sure setWindowIconText() works below

	Window newid = XtWindow( w );
	QObjectList list = children();
	for (int i = 0; i < list.size(); ++i) {
	    QWidget *widget = qobject_cast<QWidget*>(list.at(i));
	    if (!widget || widget->isWindow()) continue;

	    XReparentWindow(x11Info().display(), widget->winId(), newid,
			    widget->x(), widget->y());
	}

	// re-create this QWidget with the winid from the motif
	// widget... the geometry will be reset to roughly 1/4 of the
	// screen, so we need to restore it below
        bool visible = isVisible();
	create( newid, true, true );
        setAttribute(Qt::WA_WState_Visible, visible);

	// restore the window title and icon text
	if (!wtitle.isEmpty())
	    setWindowTitle(wtitle);
        if (!icontext.isEmpty())
            setWindowIconText(icontext);

	// restore geometry of the shell
	XMoveResizeWindow( x11Info().display(), winId(),
			   save.x(), save.y(), save.width(), save.height() );

	// if this QtMotifWidget has a parent widget, we should
	// reparent the shell into that parent
	if ( parentWidget() ) {
	    XReparentWindow( x11Info().display(), winId(),
			     parentWidget()->winId(), x(), y() );
	}

	// flush both command queues again, to make sure that we don't
	// get any of the above calls processed out of order
    	XSync(x11Info().display(), FALSE);
	XSync(QtMotif::display(), FALSE);
    }
    QtMotif::registerWidget( this );
}

/*!
    \internal
    Motif callback to send a close event to a QtMotifWidget
*/
void qmotif_widget_shell_destroy(Widget w)
{
    QtMotifWidget *widget = 0;
    if (XtIsSubclass(w, qapplicationShellWidgetClass)) {
	widget = ((QApplicationShellWidget) w)->qapplicationshell.widget;
    } else {
	Q_ASSERT(XtIsSubclass(w, qtoplevelShellWidgetClass));
	widget = ((QTopLevelShellWidget) w)->qtoplevelshell.widget;
    }
    if (!widget || widget->d->isDestructing)
	return;
    widget->d->shell = widget->d->widget = 0;
    widget->close();
    delete widget;
}

/*!
    \internal
    Motif callback to resolve a QtMotifWidget and call
    QtMotifWidget::realize().
*/
void qmotif_widget_shell_realize( Widget w, XtValueMask *mask, XSetWindowAttributes *attr )
{
    XtRealizeProc realize =
        XtSuperclass(w)->core_class.realize;
    (*realize)( w, mask, attr );

    QtMotifWidget *widget = 0;
    if (XtIsSubclass(w, qapplicationShellWidgetClass)) {
	widget = ((QApplicationShellWidget) w)->qapplicationshell.widget;
    } else {
	Q_ASSERT(XtIsSubclass(w, qtoplevelShellWidgetClass));
	widget = ((QTopLevelShellWidget) w)->qtoplevelshell.widget;
    }
    if ( ! widget )
	return;
    widget->realize( w );
}

/*!
    \internal
    Motif callback to resolve a QtMotifWidget and set the initial
    geometry of the widget.
*/
void qmotif_widget_shell_change_managed( Widget w )
{
    XtWidgetProc change_managed =
        ((CompositeWidgetClass) XtSuperclass(w))->composite_class.change_managed;
    (*change_managed)( w );

    QtMotifWidget *widget = 0;
    if (XtIsSubclass(w, qapplicationShellWidgetClass)) {
	widget = ((QApplicationShellWidget) w)->qapplicationshell.widget;
    } else {
	Q_ASSERT(XtIsSubclass(w, qtoplevelShellWidgetClass));
	widget = ((QTopLevelShellWidget) w)->qtoplevelshell.widget;
    }
    if ( ! widget )
	return;
    QRect r( widget->d->shell->core.x,
	     widget->d->shell->core.y,
	     widget->d->shell->core.width,
	     widget->d->shell->core.height ),
	x = widget->geometry();
    if ( x != r ) {
	// ### perhaps this should be a property that says "the
	// ### initial size of the QtMotifWidget should be taken from
	// ### the motif widget, otherwise use the size from the
	// ### parent widget (i.e. we are in a layout)"
	if ((! widget->isWindow() && widget->parentWidget() && widget->parentWidget()->layout())
	    || widget->testAttribute(Qt::WA_Resized)) {
	    // the widget is most likely resized a) by a layout or b) explicitly
	    XtMoveWidget( w, x.x(), x.y() );
	    XtResizeWidget( w, x.width(), x.height(), 0 );
	    widget->setGeometry( x );
	} else {
	    // take the size from the motif widget
	    widget->setGeometry( r );
	}
    }
}

/*!\reimp
 */
bool QtMotifWidget::event( QEvent* e )
{
    if ( dispatchQEvent( e, this ) )
	return true;
    return QWidget::event( e );
}

/*!\reimp
 */
bool QtMotifWidget::eventFilter( QObject *, QEvent *event )
{
    if (!d->shell)
        return false;

    switch (event->type()) {
    case QEvent::ParentChange:
        {
            // update event filters
            if (d->filter1)
                d->filter1->removeEventFilter(this);
            if (d->filter2)
                d->filter2->removeEventFilter(this);

            d->filter1 = d->filter2 = 0;

            QWidget *tlw = parentWidget();
            if (tlw) {
                while (!tlw->isWindow()) {
                    if (tlw->parentWidget()->inherits("QWorkspace")) {
                        d->filter2 = tlw;
                        d->filter2->installEventFilter(this);
                    } else if (tlw->parentWidget()->inherits("QAbstractScrollAreaWidget")
                               || tlw->parentWidget()->inherits("QAbstractScrollAreaHelper")) {
                        break;
                    }
                    tlw = tlw->parentWidget();
                    Q_ASSERT(tlw != 0);
                }
                d->filter1 = tlw;
                d->filter1->installEventFilter( this );
            }
        }
        // fall-through intended (makes sure our position is correct after a reparent)

    case QEvent::Move:
    case QEvent::Resize:
        {
            // the motif widget is embedded in our special shell, so when the
            // top-level widget moves, we need to inform the special shell
            // about our new position
            QPoint p = window()->geometry().topLeft() +
                       mapTo( window(), QPoint( 0, 0 ) );
            d->shell->core.x = p.x();
            d->shell->core.y = p.y();
            break;
        }
    default:
        break;
    }

    return false;
}

/*!\reimp
 */
bool QtMotifWidget::x11Event(XEvent *event)
{
    if (d->shell) {
        // the motif widget is embedded in our special shell, so when the
        // top-level widget moves, we need to inform the special shell
        // about our new position
        QPoint p = window()->geometry().topLeft() +
                   mapTo( window(), QPoint( 0, 0 ) );
        d->shell->core.x = p.x();
        d->shell->core.y = p.y();
    }
    return QWidget::x11Event(event);
}

bool QtMotifWidget::dispatchQEvent( QEvent* e, QWidget* w)
{
    switch ( e->type() ) {
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
	if ( QtMotif::lastEvent() ) {
	    QtMotif::lastEvent()->xany.window = w->winId();
	    QtMotif::redeliverEvent( QtMotif::lastEvent() );
	}
	break;
    case QEvent::FocusIn:
    {
	XFocusInEvent ev = { XFocusIn, 0, TRUE, QtMotif::display(), w->winId(),
			     NotifyNormal, NotifyPointer  };
	QtMotif::redeliverEvent( (XEvent*)&ev );
	break;
    }
    case QEvent::FocusOut:
    {
	XFocusOutEvent ev = { XFocusOut, 0, TRUE, QtMotif::display(), w->winId(),
			      NotifyNormal, NotifyPointer  };
	QtMotif::redeliverEvent( (XEvent*)&ev );
	break;
    }
    default:
	break;
    }
    return false;
}
