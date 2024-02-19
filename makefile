all: asembler objdump linker emulator

clean:
	rm misc/lexer.c misc/lexer.h misc/parser.c misc/parser.h assembler objdump linker emulator tests/*.o tests/*.hex

print: test
	./objdump tests/interrupts.o
	./objdump tests/main.o
	./objdump tests/program.o
	cat tests/program.hex
	cat tests/program2.hex

test: all
	./assembler tests/interrupts.s
	./assembler tests/main.s
	./linker -hex -place=ivt@0x0 -o tests/program.hex tests/interrupts.o tests/main.o
	./linker -relocateable -o tests/program.o tests/interrupts.o tests/main.o
	./linker -hex -place=ivt@0x0 -o tests/program2.hex tests/program.o

asembler: misc/lexer.c misc/parser.c src/assembler.cpp src/helper.cpp
	g++ misc/parser.c misc/lexer.c src/assembler.cpp src/helper.cpp -Iinc -Imisc -g -o assembler

misc/lexer.c misc/lexer.h: misc/lexer.l misc/parser.h
	flex -o misc/lexer.c --header-file=misc/lexer.h misc/lexer.l

misc/parser.c misc/parser.h: misc/parser.y
	bison -o misc/parser.c --defines=misc/parser.h misc/parser.y

objdump: src/objdump.cpp inc/vlah.hpp
	g++ src/objdump.cpp -Iinc -g -o objdump

linker: src/linker.cpp inc/vlah.hpp
	g++ src/linker.cpp -Iinc -g -o linker

emulator: src/emulator.cpp
	g++ src/emulator.cpp -Iinc -g -o emulator