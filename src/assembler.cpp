#include <iostream>
#include <fstream>
#include <unistd.h>
#include "lexer.h"
#include "parser.h"
#include "assembler.hpp"
#include "assemblerImpl.hpp"
#include "helper.hpp"

using namespace std;

int main(int argc,char** argv)
{
	assemblerContext context;
	std::string inPath="",outPath="";
	int opt;

	while((opt = getopt(argc, argv, "+:o:")) != -1)
	{
		switch(opt)
		{
			case 'o':{outPath=optarg;break;}
			case ':':{err("option -o needs an argument"); break;}
			case '?':{err("invalid option"); break;}
		}
	}
	if(optind==argc-1)
	{
		inPath=argv[optind];
	}
	else
	{
		err("invalid number of arguments");
	}
	if(outPath.empty())
	{
		outPath=inPath.substr(0,inPath.find_last_of('.')).append(".o");
	}


	FILE* inFile = fopen(inPath.c_str(), "r");
	if (!inFile)
	{
		err("cant open: "+inPath);
	}
	yyin = inFile;

	if(yyparse(&context)!=0)
	{
		yyerror(&context,"unknown");
	}
	context.symbols[context.currSection].size=context.locCnt;
	doEqu(&context);
	verrUndefinedSym(&context);
	vwarEqu(&context);
	saveVLAH(outPath,&context);
}

void emitInstruction(struct instruction inst,void* context)
{
	auto* cx=(assemblerContext*) context;
	int ind = inst.opcode-HALT;
	verrInvalidInstruction(inst);
	unsigned char type=R_16_BE_DA;
	if(opcodes[ind]==PSEUDO_OPCODE)
	{
		inst=translateInst(inst);
		ind=inst.opcode-HALT;
		verrInvalidInstruction(inst);
	}
	verrNoSection(cx,"instruction");
	cx->sectionContents[cx->currSection].push_back(opcodes[ind]);
	cx->locCnt++;
	if(inst.instructionType==REGD)
	{
		inst.instructionType=REGS;
		inst.regS=0xf;
	}
	if(inst.instructionType==BOPR)
	{
		inst.instructionType=DOPR;
		inst.regD=0xf;
		if(inst.op.operandType==SYMBOL_PCREL)
		{
			inst.op.reg=7;
			type=R_16_BE_PCA;
		}
	}
	else if(inst.op.operandType==SYMBOL_PCREL)
	{
		inst.op.reg=7;
		type=R_16_BE_PCA;
		inst.op.operandType=SYMBOL_REGIND;
	}
	if(inst.instructionType==DOPR)
	{
		if(inst.op.operandType==LITERAL_IMM || inst.op.operandType==SYMBOL_IMM || inst.op.operandType==LITERAL_MEMDIR ||inst.op.operandType==SYMBOL_MEMDIR)
		{
			inst.op.reg=0xF;
		}
		inst.regS=inst.op.reg;
	}
	if(inst.instructionType==REGS || inst.instructionType==DOPR)
	{
		unsigned char byte=((inst.regD&0xF)<<4)|(inst.regS&0xF);
		cx->sectionContents[cx->currSection].push_back(byte);
		cx->locCnt++;
	}
	if(inst.instructionType==DOPR)
	{
		int adrind = inst.op.operandType - LITERAL_IMM;
		cx->sectionContents[cx->currSection].push_back(addressings[adrind]);
		cx->locCnt++;
		if(inst.op.operandType!=REGDIR && inst.op.operandType!=REGIND && inst.op.operandType!=PRD2 && inst.op.operandType!=POI2)
		{
			if(inst.op.operandType==LITERAL_IMM || inst.op.operandType==LITERAL_MEMDIR || inst.op.operandType==LITERAL_REGIND)
			{
				emitWord(cx,inst.op.sl.literal,BIG_ENDIAN);
			}
			else
			{
				emitSymbolValue(inst.op.sl.symbol,cx,type);
				free(inst.op.sl.symbol);
			}
		}
	}
}

void emitDirective(struct directive dir,void* context)
{
	auto* cx=(assemblerContext*) context;
	int ind = dir.directiveType-GLOBAL;
	verrInvalidDirective(dir);
	dirEmitters[ind](dir, cx);
}

