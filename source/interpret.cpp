/*******************************************************************************
*                                                                              *
* interpret.c -- Nirvana Editor macro interpreter                              *
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
* April, 1997                                                                  *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/

#include "interpret.h"
#include "DocumentWidget.h"
#include "menu.h"
#include <cmath>

namespace {

constexpr const int PROGRAM_SIZE      = 4096; // Maximum program size
constexpr const int MAX_ERR_MSG_LEN   = 256;  // Max. length for error messages
constexpr const int LOOP_STACK_SIZE   = 200;  // (Approx.) Number of break/continue stmts allowed per program
constexpr const int INSTRUCTION_LIMIT = 100;  // Number of instructions the interpreter is allowed to execute before preempting and returning to allow other things to run

/* Temporary markers placed in a branch address location to designate
   which loop address (break or continue) the location needs */

constexpr const int NEEDS_BREAK    = 1;
constexpr const int NEEDS_CONTINUE = 2;

constexpr const int N_ARGS_ARG_SYM = -1; // special arg number meaning $n_args value

enum OpStatusCodes {
	STAT_OK = 2, 
	STAT_DONE, 
	STAT_ERROR, 
	STAT_PREEMPT
};

}

static void addLoopAddr(Inst *addr);

static void saveContextEx(RestartData *context);
static void restoreContextEx(RestartData *context);
static NString AllocNStringEx(int length);

static int returnNoVal();
static int returnVal();
static int returnValOrNone(bool valOnStack);
static int pushSymVal();
static int pushArgVal();
static int pushArgCount();
static int pushArgArray();
static int pushArraySymVal();
static int dupStack();
static int add();
static int subtract();
static int multiply();
static int divide();
static int modulo();
static int negate();
static int increment();
static int decrement();
static int gt();
static int lt();
static int ge();
static int le();
static int eq();
static int ne();
static int bitAnd();
static int bitOr();
static int logicalAnd();
static int logicalOr();
static int logicalNot();
static int power();
static int concat();
static int assign();
static int callSubroutine();
static int fetchRetVal();
static int branch();
static int branchTrue();
static int branchFalse();
static int branchNever();
static int arrayRef();
static int arrayAssign();
static int arrayRefAndAssignSetup();
static int beginArrayIter();
static int arrayIter();
static int inArray();
static int deleteArrayElement();
static int errCheck(const char *s);

template <class ...T>
int execError(const char *s1, T ... args);

static ArrayEntry *allocateSparseArrayEntry();
static rbTreeNode *arrayEmptyAllocator();
static rbTreeNode *arrayAllocateNode(rbTreeNode *src);
static int arrayEntryCopyToNode(rbTreeNode *dst, rbTreeNode *src);
static int arrayEntryCompare(rbTreeNode *left, rbTreeNode *right);
static void arrayDisposeNode(rbTreeNode *src);

// #define DEBUG_ASSEMBLY
// #define DEBUG_STACK

#if defined(DEBUG_ASSEMBLY) || defined(DEBUG_STACK)
#define DEBUG_DISASSEMBLER
static void disasm(Inst *inst, int nInstr);
#endif

#ifdef DEBUG_ASSEMBLY // for disassembly 
#define DISASM(i, n) disasm(i, n)
#else
#define DISASM(i, n)
#endif

#ifdef DEBUG_STACK // for run-time instruction and stack trace 
static void stackdump(int n, int extra);
#define STACKDUMP(n, x) stackdump(n, x)
#define DISASM_RT(i, n) disasm(i, n)
#else
#define STACKDUMP(n, x)
#define DISASM_RT(i, n)
#endif

// Global symbols and function definitions 
static QList<Symbol *> GlobalSymList;

// List of all memory allocated for strings 
static QList<char *> AllocatedStrings;

static QList<ArrayEntry *> AllocatedSparseArrayEntries;

/* Message strings used in macros (so they don't get repeated every time
   the macros are used */
constexpr static const char StackOverflowMsg[]  = "macro stack overflow";
constexpr static const char StackUnderflowMsg[] = "macro stack underflow";
constexpr static const char StringToNumberMsg[] = "string could not be converted to number";

// Temporary global data for use while accumulating programs
static QList<Symbol *> LocalSymList; // symbols local to the program
static Inst Prog[PROGRAM_SIZE];          // the program
static Inst *ProgP;                      // next free spot for code gen.
static Inst *LoopStack[LOOP_STACK_SIZE]; // addresses of break, cont stmts
static Inst **LoopStackPtr = LoopStack;  //  to fill at the end of a loop

// Global data for the interpreter
static DataValue *TheStack;                    // the stack
static DataValue *StackP;                      // next free spot on stack
static DataValue *FrameP;                      // frame pointer (start of local variables for the current subroutine invocation)
static Inst *PC;                               // program counter during execution
static char *ErrMsg;                           // global for returning error messages from executing functions

static DocumentWidget *InitiatingWindowEx = nullptr;   // window from which macro was run
static DocumentWidget *FocusWindowEx;                  // window on which macro commands operate

static bool PreemptRequest;                    // passes preemption requests from called routines back up to the interpreter

/* Array for mapping operations to functions for performing the operations
   Must correspond to the enum called "operations" in interpret.h */
static int (*OpFns[N_OPS])() = {returnNoVal, returnVal,      pushSymVal, dupStack, add,                subtract,        multiply,               divide,     modulo,       negate,      increment,
                                decrement,   gt,             lt,         ge,       le,                 eq,              ne,                     bitAnd,     bitOr,        logicalAnd,  logicalOr,
                                logicalNot,  power,          concat,     assign,   callSubroutine,     fetchRetVal,     branch,                 branchTrue, branchFalse,  branchNever, arrayRef,
                                arrayAssign, beginArrayIter, arrayIter,  inArray,  deleteArrayElement, pushArraySymVal, arrayRefAndAssignSetup, pushArgVal, pushArgCount, pushArgArray};

// Stack-> symN-sym0(FP), argArray, nArgs, oldFP, retPC, argN-arg1, next, ... 
#define FP_ARG_ARRAY_CACHE_INDEX (-1)
#define FP_ARG_COUNT_INDEX       (-2)
#define FP_OLD_FP_INDEX          (-3)
#define FP_RET_PC_INDEX          (-4)
#define FP_TO_ARGS_DIST           (4) // should be 0 - (above index) 

#define FP_GET_ITEM(xFrameP, xIndex) (*(xFrameP + xIndex))

#define FP_GET_ARG_ARRAY_CACHE(xFrameP) (FP_GET_ITEM(xFrameP, FP_ARG_ARRAY_CACHE_INDEX))
#define FP_GET_ARG_COUNT(xFrameP)       (FP_GET_ITEM(xFrameP, FP_ARG_COUNT_INDEX).val.n)
#define FP_GET_OLD_FP(xFrameP)          (FP_GET_ITEM(xFrameP, FP_OLD_FP_INDEX).val.dataval)
#define FP_GET_RET_PC(xFrameP)          (FP_GET_ITEM(xFrameP, FP_RET_PC_INDEX).val.inst)

#define FP_ARG_START_INDEX(xFrameP)     (-(FP_GET_ARG_COUNT(xFrameP) + FP_TO_ARGS_DIST))
#define FP_GET_ARG_N(xFrameP, xN) (FP_GET_ITEM(xFrameP, xN + FP_ARG_START_INDEX(xFrameP)))
#define FP_GET_SYM_N(xFrameP, xN) (FP_GET_ITEM(xFrameP, xN))
#define FP_GET_SYM_VAL(xFrameP, xSym) (FP_GET_SYM_N(xFrameP, xSym->value.val.n))

/*
** Initialize macro language global variables.  Must be called before
** any macros are even parsed, because the parser uses action routine
** symbols to comprehend hyphenated names.
*/
void InitMacroGlobals() {

	static char argName[3] = "$x";
	static DataValue dv = INIT_DATA_VALUE;

	// Add action routines from NEdit menus and text widget 
#if 0 // NOTE(eteran): we are replacing these with wrappers around Q_SLOTS
    XtActionsRec *actions;
    int nActions;

	actions = GetMenuActions(&nActions);
	for (i = 0; i < nActions; i++) {
		dv.val.xtproc = actions[i].proc;
		InstallSymbol(actions[i].string, ACTION_ROUTINE_SYM, dv);
	}
	actions = TextGetActions(&nActions);
	for (i = 0; i < nActions; i++) {
		dv.val.xtproc = actions[i].proc;
		InstallSymbol(actions[i].string, ACTION_ROUTINE_SYM, dv);
	}
#endif

	// Add subroutine argument symbols ($1, $2, ..., $9) 
    for (int i = 0; i < 9; i++) {
        argName[1] = static_cast<char>('1' + i);
		dv.val.n = i;
		InstallSymbol(argName, ARG_SYM, dv);
	}

	// Add special symbol $n_args 
	dv.val.n = N_ARGS_ARG_SYM;
	InstallSymbol("$n_args", ARG_SYM, dv);
}

/*
** To build a program for the interpreter, call BeginCreatingProgram, to
** begin accumulating the program, followed by calls to AddOp, AddSym,
** and InstallSymbol to add symbols and operations.  When the new program
** is finished, collect the results with FinishCreatingProgram.  This returns
** a self contained program that can be run with ExecuteMacro.
*/

/*
** Start collecting instructions for a program. Clears the program
** and the symbol table.
*/
void BeginCreatingProgram() {
	LocalSymList.clear();
	ProgP        = Prog;
	LoopStackPtr = LoopStack;
}

/*
** Finish up the program under construction, and return it (code and
** symbol table) as a package that ExecuteMacro can execute.  This
** program must be freed with FreeProgram.
*/
Program *FinishCreatingProgram() {

	int fpOffset = 0;

	auto newProg      = new Program;
	ptrdiff_t progLen = reinterpret_cast<char *>(ProgP) - reinterpret_cast<char *>(Prog);
	size_t count      = progLen / sizeof(Inst);
	newProg->code     = new Inst[count];

	std::copy_n(Prog, count, newProg->code);

    newProg->localSymList = LocalSymList;
    LocalSymList = QList<Symbol *>();

	/* Local variables' values are stored on the stack.  Here we assign
	   frame pointer offsets to them. */
	for(Symbol *s : newProg->localSymList) {
		s->value.val.n = fpOffset++;
	}

	DISASM(newProg->code, ProgP - Prog);

	return newProg;
}

void FreeProgram(Program *prog) {
	qDeleteAll(prog->localSymList);
	delete [] prog->code;
	delete prog;
}

/*
** Add an operator (instruction) to the end of the current program
*/
int AddOp(int op, const char **msg) {
	if (ProgP >= &Prog[PROGRAM_SIZE]) {
		*msg = "macro too large";
		return 0;
	}
	ProgP->func = OpFns[op];
	ProgP++;
	return 1;
}

/*
** Add a symbol operand to the current program
*/
int AddSym(Symbol *sym, const char **msg) {
	if (ProgP >= &Prog[PROGRAM_SIZE]) {
		*msg = "macro too large";
		return 0;
	}
	ProgP->sym = sym;
	ProgP++;
	return 1;
}

