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

#include <QtGui/QDialog>

#include <QtGui/QX11Info>

#include "QtMotif.h"
#include "QtMotifDialog.h"
#include "QtMotifWidget.h"

#include <X11/StringDefs.h>
#include <Xm/DialogS.h>
#include <Xm/DialogSP.h>

#include <Xm/MessageB.h>
#include <Xm/SelectioB.h>
#include <Xm/FileSB.h>
#include <Xm/Command.h>


XtGeometryResult qmotif_dialog_geometry_manger( Widget,
						XtWidgetGeometry *,
						XtWidgetGeometry * );
void qmotif_dialog_realize( Widget, XtValueMask *, XSetWindowAttributes * );
void qmotif_dialog_insert_child( Widget );
void qmotif_dialog_delete_child( Widget );
void qmotif_dialog_change_managed( Widget );

// XmDialogShell subclass to wrap motif dialogs into QDialogs

typedef struct {
    QtMotifDialog *dialog;
} QtMotifDialogPart;

typedef struct _QtMotifDialogRec
{
    // full instance record declaration
    CorePart			core;
    CompositePart		composite;
    ShellPart			shell;
    WMShellPart			wmshell;
    VendorShellPart		vendorshell;
    TransientShellPart		transientshell;
    XmDialogShellPart		dialogshell;
    QtMotifDialogPart		qmotifdialog;
} QtMotifDialogRec;

typedef struct
{
    // extension record
    XtPointer extension;
} QtMotifDialogClassPart;

typedef struct _QtMotifDialogClassRec
{
    CoreClassPart		core_class;
    CompositeClassPart		composite_class;
    ShellClassPart		shell_class;
    WMShellClassPart		wmshell_class;
    VendorShellClassPart	vendorshell_class;
    TransientShellClassPart	transientshell_class;
    XmDialogShellClassPart	dialogshell_class;
    QtMotifDialogClassPart	qmotifdialog_class;
} QtMotifDialogClassRec;

externalref QtMotifDialogClassRec	qmotifDialogShellClassRec;
externalref WidgetClass			qmotifDialogWidgetClass;
typedef struct _QtMotifDialogClassRec   *QtMotifDialogWidgetClass;
typedef struct _QtMotifDialogRec	       *QtMotifDialogWidget;

externaldef(qmotifdialogclassrec)
    QtMotifDialogClassRec qmotifDialogClassRec = {
	// Core
	{
	    (WidgetClass) &xmDialogShellClassRec,	/* superclass */
	    const_cast<char*>("QtMotifDialog"),		/* class_name */
	    sizeof(QtMotifDialogRec),			/* widget_size */
	    NULL,					/* class_initialize proc */
	    NULL,					/* class_part_initialize proc */
	    FALSE,					/* class_inited flag */
	    NULL,					/* instance initialize proc */
	    NULL,					/* init_hook proc */
	    qmotif_dialog_realize,			/* realize widget proc */
	    NULL,					/* action table for class */
	    0,						/* num_actions */
	    NULL,					/* resource list of class */
	    0,						/* num_resources in list */
	    NULLQUARK,					/* xrm_class ? */
	    FALSE,					/* don't compress_motion */
	    XtExposeCompressSeries,		 	/* compressed exposure */
	    FALSE,					/* do compress enter-leave */
	    FALSE,					/* do have visible_interest */
	    NULL,					/* destroy widget proc */
	    XtInheritResize,				/* resize widget proc */
	    NULL,					/* expose proc */
	    NULL,		 			/* set_values proc */
	    NULL,					/* set_values_hook proc */
	    XtInheritSetValuesAlmost,			/* set_values_almost proc */
	    NULL,					/* get_values_hook */
	    NULL,					/* accept_focus proc */
	    XtVersion,					/* current version */
	    NULL,					/* callback offset    */
	    XtInheritTranslations,			/* default translation table */
	    XtInheritQueryGeometry,			/* query geometry widget proc */
	    NULL,					/* display accelerator    */
	    NULL,					/* extension record      */
	},
	// Composite
	{
	    qmotif_dialog_geometry_manger,		// geometry_manager
	    qmotif_dialog_change_managed,		// change managed
	    qmotif_dialog_insert_child,			// insert_child
	    qmotif_dialog_delete_child,			// delete_child
	    NULL,					// extension record
	},
	// Shell extension record
	{ NULL },
	// WMShell extension record
	{ NULL },
	// VendorShell extension record
	{ NULL },
	// TransientShell extension record
	{ NULL },
	// XmDialogShell extension record
	{ NULL },
	// QMOTIGDIALOG
	{ NULL }
    };