void registerLabel(char* name,void* context)
{
	auto* cx=(assemblerContext*) context;
	verrNoSection(cx,"label");
	verrInvalidLabel(name,cx);
	if(cx->symbols.count(name)==0)
	{
		cx->symbols[name].name=-1;
		cx->symbols[name].info=VLAH_SYMBOL_LOCAL|VLAH_SYMBOL_NOTYPE;
		cx->symbols[name].section=cx->symbols[cx->currSection].section;
		cx->symbols[name].value=cx->locCnt;
		cx->symbols[name].size=0;
	}
	else if(cx->symbols[name].section==VLAH_SYMBOL_UND)
	{
		cx->symbols[name].section=cx->symbols[cx->currSection].section;
		cx->symbols[name].value=cx->locCnt;
		doBackPatch(name,cx);
		doEqu(cx);
	}
	free(name);
}

struct instruction translateInst(struct instruction inst)
{
	if(inst.opcode==PUSH)
	{
		inst.opcode=STR;
		inst.instructionType=DOPR;
		inst.op.operandType=PRD2;
		inst.op.reg=6;
	}
	else if(inst.opcode==POP)
	{
		inst.opcode=LDR;
		inst.instructionType=DOPR;
		inst.op.operandType=POI2;
		inst.op.reg=6;
	}
	return inst;
}

void emitSymbolValue(std::string name,assemblerContext* cx,unsigned char type)
{
	if(cx->symbols.count(name)==0)
	{
		cx->symbols[name].name=-1;
		cx->symbols[name].info=VLAH_SYMBOL_NOTYPE;
		cx->symbols[name].section=VLAH_SYMBOL_UND;
		cx->symbols[name].value=0;
		cx->symbols[name].size=0;
		backpatch nb;
		nb.section=cx->currSection;
		nb.offset=cx->locCnt;
		nb.type=type;
		cx->backpatches[name].push_back(nb);
		emitWord(cx,0,LITTLE_ENDIAN);
	}
	else if(cx->symbols[name].section==VLAH_SYMBOL_UND)
	{
		backpatch nb;
		nb.section=cx->currSection;
		nb.offset=cx->locCnt;
		nb.type=type;
		cx->backpatches[name].push_back(nb);
		emitWord(cx,0,LITTLE_ENDIAN);
	}
	else if(type==R_16_BE_PCA)
	{
		if(cx->symbols[name].section==VLAH_SYMBOL_ABS)
		{
			vwarnRelAbs(name);
			addRela(cx->currSection,cx->locCnt,cx,name,type);
			emitWord(cx,cx->symbols[name].value,type<16?LITTLE_ENDIAN:BIG_ENDIAN);
		}
		else if(cx->symbols[name].section==VLAH_SYMBOL_EXT)
		{
			addRela(cx->currSection,cx->locCnt,cx,name,type);
			emitWord(cx,cx->symbols[name].value,type<16?LITTLE_ENDIAN:BIG_ENDIAN);
		}
		else if(cx->sections[cx->symbols[name].section-1]==cx->currSection)
		{
			emitWord(cx,cx->symbols[name].value-2-cx->locCnt,type<16?LITTLE_ENDIAN:BIG_ENDIAN);
		}
		else
		{
			addRela(cx->currSection,cx->locCnt,cx,name,type);
			emitWord(cx,cx->symbols[name].value,type<16?LITTLE_ENDIAN:BIG_ENDIAN);
		}
	}
	else
	{
		addRela(cx->currSection,cx->locCnt,cx,name,type);
		emitWord(cx,cx->symbols[name].value,type<16?LITTLE_ENDIAN:BIG_ENDIAN);
	}
}

