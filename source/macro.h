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

class QString;
class QWidget;
class Program;

#define REPEAT_TO_END -1
#define REPEAT_IN_SEL -2

Program *ParseMacroEx(const QString &expr, int index, QString *message, int *stoppedAt);
bool CheckMacroStringEx(QWidget *dialogParent, const QString &string, const QString &errIn, int *errPos);
int CheckMacroString(Widget dialogParent, const char *string, const char *errIn, const char **errPos);
int MacroWindowCloseActions(Document *window);
int ReadMacroFileEx(Document *window, const std::string &fileName, int warnNotExist);
int ReadMacroString(Document *window, const char *string, const char *errIn);
int ReadMacroStringEx(Document *window, const QString &string, const char *errIn);
std::string GetReplayMacro();
void AbortMacroCommand(Document *window);
void AddLastCommandActionHook(XtAppContext context);
void BeginLearn(Document *window);
void CancelMacroOrLearn(Document *window);
void DoMacro(Document *window, view::string_view macro, const char *errInName);
void FinishLearn();
void ReadMacroInitFile(Document *window);
void RegisterMacroSubroutines();
void RepeatDialog(Document *window);
void RepeatMacro(Document *window, const char *command, int how);
void Replay(Document *window);
void ResumeMacroExecution(Document *window);
void ReturnShellCommandOutput(Document *window, const std::string &outText, int status);
void SafeGC();

extern std::string ReplayMacro;

#endif
