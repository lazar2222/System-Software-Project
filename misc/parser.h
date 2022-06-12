/* A Bison parser, made by GNU Bison 3.5.1.  */

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

/* Undocumented macros, especially those whose name start with YY_,
   are private implementation details.  Do not rely on them.  */

#ifndef YY_YY_MISC_PARSER_H_INCLUDED
# define YY_YY_MISC_PARSER_H_INCLUDED
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
    LABEL = 258,
    HALT = 259,
    INT = 260,
    IRET = 261,
    CALL = 262,
    RET = 263,
    JMP = 264,
    JEQ = 265,
    JNE = 266,
    JGT = 267,
    PUSH = 268,
    POP = 269,
    XCHG = 270,
    ADD = 271,
    SUB = 272,
    MUL = 273,
    DIV = 274,
    CMP = 275,
    NOT = 276,
    AND = 277,
    OR = 278,
    XOR = 279,
    TEST = 280,
    SHL = 281,
    SHR = 282,
    LDR = 283,
    STR = 284,
    GLOBAL = 285,
    EXTERN = 286,
    SECTION = 287,
    WORD = 288,
    SKIP = 289,
    ASCII = 290,
    EQU = 291,
    END = 292,
    REG = 293,
    LIT = 294,
    STRING = 295,
    SYM = 296,
    ENDL = 297
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 5 "misc/parser.y"

	int intv;
	char *stringv;

#line 105 "misc/parser.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_MISC_PARSER_H_INCLUDED  */