void doBackPatch(std::string name,assemblerContext* cx)
{
	if(cx->backpatches.count(name)!=0)
	{
		int val = cx->symbols[name].value;
		for (int i = 0; i <cx->backpatches[name].size() ; ++i)
		{
			int type=cx->backpatches[name][i].type;
			if(type==R_16_BE_PCA && cx->symbols[name].section!=VLAH_SYMBOL_ABS && cx->symbols[name].section!=VLAH_SYMBOL_EXT && cx->sections[cx->symbols[name].section-1]==cx->backpatches[name][i].section)
			{
				val=cx->symbols[name].value-2-cx->backpatches[name][i].offset;
			}
			else
			{
				addRela(cx->backpatches[name][i].section, cx->backpatches[name][i].offset, cx, name,cx->backpatches[name][i].type);
			}
			if(cx->backpatches[name][i].type<16)
			{
				cx->sectionContents[cx->backpatches[name][i].section][cx->backpatches[name][i].offset]=val&0xFF;
				cx->sectionContents[cx->backpatches[name][i].section][cx->backpatches[name][i].offset+1]=(val>>8)&0xFF;
			}
			else
			{
				cx->sectionContents[cx->backpatches[name][i].section][cx->backpatches[name][i].offset]=(val>>8)&0xFF;
				cx->sectionContents[cx->backpatches[name][i].section][cx->backpatches[name][i].offset+1]=val&0xFF;
			}
		}
		cx->backpatches.erase(name);
	}
}

void addRela(std::string section,int offset,assemblerContext* cx,std::string symbol,unsigned char type)
{
	Vlah_rela rela;
	std::string relaSym;
	rela.symbol=-1;
	rela.info=type;
	rela.offset=offset;
	if(cx->symbols[symbol].section!=VLAH_SYMBOL_ABS)
	{
		if (cx->symbols[symbol].info < 128 || cx->symbols[symbol].info == (VLAH_SYMBOL_GLOBAL + VLAH_SYMBOL_EQU))
		{
			//local
			rela.addend = cx->symbols[symbol].value;
			relaSym = cx->sections[cx->symbols[symbol].section - 1];
		}
		else
		{
			//global
			rela.addend = 0;
			relaSym = symbol;
		}
		if(type==R_16_BE_PCA)
		{
			rela.addend -= 2;
		}
		cx->relocations[section][relaSym].push_back(rela);
	}
}

void emitSection(struct directive dir,assemblerContext* cx)
{
	if(!cx->currSection.empty())
	{
		cx->symbols[cx->currSection].size=cx->locCnt;
	}
	cx->locCnt=0;
	cx->currSection=dir.sl.symbol;
	verrInvalidLabel(dir.sl.symbol,cx);
	free(dir.sl.symbol);
	cx->sections.push_back(cx->currSection);
	cx->sectionContents[cx->currSection]=vector<unsigned char>();
	if(cx->symbols.count(cx->currSection)==0)
	{
		cx->symbols[cx->currSection].name=-1;
		cx->symbols[cx->currSection].info=VLAH_SYMBOL_LOCAL;
		cx->symbols[cx->currSection].size=0;
	}
	vwarnGlobalSection(cx->currSection,cx->symbols[cx->currSection].info);
	cx->symbols[cx->currSection].info=VLAH_SYMBOL_SECTION;
	cx->symbols[cx->currSection].section=cx->sections.size();
	cx->symbols[cx->currSection].value=0;
	doBackPatch(cx->currSection,cx);
	doEqu(cx);
}

void emitGlobal(struct directive dir,assemblerContext* cx)
{
	for (int i = 0; i < dir.list->size(); ++i)
	{
		std::string name = (*dir.list)[i].value.symbol;
		free((*dir.list)[i].value.symbol);
		if(cx->symbols.count(name)==0)
		{
			cx->symbols[name].name=-1;
			cx->symbols[name].info=VLAH_SYMBOL_GLOBAL|VLAH_SYMBOL_NOTYPE;
			cx->symbols[name].section=VLAH_SYMBOL_UND;
			cx->symbols[name].value=0;
			cx->symbols[name].size=0;
		}
		else
		{
			if(cx->symbols[name].info!=VLAH_SYMBOL_SECTION)
			{
				cx->symbols[name].info|=VLAH_SYMBOL_GLOBAL;
			}
			else
			{
				vwarnGlobalSection(name,128);
			}
		}
	}
	delete dir.list;
}

