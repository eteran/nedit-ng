%{
#include "parse.h"
#include "interpret.h"
#include "Util/utils.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cctype>
#include <string>

#include <gsl/gsl_util>

/* Macros to add error processing to AddOp and AddSym calls */
#define ADD_OP(op) if (!AddOp(op, &ErrMsg)) return 1
#define ADD_SYM(sym) if (!AddSym(sym, &ErrMsg)) return 1
#define ADD_IMMED(val) if (!AddImmediate(val, &ErrMsg)) return 1
#define ADD_BR_OFF(to) if (!AddBranchOffset(to, &ErrMsg)) return 1

static void SET_BR_OFF(Inst *from, Inst *to) {
    from->value = to - from;
}

static int yyerror(const char *s);
static int yylex();
int yyparse();
static int follow(char expect, int yes, int no);
static int follow2(char expect1, int yes1, char expect2, int yes2, int no);
static int follow_non_whitespace(char expect, int yes, int no);

static QString ErrMsg;
static QString::const_iterator InPtr;
static QString::const_iterator EndPtr;
extern Inst *LoopStack[]; /* addresses of break, cont stmts */
extern Inst **LoopStackPtr;  /*  to fill at the end of a loop */

%}

%union {
    Symbol *sym;
    Inst *inst;
    int nArgs;
}

%token <sym> NUMBER STRING SYMBOL
%token DELETE ARG_LOOKUP
%token IF WHILE ELSE FOR BREAK CONTINUE RETURN
%type <nArgs> arglist
%type <inst> cond comastmts for while else and or arrayexpr
%type <sym> evalsym

%nonassoc IF_NO_ELSE
%nonassoc ELSE

%nonassoc SYMBOL ARG_LOOKUP
%right    '=' ADDEQ SUBEQ MULEQ DIVEQ MODEQ ANDEQ OREQ
%left     CONCAT
%left     OR
%left     AND
%left     '|'
%left     '&'
%left     GT GE LT LE EQ NE IN
%left     '+' '-'
%left     '*' '/' '%'
%nonassoc UNARY_MINUS NOT
%nonassoc DELETE
%nonassoc INCR DECR
%right    POW
%nonassoc '['
%nonassoc '('

%%      /* Rules */

program:    blank stmts {
                ADD_OP(OP_RETURN_NO_VAL); return 0;
            }
            | blank '{' blank stmts '}' {
                ADD_OP(OP_RETURN_NO_VAL); return 0;
            }
            | blank '{' blank '}' {
                ADD_OP(OP_RETURN_NO_VAL); return 0;
            }
            | error {
                return 1;
            }
            ;
block:      '{' blank stmts '}' blank
            | '{' blank '}' blank
            | stmt
            ;
stmts:      stmt
            | stmts stmt
            ;
stmt:       simpstmt '\n' blank
            | IF '(' cond ')' blank block %prec IF_NO_ELSE {
                SET_BR_OFF($3, GetPC());
            }
            | IF '(' cond ')' blank block else blank block %prec ELSE {
                SET_BR_OFF($3, ($7+1)); SET_BR_OFF($7, GetPC());
            }
            | while '(' cond ')' blank block {
                ADD_OP(OP_BRANCH); ADD_BR_OFF($1);
                SET_BR_OFF($3, GetPC()); FillLoopAddrs(GetPC(), $1);
            }
            | for '(' comastmts ';' cond ';' comastmts ')' blank block {
                FillLoopAddrs(GetPC()+2+($7-($5+1)), GetPC());
                SwapCode($5+1, $7, GetPC());
                ADD_OP(OP_BRANCH); ADD_BR_OFF($3); SET_BR_OFF($5, GetPC());
            }
            | for '(' SYMBOL IN arrayexpr ')' {
                Symbol *iterSym = InstallIteratorSymbol();
                ADD_OP(OP_BEGIN_ARRAY_ITER); ADD_SYM(iterSym);
                ADD_OP(OP_ARRAY_ITER); ADD_SYM($3); ADD_SYM(iterSym); ADD_BR_OFF(nullptr);
            }
                blank block {
                    ADD_OP(OP_BRANCH); ADD_BR_OFF($5+2);
                    SET_BR_OFF($5+5, GetPC());
                    FillLoopAddrs(GetPC(), $5+2);
            }
            | BREAK '\n' blank {
                ADD_OP(OP_BRANCH); ADD_BR_OFF(nullptr);
                if (AddBreakAddr(GetPC()-1)) {
                    yyerror("break outside loop"); YYERROR;
                }
            }
            | CONTINUE '\n' blank {
                ADD_OP(OP_BRANCH); ADD_BR_OFF(nullptr);
                if (AddContinueAddr(GetPC()-1)) {
                    yyerror("continue outside loop"); YYERROR;
                }
            }
            | RETURN expr '\n' blank {
                ADD_OP(OP_RETURN);
            }
            | RETURN '\n' blank {
                ADD_OP(OP_RETURN_NO_VAL);
            }
            ;
