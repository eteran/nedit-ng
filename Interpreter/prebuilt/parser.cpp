/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* First part of user prologue.  */
#line 1 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"

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


#line 109 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "parser.hpp"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_NUMBER = 3,                     /* NUMBER  */
  YYSYMBOL_STRING = 4,                     /* STRING  */
  YYSYMBOL_SYMBOL = 5,                     /* SYMBOL  */
  YYSYMBOL_DELETE = 6,                     /* DELETE  */
  YYSYMBOL_ARG_LOOKUP = 7,                 /* ARG_LOOKUP  */
  YYSYMBOL_IF = 8,                         /* IF  */
  YYSYMBOL_WHILE = 9,                      /* WHILE  */
  YYSYMBOL_ELSE = 10,                      /* ELSE  */
  YYSYMBOL_FOR = 11,                       /* FOR  */
  YYSYMBOL_BREAK = 12,                     /* BREAK  */
  YYSYMBOL_CONTINUE = 13,                  /* CONTINUE  */
  YYSYMBOL_RETURN = 14,                    /* RETURN  */
  YYSYMBOL_IF_NO_ELSE = 15,                /* IF_NO_ELSE  */
  YYSYMBOL_16_ = 16,                       /* '='  */
  YYSYMBOL_ADDEQ = 17,                     /* ADDEQ  */
  YYSYMBOL_SUBEQ = 18,                     /* SUBEQ  */
  YYSYMBOL_MULEQ = 19,                     /* MULEQ  */
  YYSYMBOL_DIVEQ = 20,                     /* DIVEQ  */
  YYSYMBOL_MODEQ = 21,                     /* MODEQ  */
  YYSYMBOL_ANDEQ = 22,                     /* ANDEQ  */
  YYSYMBOL_OREQ = 23,                      /* OREQ  */
  YYSYMBOL_CONCAT = 24,                    /* CONCAT  */
  YYSYMBOL_OR = 25,                        /* OR  */
  YYSYMBOL_AND = 26,                       /* AND  */
  YYSYMBOL_27_ = 27,                       /* '|'  */
  YYSYMBOL_28_ = 28,                       /* '&'  */
  YYSYMBOL_GT = 29,                        /* GT  */
  YYSYMBOL_GE = 30,                        /* GE  */
  YYSYMBOL_LT = 31,                        /* LT  */
  YYSYMBOL_LE = 32,                        /* LE  */
  YYSYMBOL_EQ = 33,                        /* EQ  */
  YYSYMBOL_NE = 34,                        /* NE  */
  YYSYMBOL_IN = 35,                        /* IN  */
  YYSYMBOL_36_ = 36,                       /* '+'  */
  YYSYMBOL_37_ = 37,                       /* '-'  */
  YYSYMBOL_38_ = 38,                       /* '*'  */
  YYSYMBOL_39_ = 39,                       /* '/'  */
  YYSYMBOL_40_ = 40,                       /* '%'  */
  YYSYMBOL_UNARY_MINUS = 41,               /* UNARY_MINUS  */
  YYSYMBOL_NOT = 42,                       /* NOT  */
  YYSYMBOL_INCR = 43,                      /* INCR  */
  YYSYMBOL_DECR = 44,                      /* DECR  */
  YYSYMBOL_POW = 45,                       /* POW  */
  YYSYMBOL_46_ = 46,                       /* '['  */
  YYSYMBOL_47_ = 47,                       /* '('  */
  YYSYMBOL_48_ = 48,                       /* '{'  */
  YYSYMBOL_49_ = 49,                       /* '}'  */
  YYSYMBOL_50_n_ = 50,                     /* '\n'  */
  YYSYMBOL_51_ = 51,                       /* ')'  */
  YYSYMBOL_52_ = 52,                       /* ';'  */
  YYSYMBOL_53_ = 53,                       /* ']'  */
  YYSYMBOL_54_ = 54,                       /* ','  */
  YYSYMBOL_YYACCEPT = 55,                  /* $accept  */
  YYSYMBOL_program = 56,                   /* program  */
  YYSYMBOL_block = 57,                     /* block  */
  YYSYMBOL_stmts = 58,                     /* stmts  */
  YYSYMBOL_stmt = 59,                      /* stmt  */
  YYSYMBOL_60_1 = 60,                      /* $@1  */
  YYSYMBOL_simpstmt = 61,                  /* simpstmt  */
  YYSYMBOL_evalsym = 62,                   /* evalsym  */
  YYSYMBOL_comastmts = 63,                 /* comastmts  */
  YYSYMBOL_arglist = 64,                   /* arglist  */
  YYSYMBOL_expr = 65,                      /* expr  */
  YYSYMBOL_initarraylv = 66,               /* initarraylv  */
  YYSYMBOL_arraylv = 67,                   /* arraylv  */
  YYSYMBOL_arrayexpr = 68,                 /* arrayexpr  */
  YYSYMBOL_numexpr = 69,                   /* numexpr  */
  YYSYMBOL_while = 70,                     /* while  */
  YYSYMBOL_for = 71,                       /* for  */
  YYSYMBOL_else = 72,                      /* else  */
  YYSYMBOL_cond = 73,                      /* cond  */
  YYSYMBOL_and = 74,                       /* and  */
  YYSYMBOL_or = 75,                        /* or  */
  YYSYMBOL_blank = 76                      /* blank  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_uint8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

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


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
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

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

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
#endif /* !defined yyoverflow */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
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
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
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

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   292


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
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
static const yytype_int16 yyrline[] =
{
       0,    74,    74,    77,    80,    83,    87,    88,    89,    91,
      92,    94,    95,    98,   101,   105,   110,   110,   120,   126,
     132,   135,   139,   142,   145,   148,   151,   154,   157,   160,
     163,   166,   169,   174,   179,   184,   189,   194,   199,   204,
     209,   214,   219,   224,   228,   232,   236,   240,   245,   249,
     252,   255,   259,   262,   265,   269,   270,   274,   277,   281,
     284,   288,   292,   295,   298,   301,   306,   307,   310,   313,
     316,   319,   322,   325,   328,   331,   334,   337,   340,   343,
     346,   349,   352,   355,   358,   361,   364,   367,   370,   373,
     377,   381,   385,   389,   393,   397,   401,   405,   408,   412,
     417,   422,   423
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "NUMBER", "STRING",
  "SYMBOL", "DELETE", "ARG_LOOKUP", "IF", "WHILE", "ELSE", "FOR", "BREAK",
  "CONTINUE", "RETURN", "IF_NO_ELSE", "'='", "ADDEQ", "SUBEQ", "MULEQ",
  "DIVEQ", "MODEQ", "ANDEQ", "OREQ", "CONCAT", "OR", "AND", "'|'", "'&'",
  "GT", "GE", "LT", "LE", "EQ", "NE", "IN", "'+'", "'-'", "'*'", "'/'",
  "'%'", "UNARY_MINUS", "NOT", "INCR", "DECR", "POW", "'['", "'('", "'{'",
  "'}'", "'\\n'", "')'", "';'", "']'", "','", "$accept", "program",
  "block", "stmts", "stmt", "$@1", "simpstmt", "evalsym", "comastmts",
  "arglist", "expr", "initarraylv", "arraylv", "arrayexpr", "numexpr",
  "while", "for", "else", "cond", "and", "or", "blank", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-66)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-102)

