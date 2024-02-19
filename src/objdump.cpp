#include <iostream>
#include <fstream>
#include <iomanip>
#include "vlah.hpp"

using namespace std;

void error(const string& msg);
void printVlah(ifstream& file);
int printVlahHeader(Vlah_header& header);
string readStrTab(Vlah_section& sec,unsigned int BO,ifstream& file);
void readSymTab(Vlah_symbol* arr,Vlah_section& sec,unsigned int BO,ifstream& file);
void printSectionHeader(Vlah_section* sections,int i,string& shstrtab);
void printSymTab(Vlah_symbol* symtab,int size,string& strtab);
void printData(Vlah_section& section,unsigned int BO,ifstream& file);
void printRela(Vlah_section& section,unsigned int BO,ifstream& file,Vlah_symbol* symtab,int size,string& strtab);

int main(int argc,char** argv)
{
	string inPath;
	if(argc==2)
	{
		inPath=argv[1];
	}
	else
	{
		error("invalid number of arguments");
	}

	ifstream inFile(inPath,ifstream::binary);

	if(inFile.fail())
	{
		error("cant open "+inPath);
	}

	printVlah(inFile);

	inFile.close();

	return 0;
}

void error(const string& msg)
{
	cout<<"Error: "<<msg<<endl;
	exit(-1);
}

void printVlah(ifstream& file)
{
	Vlah_header header;
	file.read((char*)&header,sizeof(Vlah_header));
	unsigned char secNum=printVlahHeader(header);

	Vlah_section sections[secNum];
	file.read((char*)&sections,sizeof(Vlah_section)*secNum);
	unsigned int baseOffset=sizeof(Vlah_header)+sizeof(Vlah_section)*secNum;

	string shstrtab=readStrTab(sections[secNum-1],baseOffset,file);
	string strtab=readStrTab(sections[1],baseOffset,file);

	Vlah_symbol symTab[sections[0].size/sizeof(Vlah_symbol)];
	readSymTab(symTab,sections[0],baseOffset,file);

	printSectionHeader(sections,0,shstrtab);
	printSymTab(symTab,sections[0].size/sizeof(Vlah_symbol),strtab);
	cout<<endl;

	for (int i = 2; i < secNum-1; ++i)
	{
		printSectionHeader(sections,i,shstrtab);
		switch (sections[i].type)
		{
			case VLAH_SECTION_TYPE_DATA: {printData(sections[i],baseOffset,file); break;}
			case VLAH_SECTION_TYPE_RELA: {printRela(sections[i],baseOffset,file,symTab,sections[0].size/sizeof(Vlah_symbol),strtab); break;}
			default:                     {error("invalid section type"); break;}
		}
		cout<<endl;
	}
}

int printVlahHeader(Vlah_header& header)
{
	if (header.ident[0]!=VLAH_MAGIC_0 ||
		header.ident[1]!=VLAH_MAGIC_1 ||
		header.ident[2]!=VLAH_MAGIC_2 ||
		header.ident[3]!=VLAH_MAGIC_3)
	{
		error("not a VLAH file");
	}
	cout<<"VLAH file, "<<(int)header.sections<<" sections"<<endl<<endl;
	return header.sections;
}

string readStrTab(Vlah_section& sec,unsigned int BO,ifstream& file)
{
	if(sec.type!=VLAH_SECTION_TYPE_STRTAB)
	{
		error("section is not a string section");
	}
	char str[sec.size];
	file.seekg(BO+sec.offset);
	file.read(str,sec.size);
	return {str,sec.size};
}

void readSymTab(Vlah_symbol* arr,Vlah_section& sec,unsigned int BO,ifstream& file)
{
	if(sec.type!=VLAH_SECTION_TYPE_SYMTAB)
	{
		error("section is not a symbol table");
	}
	file.seekg(BO+sec.offset);
	file.read((char*)arr,sec.size);
}

