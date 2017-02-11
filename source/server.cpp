/*******************************************************************************
*                                                                              *
* server.c -- Nirvana Editor edit-server component                             *
*                                                                              *
* Copyright (C) 1999 Mark Edel                                                 *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute version of this program linked to   *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License        *
* for more details.                                                            *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* November, 1995                                                               *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/

#include <QApplication>
#include <QX11Info>
#include "ui/DocumentWidget.h"
#include "ui/MainWindow.h"
#include "server.h"
#include "TextBuffer.h"
#include "nedit.h"
#include "file.h"
#include "selection.h"
#include "macro.h"
#include "preferences.h"
#include "server_common.h"
#include "util/fileUtils.h"
#include "util/utils.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>

#include <Xm/Xm.h>

#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/param.h>
#include <unistd.h>
#include <pwd.h>

static void processServerCommand();
static void cleanUpServerCommunication();
static void processServerCommandString(char *string);
static void getFileClosedPropertyEx(DocumentWidget *document);
static bool isLocatedOnDesktopEx(MainWindow *window, long currentDesktop);
static MainWindow *findWindowOnDesktopEx(int tabbed, long currentDesktop);

static Atom ServerRequestAtom = 0;
static Atom ServerExistsAtom = 0;

/*
** Set up inter-client communication for NEdit server end, expected to be
** called only once at startup time
*/
void InitServerCommunication() {
    Window rootWindow = RootWindow(QX11Info::display(), DefaultScreen(QX11Info::display()));

	// Create the server property atoms on the current DISPLAY. 
    CreateServerPropertyAtoms(GetPrefServerName().toLatin1().data(), &ServerExistsAtom, &ServerRequestAtom);

	/* Pay attention to PropertyChangeNotify events on the root window.
	   Do this before putting up the server atoms, to avoid a race
	   condition (when nc sees that the server exists, it sends a command,
	   so we must make sure that we already monitor properties). */
    XSelectInput(QX11Info::display(), rootWindow, PropertyChangeMask);

	/* Create the server-exists property on the root window to tell clients
	   whether to try a request (otherwise clients would always have to
	   try and wait for their timeouts to expire) */
    XChangeProperty(QX11Info::display(), rootWindow, ServerExistsAtom, XA_STRING, 8, PropModeReplace, (uint8_t *)"True", 4);

	// Set up exit handler for cleaning up server-exists property 
	atexit(cleanUpServerCommunication);
}

static void deleteProperty(Atom *atom) {
	if (!IsServer) {
		*atom = None;
		return;
	}

	if (*atom != None) {
        XDeleteProperty(QX11Info::display(), RootWindow(QX11Info::display(), DefaultScreen(QX11Info::display())), *atom);
		*atom = None;
	}
}

/*
** Exit handler.  Removes server-exists property on root window
*/
static void cleanUpServerCommunication() {

	/* Delete any per-file properties that still exist
	 * (and that server knows about)
	 */
    for(DocumentWidget *document : DocumentWidget::allDocuments()) {
        DeleteFileClosedPropertyEx(document);
	}

	/* Delete any per-file properties that still exist
	 * (but that that server doesn't know about)
	 */
    DeleteServerFileAtoms(GetPrefServerName().toLatin1().data(), RootWindow(QX11Info::display(), DefaultScreen(QX11Info::display())));

	/* Delete the server-exists property from the root window (if it was
	   assigned) and don't let the process exit until the X server has
	   processed the delete request (otherwise it won't be done) */
	deleteProperty(&ServerExistsAtom);
    XSync(QX11Info::display(), False);
}

/*
** Special event loop for NEdit servers.  Processes PropertyNotify events on
** the root window (this would not be necessary if it were possible to
** register an Xt event-handler for a window, rather than only a widget).
** Invokes server routines when a server-request property appears,
** re-establishes server-exists property when another server exits and
** this server is still alive to take over.
*/
void ServerMainLoop(XtAppContext context) {
	while (true) {
		XEvent event;
		XtAppNextEvent(context, &event);
		ServerDispatchEvent(&event);
	}
}

