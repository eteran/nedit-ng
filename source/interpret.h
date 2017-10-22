/*******************************************************************************
*                                                                              *
* interpret.h -- Nirvana Editor Interpreter Header File                        *
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

#ifndef INTERPRET_H_
#define INTERPRET_H_

#include "nedit.h"
#include "rbTree.h"
#include "util/string_view.h"
#include <QList>
#include <map>
#include <memory>

class DocumentWidget;
class Program;
class QString;
struct ArrayEntry;
struct DataValue;
struct Symbol;

#define STACK_SIZE         1024  // Maximum stack size
#define MAX_SYM_LEN        100   // Max. symbol name length
#define MACRO_EVENT_MARKER 2     // Special value for the send_event field of events passed to action routines.  Tells them that they were called from a macro

enum SymTypes {
    CONST_SYM,
    GLOBAL_SYM,
    LOCAL_SYM,
    ARG_SYM,
    PROC_VALUE_SYM,
    C_FUNCTION_SYM,
	MACRO_FUNCTION_SYM
};

#define N_OPS 43
enum Operations {
	OP_RETURN_NO_VAL,
	OP_RETURN,
	OP_PUSH_SYM,
	OP_DUP,
	OP_ADD,
	OP_SUB,
	OP_MUL,
	OP_DIV,
	OP_MOD,
	OP_NEGATE,
	OP_INCR,
	OP_DECR,
	OP_GT,
	OP_LT,
	OP_GE,
	OP_LE,
	OP_EQ,
	OP_NE,
	OP_BIT_AND,
	OP_BIT_OR,
	OP_AND,
	OP_OR,
	OP_NOT,
	OP_POWER,
	OP_CONCAT,
	OP_ASSIGN,
	OP_SUBR_CALL,
	OP_FETCH_RET_VAL,
	OP_BRANCH,
	OP_BRANCH_TRUE,
	OP_BRANCH_FALSE,
	OP_BRANCH_NEVER,
	OP_ARRAY_REF,
	OP_ARRAY_ASSIGN,
	OP_BEGIN_ARRAY_ITER,
	OP_ARRAY_ITER,
	OP_IN_ARRAY,
	OP_ARRAY_DELETE,
	OP_PUSH_ARRAY_SYM,
	OP_ARRAY_REF_ASSIGN_SETUP,
	OP_PUSH_ARG,
	OP_PUSH_ARG_COUNT,
	OP_PUSH_ARG_ARRAY
};

enum ExecReturnCodes {
	MACRO_TIME_LIMIT, 
	MACRO_PREEMPT, 
	MACRO_DONE, 
	MACRO_ERROR
};

#define ARRAY_DIM_SEP "\034"

union Inst {
	int (*func)();
	int value;
	Symbol *sym;
};

using BuiltInSubrEx = bool (*)(DocumentWidget *document, struct DataValue *argList, int nArgs, struct DataValue *result, const char **errMsg);

struct NString {
	char *rep;
	size_t len;
};


// NOTE(eteran): we can eventually replace this with boost::variant,
// but first we need to figure out what to do about the fact that half of these
// entries get used with a "NO_TAG" type.

enum TypeTags {
    NO_TAG,
    INT_TAG,
    STRING_TAG,
    ARRAY_TAG
};

struct DataValue {
	TypeTags tag;
	
	union {
        int            n;
        NString        str;
        ArrayEntry*    arrayPtr;

        BuiltInSubrEx  subr;    // no tag?
        Program*       prog;    // no tag?
        Inst*          inst;    // no tag?
        DataValue*     dataval; // no tag?

	} val;
};

#define INIT_DATA_VALUE {NO_TAG, {0}}

//------------------------------------------------------------------------------
struct ArrayEntry : public rbTreeNode {
    char *key;
	DataValue value;
	bool      inUse; /* we use pointers to the data to refer to the entire struct */
};

/* symbol table entry */
struct Symbol {
	std::string name;
	SymTypes    type;
	DataValue   value;
};

class Program {
public:
    QList<Symbol *> localSymList;
	Inst *code;
};

/* Information needed to re-start a preempted macro */
struct RestartData {
    DataValue *stack            = nullptr;
    DataValue *stackP           = nullptr;
    DataValue *frameP           = nullptr;
    Inst *pc                    = nullptr;
    DocumentWidget *runWindow   = nullptr;
    DocumentWidget *focusWindow = nullptr;
};

void InitMacroGlobals();

