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

// #define GRAB_DEBUG
#ifdef GRAB_DEBUG
#  define GDEBUG qDebug
#else
#  define GDEBUG if(false)qDebug
#endif

// #define EVENT_DEBUG
#ifdef EVENT_DEBUG
#  define EDEBUG qDebug
#else
#  define EDEBUG if(false)qDebug
#endif

#include <QtGui/QApplication>
#include <QtGui/QShowEvent>
#include <QtGui/QHideEvent>
#include <QtCore/QHash>
#include <QtCore/QSocketNotifier>
#include <QtGui/QWidget>
#include <QtCore/QVector>

#include "QtMotif.h"

#include <QtGui/QX11Info>

#include <unistd.h>
#include <stdlib.h>

// resolve the conflict between X11's FocusIn and QEvent::FocusIn
const int XFocusOut = FocusOut;
const int XFocusIn = FocusIn;
#undef FocusOut
#undef FocusIn

const int XKeyPress = KeyPress;
const int XKeyRelease = KeyRelease;
#undef KeyPress
#undef KeyRelease

struct QtMotifTimerInfo {
    int timerId;
    int interval;
    QObject *object;
};
typedef QHash<XtIntervalId, QtMotifTimerInfo> TimerHash;
typedef QHash<XtInputId, QSocketNotifier *> SocketNotifierHash;

Boolean qmotif_event_dispatcher(XEvent *event);

class QtMotifPrivate
{
public:
    QtMotifPrivate();
    ~QtMotifPrivate();

    void hookMeUp();
    void unhook();

    Display *display;
    XtAppContext appContext, ownContext;

    typedef QVector<XtEventDispatchProc> DispatcherArray;
    QHash<Display*,DispatcherArray> dispatchers;
    QWidgetMapper mapper;

    TimerHash timerHash;
    SocketNotifierHash socketNotifierHash;

    bool interrupt;

    int wakeUpPipe[2];

    // arguments for Xt display initialization
    const char* applicationClass;
    XrmOptionDescRec* options;
    int numOptions;
};

static QtMotifPrivate *static_d = 0;
static XEvent* last_xevent = 0;

QtMotifPrivate::QtMotifPrivate()
    : appContext(NULL), ownContext(NULL), interrupt(false),
      applicationClass(0), options(0), numOptions(0)
{
    if (static_d)
	qWarning("QtMotif: should only have one QtMotif instance!");
    static_d = this;
}

QtMotifPrivate::~QtMotifPrivate()
{
    if (static_d == this)
        static_d = 0;
}

void QtMotifPrivate::hookMeUp()
{
    // worker to plug Qt into Xt (event dispatchers)
    // and Xt into Qt (QtMotifEventLoop)

    // ### TODO extensions?
    DispatcherArray &qt_dispatchers = dispatchers[QX11Info::display()];
    DispatcherArray &qm_dispatchers = dispatchers[display];

    qt_dispatchers.resize(LASTEvent);
    qt_dispatchers.fill(0);

    qm_dispatchers.resize(LASTEvent);
    qm_dispatchers.fill(0);

    int et;
    for (et = 2; et < LASTEvent; et++) {
	qt_dispatchers[et] =
	    XtSetEventDispatcher(QX11Info::display(), et, ::qmotif_event_dispatcher);
	qm_dispatchers[et] =
	    XtSetEventDispatcher(display, et, ::qmotif_event_dispatcher);
    }
}

void QtMotifPrivate::unhook()
{
    // unhook Qt from Xt (event dispatchers)
    // unhook Xt from Qt? (QtMotifEventLoop)

    // ### TODO extensions?
    DispatcherArray &qt_dispatchers = dispatchers[QX11Info::display()];
    DispatcherArray &qm_dispatchers = dispatchers[display];

    for (int et = 2; et < LASTEvent; ++et) {
	(void) XtSetEventDispatcher(QX11Info::display(), et, qt_dispatchers[et]);
	(void) XtSetEventDispatcher(display, et, qm_dispatchers[et]);
    }

    /*
      We cannot destroy the app context here because it closes the X
      display, something QApplication does as well a bit later.
      if (ownContext)
          XtDestroyApplicationContext(ownContext);
     */
    appContext = ownContext = 0;
}

extern bool qt_try_modal(QWidget *, XEvent *); // defined in qapplication_x11.cpp

static bool xt_grab = false;
static Window xt_grab_focus_window = None;
static Display *xt_grab_display = 0;

