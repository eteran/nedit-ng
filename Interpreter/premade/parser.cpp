/* A Bison parser, made by GNU Bison 3.1.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018 Free Software Foundation, Inc.

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
#define YYBISON_VERSION "3.1"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* Copy the first part of user declarations.  */
#line 1 "parser.y" /* yacc.c:339  */

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


#line 102 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:339  */

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* In a future release of Bison, this section will be replaced
   by #include "parser.hpp".  */
#ifndef YY_YY_HOME_ETERAN_PROJECTS_NEDIT_NG_BUILD_INTERPRETER_PARSER_HPP_INCLUDED
# define YY_YY_HOME_ETERAN_PROJECTS_NEDIT_NG_BUILD_INTERPRETER_PARSER_HPP_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
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
    ADDEQ = 271,
    SUBEQ = 272,
    MULEQ = 273,
    DIVEQ = 274,
    MODEQ = 275,
    ANDEQ = 276,
    OREQ = 277,
    CONCAT = 278,
    OR = 279,
    AND = 280,
    GT = 281,
    GE = 282,
    LT = 283,
    LE = 284,
    EQ = 285,
    NE = 286,
    IN = 287,
    UNARY_MINUS = 288,
    NOT = 289,
    INCR = 290,
    DECR = 291,
    POW = 292
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 37 "parser.y" /* yacc.c:355  */

    Symbol *sym;
    Inst *inst;
    int nArgs;

#line 186 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:355  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_HOME_ETERAN_PROJECTS_NEDIT_NG_BUILD_INTERPRETER_PARSER_HPP_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 203 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:358  */

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
#else
typedef signed char yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef YY_ATTRIBUTE
# if (defined __GNUC__                                               \
      && (2 < __GNUC__ || (__GNUC__ == 2 && 96 <= __GNUC_MINOR__)))  \
     || defined __SUNPRO_C && 0x5110 <= __SUNPRO_C
#  define YY_ATTRIBUTE(Spec) __attribute__(Spec)
# else
#  define YY_ATTRIBUTE(Spec) /* empty */
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# define YY_ATTRIBUTE_PURE   YY_ATTRIBUTE ((__pure__))
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# define YY_ATTRIBUTE_UNUSED YY_ATTRIBUTE ((__unused__))
#endif

#if !defined _Noreturn \
     && (!defined __STDC_VERSION__ || __STDC_VERSION__ < 201112)
# if defined _MSC_VER && 1200 <= _MSC_VER
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn YY_ATTRIBUTE ((__noreturn__))
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && ! defined __ICC && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
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
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
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
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
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

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  4
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   543

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  55
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  22
/* YYNRULES -- Number of rules.  */
#define YYNRULES  102
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  208

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   292

#define YYTRANSLATE(YYX)                                                \
  ((unsigned) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
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
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
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

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "NUMBER", "STRING", "SYMBOL", "DELETE",
  "ARG_LOOKUP", "IF", "WHILE", "ELSE", "FOR", "BREAK", "CONTINUE",
  "RETURN", "IF_NO_ELSE", "'='", "ADDEQ", "SUBEQ", "MULEQ", "DIVEQ",
  "MODEQ", "ANDEQ", "OREQ", "CONCAT", "OR", "AND", "'|'", "'&'", "GT",
  "GE", "LT", "LE", "EQ", "NE", "IN", "'+'", "'-'", "'*'", "'/'", "'%'",
  "UNARY_MINUS", "NOT", "INCR", "DECR", "POW", "'['", "'('", "'{'", "'}'",
  "'\\n'", "')'", "';'", "']'", "','", "$accept", "program", "block",
  "stmts", "stmt", "$@1", "simpstmt", "evalsym", "comastmts", "arglist",
  "expr", "initarraylv", "arraylv", "arrayexpr", "numexpr", "while", "for",
  "else", "cond", "and", "or", "blank", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
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

#define YYPACT_NINF -66

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-66)))

