
#include "Main.h"
#include "CommandRecorder.h"
#include "DialogAbout.h"
#include "DocumentWidget.h"
#include "EditFlags.h"
#include "MainWindow.h"
#include "NeditServer.h"
#include "Preferences.h"
#include "Regex.h"
#include "Settings.h"
#include "Util/FileSystem.h"
#include "interpret.h"
#include "macro.h"
#include "nedit.h"
#include "parse.h"

#include <QApplication>
#include <QFile>
#include <QString>

namespace {

constexpr const char cmdLineHelp[] =
	"Usage: nedit-ng [-read] [-create] [-line n | +n] [-server] [-do command]\n"
	"                [-tags file] [-tabs n] [-wrap] [-nowrap] [-autowrap]\n"
	"                [-autoindent] [-noautoindent] [-autosave] [-noautosave]\n"
	"                [-lm languagemode] [-rows n] [-columns n] [-font font]\n"
	"                [-geometry geometry] [-iconic] [-noiconic] [-svrname name]\n"
	"                [-import file] [-tabbed] [-untabbed] [-group] [-V|-version]\n"
	"                [-h|-help] [--] [file...]\n";

/**
 * @brief Gets the index of the next argument parameter.
 *
 * @param args The command line arguments.
 * @param argIndex The current argument index.
 * @return The next argument index.
 */
int getArgumentParameter(const QStringList &args, int argIndex) {
	if (argIndex + 1 >= args.size()) {
		fprintf(stderr, "NEdit: %s requires an argument\n%s", qPrintable(args[argIndex]), cmdLineHelp);
		exit(EXIT_FAILURE);
	}

	return ++argIndex;
}

}

/**
 * @brief Destructor for Main class.
 */
Main::~Main() {
	CleanupMacroGlobals();
}

/**
 * @brief Constructor for Main class.
 *
 * @param args The command line arguments passed to the application.
 */