/*
** Add an immediate value operand to the current program
*/
int AddImmediate(int value, const char **msg) {
	if (ProgP >= &Prog[PROGRAM_SIZE]) {
		*msg = "macro too large";
		return 0;
	}
	ProgP->value = value;
	ProgP++;
	return 1;
}

/*
** Add a branch offset operand to the current program
*/
int AddBranchOffset(Inst *to, const char **msg) {
	if (ProgP >= &Prog[PROGRAM_SIZE]) {
		*msg = "macro too large";
		return 0;
	}
	// Should be ptrdiff_t for branch offsets 
	ProgP->value = to - ProgP;
	ProgP++;

	return 1;
}

/*
** Return the address at which the next instruction will be stored
*/
Inst *GetPC() {
	return ProgP;
}

/*
** Swap the positions of two contiguous blocks of code.  The first block
** running between locations start and boundary, and the second between
** boundary and end.
*/
void SwapCode(Inst *start, Inst *boundary, Inst *end) {

	// double-reverse method: reverse elements of both parts then whole lot 
    // eg abcdeABCD -1-> edcbaABCD -2-> edcbaDCBA -3-> DCBAedcba
    std::reverse(start, boundary); // 1
    std::reverse(boundary, end);   // 2
    std::reverse(start, end);      // 3
}

/*
** Maintain a stack to save addresses of branch operations for break and
** continue statements, so they can be filled in once the information
** on where to branch is known.
**
** Call StartLoopAddrList at the beginning of a loop, AddBreakAddr or
** AddContinueAddr to register the address at which to store the branch
** address for a break or continue statement, and FillLoopAddrs to fill
** in all the addresses and return to the level of the enclosing loop.
*/
void StartLoopAddrList() {
	addLoopAddr(nullptr);
}

int AddBreakAddr(Inst *addr) {
	if (LoopStackPtr == LoopStack)
		return 1;
	addLoopAddr(addr);
	addr->value = NEEDS_BREAK;
	return 0;
}

int AddContinueAddr(Inst *addr) {
	if (LoopStackPtr == LoopStack)
		return 1;
	addLoopAddr(addr);
	addr->value = NEEDS_CONTINUE;
	return 0;
}

static void addLoopAddr(Inst *addr) {
	if (LoopStackPtr > &LoopStack[LOOP_STACK_SIZE - 1]) {
		qCritical("NEdit: loop stack overflow in macro parser");
		return;
	}
	*LoopStackPtr++ = addr;
}

void FillLoopAddrs(Inst *breakAddr, Inst *continueAddr) {
	while (true) {
		LoopStackPtr--;
		if (LoopStackPtr < LoopStack) {
			qCritical("NEdit: internal error (lsu) in macro parser");
			return;
		}
		if (*LoopStackPtr == nullptr)
			break;
		if ((*LoopStackPtr)->value == NEEDS_BREAK)
			(*LoopStackPtr)->value = breakAddr - *LoopStackPtr;
		else if ((*LoopStackPtr)->value == NEEDS_CONTINUE)
			(*LoopStackPtr)->value = continueAddr - *LoopStackPtr;
		else
			qCritical("NEdit: internal error (uat) in macro parser");
	}
}

/*
** Execute a compiled macro, "prog", using the arguments in the array
** "args".  Returns one of MACRO_DONE, MACRO_PREEMPT, or MACRO_ERROR.
** if MACRO_DONE is returned, the macro completed, and the returned value
** (if any) can be read from "result".  If MACRO_PREEMPT is returned, the
** macro exceeded its alotted time-slice and scheduled...
*/
int ExecuteMacroEx(DocumentWidget *window, Program *prog, int nArgs, DataValue *args, DataValue *result, RestartData **continuation, const char **msg) {

    static DataValue noValue = INIT_DATA_VALUE;
    int i;

    /* Create an execution context (a stack, a stack pointer, a frame pointer,
       and a program counter) which will retain the program state across
       preemption and resumption of execution */
    auto context         = new RestartData;
    context->stack       = new DataValue[STACK_SIZE];
    *continuation        = context;
    context->stackP      = context->stack;
    context->pc          = prog->code;
    context->runWindow   = window;
    context->focusWindow = window;

    // Push arguments and call information onto the stack
    for (i = 0; i < nArgs; i++) {
        *(context->stackP++) = args[i];
    }

    context->stackP->val.subr = nullptr; // return PC
    context->stackP->tag = NO_TAG;
    context->stackP++;

    *(context->stackP++) = noValue; // old FrameP

    context->stackP->tag = NO_TAG; // nArgs
    context->stackP->val.n = nArgs;
    context->stackP++;

    *(context->stackP++) = noValue; // cached arg array

    context->frameP = context->stackP;

    // Initialize and make room on the stack for local variables
    for(Symbol *s : prog->localSymList) {
        FP_GET_SYM_VAL(context->frameP, s) = noValue;
        context->stackP++;
    }

    // Begin execution, return on error or preemption
    return ContinueMacroEx(context, result, msg);
}

/*
** Continue the execution of a suspended macro whose state is described in
** "continuation"
*/
int ContinueMacroEx(RestartData *continuation, DataValue *result, const char **msg) {

	int instCount = 0;
    RestartData oldContext;

    /* To allow macros to be invoked arbitrarily (such as those automatically
       triggered within smart-indent) within executing macros, this call is
       reentrant. */
    saveContextEx(&oldContext);

    /*
    ** Execution Loop:  Call the succesive routine addresses in the program
    ** until one returns something other than STAT_OK, then take action
    */
    restoreContextEx(continuation);
    ErrMsg = nullptr;
    for (;;) {

        // Execute an instruction
		Inst *inst = PC++;

		int status = (inst->func)();

        // If error return was not STAT_OK, return to caller
        switch(status) {
        case STAT_PREEMPT:
            saveContextEx(continuation);
            restoreContextEx(&oldContext);
            return MACRO_PREEMPT;
        case STAT_ERROR:
            *msg = ErrMsg;
            FreeRestartDataEx(continuation);
            restoreContextEx(&oldContext);
            return MACRO_ERROR;
        case STAT_DONE:
            *msg = "";
            *result = *--StackP;
            FreeRestartDataEx(continuation);
            restoreContextEx(&oldContext);
            return MACRO_DONE;
        case STAT_OK:
        default:
            break;
        }

        /* Count instructions executed.  If the instruction limit is hit,
           preempt, store re-start information in continuation and give
           X, other macros, and other shell scripts a chance to execute */
		++instCount;
        if (instCount >= INSTRUCTION_LIMIT) {
            saveContextEx(continuation);
            restoreContextEx(&oldContext);
            return MACRO_TIME_LIMIT;
        }
    }
}

/*
** If a macro is already executing, and requests that another macro be run,
** this can be called instead of ExecuteMacro to run it in the same context
** as if it were a subroutine.  This saves the caller from maintaining
** separate contexts, and serializes processing of the two macros without
** additional work.
*/
void RunMacroAsSubrCall(Program *prog) {
	static DataValue noValue = INIT_DATA_VALUE;

	/* See subroutine "callSubroutine" for a description of the stack frame
	   for a subroutine call */
	StackP->tag = NO_TAG;
	StackP->val.inst = PC; // return PC 
	StackP++;

	StackP->tag = NO_TAG;
	StackP->val.dataval = FrameP; // old FrameP 
	StackP++;

	StackP->tag = NO_TAG; // nArgs 
	StackP->val.n = 0;
	StackP++;

	*(StackP++) = noValue; // cached arg array 

	FrameP = StackP;
	PC = prog->code;
	for(Symbol *s : prog->localSymList) {
		FP_GET_SYM_VAL(FrameP, s) = noValue;
		StackP++;
	}
}

void FreeRestartDataEx(RestartData *context) {
    delete [] context->stack;
    delete context;
}

/*
** Cause a macro in progress to be preempted (called by commands which take
** a long time, or want to return to the event loop.  Call ResumeMacroExecution
** to resume.
*/
void PreemptMacro() {
	PreemptRequest = true;
}

/*
** Reset the return value for a subroutine which caused preemption (this is
** how to return a value from a routine which preempts instead of returning
** a value directly).
*/
void ModifyReturnedValueEx(RestartData *context, const DataValue &dv) {
	if ((context->pc - 1)->func == fetchRetVal) {
        *(context->stackP - 1) = dv;
	}
}

/*
** Called within a routine invoked from a macro, returns the window in
** which the macro is executing (where the banner is, not where it is focused)
*/
DocumentWidget *MacroRunWindowEx() {
    return InitiatingWindowEx;
}

/*
** Called within a routine invoked from a macro, returns the window to which
** the currently executing macro is focused (the window which macro commands
** modify, not the window from which the macro is being run)
*/
DocumentWidget *MacroFocusWindowEx() {
    return FocusWindowEx;
}

/*
** Set the window to which macro subroutines and actions which operate on an
** implied window are directed.
*/
void SetMacroFocusWindowEx(DocumentWidget *window) {
    FocusWindowEx = window;
}

/*
** install an array iteration symbol
** it is tagged as an integer but holds an array node pointer
*/
#define ARRAY_ITER_SYM_PREFIX "aryiter "
Symbol *InstallIteratorSymbol() {

	static int interatorNameIndex = 0;

	auto symbolName = ARRAY_ITER_SYM_PREFIX + std::to_string(interatorNameIndex++);

	DataValue value;
	value.tag = INT_TAG;
	value.val.arrayPtr = nullptr;

    return InstallSymbol(symbolName, LOCAL_SYM, value);
}

/*
** Lookup a constant string by its value. This allows reuse of string
** constants and fixing a leak in the interpreter.
*/
Symbol *LookupStringConstSymbol(const char *value) {

	for(Symbol *s: GlobalSymList) {
		if (s->type == CONST_SYM && s->value.tag == STRING_TAG && !strcmp(s->value.val.str.rep, value)) {
			return s;
		}
	}
	return nullptr;
}

/*
** install string str in the global symbol table with a string name
*/
#define ARRAY_STRING_CONST_SYM_PREFIX "string #"
Symbol *InstallStringConstSymbol(const char *str) {

    static int stringConstIndex = 0;

    if (Symbol *sym = LookupStringConstSymbol(str)) {
		return sym;
	}

	auto stringName = ARRAY_STRING_CONST_SYM_PREFIX + std::to_string(stringConstIndex++);

	DataValue value;
    value.tag = STRING_TAG;
    AllocNStringCpy(&value.val.str, str);
    return InstallSymbol(stringName, CONST_SYM, value);
}

/*
** find a symbol in the symbol table
*/
Symbol *LookupSymbol(view::string_view name) {

	for(Symbol *s : LocalSymList) {
		if (s->name == name) {
			return s;
		}
	}

	for(Symbol *s: GlobalSymList) {
		if (s->name == name) {
			return s;
		}
	}

	return nullptr;
}

/*
** install symbol name in symbol table
*/
Symbol *InstallSymbol(const std::string &name, enum SymTypes type, const DataValue &value) {

	auto s = new Symbol;
	s->name  = name;
	s->type  = type;
	s->value = value;

	if (type == LOCAL_SYM) {
		LocalSymList.push_front(s);
	} else {
		GlobalSymList.push_back(s);
	}
	return s;
}