#define yytable_value_is_error(Yyn) \
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
static const yytype_int8 yydefact[] =
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
static const yytype_uint8 yydefgoto[] =
{
       0,     2,   178,    17,   179,   194,    19,    20,   118,    63,
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

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
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

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
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

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
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


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
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

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF


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




# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
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
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)]);
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
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
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






/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep)
{
  YY_USE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/* Lookahead token kind.  */
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
    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */

  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


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

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
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
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
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
| yyreduce -- do a reduction.  |
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
  case 2: /* program: blank stmts  */
#line 74 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                        {
                ADD_OP(OP_RETURN_NO_VAL); return 0;
            }
#line 1374 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 3: /* program: blank '{' blank stmts '}'  */
#line 77 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                        {
                ADD_OP(OP_RETURN_NO_VAL); return 0;
            }
#line 1382 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 4: /* program: blank '{' blank '}'  */
#line 80 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                  {
                ADD_OP(OP_RETURN_NO_VAL); return 0;
            }
#line 1390 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 5: /* program: error  */
#line 83 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                    {
                return 1;
            }
#line 1398 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 12: /* stmt: IF '(' cond ')' blank block  */
#line 95 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                                           {
                SET_BR_OFF((yyvsp[-3].inst), GetPC());
            }
#line 1406 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 13: /* stmt: IF '(' cond ')' blank block else blank block  */
#line 98 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                                                      {
                SET_BR_OFF((yyvsp[-6].inst), ((yyvsp[-2].inst)+1)); SET_BR_OFF((yyvsp[-2].inst), GetPC());
            }