void printSectionHeader(Vlah_section* sections,int i,string& shstrtab)
{
	cout<<"Section: "<<shstrtab.data()+sections[i].name<<" Type: ";
	switch (sections[i].type)
	{
		case VLAH_SECTION_TYPE_DATA:{cout<<"DATA"<<endl; break;}
		case VLAH_SECTION_TYPE_SYMTAB:{cout<<"SYMTAB"<<endl; break;}
		case VLAH_SECTION_TYPE_STRTAB:{cout<<"STRTAB"<<endl; break;}
		case VLAH_SECTION_TYPE_RELA:{cout<<"RELA";
			int j=sections[i].link;
			if(sections[j].type!=VLAH_SECTION_TYPE_DATA)
			{
				error("rela for non data section");
			}
			else
			{
				cout<<" for section: "<<shstrtab.data()+sections[j].name<<endl;
			}
			break;}
		default:{cout<<endl; error("invalid section type"); break;}
	}
}

void printSymTab(Vlah_symbol* symtab,int size,string& strtab)
{
	cout<<"Num | Val  | Size  | Type  | Bind | Ndx | Name"<<endl;
	for (int i = 0; i < size; ++i)
	{
		cout<<setw(3)<<i<<" | ";
		cout<<setw(4)<<setfill('0')<<hex<<uppercase<<symtab[i].value<<" | ";
		cout<<setw(5)<<setfill(' ')<<dec<<symtab[i].size<<" | ";

		switch (symtab[i].info&0xF)
		{
			case VLAH_SYMBOL_NOTYPE:  {cout<<"NOTYP"; break;}
			case VLAH_SYMBOL_SECTION: {cout<<"SCTN "; break;}
			case VLAH_SYMBOL_EQU:     {cout<<"EQU  "; break;}
			default:                  {cout<<setw(5)<<(int)symtab[i].info; break;}
		}
		cout<<" | ";

		if(symtab[i].info<128)
		{
			cout<<"LOC ";
		}
		else
		{
			cout<<"GLOB";
		}
		cout<<" | ";

		switch (symtab[i].section)
		{
			case VLAH_SYMBOL_UND: {cout<<"UND"; break;}
			case VLAH_SYMBOL_EXT: {cout<<"EXT"; break;}
			case VLAH_SYMBOL_ABS: {cout<<"ABS"; break;}
			default:              {cout<<setw(3)<<(int)symtab[i].section; break;}
		}
		cout<<" | ";

		if(i!=0)
		{
			cout<<strtab.data()+symtab[i].name;
		}
		cout<<endl;
	}
}

void printData(Vlah_section& section,unsigned int BO,ifstream& file)
{
	file.seekg(BO+section.offset);
	unsigned char data;
	for (int i = 0; i < section.size; i+=8)
	{
		for (int j = 0; j < 8 && i+j < section.size; ++j)
		{
				file.read((char*)&data,1);
				cout<<setw(2)<<setfill('0')<<hex<<uppercase<<(int)data<<" ";
				if(j==3){cout<<" ";}
		}
		cout<<endl;
	}
	cout<<dec<<setfill(' ');
}

void printRela(Vlah_section& section,unsigned int BO,ifstream& file,Vlah_symbol* symtab,int size,string& strtab)
{
	file.seekg(BO+section.offset);
	cout<<"Offs | TYPE        | Addend | Symbol"<<endl;
	for (int i = 0; i < section.size; i+=sizeof(Vlah_rela))
	{
		Vlah_rela rela;
		file.read((char*)&rela,sizeof(Vlah_rela));

		cout<<setw(4)<<setfill('0')<<hex<<uppercase<<rela.offset<<" | ";
		cout<<dec<<setfill(' ');

		switch (rela.info)
		{
			case R_16_LE_DA:  {cout<<"R_16_LE_DA "; break;}
			case R_16_BE_DA:  {cout<<"R_16_BE_DA "; break;}
			case R_16_BE_PCA: {cout<<"R_16_BE_PCA"; break;}
			default:          {cout<<setw(11)<<(int)rela.info; break;}
		}
		cout<<" | ";

		cout<<setw(6)<<rela.addend<<" | ";

		if(rela.symbol>=size)
		{
			cout<<endl; error("invalid relocation symbol");
		}

		cout<<setw(3)<<rela.symbol<<" ("<<strtab.data()+symtab[rela.symbol].name<<")"<<endl;

	}
}