#include <iostream>
#include <fstream>
#include <iomanip>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include "vlah.hpp"

using namespace std;

typedef struct
{
	unordered_map<string,vector<unsigned char>> sectionData;
	vector<Vlah_symbol> symbols;
	unordered_map<string,int> symbolNameMap;
	vector<string> symbolNameVector;
	unordered_map<string,vector<Vlah_rela>> relocations;
} linkerContext;

typedef struct
{
	string name;
	int start;
	int end;
} segment;

void error(const string& msg);
linkerContext loadVlah(ifstream& file);
vector<segment> placeSections(vector<linkerContext>& cxs,vector<pair<int,string>>& places);
void updateSymTab(vector<Vlah_symbol>& symbols,int ind,int val);
void doRelocations(vector<linkerContext>& cxs);
int findSymVal(vector<linkerContext>& cxs,string& name);
void outputHex(string& outPath,vector<linkerContext>& cxs,vector<segment>& segments);
void header(int adr,ofstream& file);
linkerContext mergeSymbols(vector<linkerContext>& cxs);
void saveVlah(string& outPath,linkerContext& outputContext,vector<linkerContext>&cxs);


int main(int argc,char** argv)
{
	vector<linkerContext> cxs;
	vector<pair<int,string>> places;
	string outPath;
	bool hex= false;
	bool relocateable= false;

	int i = 1;

	for (; i < argc; ++i)
	{
		string arg=argv[i];
		if(arg[0]!='-'){break;}
		else if(arg.rfind("-o")==0)
		{
			if(i+1<argc && argv[i+1][0]!='-')
			{
				outPath = argv[i + 1];
			}
			else
			{
				error("-o needs an argument");
			}
			i++;
		}
		else if(arg.rfind("-place=")==0)
		{
			int ind = arg.find('@');
			if(ind==string::npos)
			{
				error("-place must be in format -place<section>@<location>");
			}
			try
			{
				places.emplace_back(stoi(arg.substr(ind + 1), nullptr, 0),arg.substr(7, ind - 7));
			}
			catch(std::invalid_argument&)
			{
				error("-place must be in format -place<section>@<location>");
			}
		}
		else if(arg.rfind("-hex")==0){hex=true;}
		else if(arg.rfind("-relocateable")==0){relocateable=true;}
		else
		{
			error("invalid option "+arg);
		}
	}

	if(!hex && !relocateable)
	{
		error("neither -hex nor -relocateable specified");
	}
	else if(hex && relocateable)
	{
		error("both -hex and -relocateable specified");
	}

	if(i==argc)
	{
		error("invalid number of arguments");
	}

	if(outPath.empty())
	{
		outPath=string(argv[i]).substr(0,string(argv[i]).find_last_of('.'));
		if(hex)
		{
			outPath.append(".hex");
		}
		else
		{
			outPath.append(".o");
		}
	}

	for (; i < argc; ++i)
	{
		ifstream inFile(argv[i],ifstream::binary);
		if(inFile.fail())
		{
			error("cant open "+string(argv[i]));
		}
		cxs.push_back(loadVlah(inFile));
		inFile.close();
	}

	sort(places.begin(),places.end());
	if(hex)
	{
		vector<segment> segments=placeSections(cxs,places);
		doRelocations(cxs);
		outputHex(outPath,cxs,segments);
	}
	else
	{
		linkerContext outputContext=mergeSymbols(cxs);
		saveVlah(outPath,outputContext,cxs);
	}

	return 0;
}

void error(const string& msg)
{
	cout<<"Error: "<<msg<<endl;
	exit(-1);
}