simpstmt:   SYMBOL '=' expr {
                ADD_OP(OP_ASSIGN); ADD_SYM($1);
            }
            | evalsym ADDEQ expr {
                ADD_OP(OP_ADD); ADD_OP(OP_ASSIGN); ADD_SYM($1);
            }
            | evalsym SUBEQ expr {
                ADD_OP(OP_SUB); ADD_OP(OP_ASSIGN); ADD_SYM($1);
            }
            | evalsym MULEQ expr {
                ADD_OP(OP_MUL); ADD_OP(OP_ASSIGN); ADD_SYM($1);
            }
            | evalsym DIVEQ expr {
                ADD_OP(OP_DIV); ADD_OP(OP_ASSIGN); ADD_SYM($1);
            }
            | evalsym MODEQ expr {
                ADD_OP(OP_MOD); ADD_OP(OP_ASSIGN); ADD_SYM($1);
            }
            | evalsym ANDEQ expr {
                ADD_OP(OP_BIT_AND); ADD_OP(OP_ASSIGN); ADD_SYM($1);
            }
            | evalsym OREQ expr {
                ADD_OP(OP_BIT_OR); ADD_OP(OP_ASSIGN); ADD_SYM($1);
            }
            | DELETE arraylv '[' arglist ']' {
                ADD_OP(OP_ARRAY_DELETE); ADD_IMMED($4);
            }
            | initarraylv '[' arglist ']' '=' expr {
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED($3);
            }
            | initarraylv '[' arglist ']' ADDEQ expr {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(1); ADD_IMMED($3);
                ADD_OP(OP_ADD);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED($3);
            }
            | initarraylv '[' arglist ']' SUBEQ expr {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(1); ADD_IMMED($3);
                ADD_OP(OP_SUB);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED($3);
            }
            | initarraylv '[' arglist ']' MULEQ expr {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(1); ADD_IMMED($3);
                ADD_OP(OP_MUL);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED($3);
            }
            | initarraylv '[' arglist ']' DIVEQ expr {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(1); ADD_IMMED($3);
                ADD_OP(OP_DIV);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED($3);
            }
            | initarraylv '[' arglist ']' MODEQ expr {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(1); ADD_IMMED($3);
                ADD_OP(OP_MOD);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED($3);
            }
            | initarraylv '[' arglist ']' ANDEQ expr {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(1); ADD_IMMED($3);
                ADD_OP(OP_BIT_AND);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED($3);
            }
            | initarraylv '[' arglist ']' OREQ expr {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(1); ADD_IMMED($3);
                ADD_OP(OP_BIT_OR);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED($3);
            }
            | initarraylv '[' arglist ']' INCR {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(0); ADD_IMMED($3);
                ADD_OP(OP_INCR);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED($3);
            }
            | initarraylv '[' arglist ']' DECR {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(0); ADD_IMMED($3);
                ADD_OP(OP_DECR);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED($3);
            }
            | INCR initarraylv '[' arglist ']' {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(0); ADD_IMMED($4);
                ADD_OP(OP_INCR);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED($4);
            }
            | DECR initarraylv '[' arglist ']' {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(0); ADD_IMMED($4);
                ADD_OP(OP_DECR);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED($4);
            }
            | SYMBOL '(' arglist ')' {
                ADD_OP(OP_SUBR_CALL);
                ADD_SYM(PromoteToGlobal($1)); ADD_IMMED($3);
            }
            | INCR SYMBOL {
                ADD_OP(OP_PUSH_SYM); ADD_SYM($2); ADD_OP(OP_INCR);
                ADD_OP(OP_ASSIGN); ADD_SYM($2);
            }
            | SYMBOL INCR {
                ADD_OP(OP_PUSH_SYM); ADD_SYM($1); ADD_OP(OP_INCR);
                ADD_OP(OP_ASSIGN); ADD_SYM($1);
            }
            | DECR SYMBOL {
                ADD_OP(OP_PUSH_SYM); ADD_SYM($2); ADD_OP(OP_DECR);
                ADD_OP(OP_ASSIGN); ADD_SYM($2);
            }
            | SYMBOL DECR {
                ADD_OP(OP_PUSH_SYM); ADD_SYM($1); ADD_OP(OP_DECR);
                ADD_OP(OP_ASSIGN); ADD_SYM($1);
            }
            ;
