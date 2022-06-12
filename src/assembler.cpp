#include <iostream>
#include "lexer.h"
#include "parser.h"
#include "assembler.hpp"

using namespace  std;

int main(int argc,char** argv)
{
	// Open a file handle to a particular file:
	FILE *myfile = fopen("tests/interrupts.s", "r");
	// Make sure it is valid:
	if (!myfile)
	{
		cout << "I can't open a.snazzle.file!" << endl;
		return -1;
	}
	// Set Flex to read from it instead of defaulting to STDIN:
	yyin = myfile;

	// Parse through the input:
	yyparse();
}

void yyerror(const char *s) {
	cout<<"Parse error on line: "<<yylineno<<" "<<s<<endl;
	exit(-1);
}

void emitInstruction(struct instruction inst)
{
	cout<<"INST"<<inst.opcode<<endl;
}

void emitDirective(struct directive dir)
{
	cout<<"DIR"<<dir.directiveType<<endl;
}

void registerLabel(char* name)
{
	cout<<"LAB"<<name<<endl;
}