#line 1414 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 14: /* stmt: while '(' cond ')' blank block  */
#line 101 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                             {
                ADD_OP(OP_BRANCH); ADD_BR_OFF((yyvsp[-5].inst));
                SET_BR_OFF((yyvsp[-3].inst), GetPC()); FillLoopAddrs(GetPC(), (yyvsp[-5].inst));
            }
#line 1423 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 15: /* stmt: for '(' comastmts ';' cond ';' comastmts ')' blank block  */
#line 105 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                                                       {
                FillLoopAddrs(GetPC()+2+((yyvsp[-3].inst)-((yyvsp[-5].inst)+1)), GetPC());
                SwapCode((yyvsp[-5].inst)+1, (yyvsp[-3].inst), GetPC());
                ADD_OP(OP_BRANCH); ADD_BR_OFF((yyvsp[-7].inst)); SET_BR_OFF((yyvsp[-5].inst), GetPC());
            }
#line 1433 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 16: /* $@1: %empty  */
#line 110 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                              {
                Symbol *iterSym = InstallIteratorSymbol();
                ADD_OP(OP_BEGIN_ARRAY_ITER); ADD_SYM(iterSym);
                ADD_OP(OP_ARRAY_ITER); ADD_SYM((yyvsp[-3].sym)); ADD_SYM(iterSym); ADD_BR_OFF(nullptr);
            }
#line 1443 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 17: /* stmt: for '(' SYMBOL IN arrayexpr ')' $@1 blank block  */
#line 115 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                            {
                    ADD_OP(OP_BRANCH); ADD_BR_OFF((yyvsp[-4].inst)+2);
                    SET_BR_OFF((yyvsp[-4].inst)+5, GetPC());
                    FillLoopAddrs(GetPC(), (yyvsp[-4].inst)+2);
            }
#line 1453 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 18: /* stmt: BREAK '\n' blank  */
#line 120 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                               {
                ADD_OP(OP_BRANCH); ADD_BR_OFF(nullptr);
                if (AddBreakAddr(GetPC()-1)) {
                    yyerror("break outside loop"); YYERROR;
                }
            }
#line 1464 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 19: /* stmt: CONTINUE '\n' blank  */
#line 126 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                  {
                ADD_OP(OP_BRANCH); ADD_BR_OFF(nullptr);
                if (AddContinueAddr(GetPC()-1)) {
                    yyerror("continue outside loop"); YYERROR;
                }
            }
#line 1475 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 20: /* stmt: RETURN expr '\n' blank  */
#line 132 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                     {
                ADD_OP(OP_RETURN);
            }
#line 1483 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 21: /* stmt: RETURN '\n' blank  */
#line 135 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                {
                ADD_OP(OP_RETURN_NO_VAL);
            }
#line 1491 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 22: /* simpstmt: SYMBOL '=' expr  */
#line 139 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                            {
                ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[-2].sym));
            }
#line 1499 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 23: /* simpstmt: evalsym ADDEQ expr  */
#line 142 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                 {
                ADD_OP(OP_ADD); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[-2].sym));
            }
#line 1507 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 24: /* simpstmt: evalsym SUBEQ expr  */
#line 145 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                 {
                ADD_OP(OP_SUB); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[-2].sym));
            }
