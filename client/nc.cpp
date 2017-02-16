/*******************************************************************************
*                                                                              *
* nc.c -- Nirvana Editor client program for nedit server processes             *
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

#include "Settings.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QSettings>
#include <QString>
#include <QTextStream>

#include "util/fileUtils.h"
#include "util/utils.h"
#include "util/system.h"
#include "util/clearcase.h"
#include "server_common.h"

#include <cstdio>
#include <cstdlib>
#include <climits>
#include <cstring>
#include <list>
#include <algorithm>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <pwd.h>

struct CommandLine {
    QString shell;
    QString serverRequest;
};

static CommandLine processCommandLine(int argc, char **argv);
static void parseCommandLine(int argc, char **arg, CommandLine *cmdLine);
static void nextArg(int argc, char **argv, int *argIndex);
static void copyCommandLineArg(CommandLine *cmdLine, const char *arg);
static void printNcVersion();



static const char cmdLineHelp[] = "Usage:  nc [-read] [-create]\n"
                                  "           [-line n | +n] [-do command] [-lm languagemode]\n"
                                  "           [-svrname name] [-svrcmd command]\n"
                                  "           [-ask] [-noask] [-timeout seconds]\n"
                                  "           [-geometry geometry | -g geometry] [-icon | -iconic]\n"
                                  "           [-tabbed] [-untabbed] [-group] [-wait]\n"
                                  "           [-V | -version] [-h|-help]\n"
                                  "           [-xrm resourcestring] [-display [host]:server[.screen]]\n"
                                  "           [--] [file...]\n";

// Structure to hold X Resource values 
static struct {	
    QString serverCmd; // holds executable name + flags
    QString serverName;	
	int timeOut;
    int autoStart;
    bool waitForClose;
} ServerPreferences;


/**
 * @brief main
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char *argv[]) {

    QCoreApplication app(argc, argv);

    if(!QDBusConnection::sessionBus().isConnected()) {
        return -1;
    }

    /* Process the command line before calling XtOpenDisplay, because the
       latter consumes certain command line arguments that we still need
       (-icon, -geometry ...) */
    CommandLine commandLine = processCommandLine(argc, argv);

    // Read the application resources into the Preferences data structure
    QString filename = Settings::configFile();
    QSettings settings(filename, QSettings::IniFormat);
    settings.beginGroup(QLatin1String("Server"));

    ServerPreferences.autoStart     = settings.value(QLatin1String("nc.autoStart"),     true).toBool();
    ServerPreferences.serverCmd     = settings.value(QLatin1String("nc.serverCommand"), QLatin1String("nedit-ng -server")).toString();
    ServerPreferences.serverName    = settings.value(QLatin1String("nc.serverName"),    QLatin1String("")).toString();
    ServerPreferences.waitForClose  = settings.value(QLatin1String("nc.waitForClose"),  false).toBool();
    ServerPreferences.timeOut       = settings.value(QLatin1String("nc.timeOut"),       10).toInt();

    /* Make sure that the time out unit is at least 1 second and not too
       large either (overflow!). */
    ServerPreferences.timeOut = qBound(1, ServerPreferences.timeOut, 1000);

    /* For Clearcase users who have not set a server name, use the clearcase
       view name.  Clearcase views make files with the same absolute path names
       but different contents (and therefore can't be edited in the same nedit
       session). This should have no bad side-effects for non-clearcase users */
    if (ServerPreferences.serverName.isEmpty()) {
        const QString viewTag = GetClearCaseViewTag();
        if (!viewTag.isNull() && viewTag.size() < MAXPATHLEN) {
            ServerPreferences.serverName =  viewTag;
        }
    }

    // TODO(eteran): start the server if needed!
    QDBusInterface iface(QLatin1String(SERVICE_NAME), QLatin1String("/Server"), QLatin1String(""), QDBusConnection::sessionBus());
    if(iface.isValid()) {
        QDBusReply<void> reply = iface.call(QLatin1String("processCommand"), commandLine.serverRequest);
        if(reply.isValid()) {
            qDebug("Success!");
        }
    }