Boolean qmotif_event_dispatcher(XEvent *event)
{
    if (xt_grab && event->xany.display == xt_grab_display) {
	if (event->type == XFocusIn && event->xfocus.mode == NotifyWhileGrabbed) {
	    GDEBUG("Xt: grab moved to window 0x%lx (detail %d)",
		   event->xany.window, event->xfocus.detail);
	    xt_grab_focus_window = event->xany.window;
	} else {
	    if (event->type == XFocusOut && (event->xfocus.mode == NotifyUngrab || event->xany.window == xt_grab_focus_window)) {
		GDEBUG("Xt: grab ended for 0x%lx (detail %d)",
		       event->xany.window, event->xfocus.detail);
		xt_grab = false;
		xt_grab_focus_window = None;
                xt_grab_display = 0;
	    } else if (event->type == DestroyNotify
		       && event->xany.window == xt_grab_focus_window) {
		GDEBUG("Xt: grab window destroyed (0x%lx)", xt_grab_focus_window);
		xt_grab = false;
		xt_grab_focus_window = None;
                xt_grab_display = 0;
	    }
	}
    }

    QWidgetMapper *mapper = &static_d->mapper;
    QWidgetMapper::Iterator it = mapper->find(event->xany.window);
    QWidget *widget = it == mapper->end() ? 0 : *it;
    if (!widget && QWidget::find(event->xany.window) == 0) {
	if (!xt_grab
            && (event->type == XFocusIn
                && (event->xfocus.mode == NotifyGrab
                    || event->xfocus.mode == NotifyWhileGrabbed))
            && (event->xfocus.detail != NotifyPointer
                && event->xfocus.detail != NotifyPointerRoot)) {
            GDEBUG("Xt: grab started for 0x%lx (detail %d)",
                   event->xany.window, event->xfocus.detail);
	    xt_grab = true;
	    xt_grab_focus_window = event->xany.window;
            xt_grab_display = event->xany.display;
	}

	// event is not for Qt, try Xt
	Widget w = XtWindowToWidget(QtMotif::display(), event->xany.window);

	while (w && (it = mapper->find(XtWindow(w))) == mapper->end()) {
	    if (XtIsShell(w)) {
		break;
	    }
	    w = XtParent(w);
	}
	widget = it != mapper->end() ? *it : 0;

 	if (widget && (event->type == XKeyPress ||
                       event->type == XKeyRelease))  {
	    // remap key events to keep accelerators working
 	    event->xany.window = widget->winId();
            if (event->xany.display != QX11Info::display())
                qApp->x11ProcessEvent(event);
 	}
    }

    bool delivered = false;
    if (event->xany.display == QX11Info::display()) {
	/*
	  If the mouse has been grabbed for a window that we don't know
	  about, we shouldn't deliver any pointer events, since this will
	  intercept the event that ends the mouse grab that Xt/Motif
	  started.
	*/
	bool do_deliver = true;
	if (xt_grab && (event->type == ButtonPress   ||
                        event->type == ButtonRelease ||
                        event->type == MotionNotify  ||
                        event->type == EnterNotify   ||
                        event->type == LeaveNotify))
	    do_deliver = false;

	last_xevent = event;
	delivered = do_deliver && (qApp->x11ProcessEvent(event) != -1);
	last_xevent = 0;
	if (widget) {
	    switch (event->type) {
	    case EnterNotify:
	    case LeaveNotify:
		event->xcrossing.focus = False;
		delivered = false;
		break;
	    case XKeyPress:
	    case XKeyRelease:
		delivered = true;
		break;
	    case XFocusIn:
	    case XFocusOut:
		delivered = false;
		break;
	    default:
		delivered = false;
		break;
	    }
	}
    }

    if (delivered) return True;

    // discard user input events when we have an active popup widget
    if (QApplication::activePopupWidget()) {
	switch (event->type) {
	case ButtonPress:			// disallow mouse/key events
	case ButtonRelease:
	case MotionNotify:
	case XKeyPress:
	case XKeyRelease:
	case EnterNotify:
	case LeaveNotify:
	case ClientMessage:
	    EDEBUG("Qt: active popup discarded event, type %d", event->type);
	    return True;

	default:
	    break;
	}
    }

    if (! xt_grab && QApplication::activeModalWidget()) {
	if (widget) {
	    // send event through Qt modality handling...
	    if (!qt_try_modal(widget, event)) {
		EDEBUG("Qt: active modal widget discarded event, type %d", event->type);
		return True;
	    }
	} else {
	    // we could have a pure Xt shell as a child of the active modal widget
	    Widget xw = XtWindowToWidget(QtMotif::display(), event->xany.window);
	    while (xw && (it = mapper->find(XtWindow(xw))) == mapper->end())
		xw = XtParent(xw);

	    QWidget *qw = it != mapper->end() ? *it : 0;
	    while (qw && qw != QApplication::activeModalWidget())
		qw = qw->parentWidget();

	    if (!qw) {
		// event is destined for an Xt widget, but since Qt has an
		// active modal widget, we stop here...
		switch (event->type) {
		case ButtonPress:			// disallow mouse/key events
		case ButtonRelease:
		case MotionNotify:
		case XKeyPress:
		case XKeyRelease:
		case EnterNotify:
		case LeaveNotify:
		case ClientMessage:
		    EDEBUG("Qt: active modal widget discarded event, type %d", event->type);
		    return True;
		default:
		    break;
		}
	    }
	}
    }

    // make click-to-focus work with QtMotifWidget children
    if (!xt_grab && event->type == ButtonPress) {
	QWidget *qw = 0;
	Widget xw = XtWindowToWidget(QtMotif::display(), event->xany.window);
	while (xw && (it = mapper->find(XtWindow(xw))) != mapper->end()) {
	    qw = *it;
	    xw = XtParent(xw);
	}

	if (qw && !qw->hasFocus() && (qw->focusPolicy() & Qt::ClickFocus))
	    qw->setFocus();
    }

    if (event->xany.type == ClientMessage)
        if (qApp->x11ProcessEvent(event) != -1)
            return True;

    Q_ASSERT(static_d->dispatchers.find(event->xany.display) != static_d->dispatchers.end());
    return static_d->dispatchers[event->xany.display][event->type](event);
}