evalsym:    SYMBOL {
                $$ = $1; ADD_OP(OP_PUSH_SYM); ADD_SYM($1);
            }
            ;
comastmts:  /* nothing */ {
                $$ = GetPC();
            }
            | simpstmt {
                $$ = GetPC();
            }
            | comastmts ',' simpstmt {
                $$ = GetPC();
            }
            ;
arglist:    /* nothing */ {
                $$ = 0;
            }
            | expr {
                $$ = 1;
            }
            | arglist ',' expr {
                $$ = $1 + 1;
            }
            ;
expr:       numexpr %prec CONCAT
            | expr numexpr %prec CONCAT {
                ADD_OP(OP_CONCAT);
            }
            ;
initarraylv:    SYMBOL {
                    ADD_OP(OP_PUSH_ARRAY_SYM); ADD_SYM($1); ADD_IMMED(1);
                }
                | initarraylv '[' arglist ']' {
                    ADD_OP(OP_ARRAY_REF); ADD_IMMED($3);
                }
                ;
arraylv:    SYMBOL {
                ADD_OP(OP_PUSH_ARRAY_SYM); ADD_SYM($1); ADD_IMMED(0);
            }
            | arraylv '[' arglist ']' {
                ADD_OP(OP_ARRAY_REF); ADD_IMMED($3);
            }
            ;
arrayexpr:  numexpr {
                $$ = GetPC();
            }
            ;
numexpr:    NUMBER {
                ADD_OP(OP_PUSH_SYM); ADD_SYM($1);
            }
            | STRING {
                ADD_OP(OP_PUSH_SYM); ADD_SYM($1);
            }
            | SYMBOL {
                ADD_OP(OP_PUSH_SYM); ADD_SYM($1);
            }
            | SYMBOL '(' arglist ')' {
                ADD_OP(OP_SUBR_CALL);
                ADD_SYM(PromoteToGlobal($1)); ADD_IMMED($3);
                ADD_OP(OP_FETCH_RET_VAL);
            }
            | '(' expr ')'
            | ARG_LOOKUP '[' numexpr ']' {
               ADD_OP(OP_PUSH_ARG);
            }
            | ARG_LOOKUP '[' ']' {
               ADD_OP(OP_PUSH_ARG_COUNT);
            }
            | ARG_LOOKUP {
               ADD_OP(OP_PUSH_ARG_ARRAY);
            }
            | numexpr '[' arglist ']' {
                ADD_OP(OP_ARRAY_REF); ADD_IMMED($3);
            }
            | numexpr '+' numexpr {
                ADD_OP(OP_ADD);
            }
            | numexpr '-' numexpr {
                ADD_OP(OP_SUB);
            }
            | numexpr '*' numexpr {
                ADD_OP(OP_MUL);
            }
            | numexpr '/' numexpr {
                ADD_OP(OP_DIV);
            }
            | numexpr '%' numexpr {
                ADD_OP(OP_MOD);
            }
            | numexpr POW numexpr {
                ADD_OP(OP_POWER);
            }
            | '-' numexpr %prec UNARY_MINUS  {
                ADD_OP(OP_NEGATE);
            }
            | numexpr GT numexpr  {
                ADD_OP(OP_GT);
            }
            | numexpr GE numexpr  {
                ADD_OP(OP_GE);
            }
            | numexpr LT numexpr  {
                ADD_OP(OP_LT);
            }
            | numexpr LE numexpr  {
                ADD_OP(OP_LE);
            }
            | numexpr EQ numexpr  {
                ADD_OP(OP_EQ);
            }
            | numexpr NE numexpr  {
                ADD_OP(OP_NE);
            }
            | numexpr '&' numexpr {
                ADD_OP(OP_BIT_AND);
            }
            | numexpr '|' numexpr  {
                ADD_OP(OP_BIT_OR);
            }
            | numexpr and numexpr %prec AND {
                ADD_OP(OP_AND); SET_BR_OFF($2, GetPC());
            }
            | numexpr or numexpr %prec OR {
                ADD_OP(OP_OR); SET_BR_OFF($2, GetPC());
            }
            | NOT numexpr {
                ADD_OP(OP_NOT);
            }
            | INCR SYMBOL {
                ADD_OP(OP_PUSH_SYM); ADD_SYM($2); ADD_OP(OP_INCR);
                ADD_OP(OP_DUP); ADD_OP(OP_ASSIGN); ADD_SYM($2);
            }
            | SYMBOL INCR {
                ADD_OP(OP_PUSH_SYM); ADD_SYM($1); ADD_OP(OP_DUP);
                ADD_OP(OP_INCR); ADD_OP(OP_ASSIGN); ADD_SYM($1);
            }
            | DECR SYMBOL {
                ADD_OP(OP_PUSH_SYM); ADD_SYM($2); ADD_OP(OP_DECR);
                ADD_OP(OP_DUP); ADD_OP(OP_ASSIGN); ADD_SYM($2);
            }
            | SYMBOL DECR {
                ADD_OP(OP_PUSH_SYM); ADD_SYM($1); ADD_OP(OP_DUP);
                ADD_OP(OP_DECR); ADD_OP(OP_ASSIGN); ADD_SYM($1);
            }
            | numexpr IN numexpr {
                ADD_OP(OP_IN_ARRAY);
            }
            ;