#define YYTABLE_NINF -102

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      22,   -66,    11,   187,   -66,   159,    12,   -15,   -66,   -66,
     -11,    19,    82,    46,    77,   -66,   -66,   344,   -66,    34,
     520,    64,    61,    74,   339,   -66,   -66,   339,   -66,    81,
     339,   -66,   -66,   -66,   -66,    47,    89,   339,   339,    87,
     118,   339,   -66,   210,   412,    92,    93,    92,    95,   250,
     -66,   -66,   339,   339,   339,   339,   339,   339,   339,   339,
     339,     1,   339,   -38,   339,   339,   412,   109,    78,    78,
     -66,   -66,   339,     5,   -42,   -42,   -66,   -66,    17,    78,
     -66,   412,   -66,   -66,   339,   339,   339,   339,   339,   339,
     339,   339,   339,   339,   339,   339,   339,   339,   339,   339,
     339,   339,   339,   339,   -66,   316,    78,   339,   339,   339,
     339,   339,   339,   339,   -28,   110,   350,   -66,     3,   -66,
     339,     9,   -66,     2,   -66,   373,   -66,    78,   472,   490,
     251,   251,   251,   251,   251,   251,   251,   333,   333,   -42,
     -42,   -42,   -42,    20,   453,   433,    45,    51,   -66,   411,
     -66,   339,   339,    35,   339,   116,   260,   -66,   -66,   -66,
     117,   117,   339,   339,   339,   339,   339,   339,   339,   339,
     -66,   -66,   260,   121,   412,   122,   -66,   -66,   163,   -66,
     339,   339,   339,   339,   339,   339,   339,   339,   -66,   -66,
      35,   270,   -66,   -66,   -66,    52,   -66,   326,   260,   260,
     -66,    78,   -66,   -66,   -66,   260,    78,   -66
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     5,     0,     0,     1,    48,     0,     0,    94,    95,
       0,     0,     0,     0,     0,   101,   102,     2,     9,     0,
       0,     0,     0,     0,     0,    45,    47,    52,    59,     0,
      97,   101,   101,    62,    63,    64,    69,     0,     0,     0,
       0,     0,   101,     0,    55,    44,     0,    46,     0,     0,
      10,   101,     0,     0,     0,     0,     0,     0,     0,    52,
      97,    49,    22,     0,    53,    52,    98,     0,    18,    19,
      90,    92,    52,     0,    77,    88,    89,    91,     0,    21,
     101,    56,   100,    99,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    52,
       0,     0,    52,    52,     4,     0,    11,    23,    24,    25,
      26,    27,    28,    29,     0,     0,    48,    50,     0,    43,
       0,     0,   101,     0,    68,     0,    66,    20,    85,    84,
      78,    79,    80,    81,    82,    83,    93,    71,    72,    73,
      74,    75,    76,     0,    86,    87,     0,     0,     3,    58,
     101,     0,    97,     0,    54,    30,     0,    65,    67,    70,
      41,    42,     0,     0,     0,     0,     0,     0,     0,     0,
      39,    40,     0,     0,    61,     0,    51,   101,    12,     8,
      31,    32,    33,    34,    35,    36,    37,    38,    14,    16,
      49,     0,    96,   101,   101,     0,   101,     0,     0,     0,
     101,     7,   101,    13,    17,     0,     6,    15
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -66,   -66,   -65,   -34,    -3,   -66,   -60,   -66,   -14,   -22,
      59,    62,   -66,   -66,    58,   -66,   -66,   -66,   -55,   -66,
     -66,   -13
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     2,   178,    17,   179,   194,    19,    20,   118,    63,
      64,    21,    29,   173,    44,    22,    23,   193,    67,   100,
     101,     3
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      18,   117,    49,    98,    99,   115,   116,     6,    33,    34,
      35,     4,    36,   119,    50,   105,   120,    28,    68,    69,
      33,    34,    35,     1,    36,   149,   120,  -101,  -101,    79,
    -101,  -101,    30,  -101,  -101,  -101,  -101,   114,   106,    31,
       5,     6,    37,   121,    13,    14,    18,    38,    39,    40,
     123,    45,    41,   157,    37,   152,   120,   153,   124,    38,
      39,    40,   155,   120,    41,  -101,  -101,   127,   126,    32,
    -101,    43,  -101,   159,   120,    46,    48,   143,    13,    14,
     146,   147,    47,    62,    51,    33,    34,    35,    66,    36,
      70,    71,    76,   176,    72,    74,    75,   175,   160,   120,
      78,    81,    50,   200,   161,   120,   153,   188,    60,   156,
      59,   107,   108,   109,   110,   111,   112,   113,    66,    37,
      81,    61,    81,    77,    38,    39,    40,    65,    16,    41,
     117,   125,    42,   203,   204,    73,    81,   172,   -57,   102,
     207,   103,   128,   129,   130,   131,   132,   133,   134,   135,
     136,   137,   138,   139,   140,   141,   142,   197,   144,   145,
     122,   150,   -60,   -58,   191,    81,    81,    81,    81,    81,
      81,    81,   189,   192,   190,    24,   195,     0,     0,   154,
     198,   199,     0,   201,     0,     0,     0,   205,    18,   206,
       0,     0,     5,     6,    50,     7,     8,     0,     9,    10,
      11,    12,    25,    26,     0,   -57,    27,     0,     0,   174,
      66,     0,    81,    33,    34,    35,     0,    36,     0,     0,
       0,   180,   181,   182,   183,   184,   185,   186,   187,     0,
      13,    14,     0,     0,     0,    15,     0,    16,    81,    81,
      81,    81,    81,    81,    81,    81,     0,    37,     0,     0,
       0,     0,    38,    39,    40,     5,     6,    41,     7,     8,
      80,     9,    10,    11,    12,     5,     6,     0,     7,     8,
       0,     9,    10,    11,    12,     5,     6,     0,     7,     8,
       0,     9,    10,    11,    12,     0,     0,    93,    94,    95,
      96,    97,     0,    13,    14,     0,    98,    99,     0,   104,
      16,     0,     0,    13,    14,     0,     0,     0,   177,     0,
      16,     0,     0,    13,    14,     0,     0,     0,     0,   196,
      16,     5,     6,     0,     7,     8,     0,     9,    10,    11,
      12,     5,     6,     0,     7,     8,     0,     9,    10,    11,
      12,     0,    33,    34,    35,     0,    36,     0,     0,     5,
       6,     0,     7,     8,     0,     9,    10,    11,    12,    13,
      14,     0,     0,     0,     0,   148,    24,     0,     0,    13,
      14,    95,    96,    97,     0,   202,    37,     0,    98,    99,
       0,    38,    39,    40,     0,   151,    41,    13,    14,     0,
       0,     0,     0,    25,    26,     0,   -57,    27,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,     0,     0,     0,     0,    98,    99,
       0,     0,     0,     0,     0,     0,   158,   162,   163,   164,
     165,   166,   167,   168,   169,     0,     0,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,     0,   170,   171,     0,    98,    99,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,     0,     0,     0,     0,    98,    99,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,     0,     0,     0,     0,    98,    99,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,     0,     0,     0,     0,    98,    99,    86,
      87,    88,    89,    90,    91,    92,    93,    94,    95,    96,
      97,     0,     0,     0,     0,    98,    99,    52,    53,    54,
      55,    56,    57,    58
};

