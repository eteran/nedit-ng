
#include "Settings.h"

#include "Util/ClearCase.h"
#include "Util/FileSystem.h"
#include "Util/ServerCommon.h"
#include "Util/System.h"
#include "Util/version.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QProcess>
#include <QSettings>
#include <QString>
#include <QThread>

#include <boost/optional.hpp>
#include <iostream>
#include <memory>

namespace {

constexpr const char cmdLineHelp[] = "Usage: nc-ng [-read] [-create]\n"
									 "             [-line n | +n] [-do command] [-lm languagemode]\n"
									 "             [-svrname name] [-svrcmd command]\n"
									 "             [-ask] [-noask] [-timeout seconds]\n"
									 "             [-geometry geometry | -g geometry] [-icon | -iconic]\n"
									 "             [-tabbed] [-untabbed] [-group] [-wait]\n"
									 "             [-V | -version] [-h|-help]\n"
									 "             [--] [file...]\n";

struct CommandLine {
	QStringList arguments;
	QByteArray jsonRequest;
};

struct {
	QString serverCmd; // holds executable name + flags
	QString serverName;
	int timeOut;
	bool autoStart;
	bool waitForClose;
} ServerPreferences;

/**
 * @brief nextArg
 * @param args
 * @param argIndex
 */
int nextArg(const QStringList &args, int argIndex) {
	if (argIndex + 1 >= args.size()) {
		fprintf(stderr, "nc-ng: %s requires an argument\n%s", qPrintable(args[argIndex]), cmdLineHelp);
		exit(EXIT_FAILURE);
	}

	++argIndex;
	return argIndex;
}

/**
 *
 * @brief parseCommandString
 * @param program
 * @return
 */
QStringList parseCommandString(const QString &program) {

	QStringList args;
	QString arg;

	int bcount     = 0;
	bool in_quotes = false;

	auto s = program.begin();

	while (s != program.end()) {
		if (!in_quotes && s->isSpace()) {

			// Close the argument and copy it
			args << arg;
			arg.clear();

			// skip the remaining spaces
			do {
				++s;
			} while (s->isSpace());

			// Start with a new argument
			bcount = 0;
		} else if (*s == QLatin1Char('\\')) {

			// '\\'
			arg += *s++;
			++bcount;

		} else if (*s == QLatin1Char('"')) {

			// '"'
			if ((bcount & 1) == 0) {
				/* Preceded by an even number of '\', this is half that
				 * number of '\', plus a quote which we erase.
				 */

				arg.chop(bcount / 2);
				in_quotes = !in_quotes;
			} else {
				/* Preceded by an odd number of '\', this is half that
				 * number of '\' followed by a '"'
				 */

				arg.chop(bcount / 2 + 1);
				arg += QLatin1Char('"');
			}

			++s;
			bcount = 0;
		} else {
			arg += *s++;
			bcount = 0;
		}
	}

	if (!arg.isEmpty()) {
		args << arg;
	}

	return args;
}

/**
 * @brief printNcVersion
 */
void printNcVersion() {
	static constexpr const char ncHelpText[] = "nc-ng (nedit-ng) Version %d.%d\n\n"
											   "Built on: %s, %s, %s\n";
	printf(ncHelpText,
		   NEDIT_VERSION_MAJ,
		   NEDIT_VERSION_REV,
		   buildOperatingSystem().latin1(),
		   buildArchitecture().latin1(),
		   buildCompiler().toLatin1().data());
}

/**
 * @brief parseCommandLine
 * @param args
 * @return
 *
 * Converts command line into a command string suitable for passing to the
 * server
 */
boost::optional<CommandLine> parseCommandLine(const QStringList &args) {

	CommandLine commandLine;
	QString toDoCommand;
	QString langMode;
	QString geometry;
	int lineNum      = 0;
	int read         = 0;
	int create       = 0;
	int iconic       = 0;
	int fileCount    = 0;
	int group        = 0;
	int tabbed       = -1;
	bool opts        = true;
	bool debug_proto = false;

	QVariantList commandData;

	for (int i = 1; i < args.size(); i++) {

		if (opts && args[i] == QLatin1String("--")) {
			opts = false; // treat all remaining arguments as filenames
			continue;
		} else if (opts && args[i] == QLatin1String("-debug-proto")) {
			debug_proto = true;
		} else if (opts && args[i] == QLatin1String("-wait")) {
			ServerPreferences.waitForClose = true;
		} else if (opts && args[i] == QLatin1String("-svrname")) {
			i = nextArg(args, i);

			ServerPreferences.serverName = args[i];
		} else if (opts && args[i] == QLatin1String("-svrcmd")) {
			i = nextArg(args, i);

			ServerPreferences.serverCmd = args[i];
		} else if (opts && args[i] == QLatin1String("-timeout")) {
			i = nextArg(args, i);

			bool ok;
			int n = args[i].toInt(&ok);
			if (!ok) {
				fprintf(stderr, "nc-ng: argument to timeout should be a number\n");
			} else {
				ServerPreferences.timeOut = n;
			}
		} else if (opts && args[i] == QLatin1String("-do")) {
			i = nextArg(args, i);

			toDoCommand = args[i];
		} else if (opts && args[i] == QLatin1String("-lm")) {
			commandLine.arguments.push_back(args[i]);
			i = nextArg(args, i);

			langMode = args[i];
			commandLine.arguments.push_back(args[i]);
		} else if (opts && (args[i] == QLatin1String("-g") || args[i] == QLatin1String("-geometry"))) {
			commandLine.arguments.push_back(args[i]);
			i = nextArg(args, i);

			geometry = args[i];
			commandLine.arguments.push_back(args[i]);
		} else if (opts && args[i] == QLatin1String("-read")) {
			read = 1;
		} else if (opts && args[i] == QLatin1String("-create")) {
			create = 1;
		} else if (opts && args[i] == QLatin1String("-tabbed")) {
			tabbed = 1;
			group  = 0; // override -group option
		} else if (opts && args[i] == QLatin1String("-untabbed")) {
			tabbed = 0;
			group  = 0; // override -group option
		} else if (opts && args[i] == QLatin1String("-group")) {
			group = 2; // 2: start new group, 1: in group
		} else if (opts && (args[i] == QLatin1String("-iconic") || args[i] == QLatin1String("-icon"))) {
			iconic = 1;
			commandLine.arguments.push_back(args[i]);
		} else if (opts && args[i] == QLatin1String("-line")) {
			i = nextArg(args, i);

			bool ok;
			int lineArg = args[i].toInt(&ok);
			if (!ok) {
				fprintf(stderr, "nc-ng: argument to line should be a number\n");
			} else {
				lineNum = lineArg;
			}
		} else if (opts && (args[i][0] == QLatin1Char('+'))) {

			bool ok;
			int lineArg = args[i].toInt(&ok);
			if (!ok) {
				fprintf(stderr, "nc-ng: argument to + should be a number\n");
			} else {
				lineNum = lineArg;
			}
		} else if (opts && (args[i] == QLatin1String("-ask"))) {
			ServerPreferences.autoStart = false;
		} else if (opts && (args[i] == QLatin1String("-noask"))) {
			ServerPreferences.autoStart = true;
		} else if (opts && (args[i] == QLatin1String("-version") || args[i] == QLatin1String("-V"))) {
			printNcVersion();
			exit(EXIT_SUCCESS);
		} else if (opts && (args[i] == QLatin1String("-h") || args[i] == QLatin1String("-help"))) {
			fprintf(stderr, "%s", cmdLineHelp);
			exit(EXIT_SUCCESS);
		} else if (opts && (args[i][0] == QLatin1Char('-'))) {

			fprintf(stderr, "nc-ng: Unrecognized option %s\n%s", qPrintable(args[i]), cmdLineHelp);
			exit(EXIT_FAILURE);
		} else {

			// this just essentially checks that the path is sane
			const PathInfo fi = parseFilename(args[i]);
			const QString path = fi.pathname + fi.filename;

			int isTabbed;

			/* determine if file is to be openned in new tab, by
			   factoring the options -group, -tabbed & -untabbed */
			if (group == 2) {
				isTabbed = 0; // start a new window for new group
				group    = 1; // next file will be within group
			} else if (group == 1) {
				isTabbed = 1; // new tab for file in group
			} else {
				isTabbed = tabbed; // not in group
			}

			QVariantMap file;
			file[QLatin1String("line_number")] = lineNum;
			file[QLatin1String("read")]        = read;
			file[QLatin1String("create")]      = create;
			file[QLatin1String("iconic")]      = iconic;
			file[QLatin1String("is_tabbed")]   = isTabbed;
			file[QLatin1String("path")]        = path;
			file[QLatin1String("toDoCommand")] = toDoCommand;
			file[QLatin1String("langMode")]    = langMode;
			file[QLatin1String("geometry")]    = geometry;
			file[QLatin1String("wait")]        = ServerPreferences.waitForClose;
			commandData.append(file);

			++fileCount;

			// These switches only affect the next filename argument, not all
			toDoCommand = QString();
			lineNum     = 0;
		}
	}

	/* If there's an un-written -do command, or we are to open a new window,
	 * or user has requested iconic state, but not provided a file name,
	 * create a server request with an empty file name and requested
	 * iconic state (and optional language mode and geometry).
	 */
	if (!toDoCommand.isEmpty() || fileCount == 0) {

		QVariantMap file;
		file[QLatin1String("line_number")] = 0;
		file[QLatin1String("read")]        = 0;
		file[QLatin1String("create")]      = 0;
		file[QLatin1String("iconic")]      = iconic;
		file[QLatin1String("is_tabbed")]   = tabbed;
		file[QLatin1String("path")]        = QString();
		file[QLatin1String("toDoCommand")] = toDoCommand;
		file[QLatin1String("langMode")]    = langMode;
		file[QLatin1String("geometry")]    = geometry;
		file[QLatin1String("wait")]        = ServerPreferences.waitForClose;
		commandData.append(file);
	}

	auto doc                = QJsonDocument::fromVariant(commandData);
	commandLine.jsonRequest = doc.toJson(QJsonDocument::Compact);

	if (debug_proto) {
		std::cout << doc.toJson(QJsonDocument::Compact).constData() << std::endl;
	}

	return commandLine;
}

/**
 * @brief processCommandLine
 * @param args
 * @return
 *
 * Reconstruct the command line in string commandLine in case we have to start
 * a server (nc command line args parallel nedit's). Include -svrname if nc
 * wants a named server, so nedit will match. Special characters are protected
 * from the shell by escaping EVERYTHING with '\'
 */
CommandLine processCommandLine(const QStringList &args) {

	// Convert command line arguments into a command string for the server
	if (boost::optional<CommandLine> commandLine = parseCommandLine(args)) {
		return *commandLine;
	}

	fprintf(stderr, "nc-ng: Invalid commandline argument\n");
	exit(EXIT_FAILURE);
}

/**
 * @brief startServer
 * @param message
 * @param commandLineArgs
 * @return
 *
 * Prompt the user about starting a server, with "message", then start server
 */
int startServer(const char *message, const QStringList &commandLineArgs) {

	// prompt user whether to start server
	if (!ServerPreferences.autoStart) {
		int c;
		printf("%s", message);
		do {
			c = getc(stdin);
		} while (c == ' ' || c == '\t');

		if (c != 'Y' && c != 'y' && c != '\n') {
			return -2;
		}
	}

	// make sure we get any arguments out of the server command
	// (typically -server is in there, but there could be more)
	QStringList arguments = parseCommandString(ServerPreferences.serverCmd);

	// add on the arguments that were on the command line
	arguments.append(commandLineArgs);

	const QString command = arguments.takeFirst();

	// start the server
	auto process = new QProcess;
	process->start(command, arguments);

	const bool sysrc = process->waitForStarted();
	if (!sysrc) {
		switch (process->error()) {
		case QProcess::FailedToStart:
			fprintf(stderr, "nc-ng: The server process failed to start. Either the invoked program is missing, or you may have insufficient permissions to invoke the program.\n");
			break;
		case QProcess::Crashed:
			fprintf(stderr, "nc-ng: The server process crashed some time after starting successfully.\n");
			break;
		case QProcess::Timedout:
			fprintf(stderr, "nc-ng: Timeout while waiting for the server process\n");
			break;
		case QProcess::WriteError:
			fprintf(stderr, "nc-ng: An error occurred when attempting to write to the server process.\n");
			break;
		case QProcess::ReadError:
			fprintf(stderr, "nc-ng: An error occurred when attempting to read from the server process.\n");
			break;
		case QProcess::UnknownError:
			fprintf(stderr, "nc-ng: An unknown error occurred.\n");
			break;
		}
	}

	return (sysrc) ? 0 : -1;
}

}