#line 1515 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 25: /* simpstmt: evalsym MULEQ expr  */
#line 148 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                 {
                ADD_OP(OP_MUL); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[-2].sym));
            }
#line 1523 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 26: /* simpstmt: evalsym DIVEQ expr  */
#line 151 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                 {
                ADD_OP(OP_DIV); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[-2].sym));
            }
#line 1531 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 27: /* simpstmt: evalsym MODEQ expr  */
#line 154 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                 {
                ADD_OP(OP_MOD); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[-2].sym));
            }
#line 1539 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 28: /* simpstmt: evalsym ANDEQ expr  */
#line 157 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                 {
                ADD_OP(OP_BIT_AND); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[-2].sym));
            }
#line 1547 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 29: /* simpstmt: evalsym OREQ expr  */
#line 160 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                {
                ADD_OP(OP_BIT_OR); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[-2].sym));
            }
#line 1555 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 30: /* simpstmt: DELETE arraylv '[' arglist ']'  */
#line 163 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                             {
                ADD_OP(OP_ARRAY_DELETE); ADD_IMMED((yyvsp[-1].nArgs));
            }
#line 1563 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 31: /* simpstmt: initarraylv '[' arglist ']' '=' expr  */
#line 166 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                                   {
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[-3].nArgs));
            }
#line 1571 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 32: /* simpstmt: initarraylv '[' arglist ']' ADDEQ expr  */
#line 169 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                                     {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(1); ADD_IMMED((yyvsp[-3].nArgs));
                ADD_OP(OP_ADD);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[-3].nArgs));
            }
#line 1581 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 33: /* simpstmt: initarraylv '[' arglist ']' SUBEQ expr  */
#line 174 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                                     {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(1); ADD_IMMED((yyvsp[-3].nArgs));
                ADD_OP(OP_SUB);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[-3].nArgs));
            }
#line 1591 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 34: /* simpstmt: initarraylv '[' arglist ']' MULEQ expr  */
#line 179 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                                     {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(1); ADD_IMMED((yyvsp[-3].nArgs));
                ADD_OP(OP_MUL);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[-3].nArgs));
            }
#line 1601 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 35: /* simpstmt: initarraylv '[' arglist ']' DIVEQ expr  */
#line 184 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                                     {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(1); ADD_IMMED((yyvsp[-3].nArgs));
                ADD_OP(OP_DIV);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[-3].nArgs));
            }
#line 1611 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 36: /* simpstmt: initarraylv '[' arglist ']' MODEQ expr  */
#line 189 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                                     {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(1); ADD_IMMED((yyvsp[-3].nArgs));
                ADD_OP(OP_MOD);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[-3].nArgs));
            }
#line 1621 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 37: /* simpstmt: initarraylv '[' arglist ']' ANDEQ expr  */
#line 194 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                                     {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(1); ADD_IMMED((yyvsp[-3].nArgs));
                ADD_OP(OP_BIT_AND);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[-3].nArgs));
            }
#line 1631 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 38: /* simpstmt: initarraylv '[' arglist ']' OREQ expr  */
#line 199 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                                    {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(1); ADD_IMMED((yyvsp[-3].nArgs));
                ADD_OP(OP_BIT_OR);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[-3].nArgs));
            }
#line 1641 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 39: /* simpstmt: initarraylv '[' arglist ']' INCR  */
#line 204 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                               {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(0); ADD_IMMED((yyvsp[-2].nArgs));
                ADD_OP(OP_INCR);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[-2].nArgs));
            }
#line 1651 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 40: /* simpstmt: initarraylv '[' arglist ']' DECR  */
#line 209 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                               {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(0); ADD_IMMED((yyvsp[-2].nArgs));
                ADD_OP(OP_DECR);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[-2].nArgs));
            }
#line 1661 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 41: /* simpstmt: INCR initarraylv '[' arglist ']'  */
#line 214 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                               {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(0); ADD_IMMED((yyvsp[-1].nArgs));
                ADD_OP(OP_INCR);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[-1].nArgs));
            }