while:  WHILE {
            $$ = GetPC(); StartLoopAddrList();
        }
        ;
for:    FOR {
            StartLoopAddrList(); $$ = GetPC();
        }
        ;
else:   ELSE {
            ADD_OP(OP_BRANCH); $$ = GetPC(); ADD_BR_OFF(nullptr);
        }
        ;
cond:   /* nothing */ {
            ADD_OP(OP_BRANCH_NEVER); $$ = GetPC(); ADD_BR_OFF(nullptr);
        }
        | numexpr {
            ADD_OP(OP_BRANCH_FALSE); $$ = GetPC(); ADD_BR_OFF(nullptr);
        }
        ;
and:    AND {
            ADD_OP(OP_DUP); ADD_OP(OP_BRANCH_FALSE); $$ = GetPC();
            ADD_BR_OFF(nullptr);
        }
        ;
or:     OR {
            ADD_OP(OP_DUP); ADD_OP(OP_BRANCH_TRUE); $$ = GetPC();
            ADD_BR_OFF(nullptr);
        }
        ;
blank:  /* nothing */
        | blank '\n'
        ;

%% /* User Subroutines Section */


/*
** Parse a string and create a program from it (this is the parser entry point).
** The program created by this routine can be executed using ExecuteProgram.
** Returns program on success, or nullptr on failure.  If the command failed,
** the error message is returned in msg, and the length of the string up
** to where parsing failed in stoppedAt.
*/
Program *compileMacro(const QString &expr, QString *msg, int *stoppedAt) {
    BeginCreatingProgram();

    /* call yyparse to parse the string and check for success.  If the parse
       failed, return the error message and string index (the grammar aborts
       parsing at the first error) */
    QString::const_iterator start = expr.begin();
    InPtr  = start;
    EndPtr = start + expr.size();

    if (yyparse()) {
        *msg       = ErrMsg;
        *stoppedAt = gsl::narrow<int>(InPtr - start);
        Program *prog = FinishCreatingProgram();
        delete prog;
        return nullptr;
    }

    /* get the newly created program */
    Program *prog = FinishCreatingProgram();

    /* parse succeeded */
    *msg       = QString();
    *stoppedAt = gsl::narrow<int>(InPtr - start);
    return prog;
}