#if 0
    if (!serverExists) {
		startNewServer(context, rootWindow, commandLine.shell, serverExistsAtom);
    }
#endif

}


#if 0
static void startNewServer(XtAppContext context, Window rootWindow, QString commandLine, Atom serverExistsAtom) {
	Boolean timeOut = False;
	XtIntervalId timerId;

	/* Add back the server name resource from the command line or resource
	   database to the command line for starting the server.  If -svrcmd
	   appeared on the original command line, it was removed by
	   CreatePreferencesDatabase before the command line was recorded
	   in commandLine.shell. Moreover, if no server name was specified, it
	   may have defaulted to the ClearCase view tag. */
    if (!ServerPreferences.serverName.isEmpty()) {
        commandLine.append(QLatin1Literal(" -svrname "));
        commandLine.append(ServerPreferences.serverName);
	}
    switch (startServer("No servers available, start one? (y|n) [y]: ", commandLine.toLatin1().data())) {
	case -1: // Start failed 
		XtCloseDisplay(TheDisplay);
		exit(EXIT_FAILURE);
		break;
	case -2: // Start canceled by user 
		XtCloseDisplay(TheDisplay);
		exit(EXIT_SUCCESS);
		break;
	}

	/* Set up a timeout proc in case the server is dead.  The standard
	   selection timeout is probably a good guess at how long to wait
	   for this style of inter-client communication as well */
	timerId = XtAppAddTimeOut(context, SERVER_START_TIMEOUT, (XtTimerCallbackProc)timeOutProc, &timeOut);
	currentWaitForAtom = serverExistsAtom;

	// Wait for the server to start 
	while (!timeOut) {
		XEvent event;
		const XPropertyEvent *e = (const XPropertyEvent *)&event;
		/* NOTE: XtAppNextEvent() does call the timeout routine but
		** doesn't return unless there are more events. */
		XtAppNextEvent(context, &event);

		/* We will get a PropertyNewValue when the server updates
		** the server exists atom. If the property is deleted the
		** the server must have died. */
		if (e->type == PropertyNotify && e->window == rootWindow && e->atom == serverExistsAtom) {
			if (e->state == PropertyNewValue) {
				break;
			} else if (e->state == PropertyDelete) {
				fprintf(stderr, "%s: The server failed to start.\n", APP_NAME);
				XtCloseDisplay(TheDisplay);
				exit(EXIT_FAILURE);
			}
		}
		XtDispatchEvent(&event);
	}
	// Exit if the timeout expired. 
	if (timeOut) {
		fprintf(stderr, "%s: The server failed to start (time-out).\n", APP_NAME);
		XtCloseDisplay(TheDisplay);
		exit(EXIT_FAILURE);
	} else {
		XtRemoveTimeOut(timerId);
	}
}


/*
** Prompt the user about starting a server, with "message", then start server
*/
static int startServer(const char *message, const char *commandLineArgs) {
	
	// prompt user whether to start server 
    if (!ServerPreferences.autoStart) {
		char c;
		printf("%s", message);
		do {
			c = getc(stdin);
		} while (c == ' ' || c == '\t');
		if (c != 'Y' && c != 'y' && c != '\n')
			return (-2);
	}

	// start the server 
    auto commandLine = QString(QLatin1String("%1 %2&")).arg(ServerPreferences.serverCmd).arg(QString::fromLatin1(commandLineArgs));
	
	int sysrc = system(commandLine.toLatin1().data());

	return (sysrc == 0) ? 0 : -1;
}
#endif

/* Reconstruct the command line in string commandLine in case we have to
 * start a server (nc command line args parallel nedit's).  Include
 * -svrname if nc wants a named server, so nedit will match. Special
 * characters are protected from the shell by escaping EVERYTHING with \
 */
static CommandLine processCommandLine(int argc, char **argv) {
	CommandLine commandLine;

    commandLine.shell = QString();

	// Convert command line arguments into a command string for the server 
	parseCommandLine(argc, argv, &commandLine);
    if(commandLine.serverRequest.isNull()) {
		fprintf(stderr, "nc: Invalid commandline argument\n");
		exit(EXIT_FAILURE);
	}

    return commandLine;
}