#line 1671 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 42: /* simpstmt: DECR initarraylv '[' arglist ']'  */
#line 219 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                               {
                ADD_OP(OP_ARRAY_REF_ASSIGN_SETUP); ADD_IMMED(0); ADD_IMMED((yyvsp[-1].nArgs));
                ADD_OP(OP_DECR);
                ADD_OP(OP_ARRAY_ASSIGN); ADD_IMMED((yyvsp[-1].nArgs));
            }
#line 1681 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 43: /* simpstmt: SYMBOL '(' arglist ')'  */
#line 224 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                     {
                ADD_OP(OP_SUBR_CALL);
                ADD_SYM(PromoteToGlobal((yyvsp[-3].sym))); ADD_IMMED((yyvsp[-1].nArgs));
            }
#line 1690 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 44: /* simpstmt: INCR SYMBOL  */
#line 228 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                          {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[0].sym)); ADD_OP(OP_INCR);
                ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[0].sym));
            }
#line 1699 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 45: /* simpstmt: SYMBOL INCR  */
#line 232 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                          {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[-1].sym)); ADD_OP(OP_INCR);
                ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[-1].sym));
            }
#line 1708 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 46: /* simpstmt: DECR SYMBOL  */
#line 236 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                          {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[0].sym)); ADD_OP(OP_DECR);
                ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[0].sym));
            }
#line 1717 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 47: /* simpstmt: SYMBOL DECR  */
#line 240 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                          {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[-1].sym)); ADD_OP(OP_DECR);
                ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[-1].sym));
            }
#line 1726 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 48: /* evalsym: SYMBOL  */
#line 245 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                   {
                (yyval.sym) = (yyvsp[0].sym); ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[0].sym));
            }
#line 1734 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 49: /* comastmts: %empty  */
#line 249 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                          {
                (yyval.inst) = GetPC();
            }
#line 1742 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 50: /* comastmts: simpstmt  */
#line 252 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                       {
                (yyval.inst) = GetPC();
            }
#line 1750 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 51: /* comastmts: comastmts ',' simpstmt  */
#line 255 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                     {
                (yyval.inst) = GetPC();
            }
#line 1758 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 52: /* arglist: %empty  */
#line 259 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                          {
                (yyval.nArgs) = 0;
            }
#line 1766 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 53: /* arglist: expr  */
#line 262 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                   {
                (yyval.nArgs) = 1;
            }
#line 1774 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 54: /* arglist: arglist ',' expr  */
#line 265 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                               {
                (yyval.nArgs) = (yyvsp[-2].nArgs) + 1;
            }
#line 1782 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 56: /* expr: expr numexpr  */
#line 270 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                        {
                ADD_OP(OP_CONCAT);
            }
#line 1790 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 57: /* initarraylv: SYMBOL  */
#line 274 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                       {
                    ADD_OP(OP_PUSH_ARRAY_SYM); ADD_SYM((yyvsp[0].sym)); ADD_IMMED(1);
                }
#line 1798 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 58: /* initarraylv: initarraylv '[' arglist ']'  */
#line 277 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                              {
                    ADD_OP(OP_ARRAY_REF); ADD_IMMED((yyvsp[-1].nArgs));
                }
#line 1806 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 59: /* arraylv: SYMBOL  */
#line 281 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                   {
                ADD_OP(OP_PUSH_ARRAY_SYM); ADD_SYM((yyvsp[0].sym)); ADD_IMMED(0);
            }
#line 1814 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 60: /* arraylv: arraylv '[' arglist ']'  */
#line 284 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                      {
                ADD_OP(OP_ARRAY_REF); ADD_IMMED((yyvsp[-1].nArgs));
            }
#line 1822 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 61: /* arrayexpr: numexpr  */
#line 288 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                    {
                (yyval.inst) = GetPC();
            }
#line 1830 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 62: /* numexpr: NUMBER  */
#line 292 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                   {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[0].sym));
            }
#line 1838 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 63: /* numexpr: STRING  */
#line 295 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                     {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[0].sym));
            }
#line 1846 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 64: /* numexpr: SYMBOL  */
#line 298 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                     {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[0].sym));
            }
