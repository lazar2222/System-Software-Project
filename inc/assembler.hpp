#ifndef SSPROJ_ASSEMBLER_HPP
#define SSPROJ_ASSEMBLER_HPP

#include <vector>
#include "parser.h"

extern int line;
void yyerror(const char *s);

enum ADDRESSING {LITERAL_IMM,SYMBOL_IMM,LITERAL_MEMDIR,SYMBOL_MEMDIR,SYMBOL_PCREL,REGDIR,REGIND,LITERAL_REGIND,SYMBOL_REGIND};
enum INSTRUCTION_TYPES {NADR,REGD,REGS,BOPR,DOPR};

union symLit
{
	int literal;
	char* symbol;
};

struct vectElem
{
	char type;
	union  symLit;
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
	struct operand;
};



struct directive
{
	enum yytokentype directiveType;
	union symLit sl;
	std::vector<vectElem> list;
};

union unitOfAssembly {
	struct instruction;
};

#endif //SSPROJ_ASSEMBLER_HPP