/**
 * @brief main
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char *argv[]) {

	QCoreApplication app(argc, argv);

	// Read the application resources into the Preferences data structure
	QString filename = Settings::configFile();
	QSettings settings(filename, QSettings::IniFormat);
	settings.beginGroup(QLatin1String("Server"));

	ServerPreferences.autoStart    = settings.value(QLatin1String("nc.autoStart"), true).toBool();
	ServerPreferences.serverCmd    = settings.value(QLatin1String("nc.serverCommand"), QLatin1String("nedit-ng -server")).toString();
	ServerPreferences.serverName   = settings.value(QLatin1String("nc.serverName"), QString()).toString();
	ServerPreferences.waitForClose = settings.value(QLatin1String("nc.waitForClose"), false).toBool();
	ServerPreferences.timeOut      = settings.value(QLatin1String("nc.timeOut"), 10).toInt();
	CommandLine commandLine        = processCommandLine(QCoreApplication::arguments());

	/* Make sure that the time out unit is at least 1 second and not too
	   large either (overflow!). */
	ServerPreferences.timeOut = qBound(1, ServerPreferences.timeOut, 1000);

	/* For Clearcase users who have not set a server name, use the clearcase
	   view name.  Clearcase views make files with the same absolute path names
	   but different contents (and therefore can't be edited in the same nedit
	   session). This should have no bad side-effects for non-clearcase users */
	if (ServerPreferences.serverName.isEmpty()) {
		const QString viewTag = ClearCase::GetViewTag();
		if (!viewTag.isEmpty()) {
			ServerPreferences.serverName = viewTag;
		}
	}

	if (!ServerPreferences.serverName.isEmpty()) {
		commandLine.arguments.append(QLatin1String("-svrname"));
		commandLine.arguments.append(ServerPreferences.serverName);
	}

	QString socketName = LocalSocketName(ServerPreferences.serverName);

	for (int i = 0; i < 10; ++i) {

		auto socket = std::make_unique<QLocalSocket>();
		socket->connectToServer(socketName, QIODevice::WriteOnly);
		if (!socket->waitForConnected(ServerPreferences.timeOut * 1000)) {
			if (i == 0) {
				switch (startServer("No servers available, start one? (y|n) [y]: ", commandLine.arguments)) {
				case -1: // Start failed
					exit(EXIT_FAILURE);
				case -2: // Start canceled by user
					exit(EXIT_SUCCESS);
				}
			}

			// give just a little bit of time for things to get going...
			QThread::usleep(125000);
			continue;
		}

		QByteArray ba;
		QDataStream stream(&ba, QIODevice::WriteOnly);
		stream.setVersion(QDataStream::Qt_5_0);
		stream << commandLine.jsonRequest;

		socket->write(ba);
		socket->flush();
		socket->waitForBytesWritten(ServerPreferences.timeOut * 1000);

		// if we are enabling wait mode, we simply wait for the server
		// to close the socket. We'll leave it to the server to track
		// when everything is all done. Otherwise, just disconnect.
		if (ServerPreferences.waitForClose) {
			socket->waitForDisconnected();
		} else {
			socket->disconnectFromServer();
		}

		return 0;
	}

	return -1;
}
