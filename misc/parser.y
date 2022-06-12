%{
	#include "lexer.h"
	#include "assembler.hpp"

	void flipSignV(std::vector<vectElem>* vect);
	void flipSign(vectElem* elem);
%}
%define api.value.type {union YYSTYPE}
%define parse.error verbose

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
%left UMIN

%nterm asm
%nterm stasm
%nterm tasm
%nterm endls
%nterm line
%nterm operation
%nterm <dir> directive
%nterm <dir> dirglobal
%nterm <dir> dirextern
%nterm <dir> dirsection
%nterm <dir> dirword
%nterm <dir> dirskip
%nterm <dir> dirascii
%nterm <dir> direqu
%nterm <list> symlist
%nterm <list> symlitlist
%nterm <ve>   symlit
%nterm <ve>   symulit
%nterm <intv> slit
%nterm <list> expression
%nterm <ins> instruction
%nterm <ins> instregd
%nterm <ins> instregs
%nterm <ins> instbopr
%nterm <ins> instdopr
%nterm <ins> nadr
%nterm <ins> regd
%nterm <ins> regs
%nterm <ins> bopr
%nterm <ins> dopr
%nterm <op> boperand
%nterm <op> doperand
%%
asm: endls stasm | stasm;
stasm: tasm endls | tasm;
tasm: tasm endls line | line
endls: endls ENDL | ENDL;

line:
	LABEL {registerLabel($1);} endls operation
	| operation
;
operation:
	directive       {emitDirective($1);}
	| instruction   {emitInstruction($1);}
;

directive:
	dirglobal       {$$=$1; $$.directiveType=GLOBAL;}
	| dirextern     {$$=$1; $$.directiveType=EXTERN;}
	| dirsection    {$$=$1; $$.directiveType=SECTION;}
	| dirword       {$$=$1; $$.directiveType=WORD;}
	| dirskip       {$$=$1; $$.directiveType=SKIP;}
	| dirascii      {$$=$1; $$.directiveType=ASCII;}
	| direqu        {$$=$1; $$.directiveType=EQU;}
	| END           {       $$.directiveType=END;}
;
dirglobal: GLOBAL symlist       {$$.list = $2;};
dirextern: EXTERN symlist       {$$.list = $2;};
dirsection: SECTION SYM         {$$.sl.symbol = $2;};
dirword: WORD symlitlist        {$$.list = $2;};
dirskip: SKIP slit              {$$.sl.literal = $2;};
dirascii: ASCII STRING          {$$.sl.symbol = $2;};
direqu: EQU SYM ',' expression  {$$.sl.symbol = $2; $$.list=$4;};
symlist:
	symlist ',' SYM             {$$=$1;                         struct vectElem elem; elem.type=SYMBOL; elem.value.symbol=$3; $$->push_back(elem);}
	| SYM                       {$$=new std::vector<vectElem>;  struct vectElem elem; elem.type=SYMBOL; elem.value.symbol=$1; $$->push_back(elem);}
;
symlitlist:
	symlitlist ',' symlit       {$$=$1;                         $$->push_back($3);}
	| symlit                    {$$=new std::vector<vectElem>;  $$->push_back($1);}
;
symlit:
	SYM                         {$$.type=SYMBOL;  $$.value.symbol=$1;}
	| slit                      {$$.type=LITERAL; $$.value.literal=$1;}
;
symulit:
	SYM                         {$$.type=SYMBOL;  $$.value.symbol=$1;}
	| LIT                       {$$.type=LITERAL; $$.value.literal=$1;}
;
slit:
	'+' LIT                     {$$=$2;}
	| '-' LIT                   {$$=-$2;}
	| LIT                       {$$=$1;}
;
expression:
	symulit                     {$$=new std::vector<vectElem>;  $$->push_back($1);}
	| expression '+' expression {$$=$1; $$->insert( $$->end(), $3->begin(), $3->end() ); delete $3;}
	| expression '-' expression {$$=$1; flipSignV($3); $$->insert( $$->end(), $3->begin(), $3->end() ); delete $3;}
	| '-' expression %prec UMIN {$$=$2; flipSignV($2);}
	| '(' expression ')'        {$$=$2;}
;

instruction:
	nadr        {$$=$1; $$.instructionType=NADR;}
	| instregd  {$$=$1; $$.instructionType=REGD;}
	| instregs  {$$=$1; $$.instructionType=REGS;}
	| instbopr  {$$=$1; $$.instructionType=BOPR;}
	| instdopr  {$$=$1; $$.instructionType=DOPR;}