/*!
    \class QtMotif
    \brief The QtMotif class provides the basis of the Motif Extension.

    QtMotif only provides a few public functions, but it is at the
    heart of the integration. QtMotif is responsible for initializing
    the Xt toolkit and the Xt application context. It does not open a
    connection to the X server; that is done by QApplication.

    The only member function in QtMotif that depends on an X server
    connection is QtMotif::initialize(). QtMotif must be created before
    QApplication.

    Example usage of QtMotif and QApplication:
    \code
    static char *resources[] = {
    ...
    };

    int main(int argc, char **argv)
    {
         QtMotif integrator("AppClass");
         XtAppSetFallbackResources(integrator.applicationContext(),
                                   resources);
         QApplication app(argc, argv);

         ...

         return app.exec();
    }
    \endcode

    Note for Qt versions 4.4 and later: There are cases where
    QtMotif's event handling will not function correctly in
    combination with Qt's non-native widget functionality. Hence,
    QtMotif by default disables this functionality. To reenable it,
    call \c {QApplication::setAttribute(Qt::AA_NativeWindows, false)}
    after creating the QtMotif object.
*/

/*!
    Constructs a QtMotif object to allow integration between Qt and
    Xt/Motif.

    If \a context is 0, QtMotif creates a default application context
    itself. The context is accessible through applicationContext().

    All arguments passed to this function (\a applicationClass, \a
    options and \a numOptions) are used to call XtDisplayInitialize()
    after QApplication has been constructed.
*/
QtMotif::QtMotif(const char *applicationClass, XtAppContext context,
		XrmOptionDescRec *options , int numOptions)
{
    d = new QtMotifPrivate;
    XtToolkitInitialize();
    if (context)
	d->appContext = context;
    else
	d->ownContext = d->appContext = XtCreateApplicationContext();
    d->applicationClass = applicationClass;
    d->options = options;
    d->numOptions = numOptions;
    d->wakeUpPipe[0] = d->wakeUpPipe[1] = 0;
#if QT_VERSION >= 0x040400
    QApplication::setAttribute(Qt::AA_NativeWindows);
#endif
}


/*!
    Destroys the QtMotif object.
*/
QtMotif::~QtMotif()
{
    delete d;
}

/*!
    Returns the application context.
*/
XtAppContext QtMotif::applicationContext() const
{
    return d->appContext;
}

/*!
    Returns the X11 display connection used by the Qt Motif Extension.
*/
Display *QtMotif::display()
{
    return static_d->display;
}

/*!\internal
 */
XEvent* QtMotif::lastEvent()
{
    return last_xevent;
}

/*!\internal
 */
void QtMotif::registerWidget(QWidget* w)
{
    if (!static_d)
	return;
    static_d->mapper.insert(w->winId(), w);
}

/*!\internal
 */
void QtMotif::unregisterWidget(QWidget* w)
{
    if (!static_d)
	return;
    static_d->mapper.remove(w->winId());
}