externaldef(qmotifdialogwidgetclass)
    WidgetClass qmotifDialogWidgetClass = (WidgetClass)&qmotifDialogClassRec;


class QtMotifDialogPrivate
{
public:
    QtMotifDialogPrivate() : shell( NULL ), dialog( NULL ) { }

    Widget shell;
    Widget dialog;
};

/*!
    \class QtMotifDialog
    \brief The QtMotifDialog class provides the QDialog API for Motif-based dialogs.

    QtMotifDialog provides two separate modes of operation.
    Application programmers can use QtMotifDialog with an existing
    Motif-based dialog and a QWidget parent, or they can use
    QtMotifDialog with a custom Qt-based dialog and a Motif-based
    parent. Modality continues to work as expected.

    Motif-based dialogs must have a \c Shell widget parent with a
    single child, due to requirements of the Motif toolkit. The
    \c Shell widget, which is a special subclass of \c XmDialogShell, is
    created during construction. It can be accessed using the shell()
    member function.

    The single child of the \c Shell can be accessed using the
    dialog() member function \e after it has been created.

    The acceptCallback() and rejectCallback() functions provide a
    convenient way to call QDialog::accept() and QDialog::reject()
    through callbacks. A pointer to the QtMotifDialog should be passed
    as the \c client_data argument to the callback.

    The QtMotifDialog's API and behavior is identical to that of QDialog
    when using a custom Qt-based dialog with a Motif-based parent.
    The only difference is that a Motif-based \e parent argument is
    passed to the constructor, instead of a QWidget parent.
*/

/*!
    Constructs a QtMotifDialog with the given \a parent and window
    flags specified by \a flags that allows the application programmer to
    use the Motif-based \a parent for a custom QDialog.

    This constructor creates a \c Shell widget, which is a special
    subclass of \c XmDialogShell. You can access the \c Shell widget
    with the shell() member function.

    \sa shell()
*/
QtMotifDialog::QtMotifDialog( Widget parent, Qt::WFlags flags )
    : QDialog(0, flags)
{
    init( 0, parent );
}

/*!
  \overload

  The created \c Shell widget is given the name \a name.
*/

QtMotifDialog::QtMotifDialog( const char* name, Widget parent, Qt::WFlags flags )
    : QDialog(0, flags)
{
    init( name, parent );
}

/*!
    Constructs a QtMotifDialog with the given \a parent and window flags
    specified by \a flags that allows the application programmer
    to use a QWidget parent for an existing Motif-based dialog.

    This constructor creates a \c Shell widget, which is a special
    subclass of \c XmDialogShell. You can access the \c Shell widget
    with the shell() member functon.

    Note that a dialog widget is not created, only the shell. Instead,
    you should create the dialog widget as a child of the
    QtMotifDialog. QtMotifDialog will take ownership of your custom
    dialog, and you can access it with the dialog() member function.

    \warning QtMotifDialog takes ownership of the child widget and
    destroys it during destruction. You should not destroy the dialog
    widget yourself.

    \sa shell() dialog()
*/
QtMotifDialog::QtMotifDialog( QWidget *parent, Qt::WFlags flags )
    : QDialog(parent, flags)
{
    init();
}

/*!
  \overload

  The created \c Shell widget is given the name \a name.
*/

QtMotifDialog::QtMotifDialog( const char *name, QWidget *parent, Qt::WFlags flags )
    : QDialog(parent, flags)
{
    init( name );
}

