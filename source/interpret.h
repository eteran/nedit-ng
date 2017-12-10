
#ifndef INTERPRET_H_
#define INTERPRET_H_

#include "rbTree/rbTree.h"
#include "util/string_view.h"

#include <gsl/span>

#include <deque>
#include <map>
#include <memory>
#include <boost/variant.hpp>

#include <QtGlobal>
#include <QString>

class DocumentWidget;
class Program;
struct ArrayEntry;
struct DataValue;
struct Symbol;

// Maximum stack size
constexpr int STACK_SIZE = 1024;

// Max. symbol name length
constexpr int MAX_SYM_LEN = 100;

// Special value for the send_event field of events passed to action routines.  Tells them that they were called from a macro
constexpr int MACRO_EVENT_MARKER = 2;

// determine a safe size for a string to hold an integer-like number contained in T
template <class T>
constexpr int TYPE_INT_STR_SIZE = ((sizeof(T) * 3) + 2);

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

struct NString {
	char *rep;
	size_t len;
};


struct ArrayIterator {
    ArrayEntry* ptr;
};

struct DataValue {
    boost::variant<
        boost::blank,
        int,
        std::string,
        ArrayEntry*,
        ArrayIterator,
        BuiltInSubrEx,
        Program*,
        Inst*,
        DataValue*
    > value;
};


//------------------------------------------------------------------------------
struct ArrayEntry : public rbTreeNode {
    std::string key;
    DataValue   value;
};

/* symbol table entry */
struct Symbol {
	std::string name;
	SymTypes    type;
	DataValue   value;
};

class Program {
public:
    std::deque<Symbol *> localSymList;
	Inst *code;
};

/* Information needed to re-start a preempted macro */
struct RestartData {
public:
    ~RestartData() {
        delete [] stack;
    }
public:
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
int ExecuteMacroEx(DocumentWidget *document, Program *prog, gsl::span<DataValue> arguments, DataValue *result, std::shared_ptr<RestartData> &continuation, QString *msg);
int ContinueMacroEx(const std::shared_ptr<RestartData> &continuation, DataValue *result, QString *msg);
void RunMacroAsSubrCall(Program *prog);
void PreemptMacro();

void GarbageCollectStrings();
Symbol *PromoteToGlobal(Symbol *sym);
void FreeProgram(Program *prog);
void ModifyReturnedValueEx(const std::shared_ptr<RestartData> &context, const DataValue &dv);
DocumentWidget *MacroRunDocumentEx();
DocumentWidget *MacroFocusDocument();
void SetMacroFocusDocument(DocumentWidget *document);

/* function used for implicit conversion from string to number */
bool StringToNum(const std::string &string, int *number);
bool StringToNum(view::string_view string, int *number);
bool StringToNum(const QString &string, int *number);


struct array_empty {};
struct array_new   {};
struct array_iter   {};

inline DataValue to_value(const array_empty &) {
    DataValue DV;
    DV.value = static_cast<ArrayEntry*>(nullptr);
    return DV;
}

inline DataValue to_value(const array_new &) {
    DataValue DV;
    DV.value = ArrayNew();
    return DV;
}

inline DataValue to_value(ArrayEntry *iter, const array_iter &) {
    DataValue DV;
    DV.value = ArrayIterator{iter};
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
    Q_ASSERT(is_string(dv));
    return boost::get<std::string>(dv.value);
}

inline QString to_qstring(const DataValue &dv) {
    Q_ASSERT(is_string(dv));
    auto str = boost::get<std::string>(dv.value);
    return QString::fromStdString(str);
}

inline int to_integer(const DataValue &dv) {
    Q_ASSERT(is_integer(dv));
    return boost::get<int>(dv.value);
}

inline Program *to_program(const DataValue &dv) {
    //Q_ASSERT(is_code(dv));
    return boost::get<Program*>(dv.value);
}

inline BuiltInSubrEx to_subroutine(const DataValue &dv) {
    //Q_ASSERT(is_subroutine(dv));
    return boost::get<BuiltInSubrEx>(dv.value);
}

inline DataValue *to_data_value(const DataValue &dv) {
    //Q_ASSERT(is_data_value(dv));
    return boost::get<DataValue*>(dv.value);
}

inline Inst *to_instruction(const DataValue &dv) {
    //Q_ASSERT(is_instruction(dv));
    return boost::get<Inst*>(dv.value);
}

inline ArrayEntry *to_array(const DataValue &dv) {
    //Q_ASSERT(is_array(dv));
    return boost::get<ArrayEntry*>(dv.value);
}

inline ArrayIterator to_iterator(const DataValue &dv) {
    //Q_ASSERT(is_iterator(dv));
    return boost::get<ArrayIterator>(dv.value);
}

#endif
