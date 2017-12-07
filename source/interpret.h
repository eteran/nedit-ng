
#ifndef INTERPRET_H_
#define INTERPRET_H_

#include "rbTree/rbTree.h"
#include "util/string_view.h"

#include <gsl/span>

#include <deque>
#include <map>
#include <memory>

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

#define INIT_DATA_VALUE {NO_TAG, {0}}

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


// NOTE(eteran): we can eventually replace this with boost::variant,

enum TypeTags {
    NO_TAG,
    INT_TAG,
    STRING_TAG,
    ARRAY_TAG,
};

struct DataValue {
	TypeTags tag;
	
	union {
        int            n;
        NString        str;
        ArrayEntry*    arrayPtr;

        BuiltInSubrEx  subr;
        Program*       prog;
        Inst*          inst;
        DataValue*     dataval;
	} val;
};

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
bool ArrayInsert(DataValue *theArray, char *keyStr, DataValue *theValue);
void ArrayDelete(DataValue *theArray, char *keyStr);
void ArrayDeleteAll(DataValue *theArray);
int ArraySize(DataValue *theArray);
bool ArrayGet(DataValue *theArray, char *keyStr, DataValue *theValue);
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

char *AllocStringCpyEx(const std::string &s);
NString AllocNStringCpyEx(const QString &s);
NString AllocNStringCpyEx(view::string_view s);
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

inline DataValue to_value(view::string_view str) {
    DataValue DV;
    DV.tag     = STRING_TAG;
    DV.val.str = AllocNStringCpyEx(str);
    return DV;
}

// NOTE(eteran): this is a non-owning API
inline DataValue to_value(char *str, int size) {
    DataValue DV;
    DV.tag     = STRING_TAG;
    DV.val.str.rep = str;
    DV.val.str.len = static_cast<size_t>(size);
    return DV;
}

inline DataValue to_value(const QString &str) {
    DataValue DV;
    DV.tag     = STRING_TAG;
    DV.val.str = AllocNStringCpyEx(str);
    return DV;
}

inline DataValue to_value(Program *prog) {
    DataValue DV;
    DV.tag      = NO_TAG;
    DV.val.prog = prog;
    return DV;
}

inline DataValue to_value(Inst *inst) {
    DataValue DV;
    DV.tag      = NO_TAG;
    DV.val.inst = inst;
    return DV;
}

inline DataValue to_value(DataValue *v) {
    DataValue DV;
    DV.tag         = NO_TAG;
    DV.val.dataval = v;
    return DV;
}

inline DataValue to_value(BuiltInSubrEx routine) {
    DataValue DV;
    DV.tag      = NO_TAG;
    DV.val.subr = routine;
    return DV;
}

inline bool is_integer(const DataValue &dv) {
    return dv.tag == INT_TAG;
}

inline bool is_string(const DataValue &dv) {
    return dv.tag == STRING_TAG;
}

inline bool is_array(const DataValue &dv) {
    return dv.tag == ARRAY_TAG;
}

inline view::string_view to_string(const DataValue &dv) {
    Q_ASSERT(is_string(dv));
    return view::string_view(dv.val.str.rep, dv.val.str.len);
}

inline QString to_qstring(const DataValue &dv) {
    Q_ASSERT(is_string(dv));
    return QString::fromLatin1(dv.val.str.rep, static_cast<int>(dv.val.str.len));
}

inline int to_integer(const DataValue &dv) {
    Q_ASSERT(is_integer(dv));
    return dv.val.n;
}

inline Program *to_program(const DataValue &dv) {
    //Q_ASSERT(is_code(dv));
    return dv.val.prog;
}

inline BuiltInSubrEx to_subroutine(const DataValue &dv) {
    //Q_ASSERT(is_subroutine(dv));
    return dv.val.subr;
}

inline DataValue *to_data_value(const DataValue &dv) {
    //Q_ASSERT(is_data_value(dv));
    return dv.val.dataval;
}

inline Inst *to_instruction(const DataValue &dv) {
    //Q_ASSERT(is_instruction(dv));
    return dv.val.inst;
}

#endif