/*!
    \internal
    Initializes QtMotifDialog by creating the QtMotifDialogPrivate data
    and the Shell widget.
*/
void QtMotifDialog::init( const char *name, Widget parent, ArgList args, Cardinal argcount )
{
    d = new QtMotifDialogPrivate;

    Arg *realargs = new Arg[ argcount + 3 ];
    memcpy( realargs, args, argcount * sizeof(Arg) );
    int screen = x11Info().screen();
    if ( !QX11Info::appDefaultVisual(screen)) {
	// make Motif use the same visual/colormap/depth as Qt (if Qt
	// is not using the default)
	XtSetArg(realargs[argcount], XtNvisual, QX11Info::appVisual(screen));
	++argcount;
	XtSetArg(realargs[argcount], XtNcolormap, QX11Info::appColormap(screen));
	++argcount;
	XtSetArg(realargs[argcount], XtNdepth, QX11Info::appDepth(screen));
	++argcount;
    }

    // create the dialog shell
    if ( parent ) {
        if (!name)
            d->shell = XtCreatePopupShell( "QtMotifDialog", qmotifDialogWidgetClass, parent,
				       realargs, argcount );
        else
            d->shell = XtCreatePopupShell( name, qmotifDialogWidgetClass, parent,
				       realargs, argcount );
    } else {
        if (!name)
            d->shell = XtAppCreateShell( "QtMotifDialog", "QtMotifDialog", qmotifDialogWidgetClass,
                                         QtMotif::display(), realargs, argcount );
        else
            d->shell = XtAppCreateShell( name, name, qmotifDialogWidgetClass,
                                         QtMotif::display(), realargs, argcount );
    }

    ( (QtMotifDialogWidget) d->shell )->qmotifdialog.dialog = this;

    delete [] realargs;
    realargs = 0;
}

/*!
    Destroys the QDialog, dialog widget, and \c Shell widget.
*/
QtMotifDialog::~QtMotifDialog()
{
    QtMotif::unregisterWidget( this );
    ( (QtMotifDialogWidget) d->shell )->qmotifdialog.dialog = 0;
    XtDestroyWidget( d->shell );

    // make sure we don't have any pending requests for the window we
    // are about to destroy
    XSync(x11Info().display(), FALSE);
    XSync(QtMotif::display(), FALSE);
    destroy( false );

    // Make sure everything is finished before ~QWidget() runs
    XSync(x11Info().display(), FALSE);
    XSync(QtMotif::display(), FALSE);

    delete d;
}

/*!
    Returns the \c Shell widget embedded in this dialog.
*/
Widget QtMotifDialog::shell() const
{
    return d->shell;
}

/*!
    Returns the Motif widget embedded in this dialog.
*/
Widget QtMotifDialog::dialog() const
{
    return d->dialog;
}

/*!
  \reimp
*/
void QtMotifDialog::accept()
{
    QDialog::accept();
}

/*!
  \reimp
*/
void QtMotifDialog::reject()
{
    QDialog::reject();
}

/*!
    This function is a reimplementation of QWidget::showEvent(). The
    reimplemented function handles the given \a event by ensuring that the
    dialog widget is managed and hidden.
*/
void QtMotifDialog::showEvent(QShowEvent *event)
{
    if (!event->spontaneous()) {
        // tell motif about modality
        Arg args[1];
        XtSetArg( args[0], XmNdialogStyle,
                  (testAttribute(Qt::WA_ShowModal)
                   ? XmDIALOG_FULL_APPLICATION_MODAL :
                   XmDIALOG_MODELESS));
        XtSetValues( d->shell, args, 1 );
        XtSetMappedWhenManaged( d->shell, False );
        if ( d->dialog ) {
            XtManageChild( d->dialog );

            XSync(x11Info().display(), FALSE);
            XSync(QtMotif::display(), FALSE);
        } else if ( !parentWidget() ) {
            adjustSize();
            QApplication::sendPostedEvents(this, QEvent::Resize);

            Widget p = XtParent( d->shell ), s = p;
            while ( s != NULL && !XtIsShell( s ) ) // find the shell
                s = XtParent( s );

            if ( p && s ) {
                int offx = ( (  XtWidth( p ) -  width() ) / 2 );
                int offy = ( ( XtHeight( p ) - height() ) / 2 );
                move( XtX ( s ) + offx, XtY( s ) + offy );
            }
        }
        XtSetMappedWhenManaged( d->shell, True );
    }
    QDialog::showEvent(event);

}

