
#include "interpret.h"
#include "Util/utils.h"
#include <cassert>
#include <cmath>
#include <gsl/gsl_util>

// This enables preemption, useful to disable it for debugging things
#define ENABLE_PREEMPTION

// #define DEBUG_ASSEMBLY
// #define DEBUG_STACK

namespace {

// Maximum stack size
constexpr int STACK_SIZE = 1024;

constexpr int PROGRAM_SIZE      = 4096; // Maximum program size
constexpr int MAX_ERR_MSG_LEN   = 256;  // Max. length for error messages
constexpr int LOOP_STACK_SIZE   = 200;  // (Approx.) Number of break/continue stmts allowed per program
constexpr int INSTRUCTION_LIMIT = 100;  // Number of instructions the interpreter is allowed to execute before preempting and returning to allow other things to run

/* Temporary markers placed in a branch address location to designate
   which loop address (break or continue) the location needs */

constexpr int NEEDS_BREAK    = 1;
constexpr int NEEDS_CONTINUE = 2;

constexpr int N_ARGS_ARG_SYM = -1; // special arg number meaning $n_args value

enum OpStatusCodes : uint8_t {
	STAT_OK = 2,
	STAT_DONE,
	STAT_ERROR,
	STAT_PREEMPT
};

/* Message strings used in macros (so they don't get repeated every time
   the macros are used */
constexpr const char StackOverflowMsg[]          = "macro stack overflow";
constexpr const char StackUnderflowMsg[]         = "macro stack underflow";
constexpr const char StringToNumberMsg[]         = "string could not be converted to number";
constexpr const char CantConvertArrayToInteger[] = "can't convert array to integer";
constexpr const char CantConvertArrayToString[]  = "can't convert array to string";

const auto MacroTooLarge = QLatin1String("macro too large");

// Global symbols and function definitions
std::deque<Symbol *> GlobalSymList;

// Temporary global data for use while accumulating programs
std::deque<Symbol *> LocalSymList; // symbols local to the program
Inst Prog[PROGRAM_SIZE];           // the program
Inst *ProgP;                       // next free spot for code gen.
Inst *LoopStack[LOOP_STACK_SIZE];  // addresses of break, cont stmts
Inst **LoopStackPtr = LoopStack;   //  to fill at the end of a loop

// Global data for the interpreter
MacroContext Context;

const char *ErrorMessage; // global for returning error messages from executing functions
bool PreemptRequest;      // passes preemption requests from called routines back up to the interpreter

// Stack-> symN-sym0(FP), argArray, nArgs, oldFP, retPC, argN-arg1, next, ...
constexpr int FP_ARG_ARRAY_CACHE_INDEX = -1;
constexpr int FP_ARG_COUNT_INDEX       = -2;
constexpr int FP_OLD_FP_INDEX          = -3;
constexpr int FP_RET_PC_INDEX          = -4;
constexpr int FP_TO_ARGS_DIST          = 4; // should be 0 - (above index)

DataValue &FP_GET_ARG_ARRAY_CACHE(DataValue *FrameP) {
	return FrameP[FP_ARG_ARRAY_CACHE_INDEX];
}

int FP_GET_ARG_COUNT(const DataValue *FrameP) {
	return to_integer(FrameP[FP_ARG_COUNT_INDEX]);
}

DataValue *FP_GET_OLD_FP(const DataValue *FrameP) {
	return to_data_value(FrameP[FP_OLD_FP_INDEX]);
}

Inst *FP_GET_RET_PC(const DataValue *FrameP) {
	return to_instruction(FrameP[FP_RET_PC_INDEX]);
}

int FP_ARG_START_INDEX(const DataValue *FrameP) {
	return -(FP_GET_ARG_COUNT(FrameP) + FP_TO_ARGS_DIST);
}

const DataValue &FP_GET_ARG_N(const DataValue *FrameP, int n) {
	return FrameP[n + FP_ARG_START_INDEX(FrameP)];
}

DataValue &FP_GET_SYM_N(DataValue *FrameP, int n) {
	return FrameP[n];
}

DataValue &FP_GET_SYM_VAL(DataValue *FrameP, Symbol *sym) {
	return FP_GET_SYM_N(FrameP, to_integer(sym->value));
}

/*
** Save and restore execution context to data structure "context"
*/
template <class Pointer>
void saveContext(Pointer context) {
	context->Stack         = Context.Stack;
	context->StackP        = Context.StackP;
	context->FrameP        = Context.FrameP;
	context->PC            = Context.PC;
	context->RunDocument   = Context.RunDocument;
	context->FocusDocument = Context.FocusDocument;
}

template <class Pointer>
void restoreContext(Pointer context) {
	Context.Stack         = context->Stack;
	Context.StackP        = context->StackP;
	Context.FrameP        = context->FrameP;
	Context.PC            = context->PC;
	Context.RunDocument   = context->RunDocument;
	Context.FocusDocument = context->FocusDocument;
}

/*
** combine two strings in a static area and set ErrMsg to point to the
** result.  Returns false so a single return execError() statement can
** be used to both process the message and return.
*/
template <class... T>
int execError(const std::error_code &error_code, T &&...args) {
	// NOTE(eteran): this warning is not needed for this function because
	// we happen to know that the inputs for `execError` always originate
	// from string constants
	QT_WARNING_PUSH
	QT_WARNING_DISABLE_GCC("-Wformat-security")
	static char msg[MAX_ERR_MSG_LEN];
	const std::string str = error_code.message();
	qsnprintf(msg, sizeof(msg), str.c_str(), std::forward<T>(args)...);
	ErrorMessage = msg;
	return STAT_ERROR;
	QT_WARNING_POP
}

template <class... T>
int execError(const char *s1, T &&...args) {
	// NOTE(eteran): this warning is not needed for this function because
	// we happen to know that the inputs for `execError` always originate
	// from string constants
	QT_WARNING_PUSH
	QT_WARNING_DISABLE_GCC("-Wformat-security")
	static char msg[MAX_ERR_MSG_LEN];
	qsnprintf(msg, sizeof(msg), s1, std::forward<T>(args)...);
	ErrorMessage = msg;
	return STAT_ERROR;
	QT_WARNING_POP
}

/*
** checks errno after operations which can set it.  If an error occurred,
** creates appropriate error messages and returns false
*/
int errCheck(const char *s) {
	switch (errno) {
	case EDOM:
		return execError("%s argument out of domain", s);
	case ERANGE:
		return execError("%s result out of range", s);
	default:
		return STAT_OK;
	}
}

}

static void addLoopAddr(Inst *addr);
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

static ArrayIterator arrayIterateFirst(DataValue *theArray);
static ArrayIterator arrayIterateNext(ArrayIterator iterator);

#if defined(DEBUG_ASSEMBLY) || defined(DEBUG_STACK)
#define DEBUG_DISASSEMBLER
static void disasm(Inst *inst, size_t nInstr);
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

/* Array for mapping operations to functions for performing the operations
   Must correspond to the enum called "operations" in interpret.h */

using operation_type = int (*)();