;
instregd: regd REG              {$$=$1; $$.regD=$2;};
instregs: regs REG ',' REG      {$$=$1; $$.regD=$2; $$.regS=$4;};
instbopr: bopr boperand         {$$=$1; $$.op=$2;};
instdopr: dopr REG ',' doperand {$$=$1; $$.regD=$2; $$.op=$4;};
nadr:
	HALT    {$$.opcode=HALT;}
	| IRET  {$$.opcode=IRET;}
	| RET   {$$.opcode=RET;}
;
regd:
	INT     {$$.opcode=INT;}
	| PUSH  {$$.opcode=PUSH;}
	| POP   {$$.opcode=POP;}
	| NOT   {$$.opcode=NOT;}
;
regs:
	XCHG    {$$.opcode=XCHG;}
	| ADD   {$$.opcode=ADD;}
	| SUB   {$$.opcode=SUB;}
	| MUL   {$$.opcode=MUL;}
	| DIV   {$$.opcode=DIV;}
	| CMP   {$$.opcode=CMP;}
	| AND   {$$.opcode=AND;}
	| OR    {$$.opcode=OR;}
	| XOR   {$$.opcode=XOR;}
	| TEST  {$$.opcode=TEST;}
	| SHL   {$$.opcode=SHL;}
	| SHR   {$$.opcode=SHR;}
;
bopr:
	CALL    {$$.opcode=CALL;}
	| JMP   {$$.opcode=JMP;}
	| JEQ   {$$.opcode=JEQ;}
	| JNE   {$$.opcode=JNE;}
	| JGT   {$$.opcode=JGT;}
;
dopr:
	LDR     {$$.opcode=LDR;}
	| STR   {$$.opcode=STR;}
;
boperand:
	slit                        {$$.operandType=LITERAL_IMM;    $$.sl.literal=$1;}
	| SYM                       {$$.operandType=SYMBOL_IMM;     $$.sl.symbol=$1;}
	| '%' SYM                   {$$.operandType=SYMBOL_PCREL;   $$.sl.symbol=$2;}
	| '*' slit                  {$$.operandType=LITERAL_MEMDIR; $$.sl.literal=$2;}
	| '*' SYM                   {$$.operandType=SYMBOL_MEMDIR;  $$.sl.symbol=$2;}
	| '*' REG                   {$$.operandType=REGDIR;         $$.reg=$2;}
	| '*' '[' REG ']'           {$$.operandType=REGIND;         $$.reg=$3;}
	| '*' '[' REG '+' slit ']'  {$$.operandType=LITERAL_REGIND; $$.reg=$3; $$.sl.literal=$5;}
	| '*' '[' REG '+' SYM ']'   {$$.operandType=SYMBOL_REGIND;  $$.reg=$3; $$.sl.symbol=$5;}
;
doperand:
	'$' slit                    {$$.operandType=LITERAL_IMM;    $$.sl.literal=$2;}
	| '$' SYM                   {$$.operandType=SYMBOL_IMM;     $$.sl.symbol=$2;}
	| slit                      {$$.operandType=LITERAL_MEMDIR; $$.sl.literal=$1;}
	| SYM                       {$$.operandType=SYMBOL_MEMDIR;  $$.sl.symbol=$1;}
	| '%' SYM                   {$$.operandType=SYMBOL_PCREL;   $$.sl.symbol=$2;}
	| REG                       {$$.operandType=REGDIR;         $$.reg=$1;}
	| '[' REG ']'               {$$.operandType=REGIND;         $$.reg=$2;}
	| '[' REG '+' slit ']'      {$$.operandType=LITERAL_REGIND; $$.reg=$2; $$.sl.literal=$4;}
	| '[' REG '+' SYM ']'       {$$.operandType=SYMBOL_REGIND;  $$.reg=$2; $$.sl.symbol=$4;}
;
%%
void flipSignV(std::vector<vectElem>* vect)
{
	for (int i = 0; i < vect->size(); ++i)
	{
		flipSign(&((*vect)[i]));
	}
}

void flipSign(vectElem* elem)
{
	switch (elem->type)
	{
		case LITERAL:           {elem->type=NEGATIVE_LITERAL; break;}
		case SYMBOL:            {elem->type=NEGATIVE_SYMBOL; break;}
		case NEGATIVE_LITERAL:  {elem->type=LITERAL; break;}
		case NEGATIVE_SYMBOL:   {elem->type=SYMBOL; break;}
	}
}