static const yytype_int16 yycheck[] =
{
       3,    61,    15,    45,    46,    60,     5,     6,     3,     4,
       5,     0,     7,    51,    17,    49,    54,     5,    31,    32,
       3,     4,     5,     1,     7,    53,    54,     5,     6,    42,
       8,     9,    47,    11,    12,    13,    14,    59,    51,    50,
       5,     6,    37,    65,    43,    44,    49,    42,    43,    44,
      72,     5,    47,    51,    37,    52,    54,    54,    53,    42,
      43,    44,    53,    54,    47,    43,    44,    80,    51,    50,
      48,    12,    50,    53,    54,    13,    14,    99,    43,    44,
     102,   103,     5,    24,    50,     3,     4,     5,    30,     7,
      43,    44,     5,   153,    47,    37,    38,   152,    53,    54,
      41,    43,   105,    51,    53,    54,    54,   172,    47,   122,
      46,    52,    53,    54,    55,    56,    57,    58,    60,    37,
      62,    47,    64,     5,    42,    43,    44,    46,    50,    47,
     190,    73,    50,   198,   199,    46,    78,   150,    46,    46,
     205,    46,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    93,    94,    95,    96,    97,    98,   191,   100,   101,
      51,    51,    46,    46,   177,   107,   108,   109,   110,   111,
     112,   113,    51,    10,    52,    16,   190,    -1,    -1,   120,
     193,   194,    -1,   196,    -1,    -1,    -1,   200,   191,   202,
      -1,    -1,     5,     6,   197,     8,     9,    -1,    11,    12,
      13,    14,    43,    44,    -1,    46,    47,    -1,    -1,   151,
     152,    -1,   154,     3,     4,     5,    -1,     7,    -1,    -1,
      -1,   162,   163,   164,   165,   166,   167,   168,   169,    -1,
      43,    44,    -1,    -1,    -1,    48,    -1,    50,   180,   181,
     182,   183,   184,   185,   186,   187,    -1,    37,    -1,    -1,
      -1,    -1,    42,    43,    44,     5,     6,    47,     8,     9,
      50,    11,    12,    13,    14,     5,     6,    -1,     8,     9,
      -1,    11,    12,    13,    14,     5,     6,    -1,     8,     9,
      -1,    11,    12,    13,    14,    -1,    -1,    36,    37,    38,
      39,    40,    -1,    43,    44,    -1,    45,    46,    -1,    49,
      50,    -1,    -1,    43,    44,    -1,    -1,    -1,    48,    -1,
      50,    -1,    -1,    43,    44,    -1,    -1,    -1,    -1,    49,
      50,     5,     6,    -1,     8,     9,    -1,    11,    12,    13,
      14,     5,     6,    -1,     8,     9,    -1,    11,    12,    13,
      14,    -1,     3,     4,     5,    -1,     7,    -1,    -1,     5,
       6,    -1,     8,     9,    -1,    11,    12,    13,    14,    43,
      44,    -1,    -1,    -1,    -1,    49,    16,    -1,    -1,    43,
      44,    38,    39,    40,    -1,    49,    37,    -1,    45,    46,
      -1,    42,    43,    44,    -1,    35,    47,    43,    44,    -1,
      -1,    -1,    -1,    43,    44,    -1,    46,    47,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    -1,    -1,    -1,    -1,    45,    46,
      -1,    -1,    -1,    -1,    -1,    -1,    53,    16,    17,    18,
      19,    20,    21,    22,    23,    -1,    -1,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    -1,    43,    44,    -1,    45,    46,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    -1,    -1,    -1,    -1,    45,    46,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    -1,    -1,    -1,    -1,    45,    46,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    -1,    -1,    -1,    -1,    45,    46,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    -1,    -1,    -1,    -1,    45,    46,    17,    18,    19,
      20,    21,    22,    23
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     1,    56,    76,     0,     5,     6,     8,     9,    11,
      12,    13,    14,    43,    44,    48,    50,    58,    59,    61,
      62,    66,    70,    71,    16,    43,    44,    47,     5,    67,
      47,    50,    50,     3,     4,     5,     7,    37,    42,    43,
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
      51,    35,    52,    54,    65,    53,    76,    51,    53,    53,
      53,    53,    16,    17,    18,    19,    20,    21,    22,    23,
      43,    44,    76,    68,    69,    73,    61,    48,    57,    59,
      65,    65,    65,    65,    65,    65,    65,    65,    57,    51,
      52,    76,    10,    72,    60,    63,    49,    58,    76,    76,
      51,    76,    49,    57,    57,    76,    76,    57
};

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

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
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


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
do                                                              \
  if (yychar == YYEMPTY)                                        \
    {                                                           \
      yychar = (Token);                                         \
      yylval = (Value);                                         \
      YYPOPSTACK (yylen);                                       \
      yystate = *yyssp;                                         \
      goto yybackup;                                            \
    }                                                           \
  else                                                          \
    {                                                           \
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;                                                  \
    }                                                           \
