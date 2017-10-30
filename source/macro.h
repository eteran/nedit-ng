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
#include <QTimer>
#include <memory>

class DocumentWidget;
class MainWindow;
class Program;
class QString;
class QWidget;
struct RestartData;

enum RepeatMethod {
    REPEAT_TO_END = -1,
    REPEAT_IN_SEL = -2,
};


Program *ParseMacroEx(const QString &expr, QString *message, int *stoppedAt);
bool CheckMacroStringEx(QWidget *dialogParent, const QString &string, const QString &errIn, int *errPos);
int MacroWindowCloseActionsEx(DocumentWidget *document);
int readCheckMacroStringEx(QWidget *dialogParent, const QString &string, DocumentWidget *runWindow, const QString &errIn, int *errPos);
int ReadMacroStringEx(DocumentWidget *document, const QString &string, const QString &errIn);
void AbortMacroCommandEx(DocumentWidget *document);
void CancelMacroOrLearnEx(DocumentWidget *document);
void DoMacroEx(DocumentWidget *document, const QString &macro, const QString &errInName);
void FinishLearnEx();
void ReplayEx(DocumentWidget *document);

void ReadMacroInitFileEx(DocumentWidget *document);
void RegisterMacroSubroutines();
void RepeatMacroEx(DocumentWidget *document, const QString &command, int how);
void ReturnShellCommandOutputEx(DocumentWidget *document, const QString &outText, int status);
void SafeGC();

/* Data attached to window during shell command execution with
   information for controling and communicating with the process */
struct MacroCommandData {
    QTimer                       bannerTimer;
    QTimer                       continuationTimer;
    Program *                    program           = nullptr;
    bool                         bannerIsUp        = false;
    bool                         closeOnCompletion = false;
    std::shared_ptr<RestartData> context;
};

#endif
