#include <iostream>
#include "lexer.h"
#include "parser.h"
#include "assembler.hpp"
#include "assemblerImpl.hpp"
#include "helper.hpp"

using namespace std;

void yyerror(void* context,const char *msg)
{
	cout<<"Error: parse error: "<<msg<<", on line: "<<yylineno<<endl;
	exit(-1);
}

void err(std::string msg)
{
	cout<<"Error: "<<msg<<endl;
	exit(-1);
}

void verrInvalidInstruction(struct instruction inst)
{
	int ind = inst.opcode-HALT;
	if(ind>=NUM_INSTRUCTIONS || opcodes[ind]==INVALID_OPCODE)
	{
		cout<<"Error: invalid instruction opcode: "<<inst.opcode<<", on line: "<<yylineno<<endl;
		exit(-1);
	}
	int adrind = inst.op.operandType - LITERAL_IMM;
	if((inst.instructionType==BOPR || inst.instructionType==DOPR) && (adrind>=NUM_ADDRESSINGS || addressings[adrind]==INVALID_ADDRESSING))
	{
		cout<<"Error: invalid instruction addressing: "<<inst.op.operandType<<", on line: "<<yylineno<<endl;
		exit(-1);
	}
}

void verrInvalidDirective(struct directive dir)
{
	int ind = dir.directiveType-GLOBAL;
	if(ind>=NUM_DIRECTIVES || dirEmitters[ind]==nullptr)
	{
		cout<<"Error: invalid directive: "<<dir.directiveType<<", on line: "<<yylineno<<endl;
		//exit(-1);
	}
}

void verrInvalidLabel(char* name,assemblerContext* cx)
{
	if(cx->symbols.count(name)!=0 && cx->symbols[name].section!=VLAH_SYMBOL_UND)
	{
		cout<<"Error: redefinition of symbol "<<name<<", on line :"<<yylineno<<endl;
		exit(-1);
	}
}

void verrNoSection(assemblerContext* cx,string msg)
{
	if(cx->currSection.empty())
	{
		cout<<"Error: invalid "<<msg<<" outside of section, on line: "<<yylineno<<endl;
		exit(-1);
	}
}

void verrSkipNegative(struct directive dir)
{
	if(dir.sl.literal<0)
	{
		cout<<"Error: cant skip less than 0, on line: "<<yylineno<<endl;
		exit(-1);
	}
}

void verrUndefinedSym(assemblerContext* cx)
{
	if(!cx->backpatches.empty())
	{
		for (auto& it:cx->backpatches)
		{
			cout<<"Error: undefined symbol: "<<it.first<<endl;
			if(cx->equs.count(it.first)!=0)
			{
				for (int i = 0; i < cx->equs[it.first].size(); ++i)
				{
					if(cx->equs[it.first][i].classifier==VLAH_SYMBOL_UND)
					{
						cout<<"    Caused by: undefined symbol: "<<cx->equs[it.first][i].value.symbol<<endl;
					}
				}
			}
		}
		exit(-1);
	}
}

std::pair<int,std::string> verrReloc(std::string name,assemblerContext* cx)
{
	std::unordered_map<unsigned char,int> relocIndex;
	int uniquePlus=0;
	int uniqueMinus=0;
	std::string globSym;
	for (int i = 0; i < cx->equs[name].size(); ++i)
	{
		if(cx->equs[name][i].type==SYMBOL)
		{
			int sec =cx->symbols[cx->equs[name][i].value.symbol].section;
			if(sec==VLAH_SYMBOL_EXT)
			{
				uniquePlus++;
				globSym=cx->equs[name][i].value.symbol;
			}
			else if(sec!=VLAH_SYMBOL_ABS)
			{
				relocIndex[sec]++;
			}
		}
		else if(cx->equs[name][i].type==NEGATIVE_SYMBOL)
		{
			int sec =cx->symbols[cx->equs[name][i].value.symbol].section;
			if(sec==VLAH_SYMBOL_EXT)
			{
				uniqueMinus++;
			}
			else if(sec!=VLAH_SYMBOL_ABS)
			{
				relocIndex[sec]--;
			}
		}
	}
	unsigned char locRel=VLAH_SYMBOL_ABS;
	for (auto& it:relocIndex)
	{
		if(it.second!=0)
		{
			if(it.second==1)
			{
				if(locRel==VLAH_SYMBOL_ABS)
				{
					locRel=it.first;
				}
				else
				{
					cout<<"Error: symbol "<<name<<" not relocateable 1."<<cx->sections[it.first-1]<<"+ 1."<<cx->sections[locRel-1]<<endl;
					exit(-1);
				}
			}
			else
			{
				cout<<"Error: symbol "<<name<<" not relocateable "<<it.second<<"."<<cx->sections[it.first-1]<<endl;
				exit(-1);
			}
		}
	}
	if(uniqueMinus>0)
	{
		cout<<"Error: symbol "<<name<<" not relocateable -"<<uniqueMinus<<".uniqe()"<<endl;
		exit(-1);
	}
	if(uniquePlus>1)
	{
		cout<<"Error: symbol "<<name<<" not relocateable "<<uniquePlus<<".uniqe()"<<endl;
		exit(-1);
	}
	if(locRel!=VLAH_SYMBOL_ABS && uniquePlus==1)
	{
		cout<<"Error: symbol "<<name<<" not relocateable 1.uniqe() + 1."<<cx->sections[locRel-1]<<endl;
		exit(-1);
	}
	if(uniquePlus==0 && locRel==VLAH_SYMBOL_ABS)
	{
		return std::make_pair(0,"");
	}
	if(uniquePlus==0 && locRel!=VLAH_SYMBOL_ABS)
	{
		return std::make_pair(1,cx->sections[locRel-1]);
	}
	if(uniquePlus==1 && locRel==VLAH_SYMBOL_ABS)
	{
		return std::make_pair(2,globSym);
	}
	return std::make_pair(-1,"");
}

void vwarnOverflow(int ilit)
{
	unsigned short lit = ilit;
	if(lit!=ilit)
	{
		cout<<"Warning: literal: "<<ilit<<" overflows 16bits, on line: "<<yylineno<<endl;
	}
}

void vwarEqu(assemblerContext* cx)
{
	if(!cx->equs.empty())
	{
		for (auto& it:cx->equs)
		{
			cout<<"Warning: undefined unused symbol: "<<it.first<<endl;
		}
	}
}

void vwarnRelAbs(std::string name)
{
	cout<<"Warning: PCREL with absolute symbol: "<<name<<endl;
}

void vwarnGlobExt(std::string name)
{
	cout<<"Warning: global equ symbol: "<<name<<" uses etern symbols, will be demoted to local symbol"<<endl;
}

void vwarnGlobalSection(std::string name,int info)
{
	if(info>127)
	{
		cout<<"Warning: sections are already global, section: "<<name<<endl;
	}
}

void emitWord(assemblerContext* cx,int sword,int endianess)
{
	verrNoSection(cx,"word");
	vwarnOverflow(sword);
	unsigned short word=sword;
	cx->locCnt+=2;
	if(endianess==LITTLE_ENDIAN)
	{
		cx->sectionContents[cx->currSection].push_back(word&0xFF);
		cx->sectionContents[cx->currSection].push_back((word>>8)&0xFF);
	}
	else
	{
		cx->sectionContents[cx->currSection].push_back((word>>8)&0xFF);
		cx->sectionContents[cx->currSection].push_back(word&0xFF);
	}
}