while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256



/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, int yyrule)
{
  unsigned long yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &(yyvsp[(yyi + 1) - (yynrhs)])
                                              );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

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
#ifndef YYINITDEPTH
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
static YYSIZE_T
yystrlen (const char *yystr)
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
static char *
yystpcpy (char *yydest, const char *yysrc)
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

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
    default: /* Avoid compiler warnings. */
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
{
  YYUSE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;


/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
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
  int yytoken = 0;
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

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
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
                  (unsigned long) yystacksize));

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
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex ();
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
      if (yytable_value_is_error (yyn))
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
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

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
     '$$ = $1'.

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
#line 72 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_RETURN_NO_VAL); return 0;
            }
#line 1490 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 3:
#line 75 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_RETURN_NO_VAL); return 0;
            }
#line 1498 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 4:
#line 78 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_RETURN_NO_VAL); return 0;
            }
#line 1506 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 5:
#line 81 "parser.y" /* yacc.c:1651  */
    {
                return 1;
            }
#line 1514 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 12:
#line 93 "parser.y" /* yacc.c:1651  */
    {
                SET_BR_OFF((yyvsp[-3].inst), GetPC());
            }
#line 1522 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 13:
#line 96 "parser.y" /* yacc.c:1651  */
    {
                SET_BR_OFF((yyvsp[-6].inst), ((yyvsp[-2].inst)+1)); SET_BR_OFF((yyvsp[-2].inst), GetPC());
            }