/*
** Promote a symbol from local to global, removing it from the local symbol
** list.
**
** This is used as a forward declaration feature for macro functions.
** If a function is called (ie while parsing the macro) where the
** function isn't defined yet, the symbol is put into the GlobalSymList
** so that the function definition uses the same symbol.
**
*/
Symbol *PromoteToGlobal(Symbol *sym) {

	if (sym->type != LOCAL_SYM) {
		return sym;
	}

	// Remove sym from the local symbol list 
	LocalSymList.erase(std::remove(LocalSymList.begin(), LocalSymList.end(), sym), LocalSymList.end());

	/* There are two scenarios which could make this check succeed:
	   a) this sym is in the GlobalSymList as a LOCAL_SYM symbol
	   b) there is another symbol as a non-LOCAL_SYM in the GlobalSymList
	   Both are errors, without question.
	   We currently just print this warning, but we should error out the
	   parsing process. */
    Symbol *const s = LookupSymbol(sym->name);
	if (sym == s) {
		/* case a)
		   just make this symbol a GLOBAL_SYM symbol and return */
		fprintf(stderr, "nedit: To boldly go where no local sym has gone before: %s\n", sym->name.c_str());
		sym->type = GLOBAL_SYM;
		return sym;
	} else if (nullptr != s) {
		/* case b)
		   sym will shadow the old symbol from the GlobalSymList */
		fprintf(stderr, "nedit: duplicate symbol in LocalSymList and GlobalSymList: %s\n", sym->name.c_str());
	}

	/* Add the symbol directly to the GlobalSymList, because InstallSymbol()
	   will allocate a new Symbol, which results in a memory leak of sym.
	   Don't use MACRO_FUNCTION_SYM as type, because in
	   macro.c:readCheckMacroString() we use ProgramFree() for the .val.prog,
	   but this symbol has no program attached and ProgramFree() is not nullptr
	   pointer safe */
	sym->type = GLOBAL_SYM;

	GlobalSymList.push_back(sym);

	return sym;
}

/*
** Allocate memory for a string, and keep track of it, such that it
** can be recovered later using GarbageCollectStrings.  (A linked list
** of pointers is maintained by threading through the memory behind
** the returned pointers).  Length does not include the terminating null
** character, so to allocate space for a string of strlen == n, you must
** use AllocString(n+1).
*/

//#define TRACK_GARBAGE_LEAKS

// Allocate a new string buffer of length chars 
char *AllocString(int length) {
    auto mem = new char [length + 1];
	AllocatedStrings.push_back(mem);
	return mem + 1;
}

/*
 * Allocate a new NString buffer of length chars (terminating \0 included),
 * The buffer length is initialized to length-1 and the terminating \0 is
 * filled in.
 */
NString AllocNStringEx(int length) {

    NString str;

    auto mem = new char[length + 1];

    AllocatedStrings.push_back(mem);

    str.rep = mem + 1;
    str.rep[length - 1] = '\0'; // forced \0
    str.len = length - 1;
    return str;
}

/*
 * Allocate a new NString buffer of length chars (terminating \0 included),
 * The buffer length is initialized to length-1 and the terminating \0 is
 * filled in.
 */
int AllocNString(NString *string, int length) {
    auto mem = new char[length + 1];
	if (!mem) {
		string->rep = nullptr;
		string->len = 0;
        return false;
	}

	AllocatedStrings.push_back(mem);

	string->rep = mem + 1;
	string->rep[length - 1] = '\0'; // forced \0 
	string->len = length - 1;
    return true;
}

/*
 * Allocate a new NString buffer of length chars (terminating \0 NOT included),
 * and copy at most length characters of the given string.
 * The buffer length is properly set and the buffer is guaranteed to be
 * \0-terminated.
 */
int AllocNStringNCpy(NString *string, const char *s, int length) {
	if (!AllocNString(string, length + 1)) // add extra char for forced \0 
        return false;
	if (!s)
		s = "";
	strncpy(string->rep, s, length);
	string->len = strlen(string->rep); // re-calculate! 
    return true;
}

// Allocate a new copy of string s
char *AllocStringCpyEx(const std::string &s) {

    auto str = AllocString(s.size() + 1);
    memcpy(str, s.data(), s.size());
    str[s.size()] = '\0';
    return str;
}

/*
 * Allocate a new NString buffer, containing a copy of the given string.
 * The length is set to the length of the string and resulting string is
 * guaranteed to be \0-terminated.
 */
int AllocNStringCpy(NString *string, const char *s) {
	size_t length = s ? strlen(s) : 0;

	if (!AllocNString(string, length + 1)) {
        return false;
	}

	if (s) {
		strncpy(string->rep, s, length);
        string->rep[length] = '\0';
	}
    return true;
}

NString AllocNStringCpyEx(const QString &s) {

    if(s.isNull()) {
        NString string;
        string.len = 0;
        string.rep = nullptr;
        return string;
    }

    size_t length = s.size();

    NString string = AllocNStringEx(length + 1);
    memcpy(string.rep, s.toLatin1().data(), length);
    string.rep[length] = '\0';

    return string;
}

NString AllocNStringCpyEx(const std::string &s) {
    size_t length = s.size();

    NString string = AllocNStringEx(length + 1);
    memcpy(string.rep, s.data(), length);
    string.rep[length] = '\0';

    return string;
}

NString AllocNStringCpyEx(const view::string_view s) {
    size_t length = s.size();

    NString string = AllocNStringEx(length + 1);
    memcpy(string.rep, s.data(), length);
    string.rep[length] = '\0';

    return string;
}

static ArrayEntry *allocateSparseArrayEntry() {
	auto mem = new ArrayEntry;
	AllocatedSparseArrayEntries.push_back(mem);
	return mem;
}

static void MarkArrayContentsAsUsed(ArrayEntry *arrayPtr) {

	if (arrayPtr) {
		arrayPtr->inUse = true;

		auto first = static_cast<ArrayEntry *>(rbTreeBegin(arrayPtr));

		for (ArrayEntry *globalSEUse = first; globalSEUse != nullptr; globalSEUse = static_cast<ArrayEntry *>(rbTreeNext(globalSEUse))) {

			globalSEUse->inUse = true;

			// test first because it may be read-only static string 
			if (globalSEUse->key[-1] == 0) {
				globalSEUse->key[-1] = 1;
			}

			if (globalSEUse->value.tag == STRING_TAG) {
				// test first because it may be read-only static string 
				if (globalSEUse->value.val.str.rep[-1] == 0) {
					globalSEUse->value.val.str.rep[-1] = 1;
				}
			} else if (globalSEUse->value.tag == ARRAY_TAG) {
				MarkArrayContentsAsUsed(globalSEUse->value.val.arrayPtr);
			}
		}
	}
}

/*
** Collect strings that are no longer referenced from the global symbol
** list.  THIS CAN NOT BE RUN WHILE ANY MACROS ARE EXECUTING.  It must
** only be run after all macro activity has ceased.
*/

void GarbageCollectStrings() {

	// mark all strings as unreferenced 
	for(char *p : AllocatedStrings) {
		*p = 0;
	}

	for(ArrayEntry *thisAP : AllocatedSparseArrayEntries) {
		thisAP->inUse = false;
	}

	/* Sweep the global symbol list, marking which strings are still
	   referenced */
	for (Symbol *s: GlobalSymList) {
		if (s->value.tag == STRING_TAG) {
			// test first because it may be read-only static string 
			if (s->value.val.str.rep[-1] == 0) {
				s->value.val.str.rep[-1] = 1;
			}
		} else if (s->value.tag == ARRAY_TAG) {
			MarkArrayContentsAsUsed(s->value.val.arrayPtr);
		}
	}

	// Collect all of the strings which remain unreferenced 
	for(auto it = AllocatedStrings.begin(); it != AllocatedStrings.end(); ) {
		char *p = *it;
		assert(p);

		if (*p == 0) {
            delete [] p;
			it = AllocatedStrings.erase(it);
		} else {
			++it;
		}
	}

	for(auto it = AllocatedSparseArrayEntries.begin(); it != AllocatedSparseArrayEntries.end(); ) {
		ArrayEntry *thisAP = *it;
		assert(thisAP);

		if (!thisAP->inUse) {
			delete thisAP;
			it = AllocatedSparseArrayEntries.erase(it);
		} else {
			++it;
		}
	}

#ifdef TRACK_GARBAGE_LEAKS
	printf("str count = %lu\nary count = %lu\n", AllocatedStrings.size(), AllocatedSparseArrayEntries.size());
#endif
}

/*
** Save and restore execution context to data structure "context"
*/
static void saveContextEx(RestartData *context) {
    context->stack       = TheStack;
    context->stackP      = StackP;
    context->frameP      = FrameP;
    context->pc          = PC;
    context->runWindow   = InitiatingWindowEx;
    context->focusWindow = FocusWindowEx;
}

static void restoreContextEx(RestartData *context) {
    TheStack           = context->stack;
    StackP             = context->stackP;
    FrameP             = context->frameP;
    PC                 = context->pc;
    InitiatingWindowEx = context->runWindow;
    FocusWindowEx      = context->focusWindow;
}

#define POP(dataVal)                                                                               \
	if (StackP == TheStack)                                                                        \
	    return execError(StackUnderflowMsg);                                                   \
	dataVal = *--StackP;

#define PUSH(dataVal)                                                                              \
	if (StackP >= &TheStack[STACK_SIZE])                                                           \
	    return execError(StackOverflowMsg);                                                    \
	*StackP++ = dataVal;

#define PEEK(dataVal, peekIndex) dataVal = *(StackP - peekIndex - 1);

#define POP_INT(number)                                                                            \
	if (StackP == TheStack)                                                                        \
	    return execError(StackUnderflowMsg);                                                   \
	--StackP;                                                                                      \
	if (StackP->tag == STRING_TAG) {                                                               \
		if (!StringToNum(StackP->val.str.rep, &number))                                            \
	        return execError(StringToNumberMsg);                                               \
	} else if (StackP->tag == INT_TAG)                                                             \
		number = StackP->val.n;                                                                    \
	else                                                                                           \
	    return execError("can't convert array to integer");

#define POP_STRING(string)                                                                         \
	if (StackP == TheStack)                                                                        \
	    return execError(StackUnderflowMsg);                                                   \
	--StackP;                                                                                      \
	if (StackP->tag == INT_TAG) {                                                                  \
		string = AllocString(TYPE_INT_STR_SIZE(int));                                              \
		sprintf(string, "%d", StackP->val.n);                                                      \
	} else if (StackP->tag == STRING_TAG)                                                          \
		string = StackP->val.str.rep;                                                              \
	else                                                                                           \
	    return execError("can't convert array to string");

#define PEEK_STRING(string, peekIndex)                                                             \
	if ((StackP - peekIndex - 1)->tag == INT_TAG) {                                                \
		string = AllocString(TYPE_INT_STR_SIZE(int));                                              \
		sprintf(string, "%d", (StackP - peekIndex - 1)->val.n);                                    \
	} else if ((StackP - peekIndex - 1)->tag == STRING_TAG) {                                      \
		string = (StackP - peekIndex - 1)->val.str.rep;                                            \
	} else {                                                                                       \
	    return execError("can't convert array to string");                                         \
	}

