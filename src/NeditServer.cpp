
#include "NeditServer.h"
#include "DocumentWidget.h"
#include "EditFlags.h"
#include "MainWindow.h"
#include "Preferences.h"
#include "Util/ServerCommon.h"
#include "Util/FileSystem.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QLocalServer>
#include <QLocalSocket>
#include <QDataStream>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>

#include <memory>

namespace {

bool isLocatedOnDesktopEx(MainWindow *window, long currentDesktop) {
	return QApplication::desktop()->screenNumber(window) == currentDesktop;
}

MainWindow *findWindowOnDesktopEx(int tabbed, long currentDesktop) {

	if (tabbed == 0 || (tabbed == -1 && !Preferences::GetPrefOpenInTab())) {
		/* A new window is requested, unless we find an untitled unmodified
			document on the current desktop */
		for(DocumentWidget *document : DocumentWidget::allDocuments()) {
			if (document->filenameSet_ || document->fileChanged_ || document->macroCmdData_) {
				continue;
			}

			auto window = MainWindow::fromDocument(document);

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

	QString socketName = LocalSocketName(Preferences::GetPrefServerName());
	server_ = new QLocalServer(this);
	server_->setSocketOptions(QLocalServer::UserAccessOption);
	connect(server_, &QLocalServer::newConnection, this, &NeditServer::newConnection);

	QLocalServer::removeServer(socketName);

	if(!server_->listen(socketName)) {
		qWarning() << "NEdit: server failed to start: " << server_->errorString();
	}
}

/**
 * @brief NeditServer::newConnection
 */
void NeditServer::newConnection() {

	std::shared_ptr<QLocalSocket> socket(server_->nextPendingConnection());

	while (socket->bytesAvailable() < static_cast<int>(sizeof(qint32))) {
		socket->waitForReadyRead();
	}

	QDataStream stream(socket.get());
	stream.setVersion(QDataStream::Qt_5_0);

	QByteArray jsonString;
	stream >> jsonString;

	auto jsonDocument = QJsonDocument::fromJson(jsonString);
	if (!jsonDocument.isArray()) {
		qWarning("NEdit: error processing server request");
		return;
	}

	int lastIconic = 0;

	QPointer<DocumentWidget> lastFile;
	const long currentDesktop = QApplication::desktop()->screenNumber(QApplication::activeWindow());

	auto array = jsonDocument.array();
	/* If the command string is empty, put up an empty, Untitled window
	   (or just pop one up if it already exists) */
	if (array.isEmpty()) {
		std::vector<DocumentWidget *> documents = DocumentWidget::allDocuments();

		auto it = std::find_if(documents.begin(), documents.end(), [currentDesktop](DocumentWidget *document) {
				return (!document->filenameSet_ && !document->fileChanged_ && isLocatedOnDesktopEx(MainWindow::fromDocument(document), currentDesktop));
		});

		if (it == documents.end()) {

			const int tabbed = -1;

			MainWindow::EditNewFile(
						findWindowOnDesktopEx(tabbed, currentDesktop),
						QString(),
						false,
						QString(),
						QString());

			MainWindow::CheckCloseEnableState();
		} else {
			(*it)->raiseDocument();
		}
		return;
	}

	for (auto entry : array) {

		if (!entry.isObject()) {
			qWarning("NEdit: error processing server request");
			break;
		}

		auto file = entry.toObject();

		const bool wait          = file[QLatin1String("wait")].toBool();
		const int lineNum        = file[QLatin1String("line_number")].toInt();
		const int readFlag       = file[QLatin1String("read")].toInt();
		const int createFlag     = file[QLatin1String("create")].toInt();
		const int iconicFlag     = file[QLatin1String("iconic")].toInt();
		const int tabbed         = file[QLatin1String("is_tabbed")].toInt();
		const QString fullname   = file[QLatin1String("path")].toString();
		const QString doCommand  = file[QLatin1String("toDoCommand")].toString();
		const QString langMode   = file[QLatin1String("langMode")].toString();
		const QString geometry   = file[QLatin1String("geometry")].toString();

		/* An empty file name means:
		 *   put up an empty, Untitled window, or use an existing one
		 *   choose a random window for executing the -do macro upon
		 */
		if (fullname.isEmpty()) {

			std::vector<DocumentWidget *> documents = DocumentWidget::allDocuments();

			auto it = std::find_if(documents.begin(), documents.end(), [currentDesktop](DocumentWidget *w) {
				return (!w->filenameSet_ && !w->fileChanged_ && isLocatedOnDesktopEx(MainWindow::fromDocument(w), currentDesktop));
			});

			if (doCommand.isEmpty()) {
				if (it == documents.end()) {

					MainWindow::EditNewFile(
								findWindowOnDesktopEx(tabbed, currentDesktop),
								QString(),
								iconicFlag,
								langMode.isEmpty() ? QString() : langMode,
								QString());
				} else {
					if (iconicFlag) {
						(*it)->raiseDocument();
					} else {
						(*it)->raiseDocumentWindow();
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
						(*win)->raiseDocument();
					} else {
						(*win)->raiseDocumentWindow();
					}
					(*win)->DoMacro(doCommand, QLatin1String("-do macro"));
				}
			}

			MainWindow::CheckCloseEnableState();
			return;
		}

		/* Process the filename by looking for the files in an
		   existing window, or opening if they don't exist */
		const int editFlags =
				(readFlag ? EditFlags::PREF_READ_ONLY : 0) |
				EditFlags::CREATE |
				(createFlag ? EditFlags::SUPPRESS_CREATE_WARN : 0);

		QString filename;
		QString pathname;
		if (!parseFilename(fullname, &filename, &pathname) != 0) {
			qWarning("NEdit: invalid file name");
			break;
		}

		DocumentWidget *document = MainWindow::FindWindowWithFile(filename, pathname);
		if (!document) {
			/* Files are opened in background to improve opening speed
			   by defering certain time  consuiming task such as syntax
			   highlighting. At the end of the file-opening loop, the
			   last file opened will be raised to restore those deferred
			   items. The current file may also be raised if there're
			   macros to execute on. */

			MainWindow *window = findWindowOnDesktopEx(tabbed, currentDesktop);

			document = DocumentWidget::EditExistingFileEx(
			               window ? window->currentDocument() : nullptr,
			               filename,
			               pathname,
			               editFlags,
			               geometry,
			               iconicFlag,
			               langMode.isEmpty() ? QString() : langMode,
			               tabbed == -1 ? Preferences::GetPrefOpenInTab() : tabbed,
			               /*bgOpen=*/true);

			if (document) {
				if (lastFile && MainWindow::fromDocument(document) != MainWindow::fromDocument(lastFile)) {
					lastFile->raiseDocument();
				}
			}
		}

		/* Do the actions requested (note DoMacro is last, since the do
		   command can do anything, including closing the window!) */
		if (document) {

			if (lineNum > 0) {
				// NOTE(eteran): this was previously window->lastFocus, but that
				// is very inconvinient to get at this point in the code (now)
				// firstPane() seems practical for now
				document->SelectNumberedLineEx(document->firstPane(), lineNum);
			}

			if (!doCommand.isEmpty()) {
				document->raiseDocument();

				/* Starting a new command while another one is still running
				   in the same window is not possible (crashes). */
				if (document->macroCmdData_) {
					QApplication::beep();
				} else {
					document->DoMacro(doCommand, QLatin1String("-do macro"));
				}
			}

			// register the last file opened for later use
			if (document) {
				lastFile = document;
				lastIconic = iconicFlag;
			}

			if(wait) {
				// by creating this lambda, we are incrmenting the reference
				// count of the socket, so it won't be destroyed until all open
				// documents are closed.
				// We create the dummy QObject in order to manage the lifetime
				// of the connection, which matters in the case of the last
				// document being "closed" and instead of being destroyed,
				// becomes an untitled window
				auto obj = new QObject(this);
				connect(document, &DocumentWidget::documentClosed, obj, [socket, obj]() {
					obj->deleteLater();
				});
			}
		}
	}

	// Raise the last file opened
	if (lastFile) {
		if (lastIconic) {
			lastFile->raiseDocument();
		} else {
			lastFile->raiseDocumentWindow();
		}
		MainWindow::CheckCloseEnableState();
	}
}
