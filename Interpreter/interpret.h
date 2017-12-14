
#ifndef INTERPRET_H_
#define INTERPRET_H_

#include "string_view.h"

#include <gsl/span>

#include <deque>
#include <map>
#include <memory>
#include <vector>
#include <boost/variant.hpp>

#include <QString>

class DocumentWidget;
struct DataValue;
struct Program;
struct Symbol;

// Maximum stack size
constexpr int STACK_SIZE = 1024;

// Special value for the send_event field of events passed to action routines.  Tells them that they were called from a macro
constexpr int MACRO_EVENT_MARKER = 2;

#define ARRAY_DIM_SEP "\034"

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

union Inst {
	int (*func)();
	int value;
	Symbol *sym;
};

using Arguments     = gsl::span<DataValue>;
using BuiltInSubrEx = bool (*)(DocumentWidget *document, Arguments arguments, struct DataValue *result, const char **errMsg);

using Array    = std::map<std::string, DataValue>;
using ArrayPtr = std::shared_ptr<Array>;

// NOTE(eteran): we use a kind of "fat iterator", because the arrayIter function
// needs to know if the iterator is at the end of the map. This requirement
// means that we need a reference to the map to compare against
struct ArrayIterator {
    ArrayPtr        m;
    Array::iterator it;
};

struct DataValue {
    boost::variant<
        boost::blank,
        int,
        std::string,
        ArrayPtr,
        ArrayIterator,
        BuiltInSubrEx,
        Program*,
        Inst*,
        DataValue*
    > value;
};


//------------------------------------------------------------------------------

/* symbol table entry */
struct Symbol {
	std::string name;
	SymTypes    type;
	DataValue   value;
};

struct Program {
    std::deque<Symbol *> localSymList;
    std::vector<Inst>    code;
};

/* Information needed to re-start a preempted macro */
struct MacroContext {
    DataValue Stack[STACK_SIZE];             // the stack
    DataValue *StackP             = nullptr; // next free spot on stack
    DataValue *FrameP             = nullptr; // frame pointer (start of local variables for the current subroutine invocation)
    Inst *PC                      = nullptr; // program counter during execution
    DocumentWidget *RunDocument   = nullptr; // document from which macro was run
    DocumentWidget *FocusDocument = nullptr; // document on which macro commands operate
};

void InitMacroGlobals();
void CleanupMacroGlobals();

ArrayIterator arrayIterateFirst(DataValue *theArray);
ArrayIterator arrayIterateNext(ArrayIterator iterator);
bool ArrayInsert(DataValue *theArray, const std::string &keyStr, DataValue *theValue);
void ArrayDelete(DataValue *theArray, const std::string &keyStr);
void ArrayDeleteAll(DataValue *theArray);
int ArraySize(DataValue *theArray);
bool ArrayGet(DataValue *theArray, const std::string &keyStr, DataValue *theValue);
int ArrayCopy(DataValue *dstArray, DataValue *srcArray);

/* Routines for creating a program, (accumulated beginning with
   BeginCreatingProgram and returned via FinishCreatingProgram) */
bool AddBranchOffset(Inst *to, const char **msg);
bool AddBreakAddr(Inst *addr);
bool AddContinueAddr(Inst *addr);
bool AddImmediate(int value, const char **msg);
bool AddOp(int op, const char **msg);
bool AddSym(Symbol *sym, const char **msg);
Inst *GetPC();
Program *FinishCreatingProgram();
Symbol *InstallIteratorSymbol();
Symbol *InstallStringConstSymbol(view::string_view str);
Symbol *InstallSymbol(const std::string &name, SymTypes type, const DataValue &value);
Symbol *InstallSymbolEx(const QString &name, enum SymTypes type, const DataValue &value);
Symbol *LookupStringConstSymbol(view::string_view value);
Symbol *LookupSymbolEx(const QString &name);
Symbol *LookupSymbol(view::string_view name);
void BeginCreatingProgram();
void FillLoopAddrs(Inst *breakAddr, Inst *continueAddr);
void StartLoopAddrList();
void SwapCode(Inst *start, Inst *boundary, Inst *end);

/* Routines for executing programs */
int ExecuteMacroEx(DocumentWidget *document, Program *prog, gsl::span<DataValue> arguments, DataValue *result, std::shared_ptr<MacroContext> &continuation, QString *msg);
int ContinueMacroEx(std::shared_ptr<MacroContext> &continuation, DataValue *result, QString *msg);
void RunMacroAsSubrCall(Program *prog);
void PreemptMacro();

Symbol *PromoteToGlobal(Symbol *sym);
void FreeProgram(Program *prog);
void ModifyReturnedValueEx(const std::shared_ptr<MacroContext> &context, const DataValue &dv);
DocumentWidget *MacroRunDocumentEx();
DocumentWidget *MacroFocusDocument();
void SetMacroFocusDocument(DocumentWidget *document);

/* function used for implicit conversion from string to number */
bool StringToNum(const std::string &string, int *number);
bool StringToNum(view::string_view string, int *number);
bool StringToNum(const QString &string, int *number);

inline DataValue to_value(const ArrayPtr &map) {
    DataValue DV;
    DV.value = map;
    return DV;
}

inline DataValue to_value(const ArrayIterator &iter) {
    DataValue DV;
    DV.value = iter;
    return DV;
}

inline DataValue to_value() {
    DataValue DV;
    return DV;
}

inline DataValue to_value(int n) {
    DataValue DV;
    DV.value = n;
    return DV;
}

inline DataValue to_value(bool n) {
    DataValue DV;
    DV.value = n ? 1 : 0;
    return DV;
}

inline DataValue to_value(view::string_view str) {
    DataValue DV;
    DV.value = str.to_string();
    return DV;
}

inline DataValue to_value(const QString &str) {
    DataValue DV;
    DV.value = str.toStdString();
    return DV;
}

inline DataValue to_value(Program *prog) {
    DataValue DV;
    DV.value = prog;
    return DV;
}

inline DataValue to_value(Inst *inst) {
    DataValue DV;
    DV.value = inst;
    return DV;
}

inline DataValue to_value(DataValue *v) {
    DataValue DV;
    DV.value = v;
    return DV;
}

inline DataValue to_value(BuiltInSubrEx routine) {
    DataValue DV;
    DV.value = routine;
    return DV;
}

inline bool is_unset(const DataValue &dv) {
    return dv.value.which() == 0;
}

inline bool is_integer(const DataValue &dv) {
    return dv.value.which() == 1;
}

inline bool is_string(const DataValue &dv) {
    return dv.value.which() == 2;
}

inline bool is_array(const DataValue &dv) {
    return dv.value.which() == 3;
}

inline std::string to_string(const DataValue &dv) {
    return boost::get<std::string>(dv.value);
}

inline int to_integer(const DataValue &dv) {
    return boost::get<int>(dv.value);
}

inline Program *to_program(const DataValue &dv) {
    return boost::get<Program*>(dv.value);
}

inline BuiltInSubrEx to_subroutine(const DataValue &dv) {
    return boost::get<BuiltInSubrEx>(dv.value);
}

inline DataValue *to_data_value(const DataValue &dv) {
    return boost::get<DataValue*>(dv.value);
}

inline Inst *to_instruction(const DataValue &dv) {
    return boost::get<Inst*>(dv.value);
}

inline ArrayPtr to_array(const DataValue &dv) {
    return boost::get<ArrayPtr>(dv.value);
}

inline ArrayIterator to_iterator(const DataValue &dv) {
    //Q_ASSERT(is_iterator(dv));
    return boost::get<ArrayIterator>(dv.value);
}

#endif