/*!
   This function is a reimplementation of QWidget::hideEvent(). The
   reimplemented function handles the given \a event by ensuring that the
   dialog widget is unmanaged and hidden.
*/
void QtMotifDialog::hideEvent(QHideEvent *event)
{
    if (!event->spontaneous()) {
        if ( d->dialog )
            XtUnmanageChild( d->dialog );
    }
    QDialog::hideEvent(event);
}

/*!
    Convenient Xt/Motif callback to accept the QtMotifDialog.

    The data is passed in \a client_data. The \a widget and \a ptr
    parameters are ignored.
*/
void QtMotifDialog::acceptCallback( Widget /* widget */, XtPointer client_data, XtPointer /* ptr */)
{
    QtMotifDialog *dialog = (QtMotifDialog *) client_data;
    dialog->accept();
}

/*!
    Convenient Xt/Motif callback to reject the QtMotifDialog.

    The data is passed in \a client_data. The \a widget and \a ptr
    parameters are ignored.
*/
void QtMotifDialog::rejectCallback( Widget /* widget */, XtPointer client_data, XtPointer /* ptr */)
{
    QtMotifDialog *dialog = (QtMotifDialog *) client_data;
    dialog->reject();
}

/*!
    \internal
    Wraps the Motif dialog by setting the X window for the
    QtMotifDialog to the X window id of the dialog shell.
*/
void QtMotifDialog::realize( Widget w )
{
    // use the winid of the dialog shell, reparent any children we have
    if ( XtWindow( w ) != winId() ) {
	XSync(QtMotif::display(), FALSE);

	XtSetMappedWhenManaged( d->shell, False );

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
        setWindowIconText(QString()); // make sure setWindowTitle() works below

	Window newid = XtWindow(w);
	QObjectList list = children();
	for (int i = 0; i < list.size(); ++i) {
	    QWidget *widget = qobject_cast<QWidget*>(list.at(i));
	    if (!widget || widget->isWindow()) continue;

	    XReparentWindow(widget->x11Info().display(), widget->winId(), newid,
			    widget->x(), widget->y());
	}
	QApplication::syncX();

	create( newid, true, true );

	// restore the window title and icon text
 	if (!wtitle.isEmpty())
 	    setWindowTitle(wtitle);
        if (!icontext.isEmpty())
            setWindowIconText(icontext);

	// if this dialog was created without a QWidget parent, then the transient
	// for will be set to the root window, which is not acceptable.
	// instead, set it to the window id of the shell's parent
	if ( ! parent() && XtParent( d->shell ) )
	    XSetTransientForHint(x11Info().display(), newid, XtWindow(XtParent(d->shell)));
    }
    QtMotif::registerWidget( this );
}

/*!
    \internal
    Sets the dialog widget for the QtMotifDialog to \a w.
*/
void QtMotifDialog::insertChild( Widget w )
{
    if ( d->dialog != NULL && d->dialog != w ) {
	qWarning( "QtMotifDialog::insertChild: cannot insert widget since one child\n"
		  "                           has been inserted already." );
	return;
    }

    d->dialog = w;

    XtSetMappedWhenManaged( d->shell, True );
}

/*!
    \internal
    Resets the dialog widget for the QtMotifDialog if the current
    dialog widget matches \a w.
*/
void QtMotifDialog::deleteChild(Widget w)
{
    if (!d->dialog) {
	qWarning("QtMotifDialog::deleteChild: cannot delete widget since no child\n"
		 "                           was inserted.");
	return;
    }

    if (d->dialog != w) {
	qWarning("QtMotifDialog::deleteChild: cannot delete widget different from\n"
		 "                           inserted child");
	return;
    }

    d->dialog = NULL;
}

/*!
    \internal
    Motif callback to resolve a QtMotifDialog and call
    QtMotifDialog::realize().
*/
void qmotif_dialog_realize( Widget w, XtValueMask *mask, XSetWindowAttributes *attr )
{
    XtRealizeProc realize = xmDialogShellClassRec.core_class.realize;
    (*realize)( w, mask, attr );

    QtMotifDialog *dialog = ( (QtMotifDialogWidget) w )->qmotifdialog.dialog;
    dialog->realize( w );
}

