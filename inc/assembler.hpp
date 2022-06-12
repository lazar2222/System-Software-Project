#ifndef SSPROJ_ASSEMBLER_HPP
#define SSPROJ_ASSEMBLER_HPP

#include <vector>
#include "parser.h"

extern int yylineno;
void yyerror(const char *s);

enum ADDRESSING {LITERAL_IMM,SYMBOL_IMM,LITERAL_MEMDIR,SYMBOL_MEMDIR,SYMBOL_PCREL,REGDIR,REGIND,LITERAL_REGIND,SYMBOL_REGIND};
enum INSTRUCTION_TYPES {NADR,REGD,REGS,BOPR,DOPR};
enum ELEMENT_TYPE {LITERAL,SYMBOL,NEGATIVE_LITERAL,NEGATIVE_SYMBOL};

union symLit
{
	int literal;
	char* symbol;
};

struct operand
{
	enum ADDRESSING operandType;
	int reg;
	union symLit sl;
};

struct instruction
{
	enum yytokentype opcode;
	enum INSTRUCTION_TYPES instructionType;
	int regD;
	int regS;
	struct operand op;
};

struct vectElem
{
	enum ELEMENT_TYPE type;
	union symLit value;
};

struct directive
{
	enum yytokentype directiveType;
	union symLit sl;
	std::vector<vectElem>* list;
};

union YYSTYPE
{
	int intv;
	char* stringv;
	struct operand op;
	struct instruction ins;
	struct vectElem ve;
	std::vector<vectElem>* list;
	struct directive dir;
};

void emitInstruction(struct instruction inst);
void emitDirective(struct directive dir);
void registerLabel(char* name);

#endif //SSPROJ_ASSEMBLER_HPP
