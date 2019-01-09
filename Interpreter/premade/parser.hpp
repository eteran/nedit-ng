/* A Bison parser, made by GNU Bison 3.1.  */

/* Bison interface for Yacc-like parsers in C

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
#line 37 "parser.y" /* yacc.c:1913  */

    Symbol *sym;
    Inst *inst;
    int nArgs;

#line 98 "/home/eteran/projects/nedit-ng/build/Interpreter/parser.hpp" /* yacc.c:1913  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_HOME_ETERAN_PROJECTS_NEDIT_NG_BUILD_INTERPRETER_PARSER_HPP_INCLUDED  */