#line 1854 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 65: /* numexpr: SYMBOL '(' arglist ')'  */
#line 301 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                     {
                ADD_OP(OP_SUBR_CALL);
                ADD_SYM(PromoteToGlobal((yyvsp[-3].sym))); ADD_IMMED((yyvsp[-1].nArgs));
                ADD_OP(OP_FETCH_RET_VAL);
            }
#line 1864 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 67: /* numexpr: ARG_LOOKUP '[' numexpr ']'  */
#line 307 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                         {
               ADD_OP(OP_PUSH_ARG);
            }
#line 1872 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 68: /* numexpr: ARG_LOOKUP '[' ']'  */
#line 310 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                 {
               ADD_OP(OP_PUSH_ARG_COUNT);
            }
#line 1880 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 69: /* numexpr: ARG_LOOKUP  */
#line 313 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                         {
               ADD_OP(OP_PUSH_ARG_ARRAY);
            }
#line 1888 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 70: /* numexpr: numexpr '[' arglist ']'  */
#line 316 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                      {
                ADD_OP(OP_ARRAY_REF); ADD_IMMED((yyvsp[-1].nArgs));
            }
#line 1896 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 71: /* numexpr: numexpr '+' numexpr  */
#line 319 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                  {
                ADD_OP(OP_ADD);
            }
#line 1904 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 72: /* numexpr: numexpr '-' numexpr  */
#line 322 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                  {
                ADD_OP(OP_SUB);
            }
#line 1912 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 73: /* numexpr: numexpr '*' numexpr  */
#line 325 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                  {
                ADD_OP(OP_MUL);
            }
#line 1920 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 74: /* numexpr: numexpr '/' numexpr  */
#line 328 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                  {
                ADD_OP(OP_DIV);
            }
#line 1928 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 75: /* numexpr: numexpr '%' numexpr  */
#line 331 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                  {
                ADD_OP(OP_MOD);
            }
#line 1936 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 76: /* numexpr: numexpr POW numexpr  */
#line 334 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                  {
                ADD_OP(OP_POWER);
            }
#line 1944 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 77: /* numexpr: '-' numexpr  */
#line 337 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                             {
                ADD_OP(OP_NEGATE);
            }
#line 1952 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 78: /* numexpr: numexpr GT numexpr  */
#line 340 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                  {
                ADD_OP(OP_GT);
            }
#line 1960 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 79: /* numexpr: numexpr GE numexpr  */
#line 343 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                  {
                ADD_OP(OP_GE);
            }
#line 1968 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 80: /* numexpr: numexpr LT numexpr  */
#line 346 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                  {
                ADD_OP(OP_LT);
            }
#line 1976 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 81: /* numexpr: numexpr LE numexpr  */
#line 349 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                  {
                ADD_OP(OP_LE);
            }
#line 1984 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 82: /* numexpr: numexpr EQ numexpr  */
#line 352 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                  {
                ADD_OP(OP_EQ);
            }
#line 1992 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 83: /* numexpr: numexpr NE numexpr  */
#line 355 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                  {
                ADD_OP(OP_NE);
            }
#line 2000 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 84: /* numexpr: numexpr '&' numexpr  */
#line 358 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                  {
                ADD_OP(OP_BIT_AND);
            }
#line 2008 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 85: /* numexpr: numexpr '|' numexpr  */
#line 361 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                   {
                ADD_OP(OP_BIT_OR);
            }
#line 2016 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 86: /* numexpr: numexpr and numexpr  */
#line 364 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                            {
                ADD_OP(OP_AND); SET_BR_OFF((yyvsp[-1].inst), GetPC());
            }
#line 2024 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 87: /* numexpr: numexpr or numexpr  */
#line 367 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                          {
                ADD_OP(OP_OR); SET_BR_OFF((yyvsp[-1].inst), GetPC());
            }
#line 2032 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 88: /* numexpr: NOT numexpr  */
#line 370 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                          {
                ADD_OP(OP_NOT);
            }
#line 2040 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 89: /* numexpr: INCR SYMBOL  */
#line 373 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                          {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[0].sym)); ADD_OP(OP_INCR);
                ADD_OP(OP_DUP); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[0].sym));
            }