linkerContext loadVlah(ifstream& file)
{
	linkerContext lc;
	Vlah_header header;
	file.read((char*)&header,sizeof(Vlah_header));
	if (header.ident[0]!=VLAH_MAGIC_0 ||
	    header.ident[1]!=VLAH_MAGIC_1 ||
	    header.ident[2]!=VLAH_MAGIC_2 ||
	    header.ident[3]!=VLAH_MAGIC_3)
	{
		error("not a VLAH file");
	}
	unsigned char secNum=header.sections;

	Vlah_section sections[secNum];
	file.read((char*)&sections,sizeof(Vlah_section)*secNum);
	unsigned int baseOffset=sizeof(Vlah_header)+sizeof(Vlah_section)*secNum;

	if(sections[secNum-1].type!=VLAH_SECTION_TYPE_STRTAB)
	{
		error("section is not a string section");
	}
	char shstrtab[sections[secNum-1].size];
	file.seekg(baseOffset+sections[secNum-1].offset);
	file.read(shstrtab,sections[secNum-1].size);

	if(sections[1].type!=VLAH_SECTION_TYPE_STRTAB)
	{
		error("section is not a string section");
	}
	char strtab[sections[1].size];
	file.seekg(baseOffset+sections[1].offset);
	file.read(strtab,sections[1].size);

	lc.symbols.resize(sections[0].size/sizeof(Vlah_symbol));
	lc.symbolNameVector.resize(sections[0].size/sizeof(Vlah_symbol));
	if(sections[0].type!=VLAH_SECTION_TYPE_SYMTAB)
	{
		error("section is not a symbol table");
	}
	file.seekg(baseOffset+sections[0].offset);
	file.read((char*)lc.symbols.data(),sections[0].size);
	for (int i = 0; i < lc.symbols.size(); ++i)
	{
		lc.symbolNameMap[strtab+lc.symbols[i].name]=i;
		lc.symbolNameVector[i]=strtab+lc.symbols[i].name;
	}

	for (int i = 2; i < secNum-1; ++i)
	{
		switch (sections[i].type)
		{
			case VLAH_SECTION_TYPE_DATA:
			{
				lc.sectionData[shstrtab+sections[i].name].resize(sections[i].size);
				file.seekg(baseOffset+sections[i].offset);
				file.read((char*)lc.sectionData[shstrtab+sections[i].name].data(),sections[i].size);
				break;
			}
			case VLAH_SECTION_TYPE_RELA:
			{
				lc.relocations[shstrtab+(sections[sections[i].link].name)].resize(sections[i].size/sizeof(Vlah_rela));
				file.seekg(baseOffset+sections[i].offset);
				file.read((char*)lc.relocations[shstrtab+(sections[sections[i].link].name)].data(),sections[i].size);
				break;
			}
			default: {error("invalid section type"); break;}
		}
	}
	return lc;
}

vector<segment> placeSections(vector<linkerContext>& cxs,vector<pair<int,string>>& places)
{
	vector<segment> segments;
	unordered_set<string> placed;
	int pos=0;

	for (int i = 0; i < places.size(); ++i)
	{
		if(placed.count(places[i].second)==0)
		{
			if(places[i].first<pos)
			{
				error("segment:"+places[i].second+" overlaps segment: "+places[i-1].second);
			}
			segment seg={places[i].second,places[i].first,places[i].first};

			for (int j = 0; j < cxs.size(); ++j)
			{
				if(cxs[j].symbolNameMap.count(seg.name)==1 &&
				   (cxs[j].symbols[cxs[j].symbolNameMap[seg.name]].info&0xF)==VLAH_SYMBOL_SECTION)
				{
					cxs[j].symbols[cxs[j].symbolNameMap[seg.name]].value=seg.end;
					updateSymTab(cxs[j].symbols,cxs[j].symbolNameMap[seg.name],seg.end);
					seg.end+=cxs[j].symbols[cxs[j].symbolNameMap[seg.name]].size;
				}
			}

			if(seg.start!=seg.end)
			{
				pos=seg.end;
				segments.push_back(seg);
			}
			placed.insert(places[i].second);
		}
		else
		{
			error("multiple place arguments for section: "+places[i].second);
		}
	}

	for (int f = 0; f < cxs.size(); ++f)
	{
		for (int i = 0; i < cxs[f].symbols.size(); ++i)
		{
			if (placed.count(cxs[f].symbolNameVector[i]) == 0 &&
				(cxs[f].symbols[i].info&0xF)==VLAH_SYMBOL_SECTION)
			{
				segment seg = {cxs[f].symbolNameVector[i], pos, pos+cxs[f].symbols[i].size};
				cxs[f].symbols[i].value=pos;
				updateSymTab(cxs[f].symbols,cxs[f].symbolNameMap[seg.name],pos);

				for (int j = f+1; j < cxs.size(); ++j)
				{
					if (cxs[j].symbolNameMap.count(seg.name) == 1 &&
					    (cxs[j].symbols[cxs[j].symbolNameMap[seg.name]].info & 0xF) == VLAH_SYMBOL_SECTION)
					{
						cxs[j].symbols[cxs[j].symbolNameMap[seg.name]].value=seg.end;
						updateSymTab(cxs[j].symbols,cxs[j].symbolNameMap[seg.name],seg.end);
						seg.end += cxs[j].symbols[cxs[j].symbolNameMap[seg.name]].size;
					}
				}

				pos = seg.end;
				segments.push_back(seg);
				placed.insert(seg.name);
			}
		}
	}

	return segments;
}

