#ifndef SSPROJ_ASSEMBLERIMPL_HPP
#define SSPROJ_ASSEMBLERIMPL_HPP

#include <vector>
#include <unordered_map>
#include "assembler.hpp"
#include "vlah.hpp"

#define NUM_INSTRUCTIONS 26
#define NUM_DIRECTIVES 8
#define NUM_ADDRESSINGS 11
#define PSEUDO_OPCODE 0xFE
#define INVALID_OPCODE 0xFF
#define INVALID_ADDRESSING 0xFF

typedef struct
{
	std::string section;
	int offset;
	unsigned char type;
} backpatch;

typedef struct
{
	unsigned short locCnt=0;
	std::string currSection="";
	std::vector<std::string> sections;
	std::unordered_map<std::string,std::vector<unsigned char>> sectionContents;
	std::unordered_map<std::string,Vlah_symbol> symbols;
	std::unordered_map<std::string,std::vector<backpatch>>backpatches;
	std::unordered_map<std::string,std::unordered_map<std::string,std::vector<Vlah_rela>>>relocations;
	std::unordered_map<std::string,std::vector<vectElem>>equs;
} assemblerContext;

struct instruction translateInst(struct instruction inst);
void emitSymbolValue(std::string name,assemblerContext* cx,unsigned char type);
void doBackPatch(std::string name,assemblerContext* cx);
void addRela(std::string section,int offset,assemblerContext* cx,std::string symbol,unsigned char type);
void doEqu(assemblerContext* cx);
std::pair<bool,int> resolveEqu(std::string,assemblerContext* cx);
void saveVLAH(std::string outPath,assemblerContext* context);

extern void (*dirEmitters[NUM_DIRECTIVES])(struct directive,assemblerContext*);
extern unsigned char opcodes[NUM_INSTRUCTIONS];
extern unsigned char addressings[NUM_ADDRESSINGS];

#endif //SSPROJ_ASSEMBLERIMPL_HPP