/*!
    \internal
    Motif callback to forward a QtMotifDialog and call
    QtMotifDialog::insertChild().
*/
void qmotif_dialog_insert_child( Widget w )
{
    if ( XtIsShell( w ) ) return; // do not allow shell children

    XtWidgetProc insert_child = xmDialogShellClassRec.composite_class.insert_child;
    (*insert_child)( w );

    QtMotifDialog *dialog = ( (QtMotifDialogWidget) w->core.parent )->qmotifdialog.dialog;
    dialog->insertChild( w );
}

/*!
    \internal
    Motif callback to resolve a QtMotifDialog and call
    QtMotifDialog::deleteChild().
*/
void qmotif_dialog_delete_child( Widget w )
{
    if ( XtIsShell( w ) ) return; // do not allow shell children

    XtWidgetProc delete_child = xmDialogShellClassRec.composite_class.delete_child;
    (*delete_child)( w );

    QtMotifDialog *dialog = ( (QtMotifDialogWidget) w->core.parent )->qmotifdialog.dialog;
    dialog->deleteChild( w );
}

/*!
    \internal
    Motif callback to resolve a QtMotifDialog and set the initial
    geometry of the dialog.
*/
void qmotif_dialog_change_managed( Widget w )
{
    QtMotifDialog *dialog = ( (QtMotifDialogWidget) w )->qmotifdialog.dialog;

    Widget p = w->core.parent;

    TopLevelShellRec shell;

    if ( p == NULL && dialog->parentWidget() ) {
	// fake a motif widget parent for proper dialog
	// sizing/placement, which happens during change_managed by
	// going through geometry_manager

	QWidget *qwidget = dialog->parentWidget();
	QRect geom = qwidget->geometry();

	memset( &shell, 0, sizeof( shell ) );

	char fakename[] = "fakename";

	shell.core.self = (Widget) &shell;
	shell.core.widget_class = (WidgetClass) &topLevelShellClassRec;
	shell.core.parent = NULL;
	shell.core.xrm_name = w->core.xrm_name;
	shell.core.being_destroyed = False;
	shell.core.destroy_callbacks = NULL;
	shell.core.constraints = NULL;
	shell.core.x = geom.x();
	shell.core.y = geom.y();
	shell.core.width = geom.width();
	shell.core.height = geom.height();
	shell.core.border_width = 0;
	shell.core.managed = True;
	shell.core.sensitive = True;
	shell.core.ancestor_sensitive = True;
	shell.core.event_table = NULL;
	shell.core.accelerators = NULL;
	shell.core.border_pixel = 0;
	shell.core.border_pixmap = 0;
	shell.core.popup_list = NULL;
        shell.core.num_popups = 0;
	shell.core.name = fakename;
	shell.core.screen = ScreenOfDisplay( qwidget->x11Info().display(),
					     qwidget->x11Info().screen() );
	shell.core.colormap = qwidget->x11Info().colormap();
	shell.core.window = qwidget->winId();
	shell.core.depth = qwidget->x11Info().depth();
	shell.core.background_pixel = 0;
	shell.core.background_pixmap = None;
	shell.core.visible = True;
	shell.core.mapped_when_managed = True;

	w->core.parent = (Widget) &shell;
    }

    XtWidgetProc change_managed = xmDialogShellClassRec.composite_class.change_managed;
    (*change_managed)( w );
    w->core.parent = p;
}

XtGeometryResult qmotif_dialog_geometry_manger( Widget w,
						XtWidgetGeometry *req,
						XtWidgetGeometry *rep )
{
    XtGeometryHandler geometry_manager =
	xmDialogShellClassRec.composite_class.geometry_manager;
    XtGeometryResult result = (*geometry_manager)( w, req, rep );

    QtMotifDialog *dialog = ( (QtMotifDialogWidget) w->core.parent )->qmotifdialog.dialog;
    dialog->setGeometry( XtX( w ), XtY( w ), XtWidth( w ), XtHeight( w ) );

    return result;
}


/*!\reimp
 */
bool QtMotifDialog::event( QEvent* e )
{
    if ( QtMotifWidget::dispatchQEvent( e, this ) )
	return true;
    return QDialog::event( e );
}