void updateSymTab(vector<Vlah_symbol>& symbols,int ind,int val)
{
	for (int i = 0; i < symbols.size(); ++i)
	{
		if(symbols[i].info>127 && symbols[i].section==ind)
		{
			symbols[i].value+=val;
		}
	}
}

void doRelocations(vector<linkerContext>& cxs)
{
	for (int f = 0; f < cxs.size(); ++f)
	{
		for (auto& it:cxs[f].relocations)
		{
			for (int i = 0; i < it.second.size(); ++i)
			{
				Vlah_rela& rela=it.second[i];
				int val;
				if(cxs[f].symbols[rela.symbol].section==VLAH_SYMBOL_EXT)
				{
					val= findSymVal(cxs,cxs[f].symbolNameVector[rela.symbol]);
					if(val==-1)
					{
						error(" undefined symbol: "+cxs[f].symbolNameVector[rela.symbol]);
					}
				}
				else
				{
					val=cxs[f].symbols[rela.symbol].value;
				}
				val+=rela.addend;
				if(rela.info==R_16_BE_PCA)
				{
					val-=rela.offset+cxs[f].symbols[cxs[f].symbolNameMap[it.first]].value;
				}
				if(rela.info<16)
				{
					cxs[f].sectionData[it.first][rela.offset]=val&0xFF;
					cxs[f].sectionData[it.first][rela.offset+1]=(val>>8)&0xFF;
				}
				else
				{
					cxs[f].sectionData[it.first][rela.offset]=(val>>8)&0xFF;
					cxs[f].sectionData[it.first][rela.offset+1]=val&0xFF;
				}
			}
		}
	}
}

int findSymVal(vector<linkerContext>& cxs,string& name)
{
	bool found=false;
	int val=-1;
	for (int f = 0; f < cxs.size(); ++f)
	{
		for (int i = 0; i < cxs[f].symbols.size(); ++i)
		{
			Vlah_symbol& sym=cxs[f].symbols[i];

			if(cxs[f].symbolNameVector[i]==name && sym.info>127 && sym.section!=VLAH_SYMBOL_EXT)
			{
				if(!found)
				{
					val=sym.value;
					found=true;
				}
				else
				{
					error(" multiple definitions of global symbol: "+name);
				}
			}
		}
	}
	return val;
}

void outputHex(string& outPath,vector<linkerContext>& cxs,vector<segment>& segments)
{
	std::ofstream outFile (outPath);
	outFile<<hex<<uppercase<<setfill('0');

	int adr=0;

	for (int i = 0; i < segments.size(); ++i)
	{
		adr=segments[i].start;
		if((i==0 || segments[i-1].end!=adr) && adr%8!=0)
		{
			adr=(adr/8)*8;
			header(adr, outFile);
			while(adr<segments[i].start)
			{
				outFile<<setw(2)<<0<<" ";
				adr++;
			}
		}
		for (int j = 0; j < cxs.size(); ++j)
		{
			if(cxs[j].sectionData.count(segments[i].name)!=0)
			{
				vector<unsigned char>&  data=cxs[j].sectionData[segments[i].name];
				for (int k = 0; k < data.size(); ++k)
				{
					header(adr,outFile);
					outFile<<setw(2)<<(int)data[k]<<" ";
					adr++;
				}
			}
		}
	}

	outFile<<endl;
	outFile.close();
	if(outFile.fail())
	{
		error("cant write to: "+outPath);
	}
}