#line 2049 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 90: /* numexpr: SYMBOL INCR  */
#line 377 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                          {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[-1].sym)); ADD_OP(OP_DUP);
                ADD_OP(OP_INCR); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[-1].sym));
            }
#line 2058 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 91: /* numexpr: DECR SYMBOL  */
#line 381 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                          {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[0].sym)); ADD_OP(OP_DECR);
                ADD_OP(OP_DUP); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[0].sym));
            }
#line 2067 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 92: /* numexpr: SYMBOL DECR  */
#line 385 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                          {
                ADD_OP(OP_PUSH_SYM); ADD_SYM((yyvsp[-1].sym)); ADD_OP(OP_DUP);
                ADD_OP(OP_DECR); ADD_OP(OP_ASSIGN); ADD_SYM((yyvsp[-1].sym));
            }
#line 2076 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 93: /* numexpr: numexpr IN numexpr  */
#line 389 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                                 {
                ADD_OP(OP_IN_ARRAY);
            }
#line 2084 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 94: /* while: WHILE  */
#line 393 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
              {
            (yyval.inst) = GetPC(); StartLoopAddrList();
        }
#line 2092 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 95: /* for: FOR  */
#line 397 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
            {
            StartLoopAddrList(); (yyval.inst) = GetPC();
        }
#line 2100 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 96: /* else: ELSE  */
#line 401 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
             {
            ADD_OP(OP_BRANCH); (yyval.inst) = GetPC(); ADD_BR_OFF(nullptr);
        }
#line 2108 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 97: /* cond: %empty  */
#line 405 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                      {
            ADD_OP(OP_BRANCH_NEVER); (yyval.inst) = GetPC(); ADD_BR_OFF(nullptr);
        }
#line 2116 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 98: /* cond: numexpr  */
#line 408 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
                  {
            ADD_OP(OP_BRANCH_FALSE); (yyval.inst) = GetPC(); ADD_BR_OFF(nullptr);
        }
#line 2124 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 99: /* and: AND  */
#line 412 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
            {
            ADD_OP(OP_DUP); ADD_OP(OP_BRANCH_FALSE); (yyval.inst) = GetPC();
            ADD_BR_OFF(nullptr);
        }
#line 2133 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;

  case 100: /* or: OR  */
#line 417 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
           {
            ADD_OP(OP_DUP); ADD_OP(OP_BRANCH_TRUE); (yyval.inst) = GetPC();
            ADD_BR_OFF(nullptr);
        }
#line 2142 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"
    break;


#line 2146 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.cpp"

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
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (YY_("syntax error"));
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
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

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

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
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
                  YY_ACCESSING_SYMBOL (yystate), yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
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
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

#line 426 "/home/eteran/projects/nedit-ng/Interpreter/parser.y"
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
	InPtr                         = start;
	EndPtr                        = start + expr.size();

	if (yyparse()) {
		*msg          = ErrMsg;
		*stoppedAt    = gsl::narrow<int>(InPtr - start);
		std::unique_ptr<Program> prog = FinishCreatingProgram();
		return nullptr;
	}

	/* get the newly created program */
	std::unique_ptr<Program> prog = FinishCreatingProgram();

	/* parse succeeded */
	*msg       = QString();
	*stoppedAt = gsl::narrow<int>(InPtr - start);
	return prog.release();
}