void emitExtern(struct directive dir,assemblerContext* cx)
{
	for (int i = 0; i < dir.list->size(); ++i)
	{
		std::string name = (*dir.list)[i].value.symbol;
		verrInvalidLabel((*dir.list)[i].value.symbol,cx);
		free((*dir.list)[i].value.symbol);
		if(cx->symbols.count(name)==0)
		{
			cx->symbols[name].name=-1;
			cx->symbols[name].info=VLAH_SYMBOL_GLOBAL|VLAH_SYMBOL_NOTYPE;
			cx->symbols[name].section=VLAH_SYMBOL_EXT;
			cx->symbols[name].value=0;
			cx->symbols[name].size=0;
		}
		else
		{
			cx->symbols[name].info|=VLAH_SYMBOL_GLOBAL;
			cx->symbols[name].section=VLAH_SYMBOL_EXT;
		}
		doBackPatch(name,cx);
		doEqu(cx);
	}
	delete dir.list;
}

void emitWordS(struct directive dir,assemblerContext* cx)
{
	verrNoSection(cx,"directive");
	for (int i = 0; i < dir.list->size(); ++i)
	{
		struct vectElem elem = (*dir.list)[i];
		if(elem.type==LITERAL)
		{
			emitWord(cx,elem.value.literal,LITTLE_ENDIAN);
		}
		else
		{
			emitSymbolValue(elem.value.symbol,cx,R_16_LE_DA);
			free(elem.value.symbol);
		}
	}
	delete dir.list;
}

void emitSkip(struct directive dir,assemblerContext* cx)
{
	verrNoSection(cx,"directive");
	verrSkipNegative(dir);
	for (int i = 0; i < dir.sl.literal; ++i)
	{
		cx->locCnt++;
		cx->sectionContents[cx->currSection].push_back(0);
	}
}

void emitAscii(struct directive dir,assemblerContext* cx)
{
	verrNoSection(cx,"directive");
	std::string str = dir.sl.symbol;
	free(dir.sl.symbol);
	for (int i = 1; i < str.length()-1; ++i)
	{
		if(str[i]=='\\')
		{
			switch (str[i+1])
			{
				case 'n' :{cx->sectionContents[cx->currSection].push_back('\n');i++;break;}
				case 'r' :{cx->sectionContents[cx->currSection].push_back('\r');i++;break;}
				case 'b' :{cx->sectionContents[cx->currSection].push_back('\b');i++;break;}
				case 't' :{cx->sectionContents[cx->currSection].push_back('\t');i++;break;}
				case 'f' :{cx->sectionContents[cx->currSection].push_back('\f');i++;break;}
				case 'v' :{cx->sectionContents[cx->currSection].push_back('\b');i++;break;}
				case '0' :{cx->sectionContents[cx->currSection].push_back('\0');i++;break;}
				case '\\':{cx->sectionContents[cx->currSection].push_back('\\');i++;break;}
				default:{cx->sectionContents[cx->currSection].push_back(str[i]);break;}
			}
		}
		else
		{
			cx->sectionContents[cx->currSection].push_back(str[i]);
		}
		cx->locCnt++;
	}
}

void emitEqu(struct directive dir,assemblerContext* cx)
{
	verrInvalidLabel(dir.sl.symbol,cx);
	std::string name=dir.sl.symbol;
	free(dir.sl.symbol);
	if(cx->symbols.count(name)==0)
	{
		cx->symbols[name].name=-1;
		cx->symbols[name].info=VLAH_SYMBOL_NOTYPE;
		cx->symbols[name].section=VLAH_SYMBOL_UND;
		cx->symbols[name].value=0;
		cx->symbols[name].size=0;
	}
	cx->equs[name]=(*dir.list);
	delete dir.list;
	for (int i = 0; i < cx->equs[name].size(); ++i)
	{
		cx->equs[name][i].classifier=VLAH_SYMBOL_UND;
	}
	doEqu(cx);
}

void emitEnd(struct directive dir,assemblerContext* cx)
{
	//Currently nothing to do
}