Main::Main(const QStringList &args) {

	int lineNum          = 0;
	int editFlags        = EditFlags::CREATE;
	bool gotoLine        = false;
	bool macroFileReadEx = false;
	bool iconic          = false;
	bool opts            = true;
	int tabbed           = -1;
	int group            = 0;
	int isTabbed;
	QString geometry;
	QString langMode;
	QString toDoCommand;
	QPointer<DocumentWidget> lastFile;

	// Enable a Qt style sheet if present
	const QString StyleFile = Settings::StyleFile();
	QFile file(StyleFile);
	if (file.open(QIODevice::ReadOnly)) {
		qApp->setStyleSheet(QString::fromUtf8(file.readAll()));
		file.close();
	}

	// Initialize global symbols and subroutines used in the macro language
	InitMacroGlobals();
	RegisterMacroSubroutines();

	/* Store preferences from the command line and .nedit file,
	   and set the appropriate preferences */
	Preferences::RestoreNEditPrefs();

	// Install word delimiters for regular expression matching
	Regex::SetDefaultWordDelimiters(Preferences::GetPrefDelimiters().toStdString());

	/* Read the nedit dynamic database of files for the Open Previous
	command (and eventually other information as well) */
	MainWindow::readNEditDB();

	/* Process -import command line argument before others which might
	   open windows (loading preferences doesn't update menu settings,
	   which would then be out of sync with the real preference settings) */
	for (int i = 1; i < args.size(); ++i) {

		const QString &arg = args[i];

		if (arg == QLatin1String("--")) {
			break; // treat all remaining arguments as filenames
		}

		if (arg == QLatin1String("-import")) {
			i = getArgumentParameter(args, i);
			Preferences::ImportPrefFile(args[i]);
		}
	}

	/* Load the default tags file. Don't complain if it doesn't load, the tag
	   file resource is intended to be set and forgotten.  Running nedit in a
	   directory without a tags should not cause it to spew out errors. */
	if (!Preferences::GetPrefTagFile().isEmpty()) {
		Tags::addTagsFile(Preferences::GetPrefTagFile(), Tags::SearchMode::TAG);
	}

	if (!Preferences::GetPrefServerName().isEmpty()) {
		IsServer = true;
	}

	bool fileSpecified = false;

	for (int i = 1; i < args.size(); i++) {

		if (opts && args[i] == QLatin1String("--")) {
			opts = false; // treat all remaining arguments as filenames
			continue;
		}

		if (opts && args[i] == QLatin1String("-tags")) {
			i = getArgumentParameter(args, i);
			if (!Tags::addTagsFile(args[i], Tags::SearchMode::TAG)) {
				fprintf(stderr, "NEdit: Unable to load tags file\n");
			}

		} else if (opts && args[i] == QLatin1String("-do")) {
			i = getArgumentParameter(args, i);
			if (checkDoMacroArg(args[i])) {
				toDoCommand = args[i];
			}
		} else if (opts && args[i] == QLatin1String("-svrname")) {
			i = getArgumentParameter(args, i);

			Settings::serverNameOverride = args[i];
			IsServer                     = true;
		} else if (opts && (args[i] == QLatin1String("-font") || args[i] == QLatin1String("-fn"))) {
			i = getArgumentParameter(args, i);

			Settings::fontName = args[i];
		} else if (opts && args[i] == QLatin1String("-wrap")) {
			Settings::autoWrap = WrapStyle::Continuous;
		} else if (opts && args[i] == QLatin1String("-nowrap")) {
			Settings::autoWrap = WrapStyle::None;
		} else if (opts && args[i] == QLatin1String("-autowrap")) {
			Settings::autoWrap = WrapStyle::Newline;
		} else if (opts && args[i] == QLatin1String("-autoindent")) {
			Settings::autoIndent = IndentStyle::Auto;
		} else if (opts && args[i] == QLatin1String("-noautoindent")) {
			Settings::autoIndent = IndentStyle::None;
		} else if (opts && args[i] == QLatin1String("-autosave")) {
			Settings::autoSave = true;
		} else if (opts && args[i] == QLatin1String("-noautosave")) {
			Settings::autoSave = false;
		} else if (opts && args[i] == QLatin1String("-rows")) {
			i = getArgumentParameter(args, i);

			bool ok;
			const int n = args[i].toInt(&ok);
			if (!ok) {
				fprintf(stderr, "NEdit: argument to rows should be a number\n");
			} else {
				Settings::textRows = n;
			}
		} else if (opts && args[i] == QLatin1String("-columns")) {
			i = getArgumentParameter(args, i);

			bool ok;
			const int n = args[i].toInt(&ok);
			if (!ok) {
				fprintf(stderr, "NEdit: argument to cols should be a number\n");
			} else {
				Settings::textCols = n;
			}
		} else if (opts && args[i] == QLatin1String("-tabs")) {
			i = getArgumentParameter(args, i);

			bool ok;
			const int n = args[i].toInt(&ok);
			if (!ok) {
				fprintf(stderr, "NEdit: argument to tabs should be a number\n");
			} else {
				Settings::tabDistance = n;
			}
		} else if (opts && args[i] == QLatin1String("-read")) {
			editFlags |= PREF_READ_ONLY;
		} else if (opts && args[i] == QLatin1String("-create")) {
			editFlags |= SUPPRESS_CREATE_WARN;
		} else if (opts && args[i] == QLatin1String("-tabbed")) {
			tabbed = 1;
			group  = 0; // override -group option
		} else if (opts && args[i] == QLatin1String("-untabbed")) {
			tabbed = 0;
			group  = 0; // override -group option
		} else if (opts && args[i] == QLatin1String("-group")) {
			group = 2; // 2: start new group, 1: in group
		} else if (opts && args[i] == QLatin1String("-line")) {
			i = getArgumentParameter(args, i);

			bool ok;
			lineNum = args[i].toInt(&ok);
			if (!ok) {
				fprintf(stderr, "NEdit: argument to line should be a number\n");
			} else {
				gotoLine = true;
			}
		} else if (opts && (args[i].startsWith(QLatin1Char('+')))) {
			bool ok;
			lineNum = args[i].toInt(&ok);
			if (!ok) {
				fprintf(stderr, "NEdit: argument to + should be a number\n");
			} else {
				gotoLine = true;
			}
		} else if (opts && args[i] == QLatin1String("-server")) {
			IsServer = true;
		} else if (opts && (args[i] == QLatin1String("-iconic") || args[i] == QLatin1String("-icon"))) {
			iconic = true;
		} else if (opts && args[i] == QLatin1String("-noiconic")) {
			iconic = false;
		} else if (opts && (args[i] == QLatin1String("-geometry") || args[i] == QLatin1String("-g"))) {
			i = getArgumentParameter(args, i);

			geometry = args[i];
		} else if (opts && args[i] == QLatin1String("-lm")) {
			i = getArgumentParameter(args, i);

			langMode = args[i];
		} else if (opts && args[i] == QLatin1String("-import")) {
			i = getArgumentParameter(args, i); // already processed, skip
		} else if (opts && (args[i] == QLatin1String("-V") || args[i] == QLatin1String("-version"))) {
			const QString infoString = DialogAbout::createInfoString();
			printf("%s", qPrintable(infoString));
			exit(EXIT_SUCCESS);
		} else if (opts && (args[i] == QLatin1String("-h") || args[i] == QLatin1String("-help"))) {
			fprintf(stderr, "%s", cmdLineHelp);
			exit(EXIT_SUCCESS);
		} else if (opts && (args[i].startsWith(QLatin1Char('-')))) {
			fprintf(stderr, "nedit: Unrecognized option %s\n%s", qPrintable(args[i]), cmdLineHelp);
			exit(EXIT_FAILURE);
		} else {

			const PathInfo fi = parseFilename(args[i]);

			/* determine if file is to be opened in new tab, by
			   factoring the options -group, -tabbed & -untabbed */
			switch (group) {
			case 2:
				isTabbed = 0; // start a new window for new group
				group    = 1; // next file will be within group
				break;
			case 1:
				isTabbed = 1; // new tab for file in group
				break;
			default: // not in group
				isTabbed = (tabbed == -1) ? Preferences::GetPrefOpenInTab() : tabbed;
			}

			/* Files are opened in background to improve opening speed
			   by deferring certain time consuming task such as syntax
			   highlighting. At the end of the file-opening loop, the
			   last file opened will be raised to restore those deferred
			   items. The current file may also be raised if there're
			   macros to execute on. */

			QPointer<DocumentWidget> document;

			if (MainWindow *window = MainWindow::firstWindow()) {
				document = DocumentWidget::editExistingFile(
					window->currentDocument(),
					fi.filename,
					fi.pathname,
					editFlags,
					geometry,
					iconic,
					langMode,
					isTabbed,
					/*background=*/true);
			} else {
				document = DocumentWidget::editExistingFile(
					nullptr,
					fi.filename,
					fi.pathname,
					editFlags,
					geometry,
					iconic,
					langMode,
					isTabbed,
					/*background=*/true);
			}

			fileSpecified = true;

			if (document) {

				// raise the last tab of previous window
				if (lastFile && MainWindow::fromDocument(lastFile) != MainWindow::fromDocument(document)) {
					lastFile->raiseDocument();
				}

				if (!macroFileReadEx) {
					document->readMacroInitFile();
					macroFileReadEx = true;
				}
				if (gotoLine) {
					document->selectNumberedLine(document->firstPane(), lineNum);
				}

				if (!toDoCommand.isNull()) {
					document->doMacro(toDoCommand, QLatin1String("-do macro"));
					toDoCommand = QString();
				}
			}

			// register last opened file for later use
			if (document) {
				lastFile = document;
			}

			// -line/+n does only affect the file following this switch
			gotoLine = false;
		}
	}

	// Raise the last file opened
	if (lastFile) {
		lastFile->raiseDocument();
	}

	MainWindow::checkCloseEnableState();

	// If no file to edit was specified, open a window to edit "Untitled"
	if (!fileSpecified) {
		DocumentWidget *document = MainWindow::editNewFile(nullptr, geometry, iconic, langMode);

		document->readMacroInitFile();
		MainWindow::checkCloseEnableState();

		if (!toDoCommand.isNull()) {
			document->doMacro(toDoCommand, QLatin1String("-do macro"));
		}
	}

	// Begin remembering last command invoked for "Repeat" menu item
	qApp->installEventFilter(CommandRecorder::instance());

	// Set up communication server!
	if (IsServer) {
		server_ = std::make_unique<NeditServer>();
	}
}

/**
 * @brief Check if the macro argument for -do is valid.
 *
 * @param macro The macro string to check.
 * @return `true` if -do macro is valid, otherwise report the an and return `false`.
 */
bool Main::checkDoMacroArg(const QString &macro) {

	QString errMsg;
	int stoppedAt;

	/* Add a terminating newline (which command line users are likely to omit
	   since they are typically invoking a single routine) */
	const QString macroString = macro + QLatin1Char('\n');

	// Do a test parse
	if (!isMacroValid(macroString, &errMsg, &stoppedAt)) {
		Preferences::reportError(nullptr, macroString, stoppedAt, tr("argument to -do"), errMsg);
		return false;
	}

	return true;
}