static void processServerCommand() {
	Atom dummyAtom;
    unsigned long nItems;
    unsigned long dummyULong;
	uint8_t *propValue;
	int getFmt;

	// Get the value of the property, and delete it from the root window 
    if (XGetWindowProperty(QX11Info::display(), RootWindow(QX11Info::display(), DefaultScreen(QX11Info::display())), ServerRequestAtom, 0, INT_MAX, True, XA_STRING, &dummyAtom, &getFmt, &nItems, &dummyULong, &propValue) != Success || getFmt != 8)
		return;

	// Invoke the command line processor on the string to process the request 
	processServerCommandString((char *)propValue);
	XFree(propValue);
}

Boolean ServerDispatchEvent(XEvent *event) {
	if (IsServer) {
        Window rootWindow = RootWindow(QX11Info::display(), DefaultScreen(QX11Info::display()));
		if (event->xany.window == rootWindow && event->xany.type == PropertyNotify) {
			const XPropertyEvent *e = &event->xproperty;

			if (e->type == PropertyNotify && e->window == rootWindow) {
				if (e->atom == ServerRequestAtom && e->state == PropertyNewValue)
					processServerCommand();
				else if (e->atom == ServerExistsAtom && e->state == PropertyDelete)
                    XChangeProperty(QX11Info::display(), rootWindow, ServerExistsAtom, XA_STRING, 8, PropModeReplace, (uint8_t *)"True", 4);
			}
		}
	}
	return XtDispatchEvent(event);
}

// Try to find existing 'FileOpen' property atom for path. 
static Atom findFileOpenProperty(const char *filename, const char *pathname) {

	if (!IsServer) {
		return (None);
	}

    QString path = QString(QLatin1String("%1%2")).arg(QString::fromLatin1(pathname)).arg(QString::fromLatin1(filename));
	
    return CreateServerFileOpenAtom(GetPrefServerName().toLatin1().data(), path.toLatin1().data());
}

/* Destroy the 'FileOpen' atom to inform nc that this file has
** been opened.
*/
static void deleteFileOpenPropertyEx(DocumentWidget *document) {
    if (document->filenameSet_) {
        Atom atom = findFileOpenProperty(document->filename_.toLatin1().data(), document->path_.toLatin1().data());
		deleteProperty(&atom);
	}
}

static void deleteFileOpenProperty2(const char *filename, const char *pathname) {
	Atom atom = findFileOpenProperty(filename, pathname);
	deleteProperty(&atom);
}

// Try to find existing 'FileClosed' property atom for path. 
static Atom findFileClosedProperty(const char *filename, const char *pathname) {

	if (!IsServer) {
		return (None);
	}

    QString path = QString(QLatin1String("%1%2")).arg(QString::fromLatin1(pathname)).arg(QString::fromLatin1(filename));

    return CreateServerFileClosedAtom(GetPrefServerName().toLatin1().data(), path.toLatin1().data(), True); // don't create
}

// Get hold of the property to use when closing the file. 
static void getFileClosedPropertyEx(DocumentWidget *document) {
    if (document->filenameSet_) {
        document->fileClosedAtom_ = findFileClosedProperty(document->filename_.toLatin1().data(), document->path_.toLatin1().data());
    }
}

/* Delete the 'FileClosed' atom to inform nc that this file has
** been closed.
*/
void DeleteFileClosedPropertyEx(DocumentWidget *document) {
    if (document->filenameSet_) {
        deleteProperty(&document->fileClosedAtom_);
    }
}

static void deleteFileClosedProperty2(const char *filename, const char *pathname) {
	Atom atom = findFileClosedProperty(filename, pathname);
	deleteProperty(&atom);
}

static bool isLocatedOnDesktopEx(MainWindow *window, long currentDesktop) {
    // TODO(eteran): look into what this is actually doing and if it is even
    //               possible to do the equivalent using Qt
    Q_UNUSED(window);
    Q_UNUSED(currentDesktop);
    return true;
}

