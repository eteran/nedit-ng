
/* A Bison parser, made by GNU Bison 2.4.1.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C
   
      Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.4.1"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Using locations.  */
#define YYLSP_NEEDED 0



/* Copy the first part of user declarations.  */

/* Line 189 of yacc.c  */
#line 1 "parser.y"

#include "parse.h"
#include "interpret.h"
#include "Util/utils.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cctype>
#include <string>

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



/* Line 189 of yacc.c  */
#line 110 "C:/Users/Evan Teran/Desktop/build-nedit-ng-Desktop_Qt_5_12_0_MSVC2017_64bit-Default/Interpreter/parser.cpp"

/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     NUMBER = 258,
     STRING = 259,
     SYMBOL = 260,
     DELETE = 261,
     ARG_LOOKUP = 262,
     IF = 263,
     WHILE = 264,
     ELSE = 265,
     FOR = 266,
     BREAK = 267,
     CONTINUE = 268,
     RETURN = 269,
     IF_NO_ELSE = 270,
     OREQ = 271,
     ANDEQ = 272,
     MODEQ = 273,
     DIVEQ = 274,
     MULEQ = 275,
     SUBEQ = 276,
     ADDEQ = 277,
     CONCAT = 278,
     OR = 279,
     AND = 280,
     IN = 281,
     NE = 282,
     EQ = 283,
     LE = 284,
     LT = 285,
     GE = 286,
     GT = 287,
     NOT = 288,
     UNARY_MINUS = 289,
     DECR = 290,
     INCR = 291,
     POW = 292
   };
#endif



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 214 of yacc.c  */
#line 37 "parser.y"

    Symbol *sym;
    Inst *inst;
    int nArgs;



/* Line 214 of yacc.c  */
#line 191 "C:/Users/Evan Teran/Desktop/build-nedit-ng-Desktop_Qt_5_12_0_MSVC2017_64bit-Default/Interpreter/parser.cpp"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


/* Copy the second part of user declarations.  */


