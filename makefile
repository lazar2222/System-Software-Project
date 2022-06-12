all: asembler

asembler: misc/lexer.c misc/parser.c src/assembler.cpp
	g++ misc/parser.c misc/lexer.c src/assembler.cpp -Iinc -Imisc -g -o assembler

misc/lexer.c misc/lexer.h: misc/lexer.l misc/parser.h
	flex -o misc/lexer.c --header-file=misc/lexer.h misc/lexer.l

misc/parser.c misc/parser.h: misc/parser.y
	bison -o misc/parser.c --defines=misc/parser.h misc/parser.y

clean:
	rm misc/lexer.c misc/lexer.h misc/parser.c misc/parser.h assembler