static int yylex(void) {

	int i;
	Symbol *s;
	static const char escape[]  = "\\\"ntbrfave";
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
	if (InPtr->isDigit()) { /* number */

		QString value;
		auto p = std::back_inserter(value);

		*p++ = *InPtr++;
		while (InPtr != EndPtr && InPtr->isDigit()) {
			*p++ = *InPtr++;
		}

		int n = value.toInt();

		char name[28];
		snprintf(name, sizeof(name), "const %d", n);

		if ((yylval.sym = LookupSymbol(name)) == nullptr) {
			yylval.sym = InstallSymbol(name, SymbolConst, make_value(n));
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

		if (symName == QLatin1String("while")) return WHILE;
		if (symName == QLatin1String("if")) return IF;
		if (symName == QLatin1String("else")) return ELSE;
		if (symName == QLatin1String("for")) return FOR;
		if (symName == QLatin1String("break")) return BREAK;
		if (symName == QLatin1String("continue")) return CONTINUE;
		if (symName == QLatin1String("return")) return RETURN;
		if (symName == QLatin1String("in")) return IN;
		if (symName == QLatin1String("$args")) return ARG_LOOKUP;
		if (symName == QLatin1String("delete") && follow_non_whitespace('(', SYMBOL, DELETE) == DELETE) return DELETE;
		if (symName == QLatin1String("define")) {
			InPtr -= 6;
			return 0;
		}

		if ((s = LookupSymbolEx(symName)) == nullptr) {
			s = InstallSymbolEx(symName,
								symName[0] == QLatin1Char('$') ? (((symName[1] > QLatin1Char('0') && symName[1] <= QLatin1Char('9')) && symName.size() == 2) ? SymbolArg : SymbolGlobal) : SymbolLocal,
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
					int hexValue                = 0;
					const char *const hexDigits = "0123456789abcdef";
					const char *hexD;

					InPtr++;

					if (InPtr == EndPtr || (hexD = strchr(hexDigits, safe_tolower(InPtr->toLatin1()))) == nullptr) {
						*p++ = QLatin1Char('x');
					} else {
						hexValue = static_cast<int>(hexD - hexDigits);
						InPtr++;

						/* now do we have another digit? only accept one more */
						if (InPtr != EndPtr && (hexD = strchr(hexDigits, safe_tolower(InPtr->toLatin1()))) != nullptr) {
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
						InPtr++; /* octal introducer: don't count this digit */
					}

					if (QLatin1Char('0') <= *InPtr && *InPtr <= QLatin1Char('7')) {
						/* treat as octal - first digit */
						char octD    = InPtr++->toLatin1();
						int octValue = octD - '0';

						if (QLatin1Char('0') <= *InPtr && *InPtr <= QLatin1Char('7')) {
							/* second digit */
							octD     = InPtr++->toLatin1();
							octValue = (octValue << 3) + octD - '0';
							/* now do we have another digit? can we add it?
							   if value is going to be too big for char (greater
							   than 0377), stop converting now before adding the
							   third digit */
							if (QLatin1Char('0') <= *InPtr && *InPtr <= QLatin1Char('7') &&
								octValue <= 037) {
								/* third digit is acceptable */
								octD     = InPtr++->toLatin1();
								octValue = (octValue << 3) + octD - '0';
							}
						}

						if (octValue != 0) {
							*p++ = QLatin1Char(static_cast<char>(octValue));
						} else {
							InPtr = backslash + 1; /* just skip the backslash */
						}
					} else {                   /* \0 followed by non-digits: go back to 0 */
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
				*p++ = *InPtr++;
			}
		}

		InPtr++;
		yylval.sym = InstallStringConstSymbolEx(string);
		return STRING;
	}

	/* process remaining two character tokens or return single char as token */
	switch (InPtr++->toLatin1()) {
	case '>':
		return follow('=', GE, GT);
	case '<':
		return follow('=', LE, LT);
	case '=':
		return follow('=', EQ, '=');
	case '!':
		return follow('=', NE, NOT);
	case '+':
		return follow2('+', INCR, '=', ADDEQ, '+');
	case '-':
		return follow2('-', DECR, '=', SUBEQ, '-');
	case '|':
		return follow2('|', OR, '=', OREQ, '|');
	case '&':
		return follow2('&', AND, '=', ANDEQ, '&');
	case '*':
		return follow2('*', POW, '=', MULEQ, '*');
	case '/':
		return follow('=', DIVEQ, '/');
	case '%':
		return follow('=', MODEQ, '%');
	case '^':
		return POW;
	default:
		return (InPtr - 1)->toLatin1();
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
		} else if (*localInPtr == QLatin1Char('\\') && *(localInPtr + 1) == QLatin1Char('\n')) {
			localInPtr += 2;
		} else if (*localInPtr == QLatin1Char(expect)) {
			return (yes);
		} else {
			return (no);
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
	ErrMsg = QString::fromUtf8(s);
	return 0;
}
