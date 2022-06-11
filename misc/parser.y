%{
	#include "lexer.h"
	#include "parser.h"
	#include "assembler.hpp"
%}
%union {
	int intv;
	char *stringv;
}
%token <stringv> LABEL
%token <stringv> NADR
%token <stringv> REGD
%token <stringv> REGS
%token <stringv> BOPR
%token <stringv> DOPR
%token GLOBAL
%token EXTERN
%token SECTION
%token WORD
%token SKIP
%token ASCII
%token EQU
%token END
%token <intv> REG
%token <intv> LIT
%token <stringv> STRING;
%token <stringv> SYM
%token ENDL

%left '+' '-'
%%
asm: endls stasm | stasm;
stasm: tasm endls | tasm;
tasm: tasm endls line | line
endls: endls ENDL | ENDL;
line: LABEL endls operation | operation;
operation: directive | instruction;
directive: dirglobal | dirextern | dirsection | dirword | dirskip | dirascii | direqu | END;
dirglobal: GLOBAL symlist;
dirextern: EXTERN symlist;
dirsection: SECTION SYM;
dirword: WORD symlitlist;
dirskip: SKIP LIT
dirascii: ASCII STRING;
direqu: EQU SYM ',' expression;
symlist: symlist ',' SYM | SYM
symlitlist: symlitlist ',' symlit | symlit;
symlit: SYM | LIT;
expression: symlit | expression '+' expression | expression '-' expression | '(' expression ')';
instruction: NADR | instregd | instregs | instbopr | instdopr;
instregd: REGD REG;
instregs: REGS REG ',' REG;
instbopr: BOPR boperand;
instdopr: DOPR REG ',' doperand;
boperand: LIT | SYM | '%' SYM | '*' LIT | '*' SYM | '*' REG | '*' '[' REG ']' | '*' '[' REG '+' LIT ']' | '*' '[' REG '+' SYM ']';
doperand: '$' LIT | '$' SYM | LIT | SYM | '%' SYM | REG | '[' REG ']' | '[' REG '+' LIT ']' | '[' REG '+' SYM ']';
%%