#define PEEK_INT(number, peekIndex)                                                                \
	if ((StackP - peekIndex - 1)->tag == STRING_TAG) {                                             \
		if (!StringToNum((StackP - peekIndex - 1)->val.str.rep, &number)) {                        \
	        return execError(StringToNumberMsg);                                               \
		}                                                                                          \
	} else if ((StackP - peekIndex - 1)->tag == INT_TAG) {                                         \
		number = (StackP - peekIndex - 1)->val.n;                                                  \
	} else {                                                                                       \
	    return execError("can't convert array to string");                                         \
	}

#define PUSH_INT(number)                                                                           \
	if (StackP >= &TheStack[STACK_SIZE])                                                           \
	    return execError(StackOverflowMsg);                                                    \
	StackP->tag   = INT_TAG;                                                                       \
	StackP->val.n = number;                                                                        \
	StackP++;

#define PUSH_STRING(string, length)                                                                \
	if (StackP >= &TheStack[STACK_SIZE])                                                           \
	    return execError(StackOverflowMsg);                                                    \
	StackP->tag     = STRING_TAG;                                                                  \
	StackP->val.str = NString({string, size_t(length)});                                           \
	StackP++;

#define BINARY_NUMERIC_OPERATION(operator)                                                         \
	int n1, n2;                                                                                    \
	DISASM_RT(PC - 1, 1);                                                                          \
	STACKDUMP(2, 3);                                                                               \
	POP_INT(n2)                                                                                    \
	POP_INT(n1)                                                                                    \
	PUSH_INT(n1 operator n2)                                                                       \
	return STAT_OK;

#define UNARY_NUMERIC_OPERATION(operator)                                                          \
	int n;                                                                                         \
	DISASM_RT(PC - 1, 1);                                                                          \
	STACKDUMP(1, 3);                                                                               \
	POP_INT(n)                                                                                     \
	PUSH_INT(operator n)                                                                           \
	return STAT_OK;

/*
** copy a symbol's value onto the stack
** Before: Prog->  [Sym], next, ...
**         TheStack-> next, ...
** After:  Prog->  Sym, [next], ...
**         TheStack-> [symVal], next, ...
*/
static int pushSymVal() {
    Symbol *s;
	int nArgs, argNum;
	DataValue symVal;

	DISASM_RT(PC - 1, 2);
	STACKDUMP(0, 3);

    s = PC->sym;
	PC++;

	if (s->type == LOCAL_SYM) {
		symVal = FP_GET_SYM_VAL(FrameP, s);
	} else if (s->type == GLOBAL_SYM || s->type == CONST_SYM) {
		symVal = s->value;
	} else if (s->type == ARG_SYM) {
		nArgs = FP_GET_ARG_COUNT(FrameP);
		argNum = s->value.val.n;
		if (argNum >= nArgs) {
			return execError("referenced undefined argument: %s", s->name.c_str());
		}
		if (argNum == N_ARGS_ARG_SYM) {
			symVal.tag = INT_TAG;
			symVal.val.n = nArgs;
		} else {
			symVal = FP_GET_ARG_N(FrameP, argNum);
		}
	} else if (s->type == PROC_VALUE_SYM) {
		const char *errMsg;
        if (!(s->value.val.subr)(FocusWindowEx, nullptr, 0, &symVal, &errMsg)) {
			return execError(errMsg, s->name.c_str());
		}
	} else
		return execError("reading non-variable: %s", s->name.c_str());
	if (symVal.tag == NO_TAG) {
		return execError("variable not set: %s", s->name.c_str());
	}

	PUSH(symVal)

	return STAT_OK;
}

static int pushArgVal() {
	int nArgs, argNum;

	DISASM_RT(PC - 1, 1);
	STACKDUMP(1, 3);

	POP_INT(argNum)
	--argNum;
	nArgs = FP_GET_ARG_COUNT(FrameP);
	if (argNum >= nArgs || argNum < 0) {
		char argStr[TYPE_INT_STR_SIZE(argNum)];
		sprintf(argStr, "%d", argNum + 1);
		return execError("referenced undefined argument: $args[%s]", argStr);
	}
	PUSH(FP_GET_ARG_N(FrameP, argNum));
	return STAT_OK;
}

static int pushArgCount() {
	DISASM_RT(PC - 1, 1);
	STACKDUMP(0, 3);

	PUSH_INT(FP_GET_ARG_COUNT(FrameP));
	return STAT_OK;
}

static int pushArgArray() {
	int nArgs;
	DataValue argVal;
	DataValue *resultArray;

	DISASM_RT(PC - 1, 1);
	STACKDUMP(0, 3);

	nArgs = FP_GET_ARG_COUNT(FrameP);
	resultArray = &FP_GET_ARG_ARRAY_CACHE(FrameP);
	if (resultArray->tag != ARRAY_TAG) {
		resultArray->tag = ARRAY_TAG;
		resultArray->val.arrayPtr = ArrayNew();

		for (int argNum = 0; argNum < nArgs; ++argNum) {

			auto intStr = std::to_string(argNum);

			argVal = FP_GET_ARG_N(FrameP, argNum);
			if (!ArrayInsert(resultArray, AllocStringCpyEx(intStr), &argVal)) {
				return execError("array insertion failure");
			}
		}
	}
	PUSH(*resultArray);
	return STAT_OK;
}

/*
** Push an array (by reference) onto the stack
** Before: Prog->  [ArraySym], makeEmpty, next, ...
**         TheStack-> next, ...
** After:  Prog->  ArraySym, makeEmpty, [next], ...
**         TheStack-> [elemValue], next, ...
** makeEmpty is either true (1) or false (0): if true, and the element is not
** present in the array, create it.
*/
static int pushArraySymVal() {

	DataValue *dataPtr;
	
	DISASM_RT(PC - 1, 3);
	STACKDUMP(0, 3);

	Symbol *sym   = PC++->sym;
	int initEmpty = PC++->value;

	if (sym->type == LOCAL_SYM) {
		dataPtr = &FP_GET_SYM_VAL(FrameP, sym);
	} else if (sym->type == GLOBAL_SYM) {
		dataPtr = &sym->value;
	} else {
		return execError("assigning to non-lvalue array or non-array: %s", sym->name.c_str());
	}

	if (initEmpty && dataPtr->tag == NO_TAG) {
		dataPtr->tag = ARRAY_TAG;
		dataPtr->val.arrayPtr = ArrayNew();
	}

	if (dataPtr->tag == NO_TAG) {
		return execError("variable not set: %s", sym->name.c_str());
	}

	PUSH(*dataPtr)

	return STAT_OK;
}

/*
** assign top value to next symbol
**
** Before: Prog->  [symbol], next, ...
**         TheStack-> [value], next, ...
** After:  Prog->  symbol, [next], ...
**         TheStack-> next, ...
*/
static int assign() {
	Symbol *sym;
	DataValue *dataPtr;
	DataValue value;

	DISASM_RT(PC - 1, 2);
	STACKDUMP(1, 3);

	sym = PC->sym;
	PC++;

	if (sym->type != GLOBAL_SYM && sym->type != LOCAL_SYM) {
		if (sym->type == ARG_SYM) {
			return execError("assignment to function argument: %s", sym->name.c_str());
		} else if (sym->type == PROC_VALUE_SYM) {
			return execError("assignment to read-only variable: %s", sym->name.c_str());
		} else {
			return execError("assignment to non-variable: %s", sym->name.c_str());
		}
	}

	if (sym->type == LOCAL_SYM) {
		dataPtr = &FP_GET_SYM_VAL(FrameP, sym);
	} else {
		dataPtr = &sym->value;
	}

	POP(value)

	if (value.tag == ARRAY_TAG) {
		return ArrayCopy(dataPtr, &value);
	}

	*dataPtr = value;
	return STAT_OK;
}

/*
** copy the top value of the stack
** Before: TheStack-> value, next, ...
** After:  TheStack-> value, value, next, ...
*/
static int dupStack() {
	DataValue value;

	DISASM_RT(PC - 1, 1);
	STACKDUMP(1, 3);

	PEEK(value, 0)
	PUSH(value)

	return STAT_OK;
}

/*
** if left and right arguments are arrays, then the result is a new array
** in which all the keys from both the right and left are copied
** the values from the right array are used in the result array when the
** keys are the same
** Before: TheStack-> value2, value1, next, ...
** After:  TheStack-> resValue, next, ...
*/
static int add() {
	DataValue leftVal, rightVal, resultArray;
	int n1, n2;

	DISASM_RT(PC - 1, 1);
	STACKDUMP(2, 3);

	PEEK(rightVal, 0)
	if (rightVal.tag == ARRAY_TAG) {
		PEEK(leftVal, 1)
		if (leftVal.tag == ARRAY_TAG) {
			ArrayEntry *leftIter, *rightIter;
			resultArray.tag = ARRAY_TAG;
			resultArray.val.arrayPtr = ArrayNew();

			POP(rightVal)
			POP(leftVal)
			leftIter = arrayIterateFirst(&leftVal);
			rightIter = arrayIterateFirst(&rightVal);
			while (leftIter || rightIter) {
                bool insertResult = true;

				if (leftIter && rightIter) {
					int compareResult = arrayEntryCompare(leftIter, rightIter);
					if (compareResult < 0) {
						insertResult = ArrayInsert(&resultArray, leftIter->key, &leftIter->value);
						leftIter = arrayIterateNext(leftIter);
					} else if (compareResult > 0) {
						insertResult = ArrayInsert(&resultArray, rightIter->key, &rightIter->value);
						rightIter = arrayIterateNext(rightIter);
					} else {
						insertResult = ArrayInsert(&resultArray, rightIter->key, &rightIter->value);
						leftIter = arrayIterateNext(leftIter);
						rightIter = arrayIterateNext(rightIter);
					}
				} else if (leftIter) {
					insertResult = ArrayInsert(&resultArray, leftIter->key, &leftIter->value);
					leftIter = arrayIterateNext(leftIter);
				} else {
					insertResult = ArrayInsert(&resultArray, rightIter->key, &rightIter->value);
					rightIter = arrayIterateNext(rightIter);
				}
				if (!insertResult) {
					return execError("array insertion failure");
				}
			}
			PUSH(resultArray)
		} else {
			return execError("can't mix math with arrays and non-arrays");
		}
	} else {
		POP_INT(n2)
		POP_INT(n1)
		PUSH_INT(n1 + n2)
	}
	return STAT_OK;
}