static int yylex(void) {

    int i;
    Symbol *s;
    static const char escape[] = "\\\"ntbrfave";
    static const char replace[] = "\\\"\n\t\b\r\f\a\v\x1B"; /* ASCII escape */

    /* skip whitespace, backslash-newline combinations, and comments, which are
       all considered whitespace */
    for (;;) {
        if (*InPtr == QLatin1Char('\\') && *(InPtr + 1) == QLatin1Char('\n')) {
            InPtr += 2;
        } else if (*InPtr == QLatin1Char(' ') || *InPtr == QLatin1Char('\t')) {
            InPtr++;
        } else if (*InPtr == QLatin1Char('#')) {
            while (*InPtr != QLatin1Char('\n') && InPtr != EndPtr) {
                /* Comments stop at escaped newlines */
                if (*InPtr == QLatin1Char('\\') && *(InPtr + 1) == QLatin1Char('\n')) {
                    InPtr += 2;
                    break;
                }
                InPtr++;
            }
        } else {
            break;
        }
    }

    /* return end of input at the end of the string */
    if (InPtr == EndPtr) {
        return 0;
    }

    /* process number tokens */
    if (safe_isdigit(InPtr->toLatin1()))  { /* number */

        QString value;
        auto p = std::back_inserter(value);

        *p++ = *InPtr++;
        while (InPtr != EndPtr && safe_isdigit(InPtr->toLatin1())) {
            *p++ = *InPtr++;
        }

        int n = value.toInt();

        char name[28];
        snprintf(name, sizeof(name), "const %d", n);

        if ((yylval.sym = LookupSymbol(name)) == nullptr) {
            yylval.sym = InstallSymbol(name, CONST_SYM, make_value(n));
        }

        return NUMBER;
    }

    /* process symbol tokens.  "define" is a special case not handled
       by this parser, considered end of input. */
    if (safe_isalpha(InPtr->toLatin1()) || *InPtr == QLatin1Char('$')) {

        QString symName;
        auto p = std::back_inserter(symName);

        *p++ = *InPtr++;
        while ((InPtr != EndPtr) && (safe_isalnum(InPtr->toLatin1()) || *InPtr == QLatin1Char('_'))) {
            *p++ = *InPtr++;
        }

        if (symName == QLatin1String("while"))    return WHILE;
        if (symName == QLatin1String("if"))       return IF;
        if (symName == QLatin1String("else"))     return ELSE;
        if (symName == QLatin1String("for"))      return FOR;
        if (symName == QLatin1String("break"))    return BREAK;
        if (symName == QLatin1String("continue")) return CONTINUE;
        if (symName == QLatin1String("return"))   return RETURN;
        if (symName == QLatin1String("in"))       return IN;
        if (symName == QLatin1String("$args"))    return ARG_LOOKUP;
        if (symName == QLatin1String("delete") && follow_non_whitespace('(', SYMBOL, DELETE) == DELETE) return DELETE;
        if (symName == QLatin1String("define")) {
            InPtr -= 6;
            return 0;
        }


        if ((s = LookupSymbolEx(symName)) == nullptr) {
            s = InstallSymbolEx(symName,
                              symName[0]==QLatin1Char('$') ? (((symName[1] > QLatin1Char('0') && symName[1] <= QLatin1Char('9')) && symName.size() == 2) ? ARG_SYM : GLOBAL_SYM) : LOCAL_SYM,
                              make_value());
        }

        yylval.sym = s;
        return SYMBOL;
    }

    /* Process quoted strings with embedded escape sequences:
         For backslashes we recognise hexadecimal values with initial 'x' such
       as "\x1B"; octal value (upto 3 oct digits with a possible leading zero)
       such as "\33", "\033" or "\0033", and the C escapes: \", \', \n, \t, \b,
       \r, \f, \a, \v, and the added \e for the escape character, as for REs.
         Disallow hex/octal zero values (NUL): instead ignore the introductory
       backslash, eg "\x0xyz" becomes "x0xyz" and "\0000hello" becomes
       "0000hello". */

    if (*InPtr == QLatin1Char('\"')) {

        QString string;
        auto p = std::back_inserter(string);

        InPtr++;
        while (InPtr != EndPtr && *InPtr != QLatin1Char('\"') && *InPtr != QLatin1Char('\n')) {

            if (*InPtr == QLatin1Char('\\')) {
                QString::const_iterator backslash = InPtr;
                InPtr++;
                if (*InPtr == QLatin1Char('\n')) {
                    InPtr++;
                    continue;
                }
                if (*InPtr == QLatin1Char('x')) {
                    /* a hex introducer */
                    int hexValue = 0;
                    const char *const hexDigits = "0123456789abcdef";
                    const char *hexD;

                    InPtr++;

                    if (InPtr == EndPtr || (hexD = strchr(hexDigits, safe_tolower(InPtr->toLatin1()))) == nullptr) {
                        *p++ = QLatin1Char('x');
                    } else {
                        hexValue = static_cast<int>(hexD - hexDigits);
                        InPtr++;

                        /* now do we have another digit? only accept one more */
                        if (InPtr != EndPtr && (hexD = strchr(hexDigits, safe_tolower(InPtr->toLatin1()))) != nullptr){
                          hexValue = static_cast<int>(hexD - hexDigits + (hexValue << 4));
                          InPtr++;
                        }

                        if (hexValue != 0) {
                            *p++ = QLatin1Char(static_cast<char>(hexValue));
                        } else {
                            InPtr = backslash + 1; /* just skip the backslash */
                        }
                    }
                    continue;
                }

                /* the RE documentation requires \0 as the octal introducer;
                   here you can start with any octal digit, but you are only
                   allowed up to three (or four if the first is '0'). */
                if (QLatin1Char('0') <= *InPtr && *InPtr <= QLatin1Char('7')) {

                    if (*InPtr == QLatin1Char('0')) {
                        InPtr++;  /* octal introducer: don't count this digit */
                    }

                    if (QLatin1Char('0') <= *InPtr && *InPtr <= QLatin1Char('7')) {
                        /* treat as octal - first digit */
                        char octD = InPtr++->toLatin1();
                        int octValue = octD - '0';

                        if (QLatin1Char('0') <= *InPtr && *InPtr <= QLatin1Char('7')) {
                            /* second digit */
                            octD = InPtr++->toLatin1();
                            octValue = (octValue << 3) + octD - '0';
                            /* now do we have another digit? can we add it?
                               if value is going to be too big for char (greater
                               than 0377), stop converting now before adding the
                               third digit */
                            if (QLatin1Char('0') <= *InPtr && *InPtr <= QLatin1Char('7') &&
                                octValue <= 037) {
                                /* third digit is acceptable */
                                octD = InPtr++->toLatin1();
                                octValue = (octValue << 3) + octD - '0';
                            }
                        }

                        if (octValue != 0) {
                            *p++ = QLatin1Char(static_cast<char>(octValue));
                        } else {
                            InPtr = backslash + 1; /* just skip the backslash */
                        }
                    } else { /* \0 followed by non-digits: go back to 0 */
                        InPtr = backslash + 1; /* just skip the backslash */
                    }
                    continue;
                }

                for (i = 0; escape[i] != '\0'; i++) {
                    if (QLatin1Char(escape[i]) == *InPtr) {
                        *p++ = QLatin1Char(replace[i]);
                        InPtr++;
                        break;
                    }
                }

                /* if we get here, we didn't recognise the character after
                   the backslash: just copy it next time round the loop */
            } else {
                *p++= *InPtr++;
            }
        }

        InPtr++;
        yylval.sym = InstallStringConstSymbolEx(string);
        return STRING;
    }

    /* process remaining two character tokens or return single char as token */
    switch(InPtr++->toLatin1()) {
    case '>':   return follow('=', GE, GT);
    case '<':   return follow('=', LE, LT);
    case '=':   return follow('=', EQ, '=');
    case '!':   return follow('=', NE, NOT);
    case '+':   return follow2('+', INCR, '=', ADDEQ, '+');
    case '-':   return follow2('-', DECR, '=', SUBEQ, '-');
    case '|':   return follow2('|', OR, '=', OREQ, '|');
    case '&':   return follow2('&', AND, '=', ANDEQ, '&');
    case '*':   return follow2('*', POW, '=', MULEQ, '*');
    case '/':   return follow('=', DIVEQ, '/');
    case '%':   return follow('=', MODEQ, '%');
    case '^':   return POW;
    default:    return (InPtr-1)->toLatin1();
    }
}

