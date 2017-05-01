/*******************************************************************************
*                                                                              *
* macro.h -- Nirvana Editor Macro Header File                                  *
*                                                                              *
* Copyright 2004 The NEdit Developers                                          *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute versions of this program linked to  *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for    *
* more details.                                                                *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* July 31, 2001                                                                *
*                                                                              *
*******************************************************************************/

#ifndef MACRO_H_
#define MACRO_H_

#include "nedit.h"
#include "util/string_view.h"
#include <QFuture>

class DocumentWidget;
class MainWindow;
class Program;
class QString;
class QTimer;
class QWidget;
struct RestartData;

#define REPEAT_TO_END -1
#define REPEAT_IN_SEL -2

Program *ParseMacroEx(const QString &expr, QString *message, int *stoppedAt);
bool CheckMacroStringEx(QWidget *dialogParent, const QString &string, const QString &errIn, int *errPos);
int MacroWindowCloseActionsEx(DocumentWidget *document);
int ReadMacroFileEx(DocumentWidget *window, const std::string &fileName, int warnNotExist);

int ReadMacroStringEx(DocumentWidget *window, const QString &string, const char *errIn);
void AbortMacroCommandEx(DocumentWidget *document);
void CancelMacroOrLearnEx(DocumentWidget *document);
void DoMacroEx(DocumentWidget *document, const QString &macro, const char *errInName);
void FinishLearnEx();
void ReplayEx(DocumentWidget *window);

void ReadMacroInitFileEx(DocumentWidget *window);
void RegisterMacroSubroutines();
void RepeatMacroEx(DocumentWidget *window, const char *command, int how);
void ResumeMacroExecutionEx(DocumentWidget *window);
void ReturnShellCommandOutputEx(DocumentWidget *window, const std::string &outText, int status);
void SafeGC();

/* Data attached to window during shell command execution with
   information for controling and communicating with the process */
struct macroCmdInfoEx {
    QTimer *bannerTimeoutID;
    Program *program;
    QFuture<bool> continueWorkProcID;
    bool bannerIsUp;
    bool closeOnCompletion;
    RestartData *context;
};

#endif