static const operation_type OpFns[N_OPS] = {
	returnNoVal,
	returnVal,
	pushSymVal,
	dupStack,
	add,
	subtract,
	multiply,
	divide,
	modulo,
	negate,
	increment,
	decrement,
	gt,
	lt,
	ge,
	le,
	eq,
	ne,
	bitAnd,
	bitOr,
	logicalAnd,
	logicalOr,
	logicalNot,
	power,
	concat,
	assign,
	callSubroutine,
	fetchRetVal,
	branch,
	branchTrue,
	branchFalse,
	branchNever,
	arrayRef,
	arrayAssign,
	beginArrayIter,
	arrayIter,
	inArray,
	deleteArrayElement,
	pushArraySymVal,
	arrayRefAndAssignSetup,
	pushArgVal,
	pushArgCount,
	pushArgArray,
};

/*
** Initialize macro language global variables.  Must be called before
** any macros are even parsed, because the parser uses action routine
** symbols to comprehend hyphenated names.
*/
void InitMacroGlobals() {

	// Add subroutine argument symbols ($1, $2, ..., $9)
	for (int i = 0; i < 9; i++) {
		static char argName[3] = "$x";
		argName[1]             = static_cast<char>('1' + i);
		InstallSymbol(argName, ARG_SYM, make_value(i));
	}

	// Add special symbol $n_args
	InstallSymbol("$n_args", ARG_SYM, make_value(N_ARGS_ARG_SYM));
}

/**
 * @brief CleanupMacroGlobals
 */
void CleanupMacroGlobals() {
	for (Symbol *sym : GlobalSymList) {
		delete sym;
	}
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

	auto newProg = std::make_unique<Program>();
	std::copy(Prog, ProgP, std::back_inserter(newProg->code));

	newProg->localSymList = LocalSymList;
	LocalSymList.clear();

	int fpOffset = 0;

	/* Local variables' values are stored on the stack.  Here we assign
	   frame pointer offsets to them. */
	for (Symbol *s : newProg->localSymList) {
		s->value = make_value(fpOffset++);
	}

	DISASM(newProg->code.data(), newProg->code.size());
	return newProg.release();
}

/*
** Add an operator (instruction) to the end of the current program
*/
bool AddOp(int op, QString *msg) {
	if (ProgP >= &Prog[PROGRAM_SIZE]) {
		*msg = MacroTooLarge;
		return false;
	}

	ProgP++->func = OpFns[op];
	return true;
}

/*
** Add a symbol operand to the current program
*/
bool AddSym(Symbol *sym, QString *msg) {
	if (ProgP >= &Prog[PROGRAM_SIZE]) {
		*msg = MacroTooLarge;
		return false;
	}

	ProgP++->sym = sym;
	return true;
}

/*
** Add an immediate value operand to the current program
*/
bool AddImmediate(int value, QString *msg) {
	if (ProgP >= &Prog[PROGRAM_SIZE]) {
		*msg = MacroTooLarge;
		return false;
	}

	ProgP++->value = value;
	return true;
}