/*
** if left and right arguments are arrays, then the result is a new array
** in which only the keys which exist in the left array but not in the right
** are copied
** Before: TheStack-> value2, value1, next, ...
** After:  TheStack-> resValue, next, ...
*/
static int subtract() {
	DataValue leftVal, rightVal, resultArray;
	int n1, n2;

	DISASM_RT(PC - 1, 1);
	STACKDUMP(2, 3);

	PEEK(rightVal, 0)
	if (rightVal.tag == ARRAY_TAG) {
		PEEK(leftVal, 1)
		if (leftVal.tag == ARRAY_TAG) {
			ArrayEntry *leftIter, *rightIter;
			resultArray.tag = ARRAY_TAG;
			resultArray.val.arrayPtr = ArrayNew();

			POP(rightVal)
			POP(leftVal)
			leftIter = arrayIterateFirst(&leftVal);
			rightIter = arrayIterateFirst(&rightVal);
			while (leftIter) {
                bool insertResult = true;

				if (leftIter && rightIter) {
					int compareResult = arrayEntryCompare(leftIter, rightIter);
					if (compareResult < 0) {
						insertResult = ArrayInsert(&resultArray, leftIter->key, &leftIter->value);
						leftIter = arrayIterateNext(leftIter);
					} else if (compareResult > 0) {
						rightIter = arrayIterateNext(rightIter);
					} else {
						leftIter = arrayIterateNext(leftIter);
						rightIter = arrayIterateNext(rightIter);
					}
				} else if (leftIter) {
					insertResult = ArrayInsert(&resultArray, leftIter->key, &leftIter->value);
					leftIter = arrayIterateNext(leftIter);
				}
				if (!insertResult) {
					return execError("array insertion failure");
				}
			}
			PUSH(resultArray)
		} else {
			return execError("can't mix math with arrays and non-arrays");
		}
	} else {
		POP_INT(n2)
		POP_INT(n1)
		PUSH_INT(n1 - n2)
	}
	return STAT_OK;
}

/*
** Other binary operators
** Before: TheStack-> value2, value1, next, ...
** After:  TheStack-> resValue, next, ...
**
** Other unary operators
** Before: TheStack-> value, next, ...
** After:  TheStack-> resValue, next, ...
*/
static int multiply() {
	BINARY_NUMERIC_OPERATION(*)
}

static int divide() {
	int n1, n2;

	DISASM_RT(PC - 1, 1);
	STACKDUMP(2, 3);

	POP_INT(n2)
	POP_INT(n1)
	if (n2 == 0) {
		return execError("division by zero");
	}
	PUSH_INT(n1 / n2)
	return STAT_OK;
}

static int modulo() {
	int n1, n2;

	DISASM_RT(PC - 1, 1);
	STACKDUMP(2, 3);

	POP_INT(n2)
	POP_INT(n1)
	if (n2 == 0) {
		return execError("modulo by zero");
	}
	PUSH_INT(n1 % n2)
	return STAT_OK;
}

static int negate() {
	UNARY_NUMERIC_OPERATION(-)
}

static int increment() {
	UNARY_NUMERIC_OPERATION(++)
}

static int decrement() {
	UNARY_NUMERIC_OPERATION(--)
}

static int gt() {
	BINARY_NUMERIC_OPERATION(> )
}

static int lt() {
	BINARY_NUMERIC_OPERATION(< )
}

static int ge() {
	BINARY_NUMERIC_OPERATION(>= )
}

static int le() {
	BINARY_NUMERIC_OPERATION(<= )
}

/*
** verify that compares are between integers and/or strings only
** Before: TheStack-> value1, value2, next, ...
** After:  TheStack-> resValue, next, ...
** where resValue is 1 for true, 0 for false
*/
static int eq() {
	DataValue v1, v2;

	DISASM_RT(PC - 1, 1);
	STACKDUMP(2, 3);

	POP(v1)
	POP(v2)
	if (v1.tag == INT_TAG && v2.tag == INT_TAG) {
		v1.val.n = v1.val.n == v2.val.n;
	} else if (v1.tag == STRING_TAG && v2.tag == STRING_TAG) {
		v1.val.n = !strcmp(v1.val.str.rep, v2.val.str.rep);
	} else if (v1.tag == STRING_TAG && v2.tag == INT_TAG) {
		int number;
		if (!StringToNum(v1.val.str.rep, &number)) {
			v1.val.n = 0;
		} else {
			v1.val.n = number == v2.val.n;
		}
	} else if (v2.tag == STRING_TAG && v1.tag == INT_TAG) {
		int number;
		if (!StringToNum(v2.val.str.rep, &number)) {
			v1.val.n = 0;
		} else {
			v1.val.n = number == v1.val.n;
		}
	} else {
		return execError("incompatible types to compare");
	}
	v1.tag = INT_TAG;
	PUSH(v1)
	return STAT_OK;
}

// negated eq() call 
static int ne() {
	eq();
	return logicalNot();
}

/*
** if left and right arguments are arrays, then the result is a new array
** in which only the keys which exist in both the right or left are copied
** the values from the right array are used in the result array
** Before: TheStack-> value2, value1, next, ...
** After:  TheStack-> resValue, next, ...
*/
static int bitAnd() {
	DataValue leftVal, rightVal, resultArray;
	int n1, n2;

	DISASM_RT(PC - 1, 1);
	STACKDUMP(2, 3);

	PEEK(rightVal, 0)
	if (rightVal.tag == ARRAY_TAG) {
		PEEK(leftVal, 1)
		if (leftVal.tag == ARRAY_TAG) {
			ArrayEntry *leftIter, *rightIter;
			resultArray.tag = ARRAY_TAG;
			resultArray.val.arrayPtr = ArrayNew();

			POP(rightVal)
			POP(leftVal)
			leftIter = arrayIterateFirst(&leftVal);
			rightIter = arrayIterateFirst(&rightVal);
			while (leftIter && rightIter) {
                bool insertResult = true;
				int compareResult = arrayEntryCompare(leftIter, rightIter);

				if (compareResult < 0) {
					leftIter = arrayIterateNext(leftIter);
				} else if (compareResult > 0) {
					rightIter = arrayIterateNext(rightIter);
				} else {
					insertResult = ArrayInsert(&resultArray, rightIter->key, &rightIter->value);
					leftIter = arrayIterateNext(leftIter);
					rightIter = arrayIterateNext(rightIter);
				}
				if (!insertResult) {
					return execError("array insertion failure");
				}
			}
			PUSH(resultArray)
		} else {
			return execError("can't mix math with arrays and non-arrays");
		}
	} else {
		POP_INT(n2)
		POP_INT(n1)
		PUSH_INT(n1 & n2)
	}
	return STAT_OK;
}

/*
** if left and right arguments are arrays, then the result is a new array
** in which only the keys which exist in either the right or left but not both
** are copied
** Before: TheStack-> value2, value1, next, ...
** After:  TheStack-> resValue, next, ...
*/
static int bitOr() {
	DataValue leftVal, rightVal, resultArray;
	int n1, n2;

	DISASM_RT(PC - 1, 1);
	STACKDUMP(2, 3);

	PEEK(rightVal, 0)
	if (rightVal.tag == ARRAY_TAG) {
		PEEK(leftVal, 1)
		if (leftVal.tag == ARRAY_TAG) {
			ArrayEntry *leftIter, *rightIter;
			resultArray.tag = ARRAY_TAG;
			resultArray.val.arrayPtr = ArrayNew();

			POP(rightVal)
			POP(leftVal)
			leftIter = arrayIterateFirst(&leftVal);
			rightIter = arrayIterateFirst(&rightVal);
			while (leftIter || rightIter) {
                bool insertResult = true;

				if (leftIter && rightIter) {
					int compareResult = arrayEntryCompare(leftIter, rightIter);
					if (compareResult < 0) {
						insertResult = ArrayInsert(&resultArray, leftIter->key, &leftIter->value);
						leftIter = arrayIterateNext(leftIter);
					} else if (compareResult > 0) {
						insertResult = ArrayInsert(&resultArray, rightIter->key, &rightIter->value);
						rightIter = arrayIterateNext(rightIter);
					} else {
						leftIter = arrayIterateNext(leftIter);
						rightIter = arrayIterateNext(rightIter);
					}
				} else if (leftIter) {
					insertResult = ArrayInsert(&resultArray, leftIter->key, &leftIter->value);
					leftIter = arrayIterateNext(leftIter);
				} else {
					insertResult = ArrayInsert(&resultArray, rightIter->key, &rightIter->value);
					rightIter = arrayIterateNext(rightIter);
				}
				if (!insertResult) {
					return execError("array insertion failure");
				}
			}
			PUSH(resultArray)
		} else {
			return execError("can't mix math with arrays and non-arrays");
		}
	} else {
		POP_INT(n2)
		POP_INT(n1)
		PUSH_INT(n1 | n2)
	}
	return STAT_OK;
}

static int logicalAnd() {
	BINARY_NUMERIC_OPERATION(&&)
}

static int logicalOr() {
	BINARY_NUMERIC_OPERATION(|| )
}

static int logicalNot() {
	UNARY_NUMERIC_OPERATION(!)
}

/*
** raise one number to the power of another
** Before: TheStack-> raisedBy, number, next, ...
** After:  TheStack-> result, next, ...
*/
static int power() {
	int n1, n2, n3;

	DISASM_RT(PC - 1, 1);
	STACKDUMP(2, 3);

	POP_INT(n2)
	POP_INT(n1)
	/*  We need to round to deal with pow() giving results slightly above
	    or below the real result since it deals with floating point numbers.
	    Note: We're not really wanting rounded results, we merely
	    want to deal with this simple issue. So, 2^-2 = .5, but we
	    don't want to round this to 1. This is mainly intended to deal with
	    4^2 = 15.999996 and 16.000001.
	*/
	if (n2 < 0 && n1 != 1 && n1 != -1) {
		if (n1 != 0) {
			// since we're integer only, nearly all negative exponents result in 0 
			n3 = 0;
		} else {
			// allow error to occur 
			n3 = (int)pow((double)n1, (double)n2);
		}
	} else {
		if ((n1 < 0) && (n2 & 1)) {
			// round to nearest integer for negative values
			n3 = (int)(pow((double)n1, (double)n2) - (double)0.5);
		} else {
			// round to nearest integer for positive values
			n3 = (int)(pow((double)n1, (double)n2) + (double)0.5);
		}
	}
	PUSH_INT(n3)
	return errCheck("exponentiation");
}

/*
** concatenate two top items on the stack
** Before: TheStack-> str2, str1, next, ...
** After:  TheStack-> result, next, ...
*/
static int concat() {
    char *s1, *s2;

	DISASM_RT(PC - 1, 1);
	STACKDUMP(2, 3);

	POP_STRING(s2)
	POP_STRING(s1)

	int len1 = strlen(s1);
	int len2 = strlen(s2);
    char *out = AllocString(len1 + len2 + 1);
	strncpy(out, s1, len1);
	strcpy(&out[len1], s2);
	PUSH_STRING(out, len1 + len2)
	return STAT_OK;
}