#line 1530 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 14:
#line 99 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_BRANCH); ADD_BR_OFF((yyvsp[-5].inst));
                SET_BR_OFF((yyvsp[-3].inst), GetPC()); FillLoopAddrs(GetPC(), (yyvsp[-5].inst));
            }
#line 1539 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 15:
#line 103 "parser.y" /* yacc.c:1651  */
    {
                FillLoopAddrs(GetPC()+2+((yyvsp[-3].inst)-((yyvsp[-5].inst)+1)), GetPC());
                SwapCode((yyvsp[-5].inst)+1, (yyvsp[-3].inst), GetPC());
                ADD_OP(OP_BRANCH); ADD_BR_OFF((yyvsp[-7].inst)); SET_BR_OFF((yyvsp[-5].inst), GetPC());
            }
#line 1549 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 16:
#line 108 "parser.y" /* yacc.c:1651  */
    {
                Symbol *iterSym = InstallIteratorSymbol();
                ADD_OP(OP_BEGIN_ARRAY_ITER); ADD_SYM(iterSym);
                ADD_OP(OP_ARRAY_ITER); ADD_SYM((yyvsp[-3].sym)); ADD_SYM(iterSym); ADD_BR_OFF(nullptr);
            }
#line 1559 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 17:
#line 113 "parser.y" /* yacc.c:1651  */
    {
                    ADD_OP(OP_BRANCH); ADD_BR_OFF((yyvsp[-4].inst)+2);
                    SET_BR_OFF((yyvsp[-4].inst)+5, GetPC());
                    FillLoopAddrs(GetPC(), (yyvsp[-4].inst)+2);
            }
