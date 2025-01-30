
#include "NeditServer.h"
#include "DocumentWidget.h"
#include "EditFlags.h"
#include "MainWindow.h"
#include "Preferences.h"
#include "Util/FileSystem.h"
#include "Util/ServerCommon.h"

#include <QApplication>
#include <QDataStream>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QScreen>
#include <QVarLengthArray>

#include <memory>

namespace {

QScreen *screenAt(QPoint pos) {
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
	QVarLengthArray<const QScreen *, 8> visitedScreens;
	for (const QScreen *screen : QGuiApplication::screens()) {
		if (visitedScreens.contains(screen)) {
			continue;
		}

		// The virtual siblings include the screen itself, so iterate directly
		for (QScreen *sibling : screen->virtualSiblings()) {
			if (sibling->geometry().contains(pos)) {
				return sibling;
			}

			visitedScreens.append(sibling);
		}
	}

	return nullptr;
#else
	return QApplication::screenAt(pos);
#endif
}

/**
 * @brief isLocatedOnDesktop
 * @param widget
 * @param currentDesktop
 * @return
 */
bool isLocatedOnDesktop(QWidget *widget, QScreen *currentDesktop) {
	if (!currentDesktop) {
		return true;
	}

	return screenAt(widget->pos()) == currentDesktop;
}

/**
 * @brief documentForTargetScreen
 * @param currentDesktop
 * @return
 */
DocumentWidget *documentForTargetScreen(QScreen *currentDesktop) {
	const std::vector<MainWindow *> windows = MainWindow::allWindows();

	// Find a window on the current desktop to hold the new document
	for (MainWindow *window : windows) {

		if (isLocatedOnDesktop(window, currentDesktop)) {
			return window->currentDocument();
		}
	}

	return nullptr;
}

/**
 * @brief findDocumentOnDesktop
 * @param tabbed
 * @param currentDesktop
 * @return
 */
DocumentWidget *findDocumentOnDesktop(int tabbed, QScreen *currentDesktop) {

	if (tabbed == 0 || (tabbed == -1 && !Preferences::GetPrefOpenInTab())) {

		// A new window is requested, unless we find an untitled unmodified document on the current desktop
		const std::vector<DocumentWidget *> documents = DocumentWidget::allDocuments();
		for (DocumentWidget *document : documents) {
			if (document->filenameSet() || document->fileChanged() || document->macroCmdData_) {
				continue;
			}

			if (isLocatedOnDesktop(document, currentDesktop)) {
				return document;
			}
		}
	} else {
		return documentForTargetScreen(currentDesktop);
	}

	// No window found on current desktop -> create new window
	return nullptr;
}

/**
 * @brief current_desktop
 * @return
 */
QScreen *current_desktop() {
	return screenAt(QCursor::pos());
}

}

/**
 * @brief NeditServer::NeditServer
 * @param parent
 */
NeditServer::NeditServer(QObject *parent)
	: QObject(parent) {

	const QString socketName = LocalSocketName(Preferences::GetPrefServerName());
	server_                  = new QLocalServer(this);
	server_->setSocketOptions(QLocalServer::UserAccessOption);
	connect(server_, &QLocalServer::newConnection, this, &NeditServer::newConnection);

	QLocalServer::removeServer(socketName);

	if (!server_->listen(socketName)) {
		qWarning() << "NEdit: server failed to start: " << server_->errorString();
	}
}

/**
 * @brief NeditServer::newConnection
 */