/*
** Converts command line into a command string suitable for passing to
** the server
*/
static void parseCommandLine(int argc, char **argv, CommandLine *commandLine) {

    char name[MAXPATHLEN];
    char path[MAXPATHLEN];
    const char *toDoCommand = "";
    const char *langMode = "";
    const char *geometry = "";
    int lineNum = 0;
    int read = 0;
    int create = 0;
    int iconic = 0;
    int tabbed = -1;
    int i;
    int lineArg;
    int fileCount = 0;
    int group = 0;
    int isTabbed;
	bool opts = true;

    // Parse the arguments and write the output string
    QString commandString;
    QTextStream out(&commandString);
	
	for (i = 1; i < argc; i++) {
		if (opts && !strcmp(argv[i], "--")) {
            opts = false; // treat all remaining arguments as filenames
			continue;
		} else if (opts && !strcmp(argv[i], "-do")) {
			nextArg(argc, argv, &i);
			toDoCommand = argv[i];
		} else if (opts && !strcmp(argv[i], "-lm")) {
			copyCommandLineArg(commandLine, argv[i]);
			nextArg(argc, argv, &i);
			langMode = argv[i];
			copyCommandLineArg(commandLine, argv[i]);
		} else if (opts && (!strcmp(argv[i], "-g") || !strcmp(argv[i], "-geometry"))) {
			copyCommandLineArg(commandLine, argv[i]);
			nextArg(argc, argv, &i);
			geometry = argv[i];
			copyCommandLineArg(commandLine, argv[i]);
		} else if (opts && !strcmp(argv[i], "-read")) {
			read = 1;
		} else if (opts && !strcmp(argv[i], "-create")) {
			create = 1;
		} else if (opts && !strcmp(argv[i], "-tabbed")) {
			tabbed = 1;
			group = 0; // override -group option 
		} else if (opts && !strcmp(argv[i], "-untabbed")) {
			tabbed = 0;
			group = 0; // override -group option 
		} else if (opts && !strcmp(argv[i], "-group")) {
			group = 2; // 2: start new group, 1: in group 
		} else if (opts && (!strcmp(argv[i], "-iconic") || !strcmp(argv[i], "-icon"))) {
			iconic = 1;
			copyCommandLineArg(commandLine, argv[i]);
		} else if (opts && !strcmp(argv[i], "-line")) {
			nextArg(argc, argv, &i);
			
			char *end = nullptr;
			lineArg = strtol(argv[i], &end, 10);
			if(*end != '\0') {
				fprintf(stderr, "nc: argument to line should be a number\n");
			} else {
				lineNum = lineArg;
			}
		} else if (opts && (*argv[i] == '+')) {
		
			char *end = nullptr;
			lineArg = strtol(argv[i], &end, 10);
			if(*end != '\0') {
				fprintf(stderr, "nc: argument to + should be a number\n");
			} else {
				lineNum = lineArg;
			}
		} else if (opts && (!strcmp(argv[i], "-ask") || !strcmp(argv[i], "-noask"))) {
			; // Ignore resource-based arguments which are processed later 
		} else if (opts && (!strcmp(argv[i], "-svrname") || !strcmp(argv[i], "-svrcmd"))) {
			nextArg(argc, argv, &i); // Ignore rsrc args with data 
		} else if (opts && (!strcmp(argv[i], "-xrm") || !strcmp(argv[i], "-display"))) {
			copyCommandLineArg(commandLine, argv[i]);
			nextArg(argc, argv, &i); // Ignore rsrc args with data 
			copyCommandLineArg(commandLine, argv[i]);
		} else if (opts && (!strcmp(argv[i], "-version") || !strcmp(argv[i], "-V"))) {
			printNcVersion();
			exit(EXIT_SUCCESS);
		} else if (opts && (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-help"))) {
			fprintf(stderr, "%s", cmdLineHelp);
			exit(EXIT_SUCCESS);
		} else if (opts && (*argv[i] == '-')) {

			fprintf(stderr, "nc: Unrecognized option %s\n%s", argv[i], cmdLineHelp);
			exit(EXIT_FAILURE);
		} else {
			if (ParseFilename(argv[i], name, path) != 0) {
				// An Error, most likely too long paths/strings given 
                commandLine->serverRequest = QString();
				return;
			}
			strcat(path, name);

			/* determine if file is to be openned in new tab, by
			   factoring the options -group, -tabbed & -untabbed */
			if (group == 2) {
				isTabbed = 0; // start a new window for new group 
				group = 1;    // next file will be within group 
			} else if (group == 1) {
				isTabbed = 1; // new tab for file in group 
			} else {
				isTabbed = tabbed; // not in group 
			}

			/* SunOS 4 acc or acc and/or its runtime library has a bug
			   such that %n fails (segv) if it follows a string in a
			   printf or sprintf.  The silly code below avoids this.

			   The "long" cast on strlen() is necessary because size_t
			   is 64 bit on Alphas, and 32-bit on most others.  There is
			   no printf format specifier for "size_t", thanx, ANSI. */
            QString temp;
            temp.sprintf("%d %d %d %d %d %ld %ld %ld %ld\n",
                         lineNum,
                         read,
                         create,
                         iconic,
                         isTabbed,
                         static_cast<long>(strlen(path)),
                         static_cast<long>(strlen(toDoCommand)),
                         static_cast<long>(strlen(langMode)),
                         static_cast<long>(strlen(geometry)));

            out << temp;
            out << path << '\n';
            out << toDoCommand << '\n';
            out << langMode << '\n';
            out << geometry << '\n';

			// These switches only affect the next filename argument, not all 
			toDoCommand = "";
			lineNum = 0;
		}
	}

	/* If there's an un-written -do command, or we are to open a new window,
	 * or user has requested iconic state, but not provided a file name,
	 * create a server request with an empty file name and requested
	 * iconic state (and optional language mode and geometry).
	 */
	if (toDoCommand[0] != '\0' || fileCount == 0) {

        QString temp;
        temp.sprintf("0 0 0 %d %d 0 %ld %ld %ld\n\n",
                     iconic,
                     tabbed,
                     static_cast<long>(strlen(toDoCommand)),
                     static_cast<long>(strlen(langMode)),
                     static_cast<long>(strlen(geometry)));

        out << temp;
        out << toDoCommand << '\n';
        out << langMode << '\n';
        out << geometry << '\n';
	}

	commandLine->serverRequest = commandString;
}
/**
 * @brief nextArg
 * @param argc
 * @param argv
 * @param argIndex
 */
static void nextArg(int argc, char **argv, int *argIndex) {
	if (*argIndex + 1 >= argc) {
		fprintf(stderr, "nc: %s requires an argument\n%s", argv[*argIndex], cmdLineHelp);
		exit(EXIT_FAILURE);
	}
	(*argIndex)++;
}

/* Copies a given nc command line argument to the server startup command
** line (-icon, -geometry, -xrm, ...) Special characters are protected from
** the shell by escaping EVERYTHING with \
** Note that the .shell string in the command line structure is large enough
** to hold the escaped characters.
*/
static void copyCommandLineArg(CommandLine *commandLine, const char *arg) {

    auto outPtr = std::back_inserter(commandLine->shell);

    *outPtr++ = QLatin1Char('\'');
    for (const char *c = arg; *c != '\0'; c++) {

        if (*c == '\'') {
            *outPtr++ = QLatin1Char('\'');
            *outPtr++ = QLatin1Char('\\');
		}

        *outPtr++ = QLatin1Char(*c);

        if (*c == '\'') {
            *outPtr++ = QLatin1Char('\'');
		}
	}

    *outPtr++ = QLatin1Char('\'');
    *outPtr++ = QLatin1Char(' ');
}

// Print version of 'nc' 
static void printNcVersion() {
    static const char ncHelpText[] = "nc (nedit-ng) Version 1.0\n\n\
     Built on: %s, %s, %s\n\
     Built at: %s, %s\n";

    printf(ncHelpText, COMPILE_OS, COMPILE_MACHINE, COMPILE_COMPILER, __DATE__, __TIME__);
}
