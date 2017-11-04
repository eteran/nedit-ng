/*******************************************************************************
*                                                                              *
* nedit.c -- Nirvana Editor main program                                       *
*                                                                              *
* Copyright (C) 1999 Mark Edel                                                 *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute versions of this program linked to  *
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
* May 10, 1991                                                                 *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
* Modifications:                                                               *
*                                                                              *
*        8/18/93 - Mark Edel & Joy Kyriakopulos - Ported to VMS                *
*                                                                              *
*******************************************************************************/


#include "nedit.h"
#include "DialogAbout.h"
#include "DialogPrint.h"
#include "DocumentWidget.h"
#include "MainWindow.h"
#include "NeditServer.h"
#include "EditFlags.h"
#include "interpret.h"
#include "macro.h"
#include "parse.h"
#include "preferences.h"
#include "Settings.h"
#include "regularExp.h"
#include "selection.h"
#include "WrapStyle.h"
#include "tags.h"
#include "CommandRecorder.h"
#include "util/fileUtils.h"

#include <QApplication>
#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <cstring>
#include <sys/param.h>
#include "TextArea.h"
#include "TextBuffer.h"

static void nextArg(const QStringList &args, int *argIndex);
static bool checkDoMacroArg(const QString &macro);

bool IsServer = false;

namespace {

constexpr const char cmdLineHelp[] =
    "Usage: nedit [-read] [-create] [-line n | +n] [-server] [-do command]\n"
    "             [-tags file] [-tabs n] [-wrap] [-nowrap] [-autowrap]\n"
    "             [-autoindent] [-noautoindent] [-autosave] [-noautosave]\n"
    "             [-lm languagemode] [-rows n] [-columns n] [-font font]\n"
    "             [-geometry geometry] [-iconic] [-noiconic] [-svrname name]\n"
    "             [-import file] [-tabbed] [-untabbed] [-group] [-V|-version]\n"
    "             [-h|-help] [--] [file...]\n";
}

int main(int argc, char *argv[]) {

    int lineNum = 0;
    int editFlags = EditFlags::CREATE;
    bool gotoLine = false;
    bool macroFileReadEx = false;
    bool opts = true;
    bool iconic = false;
    int tabbed = -1;
    int group = 0;
    int isTabbed;
    QString geometry;
    QString langMode;
    QString filename;
    QString pathname;
    QString toDoCommand;
    QPointer<DocumentWidget> lastFileEx = nullptr;

#ifdef Q_OS_LINUX
    if (qEnvironmentVariableIsEmpty("DISPLAY")) {
		// Respond to -V or -version even if there is no display 
		for (int i = 1; i < argc && strcmp(argv[i], "--"); i++) {

            if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "-version") == 0) {
                QString infoString = DialogAbout::createInfoString();
                printf("%s", qPrintable(infoString));
				exit(EXIT_SUCCESS);
			}
		}
		fputs("NEdit: Can't open display\n", stderr);
		exit(EXIT_FAILURE);
    }
