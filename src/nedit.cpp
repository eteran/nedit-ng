
#include "nedit.h"
#include "Main.h"

#include <QApplication>
#include <QFontDatabase>
#include <QIcon>
#include <QLibraryInfo>
#include <QPalette>
#include <QStringList>
#include <QStyleFactory>
#include <QTranslator>

bool IsServer = false;

namespace {

/**
 * @brief Custom message handler for Qt logging.
 *
 * @param type The type of the message (debug, warning, info, critical, fatal).
 * @param context The context of the message, including file, function, and line number.
 * @param msg The message to log.
 */
void MessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {

	Q_UNUSED(context);

	switch (type) {
	case QtDebugMsg:
#ifndef NDEBUG
		fprintf(stderr, "Debug: %s\n", qPrintable(msg));
#endif
		break;
	case QtWarningMsg:
		fprintf(stderr, "Warning: %s\n", qPrintable(msg));
		break;
	case QtInfoMsg:
		fprintf(stderr, "Info: %s\n", qPrintable(msg));
		break;
	case QtCriticalMsg:
		fprintf(stderr, "Critical: %s\n", qPrintable(msg));
		break;
	case QtFatalMsg:
		fprintf(stderr, "Fatal: %s\n", qPrintable(msg));
		abort();
	}
}

}

/**
 * @brief Main entry point for the NEdit application.
 *
 * @param argc The number of command line arguments.
 * @param argv The command line arguments as an array of strings.
 * @return The exit code of the application.
 */
int main(int argc, char *argv[]) {

	// On linux (and perhaps other UNIX's), we should support -version even
	// when X11 is not running
#ifdef Q_OS_LINUX
	if (qEnvironmentVariableIsEmpty("DISPLAY")) {
		// Respond to -V or -version even if there is no display
		for (int i = 1; i < argc && strcmp(argv[i], "--") != 0; i++) {

			if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "-version") == 0) {
				const QString infoString = DialogAbout::createInfoString();
				printf("%s", qPrintable(infoString));
				exit(EXIT_SUCCESS);
			}
		}

		fputs("NEdit: Can't open display\n", stderr);
		exit(EXIT_FAILURE);
	}
#endif

	qInstallMessageHandler(MessageHandler);

#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0) && QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
#ifdef Q_OS_MACOS
	QCoreApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
	QCoreApplication::setAttribute(Qt::AA_DontUseNativeMenuBar);
#endif
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

	// NOTE(eteran): for issue #38, grab -geometry <arg> before Qt consumes it!
	QString geometry;
	for (int i = 1; i < argc && strcmp(argv[i], "--") != 0; i++) {
		if (strcmp(argv[i], "-geometry") == 0) {
			if (i++ < argc) {
				geometry = QString::fromLatin1(argv[i]);
			}
		}
	}

	QApplication app(argc, argv);
	QApplication::setWindowIcon(QIcon(QStringLiteral(":/nedit-ng.png")));

	// NOTE: for issue #41, translate QMessageBox.
	QTranslator qtTranslator;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	if (qtTranslator.load(QLocale(), QStringLiteral("qtbase"), QStringLiteral("_"), QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
		QApplication::installTranslator(&qtTranslator);
	}
#else
	if (qtTranslator.load(QLocale(), QStringLiteral("qtbase"), QStringLiteral("_"), QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
		QApplication::installTranslator(&qtTranslator);
	}
#endif

	QTranslator translator;
	// look up e.g. :/i18n/nedit-ng_{lang}.qm
	if (translator.load(QLocale(), QStringLiteral("nedit-ng"), QStringLiteral("_"), QStringLiteral(":/i18n"))) {
		QApplication::installTranslator(&translator);
	}

	// NOTE(eteran): for issue #38, re-add -geometry <arg> because Qt has
	// removed it at this point
	QStringList arguments = QApplication::arguments();
	if (!geometry.isNull()) {
		arguments.insert(1, QStringLiteral("-geometry"));
		arguments.insert(2, geometry);
	}

#ifdef Q_OS_MACOS
	qApp->setStyle(QStyleFactory::create(QStringLiteral("fusion")));
#endif

	// Light/Dark icons on all platforms
	if (qApp->palette().window().color().lightnessF() >= 0.5) {
		QIcon::setThemeName(QStringLiteral("breeze-nedit"));
	} else {
		QIcon::setThemeName(QStringLiteral("breeze-dark-nedit"));
	}

	// Make all text fields use fixed-width fonts by default
	const QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	QApplication::setFont(fixedFont, "QLineEdit");
	QApplication::setFont(fixedFont, "QTextEdit");
	QApplication::setFont(fixedFont, "QPlainTextEdit");

	// NEdit classic applied fixed-width fonts to list widgets, but since
	// they're not editable, it's not really needed here. I imagine it
	// was there to deal with fake columns in the tags conflict dialog,
	// or anywhere else that used spaces to emulate columns. Motif didn't
	// have column-based list widgets.
	// QApplication::setFont(fixedFont, "QListWidget");

	Main main{arguments};

	// Process events.
	return qApp->exec();
}