#line 1569 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 18:
#line 118 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_BRANCH); ADD_BR_OFF(nullptr);
                if (AddBreakAddr(GetPC()-1)) {
                    yyerror("break outside loop"); YYERROR;
                }
            }
#line 1580 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 19:
#line 124 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_BRANCH); ADD_BR_OFF(nullptr);
                if (AddContinueAddr(GetPC()-1)) {
                    yyerror("continue outside loop"); YYERROR;
                }
            }
#line 1591 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 20:
#line 130 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_RETURN);
            }
#line 1599 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 21:
#line 133 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_RETURN_NO_VAL);
            }
#line 1607 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 22:
#line 137 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[-2].sym));
            }
#line 1615 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 23:
#line 140 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_ADD); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[-2].sym));
            }
#line 1623 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 24:
#line 143 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_SUB); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[-2].sym));
            }
#line 1631 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 25:
#line 146 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_MUL); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[-2].sym));
            }
#line 1639 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 26:
#line 149 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_DIV); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[-2].sym));
            }
#line 1647 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 27:
#line 152 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_MOD); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[-2].sym));
            }
#line 1655 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 28:
#line 155 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_BIT_AND); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[-2].sym));
            }
#line 1663 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 29:
#line 158 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_BIT_OR); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[-2].sym));
            }
#line 1671 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 30:
#line 161 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_ARRAY_DELETE); ADD_IMMED((yyvsp[-1].nArgs));
            }
#line 1679 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 31:
#line 164 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[-3].nArgs));
            }
#line 1687 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 32:
#line 167 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(1); ADD_IMMED((yyvsp[-3].nArgs));
                ADD_OP(OP_ADD);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[-3].nArgs));
            }
#line 1697 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 33:
#line 172 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(1); ADD_IMMED((yyvsp[-3].nArgs));
                ADD_OP(OP_SUB);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[-3].nArgs));
            }
#line 1707 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 34:
#line 177 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(1); ADD_IMMED((yyvsp[-3].nArgs));
                ADD_OP(OP_MUL);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[-3].nArgs));
            }
#line 1717 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 35:
#line 182 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(1); ADD_IMMED((yyvsp[-3].nArgs));
                ADD_OP(OP_DIV);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[-3].nArgs));
            }
#line 1727 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 36:
#line 187 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(1); ADD_IMMED((yyvsp[-3].nArgs));
                ADD_OP(OP_MOD);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[-3].nArgs));
            }
#line 1737 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 37:
#line 192 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(1); ADD_IMMED((yyvsp[-3].nArgs));
                ADD_OP(OP_BIT_AND);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[-3].nArgs));
            }
#line 1747 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 38:
#line 197 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(1); ADD_IMMED((yyvsp[-3].nArgs));
                ADD_OP(OP_BIT_OR);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[-3].nArgs));
            }
#line 1757 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 39:
#line 202 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(0); ADD_IMMED((yyvsp[-2].nArgs));
                ADD_OP(OP_INCR);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[-2].nArgs));
            }
#line 1767 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 40:
#line 207 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(0); ADD_IMMED((yyvsp[-2].nArgs));
                ADD_OP(OP_DECR);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[-2].nArgs));
            }
#line 1777 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 41:
#line 212 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(0); ADD_IMMED((yyvsp[-1].nArgs));
                ADD_OP(OP_INCR);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[-1].nArgs));
            }
#line 1787 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 42:
#line 217 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(0); ADD_IMMED((yyvsp[-1].nArgs));
                ADD_OP(OP_DECR);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[-1].nArgs));
            }
#line 1797 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 43:
#line 222 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_SUBR_CALL);
                ADD_SYM(PromoteToGlobal((yyvsp[-3].sym))); ADD_IMMED((yyvsp[-1].nArgs));
            }