/*! \internal
  Redeliver the given XEvent to Xt.

  Rationale: An XEvent handled by Qt does not go through the Xt event
  handlers, and the internal state of Xt/Motif widgets will not be
  updated. This function should only be used if an event delivered by
  Qt to a QWidget needs to be sent to an Xt/Motif widget.
*/
bool QtMotif::redeliverEvent(XEvent *event)
{
    // redeliver the event to Xt, NOT through Qt
    return (static_d->dispatchers[event->xany.display][event->type](event));
}

/*! \reimp
 */
bool QtMotif::processEvents(QEventLoop::ProcessEventsFlags flags)
{
    emit awake();

    // Qt uses posted events to do lots of delayed operations, like
    // repaints... these need to be delivered before we go to sleep
    QApplication::sendPostedEvents(0,-1); // -1 means all events incl. deffered delete

    const bool canWait = (!d->interrupt && (flags & QEventLoop::WaitForMoreEvents));

    XtInputMask allowedMask = XtIMAll;
    if (flags & QEventLoop::ExcludeSocketNotifiers)
        allowedMask &= ~XtIMAlternateInput;
    if (flags & QEventLoop::X11ExcludeTimers)
        allowedMask &= ~XtIMTimer;

    XtInputMask pendingMask = XtAppPending(d->appContext);

    do {
    // get the pending event mask from Xt and process the next event
    pendingMask &= allowedMask;
    if (pendingMask & XtIMTimer) {
	pendingMask &= ~XtIMTimer;
	// zero timers will starve the Xt X event dispatcher... so
	// process something *instead* of a timer first...
	if (pendingMask != 0)
	    XtAppProcessEvent(d->appContext, pendingMask);
	// and process a timer afterwards
	pendingMask = XtIMTimer;
    }

    if (canWait) {
        emit aboutToBlock();
	XtAppProcessEvent(d->appContext, allowedMask);
    } else if (pendingMask != 0) {
        XtAppProcessEvent(d->appContext, pendingMask);
    }

    QApplication::sendPostedEvents(0,-1);

    } while ((pendingMask = XtAppPending(d->appContext)) && !d->interrupt && !canWait);

    d->interrupt = false;
    return true;
}

/*! \reimp
 */
bool QtMotif::hasPendingEvents()
{
    extern Q_CORE_EXPORT uint qGlobalPostedEventsCount();
    return (XtAppPending(d->appContext) != 0) || (qGlobalPostedEventsCount() != 0);
}

/*! \internal
 */
void qmotif_socket_notifier_handler(XtPointer, int *, XtInputId *id)
{
    QSocketNotifier *notifier = static_d->socketNotifierHash.value(*id);
    if (!notifier)
        return; // shouldn't happen
    QEvent event(QEvent::SockAct);
    QApplication::sendEvent(notifier, &event);
}

/*! \reimp
 */
void QtMotif::registerSocketNotifier(QSocketNotifier *notifier)
{
    XtInputMask mask;
    switch (notifier->type()) {
    case QSocketNotifier::Read:
	mask = XtInputReadMask;
	break;
    case QSocketNotifier::Write:
	mask = XtInputWriteMask;
	break;
    case QSocketNotifier::Exception:
	mask = XtInputExceptMask;
	break;
    default:
	qWarning("QtMotifEventLoop: socket notifier has invalid type");
	return;
    }

    XtInputId id = XtAppAddInput(d->appContext, notifier->socket(), (XtPointer) mask,
				 qmotif_socket_notifier_handler, this);
    d->socketNotifierHash.insert(id, notifier);
}

/*! \reimp
 */
void QtMotif::unregisterSocketNotifier(QSocketNotifier *notifier)
{
    SocketNotifierHash::Iterator it = d->socketNotifierHash.begin();
    while (*it != notifier) ++it;
    if (*it != notifier) { // this shouldn't happen
	qWarning("QtMotifEventLoop: failed to unregister socket notifier");
	return;
    }

    XtRemoveInput(it.key());
    d->socketNotifierHash.erase(it);
}

/*! \internal
 */
void qmotif_timer_handler(XtPointer, XtIntervalId *id)
{
    if (!id)
        return; // shouldn't happen
    TimerHash::iterator it = static_d->timerHash.find(*id);
    if (it == static_d->timerHash.constEnd())
        return;
    QTimerEvent event(it->timerId);
    // Qt assumes that timers repeat per default
    XtIntervalId newId = XtAppAddTimeOut(static_d->appContext, it->interval, qmotif_timer_handler, 0);
    QtMotifTimerInfo newInfo = (*it);
    (void) static_d->timerHash.erase(it);
    static_d->timerHash.insert(newId, newInfo);
    QApplication::sendEvent(newInfo.object, &event);
}

/*! \reimp
 */