/*
** Call a subroutine or function (user defined or built-in).  Args are the
** subroutine's symbol, and the number of arguments which have been pushed
** on the stack.
**
** For a macro subroutine, the return address, frame pointer, number of
** arguments and space for local variables are added to the stack, and the
** PC is set to point to the new function. For a built-in routine, the
** arguments are popped off the stack, and the routine is just called.
**
** Before: Prog->  [subrSym], nArgs, next, ...
**         TheStack-> argN-arg1, next, ...
** After:  Prog->  next, ...            -- (built-in called subr)
**         TheStack-> retVal?, next, ...
**    or:  Prog->  (in called)next, ... -- (macro code called subr)
**         TheStack-> symN-sym1(FP), argArray, nArgs, oldFP, retPC, argN-arg1, next, ...
*/
static int callSubroutine() {

	static DataValue noValue = INIT_DATA_VALUE;
	Program *prog;
	const char *errMsg;

    Symbol *sym = PC->sym;
	PC++;

    int nArgs = PC->value;
	PC++;

	DISASM_RT(PC - 3, 3);
	STACKDUMP(nArgs, 3);

	/*
	** If the subroutine is built-in, call the built-in routine
	*/
	if (sym->type == C_FUNCTION_SYM) {
		DataValue result;

		// "pop" stack back to the first argument in the call stack 
		StackP -= nArgs;

		// Call the function and check for preemption 
        PreemptRequest = false;
        if (!sym->value.val.subr(FocusWindowEx, StackP, nArgs, &result, &errMsg)) {
            return execError(errMsg, sym->name.c_str());
        }

        if (PC->func == fetchRetVal) {
            if (result.tag == NO_TAG) {
                return execError("%s does not return a value", sym->name.c_str());
            }
            PUSH(result);
            PC++;
        }
        return PreemptRequest ? STAT_PREEMPT : STAT_OK;
	}

	/*
	** Call a macro subroutine:
	**
	** Push all of the required information to resume, and make space on the
	** stack for local variables (and initialize them), on top of the argument
	** values which are already there.
	*/
	if (sym->type == MACRO_FUNCTION_SYM) {

		StackP->tag = NO_TAG; // return PC
		StackP->val.inst = PC;
		StackP++;

		StackP->tag = NO_TAG; // old FrameP 
		StackP->val.dataval = FrameP;
		StackP++;

		StackP->tag = NO_TAG; // nArgs 
		StackP->val.n = nArgs;
		StackP++;

		*(StackP++) = noValue; // cached arg array 

		FrameP = StackP;
		prog = sym->value.val.prog;
		PC = prog->code;

		for(Symbol *s : prog->localSymList) {
			FP_GET_SYM_VAL(FrameP, s) = noValue;
			StackP++;
		}
		return STAT_OK;
	}

	// Calling a non subroutine symbol 
	return execError("%s is not a function or subroutine", sym->name.c_str());
}

/*
** This should never be executed, returnVal checks for the presence of this
** instruction at the PC to decide whether to push the function's return
** value, then skips over it without executing.
*/
static int fetchRetVal() {
	return execError("internal error: frv");
}

// see comments for returnValOrNone() 
static int returnNoVal() {
    return returnValOrNone(false);
}
static int returnVal() {
    return returnValOrNone(true);
}

/*
** Return from a subroutine call
** Before: Prog->  [next], ...
**         TheStack-> retVal?, ...(FP), argArray, nArgs, oldFP, retPC, argN-arg1, next, ...
** After:  Prog->  next, ..., (in caller)[FETCH_RET_VAL?], ...
**         TheStack-> retVal?, next, ...
*/
static int returnValOrNone(bool valOnStack) {
	DataValue retVal;
	static DataValue noValue = INIT_DATA_VALUE;
	DataValue *newFrameP;
	int nArgs;

	DISASM_RT(PC - 1, 1);
	STACKDUMP(StackP - FrameP + FP_GET_ARG_COUNT(FrameP) + FP_TO_ARGS_DIST, 3);

	// return value is on the stack 
	if (valOnStack) {
		POP(retVal);
	}

	// get stored return information 
    nArgs     = FP_GET_ARG_COUNT(FrameP);
	newFrameP = FP_GET_OLD_FP(FrameP);
    PC        = FP_GET_RET_PC(FrameP);

	// pop past local variables 
	StackP = FrameP;
	// pop past function arguments 
	StackP -= (FP_TO_ARGS_DIST + nArgs);
	FrameP = newFrameP;

	// push returned value, if requsted 
	if(!PC) {
		if (valOnStack) {
			PUSH(retVal);
		} else {
			PUSH(noValue);
		}
	} else if (PC->func == fetchRetVal) {
		if (valOnStack) {
			PUSH(retVal);
			PC++;
		} else {
			return execError("using return value of %s which does not return a value", ((PC - 2)->sym->name.c_str()));
		}
	}

	// nullptr return PC indicates end of program 
	return PC == nullptr ? STAT_DONE : STAT_OK;
}

/*
** Unconditional branch offset by immediate operand
**
** Before: Prog->  [branchDest], next, ..., (branchdest)next
** After:  Prog->  branchDest, next, ..., (branchdest)[next]
*/
static int branch() {
	DISASM_RT(PC - 1, 2);
	STACKDUMP(0, 3);

	PC += PC->value;
	return STAT_OK;
}

/*
** Conditional branches if stack value is True/False (non-zero/0) to address
** of immediate operand (pops stack)
**
** Before: Prog->  [branchDest], next, ..., (branchdest)next
** After:  either: Prog->  branchDest, [next], ...
** After:  or:     Prog->  branchDest, next, ..., (branchdest)[next]
*/
static int branchTrue() {
	int value;
	Inst *addr;

	DISASM_RT(PC - 1, 2);
	STACKDUMP(1, 3);

	POP_INT(value)
	addr = PC + PC->value;
	PC++;

	if (value)
		PC = addr;
	return STAT_OK;
}
static int branchFalse() {
	int value;
	Inst *addr;

	DISASM_RT(PC - 1, 2);
	STACKDUMP(1, 3);

	POP_INT(value)
	addr = PC + PC->value;
	PC++;

	if (!value)
		PC = addr;
	return STAT_OK;
}

/*
** Ignore the address following the instruction and continue.  Why? So
** some code that uses conditional branching doesn't have to figure out
** whether to store a branch address.
**
** Before: Prog->  [branchDest], next, ...
** After:  Prog->  branchDest, [next], ...
*/
static int branchNever() {
	DISASM_RT(PC - 1, 2);
	STACKDUMP(0, 3);

	PC++;
	return STAT_OK;
}

/*
** recursively copy(duplicate) the sparse array nodes of an array
** this does not duplicate the key/node data since they are never
** modified, only replaced
*/
int ArrayCopy(DataValue *dstArray, DataValue *srcArray) {
	ArrayEntry *srcIter;

	dstArray->tag = ARRAY_TAG;
	dstArray->val.arrayPtr = ArrayNew();

	srcIter = arrayIterateFirst(srcArray);
	while (srcIter) {
		if (srcIter->value.tag == ARRAY_TAG) {
			int errNum;
			DataValue tmpArray;

			errNum = ArrayCopy(&tmpArray, &srcIter->value);
			if (errNum != STAT_OK) {
				return errNum;
			}
			if (!ArrayInsert(dstArray, srcIter->key, &tmpArray)) {
				return execError("array copy failed");
			}
		} else {
			if (!ArrayInsert(dstArray, srcIter->key, &srcIter->value)) {
				return execError("array copy failed");
			}
		}
		srcIter = arrayIterateNext(srcIter);
	}
	return STAT_OK;
}

/*
** creates an allocated string of a single key for all the sub-scripts
** using ARRAY_DIM_SEP as a separator
** this function uses the PEEK macros in order to remove most limits on
** the number of arguments to an array
** I really need to optimize the size approximation rather than assuming
** a worst case size for every integer argument
*/
static int makeArrayKeyFromArgs(int nArgs, char **keyString, int leaveParams) {
	DataValue tmpVal;
	int sepLen = strlen(ARRAY_DIM_SEP);
	int keyLength = 0;
	int i;

	keyLength = sepLen * (nArgs - 1);
	for (i = nArgs - 1; i >= 0; --i) {
		PEEK(tmpVal, i)
		if (tmpVal.tag == INT_TAG) {
			keyLength += TYPE_INT_STR_SIZE(tmpVal.val.n);
		} else if (tmpVal.tag == STRING_TAG) {
			keyLength += tmpVal.val.str.len;
		} else {
			return execError("can only index array with string or int.");
		}
	}
	*keyString = AllocString(keyLength + 1);
	(*keyString)[0] = '\0';
	for (i = nArgs - 1; i >= 0; --i) {
		if (i != nArgs - 1) {
			strcat(*keyString, ARRAY_DIM_SEP);
		}
		PEEK(tmpVal, i)
		if (tmpVal.tag == INT_TAG) {
			sprintf(&((*keyString)[strlen(*keyString)]), "%d", tmpVal.val.n);
		} else if (tmpVal.tag == STRING_TAG) {
			strcat(*keyString, tmpVal.val.str.rep);
		} else {
			return execError("can only index array with string or int.");
		}
	}
	if (!leaveParams) {
		for (i = nArgs - 1; i >= 0; --i) {
			POP(tmpVal)
		}
	}
    return STAT_OK;
}

/*
** allocate an empty array node, this is used as the root node and never
** contains any data, only refernces to other nodes
*/
static rbTreeNode *arrayEmptyAllocator() {
	ArrayEntry *newNode = allocateSparseArrayEntry();
	if (newNode) {
		newNode->key = nullptr;
		newNode->value.tag = NO_TAG;
	}
	return newNode;
}

/*
** create and copy array node and copy contents, we merely copy pointers
** since they are never modified, only replaced
*/
static rbTreeNode *arrayAllocateNode(rbTreeNode *src) {
	ArrayEntry *newNode = allocateSparseArrayEntry();
	if (newNode) {
		newNode->key   = static_cast<ArrayEntry *>(src)->key;
		newNode->value = static_cast<ArrayEntry *>(src)->value;
	}
	return newNode;
}

/*
** copy array node data, we merely copy pointers since they are never
** modified, only replaced
*/
static int arrayEntryCopyToNode(rbTreeNode *dst, rbTreeNode *src) {
	static_cast<ArrayEntry *>(dst)->key   = static_cast<ArrayEntry *>(src)->key;
	static_cast<ArrayEntry *>(dst)->value = static_cast<ArrayEntry *>(src)->value;
	return 1;
}

/*
** compare two array nodes returning an integer value similar to strcmp()
*/
static int arrayEntryCompare(rbTreeNode *left, rbTreeNode *right) {
	return strcmp(static_cast<ArrayEntry *>(left)->key, static_cast<ArrayEntry *>(right)->key);
}

/*
** dispose an array node, garbage collection handles this, so we mark it
** to allow iterators in macro language to determine they have been unlinked
*/
static void arrayDisposeNode(rbTreeNode *src) {
	// Let garbage collection handle this but mark it so iterators can tell 
	src->left   = nullptr;
	src->right  = nullptr;
	src->parent = nullptr;
	src->color  = -1;
}

ArrayEntry *ArrayNew() {
	return static_cast<ArrayEntry *>(rbTreeNew(arrayEmptyAllocator));
}

/*
** insert a DataValue into an array, allocate the array if needed
** keyStr must be a string that was allocated with AllocString()
*/
bool ArrayInsert(DataValue *theArray, char *keyStr, DataValue *theValue) {
	ArrayEntry tmpEntry;

	tmpEntry.key   = keyStr;
	tmpEntry.value = *theValue;

	if (!theArray->val.arrayPtr) {
		theArray->val.arrayPtr = ArrayNew();
	}

	if (theArray->val.arrayPtr) {
		rbTreeNode *insertedNode = rbTreeInsert((theArray->val.arrayPtr), &tmpEntry, arrayEntryCompare, arrayAllocateNode, arrayEntryCopyToNode);

		if (insertedNode) {
            return true;
		} else {
            return false;
		}
	}

    return false;
}