#line 1806 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 44:
#line 226 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[0].sym)); ADD_OP(OP_INCR);
                ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[0].sym));
            }
#line 1815 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 45:
#line 230 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[-1].sym)); ADD_OP(OP_INCR);
                ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[-1].sym));
            }
#line 1824 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 46:
#line 234 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[0].sym)); ADD_OP(OP_DECR);
                ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[0].sym));
            }
#line 1833 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 47:
#line 238 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[-1].sym)); ADD_OP(OP_DECR);
                ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[-1].sym));
            }
#line 1842 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 48:
#line 243 "parser.y" /* yacc.c:1651  */
    {
                (yyval.sym) = (yyvsp[0].sym); ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[0].sym));
            }
#line 1850 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 49:
#line 247 "parser.y" /* yacc.c:1651  */
    {
                (yyval.inst) = GetPC();
            }
#line 1858 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 50:
#line 250 "parser.y" /* yacc.c:1651  */
    {
                (yyval.inst) = GetPC();
            }
#line 1866 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 51:
#line 253 "parser.y" /* yacc.c:1651  */
    {
                (yyval.inst) = GetPC();
            }
#line 1874 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 52:
#line 257 "parser.y" /* yacc.c:1651  */
    {
                (yyval.nArgs) = 0;
            }
#line 1882 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 53:
#line 260 "parser.y" /* yacc.c:1651  */
    {
                (yyval.nArgs) = 1;
            }
#line 1890 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 54:
#line 263 "parser.y" /* yacc.c:1651  */
    {
                (yyval.nArgs) = (yyvsp[-2].nArgs) + 1;
            }
#line 1898 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 56:
#line 268 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_CONCAT);
            }
#line 1906 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 57:
#line 272 "parser.y" /* yacc.c:1651  */
    {
                    ADD_OP(OP_PUSH_ARRAY_SYM); ADD_SYM((yyvsp[0].sym)); ADD_IMMED(1);
                }
#line 1914 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 58:
#line 275 "parser.y" /* yacc.c:1651  */
    {
                    ADD_OP(OP_ARRAY_REF); ADD_IMMED((yyvsp[-1].nArgs));
                }
#line 1922 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 59:
#line 279 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_PUSH_ARRAY_SYM); ADD_SYM((yyvsp[0].sym)); ADD_IMMED(0);
            }
#line 1930 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 60:
#line 282 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_ARRAY_REF); ADD_IMMED((yyvsp[-1].nArgs));
            }
#line 1938 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 61:
#line 286 "parser.y" /* yacc.c:1651  */
    {
                (yyval.inst) = GetPC();
            }
#line 1946 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 62:
#line 290 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[0].sym));
            }
#line 1954 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 63:
#line 293 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[0].sym));
            }
#line 1962 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 64:
#line 296 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[0].sym));
            }
#line 1970 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 65:
#line 299 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_SUBR_CALL);
                ADD_SYM(PromoteToGlobal((yyvsp[-3].sym))); ADD_IMMED((yyvsp[-1].nArgs));
                ADD_OP(OP_FETCH_RET_VAL);
            }
#line 1980 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 67:
#line 305 "parser.y" /* yacc.c:1651  */
    {
               ADD_OP(OP_PUSH_ARG);
            }
#line 1988 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 68:
#line 308 "parser.y" /* yacc.c:1651  */
    {
               ADD_OP(OP_PUSH_ARG_COUNT);
            }
#line 1996 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 69:
#line 311 "parser.y" /* yacc.c:1651  */
    {
               ADD_OP(OP_PUSH_ARG_ARRAY);
            }
#line 2004 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 70:
#line 314 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_ARRAY_REF); ADD_IMMED((yyvsp[-1].nArgs));
            }
#line 2012 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 71:
#line 317 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_ADD);
            }
#line 2020 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 72:
#line 320 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_SUB);
            }
#line 2028 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 73:
#line 323 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_MUL);
            }
