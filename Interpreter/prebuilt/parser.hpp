
/* A Bison parser, made by GNU Bison 2.4.1.  */

/* Skeleton interface for Bison's Yacc-like parsers in C
   
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

/* Line 1676 of yacc.c  */
#line 37 "parser.y"

    Symbol *sym;
    Inst *inst;
    int nArgs;



/* Line 1676 of yacc.c  */
#line 97 "Interpreter/parser.hpp"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

extern YYSTYPE yylval;