/*
** remove a node from an array whose key matches keyStr
*/
void ArrayDelete(DataValue *theArray, char *keyStr) {
	ArrayEntry searchEntry;

	if (theArray->val.arrayPtr) {
		searchEntry.key = keyStr;
		rbTreeDelete(theArray->val.arrayPtr, &searchEntry, arrayEntryCompare, arrayDisposeNode);
	}
}

/*
** remove all nodes from an array
*/
void ArrayDeleteAll(DataValue *theArray) {
	if (theArray->val.arrayPtr) {
		rbTreeNode *iter = rbTreeBegin(theArray->val.arrayPtr);
		while (iter) {
			rbTreeNode *nextIter = rbTreeNext(iter);
			rbTreeDeleteNode(theArray->val.arrayPtr, iter, arrayDisposeNode);

			iter = nextIter;
		}
	}
}

/*
** returns the number of elements (nodes containing values) of an array
*/
unsigned ArraySize(DataValue *theArray) {
	if (theArray->val.arrayPtr) {
		return rbTreeSize(theArray->val.arrayPtr);
	} else {
		return 0;
	}
}

/*
** retrieves an array node whose key matches
** returns 1 for success 0 for not found
*/
bool ArrayGet(DataValue *theArray, char *keyStr, DataValue *theValue) {

	if (theArray->val.arrayPtr) {
		ArrayEntry searchEntry;
		searchEntry.key = keyStr;
		rbTreeNode *foundNode = rbTreeFind(theArray->val.arrayPtr, &searchEntry, arrayEntryCompare);
		if (foundNode) {
			*theValue = static_cast<ArrayEntry *>(foundNode)->value;
            return true;
		}
	}

    return false;
}

/*
** get pointer to start iterating an array
*/
ArrayEntry *arrayIterateFirst(DataValue *theArray) {
	ArrayEntry *startPos;
	if (theArray->val.arrayPtr) {
		startPos = static_cast<ArrayEntry *>(rbTreeBegin(theArray->val.arrayPtr));
	} else {
		startPos = nullptr;
	}
	return startPos;
}

/*
** move iterator to next entry in array
*/
ArrayEntry *arrayIterateNext(ArrayEntry *iterator) {
	ArrayEntry *nextPos;
	if (iterator) {
		nextPos = static_cast<ArrayEntry *>(rbTreeNext(iterator));
	} else {
		nextPos = nullptr;
	}
	return nextPos;
}

/*
** evaluate an array element and push the result onto the stack
**
** Before: Prog->  [nDim], next, ...
**         TheStack-> indnDim, ... ind1, ArraySym, next, ...
** After:  Prog->  nDim, [next], ...
**         TheStack-> indexedArrayVal, next, ...
*/
static int arrayRef() {

	DataValue srcArray;
	DataValue valueItem;
	char *keyString = nullptr;

	int nDim = PC->value;
	PC++;

	DISASM_RT(PC - 2, 2);
	STACKDUMP(nDim, 3);

	if (nDim > 0) {
		int errNum = makeArrayKeyFromArgs(nDim, &keyString, 0);
		if (errNum != STAT_OK) {
			return errNum;
		}

		POP(srcArray)
		if (srcArray.tag == ARRAY_TAG) {
			if (!ArrayGet(&srcArray, keyString, &valueItem)) {
				return execError("referenced array value not in array: %s", keyString);
			}
			PUSH(valueItem)
			return STAT_OK;
		} else {
			return execError("operator [] on non-array");
		}
	} else {
		POP(srcArray)
		if (srcArray.tag == ARRAY_TAG) {
			PUSH_INT(ArraySize(&srcArray))
			return STAT_OK;
		} else {
			return execError("operator [] on non-array");
		}
	}
}

/*
** assign to an array element of a referenced array on the stack
**
** Before: Prog->  [nDim], next, ...
**         TheStack-> rhs, indnDim, ... ind1, ArraySym, next, ...
** After:  Prog->  nDim, [next], ...
**         TheStack-> next, ...
*/
static int arrayAssign() {
	char *keyString = nullptr;
	DataValue srcValue;
	DataValue dstArray;
	int nDim;

	nDim = PC->value;
	PC++;

	DISASM_RT(PC - 2, 1);
	STACKDUMP(nDim, 3);

	if (nDim > 0) {
		POP(srcValue)

		int errNum = makeArrayKeyFromArgs(nDim, &keyString, 0);
		if (errNum != STAT_OK) {
			return errNum;
		}

		POP(dstArray)

		if (dstArray.tag != ARRAY_TAG && dstArray.tag != NO_TAG) {
			return execError("cannot assign array element of non-array");
		}
		if (srcValue.tag == ARRAY_TAG) {
			DataValue arrayCopyValue;

			errNum = ArrayCopy(&arrayCopyValue, &srcValue);
			srcValue = arrayCopyValue;
			if (errNum != STAT_OK) {
				return errNum;
			}
		}
		if (ArrayInsert(&dstArray, keyString, &srcValue)) {
			return STAT_OK;
		} else {
			return execError("array member allocation failure");
		}
	}
	return execError("empty operator []");
}

/*
** for use with assign-op operators (eg a[i,j] += k
**
** Before: Prog->  [binOp], nDim, next, ...
**         TheStack-> [rhs], indnDim, ... ind1, next, ...
** After:  Prog->  binOp, nDim, [next], ...
**         TheStack-> [rhs], arrayValue, next, ...
*/
static int arrayRefAndAssignSetup() {

	DataValue srcArray;
	DataValue valueItem;
	DataValue moveExpr;
	char *keyString = nullptr;

	int binaryOp = PC->value;
	PC++;
	int nDim = PC->value;
	PC++;

	DISASM_RT(PC - 3, 3);
	STACKDUMP(nDim + 1, 3);

	if (binaryOp) {
		POP(moveExpr)
	}

	if (nDim > 0) {
		int errNum = makeArrayKeyFromArgs(nDim, &keyString, 1);
		if (errNum != STAT_OK) {
			return errNum;
		}

		PEEK(srcArray, nDim)
		if (srcArray.tag == ARRAY_TAG) {
			if (!ArrayGet(&srcArray, keyString, &valueItem)) {
				return execError("referenced array value not in array: %s", keyString);
			}
			PUSH(valueItem)
			if (binaryOp) {
				PUSH(moveExpr)
			}
			return STAT_OK;
		} else {
			return execError("operator [] on non-array");
		}
	} else {
		return execError("array[] not an lvalue");
	}
}

/*
** setup symbol values for array iteration in interpreter
**
** Before: Prog->  [iter], ARRAY_ITER, iterVar, iter, endLoopBranch, next, ...
**         TheStack-> [arrayVal], next, ...
** After:  Prog->  iter, [ARRAY_ITER], iterVar, iter, endLoopBranch, next, ...
**         TheStack-> [next], ...
** Where:
**      iter is a symbol which gives the position of the iterator value in
**              the stack frame
**      arrayVal is the data value holding the array in question
*/
static int beginArrayIter() {
	Symbol *iterator;
	DataValue *iteratorValPtr;
	DataValue arrayVal;

	DISASM_RT(PC - 1, 2);
	STACKDUMP(1, 3);

	iterator = PC->sym;
	PC++;

	POP(arrayVal)

	if (iterator->type == LOCAL_SYM) {
		iteratorValPtr = &FP_GET_SYM_VAL(FrameP, iterator);
	} else {
		return execError("bad temporary iterator: %s", iterator->name.c_str());
	}

	iteratorValPtr->tag = INT_TAG;
	if (arrayVal.tag != ARRAY_TAG) {
		return execError("can't iterate non-array");
	}

	iteratorValPtr->val.arrayPtr = arrayIterateFirst(&arrayVal);
	return STAT_OK;
}

/*
** copy key to symbol if node is still valid, marked bad by a color of -1
** then move iterator to next node
** this allows iterators to progress even if you delete any node in the array
** except the item just after the current key
**
** Before: Prog->  iter, ARRAY_ITER, [iterVar], iter, endLoopBranch, next, ...
**         TheStack-> [next], ...
** After:  Prog->  iter, ARRAY_ITER, iterVar, iter, endLoopBranch, [next], ...
**         TheStack-> [next], ...      (unchanged)
** Where:
**      iter is a symbol which gives the position of the iterator value in
**              the stack frame (set up by BEGIN_ARRAY_ITER); that value refers
**              to the array and a position within it
**      iterVal is the programmer-visible symbol which will take the current
**              key value
**      endLoopBranch is the instruction offset to the instruction following the
**              loop (measured from itself)
**      arrayVal is the data value holding the array in question
** The return-to-start-of-loop branch (at the end of the loop) should address
** the ARRAY_ITER instruction
*/
static int arrayIter() {
	Symbol *iterator;
	Symbol *item;
	DataValue *iteratorValPtr;
	DataValue *itemValPtr;
	ArrayEntry *thisEntry;
	Inst *branchAddr;

	DISASM_RT(PC - 1, 4);
	STACKDUMP(0, 3);

	item = PC->sym;
	PC++;
	iterator = PC->sym;
	PC++;
	branchAddr = PC + PC->value;
	PC++;

	if (item->type == LOCAL_SYM) {
		itemValPtr = &FP_GET_SYM_VAL(FrameP, item);
	} else if (item->type == GLOBAL_SYM) {
		itemValPtr = &(item->value);
	} else {
		return execError("can't assign to: %s", item->name.c_str());
	}
	itemValPtr->tag = NO_TAG;

	if (iterator->type == LOCAL_SYM) {
		iteratorValPtr = &FP_GET_SYM_VAL(FrameP, iterator);
	} else {
		return execError("bad temporary iterator: %s", iterator->name.c_str());
	}

	thisEntry = iteratorValPtr->val.arrayPtr;
	if (thisEntry && thisEntry->color != -1) {
		itemValPtr->tag = STRING_TAG;
		itemValPtr->val.str = NString{thisEntry->key, strlen(thisEntry->key)};

		iteratorValPtr->val.arrayPtr = arrayIterateNext(thisEntry);
	} else {
		PC = branchAddr;
	}
	return STAT_OK;
}

/*
** determine if a key or keys exists in an array
** if the left argument is a string or integer a single check is performed
** if the key exists, 1 is pushed onto the stack, otherwise 0
** if the left argument is an array 1 is pushed onto the stack if every key
** in the left array exists in the right array, otherwise 0
**
** Before: Prog->  [next], ...
**         TheStack-> [ArraySym], inSymbol, next, ...
** After:  Prog->  [next], ...      -- (unchanged)
**         TheStack-> next, ...
*/
static int inArray() {
	DataValue theArray;
	DataValue leftArray;
	DataValue theValue;

	int inResult = 0;

	DISASM_RT(PC - 1, 1);
	STACKDUMP(2, 3);

	POP(theArray)
	if (theArray.tag != ARRAY_TAG) {
		return execError("operator in on non-array");
	}
	PEEK(leftArray, 0)
	if (leftArray.tag == ARRAY_TAG) {

		POP(leftArray)
		inResult = 1;
		ArrayEntry *iter = arrayIterateFirst(&leftArray);
		while (inResult && iter) {
			inResult = inResult && ArrayGet(&theArray, iter->key, &theValue);
			iter = arrayIterateNext(iter);
		}
	} else {
		char *keyStr;
		POP_STRING(keyStr)
		if (ArrayGet(&theArray, keyStr, &theValue)) {
			inResult = 1;
		}
	}
	PUSH_INT(inResult)
	return STAT_OK;
}