void header(int adr,ofstream& file)
{
	static bool first=true;
	if(adr%8==0)
	{
		if(!first)
		{
			file<<endl;
		}
		file<<setw(4)<<adr<<": ";
	}
	first=false;
}

linkerContext mergeSymbols(vector<linkerContext>& cxs)
{
	vector<Vlah_symbol> symbols;
	unordered_map<string,int> symbolNameMap;
	vector<string> symbolNameVector;
	unordered_map<string,vector<Vlah_rela>> relocations;

	symbols.push_back({0,0,0,0,0});
	symbolNameVector.emplace_back("");

	int ind=1;

	for (int f = 0; f < cxs.size(); ++f)
	{
		for (int i = 1; i < cxs[f].symbols.size(); ++i)
		{
			Vlah_symbol sym=cxs[f].symbols[i];
			string name=cxs[f].symbolNameVector[i];

			if(sym.info==VLAH_SYMBOL_SECTION)
			{
				int olds=sym.section;
				if(symbolNameMap.count(name)==0)
				{
					sym.section=ind;
					symbols.push_back(sym);
					symbolNameVector.push_back(name);
					if(sym.info>127 || (sym.info&0xF)==VLAH_SYMBOL_SECTION)
					{
						symbolNameMap[name] = ind;
					}
					ind++;
				}
				else
				{
					sym.section=symbolNameMap[name];
					sym.value=symbols[sym.section].size;
					cxs[f].symbols[i].value=symbols[sym.section].size;
					symbols[sym.section].size+=sym.size;
				}
				for (int j = 1; j < cxs[f].symbols.size(); ++j)
				{
					if(cxs[f].symbols[j].section==olds && cxs[f].symbols[j].info!=VLAH_SYMBOL_SECTION)
					{
						Vlah_symbol newSym=cxs[f].symbols[j];
						newSym.section=sym.section;
						newSym.value += sym.value;
						symbols.push_back(newSym);
						symbolNameVector.push_back(cxs[f].symbolNameVector[j]);
						if(newSym.info>127)
						{
							if(symbolNameMap.count(cxs[f].symbolNameVector[j])==0)
							{
								symbolNameMap[cxs[f].symbolNameVector[j]] = ind;
							}
							else
							{
								error("multiple definitions of symbol: "+cxs[f].symbolNameVector[j]);
							}
						}
						ind++;
					}
				}
			}
			else if(sym.section==VLAH_SYMBOL_UND)
			{
				error("undefined symbol: "+name);
			}
			else if(sym.section==VLAH_SYMBOL_EXT)
			{
				if(findSymVal(cxs,name)==-1)
				{
					symbols.push_back(sym);
					symbolNameVector.push_back(name);
					symbolNameMap[name]=ind;
					ind++;
				}
			}
			else if(sym.section==VLAH_SYMBOL_ABS)
			{
				symbols.push_back(sym);
				symbolNameVector.push_back(name);
				if(sym.info>127)
				{
					if(symbolNameMap.count(name)==0)
					{
						symbolNameMap[name]=ind;
					}
					else
					{
						error("multiple definitions of symbol: "+name);
					}
				}
				ind++;
			}
		}

	}
	for (int f = 0; f < cxs.size(); ++f)
	{
		for (auto &it: cxs[f].relocations)
		{
			for (int i = 0; i < it.second.size(); i++)
			{
				it.second[i].symbol = symbolNameMap[cxs[f].symbolNameVector[it.second[i].symbol]];
				it.second[i].offset += cxs[f].symbols[cxs[f].symbolNameMap[it.first]].value;
				relocations[it.first].push_back(it.second[i]);
			}
		}
	}
	return {unordered_map<string,vector<unsigned char>>(),symbols,symbolNameMap,symbolNameVector,relocations};
}

