#include <iostream>
#include "lexer.h"
#include "parser.h"
#include "assembler.hpp"

using namespace  std;

int line = 1;

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
	cout << "EEK, parse error on line " << line << "!  Message: " << s << endl;
	// might as well halt now:
	exit(-1);
}