void doEqu(assemblerContext* cx)
{
	bool changes;
	do
	{
		changes=false;
		for (auto it=cx->equs.begin();it!=cx->equs.end();)
		{
			auto val=resolveEqu(it->first,cx);
			if(val.first)
			{
				auto rel = verrReloc(it->first,cx);
				changes=true;
				cx->symbols[it->first].value=val.second;
				switch (rel.first)
				{
					case 0:	{cx->symbols[it->first].section=VLAH_SYMBOL_ABS; break;}
					case 1:	{cx->symbols[it->first].section=cx->symbols[rel.second].section; break;}
					case 2:	{cx->sections.push_back(rel.second); cx->symbols[it->first].section=cx->sections.size(); cx->symbols[it->first].info|=VLAH_SYMBOL_EQU;break;}
				}
				doBackPatch(it->first,cx);
				for (int i = 0; i < it->second.size(); ++i)
				{
					if(it->second[i].type==SYMBOL || it->second[i].type==NEGATIVE_SYMBOL)
					{
						free (it->second[i].value.symbol);
					}
				}
				it=cx->equs.erase(it);
			}
			else
			{
				it++;
			}
		}

	}while(changes);

}

std::pair<bool,int> resolveEqu(std::string name,assemblerContext* cx)
{
	bool resolved = true;
	int val=0;
	for (int i = 0; i < cx->equs[name].size(); ++i)
	{
		if(cx->equs[name][i].classifier==VLAH_SYMBOL_UND)
		{
			if(cx->equs[name][i].type==LITERAL || cx->equs[name][i].type==NEGATIVE_LITERAL)
			{
				cx->equs[name][i].classifier=VLAH_SYMBOL_ABS;
			}
			else if(cx->symbols.count(cx->equs[name][i].value.symbol)!=0 && cx->symbols[cx->equs[name][i].value.symbol].section!=VLAH_SYMBOL_UND)
			{
				cx->equs[name][i].classifier=cx->symbols[cx->equs[name][i].value.symbol].section;
			}
			else
			{
				resolved=false;
			}
		}
	}
	if(resolved)
	{
		for (int i = 0; i < cx->equs[name].size(); ++i)
		{
				if(cx->equs[name][i].type==LITERAL)
				{
					val+=cx->equs[name][i].value.literal;
				}
				else if(cx->equs[name][i].type==NEGATIVE_LITERAL)
				{
					val-=cx->equs[name][i].value.literal;
				}
				else if(cx->equs[name][i].type==SYMBOL)
				{
					val+=cx->symbols[cx->equs[name][i].value.symbol].value;
				}
				else if(cx->equs[name][i].type==NEGATIVE_SYMBOL)
				{
					val-=cx->symbols[cx->equs[name][i].value.symbol].value;
				}
		}
	}
	return std::make_pair(resolved,val);
}