static MainWindow *findWindowOnDesktopEx(int tabbed, long currentDesktop) {

    if (tabbed == 0 || (tabbed == -1 && GetPrefOpenInTab() == 0)) {
        /* A new window is requested, unless we find an untitled unmodified
            document on the current desktop */
        for(DocumentWidget *document : DocumentWidget::allDocuments()) {
            if (document->filenameSet_ || document->fileChanged_ || document->macroCmdData_) {
                continue;
            }

            MainWindow *window = document->toWindow();

            // No check for top document here!
            if (isLocatedOnDesktopEx(window, currentDesktop)) {
                return window;
            }
        }
    } else {
        // Find a window on the current desktop to hold the new document
        for(MainWindow *window : MainWindow::allWindows()) {
            if (isLocatedOnDesktopEx(window, currentDesktop)) {
                return window;
            }
        }
    }

    return nullptr; // No window found on current desktop -> create new window
}

static void processServerCommandString(char *string) {
    char *fullname;
    char filename[MAXPATHLEN];
    char pathname[MAXPATHLEN];
    char *doCommand;
    char *geometry;
    char *langMode;
    char *inPtr;
    int stringLen = strlen(string);
    int lineNum;
    int createFlag;
    int readFlag;
    int iconicFlag;
    int lastIconic = 0;
    int tabbed = -1;
    int fileLen;
    int doLen;
    int lmLen;
    int geomLen;
    int charsRead;
    int itemsRead;
    QPointer<DocumentWidget> lastFileEx = nullptr;
    const long currentDesktop = -1;

	/* If the command string is empty, put up an empty, Untitled window
	   (or just pop one up if it already exists) */
	if (string[0] == '\0') {

        QList<DocumentWidget *> documents = DocumentWidget::allDocuments();

        auto it = std::find_if(documents.begin(), documents.end(), [currentDesktop](DocumentWidget *document) {
            return (!document->filenameSet_ && !document->fileChanged_ && isLocatedOnDesktopEx(document->toWindow(), currentDesktop));
		});

        if(it == documents.end()) {
            MainWindow::EditNewFileEx(findWindowOnDesktopEx(tabbed, currentDesktop), QString(), false, QString(), QString());
            MainWindow::CheckCloseDimEx();
		} else {
            (*it)->RaiseDocument();
		}
		return;
	}

	/*
	** Loop over all of the files in the command list
	*/
	inPtr = string;
	while (true) {

		if (*inPtr == '\0')
			break;

		/* Read a server command from the input string.  Header contains:
		   linenum createFlag fileLen doLen\n, followed by a filename and -do
		   command both followed by newlines.  This bit of code reads the
		   header, and converts the newlines following the filename and do
		   command to nulls to terminate the filename and doCommand strings */
		itemsRead = sscanf(inPtr, "%d %d %d %d %d %d %d %d %d%n", &lineNum, &readFlag, &createFlag, &iconicFlag, &tabbed, &fileLen, &doLen, &lmLen, &geomLen, &charsRead);
		if (itemsRead != 9)
			goto readError;
		inPtr += charsRead + 1;
		if (inPtr - string + fileLen > stringLen)
			goto readError;
		fullname = inPtr;
		inPtr += fileLen;
		*inPtr++ = '\0';
		if (inPtr - string + doLen > stringLen)
			goto readError;
		doCommand = inPtr;
		inPtr += doLen;
		*inPtr++ = '\0';
		if (inPtr - string + lmLen > stringLen)
			goto readError;
		langMode = inPtr;
		inPtr += lmLen;
		*inPtr++ = '\0';
		if (inPtr - string + geomLen > stringLen)
			goto readError;
		geometry = inPtr;
		inPtr += geomLen;
		*inPtr++ = '\0';

		/* An empty file name means:
		 *   put up an empty, Untitled window, or use an existing one
		 *   choose a random window for executing the -do macro upon
		 */
		if (fileLen <= 0) {
			
            QList<DocumentWidget *> documents = DocumentWidget::allDocuments();

            auto it = std::find_if(documents.begin(), documents.end(), [currentDesktop](DocumentWidget *w) {
                return (!w->filenameSet_ && !w->fileChanged_ && isLocatedOnDesktopEx(w->toWindow(), currentDesktop));
			});			
			
		
			if (*doCommand == '\0') {
                if(it == documents.end()) {
                    MainWindow::EditNewFileEx(
                                findWindowOnDesktopEx(tabbed, currentDesktop),
                                QString(),
                                iconicFlag,
                                lmLen == 0 ? QString() : QString::fromLatin1(langMode),
                                QString());
				} else {
					if (iconicFlag) {
						(*it)->RaiseDocument();
					} else {
						(*it)->RaiseDocumentWindow();
					}
				}
			} else {
				
				/* Starting a new command while another one is still running
				   in the same window is not possible (crashes). */
                auto win = std::find_if(documents.begin(), documents.end(), [](DocumentWidget *document) {
                    return document->macroCmdData_ == nullptr;
                });

                if (win == documents.end()) {
					QApplication::beep();
				} else {
					// Raise before -do (macro could close window). 
					if (iconicFlag) {
						(*win)->RaiseDocument();
					} else {
						(*win)->RaiseDocumentWindow();
					}
                    DoMacroEx(*win, QString::fromLatin1(doCommand), "-do macro");
				}
			}

            MainWindow::CheckCloseDimEx();
			return;
		}

		/* Process the filename by looking for the files in an
		   existing window, or opening if they don't exist */
        int editFlags = (readFlag ? PREF_READ_ONLY : 0) | CREATE | (createFlag ? SUPPRESS_CREATE_WARN : 0);
		if (ParseFilename(fullname, filename, pathname) != 0) {
			fprintf(stderr, "NEdit: invalid file name\n");
			deleteFileClosedProperty2(filename, pathname);
			break;
		}

        DocumentWidget *window = MainWindow::FindWindowWithFile(QString::fromLatin1(filename), QString::fromLatin1(pathname));
		if(!window) {
			/* Files are opened in background to improve opening speed
			   by defering certain time  consuiming task such as syntax
			   highlighting. At the end of the file-opening loop, the
			   last file opened will be raised to restore those deferred
			   items. The current file may also be raised if there're
			   macros to execute on. */
            window = DocumentWidget::EditExistingFileEx(
                        findWindowOnDesktopEx(tabbed, currentDesktop)->currentDocument(),
                        QString::fromLatin1(filename),
                        QString::fromLatin1(pathname),
                        editFlags,
                        geometry ? QString::fromLatin1(geometry) : QString(),
                        iconicFlag,
                        lmLen  ==  0 ? QString()          : QString::fromLatin1(langMode),
                        tabbed == -1 ? GetPrefOpenInTab() : tabbed,
                        true);

			if (window) {
                if (lastFileEx && window->toWindow() != lastFileEx->toWindow()) {
                    lastFileEx->RaiseDocument();
				}
			}
		}

		/* Do the actions requested (note DoMacro is last, since the do
		   command can do anything, including closing the window!) */
		if (window) {
            deleteFileOpenPropertyEx(window);
            getFileClosedPropertyEx(window);

            if (lineNum > 0) {
                // TODO(eteran): how was the TextArea previously determined?
                SelectNumberedLineEx(window, window->firstPane(), lineNum);
            }

			if (*doCommand != '\0') {
				window->RaiseDocument();

				/* Starting a new command while another one is still running
				   in the same window is not possible (crashes). */
				if (window->macroCmdData_) {
					QApplication::beep();
				} else {
                    DoMacroEx(window, QString::fromLatin1(doCommand), "-do macro");
				}
			}

			// register the last file opened for later use 
			if (window) {
                lastFileEx = window;
				lastIconic = iconicFlag;
			}
		} else {
			deleteFileOpenProperty2(filename, pathname);
			deleteFileClosedProperty2(filename, pathname);
		}
	}

	// Raise the last file opened 
    if (lastFileEx) {
		if (lastIconic) {
            lastFileEx->RaiseDocument();
		} else {
            lastFileEx->RaiseDocumentWindow();
		}
        MainWindow::CheckCloseDimEx();
	}
	return;

readError:
	fprintf(stderr, "NEdit: error processing server request\n");
	return;
}