void QtMotif::registerTimer(int timerId, int interval, QObject *object)
{
    XtIntervalId id = XtAppAddTimeOut(d->appContext, interval, qmotif_timer_handler, 0);
    QtMotifTimerInfo timerInfo = { timerId, interval, object };
    d->timerHash.insert(id, timerInfo);
}

/*! \reimp
 */
bool QtMotif::unregisterTimer(int timerId)
{
    TimerHash::iterator it = d->timerHash.begin();
    const TimerHash::iterator end = d->timerHash.end();
    for (; it != end; ++it) {
        if (it.value().timerId == timerId) {
            XtRemoveTimeOut(it.key());
            (void) d->timerHash.erase(it);
            return true;
        }
    }
    return false;
}

/*! \reimp
 */
bool QtMotif::unregisterTimers(QObject *object)
{
    TimerHash::iterator it = d->timerHash.begin();
    const TimerHash::iterator end = d->timerHash.end();
    while (it != end) {
        if (it.value().object == object) {
            XtRemoveTimeOut(it.key());
            it = d->timerHash.erase(it);
        } else {
	    ++it;
	}
    }
    return true;
}

/*! \reimp
 */
QList<QtMotif::TimerInfo> QtMotif::registeredTimers(QObject *object) const
{
    QList<QtMotif::TimerInfo> list;
    TimerHash::const_iterator it = d->timerHash.constBegin();
    const TimerHash::const_iterator end = d->timerHash.constEnd();
    for (; it != end; ++it) {
        const QtMotifTimerInfo &timerInfo = it.value();
        if (timerInfo.object == object)
            list << TimerInfo(timerInfo.timerId, timerInfo.interval);
    }
    return list;
}

/*! \internal
 */
void qmotif_wakeup_handler(XtPointer, int *readPFD, XtInputId *)
{
    char buf[256];
    if (readPFD)
        read(*readPFD, buf, 256);
}

/*! \reimp
 */
void QtMotif::wakeUp()
{
    if (d->wakeUpPipe[1]) {
        // write to a pipe to wake up the Xt event loop
        write(d->wakeUpPipe[1], "w", 1);
    }
}

/*! \reimp
 */
void QtMotif::interrupt()
{
    d->interrupt = true;
    wakeUp();
}

/*! \reimp
 */
void QtMotif::flush()
{
    XFlush(d->display);
    XFlush(QX11Info::display());
}

/*! \internal
 */
void QtMotif::startingUp()
{
    /*
      QApplication could be using a Display from an outside source, so
      we should only initialize the display if the current application
      context does not contain the QApplication display
    */
    bool display_found = false;
    Display **displays;
    Cardinal x, count;
    XtGetDisplays(d->appContext, &displays, &count);
    for (x = 0; x < count && ! display_found; ++x) {
	if (displays[x] == QX11Info::display())
	    display_found = true;
    }
    if (displays)
	XtFree((char *) displays);

    int argc;
    argc = qApp->argc();
    char **argv = new char*[argc];
    QByteArray appName = qApp->objectName().toLatin1();

    if (! display_found) {
    	argc = qApp->argc();
        for (int i = 0; i < argc; ++i)
            argv[i] = qApp->argv()[i];

	XtDisplayInitialize(d->appContext,
                            QX11Info::display(),
                            appName.constData(),
                            d->applicationClass,
                            d->options,
                            d->numOptions,
                            &argc,
                            argv);
    }

    // open a second connection to the X server... QtMotifWidget and
    // QtMotifDialog will use this connection to create their wrapper
    // shells, which will allow for Motif<->Qt clipboard operations
    d->display = XOpenDisplay(DisplayString(QX11Info::display()));
    if (!d->display) {
	qWarning("%s: (QtMotif) cannot create second connection to X server '%s'",
		 qApp->argv()[0], DisplayString(QX11Info::display()));
	::exit(1);
    }

    argc = qApp->argc();
    for (int i = 0; i < argc; ++i)
        argv[i] = qApp->argv()[i];

    XtDisplayInitialize(d->appContext,
			d->display,
			appName.constData(),
			d->applicationClass,
			d->options,
			d->numOptions,
			&argc,
                        argv);
    XSync(d->display, False);

    delete [] argv;

    // setup event dispatchers
    d->hookMeUp();

    if (pipe(d->wakeUpPipe))
        qErrnoWarning("QtMotif: Failed to create pipe.");
    XtAppAddInput(d->appContext, d->wakeUpPipe[0], (XtPointer)XtInputReadMask,
                  qmotif_wakeup_handler, 0);

}

/*! \internal
 */
void QtMotif::closingDown()
{
    d->unhook();
    XtCloseDisplay(d->display);
    d->display = 0;
}