/*
** look ahead for >=, etc.
*/
static int follow(char expect, int yes, int no) {

    if (*InPtr++ == QLatin1Char(expect))
        return yes;
    InPtr--;
    return no;
}

static int follow2(char expect1, int yes1, char expect2, int yes2, int no) {

    QChar next = *InPtr++;
    if (next == QLatin1Char(expect1))
        return yes1;
    if (next == QLatin1Char(expect2))
        return yes2;
    InPtr--;
    return no;
}

static int follow_non_whitespace(char expect, int yes, int no) {

    QString::const_iterator localInPtr = InPtr;

    while (1) {
        if (*localInPtr == QLatin1Char(' ') || *localInPtr == QLatin1Char('\t')) {
            ++localInPtr;
        }
        else if (*localInPtr == QLatin1Char('\\') && *(localInPtr + 1) == QLatin1Char('\n')) {
            localInPtr += 2;
        }
        else if (*localInPtr == QLatin1Char(expect)) {
            return(yes);
        }
        else {
            return(no);
        }
    }
}

/*
** Called by yacc to report errors (just stores for returning when
** parsing is aborted.  The error token action is to immediate abort
** parsing, so this message is immediately reported to the caller
** of ParseExpr)
*/
static int yyerror(const char *s) {
    ErrMsg = QString::fromLatin1(s);
    return 0;
}
