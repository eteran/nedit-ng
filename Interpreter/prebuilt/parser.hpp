/* A Bison parser, made by GNU Bison 3.7.1.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2020 Free Software Foundation,
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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_YY_HOME_ETERAN_PROJECTS_NEDIT_NG_BUILD_INTERPRETER_PARSER_HPP_INCLUDED
# define YY_YY_HOME_ETERAN_PROJECTS_NEDIT_NG_BUILD_INTERPRETER_PARSER_HPP_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    NUMBER = 258,                  /* NUMBER  */
    STRING = 259,                  /* STRING  */
    SYMBOL = 260,                  /* SYMBOL  */
    DELETE = 261,                  /* DELETE  */
    ARG_LOOKUP = 262,              /* ARG_LOOKUP  */
    IF = 263,                      /* IF  */
    WHILE = 264,                   /* WHILE  */
    ELSE = 265,                    /* ELSE  */
    FOR = 266,                     /* FOR  */
    BREAK = 267,                   /* BREAK  */
    CONTINUE = 268,                /* CONTINUE  */
    RETURN = 269,                  /* RETURN  */
    IF_NO_ELSE = 270,              /* IF_NO_ELSE  */
    ADDEQ = 271,                   /* ADDEQ  */
    SUBEQ = 272,                   /* SUBEQ  */
    MULEQ = 273,                   /* MULEQ  */
    DIVEQ = 274,                   /* DIVEQ  */
    MODEQ = 275,                   /* MODEQ  */
    ANDEQ = 276,                   /* ANDEQ  */
    OREQ = 277,                    /* OREQ  */
    CONCAT = 278,                  /* CONCAT  */
    OR = 279,                      /* OR  */
    AND = 280,                     /* AND  */
    GT = 281,                      /* GT  */
    GE = 282,                      /* GE  */
    LT = 283,                      /* LT  */
    LE = 284,                      /* LE  */
    EQ = 285,                      /* EQ  */
    NE = 286,                      /* NE  */
    IN = 287,                      /* IN  */
    UNARY_MINUS = 288,             /* UNARY_MINUS  */
    NOT = 289,                     /* NOT  */
    INCR = 290,                    /* INCR  */
    DECR = 291,                    /* DECR  */
    POW = 292                      /* POW  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 38 "parser.y"

    Symbol *sym;
    Inst *inst;
    int nArgs;

#line 107 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.hpp"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_HOME_ETERAN_PROJECTS_NEDIT_NG_BUILD_INTERPRETER_PARSER_HPP_INCLUDED  */