void NeditServer::newConnection() {

	// NOTE(eteran): shared because later a lambda will capture this
	// and use it to keep this socket alive until it returns
	const std::shared_ptr<QLocalSocket> socket(server_->nextPendingConnection());

	QDataStream stream(socket.get());
	stream.setVersion(QDataStream::Qt_5_0);

	QByteArray jsonString;
	do {
		if (!socket->waitForReadyRead()) {
			qWarning("NEdit: error processing server request: [%d] %s", socket->error(), qPrintable(socket->errorString()));
			return;
		}

		stream.startTransaction();
		stream >> jsonString;
	} while (!stream.commitTransaction());

	QJsonParseError error;
	auto jsonDocument = QJsonDocument::fromJson(jsonString, &error);

	if (error.error != QJsonParseError::NoError) {
		qWarning("NEdit: error parsing JSON: [%d] %s \n", error.error, qPrintable(error.errorString()));
		return;
	}

	if (!jsonDocument.isArray()) {
		qWarning("NEdit: error processing server request. Top level JSON value is not an array.");
		return;
	}

	int lastIconic = 0;

	QPointer<DocumentWidget> lastFile;
	QScreen *const currentDesktop = current_desktop();

	auto array = jsonDocument.array();
	/* If the command string is empty, put up an empty, Untitled window
	   (or just pop one up if it already exists) */
	if (array.isEmpty()) {
		std::vector<DocumentWidget *> documents = DocumentWidget::allDocuments();

		auto it = std::find_if(documents.begin(), documents.end(), [currentDesktop](DocumentWidget *document) {
			return (!document->filenameSet() && !document->fileChanged() && isLocatedOnDesktop(MainWindow::fromDocument(document), currentDesktop));
		});

		if (it == documents.end()) {

			const int tabbed = -1;

			MainWindow::editNewFile(
				MainWindow::fromDocument(findDocumentOnDesktop(tabbed, currentDesktop)),
				QString(),
				false,
				QString());

			MainWindow::checkCloseEnableState();
		} else {
			(*it)->raiseDocument();
		}
		return;
	}

	for (auto entry : array) {

		if (!entry.isObject()) {
			qWarning("NEdit: error processing server request. Non-object in JSON array.");
			break;
		}

		auto file = entry.toObject();

		const bool wait            = file[QLatin1String("wait")].toBool();
		const int lineNum          = file[QLatin1String("line_number")].toInt();
		const int readFlag         = file[QLatin1String("read")].toInt();
		const int createFlag       = file[QLatin1String("create")].toInt();
		const int iconicFlag       = file[QLatin1String("iconic")].toInt();
		const int tabbed           = file[QLatin1String("is_tabbed")].toInt();
		const QString fullname     = file[QLatin1String("path")].toString();
		const QString doCommand    = file[QLatin1String("toDoCommand")].toString();
		const QString languageMode = file[QLatin1String("langMode")].toString();
		const QString geometry     = file[QLatin1String("geometry")].toString();

		/* An empty file name means:
		 *   put up an empty, Untitled window, or use an existing one
		 *   choose a random window for executing the -do macro upon
		 */
		if (fullname.isEmpty()) {

			std::vector<DocumentWidget *> documents = DocumentWidget::allDocuments();

			if (doCommand.isEmpty()) {

				auto it = std::find_if(documents.begin(), documents.end(), [currentDesktop](DocumentWidget *doc) {
					return (!doc->filenameSet() && !doc->fileChanged() && isLocatedOnDesktop(MainWindow::fromDocument(doc), currentDesktop));
				});

				if (it == documents.end()) {

					MainWindow::editNewFile(
						MainWindow::fromDocument(findDocumentOnDesktop(tabbed, currentDesktop)),
						QString(),
						iconicFlag,
						languageMode.isEmpty() ? QString() : languageMode);
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
					(*win)->doMacro(doCommand, QLatin1String("-do macro"));
				}
			}

			MainWindow::checkCloseEnableState();
			return;
		}

		/* Process the filename by looking for the files in an
		   existing window, or opening if they don't exist */
		const int editFlags =
			(readFlag ? EditFlags::PREF_READ_ONLY : 0) |
			EditFlags::CREATE |
			(createFlag ? EditFlags::SUPPRESS_CREATE_WARN : 0);

		const PathInfo fi = parseFilename(fullname);

		DocumentWidget *document = MainWindow::findWindowWithFile(fi);
		if (!document) {
			/* Files are opened in background to improve opening speed
			   by deferring certain time consuming task such as syntax
			   highlighting. At the end of the file-opening loop, the
			   last file opened will be raised to restore those deferred
			   items. The current file may also be raised if there're
			   macros to execute on. */

			document = DocumentWidget::editExistingFile(
				findDocumentOnDesktop(tabbed, currentDesktop),
				fi.filename,
				fi.pathname,
				editFlags,
				geometry,
				iconicFlag,
				languageMode.isEmpty() ? QString() : languageMode,
				tabbed == -1 ? Preferences::GetPrefOpenInTab() : tabbed,
				/*background=*/true);

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
				// is very inconvenient to get at this point in the code (now)
				// firstPane() seems practical for now
				document->selectNumberedLine(document->firstPane(), lineNum);
			}

			if (!doCommand.isEmpty()) {
				document->raiseDocument();

				/* Starting a new command while another one is still running
				   in the same window is not possible (crashes). */
				if (document->macroCmdData_) {
					QApplication::beep();
				} else {
					document->doMacro(doCommand, QLatin1String("-do macro"));
				}
			}

			// register the last file opened for later use
			if (document) {
				lastFile   = document;
				lastIconic = iconicFlag;
			}

			if (wait) {
				// by creating this lambda, we are incrementing the reference
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
		MainWindow::checkCloseEnableState();
	}
}