/*
** Add a branch offset operand to the current program
*/
bool AddBranchOffset(const Inst *to, QString *msg) {
	if (ProgP >= &Prog[PROGRAM_SIZE]) {
		*msg = MacroTooLarge;
		return false;
	}

	/* we don't use gsl::narrow here because when to is nullptr (to indicate
	 * end of program) it produces values that won't fit into an int on
	 * 64-bit systems */
	ProgP->value = static_cast<int>(to - ProgP);
	ProgP++;
	return true;
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

bool AddBreakAddr(Inst *addr) {
	if (LoopStackPtr == LoopStack) {
		return true;
	}

	addLoopAddr(addr);
	addr->value = NEEDS_BREAK;
	return false;
}

bool AddContinueAddr(Inst *addr) {
	if (LoopStackPtr == LoopStack) {
		return true;
	}

	addLoopAddr(addr);
	addr->value = NEEDS_CONTINUE;
	return false;
}

static void addLoopAddr(Inst *addr) {
	if (LoopStackPtr > &LoopStack[LOOP_STACK_SIZE - 1]) {
		qCritical("NEdit: loop stack overflow in macro parser");
		return;
	}
	*LoopStackPtr++ = addr;
}

void FillLoopAddrs(const Inst *breakAddr, const Inst *continueAddr) {
	while (true) {
		LoopStackPtr--;
		if (LoopStackPtr < LoopStack) {
			qCritical("NEdit: internal error (lsu) in macro parser");
			return;
		}

		if (*LoopStackPtr == nullptr) {
			break;
		}

		if ((*LoopStackPtr)->value == NEEDS_BREAK) {
			(*LoopStackPtr)->value = breakAddr - *LoopStackPtr;
		} else if ((*LoopStackPtr)->value == NEEDS_CONTINUE) {
			(*LoopStackPtr)->value = continueAddr - *LoopStackPtr;
		} else {
			qCritical("NEdit: internal error (uat) in macro parser");
		}
	}
}

/*
** Execute a compiled macro, "prog", using the arguments in the array
** "args".  Returns one of MACRO_DONE, MACRO_PREEMPT, or MACRO_ERROR.
** if MACRO_DONE is returned, the macro completed, and the returned value
** (if any) can be read from "result".  If MACRO_PREEMPT is returned, the
** macro exceeded its allotted time-slice and scheduled...
*/
int executeMacro(DocumentWidget *document, Program *prog, gsl::span<DataValue> arguments, DataValue *result, std::shared_ptr<MacroContext> &continuation, QString *msg) {

	/* Create an execution context (a stack, a stack pointer, a frame pointer,
	   and a program counter) which will retain the program state across
	   preemption and resumption of execution */

	auto context           = std::make_shared<MacroContext>();
	context->Stack         = std::shared_ptr<DataValue>(new DataValue[STACK_SIZE], std::default_delete<DataValue[]>());
	context->StackP        = context->Stack.get();
	context->PC            = prog->code.data();
	context->RunDocument   = document;
	context->FocusDocument = document;

	continuation = context;

	// Push arguments and call information onto the stack
	for (const DataValue &dv : arguments) {
		*(context->StackP++) = dv;
	}

	*(context->StackP++) = make_value(static_cast<Inst *>(nullptr));       // return PC
	*(context->StackP++) = make_value(static_cast<DataValue *>(nullptr));  // old FrameP
	*(context->StackP++) = make_value(static_cast<int>(arguments.size())); // nArgs
	*(context->StackP++) = make_value();                                   // cached arg array

	context->FrameP = context->StackP;

	// Initialize and make room on the stack for local variables
	for (Symbol *s : prog->localSymList) {
		FP_GET_SYM_VAL(context->FrameP, s) = make_value();
		context->StackP++;
	}

	// Begin execution, return on error or preemption
	return continueMacro(context, result, msg);
}

/*
** Continue the execution of a suspended macro whose state is described in
** "continuation"
*/
ExecReturnCodes continueMacro(const std::shared_ptr<MacroContext> &continuation, DataValue *result, QString *msg) {

	int instCount = 0;

	/* To allow macros to be invoked arbitrarily (such as those automatically
	   triggered within smart-indent) within executing macros, this call is
	   reentrant. */
	MacroContext oldContext;
	saveContext(&oldContext);

	Q_ASSERT(continuation);

	/*
	** Execution Loop:  Call the successive routine addresses in the program
	** until one returns something other than STAT_OK, then take action
	*/
	restoreContext(continuation);
	ErrorMessage = nullptr;
	Q_FOREVER {

		// Execute an instruction
		Inst *inst = Context.PC++;

		auto status = static_cast<OpStatusCodes>(inst->func());

		// If error return was not STAT_OK, return to caller
		switch (status) {
		case STAT_PREEMPT:
			saveContext(continuation);
			restoreContext(&oldContext);
			return MACRO_PREEMPT;
		case STAT_ERROR:
			*msg = QString::fromLatin1(ErrorMessage);
			restoreContext(&oldContext);
			return MACRO_ERROR;
		case STAT_DONE:
			*msg    = QString();
			*result = *--Context.StackP;
			restoreContext(&oldContext);
			return MACRO_DONE;
		case STAT_OK:
			break;
		}

		/* Count instructions executed.  If the instruction limit is hit,
		   preempt, store re-start information in continuation and give
		   X, other macros, and other shell scripts a chance to execute */
		++instCount;
#if defined(ENABLE_PREEMPTION)
		if (instCount >= INSTRUCTION_LIMIT) {
			saveContext(continuation);
			restoreContext(&oldContext);
			return MACRO_TIME_LIMIT;
		}
#endif
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

	/* See "callSubroutine" for a description of the stack frame
	   for a subroutine call */
	*Context.StackP++ = make_value(Context.PC);     // return PC
	*Context.StackP++ = make_value(Context.FrameP); // old FrameP
	*Context.StackP++ = make_value(0);              // nArgs
	*Context.StackP++ = make_value();               // cached arg array

	Context.FrameP = Context.StackP;
	Context.PC     = prog->code.data();

	for (Symbol *s : prog->localSymList) {
		FP_GET_SYM_VAL(Context.FrameP, s) = make_value();
		Context.StackP++;
	}
}

/*
** Cause a macro in progress to be preempted (called by commands which take
** a long time, or want to return to the event loop.  Call ResumeMacroExecution
** to resume.
*/
void preemptMacro() {
	PreemptRequest = true;
}

/*
** Reset the return value for a subroutine which caused preemption (this is
** how to return a value from a routine which preempts instead of returning
** a value directly).
*/
void modifyReturnedValue(const std::shared_ptr<MacroContext> &context, const DataValue &dv) {
	if ((context->PC - 1)->func == fetchRetVal) {
		*(context->StackP - 1) = dv;
	}
}

/*
** Called within a routine invoked from a macro, returns the document in
** which the macro is executing (where the banner is, not where it is focused)
*/
DocumentWidget *MacroRunDocument() {
	return Context.RunDocument;
}

/*
** Called within a routine invoked from a macro, returns the document to which
** the currently executing macro is focused (the window which macro commands
** modify, not the window from which the macro is being run)
*/
DocumentWidget *MacroFocusDocument() {
	return Context.FocusDocument;
}

/*
** Set the window to which macro subroutines and actions which operate on an
** implied window are directed.
*/
void SetMacroFocusDocument(DocumentWidget *document) {
	Context.FocusDocument = document;
}

/*
** install an array iteration symbol
*/
Symbol *InstallIteratorSymbol() {

	static int interatorNameIndex = 0;

	auto symbolName = QStringLiteral("aryiter %1").arg(interatorNameIndex++);

	return InstallSymbolEx(symbolName, LOCAL_SYM, make_value(ArrayIterator()));
}

/*
** Lookup a constant string by its value.
*/
Symbol *LookupStringConstSymbol(std::string_view value) {

	auto it = std::find_if(GlobalSymList.begin(), GlobalSymList.end(), [value](Symbol *s) {
		return (s->type == CONST_SYM && is_string(s->value) && to_string(s->value) == value);
	});

	if (it != GlobalSymList.end()) {
		return *it;
	}

	return nullptr;
}

Symbol *InstallStringConstSymbolEx(const QString &str) {
	return InstallStringConstSymbol(str.toStdString());
}

/*
** install string str in the global symbol table with a string name
*/
Symbol *InstallStringConstSymbol(std::string_view str) {

	static int stringConstIndex = 0;

	if (Symbol *sym = LookupStringConstSymbol(str)) {
		return sym;
	}

	auto stringName = QStringLiteral("string #%1").arg(stringConstIndex++);

	DataValue value = make_value(str);
	return InstallSymbolEx(stringName, CONST_SYM, value);
}

/*
** find a symbol in the symbol table
*/
Symbol *LookupSymbolEx(const QString &name) {
	return LookupSymbol(name.toStdString());
}

Symbol *LookupSymbol(std::string_view name) {

	// first look for a local symbol
	auto local = std::find_if(LocalSymList.begin(), LocalSymList.end(), [name](Symbol *s) {
		return (s->name == name);
	});

	if (local != LocalSymList.end()) {
		return *local;
	}

	// then a global symbol
	auto global = std::find_if(GlobalSymList.begin(), GlobalSymList.end(), [name](Symbol *s) {
		return (s->name == name);
	});

	if (global != GlobalSymList.end()) {
		return *global;
	}

	return nullptr;
}

/*
** install symbol name in symbol table
*/
Symbol *InstallSymbolEx(const QString &name, SymTypes type, const DataValue &value) {
	return InstallSymbol(name.toStdString(), type, value);
}

Symbol *InstallSymbol(const std::string &name, SymTypes type, const DataValue &value) {

	auto s = new Symbol{name, type, value};

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
		qInfo("NEdit: To boldly go where no local sym has gone before: %s", sym->name.c_str());
		sym->type = GLOBAL_SYM;
		return sym;
	}

	if (s != nullptr) {
		/* case b)
		   sym will shadow the old symbol from the GlobalSymList */
		qInfo("NEdit: duplicate symbol in LocalSymList and GlobalSymList: %s", sym->name.c_str());
	}

	/* Add the symbol directly to the GlobalSymList, because InstallSymbol()
	 * will allocate a new Symbol, which results in a memory leak of sym.
	 * Don't use MACRO_FUNCTION_SYM as type */
	sym->type = GLOBAL_SYM;

	GlobalSymList.push_back(sym);

	return sym;
}

#define POP(dataVal)                               \
	do {                                           \
		if (Context.StackP == Context.Stack.get()) \
			return execError(StackUnderflowMsg);   \
		(dataVal) = *--Context.StackP;             \
	} while (0)

#define PUSH(dataVal)                                           \
	do {                                                        \
		if (Context.StackP >= &Context.Stack.get()[STACK_SIZE]) \
			return execError(StackOverflowMsg);                 \
		*Context.StackP++ = (dataVal);                          \
	} while (0)

#define PEEK(dataVal, peekIndex)                         \
	do {                                                 \
		(dataVal) = *(Context.StackP - (peekIndex) - 1); \
	} while (0)

#define POP_INT(number)                                              \
	do {                                                             \
		if (Context.StackP == Context.Stack.get())                   \
			return execError(StackUnderflowMsg);                     \
		--Context.StackP;                                            \
		if (is_string(*Context.StackP)) {                            \
			if (!StringToNum(to_string(*Context.StackP), &(number))) \
				return execError(StringToNumberMsg);                 \
		} else if (is_integer(*Context.StackP))                      \
			(number) = to_integer(*Context.StackP);                  \
		else                                                         \
			return execError(CantConvertArrayToInteger);             \
	} while (0)

#define POP_STRING(string_ref)                                          \
	do {                                                                \
		if (Context.StackP == Context.Stack.get())                      \
			return execError(StackUnderflowMsg);                        \
		--Context.StackP;                                               \
		if (is_integer(*Context.StackP)) {                              \
			(string_ref) = std::to_string(to_integer(*Context.StackP)); \
		} else if (is_string(*Context.StackP)) {                        \
			(string_ref) = to_string(*Context.StackP);                  \
		} else {                                                        \
			return execError(CantConvertArrayToString);                 \
		}                                                               \
	} while (0)

#define PUSH_INT(number)                                        \
	do {                                                        \
		if (Context.StackP >= &Context.Stack.get()[STACK_SIZE]) \
			return execError(StackOverflowMsg);                 \
		*Context.StackP++ = make_value(number);                 \
	} while (0)

#define PUSH_STRING(string)                                     \
	do {                                                        \
		if (Context.StackP >= &Context.Stack.get()[STACK_SIZE]) \
			return execError(StackOverflowMsg);                 \
		*Context.StackP++ = make_value(string);                 \
	} while (0)

#define BINARY_NUMERIC_OPERATION(op) \
	do {                             \
		int n1;                      \
		int n2;                      \
		DISASM_RT(PC - 1, 1);        \
		STACKDUMP(2, 3);             \
		POP_INT(n2);                 \
		POP_INT(n1);                 \
		PUSH_INT(n1 op n2);          \
		return STAT_OK;              \
	} while (0)

#define UNARY_NUMERIC_OPERATION(op) \
	do {                            \
		int n;                      \
		DISASM_RT(PC - 1, 1);       \
		STACKDUMP(1, 3);            \
		POP_INT(n);                 \
		PUSH_INT(op n);             \
		return STAT_OK;             \
	} while (0)

/*
** copy a symbol's value onto the stack
** Before: Prog->  [Sym], next, ...
**         TheStack-> next, ...
** After:  Prog->  Sym, [next], ...
**         TheStack-> [symVal], next, ...
*/
static int pushSymVal() {

	DataValue symVal;

	DISASM_RT(PC - 1, 2);
	STACKDUMP(0, 3);

	Symbol *s = Context.PC++->sym;

	if (s->type == LOCAL_SYM) {
		symVal = FP_GET_SYM_VAL(Context.FrameP, s);
	} else if (s->type == GLOBAL_SYM || s->type == CONST_SYM) {
		symVal = s->value;
	} else if (s->type == ARG_SYM) {
		const int nArgs  = FP_GET_ARG_COUNT(Context.FrameP);
		const int argNum = to_integer(s->value);
		if (argNum >= nArgs) {
			return execError("referenced undefined argument: %s", s->name.c_str());
		}
		if (argNum == N_ARGS_ARG_SYM) {
			symVal = make_value(nArgs);
		} else {
			symVal = FP_GET_ARG_N(Context.FrameP, argNum);
		}
	} else if (s->type == PROC_VALUE_SYM) {

		if (const std::error_code ec = (to_subroutine(s->value))(Context.FocusDocument, {}, &symVal)) {
			return execError(ec, s->name.c_str());
		}
	} else {
		return execError("reading non-variable: %s", s->name.c_str());
	}

	if (is_unset(symVal)) {
		return execError("variable not set: %s", s->name.c_str());
	}

	PUSH(symVal);

	return STAT_OK;
}

static int pushArgVal() {
	int argNum;

	DISASM_RT(PC - 1, 1);
	STACKDUMP(1, 3);

	POP_INT(argNum);
	--argNum;

	const int nArgs = FP_GET_ARG_COUNT(Context.FrameP);
	if (argNum >= nArgs || argNum < 0) {
		auto argStr = std::to_string(argNum + 1);
		return execError("referenced undefined argument: $args[%s]", argStr.c_str());
	}
	PUSH(FP_GET_ARG_N(Context.FrameP, argNum));
	return STAT_OK;
}

static int pushArgCount() {
	DISASM_RT(PC - 1, 1);
	STACKDUMP(0, 3);

	PUSH_INT(FP_GET_ARG_COUNT(Context.FrameP));
	return STAT_OK;
}

static int pushArgArray() {

	DataValue argVal;

	DISASM_RT(PC - 1, 1);
	STACKDUMP(0, 3);

	const int nArgs        = FP_GET_ARG_COUNT(Context.FrameP);
	DataValue *resultArray = &FP_GET_ARG_ARRAY_CACHE(Context.FrameP);

	if (!is_array(*resultArray)) {

		*resultArray = make_value(std::make_shared<Array>());

		for (int argNum = 0; argNum < nArgs; ++argNum) {

			auto intStr = std::to_string(argNum);

			argVal = FP_GET_ARG_N(Context.FrameP, argNum);
			if (!ArrayInsert(resultArray, intStr, &argVal)) {
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

	Symbol *sym             = Context.PC++->sym;
	const int64_t initEmpty = Context.PC++->value;

	if (sym->type == LOCAL_SYM) {
		dataPtr = &FP_GET_SYM_VAL(Context.FrameP, sym);
	} else if (sym->type == GLOBAL_SYM) {
		dataPtr = &sym->value;
	} else {
		return execError("assigning to non-lvalue array or non-array: %s", sym->name.c_str());
	}

	if (initEmpty && is_unset(*dataPtr)) {
		*dataPtr = make_value(std::make_shared<Array>());
	}

	if (is_unset(*dataPtr)) {
		return execError("variable not set: %s", sym->name.c_str());
	}

	PUSH(*dataPtr);

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

	DataValue *dataPtr;
	DataValue value;

	DISASM_RT(PC - 1, 2);
	STACKDUMP(1, 3);

	Symbol *sym = Context.PC++->sym;

	switch (sym->type) {
	case LOCAL_SYM:
		dataPtr = &FP_GET_SYM_VAL(Context.FrameP, sym);
		break;
	case GLOBAL_SYM:
		dataPtr = &sym->value;
		break;
	case ARG_SYM:
		return execError("assignment to function argument: %s", sym->name.c_str());
	case PROC_VALUE_SYM:
		return execError("assignment to read-only variable: %s", sym->name.c_str());
	default:
		return execError("assignment to non-variable: %s", sym->name.c_str());
	}

	POP(value);

	if (is_array(value)) {
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

	PEEK(value, 0);
	PUSH(value);

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
	DataValue leftVal;
	DataValue rightVal;

	DISASM_RT(PC - 1, 1);
	STACKDUMP(2, 3);

	PEEK(rightVal, 0);
	if (is_array(rightVal)) {

		PEEK(leftVal, 1);
		if (is_array(leftVal)) {

			DataValue resultArray = make_value(std::make_shared<Array>());

			POP(rightVal);
			POP(leftVal);

			const ArrayPtr &leftMap  = to_array(leftVal);
			const ArrayPtr &rightMap = to_array(rightVal);

			auto leftIter  = leftMap->begin();
			auto rightIter = rightMap->begin();

			while (leftIter != leftMap->end() || rightIter != rightMap->end()) {

				bool insertResult = true;

				if (leftIter != leftMap->end() && rightIter != rightMap->end()) {
					const int compareResult = leftIter->first.compare(rightIter->first);
					if (compareResult < 0) {
						insertResult = ArrayInsert(&resultArray, leftIter->first, &leftIter->second);
						++leftIter;
					} else if (compareResult > 0) {
						insertResult = ArrayInsert(&resultArray, rightIter->first, &rightIter->second);
						++rightIter;
					} else {
						insertResult = ArrayInsert(&resultArray, rightIter->first, &rightIter->second);
						++leftIter;
						++rightIter;
					}
				} else if (leftIter != leftMap->end()) {
					insertResult = ArrayInsert(&resultArray, leftIter->first, &leftIter->second);
					++leftIter;
				} else {
					insertResult = ArrayInsert(&resultArray, rightIter->first, &rightIter->second);
					++rightIter;
				}
				if (!insertResult) {
					return execError("array insertion failure");
				}
			}
			PUSH(resultArray);
		} else {
			return execError("can't mix math with arrays and non-arrays");
		}
	} else {
		int n1;
		int n2;

		POP_INT(n2);
		POP_INT(n1);
		PUSH_INT(n1 + n2);
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

	DataValue leftVal;
	DataValue rightVal;

	DISASM_RT(PC - 1, 1);
	STACKDUMP(2, 3);

	PEEK(rightVal, 0);
	if (is_array(rightVal)) {
		PEEK(leftVal, 1);
		if (is_array(leftVal)) {

			DataValue resultArray = make_value(std::make_shared<Array>());

			POP(rightVal);
			POP(leftVal);

			const ArrayPtr &leftMap  = to_array(leftVal);
			const ArrayPtr &rightMap = to_array(rightVal);

			auto leftIter  = leftMap->begin();
			auto rightIter = rightMap->begin();

			while (leftIter != leftMap->end()) {
				bool insertResult = true;

				if (leftIter != leftMap->end() && rightIter != rightMap->end()) {
					const int compareResult = leftIter->first.compare(rightIter->first);
					if (compareResult < 0) {
						insertResult = ArrayInsert(&resultArray, leftIter->first, &leftIter->second);
						++leftIter;
					} else if (compareResult > 0) {
						++rightIter;
					} else {
						++leftIter;
						++rightIter;
					}
				} else if (leftIter != leftMap->end()) {
					insertResult = ArrayInsert(&resultArray, leftIter->first, &leftIter->second);
					++leftIter;
				}
				if (!insertResult) {
					return execError("array insertion failure");
				}
			}
			PUSH(resultArray);
		} else {
			return execError("can't mix math with arrays and non-arrays");
		}
	} else {
		int n1;
		int n2;

		POP_INT(n2);
		POP_INT(n1);
		PUSH_INT(n1 - n2);
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
	BINARY_NUMERIC_OPERATION(*);
}

static int divide() {
	int n1;
	int n2;

	DISASM_RT(PC - 1, 1);
	STACKDUMP(2, 3);

	POP_INT(n2);
	POP_INT(n1);
	if (n2 == 0) {
		return execError("division by zero");
	}
	PUSH_INT(n1 / n2);
	return STAT_OK;
}

static int modulo() {
	int n1;
	int n2;

	DISASM_RT(PC - 1, 1);
	STACKDUMP(2, 3);

	POP_INT(n2);
	POP_INT(n1);
	if (n2 == 0) {
		return execError("modulo by zero");
	}
	PUSH_INT(n1 % n2);
	return STAT_OK;
}

static int negate() {
	UNARY_NUMERIC_OPERATION(-);
}

static int increment() {
	UNARY_NUMERIC_OPERATION(++);
}

static int decrement() {
	UNARY_NUMERIC_OPERATION(--);
}

static int gt() {
	BINARY_NUMERIC_OPERATION(>);
}

static int lt() {
	BINARY_NUMERIC_OPERATION(<);
}

static int ge() {
	BINARY_NUMERIC_OPERATION(>=);
}

static int le() {
	BINARY_NUMERIC_OPERATION(<=);
}

/*
** verify that compares are between integers and/or strings only
** Before: TheStack-> value1, value2, next, ...
** After:  TheStack-> resValue, next, ...
** where resValue is 1 for true, 0 for false
*/
static int eq() {
	DataValue v1;
	DataValue v2;

	DISASM_RT(PC - 1, 1);
	STACKDUMP(2, 3);

	POP(v1);
	POP(v2);

	if (is_integer(v1) && is_integer(v2)) {
		auto n1 = to_integer(v1);
		auto n2 = to_integer(v2);
		v1      = make_value(n1 == n2);
	} else if (is_string(v1) && is_string(v2)) {
		auto s1 = to_string(v1);
		auto s2 = to_string(v2);
		v1      = make_value(s1 == s2);
	} else if (is_string(v1) && is_integer(v2)) {
		int number;
		if (!StringToNum(to_string(v1), &number)) {
			v1 = make_value(0);
		} else {
			v1 = make_value(number == to_integer(v2));
		}
	} else if (is_string(v2) && is_integer(v1)) {
		int number;
		if (!StringToNum(to_string(v2), &number)) {
			v1 = make_value(0);
		} else {
			v1 = make_value(number == to_integer(v1));
		}
	} else {
		return execError("incompatible types to compare");
	}

	PUSH(v1);
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

	DataValue leftVal;
	DataValue rightVal;

	DISASM_RT(PC - 1, 1);
	STACKDUMP(2, 3);

	PEEK(rightVal, 0);
	if (is_array(rightVal)) {
		PEEK(leftVal, 1);
		if (is_array(leftVal)) {

			DataValue resultArray = make_value(std::make_shared<Array>());

			POP(rightVal);
			POP(leftVal);

			const ArrayPtr &leftMap  = to_array(leftVal);
			const ArrayPtr &rightMap = to_array(rightVal);

			auto leftIter  = leftMap->begin();
			auto rightIter = rightMap->begin();

			while (leftIter != leftMap->end() && rightIter != rightMap->end()) {
				bool insertResult       = true;
				const int compareResult = leftIter->first.compare(rightIter->first);

				if (compareResult < 0) {
					++leftIter;
				} else if (compareResult > 0) {
					++rightIter;
				} else {
					insertResult = ArrayInsert(&resultArray, rightIter->first, &rightIter->second);
					++leftIter;
					++rightIter;
				}
				if (!insertResult) {
					return execError("array insertion failure");
				}
			}
			PUSH(resultArray);
		} else {
			return execError("can't mix math with arrays and non-arrays");
		}
	} else {
		int n1;
		int n2;

		POP_INT(n2);
		POP_INT(n1);
		PUSH_INT(n1 & n2);
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

	DataValue leftVal;
	DataValue rightVal;

	DISASM_RT(PC - 1, 1);
	STACKDUMP(2, 3);

	PEEK(rightVal, 0);
	if (is_array(rightVal)) {
		PEEK(leftVal, 1);
		if (is_array(leftVal)) {

			DataValue resultArray = make_value(std::make_shared<Array>());

			POP(rightVal);
			POP(leftVal);

			const ArrayPtr &leftMap  = to_array(leftVal);
			const ArrayPtr &rightMap = to_array(rightVal);

			auto leftIter  = leftMap->begin();
			auto rightIter = rightMap->begin();

			while (leftIter != leftMap->end() || rightIter != rightMap->end()) {
				bool insertResult = true;

				if (leftIter != leftMap->end() && rightIter != rightMap->end()) {
					const int compareResult = leftIter->first.compare(rightIter->first);
					if (compareResult < 0) {
						insertResult = ArrayInsert(&resultArray, leftIter->first, &leftIter->second);
						++leftIter;
					} else if (compareResult > 0) {
						insertResult = ArrayInsert(&resultArray, rightIter->first, &rightIter->second);
						++rightIter;
					} else {
						++leftIter;
						++rightIter;
					}
				} else if (leftIter != leftMap->end()) {
					insertResult = ArrayInsert(&resultArray, leftIter->first, &leftIter->second);
					++leftIter;
				} else {
					insertResult = ArrayInsert(&resultArray, rightIter->first, &rightIter->second);
					++rightIter;
				}
				if (!insertResult) {
					return execError("array insertion failure");
				}
			}
			PUSH(resultArray);
		} else {
			return execError("can't mix math with arrays and non-arrays");
		}
	} else {
		int n1;
		int n2;
		POP_INT(n2);
		POP_INT(n1);
		PUSH_INT(n1 | n2);
	}
	return STAT_OK;
}

static int logicalAnd() {
	BINARY_NUMERIC_OPERATION(&&);
}

static int logicalOr() {
	BINARY_NUMERIC_OPERATION(||);
}

static int logicalNot() {
	UNARY_NUMERIC_OPERATION(!);
}

/*
** raise one number to the power of another
** Before: TheStack-> raisedBy, number, next, ...
** After:  TheStack-> result, next, ...
*/
static int power() {
	int n1;
	int n2;
	int n3;

	DISASM_RT(PC - 1, 1);
	STACKDUMP(2, 3);

	POP_INT(n2);
	POP_INT(n1);
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
			n3 = static_cast<int>(pow(static_cast<double>(n1), static_cast<double>(n2)));
		}
	} else {
		// round to nearest integer
		// NOTE(eteran): this use to detect if the result would be negative and round by adding/subtracting
		// 0.5 before the final cast to int
		// this SHOULD be the equivalent to that
		n3 = static_cast<int>(std::lround(pow(static_cast<double>(n1), static_cast<double>(n2))));
	}

	PUSH_INT(n3);
	return errCheck("exponentiation");
}

/*
** concatenate two top items on the stack
** Before: TheStack-> str2, str1, next, ...
** After:  TheStack-> result, next, ...
*/
static int concat() {
	std::string s1;
	std::string s2;

	DISASM_RT(PC - 1, 1);
	STACKDUMP(2, 3);

	POP_STRING(s2);
	POP_STRING(s1);

	const std::string out = s1 + s2;

	PUSH_STRING(out);
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

	Symbol *sym   = Context.PC++->sym;
	int64_t nArgs = Context.PC++->value;

	DISASM_RT(PC - 3, 3);
	STACKDUMP(nArgs, 3);

	/*
	** If the subroutine is built-in, call the built-in routine
	*/
	if (sym->type == C_FUNCTION_SYM) {
		DataValue result;

		// "pop" stack back to the first argument in the call stack
		Context.StackP -= nArgs;

		// Call the function and check for preemption
		PreemptRequest = false;

		if (const std::error_code ec = to_subroutine(sym->value)(Context.FocusDocument, Arguments(Context.StackP, nArgs), &result)) {
			return execError(ec, sym->name.c_str());
		}

		if (Context.PC->func == fetchRetVal) {

			if (is_unset(result)) {
				return execError("%s does not return a value", sym->name.c_str());
			}

			PUSH(result);
			Context.PC++;
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

		*Context.StackP++ = make_value(Context.PC);     // return PC
		*Context.StackP++ = make_value(Context.FrameP); // old FrameP
		*Context.StackP++ = make_value(nArgs);          // nArgs
		*Context.StackP++ = make_value();               // cached arg array

		Context.FrameP = Context.StackP;
		Program *prog  = to_program(sym->value);
		Context.PC     = prog->code.data();

		for (Symbol *s : prog->localSymList) {
			FP_GET_SYM_VAL(Context.FrameP, s) = make_value();
			Context.StackP++;
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

	DISASM_RT(PC - 1, 1);
	STACKDUMP(StackP - FrameP + FP_GET_ARG_COUNT(FrameP) + FP_TO_ARGS_DIST, 3);

	// return value is on the stack
	if (valOnStack) {
		POP(retVal);
	}

	// get stored return information
	const int nArgs      = FP_GET_ARG_COUNT(Context.FrameP);
	DataValue *newFrameP = FP_GET_OLD_FP(Context.FrameP);
	Context.PC           = FP_GET_RET_PC(Context.FrameP);

	// pop past local variables
	Context.StackP = Context.FrameP;
	// pop past function arguments
	Context.StackP -= (FP_TO_ARGS_DIST + nArgs);
	Context.FrameP = newFrameP;

	// push returned value, if requested
	if (!Context.PC) {
		if (valOnStack) {
			PUSH(retVal);
		} else {
			PUSH(make_value());
		}
	} else if (Context.PC->func == fetchRetVal) {
		if (valOnStack) {
			PUSH(retVal);
			Context.PC++;
		} else {
			return execError("using return value of %s which does not return a value", ((Context.PC - 2)->sym->name.c_str()));
		}
	}

	// nullptr return PC indicates end of program
	return (Context.PC == nullptr) ? STAT_DONE : STAT_OK;
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

	Context.PC += Context.PC->value;
	return STAT_OK;
}

/*
** Conditional branches if stack value is true/false (non-zero/0) to address
** of immediate operand (pops stack)
**
** Before: Prog->  [branchDest], next, ..., (branchdest)next
** After:  either: Prog->  branchDest, [next], ...
** After:  or:     Prog->  branchDest, next, ..., (branchdest)[next]
*/
static int branchTrue() {
	int value;

	DISASM_RT(PC - 1, 2);
	STACKDUMP(1, 3);

	POP_INT(value);
	Inst *addr = Context.PC + Context.PC->value;
	Context.PC++;

	if (value) {
		Context.PC = addr;
	}

	return STAT_OK;
}

static int branchFalse() {
	int value;

	DISASM_RT(PC - 1, 2);
	STACKDUMP(1, 3);

	POP_INT(value);
	Inst *addr = Context.PC + Context.PC->value;
	Context.PC++;

	if (!value) {
		Context.PC = addr;
	}

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

	Context.PC++;
	return STAT_OK;
}

/*
** recursively copy(duplicate) the sparse array nodes of an array
** this does not duplicate the key/node data since they are never
** modified, only replaced
*/
int ArrayCopy(DataValue *dstArray, const DataValue *srcArray) {
	*dstArray = *srcArray;
	return STAT_OK;
}

/*
** creates a string of a single key for all the sub-scripts
** using ARRAY_DIM_SEP as a separator
** this function uses the PEEK macros in order to remove most limits on
** the number of arguments to an array
*/
static int makeArrayKeyFromArgs(int64_t nArgs, std::string *keyString, bool leaveParams) {
	DataValue tmpVal;

	std::string str;

	for (int64_t i = nArgs - 1; i >= 0; --i) {
		if (i != nArgs - 1) {
			str.append(ARRAY_DIM_SEP);
		}
		PEEK(tmpVal, i);
		if (is_integer(tmpVal)) {
			str.append(std::to_string(to_integer(tmpVal)));
		} else if (is_string(tmpVal)) {
			auto s = to_string(tmpVal);
			str.append(s.begin(), s.end());
		} else {
			return execError("can only index array with string or int.");
		}
	}

	if (!leaveParams) {
		for (int64_t i = nArgs - 1; i >= 0; --i) {
			POP(tmpVal);
		}
	}

	*keyString = str;

	return STAT_OK;
}

/*
** insert a DataValue into an array
*/
bool ArrayInsert(DataValue *theArray, const std::string &keyStr, DataValue *theValue) {

	const ArrayPtr &m = to_array(*theArray);
	auto p            = m->insert(std::make_pair(keyStr, *theValue));
	if (p.second) {
		return true;
	}

	p.first->second = *theValue;
	return true;
}

/*
** remove a node from an array whose key matches keyStr
*/
void ArrayDelete(DataValue *theArray, const std::string &keyStr) {

	const ArrayPtr &m = to_array(*theArray);

	auto it = m->find(keyStr);
	if (it != m->end()) {
		m->erase(it);
	}
}

/*
** remove all nodes from an array
*/
void ArrayDeleteAll(DataValue *theArray) {

	const ArrayPtr &m = to_array(*theArray);
	m->clear();
}

/*
** returns the number of elements (nodes containing values) of an array
*/
int ArraySize(DataValue *theArray) {
	const ArrayPtr &m = to_array(*theArray);
	return gsl::narrow<int>(m->size());
}

/*
** retrieves an array node whose key matches
** returns 1 for success 0 for not found
*/
bool ArrayGet(DataValue *theArray, const std::string &keyStr, DataValue *theValue) {

	const ArrayPtr &m = to_array(*theArray);
	auto it           = m->find(keyStr);
	if (it != m->end()) {
		*theValue = it->second;
		return true;
	}

	return false;
}

/*
** get pointer to start iterating an array
*/
ArrayIterator arrayIterateFirst(DataValue *theArray) {

	const ArrayPtr &m = to_array(*theArray);
	ArrayIterator it{m, m->begin()};

	return it;
}

/*
** move iterator to next entry in array
*/
ArrayIterator arrayIterateNext(ArrayIterator iterator) {

	Q_ASSERT(iterator.it != iterator.m->end());
	++(iterator.it);
	return iterator;
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
	std::string keyString;

	const int64_t nDim = Context.PC++->value;

	DISASM_RT(PC - 2, 2);
	STACKDUMP(nDim, 3);

	if (nDim > 0) {
		const int errNum = makeArrayKeyFromArgs(nDim, &keyString, false);
		if (errNum != STAT_OK) {
			return errNum;
		}

		POP(srcArray);
		if (is_array(srcArray)) {
			if (!ArrayGet(&srcArray, keyString, &valueItem)) {
				return execError("referenced array value not in array: %s", keyString.c_str());
			}
			PUSH(valueItem);
			return STAT_OK;
		}

		return execError("operator [] on non-array");
	}

	POP(srcArray);
	if (is_array(srcArray)) {
		PUSH_INT(ArraySize(&srcArray));
		return STAT_OK;
	}

	return execError("operator [] on non-array");
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
	std::string keyString;
	DataValue srcValue;
	DataValue dstArray;

	const int64_t nDim = Context.PC++->value;

	DISASM_RT(PC - 2, 1);
	STACKDUMP(nDim, 3);

	if (nDim > 0) {
		POP(srcValue);

		if (const int errNum = makeArrayKeyFromArgs(nDim, &keyString, false); errNum != STAT_OK) {
			return errNum;
		}

		POP(dstArray);

		if (!is_array(dstArray) && is_unset(dstArray)) {
			return execError("cannot assign array element of non-array");
		}

		if (is_array(srcValue)) {
			DataValue arrayCopyValue;

			const int errNum = ArrayCopy(&arrayCopyValue, &srcValue);
			srcValue         = arrayCopyValue;

			// TODO(eteran): should we return before assigning to srcValue?
			if (errNum != STAT_OK) {
				return errNum;
			}
		}

		if (ArrayInsert(&dstArray, keyString, &srcValue)) {
			return STAT_OK;
		}

		return execError("array member allocation failure");
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
	std::string keyString;

	const int64_t binaryOp = Context.PC++->value;
	const int64_t nDim     = Context.PC++->value;

	DISASM_RT(PC - 3, 3);
	STACKDUMP(nDim + 1, 3);

	if (binaryOp) {
		POP(moveExpr);
	}

	if (nDim > 0) {
		if (const int errNum = makeArrayKeyFromArgs(nDim, &keyString, true); errNum != STAT_OK) {
			return errNum;
		}

		PEEK(srcArray, nDim);
		if (is_array(srcArray)) {
			if (!ArrayGet(&srcArray, keyString, &valueItem)) {
				return execError("referenced array value not in array: %s", keyString.c_str());
			}
			PUSH(valueItem);
			if (binaryOp) {
				PUSH(moveExpr);
			}
			return STAT_OK;
		}

		return execError("operator [] on non-array");
	}

	return execError("array[] not an lvalue");
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

	DataValue arrayVal;

	DISASM_RT(PC - 1, 2);
	STACKDUMP(1, 3);

	Symbol *iterator = Context.PC++->sym;

	POP(arrayVal);

	if (iterator->type != LOCAL_SYM) {
		return execError("bad temporary iterator: %s", iterator->name.c_str());
	}

	DataValue *iteratorValPtr = &FP_GET_SYM_VAL(Context.FrameP, iterator);

	if (!is_array(arrayVal)) {
		return execError("can't iterate non-array");
	}

	*iteratorValPtr = make_value(arrayIterateFirst(&arrayVal));
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

	DataValue *itemValPtr;

	DISASM_RT(PC - 1, 4);
	STACKDUMP(0, 3);

	Symbol *const item     = Context.PC++->sym;
	Symbol *const iterator = Context.PC++->sym;
	Inst *const branchAddr = Context.PC + Context.PC->value;
	++Context.PC;

	switch (item->type) {
	case LOCAL_SYM:
		itemValPtr = &FP_GET_SYM_VAL(Context.FrameP, item);
		break;
	case GLOBAL_SYM:
		itemValPtr = &(item->value);
		break;
	default:
		return execError("can't assign to: %s", item->name.c_str());
	}

	*itemValPtr = make_value();

	if (iterator->type != LOCAL_SYM) {
		return execError("bad temporary iterator: %s", iterator->name.c_str());
	}

	DataValue *iteratorValPtr = &FP_GET_SYM_VAL(Context.FrameP, iterator);

	ArrayIterator thisEntry = to_iterator(*iteratorValPtr);

	if (thisEntry.it != thisEntry.m->end()) {
		*itemValPtr     = make_value(thisEntry.it->first);
		*iteratorValPtr = make_value(arrayIterateNext(thisEntry));
	} else {
		Context.PC = branchAddr;
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

	POP(theArray);
	if (!is_array(theArray)) {
		return execError("operator in on non-array");
	}

	PEEK(leftArray, 0);
	if (is_array(leftArray)) {

		POP(leftArray);

		const ArrayPtr &m = to_array(leftArray);

		inResult  = 1;
		auto iter = m->begin();
		while (inResult && iter != m->end()) {
			inResult = inResult && ArrayGet(&theArray, iter->first, &theValue);
			++iter;
		}
	} else {
		std::string keyStr;
		POP_STRING(keyStr);

		if (ArrayGet(&theArray, keyStr, &theValue)) {
			inResult = 1;
		}
	}
	PUSH_INT(inResult);
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
	std::string keyString;

	const int64_t nDim = Context.PC++->value;

	DISASM_RT(PC - 2, 2);
	STACKDUMP(nDim + 1, 3);

	if (nDim > 0) {
		const int errNum = makeArrayKeyFromArgs(nDim, &keyString, false);
		if (errNum != STAT_OK) {
			return errNum;
		}
	}

	POP(theArray);
	if (is_array(theArray)) {
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

bool StringToNum(const std::string &string, int *number) {
	auto it = string.begin();

	while (it != string.end() && (*it == ' ' || *it == '\t')) {
		++it;
	}

	if (it != string.end() && (*it == '+' || *it == '-')) {
		++it;
	}

	while (it != string.end() && safe_isdigit(*it)) {
		++it;
	}

	while (it != string.end() && (*it == ' ' || *it == '\t')) {
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

#if defined(DEBUG_DISASSEMBLER) // dumping values in disassembly or stack dump
static void dumpVal(DataValue dv) {

	auto escape_string = [](const std::string &s) -> std::string {
		std::string r;

		for (char ch : s) {
			switch (ch) {
			case '\n':
				r.append("\\n");
				break;
			case '\t':
				r.append("\\t");
				break;
			case '\"':
				r.append("\\\"");
				break;
			default:
				r.push_back(ch);
				break;
			}
		}

		return r;
	};

	if (is_integer(dv)) {
		printf("i=%d", to_integer(dv));
	} else if (is_string(dv)) {
		auto str = to_string(dv);
		if (str.size() > 20) {
			printf("<%lu> \"%s\"...", str.size(), escape_string(str.substr(0, 20)).c_str());
		} else {
			printf("<%lu> \"%s\"", str.size(), escape_string(str.substr(0, 20)).c_str());
		}
	} else if (is_array(dv)) {
		printf("<array>");
	} else if (is_unset(dv)) {
		printf("<no value>");
	} else {
		printf("<value>");
	}
}
#endif // #ifdef DEBUG_DISASSEMBLER

#ifdef DEBUG_DISASSEMBLER // For debugging code generation
static void disasm(Inst *inst, size_t nInstr) {
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
	int j;

	printf("\n");
	for (size_t i = 0; i < nInstr; ++i) {
		printf("Prog %8p ", static_cast<void *>(&inst[i]));
		for (j = 0; j < N_OPS; ++j) {
			if (inst[i].func == OpFns[j]) {
				printf("%22s ", opNames[j]);
				if (j == OP_PUSH_SYM || j == OP_ASSIGN) {
					Symbol *sym = inst[i + 1].sym;
					printf("%s", sym->name.c_str());
					if (is_string(sym->value) && sym->name.compare(0, 8, "string #") == 0) {
						dumpVal(sym->value);
					}
					++i;
				} else if (j == OP_BRANCH || j == OP_BRANCH_FALSE || j == OP_BRANCH_NEVER || j == OP_BRANCH_TRUE) {
					printf("to=(%+ld) %p", inst[i + 1].value, static_cast<void *>(&inst[i + 1] + inst[i + 1].value));
					++i;
				} else if (j == OP_SUBR_CALL) {
					printf("%s (%ld arg)", inst[i + 1].sym->name.c_str(), inst[i + 2].value);
					i += 2;
				} else if (j == OP_BEGIN_ARRAY_ITER) {
					printf("%s in", inst[i + 1].sym->name.c_str());
					++i;
				} else if (j == OP_ARRAY_ITER) {
					printf("%s = %s++ end-loop=(%ld) %p", inst[i + 1].sym->name.c_str(), inst[i + 2].sym->name.c_str(), inst[i + 3].value, static_cast<void *>(&inst[i + 3] + inst[i + 3].value));
					i += 3;
				} else if (j == OP_ARRAY_REF || j == OP_ARRAY_DELETE || j == OP_ARRAY_ASSIGN) {
					printf("nDim=%ld", inst[i + 1].value);
					++i;
				} else if (j == OP_ARRAY_REF_ASSIGN_SETUP) {
					printf("binOp=%s ", inst[i + 1].value ? "true" : "false");
					printf("nDim=%ld", inst[i + 2].value);
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
			printf("%lx\n", inst[i].value);
		}
	}
}
#endif // #ifdef DEBUG_DISASSEMBLER

#ifdef DEBUG_STACK // for run-time stack dumping
#define STACK_DUMP_ARG_PREFIX "Arg"
static void stackdump(int n, int extra) {
	// TheStack-> symN-sym1(FP), argArray, nArgs, oldFP, retPC, argN-arg1, next, ...
	int nArgs = FP_GET_ARG_COUNT(FrameP);
	int i;
	char buffer[256];
	printf("Stack ----->\n");
	for (i = 0; i < n + extra; i++) {
		const char *pos = "";
		DataValue *dv   = &StackP[-i - 1];
		if (dv < TheStack) {
			printf("--------------Stack base--------------\n");
			break;
		}

		long offset = dv - FrameP;

		printf("%4.4s", i < n ? ">>>>" : "");
		printf("%8p ", static_cast<void *>(dv));

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
				pos = buffer;
				snprintf(buffer, sizeof(buffer), STACK_DUMP_ARG_PREFIX "%ld", offset + FP_TO_ARGS_DIST + nArgs + 1);
			}
			break;
		}

		printf("%-6s ", pos);
		dumpVal(*dv);
		printf("\n");
	}
}
#endif // ifdef DEBUG_STACK
