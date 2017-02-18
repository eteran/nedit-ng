
#include "NeditServer.h"
#include "DocumentWidget.h"
#include "MainWindow.h"
#include "file.h"
#include "macro.h"
#include "preferences.h"
#include "selection.h"
#include "server_common.h"
#include "util/fileUtils.h"
#include <QApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <sys/param.h>

namespace {

bool isLocatedOnDesktopEx(MainWindow *window, long currentDesktop) {
    // TODO(eteran): look into what this is actually doing and if it is even
    //               possible to do the equivalent using Qt
    Q_UNUSED(window);
    Q_UNUSED(currentDesktop);
    return true;
}

MainWindow *findWindowOnDesktopEx(int tabbed, long currentDesktop) {

    if (tabbed == 0 || (tabbed == -1 && !GetPrefOpenInTab())) {
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

    // No window found on current desktop -> create new window
    return nullptr;
}

}

/**
 * @brief NeditServer::NeditServer
 * @param parent
 */
NeditServer::NeditServer(QObject *parent) : QObject(parent) {

    if(!QDBusConnection::sessionBus().isConnected()) {
        qDebug("Cannot connecto to the D-Bus session bus");
        return;
    }

    if(!QDBusConnection::sessionBus().registerService(QLatin1String(SERVICE_NAME))) {
        qDebug("%s", qPrintable(QDBusConnection::sessionBus().lastError().message()));
        return;
    }

    QDBusConnection::sessionBus().registerObject(QLatin1String("/Server"), this, QDBusConnection::ExportAllSlots);
}

/**
 * @brief NeditServer::processCommand
 * @param command
 */
void NeditServer::processCommand(const QString &command) {

    QString fullname;
    char filename[MAXPATHLEN];
    char pathname[MAXPATHLEN];
    QString doCommand;
    QString geometry;
    QString langMode;
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
    if (command.isEmpty()) {

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

    QByteArray stringBytes = command.toLatin1();
    const char *string = stringBytes.data();


    const char *inPtr = string;
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
        if (inPtr - string + fileLen > command.size())
            goto readError;

        fullname = QString::fromLatin1(inPtr, fileLen);
        inPtr += fileLen;
        inPtr++;
        if (inPtr - string + doLen > command.size())
            goto readError;

        doCommand = QString::fromLatin1(inPtr, doLen);
        inPtr += doLen;
        inPtr++;
        if (inPtr - string + lmLen > command.size())
            goto readError;

        langMode = QString::fromLatin1(inPtr, lmLen);
        inPtr += lmLen;
        inPtr++;
        if (inPtr - string + geomLen > command.size())
            goto readError;

        geometry = QString::fromLatin1(inPtr, geomLen);
        inPtr += geomLen;
        inPtr++;

        /* An empty file name means:
         *   put up an empty, Untitled window, or use an existing one
         *   choose a random window for executing the -do macro upon
         */
        if (fileLen <= 0) {

            QList<DocumentWidget *> documents = DocumentWidget::allDocuments();

            auto it = std::find_if(documents.begin(), documents.end(), [currentDesktop](DocumentWidget *w) {
                return (!w->filenameSet_ && !w->fileChanged_ && isLocatedOnDesktopEx(w->toWindow(), currentDesktop));
            });


            if (doCommand.isEmpty()) {
                if(it == documents.end()) {

                    MainWindow::EditNewFileEx(
                                findWindowOnDesktopEx(tabbed, currentDesktop),
                                QString(),
                                iconicFlag,
                                langMode.isEmpty() ? QString() : langMode,
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
                    DoMacroEx(*win, doCommand, "-do macro");
                }
            }

            MainWindow::CheckCloseDimEx();
            return;
        }

        /* Process the filename by looking for the files in an
           existing window, or opening if they don't exist */
        int editFlags = (readFlag ? PREF_READ_ONLY : 0) | CREATE | (createFlag ? SUPPRESS_CREATE_WARN : 0);
        if (ParseFilename(fullname.toLatin1().data(), filename, pathname) != 0) {
            fprintf(stderr, "NEdit: invalid file name\n");
            break;
        }

        DocumentWidget *document = MainWindow::FindWindowWithFile(QString::fromLatin1(filename), QString::fromLatin1(pathname));
        if(!document) {
            /* Files are opened in background to improve opening speed
               by defering certain time  consuiming task such as syntax
               highlighting. At the end of the file-opening loop, the
               last file opened will be raised to restore those deferred
               items. The current file may also be raised if there're
               macros to execute on. */
            document = DocumentWidget::EditExistingFileEx(
                        findWindowOnDesktopEx(tabbed, currentDesktop)->currentDocument(),
                        QString::fromLatin1(filename),
                        QString::fromLatin1(pathname),
                        editFlags,
                        geometry,
                        iconicFlag,
                        langMode.isEmpty() ? QString() : langMode,
                        tabbed == -1 ? GetPrefOpenInTab() : tabbed,
                        true);

            if (document) {
                if (lastFileEx && document->toWindow() != lastFileEx->toWindow()) {
                    lastFileEx->RaiseDocument();
                }
            }
        }

        /* Do the actions requested (note DoMacro is last, since the do
           command can do anything, including closing the window!) */
        if (document) {

            if (lineNum > 0) {
                // TODO(eteran): how was the TextArea previously determined?
                SelectNumberedLineEx(document, document->firstPane(), lineNum);
            }

            if (!doCommand.isEmpty()) {
                document->RaiseDocument();

                /* Starting a new command while another one is still running
                   in the same window is not possible (crashes). */
                if (document->macroCmdData_) {
                    QApplication::beep();
                } else {
                    DoMacroEx(document, doCommand, "-do macro");
                }
            }

            // register the last file opened for later use
            if (document) {
                lastFileEx = document;
                lastIconic = iconicFlag;
            }
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
