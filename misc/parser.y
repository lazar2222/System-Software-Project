%{
	#include "lexer.h"
	#include "assembler.hpp"
%}
%union {
	int intv;
	char *stringv;
}
%token <stringv> LABEL
%token HALT
%token INT
%token IRET
%token CALL
%token RET
%token JMP
%token JEQ
%token JNE
%token JGT
%token PUSH
%token POP
%token XCHG
%token ADD
%token SUB
%token MUL
%token DIV
%token CMP
%token NOT
%token AND
%token OR
%token XOR
%token TEST
%token SHL
%token SHR
%token LDR
%token STR
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
instruction: nadr | instregd | instregs | instbopr | instdopr;
instregd: regd REG;
instregs: regs REG ',' REG;
instbopr: bopr boperand;
instdopr: dopr REG ',' doperand;
nadr: HALT | IRET | RET;
regd: INT | PUSH | POP | NOT;
regs: XCHG | ADD | SUB | MUL | DIV | CMP | AND | OR | XOR | TEST | SHL | SHR;
bopr: CALL | JMP | JEQ | JNE | JGT;
dopr: LDR | STR;
boperand: LIT | SYM | '%' SYM | '*' LIT | '*' SYM | '*' REG | '*' '[' REG ']' | '*' '[' REG '+' LIT ']' | '*' '[' REG '+' SYM ']';
doperand: '$' LIT | '$' SYM | LIT | SYM | '%' SYM | REG | '[' REG ']' | '[' REG '+' LIT ']' | '[' REG '+' SYM ']';
%%