ArrayEntry *arrayIterateFirst(DataValue *theArray);
ArrayEntry *arrayIterateNext(ArrayEntry *iterator);
ArrayEntry *ArrayNew();
bool ArrayInsert(DataValue *theArray, char *keyStr, DataValue *theValue);
void ArrayDelete(DataValue *theArray, char *keyStr);
void ArrayDeleteAll(DataValue *theArray);
unsigned ArraySize(DataValue *theArray);
bool ArrayGet(DataValue *theArray, char *keyStr, DataValue *theValue);
int ArrayCopy(DataValue *dstArray, DataValue *srcArray);

/* Routines for creating a program, (accumulated beginning with
   BeginCreatingProgram and returned via FinishCreatingProgram) */
void BeginCreatingProgram();
int AddOp(int op, const char **msg);
int AddSym(Symbol *sym, const char **msg);
int AddImmediate(int value, const char **msg);
int AddBranchOffset(Inst *to, const char **msg);
Inst *GetPC();
Symbol *InstallIteratorSymbol();
Symbol *LookupStringConstSymbol(const char *value);
Symbol *InstallStringConstSymbol(const char *str);
Symbol *LookupSymbol(view::string_view name);
Symbol *LookupSymbolEx(const QString &name);
Symbol *InstallSymbol(const std::string &name, SymTypes type, const DataValue &value);
Symbol *InstallSymbolEx(const QString &name, enum SymTypes type, const DataValue &value);
Program *FinishCreatingProgram();
void SwapCode(Inst *start, Inst *boundary, Inst *end);
void StartLoopAddrList();
int AddBreakAddr(Inst *addr);
int AddContinueAddr(Inst *addr);
void FillLoopAddrs(Inst *breakAddr, Inst *continueAddr);

/* create a permanently allocated static string (only for use with static strings) */
#define PERM_ALLOC_STR(xStr) ((const_cast<char *>("\001" xStr)) + 1)

/* Routines for executing programs */
int ExecuteMacroEx(DocumentWidget *window, Program *prog, int nArgs, DataValue *args, DataValue *result, std::shared_ptr<RestartData> &continuation, const char **msg);
int ContinueMacroEx(const std::shared_ptr<RestartData> &continuation, DataValue *result, const char **msg);
void RunMacroAsSubrCall(Program *prog);
void PreemptMacro();

char *AllocString(int length);
int AllocNString(NString *string, int length);

char *AllocStringCpyEx(const std::string &s);
NString AllocNStringCpyEx(const QString &s);
NString AllocNStringCpyEx(const std::string &s);
NString AllocNStringCpyEx(const view::string_view s);
void GarbageCollectStrings();
void FreeRestartDataEx(const std::shared_ptr<RestartData> &context);
Symbol *PromoteToGlobal(Symbol *sym);
void FreeProgram(Program *prog);
void ModifyReturnedValueEx(const std::shared_ptr<RestartData> &context, const DataValue &dv);
DocumentWidget *MacroRunWindowEx();
DocumentWidget *MacroFocusWindowEx();
void SetMacroFocusWindowEx(DocumentWidget *window);

/* function used for implicit conversion from string to number */
bool StringToNum(const char *string, int *number);
bool StringToNum(const std::string &string, int *number);
bool StringToNum(const QString &string, int *number);


struct array_empty {};
struct array_new   {};

inline DataValue to_value(const array_empty &) {
    DataValue DV;
    DV.tag          = ARRAY_TAG;
    DV.val.arrayPtr = nullptr;
    return DV;
}

inline DataValue to_value(const array_new &) {
    DataValue DV;
    DV.tag          = ARRAY_TAG;
    DV.val.arrayPtr = ArrayNew();
    return DV;
}

constexpr inline DataValue to_value() {
    DataValue DV = INIT_DATA_VALUE;
    return DV;
}

inline DataValue to_value(int n) {
    DataValue DV;
    DV.tag   = INT_TAG;
    DV.val.n = n;
    return DV;
}

inline DataValue to_value(bool n) {
    DataValue DV;
    DV.tag   = INT_TAG;
    DV.val.n = n ? 1 : 0;
    return DV;
}

// TODO(eteran): 2.0, deprecate this API in favor of std::string/QString
inline DataValue to_value(const char *str) {
    DataValue DV;
    DV.tag     = STRING_TAG;
    DV.val.str = AllocNStringCpyEx(view::string_view(str));
    return DV;
}

inline DataValue to_value(const std::string &str) {
    DataValue DV;
    DV.tag     = STRING_TAG;
    DV.val.str = AllocNStringCpyEx(str);
    return DV;
}

inline DataValue to_value(const QString &str) {
    DataValue DV;
    DV.tag     = STRING_TAG;
    DV.val.str = AllocNStringCpyEx(str);
    return DV;
}

#endif