void saveVlah(string& outPath,linkerContext& outputContext,vector<linkerContext>&cxs)
{
	std::ofstream outFile (outPath, std::ofstream::binary);
	char sep=0;

	Vlah_header header;
	vector<Vlah_section> sections;
	std::string symstrpool="";
	std::string strpool="";
	unsigned int offset=0;

	header.ident[0]=VLAH_MAGIC_0;
	header.ident[1]=VLAH_MAGIC_1;
	header.ident[2]=VLAH_MAGIC_2;
	header.ident[3]=VLAH_MAGIC_3;
	header.sections=0;

	for (int i=1;i<outputContext.symbols.size();i++)
	{
		outputContext.symbols[i].name = symstrpool.size();
		symstrpool.append(outputContext.symbolNameVector[i]);
		symstrpool.append(&sep,1);
	}

	sections.push_back({(unsigned int)strpool.size(),offset,
	                    (unsigned short)(sizeof(Vlah_symbol)*outputContext.symbols.size()),VLAH_SECTION_TYPE_SYMTAB,0});
	strpool.append(".symtab");
	strpool.append(&sep,1);
	header.sections++;
	offset+=sizeof(Vlah_symbol)*outputContext.symbols.size();

	sections.push_back({(unsigned int)strpool.size(),offset,
	                    (unsigned short)(symstrpool.size()),VLAH_SECTION_TYPE_STRTAB,0});
	strpool.append(".strtab");
	strpool.append(&sep,1);
	header.sections++;
	offset+=symstrpool.size();

	for (int i = 1; i < outputContext.symbols.size(); ++i)
	{
		if(outputContext.symbols[i].info==VLAH_SYMBOL_SECTION)
		{
			Vlah_section s;
			s.name=strpool.size();
			strpool.append(outputContext.symbolNameVector[i]);
			strpool.append(&sep,1);
			s.type=VLAH_SECTION_TYPE_DATA;
			s.offset=offset;
			s.size=outputContext.symbols[i].size;
			s.link=0;
			header.sections++;
			offset+=s.size;
			sections.push_back(s);

			if(outputContext.relocations.count(outputContext.symbolNameVector[i])==1)
			{
				s.name=strpool.size();
				strpool.append(".rela"+outputContext.symbolNameVector[i]);
				strpool.append(&sep,1);
				s.type=VLAH_SECTION_TYPE_RELA;
				s.offset=offset;
				s.size=sizeof(Vlah_rela)*outputContext.relocations[outputContext.symbolNameVector[i]].size();
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
	outFile.write((char*)outputContext.symbols.data(),sizeof(Vlah_symbol)*outputContext.symbols.size());
	outFile.write(symstrpool.data(),symstrpool.size());

	for (int i = 1; i < outputContext.symbols.size(); ++i)
	{
		if(outputContext.symbols[i].info==VLAH_SYMBOL_SECTION)
		{
			for (int j = 0; j < cxs.size(); ++j)
			{
				if(cxs[j].sectionData.count(outputContext.symbolNameVector[i])!=0)
				{
					outFile.write((char*)cxs[j].sectionData[outputContext.symbolNameVector[i]].data(),cxs[j].sectionData[outputContext.symbolNameVector[i]].size());
				}
			}

			if(outputContext.relocations.count(outputContext.symbolNameVector[i])==1)
			{
				outFile.write((char*)outputContext.relocations[outputContext.symbolNameVector[i]].data(),sizeof(Vlah_rela)*outputContext.relocations[outputContext.symbolNameVector[i]].size());
			}
		}
	}

	outFile.write(strpool.data(),strpool.size());

	outFile.close();
	if(outFile.fail())
	{
		error("cant write to: "+outPath);
	}
}