/*******************************************************************************
*                                                                              *
* smartIndent.h -- Nirvana Editor Smart Indent Header File                     *
*                                                                              *
* Copyright 2003 The NEdit Developers                                          *
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

#ifndef X_SMART_INDENT_H_
#define X_SMART_INDENT_H_

#include "preferences.h"
#include "util/string_view.h"

class QString;
class QByteArray;
class SmartIndentEntry;
class DocumentWidget;

bool InSmartIndentMacrosEx(DocumentWidget *document);
int LMHasSmartIndentMacros(const QString &languageMode);
int LoadSmartIndentCommonStringEx(const QString &string);
int LoadSmartIndentStringEx(const QString &string);
int SmartIndentMacrosAvailable(const QString &languageMode);
QString  WriteSmartIndentCommonStringEx();
QString WriteSmartIndentStringEx();
void EditCommonSmartIndentMacro();
void EndSmartIndentEx(DocumentWidget *window);
void RenameSmartIndentMacros(const QString &oldName, const QString &newName);
void UpdateLangModeMenuSmartIndent();
QByteArray defaultCommonMacros();
const SmartIndentEntry *findIndentSpec(const QString &modeName);

extern QString CommonMacros;

#define N_DEFAULT_INDENT_SPECS 4

extern SmartIndentEntry DefaultIndentSpecs[N_DEFAULT_INDENT_SPECS];
extern QList<SmartIndentEntry> SmartIndentSpecs;

class Program;

struct SmartIndentData {
    Program *newlineMacro;
    int inNewLineMacro;
    Program *modMacro;
    int inModMacro;
};

#endif 