#line 2036 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 74:
#line 326 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_DIV);
            }
#line 2044 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 75:
#line 329 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_MOD);
            }
#line 2052 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 76:
#line 332 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_POWER);
            }
#line 2060 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 77:
#line 335 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_NEGATE);
            }
#line 2068 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 78:
#line 338 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_GT);
            }
#line 2076 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 79:
#line 341 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_GE);
            }
#line 2084 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 80:
#line 344 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_LT);
            }
#line 2092 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 81:
#line 347 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_LE);
            }
#line 2100 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 82:
#line 350 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_EQ);
            }
#line 2108 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 83:
#line 353 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_NE);
            }
#line 2116 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 84:
#line 356 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_BIT_AND);
            }
#line 2124 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 85:
#line 359 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_BIT_OR);
            }
#line 2132 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 86:
#line 362 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_AND); SET_BR_OFF((yyvsp[-1].inst), GetPC());
            }
#line 2140 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 87:
#line 365 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_OR); SET_BR_OFF((yyvsp[-1].inst), GetPC());
            }
#line 2148 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 88:
#line 368 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_NOT);
            }
#line 2156 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 89:
#line 371 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[0].sym)); ADD_OP(OP_INCR);
                ADD_OP(OP_DUP); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[0].sym));
            }
#line 2165 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 90:
#line 375 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[-1].sym)); ADD_OP(OP_DUP);
                ADD_OP(OP_INCR); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[-1].sym));
            }
#line 2174 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 91:
#line 379 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[0].sym)); ADD_OP(OP_DECR);
                ADD_OP(OP_DUP); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[0].sym));
            }
#line 2183 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 92:
#line 383 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[-1].sym)); ADD_OP(OP_DUP);
                ADD_OP(OP_DECR); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[-1].sym));
            }
#line 2192 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 93:
#line 387 "parser.y" /* yacc.c:1651  */
    {
                ADD_OP(OP_IN_ARRAY);
            }
#line 2200 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 94:
#line 391 "parser.y" /* yacc.c:1651  */
    {
            (yyval.inst) = GetPC(); StartLoopAddrList();
        }
#line 2208 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 95:
#line 395 "parser.y" /* yacc.c:1651  */
    {
            StartLoopAddrList(); (yyval.inst) = GetPC();
        }
#line 2216 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 96:
#line 399 "parser.y" /* yacc.c:1651  */
    {
            ADD_OP(OP_BRANCH); (yyval.inst) = GetPC(); ADD_BR_OFF(nullptr);
        }
#line 2224 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 97:
#line 403 "parser.y" /* yacc.c:1651  */
    {
            ADD_OP(OP_BRANCH_NEVER); (yyval.inst) = GetPC(); ADD_BR_OFF(nullptr);
        }
#line 2232 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 98:
#line 406 "parser.y" /* yacc.c:1651  */
    {
            ADD_OP(OP_BRANCH_FALSE); (yyval.inst) = GetPC(); ADD_BR_OFF(nullptr);
        }
#line 2240 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 99:
#line 410 "parser.y" /* yacc.c:1651  */
    {
            ADD_OP(OP_DUP); ADD_OP(OP_BRANCH_FALSE); (yyval.inst) = GetPC();
            ADD_BR_OFF(nullptr);
        }
#line 2249 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;

  case 100:
#line 415 "parser.y" /* yacc.c:1651  */
    {
            ADD_OP(OP_DUP); ADD_OP(OP_BRANCH_TRUE); (yyval.inst) = GetPC();
            ADD_BR_OFF(nullptr);
        }
#line 2258 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
    break;


#line 2262 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp" /* yacc.c:1651  */
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
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

  /* Do not reclaim the symbols of the rule whose action triggered
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
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
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

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


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

#if !defined yyoverflow || YYERROR_VERBOSE
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
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
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
  return yyresult;
}
#line 424 "parser.y" /* yacc.c:1910  */
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
