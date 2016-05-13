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
#include "string_view.h"

class QString;
class QWidget;
class Program;

#define REPEAT_TO_END -1
#define REPEAT_IN_SEL -2

void RegisterMacroSubroutines(void);
void AddLastCommandActionHook(XtAppContext context);
void BeginLearn(Document *window);
void FinishLearn(void);
void CancelMacroOrLearn(Document *window);
void Replay(Document *window);
void SafeGC(void);
void DoMacro(Document *window, view::string_view macro, const char *errInName);
void ResumeMacroExecution(Document *window);
void AbortMacroCommand(Document *window);
int MacroWindowCloseActions(Document *window);
void RepeatDialog(Document *window);
void RepeatMacro(Document *window, const char *command, int how);
int ReadMacroFileEx(Document *window, const std::string &fileName, int warnNotExist);
int ReadMacroString(Document *window, const char *string, const char *errIn);
int CheckMacroString(Widget dialogParent, const char *string, const char *errIn, const char **errPos);
bool CheckMacroStringEx(QWidget *dialogParent, const QString &string, const QString &errIn, int *errPos);
std::string GetReplayMacro(void);
void ReadMacroInitFile(Document *window);
void ReturnShellCommandOutput(Document *window, const std::string &outText, int status);
Program *ParseMacroEx(const QString &expr, int index, QString *message, int *stoppedAt);

#endif