void saveVLAH(std::string outPath,assemblerContext* context)
{
	std::ofstream outFile (outPath, std::ofstream::binary);
	char sep=0;

	Vlah_header header;
	vector<Vlah_section> sections;
	vector<Vlah_symbol> symtab;
	std::string symstrpool="";
	std::string strpool="";
	unsigned int offset=0;

	header.ident[0]=VLAH_MAGIC_0;
	header.ident[1]=VLAH_MAGIC_1;
	header.ident[2]=VLAH_MAGIC_2;
	header.ident[3]=VLAH_MAGIC_3;
	header.sections=0;

	symtab.push_back({0,0,0,0,0});
	for (int i = 0; i < context->sections.size(); ++i)
	{
		Vlah_symbol sym = context->symbols[context->sections[i]];
		context->symbols[context->sections[i]].name=symtab.size();
		sym.name=symstrpool.size();
		symstrpool.append(context->sections[i]+'\0');
		symtab.push_back(sym);
	}
	for (auto& it:context->symbols)
	{
		if(it.second.name==-1)
		{
			if(it.second.info==(VLAH_SYMBOL_GLOBAL+VLAH_SYMBOL_EQU))
			{
				vwarnGlobExt(it.first);
				it.second.info&=0xF;
			}
			Vlah_symbol sym = it.second;
			it.second.name = symtab.size();
			sym.name = symstrpool.size();
			symstrpool.append(it.first + '\0');
			symtab.push_back(sym);
		}
	}

	sections.push_back({(unsigned int)strpool.size(),offset,
						(unsigned short)(sizeof(Vlah_symbol)*symtab.size()),VLAH_SECTION_TYPE_SYMTAB,0});
	strpool.append(".symtab");
	strpool.append(&sep,1);
	header.sections++;
	offset+=sizeof(Vlah_symbol)*symtab.size();

	sections.push_back({(unsigned int)strpool.size(),offset,
	                    (unsigned short)(symstrpool.size()),VLAH_SECTION_TYPE_STRTAB,0});
	strpool.append(".strtab");
	strpool.append(&sep,1);
	header.sections++;
	offset+=symstrpool.size();

	for (int i = 0; i < context->sections.size(); ++i)
	{
		if((context->symbols[context->sections[i]].info&0xF)==VLAH_SYMBOL_SECTION)
		{
			Vlah_section s;
			s.name=strpool.size();
			strpool.append(context->sections[i]);
			strpool.append(&sep,1);
			s.type=VLAH_SECTION_TYPE_DATA;
			s.offset=offset;
			s.size=context->symbols[context->sections[i]].size;
			s.link=0;
			header.sections++;
			offset+=s.size;
			sections.push_back(s);

			if(context->relocations.count(context->sections[i])==1)
			{
				s.name=strpool.size();
				strpool.append(".rela"+context->sections[i]);
				strpool.append(&sep,1);
				s.type=VLAH_SECTION_TYPE_RELA;
				s.offset=offset;
				int relasiz=0;
				for (auto& it:context->relocations[context->sections[i]])
				{
					relasiz+=it.second.size()*sizeof(Vlah_rela);
					int ind=context->symbols[it.first].name;
					for (int j = 0; j < it.second.size(); ++j)
					{
							it.second[j].symbol=ind;
					}
				}
				s.size=relasiz;
				s.link=header.sections-1;
				header.sections++;
				offset+=s.size;
				sections.push_back(s);
			}
		}
	}

	sections.push_back({(unsigned int)strpool.size(),offset,
	                    (unsigned short)(strpool.size()+10),VLAH_SECTION_TYPE_STRTAB,0});
	strpool.append(".shstrtab");
	strpool.append(&sep,1);
	header.sections++;
	offset+=strpool.size();

	outFile.write((char*)&header,sizeof(header));
	outFile.write((char*)sections.data(),sizeof(Vlah_section)*sections.size());
	outFile.write((char*)symtab.data(),sizeof(Vlah_symbol)*symtab.size());
	outFile.write(symstrpool.data(),symstrpool.size());
	for (int i = 0; i < context->sections.size(); ++i)
	{
		if((context->symbols[context->sections[i]].info&0xF)==VLAH_SYMBOL_SECTION)
		{
			outFile.write((char*)context->sectionContents[context->sections[i]].data(),context->sectionContents[context->sections[i]].size());
			if(context->relocations.count(context->sections[i])==1)
			{
				for (auto& it:context->relocations[context->sections[i]])
				{
					outFile.write((char*)it.second.data(),sizeof(Vlah_rela)*it.second.size());
				}
			}
		}
	}

	outFile.write(strpool.data(),strpool.size());

	outFile.close();
	if(outFile.fail())
	{
		err("cant write to: "+outPath);
	}
}

void (*dirEmitters[NUM_DIRECTIVES])(struct directive,assemblerContext*)=
		{emitGlobal, emitExtern, emitSection, emitWordS, emitSkip,emitAscii, emitEqu, emitEnd};
unsigned char opcodes[NUM_INSTRUCTIONS]=
		{0x0,0x10,0x20,0x30,0x40,0x50,0x51,0x52,0x53,
		 PSEUDO_OPCODE,PSEUDO_OPCODE,0x60,0x70,0x71,0x72,0x73,0x74,
		 0x80,0x81,0x82,0x83,0x84,0x90,0x91,0xA0,0xB0};
unsigned char addressings[NUM_ADDRESSINGS]=
		{0x00,0x00,0x04,0x04,0x05,0x01,0x02,0x03,0x03,0x12,0x42};