/*
** remove a given key from an array unless nDim is 0, then all keys are removed
**
** for use with assign-op operators (eg a[i,j] += k
** Before: Prog->  [nDim], next, ...
**         TheStack-> [indnDim], ... ind1, arrayValue, next, ...
** After:  Prog->  nDim, [next], ...
**         TheStack-> next, ...
*/
static int deleteArrayElement() {
	DataValue theArray;
	char *keyString = nullptr;
	int nDim;

	nDim = PC->value;
	PC++;

	DISASM_RT(PC - 2, 2);
	STACKDUMP(nDim + 1, 3);

	if (nDim > 0) {
		int errNum;

		errNum = makeArrayKeyFromArgs(nDim, &keyString, 0);
		if (errNum != STAT_OK) {
			return errNum;
		}
	}

	POP(theArray)
	if (theArray.tag == ARRAY_TAG) {
		if (nDim > 0) {
			ArrayDelete(&theArray, keyString);
		} else {
			ArrayDeleteAll(&theArray);
		}
	} else {
		return execError("attempt to delete from non-array");
	}
	return STAT_OK;
}

/*
** checks errno after operations which can set it.  If an error occured,
** creates appropriate error messages and returns false
*/
static int errCheck(const char *s) {
	if (errno == EDOM)
		return execError("%s argument out of domain", s);
	else if (errno == ERANGE)
		return execError("%s result out of range", s);
	else
		return STAT_OK;
}

/*
** combine two strings in a static area and set ErrMsg to point to the
** result.  Returns false so a single return execError() statement can
** be used to both process the message and return.
*/
template <class ...T>
int execError(const char *s1, T ... args) {
	static char msg[MAX_ERR_MSG_LEN];

	snprintf(msg, sizeof(msg), s1, args...);
	ErrMsg = msg;
	return STAT_ERROR;
}

bool StringToNum(const QString &string, int *number) {
    bool ok;
    *number = string.toInt(&ok);
    return ok;

}

bool StringToNum(const std::string &string, int *number) {
    auto it = string.begin();

    while (*it == ' ' || *it == '\t') {
        ++it;
    }

    if (*it == '+' || *it == '-') {
        ++it;
    }

    while (isdigit((uint8_t)*it)) {
        ++it;
    }

    while (*it == ' ' || *it == '\t') {
        ++it;
    }

    if (it != string.end()) {
        // if everything went as expected, we should be at end, but we're not
        return false;
    }

    if (number) {
        if (sscanf(string.c_str(), "%d", number) != 1) {
            // This case is here to support old behavior
            *number = 0;
        }
    }
    return true;
}

bool StringToNum(const char *string, int *number) {
	const char *c = string;

	while (*c == ' ' || *c == '\t') {
		++c;
	}
	if (*c == '+' || *c == '-') {
		++c;
	}
	while (isdigit((uint8_t)*c)) {
		++c;
	}
	while (*c == ' ' || *c == '\t') {
		++c;
	}
    if (*c != '\0') {
		// if everything went as expected, we should be at end, but we're not 
        return false;
	}
	if (number) {
		if (sscanf(string, "%d", number) != 1) {
			// This case is here to support old behavior 
			*number = 0;
		}
	}
    return true;
}

#ifdef DEBUG_DISASSEMBLER // dumping values in disassembly or stack dump 
static void dumpVal(DataValue dv) {
	switch (dv.tag) {
	case INT_TAG:
		printf("i=%d", dv.val.n);
		break;
	case STRING_TAG: {
		int k;
		char s[21];
		char *src = dv.val.str.rep;
		if (!src) {
			printf("s=<nullptr>");
		} else {
			for (k = 0; k < sizeof(s) - 1 && src[k]; k++) {
				s[k] = isprint(src[k]) ? src[k] : '?';
			}
			s[k] = 0;
			printf("s=\"%s\"%s[%d]", s, src[k] ? "..." : "", strlen(src));
		}
	} break;
	case ARRAY_TAG:
		printf("<array>");
		break;
	case NO_TAG:
		if (!dv.val.inst) {
			printf("<no value>");
		} else {
			printf("?%8p", dv.val.inst);
		}
		break;
	default:
		printf("UNKNOWN DATA TAG %d ?%8p", dv.tag, dv.val.inst);
		break;
	}
}
#endif // #ifdef DEBUG_DISASSEMBLER 

#ifdef DEBUG_DISASSEMBLER // For debugging code generation 
static void disasm(Inst *inst, int nInstr) {
	static const char *opNames[N_OPS] = {
	    "RETURN_NO_VAL",          // returnNoVal 
	    "RETURN",                 // returnVal 
	    "PUSH_SYM",               // pushSymVal 
	    "DUP",                    // dupStack 
	    "ADD",                    // add 
	    "SUB",                    // subtract 
	    "MUL",                    // multiply 
	    "DIV",                    // divide 
	    "MOD",                    // modulo 
	    "NEGATE",                 // negate 
	    "INCR",                   // increment 
	    "DECR",                   // decrement 
	    "GT",                     // gt 
	    "LT",                     // lt 
	    "GE",                     // ge 
	    "LE",                     // le 
	    "EQ",                     // eq 
	    "NE",                     // ne 
	    "BIT_AND",                // bitAnd 
	    "BIT_OR",                 // bitOr 
	    "AND",                    // and 
	    "OR",                     // or 
	    "NOT",                    // not 
	    "POWER",                  // power 
	    "CONCAT",                 // concat 
	    "ASSIGN",                 // assign 
	    "SUBR_CALL",              // callSubroutine 
	    "FETCH_RET_VAL",          // fetchRetVal 
	    "BRANCH",                 // branch 
	    "BRANCH_TRUE",            // branchTrue 
	    "BRANCH_FALSE",           // branchFalse 
	    "BRANCH_NEVER",           // branchNever 
	    "ARRAY_REF",              // arrayRef 
	    "ARRAY_ASSIGN",           // arrayAssign 
	    "BEGIN_ARRAY_ITER",       // beginArrayIter 
	    "ARRAY_ITER",             // arrayIter 
	    "IN_ARRAY",               // inArray 
	    "ARRAY_DELETE",           // deleteArrayElement 
	    "PUSH_ARRAY_SYM",         // pushArraySymVal 
	    "ARRAY_REF_ASSIGN_SETUP", // arrayRefAndAssignSetup 
	    "PUSH_ARG",               // $arg[expr] 
	    "PUSH_ARG_COUNT",         // $arg[] 
	    "PUSH_ARG_ARRAY"          // $arg 
	};
	int i, j;

	printf("\n");
	for (i = 0; i < nInstr; ++i) {
		printf("Prog %8p ", &inst[i]);
		for (j = 0; j < N_OPS; ++j) {
			if (inst[i].func == OpFns[j]) {
				printf("%22s ", opNames[j]);
				if (j == OP_PUSH_SYM || j == OP_ASSIGN) {
					Symbol *sym = inst[i + 1].sym;
					printf("%s", sym->name.c_str());
					if (sym->value.tag == STRING_TAG && strncmp(sym->name.c_str(), "string #", 8) == 0) {
						dumpVal(sym->value);
					}
					++i;
				} else if (j == OP_BRANCH || j == OP_BRANCH_FALSE || j == OP_BRANCH_NEVER || j == OP_BRANCH_TRUE) {
					printf("to=(%d) %p", inst[i + 1].value, &inst[i + 1] + inst[i + 1].value);
					++i;
				} else if (j == OP_SUBR_CALL) {
					printf("%s (%d arg)", inst[i + 1].sym->name.c_str(), inst[i + 2].value);
					i += 2;
				} else if (j == OP_BEGIN_ARRAY_ITER) {
					printf("%s in", inst[i + 1].sym->name.c_str());
					++i;
				} else if (j == OP_ARRAY_ITER) {
					printf("%s = %s++ end-loop=(%d) %p", inst[i + 1].sym->name.c_str(), inst[i + 2].sym->name.c_str(), inst[i + 3].value, &inst[i + 3] + inst[i + 3].value);
					i += 3;
				} else if (j == OP_ARRAY_REF || j == OP_ARRAY_DELETE || j == OP_ARRAY_ASSIGN) {
					printf("nDim=%d", inst[i + 1].value);
					++i;
				} else if (j == OP_ARRAY_REF_ASSIGN_SETUP) {
					printf("binOp=%s ", inst[i + 1].value ? "true" : "false");
					printf("nDim=%d", inst[i + 2].value);
					i += 2;
				} else if (j == OP_PUSH_ARRAY_SYM) {
					printf("%s", inst[++i].sym->name.c_str());
					printf(" %s", inst[i + 1].value ? "createAndRef" : "refOnly");
					++i;
				}

				printf("\n");
				break;
			}
		}
		if (j == N_OPS) {
			printf("%x\n", inst[i].value);
		}
	}
}
#endif // #ifdef DEBUG_DISASSEMBLER 

#ifdef DEBUG_STACK // for run-time stack dumping 
#define STACK_DUMP_ARG_PREFIX "Arg"
static void stackdump(int n, int extra) {
	// TheStack-> symN-sym1(FP), argArray, nArgs, oldFP, retPC, argN-arg1, next, ... 
	int nArgs = FP_GET_ARG_COUNT(FrameP);
	int i, offset;
	char buffer[sizeof(STACK_DUMP_ARG_PREFIX) + TYPE_INT_STR_SIZE(int)];
	printf("Stack ----->\n");
	for (i = 0; i < n + extra; i++) {
		char *pos = "";
		DataValue *dv = &StackP[-i - 1];
		if (dv < TheStack) {
			printf("--------------Stack base--------------\n");
			break;
		}
		offset = dv - FrameP;

		printf("%4.4s", i < n ? ">>>>" : "");
		printf("%8p ", dv);
		switch (offset) {
		case 0:
			pos = "FrameP";
			break; // first local symbol value 
		case FP_ARG_ARRAY_CACHE_INDEX:
			pos = "args";
			break; // arguments array 
		case FP_ARG_COUNT_INDEX:
			pos = "NArgs";
			break; // number of arguments 
		case FP_OLD_FP_INDEX:
			pos = "OldFP";
			break;
		case FP_RET_PC_INDEX:
			pos = "RetPC";
			break;
		default:
			if (offset < -FP_TO_ARGS_DIST && offset >= -FP_TO_ARGS_DIST - nArgs) {
				sprintf(pos = buffer, STACK_DUMP_ARG_PREFIX "%d", offset + FP_TO_ARGS_DIST + nArgs + 1);
			}
			break;
		}
		printf("%-6s ", pos);
		dumpVal(*dv);
		printf("\n");
	}
}
#endif // ifdef DEBUG_STACK 