/* Line 264 of yacc.c  */
#line 203 "C:/Users/Evan Teran/Desktop/build-nedit-ng-Desktop_Qt_5_12_0_MSVC2017_64bit-Default/Interpreter/parser.cpp"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef _STDLIB_H
#      define _STDLIB_H 1
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined _STDLIB_H \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef _STDLIB_H
#    define _STDLIB_H 1
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  4
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   548

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  55
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  22
/* YYNRULES -- Number of rules.  */
#define YYNRULES  102
/* YYNRULES -- Number of states.  */
#define YYNSTATES  208

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   292

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      50,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,    40,    28,     2,
      47,    51,    38,    36,    54,    37,     2,    39,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    52,
       2,    16,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    46,     2,    53,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    48,    27,    49,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    29,    30,    31,    32,    33,    34,    35,    41,    42,
      43,    44,    45
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     6,    12,    17,    19,    25,    30,    32,
      34,    37,    41,    48,    58,    65,    76,    77,    87,    91,
      95,   100,   104,   108,   112,   116,   120,   124,   128,   132,
     136,   142,   149,   156,   163,   170,   177,   184,   191,   198,
     204,   210,   216,   222,   227,   230,   233,   236,   239,   241,
     242,   244,   248,   249,   251,   255,   257,   260,   262,   267,
     269,   274,   276,   278,   280,   282,   287,   291,   296,   300,
     302,   307,   311,   315,   319,   323,   327,   331,   334,   338,
     342,   346,   350,   354,   358,   362,   366,   370,   374,   377,
     380,   383,   386,   389,   393,   395,   397,   399,   400,   402,
     404,   406,   407
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      56,     0,    -1,    76,    58,    -1,    76,    48,    76,    58,
      49,    -1,    76,    48,    76,    49,    -1,     1,    -1,    48,
      76,    58,    49,    76,    -1,    48,    76,    49,    76,    -1,
      59,    -1,    59,    -1,    58,    59,    -1,    61,    50,    76,
      -1,     8,    47,    73,    51,    76,    57,    -1,     8,    47,
      73,    51,    76,    57,    72,    76,    57,    -1,    70,    47,
      73,    51,    76,    57,    -1,    71,    47,    63,    52,    73,
      52,    63,    51,    76,    57,    -1,    -1,    71,    47,     5,
      29,    68,    51,    60,    76,    57,    -1,    12,    50,    76,
      -1,    13,    50,    76,    -1,    14,    65,    50,    76,    -1,
      14,    50,    76,    -1,     5,    16,    65,    -1,    62,    23,
      65,    -1,    62,    22,    65,    -1,    62,    21,    65,    -1,
      62,    20,    65,    -1,    62,    19,    65,    -1,    62,    18,
      65,    -1,    62,    17,    65,    -1,     6,    67,    46,    64,
      53,    -1,    66,    46,    64,    53,    16,    65,    -1,    66,
      46,    64,    53,    23,    65,    -1,    66,    46,    64,    53,
      22,    65,    -1,    66,    46,    64,    53,    21,    65,    -1,
      66,    46,    64,    53,    20,    65,    -1,    66,    46,    64,
      53,    19,    65,    -1,    66,    46,    64,    53,    18,    65,
      -1,    66,    46,    64,    53,    17,    65,    -1,    66,    46,
      64,    53,    44,    -1,    66,    46,    64,    53,    43,    -1,
      44,    66,    46,    64,    53,    -1,    43,    66,    46,    64,
      53,    -1,     5,    47,    64,    51,    -1,    44,     5,    -1,
       5,    44,    -1,    43,     5,    -1,     5,    43,    -1,     5,
      -1,    -1,    61,    -1,    63,    54,    61,    -1,    -1,    65,
      -1,    64,    54,    65,    -1,    69,    -1,    65,    69,    -1,
       5,    -1,    66,    46,    64,    53,    -1,     5,    -1,    67,
      46,    64,    53,    -1,    69,    -1,     3,    -1,     4,    -1,
       5,    -1,     5,    47,    64,    51,    -1,    47,    65,    51,
      -1,     7,    46,    69,    53,    -1,     7,    46,    53,    -1,
       7,    -1,    69,    46,    64,    53,    -1,    69,    36,    69,
      -1,    69,    37,    69,    -1,    69,    38,    69,    -1,    69,
      39,    69,    -1,    69,    40,    69,    -1,    69,    45,    69,
      -1,    37,    69,    -1,    69,    35,    69,    -1,    69,    34,
      69,    -1,    69,    33,    69,    -1,    69,    32,    69,    -1,
      69,    31,    69,    -1,    69,    30,    69,    -1,    69,    28,
      69,    -1,    69,    27,    69,    -1,    69,    74,    69,    -1,
      69,    75,    69,    -1,    41,    69,    -1,    44,     5,    -1,
       5,    44,    -1,    43,     5,    -1,     5,    43,    -1,    69,
      29,    69,    -1,     9,    -1,    11,    -1,    10,    -1,    -1,
      69,    -1,    26,    -1,    25,    -1,    -1,    76,    50,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,    72,    72,    75,    78,    81,    85,    86,    87,    89,
      90,    92,    93,    96,    99,   103,   108,   108,   118,   124,
     130,   133,   137,   140,   143,   146,   149,   152,   155,   158,
     161,   164,   167,   172,   177,   182,   187,   192,   197,   202,
     207,   212,   217,   222,   226,   230,   234,   238,   243,   247,
     250,   253,   257,   260,   263,   267,   268,   272,   275,   279,
     282,   286,   290,   293,   296,   299,   304,   305,   308,   311,
     314,   317,   320,   323,   326,   329,   332,   335,   338,   341,
     344,   347,   350,   353,   356,   359,   362,   365,   368,   371,
     375,   379,   383,   387,   391,   395,   399,   403,   406,   410,
     415,   420,   421
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "NUMBER", "STRING", "SYMBOL", "DELETE",
  "ARG_LOOKUP", "IF", "WHILE", "ELSE", "FOR", "BREAK", "CONTINUE",
  "RETURN", "IF_NO_ELSE", "'='", "OREQ", "ANDEQ", "MODEQ", "DIVEQ",
  "MULEQ", "SUBEQ", "ADDEQ", "CONCAT", "OR", "AND", "'|'", "'&'", "IN",
  "NE", "EQ", "LE", "LT", "GE", "GT", "'+'", "'-'", "'*'", "'/'", "'%'",
  "NOT", "UNARY_MINUS", "DECR", "INCR", "POW", "'['", "'('", "'{'", "'}'",
  "'\\n'", "')'", "';'", "']'", "','", "$accept", "program", "block",
  "stmts", "stmt", "$@1", "simpstmt", "evalsym", "comastmts", "arglist",
  "expr", "initarraylv", "arraylv", "arrayexpr", "numexpr", "while", "for",
  "else", "cond", "and", "or", "blank", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,    61,   271,   272,   273,
     274,   275,   276,   277,   278,   279,   280,   124,    38,   281,
     282,   283,   284,   285,   286,   287,    43,    45,    42,    47,
      37,   288,   289,   290,   291,   292,    91,    40,   123,   125,
      10,    41,    59,    93,    44
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    55,    56,    56,    56,    56,    57,    57,    57,    58,
      58,    59,    59,    59,    59,    59,    60,    59,    59,    59,
      59,    59,    61,    61,    61,    61,    61,    61,    61,    61,
      61,    61,    61,    61,    61,    61,    61,    61,    61,    61,
      61,    61,    61,    61,    61,    61,    61,    61,    62,    63,
      63,    63,    64,    64,    64,    65,    65,    66,    66,    67,
      67,    68,    69,    69,    69,    69,    69,    69,    69,    69,
      69,    69,    69,    69,    69,    69,    69,    69,    69,    69,
      69,    69,    69,    69,    69,    69,    69,    69,    69,    69,
      69,    69,    69,    69,    70,    71,    72,    73,    73,    74,
      75,    76,    76
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     2,     5,     4,     1,     5,     4,     1,     1,
       2,     3,     6,     9,     6,    10,     0,     9,     3,     3,
       4,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       5,     6,     6,     6,     6,     6,     6,     6,     6,     5,
       5,     5,     5,     4,     2,     2,     2,     2,     1,     0,
       1,     3,     0,     1,     3,     1,     2,     1,     4,     1,
       4,     1,     1,     1,     1,     4,     3,     4,     3,     1,
       4,     3,     3,     3,     3,     3,     3,     2,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     2,     2,
       2,     2,     2,     3,     1,     1,     1,     0,     1,     1,
       1,     0,     2
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     5,     0,     0,     1,    48,     0,     0,    94,    95,
       0,     0,     0,     0,     0,   101,   102,     2,     9,     0,
       0,     0,     0,     0,     0,    47,    45,    52,    59,     0,
      97,   101,   101,    62,    63,    64,    69,     0,     0,     0,
       0,     0,   101,     0,    55,    46,     0,    44,     0,     0,
      10,   101,     0,     0,     0,     0,     0,     0,     0,    52,
      97,    49,    22,     0,    53,    52,    98,     0,    18,    19,
      92,    90,    52,     0,    77,    88,    91,    89,     0,    21,
     101,    56,   100,    99,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    52,
       0,     0,    52,    52,     4,     0,    11,    29,    28,    27,
      26,    25,    24,    23,     0,     0,    48,    50,     0,    43,
       0,     0,   101,     0,    68,     0,    66,    20,    85,    84,
      93,    83,    82,    81,    80,    79,    78,    71,    72,    73,
      74,    75,    76,     0,    86,    87,     0,     0,     3,    58,
     101,     0,    97,     0,    54,    30,     0,    65,    67,    70,
      42,    41,     0,     0,     0,     0,     0,     0,     0,     0,
      40,    39,     0,     0,    61,     0,    51,   101,    12,     8,
      31,    38,    37,    36,    35,    34,    33,    32,    14,    16,
      49,     0,    96,   101,   101,     0,   101,     0,     0,     0,
     101,     7,   101,    13,    17,     0,     6,    15
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     2,   178,    17,   179,   194,    19,    20,   118,    63,
      64,    21,    29,   173,    44,    22,    23,   193,    67,   100,
     101,     3
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -124
static const yytype_int16 yypact[] =
{
      22,  -124,    10,   187,  -124,   235,    -1,   -30,  -124,  -124,
     -18,   -10,    82,    39,    64,  -124,  -124,   335,  -124,    34,
     525,    61,     8,    63,   330,  -124,  -124,   330,  -124,    62,
     330,  -124,  -124,  -124,  -124,   -31,    75,   330,   330,   119,
     122,   330,  -124,   170,   406,    93,   114,    93,   115,   241,
    -124,  -124,   330,   330,   330,   330,   330,   330,   330,   330,
     330,    47,   330,   -48,   330,   330,   406,    77,    88,    88,
    -124,  -124,   330,     4,   -20,   -20,  -124,  -124,    17,    88,
    -124,   406,  -124,  -124,   330,   330,   330,   330,   330,   330,
     330,   330,   330,   330,   330,   330,   330,   330,   330,   330,
     330,   330,   330,   330,  -124,   307,    88,   330,   330,   330,
     330,   330,   330,   330,     9,   111,   339,  -124,    40,  -124,
     330,    20,  -124,   -12,  -124,   367,  -124,    88,   466,   484,
     495,   495,   495,   495,   495,   495,   495,    95,    95,   -20,
     -20,   -20,   -20,    25,   447,   427,    45,    50,  -124,   405,
    -124,   330,   330,   347,   330,   117,   251,  -124,  -124,  -124,
     126,   126,   330,   330,   330,   330,   330,   330,   330,   330,
    -124,  -124,   251,   125,   406,   130,  -124,  -124,   168,  -124,
     330,   330,   330,   330,   330,   330,   330,   330,  -124,  -124,
     347,   261,  -124,  -124,  -124,     5,  -124,   317,   251,   251,
    -124,    88,  -124,  -124,  -124,   251,    88,  -124
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
    -124,  -124,  -123,   -34,    -3,  -124,   -60,  -124,    -6,   -22,
      59,    92,  -124,  -124,    58,  -124,  -124,  -124,   -55,  -124,
    -124,   -13
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -102
static const yytype_int16 yytable[] =
{
      18,   117,    49,   119,    28,   115,   120,    33,    34,    35,
       4,    36,    70,    71,    50,   105,    72,    30,    68,    69,
      33,    34,    35,     1,    36,    98,    99,  -101,  -101,    79,
    -101,  -101,    31,  -101,  -101,  -101,  -101,   114,   106,   157,
      32,    37,   120,   121,    45,    38,    18,    39,    40,   188,
     123,    41,   116,     6,    37,    60,   200,   124,    38,   153,
      39,    40,   149,   120,    41,  -101,  -101,   127,   126,    47,
    -101,    43,  -101,   155,   120,   203,   204,   143,   159,   120,
     146,   147,   207,    62,    51,    33,    34,    35,    66,    36,
      13,    14,   152,   176,   153,    74,    75,   175,   160,   120,
      78,    81,    50,   161,   120,    46,    48,    59,    65,   156,
      61,   107,   108,   109,   110,   111,   112,   113,    66,    37,
      81,    73,    81,    38,    76,    39,    40,    77,   122,    41,
     117,   125,    42,    95,    96,    97,    81,   172,    16,   -57,
      98,    99,   128,   129,   130,   131,   132,   133,   134,   135,
     136,   137,   138,   139,   140,   141,   142,   197,   144,   145,
     102,   103,   150,   -60,   191,    81,    81,    81,    81,    81,
      81,    81,   -58,    33,    34,    35,   189,    36,   192,   154,
     198,   199,   190,   201,   195,     0,     0,   205,    18,   206,
       0,     0,     5,     6,    50,     7,     8,     0,     9,    10,
      11,    12,     0,     0,     0,     0,     0,    37,     0,   174,
      66,    38,    81,    39,    40,     0,     0,    41,     0,     0,
      80,   180,   181,   182,   183,   184,   185,   186,   187,     0,
      13,    14,     0,     0,     0,    15,     0,    16,    81,    81,
      81,    81,    81,    81,    81,    81,     5,     6,     0,     7,
       8,    24,     9,    10,    11,    12,     5,     6,     0,     7,
       8,     0,     9,    10,    11,    12,     5,     6,     0,     7,
       8,     0,     9,    10,    11,    12,     0,     0,    25,    26,
       0,   -57,    27,     0,    13,    14,     0,     0,     0,     0,
     104,    16,     0,     0,    13,    14,     0,     0,     0,   177,
       0,    16,     0,     0,    13,    14,     0,     0,     0,     0,
     196,    16,     5,     6,     0,     7,     8,     0,     9,    10,
      11,    12,     5,     6,     0,     7,     8,     0,     9,    10,
      11,    12,     0,    33,    34,    35,     0,    36,     0,     0,
       5,     6,     0,     7,     8,     0,     9,    10,    11,    12,
      13,    14,     5,     6,     0,    24,   148,     0,     0,     0,
      13,    14,     0,     0,     0,     0,   202,    37,   151,     0,
       0,    38,     0,    39,    40,     0,     0,    41,    13,    14,
       0,     0,    25,    26,     0,   -57,    27,     0,     0,     0,
      13,    14,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,    96,    97,     0,     0,
       0,     0,    98,    99,     0,     0,     0,     0,     0,     0,
     158,   162,   163,   164,   165,   166,   167,   168,   169,     0,
       0,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    93,    94,    95,    96,    97,     0,   170,   171,
       0,    98,    99,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,    96,    97,     0,     0,
       0,     0,    98,    99,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,    96,    97,     0,     0,
       0,     0,    98,    99,    85,    86,    87,    88,    89,    90,
      91,    92,    93,    94,    95,    96,    97,     0,     0,     0,
       0,    98,    99,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    96,    97,     0,     0,     0,     0,    98,
      99,    93,    94,    95,    96,    97,     0,     0,     0,     0,
      98,    99,    52,    53,    54,    55,    56,    57,    58
};

static const yytype_int16 yycheck[] =
{
       3,    61,    15,    51,     5,    60,    54,     3,     4,     5,
       0,     7,    43,    44,    17,    49,    47,    47,    31,    32,
       3,     4,     5,     1,     7,    45,    46,     5,     6,    42,
       8,     9,    50,    11,    12,    13,    14,    59,    51,    51,
      50,    37,    54,    65,     5,    41,    49,    43,    44,   172,
      72,    47,     5,     6,    37,    47,    51,    53,    41,    54,
      43,    44,    53,    54,    47,    43,    44,    80,    51,     5,
      48,    12,    50,    53,    54,   198,   199,    99,    53,    54,
     102,   103,   205,    24,    50,     3,     4,     5,    30,     7,
      43,    44,    52,   153,    54,    37,    38,   152,    53,    54,
      41,    43,   105,    53,    54,    13,    14,    46,    46,   122,
      47,    52,    53,    54,    55,    56,    57,    58,    60,    37,
      62,    46,    64,    41,     5,    43,    44,     5,    51,    47,
     190,    73,    50,    38,    39,    40,    78,   150,    50,    46,
      45,    46,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    93,    94,    95,    96,    97,    98,   191,   100,   101,
      46,    46,    51,    46,   177,   107,   108,   109,   110,   111,
     112,   113,    46,     3,     4,     5,    51,     7,    10,   120,
     193,   194,    52,   196,   190,    -1,    -1,   200,   191,   202,
      -1,    -1,     5,     6,   197,     8,     9,    -1,    11,    12,
      13,    14,    -1,    -1,    -1,    -1,    -1,    37,    -1,   151,
     152,    41,   154,    43,    44,    -1,    -1,    47,    -1,    -1,
      50,   162,   163,   164,   165,   166,   167,   168,   169,    -1,
      43,    44,    -1,    -1,    -1,    48,    -1,    50,   180,   181,
     182,   183,   184,   185,   186,   187,     5,     6,    -1,     8,
       9,    16,    11,    12,    13,    14,     5,     6,    -1,     8,
       9,    -1,    11,    12,    13,    14,     5,     6,    -1,     8,
       9,    -1,    11,    12,    13,    14,    -1,    -1,    43,    44,
      -1,    46,    47,    -1,    43,    44,    -1,    -1,    -1,    -1,
      49,    50,    -1,    -1,    43,    44,    -1,    -1,    -1,    48,
      -1,    50,    -1,    -1,    43,    44,    -1,    -1,    -1,    -1,
      49,    50,     5,     6,    -1,     8,     9,    -1,    11,    12,
      13,    14,     5,     6,    -1,     8,     9,    -1,    11,    12,
      13,    14,    -1,     3,     4,     5,    -1,     7,    -1,    -1,
       5,     6,    -1,     8,     9,    -1,    11,    12,    13,    14,
      43,    44,     5,     6,    -1,    16,    49,    -1,    -1,    -1,
      43,    44,    -1,    -1,    -1,    -1,    49,    37,    29,    -1,
      -1,    41,    -1,    43,    44,    -1,    -1,    47,    43,    44,
      -1,    -1,    43,    44,    -1,    46,    47,    -1,    -1,    -1,
      43,    44,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    -1,    -1,
      -1,    -1,    45,    46,    -1,    -1,    -1,    -1,    -1,    -1,
      53,    16,    17,    18,    19,    20,    21,    22,    23,    -1,
      -1,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    -1,    43,    44,
      -1,    45,    46,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    -1,    -1,
      -1,    -1,    45,    46,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    -1,    -1,
      -1,    -1,    45,    46,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    -1,    -1,    -1,
      -1,    45,    46,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    -1,    -1,    -1,    -1,    45,
      46,    36,    37,    38,    39,    40,    -1,    -1,    -1,    -1,
      45,    46,    17,    18,    19,    20,    21,    22,    23
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     1,    56,    76,     0,     5,     6,     8,     9,    11,
      12,    13,    14,    43,    44,    48,    50,    58,    59,    61,
      62,    66,    70,    71,    16,    43,    44,    47,     5,    67,
      47,    50,    50,     3,     4,     5,     7,    37,    41,    43,
      44,    47,    50,    65,    69,     5,    66,     5,    66,    76,
      59,    50,    17,    18,    19,    20,    21,    22,    23,    46,
      47,    47,    65,    64,    65,    46,    69,    73,    76,    76,
      43,    44,    47,    46,    69,    69,     5,     5,    65,    76,
      50,    69,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    45,    46,
      74,    75,    46,    46,    49,    58,    76,    65,    65,    65,
      65,    65,    65,    65,    64,    73,     5,    61,    63,    51,
      54,    64,    51,    64,    53,    69,    51,    76,    69,    69,
      69,    69,    69,    69,    69,    69,    69,    69,    69,    69,
      69,    69,    69,    64,    69,    69,    64,    64,    49,    53,
      51,    29,    52,    54,    65,    53,    76,    51,    53,    53,
      53,    53,    16,    17,    18,    19,    20,    21,    22,    23,
      43,    44,    76,    68,    69,    73,    61,    48,    57,    59,
      65,    65,    65,    65,    65,    65,    65,    65,    57,    51,
      52,    76,    10,    72,    60,    63,    49,    58,    76,    76,
      51,    76,    49,    57,    57,    76,    76,    57
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
	      (Loc).first_line, (Loc).first_column,	\
	      (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule)
#else
static void
yy_reduce_print (yyvsp, yyrule)
    YYSTYPE *yyvsp;
    int yyrule;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into YYRESULT an error message about the unexpected token
   YYCHAR while in state YYSTATE.  Return the number of bytes copied,
   including the terminating null byte.  If YYRESULT is null, do not
   copy anything; just return the number of bytes that would be
   copied.  As a special case, return 0 if an ordinary "syntax error"
   message will do.  Return YYSIZE_MAXIMUM if overflow occurs during
   size calculation.  */
static YYSIZE_T
yysyntax_error (char *yyresult, int yystate, int yychar)
{
  int yyn = yypact[yystate];

  if (! (YYPACT_NINF < yyn && yyn <= YYLAST))
    return 0;
  else
    {
      int yytype = YYTRANSLATE (yychar);
      YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
      YYSIZE_T yysize = yysize0;
      YYSIZE_T yysize1;
      int yysize_overflow = 0;
      enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
      char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
      int yyx;

# if 0
      /* This is so xgettext sees the translatable formats that are
	 constructed on the fly.  */
      YY_("syntax error, unexpected %s");
      YY_("syntax error, unexpected %s, expecting %s");
      YY_("syntax error, unexpected %s, expecting %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
# endif
      char *yyfmt;
      char const *yyf;
      static char const yyunexpected[] = "syntax error, unexpected %s";
      static char const yyexpecting[] = ", expecting %s";
      static char const yyor[] = " or %s";
      char yyformat[sizeof yyunexpected
		    + sizeof yyexpecting - 1
		    + ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
		       * (sizeof yyor - 1))];
      char const *yyprefix = yyexpecting;

      /* Start YYX at -YYN if negative to avoid negative indexes in
	 YYCHECK.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;

      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yycount = 1;

      yyarg[0] = yytname[yytype];
      yyfmt = yystpcpy (yyformat, yyunexpected);

      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	  {
	    if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
	      {
		yycount = 1;
		yysize = yysize0;
		yyformat[sizeof yyunexpected - 1] = '\0';
		break;
	      }
	    yyarg[yycount++] = yytname[yyx];
	    yysize1 = yysize + yytnamerr (0, yytname[yyx]);
	    yysize_overflow |= (yysize1 < yysize);
	    yysize = yysize1;
	    yyfmt = yystpcpy (yyfmt, yyprefix);
	    yyprefix = yyor;
	  }

      yyf = YY_(yyformat);
      yysize1 = yysize + yystrlen (yyf);
      yysize_overflow |= (yysize1 < yysize);
      yysize = yysize1;

      if (yysize_overflow)
	return YYSIZE_MAXIMUM;

      if (yyresult)
	{
	  /* Avoid sprintf, as that infringes on the user's name space.
	     Don't have undefined behavior even if the translation
	     produced a string with the wrong number of "%s"s.  */
	  char *yyp = yyresult;
	  int yyi = 0;
	  while ((*yyp = *yyf) != '\0')
	    {
	      if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		{
		  yyp += yytnamerr (yyp, yyarg[yyi++]);
		  yyf += 2;
		}
	      else
		{
		  yyp++;
		  yyf++;
		}
	    }
	}
      return yysize;
    }
}
#endif /* YYERROR_VERBOSE */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  YYUSE (yyvaluep);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}

/* Prevent warnings from -Wmissing-prototypes.  */
#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */


/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;



/*-------------------------.
| yyparse or yypush_parse.  |
`-------------------------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{


    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.

       Refer to the stacks thru separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yytoken = 0;
  yyss = yyssa;
  yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */
  yyssp = yyss;
  yyvsp = yyvs;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss_alloc, yyss);
	YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:

/* Line 1455 of yacc.c  */
#line 72 "parser.y"
    {
                ADD_OP(OP_RETURN_NO_VAL); return 0;
            ;}
    break;

  case 3:

/* Line 1455 of yacc.c  */
#line 75 "parser.y"
    {
                ADD_OP(OP_RETURN_NO_VAL); return 0;
            ;}
    break;

  case 4:

/* Line 1455 of yacc.c  */
#line 78 "parser.y"
    {
                ADD_OP(OP_RETURN_NO_VAL); return 0;
            ;}
    break;

  case 5:

/* Line 1455 of yacc.c  */
#line 81 "parser.y"
    {
                return 1;
            ;}
    break;

  case 12:

/* Line 1455 of yacc.c  */
#line 93 "parser.y"
    {
                SET_BR_OFF((yyvsp[(3) - (6)].inst), GetPC());
            ;}
    break;

  case 13:

/* Line 1455 of yacc.c  */
#line 96 "parser.y"
    {
                SET_BR_OFF((yyvsp[(3) - (9)].inst), ((yyvsp[(7) - (9)].inst)+1)); SET_BR_OFF((yyvsp[(7) - (9)].inst), GetPC());
            ;}
    break;

  case 14:

/* Line 1455 of yacc.c  */
#line 99 "parser.y"
    {
                ADD_OP(OP_BRANCH); ADD_BR_OFF((yyvsp[(1) - (6)].inst));
                SET_BR_OFF((yyvsp[(3) - (6)].inst), GetPC()); FillLoopAddrs(GetPC(), (yyvsp[(1) - (6)].inst));
            ;}
    break;

  case 15:

/* Line 1455 of yacc.c  */
#line 103 "parser.y"
    {
                FillLoopAddrs(GetPC()+2+((yyvsp[(7) - (10)].inst)-((yyvsp[(5) - (10)].inst)+1)), GetPC());
                SwapCode((yyvsp[(5) - (10)].inst)+1, (yyvsp[(7) - (10)].inst), GetPC());
                ADD_OP(OP_BRANCH); ADD_BR_OFF((yyvsp[(3) - (10)].inst)); SET_BR_OFF((yyvsp[(5) - (10)].inst), GetPC());
            ;}
    break;

  case 16:

/* Line 1455 of yacc.c  */
#line 108 "parser.y"
    {
                Symbol *iterSym = InstallIteratorSymbol();
                ADD_OP(OP_BEGIN_ARRAY_ITER); ADD_SYM(iterSym);
                ADD_OP(OP_ARRAY_ITER); ADD_SYM((yyvsp[(3) - (6)].sym)); ADD_SYM(iterSym); ADD_BR_OFF(nullptr);
            ;}
    break;

  case 17:

/* Line 1455 of yacc.c  */
#line 113 "parser.y"
    {
                    ADD_OP(OP_BRANCH); ADD_BR_OFF((yyvsp[(5) - (9)].inst)+2);
                    SET_BR_OFF((yyvsp[(5) - (9)].inst)+5, GetPC());
                    FillLoopAddrs(GetPC(), (yyvsp[(5) - (9)].inst)+2);
            ;}
    break;

  case 18:

/* Line 1455 of yacc.c  */
#line 118 "parser.y"
    {
                ADD_OP(OP_BRANCH); ADD_BR_OFF(nullptr);
                if (AddBreakAddr(GetPC()-1)) {
                    yyerror("break outside loop"); YYERROR;
                }
            ;}
    break;

  case 19:

/* Line 1455 of yacc.c  */
#line 124 "parser.y"
    {
                ADD_OP(OP_BRANCH); ADD_BR_OFF(nullptr);
                if (AddContinueAddr(GetPC()-1)) {
                    yyerror("continue outside loop"); YYERROR;
                }
            ;}
    break;

  case 20:

/* Line 1455 of yacc.c  */
#line 130 "parser.y"
    {
                ADD_OP(OP_RETURN);
            ;}
    break;

  case 21:

/* Line 1455 of yacc.c  */
#line 133 "parser.y"
    {
                ADD_OP(OP_RETURN_NO_VAL);
            ;}
    break;

  case 22:

/* Line 1455 of yacc.c  */
#line 137 "parser.y"
    {
                ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[(1) - (3)].sym));
            ;}
    break;

  case 23:

/* Line 1455 of yacc.c  */
#line 140 "parser.y"
    {
                ADD_OP(OP_ADD); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[(1) - (3)].sym));
            ;}
    break;

  case 24:

/* Line 1455 of yacc.c  */
#line 143 "parser.y"
    {
                ADD_OP(OP_SUB); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[(1) - (3)].sym));
            ;}
    break;

  case 25:

/* Line 1455 of yacc.c  */
#line 146 "parser.y"
    {
                ADD_OP(OP_MUL); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[(1) - (3)].sym));
            ;}
    break;

  case 26:

/* Line 1455 of yacc.c  */
#line 149 "parser.y"
    {
                ADD_OP(OP_DIV); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[(1) - (3)].sym));
            ;}
    break;

  case 27:

/* Line 1455 of yacc.c  */
#line 152 "parser.y"
    {
                ADD_OP(OP_MOD); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[(1) - (3)].sym));
            ;}
    break;

  case 28:

/* Line 1455 of yacc.c  */
#line 155 "parser.y"
    {
                ADD_OP(OP_BIT_AND); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[(1) - (3)].sym));
            ;}
    break;

  case 29:

/* Line 1455 of yacc.c  */
#line 158 "parser.y"
    {
                ADD_OP(OP_BIT_OR); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[(1) - (3)].sym));
            ;}
    break;

  case 30:

/* Line 1455 of yacc.c  */
#line 161 "parser.y"
    {
                ADD_OP(OP_ARRAY_DELETE); ADD_IMMED((yyvsp[(4) - (5)].nArgs));
            ;}
    break;

  case 31:

/* Line 1455 of yacc.c  */
#line 164 "parser.y"
    {
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[(3) - (6)].nArgs));
            ;}
    break;

  case 32:

/* Line 1455 of yacc.c  */
#line 167 "parser.y"
    {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(1); ADD_IMMED((yyvsp[(3) - (6)].nArgs));
                ADD_OP(OP_ADD);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[(3) - (6)].nArgs));
            ;}
    break;

  case 33:

/* Line 1455 of yacc.c  */
#line 172 "parser.y"
    {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(1); ADD_IMMED((yyvsp[(3) - (6)].nArgs));
                ADD_OP(OP_SUB);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[(3) - (6)].nArgs));
            ;}
    break;

  case 34:

/* Line 1455 of yacc.c  */
#line 177 "parser.y"
    {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(1); ADD_IMMED((yyvsp[(3) - (6)].nArgs));
                ADD_OP(OP_MUL);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[(3) - (6)].nArgs));
            ;}
    break;

  case 35:

/* Line 1455 of yacc.c  */
#line 182 "parser.y"
    {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(1); ADD_IMMED((yyvsp[(3) - (6)].nArgs));
                ADD_OP(OP_DIV);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[(3) - (6)].nArgs));
            ;}
    break;

  case 36:

/* Line 1455 of yacc.c  */
#line 187 "parser.y"
    {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(1); ADD_IMMED((yyvsp[(3) - (6)].nArgs));
                ADD_OP(OP_MOD);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[(3) - (6)].nArgs));
            ;}
    break;

  case 37:

/* Line 1455 of yacc.c  */
#line 192 "parser.y"
    {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(1); ADD_IMMED((yyvsp[(3) - (6)].nArgs));
                ADD_OP(OP_BIT_AND);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[(3) - (6)].nArgs));
            ;}
    break;

  case 38:

/* Line 1455 of yacc.c  */
#line 197 "parser.y"
    {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(1); ADD_IMMED((yyvsp[(3) - (6)].nArgs));
                ADD_OP(OP_BIT_OR);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[(3) - (6)].nArgs));
            ;}
    break;

  case 39:

/* Line 1455 of yacc.c  */
#line 202 "parser.y"
    {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(0); ADD_IMMED((yyvsp[(3) - (5)].nArgs));
                ADD_OP(OP_INCR);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[(3) - (5)].nArgs));
            ;}
    break;

  case 40:

/* Line 1455 of yacc.c  */
#line 207 "parser.y"
    {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(0); ADD_IMMED((yyvsp[(3) - (5)].nArgs));
                ADD_OP(OP_DECR);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[(3) - (5)].nArgs));
            ;}
    break;

  case 41:

/* Line 1455 of yacc.c  */
#line 212 "parser.y"
    {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(0); ADD_IMMED((yyvsp[(4) - (5)].nArgs));
                ADD_OP(OP_INCR);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[(4) - (5)].nArgs));
            ;}
    break;

  case 42:

/* Line 1455 of yacc.c  */
#line 217 "parser.y"
    {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(0); ADD_IMMED((yyvsp[(4) - (5)].nArgs));
                ADD_OP(OP_DECR);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[(4) - (5)].nArgs));
            ;}
    break;

  case 43:

/* Line 1455 of yacc.c  */
#line 222 "parser.y"
    {
                ADD_OP(OP_SUBR_CALL);
                ADD_SYM(PromoteToGlobal((yyvsp[(1) - (4)].sym))); ADD_IMMED((yyvsp[(3) - (4)].nArgs));
            ;}
    break;

  case 44:

/* Line 1455 of yacc.c  */
#line 226 "parser.y"
    {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[(2) - (2)].sym)); ADD_OP(OP_INCR);
                ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[(2) - (2)].sym));
            ;}
    break;

  case 45:

/* Line 1455 of yacc.c  */
#line 230 "parser.y"
    {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[(1) - (2)].sym)); ADD_OP(OP_INCR);
                ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[(1) - (2)].sym));
            ;}
    break;

  case 46:

/* Line 1455 of yacc.c  */
#line 234 "parser.y"
    {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[(2) - (2)].sym)); ADD_OP(OP_DECR);
                ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[(2) - (2)].sym));
            ;}
    break;

  case 47:

/* Line 1455 of yacc.c  */
#line 238 "parser.y"
    {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[(1) - (2)].sym)); ADD_OP(OP_DECR);
                ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[(1) - (2)].sym));
            ;}
    break;

  case 48:

/* Line 1455 of yacc.c  */
#line 243 "parser.y"
    {
                (yyval.sym) = (yyvsp[(1) - (1)].sym); ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[(1) - (1)].sym));
            ;}
    break;

  case 49:

/* Line 1455 of yacc.c  */
#line 247 "parser.y"
    {
                (yyval.inst) = GetPC();
            ;}
    break;

  case 50:

/* Line 1455 of yacc.c  */
#line 250 "parser.y"
    {
                (yyval.inst) = GetPC();
            ;}
    break;

  case 51:

/* Line 1455 of yacc.c  */
#line 253 "parser.y"
    {
                (yyval.inst) = GetPC();
            ;}
    break;

  case 52:

/* Line 1455 of yacc.c  */
#line 257 "parser.y"
    {
                (yyval.nArgs) = 0;
            ;}
    break;

  case 53:

/* Line 1455 of yacc.c  */
#line 260 "parser.y"
    {
                (yyval.nArgs) = 1;
            ;}
    break;

  case 54:

/* Line 1455 of yacc.c  */
#line 263 "parser.y"
    {
                (yyval.nArgs) = (yyvsp[(1) - (3)].nArgs) + 1;
            ;}
    break;

  case 56:

/* Line 1455 of yacc.c  */
#line 268 "parser.y"
    {
                ADD_OP(OP_CONCAT);
            ;}
    break;

  case 57:

/* Line 1455 of yacc.c  */
#line 272 "parser.y"
    {
                    ADD_OP(OP_PUSH_ARRAY_SYM); ADD_SYM((yyvsp[(1) - (1)].sym)); ADD_IMMED(1);
                ;}
    break;

  case 58:

/* Line 1455 of yacc.c  */
#line 275 "parser.y"
    {
                    ADD_OP(OP_ARRAY_REF); ADD_IMMED((yyvsp[(3) - (4)].nArgs));
                ;}
    break;

  case 59:

/* Line 1455 of yacc.c  */
#line 279 "parser.y"
    {
                ADD_OP(OP_PUSH_ARRAY_SYM); ADD_SYM((yyvsp[(1) - (1)].sym)); ADD_IMMED(0);
            ;}
    break;

  case 60:

/* Line 1455 of yacc.c  */
#line 282 "parser.y"
    {
                ADD_OP(OP_ARRAY_REF); ADD_IMMED((yyvsp[(3) - (4)].nArgs));
            ;}
    break;

  case 61:

/* Line 1455 of yacc.c  */
#line 286 "parser.y"
    {
                (yyval.inst) = GetPC();
            ;}
    break;

  case 62:

/* Line 1455 of yacc.c  */
#line 290 "parser.y"
    {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[(1) - (1)].sym));
            ;}
    break;

  case 63:

/* Line 1455 of yacc.c  */
#line 293 "parser.y"
    {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[(1) - (1)].sym));
            ;}
    break;

  case 64:

/* Line 1455 of yacc.c  */
#line 296 "parser.y"
    {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[(1) - (1)].sym));
            ;}
    break;

  case 65:

/* Line 1455 of yacc.c  */
#line 299 "parser.y"
    {
                ADD_OP(OP_SUBR_CALL);
                ADD_SYM(PromoteToGlobal((yyvsp[(1) - (4)].sym))); ADD_IMMED((yyvsp[(3) - (4)].nArgs));
                ADD_OP(OP_FETCH_RET_VAL);
            ;}
    break;

  case 67:

/* Line 1455 of yacc.c  */
#line 305 "parser.y"
    {
               ADD_OP(OP_PUSH_ARG);
            ;}
    break;

  case 68:

/* Line 1455 of yacc.c  */
#line 308 "parser.y"
    {
               ADD_OP(OP_PUSH_ARG_COUNT);
            ;}
    break;

  case 69:

/* Line 1455 of yacc.c  */
#line 311 "parser.y"
    {
               ADD_OP(OP_PUSH_ARG_ARRAY);
            ;}
    break;

  case 70:

/* Line 1455 of yacc.c  */
#line 314 "parser.y"
    {
                ADD_OP(OP_ARRAY_REF); ADD_IMMED((yyvsp[(3) - (4)].nArgs));
            ;}
    break;

  case 71:

/* Line 1455 of yacc.c  */
#line 317 "parser.y"
    {
                ADD_OP(OP_ADD);
            ;}
    break;

  case 72:

/* Line 1455 of yacc.c  */
#line 320 "parser.y"
    {
                ADD_OP(OP_SUB);
            ;}
    break;

  case 73:

/* Line 1455 of yacc.c  */
#line 323 "parser.y"
    {
                ADD_OP(OP_MUL);
            ;}
    break;

  case 74:

/* Line 1455 of yacc.c  */
#line 326 "parser.y"
    {
                ADD_OP(OP_DIV);
            ;}
    break;

  case 75:

/* Line 1455 of yacc.c  */
#line 329 "parser.y"
    {
                ADD_OP(OP_MOD);
            ;}
    break;

  case 76:

/* Line 1455 of yacc.c  */
#line 332 "parser.y"
    {
                ADD_OP(OP_POWER);
            ;}
    break;

  case 77:

/* Line 1455 of yacc.c  */
#line 335 "parser.y"
    {
                ADD_OP(OP_NEGATE);
            ;}
    break;

  case 78:

/* Line 1455 of yacc.c  */
#line 338 "parser.y"
    {
                ADD_OP(OP_GT);
            ;}
    break;

  case 79:

/* Line 1455 of yacc.c  */
#line 341 "parser.y"
    {
                ADD_OP(OP_GE);
            ;}
    break;

  case 80:

/* Line 1455 of yacc.c  */
#line 344 "parser.y"
    {
                ADD_OP(OP_LT);
            ;}
    break;

  case 81:

/* Line 1455 of yacc.c  */
#line 347 "parser.y"
    {
                ADD_OP(OP_LE);
            ;}
    break;

  case 82:

/* Line 1455 of yacc.c  */
#line 350 "parser.y"
    {
                ADD_OP(OP_EQ);
            ;}
    break;

  case 83:

/* Line 1455 of yacc.c  */
#line 353 "parser.y"
    {
                ADD_OP(OP_NE);
            ;}
    break;

  case 84:

/* Line 1455 of yacc.c  */
#line 356 "parser.y"
    {
                ADD_OP(OP_BIT_AND);
            ;}
    break;

  case 85:

/* Line 1455 of yacc.c  */
#line 359 "parser.y"
    {
                ADD_OP(OP_BIT_OR);
            ;}
    break;

  case 86:

/* Line 1455 of yacc.c  */
#line 362 "parser.y"
    {
                ADD_OP(OP_AND); SET_BR_OFF((yyvsp[(2) - (3)].inst), GetPC());
            ;}
    break;

  case 87:

/* Line 1455 of yacc.c  */
#line 365 "parser.y"
    {
                ADD_OP(OP_OR); SET_BR_OFF((yyvsp[(2) - (3)].inst), GetPC());
            ;}
    break;

  case 88:

/* Line 1455 of yacc.c  */
#line 368 "parser.y"
    {
                ADD_OP(OP_NOT);
            ;}
    break;

  case 89:

/* Line 1455 of yacc.c  */
#line 371 "parser.y"
    {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[(2) - (2)].sym)); ADD_OP(OP_INCR);
                ADD_OP(OP_DUP); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[(2) - (2)].sym));
            ;}
    break;

  case 90:

/* Line 1455 of yacc.c  */
#line 375 "parser.y"
    {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[(1) - (2)].sym)); ADD_OP(OP_DUP);
                ADD_OP(OP_INCR); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[(1) - (2)].sym));
            ;}
    break;

  case 91:

/* Line 1455 of yacc.c  */
#line 379 "parser.y"
    {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[(2) - (2)].sym)); ADD_OP(OP_DECR);
                ADD_OP(OP_DUP); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[(2) - (2)].sym));
            ;}
    break;

  case 92:

/* Line 1455 of yacc.c  */
#line 383 "parser.y"
    {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[(1) - (2)].sym)); ADD_OP(OP_DUP);
                ADD_OP(OP_DECR); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[(1) - (2)].sym));
            ;}
    break;

  case 93:

/* Line 1455 of yacc.c  */
#line 387 "parser.y"
    {
                ADD_OP(OP_IN_ARRAY);
            ;}
    break;

  case 94:

/* Line 1455 of yacc.c  */
#line 391 "parser.y"
    {
            (yyval.inst) = GetPC(); StartLoopAddrList();
        ;}
    break;

  case 95:

/* Line 1455 of yacc.c  */
#line 395 "parser.y"
    {
            StartLoopAddrList(); (yyval.inst) = GetPC();
        ;}
    break;

  case 96:

/* Line 1455 of yacc.c  */
#line 399 "parser.y"
    {
            ADD_OP(OP_BRANCH); (yyval.inst) = GetPC(); ADD_BR_OFF(nullptr);
        ;}
    break;

  case 97:

/* Line 1455 of yacc.c  */
#line 403 "parser.y"
    {
            ADD_OP(OP_BRANCH_NEVER); (yyval.inst) = GetPC(); ADD_BR_OFF(nullptr);
        ;}
    break;

  case 98:

/* Line 1455 of yacc.c  */
#line 406 "parser.y"
    {
            ADD_OP(OP_BRANCH_FALSE); (yyval.inst) = GetPC(); ADD_BR_OFF(nullptr);
        ;}
    break;

  case 99:

/* Line 1455 of yacc.c  */
#line 410 "parser.y"
    {
            ADD_OP(OP_DUP); ADD_OP(OP_BRANCH_FALSE); (yyval.inst) = GetPC();
            ADD_BR_OFF(nullptr);
        ;}
    break;

  case 100:

/* Line 1455 of yacc.c  */
#line 415 "parser.y"
    {
            ADD_OP(OP_DUP); ADD_OP(OP_BRANCH_TRUE); (yyval.inst) = GetPC();
            ADD_BR_OFF(nullptr);
        ;}
    break;



/* Line 1455 of yacc.c  */
#line 2516 "C:/Users/Evan Teran/Desktop/build-nedit-ng-Desktop_Qt_5_12_0_MSVC2017_64bit-Default/Interpreter/parser.cpp"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
      {
	YYSIZE_T yysize = yysyntax_error (0, yystate, yychar);
	if (yymsg_alloc < yysize && yymsg_alloc < YYSTACK_ALLOC_MAXIMUM)
	  {
	    YYSIZE_T yyalloc = 2 * yysize;
	    if (! (yysize <= yyalloc && yyalloc <= YYSTACK_ALLOC_MAXIMUM))
	      yyalloc = YYSTACK_ALLOC_MAXIMUM;
	    if (yymsg != yymsgbuf)
	      YYSTACK_FREE (yymsg);
	    yymsg = (char *) YYSTACK_ALLOC (yyalloc);
	    if (yymsg)
	      yymsg_alloc = yyalloc;
	    else
	      {
		yymsg = yymsgbuf;
		yymsg_alloc = sizeof yymsgbuf;
	      }
	  }

	if (0 < yysize && yysize <= yymsg_alloc)
	  {
	    (void) yysyntax_error (yymsg, yystate, yychar);
	    yyerror (yymsg);
	  }
	else
	  {
	    yyerror (YY_("syntax error"));
	    if (yysize != 0)
	      goto yyexhaustedlab;
	  }
      }
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;


      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  *++yyvsp = yylval;


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined(yyoverflow) || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}



/* Line 1675 of yacc.c  */
#line 424 "parser.y"
 /* User Subroutines Section */


/*
** Parse a string and create a program from it (this is the parser entry point).
** The program created by this routine can be executed using ExecuteProgram.
** Returns program on success, or nullptr on failure.  If the command failed,
** the error message is returned in msg, and the length of the string up
** to where parsing failed in stoppedAt.
*/
Program *CompileMacro(const QString &expr, QString *msg, int *stoppedAt) {
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
    if (safe_ctype<isdigit>(InPtr->toLatin1()))  { /* number */

        QString value;
        auto p = std::back_inserter(value);

        *p++ = *InPtr++;
        while (InPtr != EndPtr && safe_ctype<isdigit>(InPtr->toLatin1())) {
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
    if (safe_ctype<isalpha>(InPtr->toLatin1()) || *InPtr == QLatin1Char('$')) {

        QString symName;
        auto p = std::back_inserter(symName);

        *p++ = *InPtr++;
        while ((InPtr != EndPtr) && (safe_ctype<isalnum>(InPtr->toLatin1()) || *InPtr==QLatin1Char('_'))) {
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

                    if (InPtr == EndPtr || (hexD = strchr(hexDigits, safe_ctype<tolower>(InPtr->toLatin1()))) == nullptr) {
                        *p++ = QLatin1Char('x');
                    } else {
                        hexValue = static_cast<int>(hexD - hexDigits);
                        InPtr++;

                        /* now do we have another digit? only accept one more */
                        if (InPtr != EndPtr && (hexD = strchr(hexDigits, safe_ctype<tolower>(InPtr->toLatin1()))) != nullptr){
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