#endif

	QApplication app(argc, argv);
	QApplication::setWindowIcon(QIcon(QLatin1String(":/res/nedit.png")));

    QStringList args = app.arguments();

    // Enable a Qt style sheet if present
    QString styleFile = Settings::styleFile();
    QFile file(styleFile);
    if(file.open(QIODevice::ReadOnly)) {
        qApp->setStyleSheet(QString::fromLatin1(file.readAll()));
    }

	// Initialize global symbols and subroutines used in the macro language
	InitMacroGlobals();
	RegisterMacroSubroutines();

	/* Store preferences from the command line and .nedit file,
	   and set the appropriate preferences */
	RestoreNEditPrefs();

	// More preference stuff
	DialogPrint::LoadPrintPreferencesEx(true);

	// Install word delimiters for regular expression matching
	SetREDefaultWordDelimiters(GetPrefDelimiters().toLatin1().data());

	/* Read the nedit dynamic database of files for the Open Previous
	command (and eventually other information as well) */
	MainWindow::ReadNEditDB();

	/* Process -import command line argument before others which might
	   open windows (loading preferences doesn't update menu settings,
	   which would then be out of sync with the real preference settings) */
    for (int i = 1; i < args.size(); ++i) {

        const QString arg = args[i];

        if (arg == QLatin1String("--")) {
			break; // treat all remaining arguments as filenames 
        } else if (arg == QLatin1String("-import")) {
            nextArg(args, &i);
            ImportPrefFile(args[i], false);
        } else if (arg == QLatin1String("-importold")) {
            nextArg(args, &i);
            ImportPrefFile(args[i], true);
		}
	}

	/* Load the default tags file. Don't complain if it doesn't load, the tag
	   file resource is intended to be set and forgotten.  Running nedit in a
	   directory without a tags should not cause it to spew out errors. */
    if (!GetPrefTagFile().isEmpty()) {
        AddTagsFileEx(GetPrefTagFile(), TAG);
	}

    if (!GetPrefServerName().isEmpty()) {
		IsServer = true;
	}

    bool fileSpecified = false;

    for (int i = 1; i < args.size(); i++) {

        if (opts && args[i] == QLatin1String("--")) {
			opts = false; // treat all remaining arguments as filenames 
			continue;
        } else if (opts && args[i] == QLatin1String("-tags")) {
            nextArg(args, &i);
            if (!AddTagsFileEx(args[i], TAG)) {
				fprintf(stderr, "NEdit: Unable to load tags file\n");
            }

        } else if (opts && args[i] == QLatin1String("-do")) {
            nextArg(args, &i);
            if (checkDoMacroArg(args[i])) {
                toDoCommand = args[i];
            }
        } else if (opts && args[i] == QLatin1String("-svrname")) {
            nextArg(args, &i);
            GetSettings().serverName = args[i];
        } else if (opts && (args[i] == QLatin1String("-font") || args[i] == QLatin1String("-fn"))) {
            nextArg(args, &i);
            GetSettings().textFont = args[i];
        } else if (opts && args[i] == QLatin1String("-wrap")) {
            GetSettings().autoWrap = CONTINUOUS_WRAP;
        } else if (opts && args[i] == QLatin1String("-nowrap")) {
            GetSettings().autoWrap = NO_WRAP;
        } else if (opts && args[i] == QLatin1String("-autowrap")) {
            GetSettings().autoWrap = NEWLINE_WRAP;
        } else if (opts && args[i] == QLatin1String("-autoindent")) {
            GetSettings().autoIndent = IndentStyle::Auto;
        } else if (opts && args[i] == QLatin1String("-noautoindent")) {
            GetSettings().autoIndent = IndentStyle::None;
        } else if (opts && args[i] == QLatin1String("-autosave")) {
            GetSettings().autoSave = true;
        } else if (opts && args[i] == QLatin1String("-noautosave")) {
            GetSettings().autoSave = false;
        } else if (opts && args[i] == QLatin1String("-rows")) {
            nextArg(args, &i);

            bool ok;
            int n = args[i].toInt(&ok);
            if(!ok) {
                fprintf(stderr, "NEdit: argument to rows should be a number\n");
            } else {
                GetSettings().textRows = n;
            }
        } else if (opts && args[i] == QLatin1String("-columns")) {
            nextArg(args, &i);

            bool ok;
            int n = args[i].toInt(&ok);
            if(!ok) {
                fprintf(stderr, "NEdit: argument to cols should be a number\n");
            } else {
                GetSettings().textCols = n;
            }
        } else if (opts && args[i] == QLatin1String("-tabs")) {
            nextArg(args, &i);

            bool ok;
            int n = args[i].toInt(&ok);
            if(!ok) {
                fprintf(stderr, "NEdit: argument to tabs should be a number\n");
            } else {
                GetSettings().tabDistance = n;
            }
        } else if (opts && args[i] == QLatin1String("-read")) {
			editFlags |= PREF_READ_ONLY;
        } else if (opts && args[i] == QLatin1String("-create")) {
			editFlags |= SUPPRESS_CREATE_WARN;
        } else if (opts && args[i] == QLatin1String("-tabbed")) {
			tabbed = 1;
			group = 0; // override -group option 
        } else if (opts && args[i] == QLatin1String("-untabbed")) {
			tabbed = 0;
			group = 0; // override -group option 
        } else if (opts && args[i] == QLatin1String("-group")) {
			group = 2; // 2: start new group, 1: in group 
        } else if (opts && args[i] == QLatin1String("-line")) {
            nextArg(args, &i);

            bool ok;
            lineNum = args[i].toInt(&ok);
            if(!ok) {
				fprintf(stderr, "NEdit: argument to line should be a number\n");
            } else {
				gotoLine = true;
            }
		} else if (opts && (*argv[i] == '+')) {
            bool ok;
            lineNum = args[i].toInt(&ok);
            if(!ok) {
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
            nextArg(args, &i);
            geometry = args[i];
        } else if (opts && args[i] == QLatin1String("-lm")) {
            nextArg(args, &i);
            langMode = args[i];
        } else if (opts && args[i] == QLatin1String("-import")) {
            nextArg(args, &i); // already processed, skip
        } else if (opts && (args[i] == QLatin1String("-V") || args[i] == QLatin1String("-version"))) {
            QString infoString = DialogAbout::createInfoString();
            printf("%s", qPrintable(infoString));
			exit(EXIT_SUCCESS);
        } else if (opts && (args[i] == QLatin1String("-h") || args[i] == QLatin1String("-help"))) {
			fprintf(stderr, "%s", cmdLineHelp);
			exit(EXIT_SUCCESS);
        } else if (opts && (args[i][0] == QLatin1Char('-'))) {

            fprintf(stderr, "nedit: Unrecognized option %s\n%s", qPrintable(args[i]), cmdLineHelp);
			exit(EXIT_FAILURE);
		} else {

            if (ParseFilenameEx(args[i], &filename, &pathname) == 0) {
				/* determine if file is to be openned in new tab, by
				   factoring the options -group, -tabbed & -untabbed */
                switch(group) {
                case 2:
                    isTabbed = 0; // start a new window for new group
                    group = 1;    // next file will be within group
                    break;
                case 1:
                    isTabbed = 1; // new tab for file in group
                    break;
                default:          // not in group
                    isTabbed = (tabbed == -1) ? GetPrefOpenInTab() : tabbed;
                }

				/* Files are opened in background to improve opening speed
				   by defering certain time  consuiming task such as syntax
				   highlighting. At the end of the file-opening loop, the
				   last file opened will be raised to restore those deferred
				   items. The current file may also be raised if there're
				   macros to execute on. */

                QPointer<DocumentWidget> documentEx = nullptr;

                if(MainWindow *window = MainWindow::firstWindow()) {
                    documentEx = DocumentWidget::EditExistingFileEx(
                                window->currentDocument(),
                                filename,
                                pathname,
                                editFlags,
                                geometry,
                                iconic,
                                langMode,
                                isTabbed,
                                true);
                } else {
                    documentEx = DocumentWidget::EditExistingFileEx(
                                nullptr,
                                filename,
                                pathname,
                                editFlags,
                                geometry,
                                iconic,
                                langMode,
                                isTabbed,
                                true);
                }

                fileSpecified = true;

                if (documentEx) {

                    // raise the last tab of previous window
                    if (lastFileEx && lastFileEx->toWindow() != documentEx->toWindow()) {
                        lastFileEx->RaiseDocument();
                    }

                    if (!macroFileReadEx) {
                        ReadMacroInitFileEx(documentEx);
                        macroFileReadEx = true;
                    }
                    if (gotoLine) {
                        SelectNumberedLineEx(documentEx, documentEx->firstPane(), lineNum);
                    }

                    if (!toDoCommand.isNull()) {
                        DoMacroEx(documentEx, toDoCommand, QLatin1String("-do macro"));
                        toDoCommand = QString();
                    }
                }

                // register last opened file for later use
                if (documentEx) {
                    lastFileEx = documentEx;
                }

			} else {
                fprintf(stderr, "nedit: file name too long: %s\n", qPrintable(args[i]));
			}

			// -line/+n does only affect the file following this switch 
			gotoLine = false;
		}
	}

	// Raise the last file opened 
    if (lastFileEx) {
        lastFileEx->RaiseDocument();
    }
    MainWindow::CheckCloseDimEx();

    // If no file to edit was specified, open a window to edit "Untitled"
	if (!fileSpecified) {
        DocumentWidget *documentEx = MainWindow::EditNewFileEx(nullptr, geometry, iconic, langMode, QString());

        ReadMacroInitFileEx(documentEx);
        MainWindow::CheckCloseDimEx();

        if (!toDoCommand.isNull()) {
            DoMacroEx(documentEx, toDoCommand, QLatin1String("-do macro"));
        }
	}

    // Begin remembering last command invoked for "Repeat" menu item
    qApp->installEventFilter(CommandRecorder::getInstance());

    // Set up communication server!
    std::unique_ptr<NeditServer> server;
    if (IsServer) {
        server = std::make_unique<NeditServer>();
	}

	// Process events. 
    return app.exec();
}

static void nextArg(const QStringList &args, int *argIndex) {
    if (*argIndex + 1 >= args.size()) {
        fprintf(stderr, "NEdit: %s requires an argument\n%s", qPrintable(args[*argIndex]), cmdLineHelp);
		exit(EXIT_FAILURE);
	}
	(*argIndex)++;
}

/*
** Return True if -do macro is valid, otherwise write an error on stderr
*/
static bool checkDoMacroArg(const QString &macro) {

    QString errMsg;
    int stoppedAt;

	/* Add a terminating newline (which command line users are likely to omit
	   since they are typically invoking a single routine) */
    auto macroString = macro + QLatin1Char('\n');

	// Do a test parse 
    Program *const prog = ParseMacroEx(macroString, &errMsg, &stoppedAt);

	if(!prog) {        
        ParseErrorEx(nullptr, macroString, stoppedAt, QLatin1String("argument to -do"), errMsg);
        return false;
	}

	FreeProgram(prog);
    return true;
}
