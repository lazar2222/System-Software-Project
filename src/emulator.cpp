#include <iostream>
#include <cstdio>
#include <unistd.h>
#include <termios.h>
#include <fstream>
#include <iomanip>
#include <bitset>
#include <unordered_map>
#include <chrono>

#define PAGE(x) (((x)>>8)&0xFF)
#define OFFS(x) ((x)&0xFF)
#define HINIB(x) (((x)>>4)&0xF)
#define LONIB(x) ((x)&0xF)
#define SETBIT(x,y,z) (x)=((x)&(~(y)))|((y)*(z))


#define PSW_I 0x8000
#define PSW_Tl 0x4000
#define PSW_Tr 0x2000
#define PSW_N 0x0008
#define PSW_C 0x0004
#define PSW_O 0x002
#define PSW_Z 0x001

using namespace std;

typedef struct
{
	bool running;
	unsigned short r[9];
	unsigned char* memory[255];
	unordered_map<unsigned char,unsigned short(*)(unsigned char,void*)> IOread;
	unordered_map<unsigned char,void(*)(unsigned char,unsigned short,void*)> IOwrite;
	unordered_map<unsigned char,void*> MMIOcontext;
	unordered_map<unsigned char,bool(*)(void*)> isr;
	unordered_map<unsigned char,void*> peripheral;
} cpuContext;

typedef struct
{
	unsigned long long lastInt;
	bool raiseInt;
	unsigned short reg;
	unsigned long period;
} timerContext;

typedef struct
{
	bool raiseInt;
	unsigned short rx;
} terminalContext;

void error(const string& msg);
void loadMem(cpuContext& ccx,ifstream& inFile);
unsigned char readByte(cpuContext& ccx ,unsigned short adr);
void writeByte(cpuContext& ccx ,unsigned short adr,unsigned char data);
unsigned short readWord(cpuContext& ccx ,unsigned short adr,int endianess);
void writeWord(cpuContext& ccx ,unsigned short adr,unsigned short data,int endianess);
void executeInst(cpuContext& ccx);
void interruptHandler(cpuContext& ccx,unsigned char ent);
void HALThandler(cpuContext& ccx,unsigned char instrDescr);
void INThandler(cpuContext& ccx,unsigned char instrDescr);
void IREThandler(cpuContext& ccx,unsigned char instrDescr);
void CALLhandler(cpuContext& ccx,unsigned char instrDescr);
void REThandler(cpuContext& ccx,unsigned char instrDescr);
void JMPhandler(cpuContext& ccx,unsigned char instrDescr);
void XCHGhandler(cpuContext& ccx,unsigned char instrDescr);
void ARIThandler(cpuContext& ccx,unsigned char instrDescr);
void LOGIChandler(cpuContext& ccx,unsigned char instrDescr);
void SHIFThandler(cpuContext& ccx,unsigned char instrDescr);
void LDhandler(cpuContext& ccx,unsigned char instrDescr);
void SThandler(cpuContext& ccx,unsigned char instrDescr);
void serveInterrupts(cpuContext& ccx);
bool timerInt(void* cx);
unsigned short timerRead(unsigned char adr,void* cx);
void timerWrite(unsigned char adr,unsigned short data,void* cx);
void timerUpdate(timerContext& cx);
bool terminalInt(void* cx);
unsigned short terminalRead(unsigned char adr,void* cx);
void terminalWrite(unsigned char adr,unsigned short data,void* cx);
void terminalUpdate(terminalContext& cx);

extern void (*instHandlers[16])(cpuContext& ccx,unsigned char instrDescr);

int main(int argc,char** argv)
{
	if(argc!=2)
	{
		error("invalid number of arguments");
	}

	cpuContext ccx;

	for (int i = 0; i < 255; ++i)
	{
		ccx.memory[i]= nullptr;
	}
	for (int i = 0; i < 9; ++i)
	{
		ccx.r[i]=0;
	}

	ifstream inFile(argv[1]);
	if(inFile.fail())
	{
		error("cant open "+string(argv[1]));
	}
	loadMem(ccx,inFile);
	inFile.close();

	ccx.running=true;
	ccx.r[6]=0xFF00;
	ccx.r[7]=readWord(ccx,0,LITTLE_ENDIAN);

	timerContext ticx;
	ticx.lastInt=0;
	ticx.reg=0;
	ticx.period=500;
	ticx.raiseInt=false;
	ccx.peripheral[2]=&ticx;
	ccx.isr[2]=timerInt;
	ccx.MMIOcontext[0x10]=&ticx;
	ccx.MMIOcontext[0x11]=&ticx;
	ccx.IOread[0x10]=timerRead;
	ccx.IOread[0x11]=timerRead;
	ccx.IOwrite[0x10]=timerWrite;
	ccx.IOwrite[0x11]=timerWrite;

	terminalContext tecx;
	tecx.rx=0;
	tecx.raiseInt=false;
	ccx.peripheral[3]=&tecx;
	ccx.isr[3]=terminalInt;
	ccx.MMIOcontext[0x00]=&tecx;
	ccx.MMIOcontext[0x01]=&tecx;
	ccx.MMIOcontext[0x02]=&tecx;
	ccx.MMIOcontext[0x03]=&tecx;
	ccx.IOread[0x00]=terminalRead;
	ccx.IOread[0x01]=terminalRead;
	ccx.IOread[0x02]=terminalRead;
	ccx.IOread[0x03]=terminalRead;
	ccx.IOwrite[0x00]=terminalWrite;
	ccx.IOwrite[0x01]=terminalWrite;
	ccx.IOwrite[0x01]=terminalWrite;
	ccx.IOwrite[0x03]=terminalWrite;

	struct termios initial_settings;
	struct termios new_settings;
	tcgetattr(0,&initial_settings);

	new_settings = initial_settings;
	new_settings.c_lflag &= ~ICANON;
	new_settings.c_lflag &= ~ECHO;
	new_settings.c_lflag &= ~ISIG;
	new_settings.c_cc[VMIN] = 0;
	new_settings.c_cc[VTIME] = 0;

	tcsetattr(0, TCSANOW, &new_settings);

	ticx.lastInt=chrono::duration_cast< std::chrono::milliseconds >(chrono::system_clock::now().time_since_epoch()).count();
	while(ccx.running)
	{
		//cout<<hex<<ccx.r[7]<<endl;
		executeInst(ccx);
		serveInterrupts(ccx);
		timerUpdate(ticx);
		terminalUpdate(tecx);
	}

	tcsetattr(0, TCSANOW, &initial_settings);

	cout<<endl<<"------------------------------------------------"<<endl;
	cout<<"Emulated processor executed halt instruction"<<endl;
	cout<<"Emulated processor state: psw=0b"<<bitset<16>(ccx.r[8])<<endl;
	for (int i = 0; i < 8; ++i)
	{
		cout<<"r"<<i<<"=0x"<<hex<<uppercase<<setfill('0')<<setw(4)<<ccx.r[i]<<"    ";
		if(i==3)
		{
			cout<<endl;
		}
	}
	cout<<endl;
	cout<<dec<<setfill(' ');

	for (int i = 0; i < 255; ++i)
	{
		if(ccx.memory[i]!= nullptr)
		{
			delete[] ccx.memory[i];
		}
	}
	return 0;
}

void error(const string& msg)
{
	cout<<"Error: "<<msg<<endl;
	exit(-1);
}

void loadMem(cpuContext& ccx,ifstream& inFile)
{
	string line;
	size_t rem;
	int adr;
	while(getline(inFile,line))
	{
		try
		{
			adr=stoi(line,&rem,16);
		}
		catch(invalid_argument&)
		{
			error("not a valid hex file");
		}
		if(adr<0 || PAGE(adr)>254)
		{
			error("not a valid hex file");
		}
		line=line.substr(rem+1);
		try
		{
			while (true)
			{
				int data= stoi(line,&rem,16);
				line=line.substr(rem+1);
				if(data<0 || data>255)
				{
					error("not a valid hex file");
				}
				writeByte(ccx,adr,data);
				adr++;
			}
		}
		catch(invalid_argument&){}
	}
}

unsigned char readByte(cpuContext& ccx ,unsigned short adr)
{
	if(PAGE(adr)==255)
	{
		if(ccx.IOread.count(OFFS(adr))!=0)
		{
			return ccx.IOread[OFFS(adr)](OFFS(adr),ccx.MMIOcontext[OFFS(adr)]);
		}
		return 0;
	}
	if(ccx.memory[PAGE(adr)]==nullptr)
	{
		return 0;
	}
	return ccx.memory[PAGE(adr)][OFFS(adr)];
}

void writeByte(cpuContext& ccx ,unsigned short adr,unsigned char data)
{
	if(PAGE(adr)==255)
	{
		if(ccx.IOwrite.count(OFFS(adr))!=0)
		{
			ccx.IOwrite[OFFS(adr)](OFFS(adr),data,ccx.MMIOcontext[OFFS(adr)]);
		}
		return;
	}
	if(ccx.memory[PAGE(adr)]==nullptr)
	{
		ccx.memory[PAGE(adr)]=new unsigned char[256];
	}
	ccx.memory[PAGE(adr)][OFFS(adr)]=data;
}

unsigned short readWord(cpuContext& ccx ,unsigned short adr,int endianess)
{
	return (((unsigned short)readByte(ccx,adr))<<(endianess==LITTLE_ENDIAN?0:8)) | (((unsigned short)readByte(ccx,adr+1))<<(endianess==LITTLE_ENDIAN?8:0));
}

void writeWord(cpuContext& ccx ,unsigned short adr,unsigned short data,int endianess)
{
	writeByte(ccx,adr+(endianess==LITTLE_ENDIAN?0:1), OFFS(data));
	writeByte(ccx,adr+(endianess==LITTLE_ENDIAN?1:0), PAGE(data));
}

void executeInst(cpuContext& ccx)
{
	unsigned char instrDescr=readByte(ccx,ccx.r[7]);
	ccx.r[7]++;
	if(instHandlers[HINIB(instrDescr)]== nullptr)
	{
		ccx.r[7]--;
		interruptHandler(ccx,1);
		return;
	}
	instHandlers[HINIB(instrDescr)](ccx,instrDescr);
}

void interruptHandler(cpuContext& ccx,unsigned char ent)
{
	if(ent>8)
	{
		interruptHandler(ccx,1);
		return;
	}
	writeWord(ccx,ccx.r[6]-2,ccx.r[7],LITTLE_ENDIAN);
	writeWord(ccx,ccx.r[6]-4,ccx.r[8],LITTLE_ENDIAN);
	ccx.r[6]-=4;
	ccx.r[7]=readWord(ccx,2*ent,LITTLE_ENDIAN);
}

void HALThandler(cpuContext& ccx,unsigned char instrDescr)
{
	if(instrDescr!=0x00)
	{
		ccx.r[7]--;
		interruptHandler(ccx,1);
		return;
	}
	ccx.running=false;
}

void INThandler(cpuContext& ccx,unsigned char instrDescr)
{
	if(instrDescr!=0x10)
	{
		ccx.r[7]--;
		interruptHandler(ccx,1);
		return;
	}
	unsigned char regsDescr= readByte(ccx,ccx.r[7]);
	ccx.r[7]++;
	if(LONIB(regsDescr)!=0xF)
	{
		ccx.r[7]-=2;
		interruptHandler(ccx,1);
		return;
	}
	interruptHandler(ccx, ccx.r[HINIB(regsDescr)]%8);
}

void IREThandler(cpuContext& ccx,unsigned char instrDescr)
{
	if(instrDescr!=0x20)
	{
		ccx.r[7]--;
		interruptHandler(ccx,1);
		return;
	}
	ccx.r[8]= readWord(ccx,ccx.r[6],LITTLE_ENDIAN);
	ccx.r[7]= readWord(ccx,ccx.r[6]+2,LITTLE_ENDIAN);
	ccx.r[6]+=4;
}

void CALLhandler(cpuContext& ccx,unsigned char instrDescr)
{
	if(instrDescr!=0x30)
	{
		ccx.r[7]--;
		interruptHandler(ccx,1);
		return;
	}
	unsigned char regsDescr= readByte(ccx,ccx.r[7]);
	ccx.r[7]++;
	if(HINIB(regsDescr)!=0xF || (LONIB(regsDescr)>8 && LONIB(regsDescr)<15))
	{
		ccx.r[7]-=2;
		interruptHandler(ccx,1);
		return;
	}
	unsigned char addrMode= readByte(ccx,ccx.r[7]);
	ccx.r[7]++;
	if(LONIB(addrMode)>5 || HINIB(addrMode)>4)
	{
		ccx.r[7]-=3;
		interruptHandler(ccx,1);
		return;
	}
	unsigned short data;
	if(LONIB(addrMode)!=1 && LONIB(addrMode)!=2)
	{
		data=readWord(ccx,ccx.r[7],BIG_ENDIAN);
		ccx.r[7]+=2;
	}
	if(LONIB(addrMode)==3 || LONIB(addrMode)==2)
	{
		switch (HINIB(addrMode))
		{
			case 1: {ccx.r[LONIB(regsDescr)]-=2; break;}
			case 2: {ccx.r[LONIB(regsDescr)]+=2; break;}
		}
	}
	switch (LONIB(addrMode))
	{
		case 1: {data=ccx.r[LONIB(regsDescr)]; break;}
		case 2: {data=readWord(ccx,ccx.r[LONIB(regsDescr)],LITTLE_ENDIAN); break;}
		case 3: {data=readWord(ccx,ccx.r[LONIB(regsDescr)]+data,LITTLE_ENDIAN); break;}
		case 4: {data=readWord(ccx,data,LITTLE_ENDIAN); break;}
		case 5: {data+=ccx.r[LONIB(regsDescr)]; break;}
	}
	if(LONIB(addrMode)==3 || LONIB(addrMode)==2)
	{
		switch (HINIB(addrMode))
		{
			case 3: {ccx.r[LONIB(regsDescr)]-=2; break;}
			case 4: {ccx.r[LONIB(regsDescr)]+=2; break;}
		}
	}
	writeWord(ccx,ccx.r[6]-2,ccx.r[7],LITTLE_ENDIAN);
	ccx.r[6]-=2;
	ccx.r[7]=data;
}

void REThandler(cpuContext& ccx,unsigned char instrDescr)
{
	if(instrDescr!=0x40)
	{
		ccx.r[7]--;
		interruptHandler(ccx,1);
		return;
	}
	ccx.r[7]= readWord(ccx,ccx.r[6],LITTLE_ENDIAN);
	ccx.r[6]+=2;
}

void JMPhandler(cpuContext& ccx,unsigned char instrDescr)
{
	if(HINIB(instrDescr)!=0x5 || LONIB(instrDescr)>3)
	{
		ccx.r[7]--;
		interruptHandler(ccx,1);
		return;
	}
	unsigned char regsDescr= readByte(ccx,ccx.r[7]);
	ccx.r[7]++;
	if(HINIB(regsDescr)!=0xF || (LONIB(regsDescr)>8 && LONIB(regsDescr)<15))
	{
		ccx.r[7]-=2;
		interruptHandler(ccx,1);
		return;
	}
	unsigned char addrMode= readByte(ccx,ccx.r[7]);
	ccx.r[7]++;
	if(LONIB(addrMode)>5 || HINIB(addrMode)>4)
	{
		ccx.r[7]-=3;
		interruptHandler(ccx,1);
		return;
	}
	unsigned short data;
	if(LONIB(addrMode)!=1 && LONIB(addrMode)!=2)
	{
		data=readWord(ccx,ccx.r[7],BIG_ENDIAN);
		ccx.r[7]+=2;
	}
	if(LONIB(addrMode)==3 || LONIB(addrMode)==2)
	{
		switch (HINIB(addrMode))
		{
			case 1: {ccx.r[LONIB(regsDescr)]-=2; break;}
			case 2: {ccx.r[LONIB(regsDescr)]+=2; break;}
		}
	}
	switch (LONIB(addrMode))
	{
		case 1: {data=ccx.r[LONIB(regsDescr)]; break;}
		case 2: {data=readWord(ccx,ccx.r[LONIB(regsDescr)],LITTLE_ENDIAN); break;}
		case 3: {data=readWord(ccx,ccx.r[LONIB(regsDescr)]+data,LITTLE_ENDIAN); break;}
		case 4: {data=readWord(ccx,data,LITTLE_ENDIAN); break;}
		case 5: {data+=ccx.r[LONIB(regsDescr)]; break;}
	}
	if(LONIB(addrMode)==3 || LONIB(addrMode)==2)
	{
		switch (HINIB(addrMode))
		{
			case 3: {ccx.r[LONIB(regsDescr)]-=2; break;}
			case 4: {ccx.r[LONIB(regsDescr)]+=2; break;}
		}
	}
	bool jump=true;
	switch (LONIB(instrDescr))
	{
		case 1: {jump=(ccx.r[8]&PSW_Z)!=0; break;}
		case 2: {jump=(ccx.r[8]&PSW_Z)==0; break;}
		case 3: {jump=(ccx.r[8]&PSW_Z)==0 && (((ccx.r[8]&PSW_N)==0 && (ccx.r[8]&PSW_O)==0) || ((ccx.r[8]&PSW_N)!=0 && (ccx.r[8]&PSW_O)!=0)); break;}
	}
	if(jump)
	{
		ccx.r[7]=data;
	}
}

void XCHGhandler(cpuContext& ccx,unsigned char instrDescr)
{
	if(instrDescr!=0x60)
	{
		ccx.r[7]--;
		interruptHandler(ccx,1);
		return;
	}
	unsigned char regsDescr= readByte(ccx,ccx.r[7]);
	ccx.r[7]++;
	if(LONIB(regsDescr)>8 || HINIB(regsDescr)>8)
	{
		ccx.r[7]-=2;
		interruptHandler(ccx,1);
		return;
	}
	unsigned short tmp=ccx.r[HINIB(regsDescr)];
	ccx.r[HINIB(regsDescr)]=ccx.r[LONIB(regsDescr)];
	ccx.r[LONIB(regsDescr)]=tmp;
}

void ARIThandler(cpuContext& ccx,unsigned char instrDescr)
{
	if(HINIB(instrDescr)!=0x7 || LONIB(instrDescr)>4)
	{
		ccx.r[7]--;
		interruptHandler(ccx,1);
		return;
	}
	unsigned char regsDescr= readByte(ccx,ccx.r[7]);
	ccx.r[7]++;
	if(LONIB(regsDescr)>8 || HINIB(regsDescr)>8)
	{
		ccx.r[7]-=2;
		interruptHandler(ccx,1);
		return;
	}
	switch (LONIB(instrDescr))
	{
		case 0: {ccx.r[HINIB(regsDescr)]+=ccx.r[LONIB(regsDescr)]; break;}
		case 1: {ccx.r[HINIB(regsDescr)]-=ccx.r[LONIB(regsDescr)]; break;}
		case 2: {ccx.r[HINIB(regsDescr)]*=ccx.r[LONIB(regsDescr)]; break;}
		case 3: {ccx.r[HINIB(regsDescr)]/=ccx.r[LONIB(regsDescr)]; break;}
		case 4: {
			int tmp=((int)((short)ccx.r[HINIB(regsDescr)]))-((int)((short)ccx.r[LONIB(regsDescr)]));
			short stmp=((short)ccx.r[HINIB(regsDescr)])-((short)ccx.r[LONIB(regsDescr)]);
			SETBIT(ccx.r[8],PSW_Z,(tmp&0xFFFF)==0);
			SETBIT(ccx.r[8],PSW_O,tmp!=stmp);
			SETBIT(ccx.r[8],PSW_C,(tmp&0x10000)!=0);
			SETBIT(ccx.r[8],PSW_N,(tmp&0x8000)!=0);
			break;
		}
	}
}

void LOGIChandler(cpuContext& ccx,unsigned char instrDescr)
{
	if(HINIB(instrDescr)!=0x8 || LONIB(instrDescr)>4)
	{
		ccx.r[7]--;
		interruptHandler(ccx,1);
		return;
	}
	unsigned char regsDescr= readByte(ccx,ccx.r[7]);
	ccx.r[7]++;
	if((LONIB(regsDescr)>8 && LONIB(regsDescr)<15) || HINIB(regsDescr)>8)
	{
		ccx.r[7]-=2;
		interruptHandler(ccx,1);
		return;
	}
	switch (LONIB(instrDescr))
	{
		case 0: {ccx.r[HINIB(regsDescr)]=~ccx.r[HINIB(regsDescr)]; break;}
		case 1: {ccx.r[HINIB(regsDescr)]&=ccx.r[LONIB(regsDescr)]; break;}
		case 2: {ccx.r[HINIB(regsDescr)]|=ccx.r[LONIB(regsDescr)]; break;}
		case 3: {ccx.r[HINIB(regsDescr)]^=ccx.r[LONIB(regsDescr)]; break;}
		case 4: {
			unsigned short tmp=ccx.r[HINIB(regsDescr)]&ccx.r[LONIB(regsDescr)];
			SETBIT(ccx.r[8],PSW_Z,(tmp&0xFFFF)==0);
			SETBIT(ccx.r[8],PSW_N,(tmp&0x8000)!=0);
			break;
		}
	}
}

void SHIFThandler(cpuContext& ccx,unsigned char instrDescr)
{
	if(HINIB(instrDescr)!=0x9 || LONIB(instrDescr)>1)
	{
		ccx.r[7]--;
		interruptHandler(ccx,1);
		return;
	}
	unsigned char regsDescr= readByte(ccx,ccx.r[7]);
	ccx.r[7]++;
	if(LONIB(regsDescr)>8 || HINIB(regsDescr)>8)
	{
		ccx.r[7]-=2;
		interruptHandler(ccx,1);
		return;
	}
	switch (LONIB(instrDescr))
	{
		case 0: {
			unsigned short exit = ccx.r[HINIB(regsDescr)] & (1<<(ccx.r[HINIB(regsDescr)]-1));
            SETBIT(ccx.r[8],PSW_C,exit!=0);
			ccx.r[HINIB(regsDescr)]<<=ccx.r[HINIB(regsDescr)];
			break;
		}
		case 1: {
			unsigned short exit = ccx.r[HINIB(regsDescr)] & (0x8000>>(ccx.r[HINIB(regsDescr)]-1));
			SETBIT(ccx.r[8],PSW_C,exit!=0);
			ccx.r[HINIB(regsDescr)]>>=ccx.r[LONIB(regsDescr)];
			break;
		}
	}
	SETBIT(ccx.r[8],PSW_Z,(ccx.r[HINIB(regsDescr)]&0xFFFF)==0);
	SETBIT(ccx.r[8],PSW_N,(ccx.r[HINIB(regsDescr)]&0x8000)!=0);
}

void LDhandler(cpuContext& ccx,unsigned char instrDescr)
{
	if(instrDescr!=0xA0)
	{
		ccx.r[7]--;
		interruptHandler(ccx,1);
		return;
	}
	unsigned char regsDescr= readByte(ccx,ccx.r[7]);
	ccx.r[7]++;
	if(HINIB(regsDescr)>8 || (LONIB(regsDescr)>8 && LONIB(regsDescr)<15))
	{
		ccx.r[7]-=2;
		interruptHandler(ccx,1);
		return;
	}
	unsigned char addrMode= readByte(ccx,ccx.r[7]);
	ccx.r[7]++;
	if(LONIB(addrMode)>5 || HINIB(addrMode)>4)
	{
		ccx.r[7]-=3;
		interruptHandler(ccx,1);
		return;
	}
	unsigned short data;
	if(LONIB(addrMode)!=1 && LONIB(addrMode)!=2)
	{
		data=readWord(ccx,ccx.r[7],BIG_ENDIAN);
		ccx.r[7]+=2;
	}
	if(LONIB(addrMode)==3 || LONIB(addrMode)==2)
	{
		switch (HINIB(addrMode))
		{
			case 1: {ccx.r[LONIB(regsDescr)]-=2; break;}
			case 2: {ccx.r[LONIB(regsDescr)]+=2; break;}
		}
	}
	switch (LONIB(addrMode))
	{
		case 1: {data=ccx.r[LONIB(regsDescr)]; break;}
		case 2: {data=readWord(ccx,ccx.r[LONIB(regsDescr)],LITTLE_ENDIAN); break;}
		case 3: {data=readWord(ccx,ccx.r[LONIB(regsDescr)]+data,LITTLE_ENDIAN); break;}
		case 4: {data=readWord(ccx,data,LITTLE_ENDIAN); break;}
		case 5: {data+=ccx.r[LONIB(regsDescr)]; break;}
	}
	if(LONIB(addrMode)==3 || LONIB(addrMode)==2)
	{
		switch (HINIB(addrMode))
		{
			case 3: {ccx.r[LONIB(regsDescr)]-=2; break;}
			case 4: {ccx.r[LONIB(regsDescr)]+=2; break;}
		}
	}
	ccx.r[HINIB(regsDescr)]=data;
}

void SThandler(cpuContext& ccx,unsigned char instrDescr)
{
	if(instrDescr!=0xB0)
	{
		ccx.r[7]--;
		interruptHandler(ccx,1);
		return;
	}
	unsigned char regsDescr= readByte(ccx,ccx.r[7]);
	ccx.r[7]++;
	if(HINIB(regsDescr)>8 || (LONIB(regsDescr)>8 && LONIB(regsDescr)<15))
	{
		ccx.r[7]-=2;
		interruptHandler(ccx,1);
		return;
	}
	unsigned char addrMode= readByte(ccx,ccx.r[7]);
	ccx.r[7]++;
	if(LONIB(addrMode)>5 || HINIB(addrMode)>4 || LONIB(addrMode)==0 || LONIB(addrMode)==5)
	{
		ccx.r[7]-=3;
		interruptHandler(ccx,1);
		return;
	}
	unsigned short data;
	if(LONIB(addrMode)!=1 && LONIB(addrMode)!=2)
	{
		data=readWord(ccx,ccx.r[7],BIG_ENDIAN);
		ccx.r[7]+=2;
	}
	if(LONIB(addrMode)==3 || LONIB(addrMode)==2)
	{
		switch (HINIB(addrMode))
		{
			case 1: {ccx.r[LONIB(regsDescr)]-=2; break;}
			case 2: {ccx.r[LONIB(regsDescr)]+=2; break;}
		}
	}
	switch (LONIB(addrMode))
	{
		case 1: {ccx.r[LONIB(regsDescr)]=ccx.r[HINIB(regsDescr)]; break;}
		case 2: {writeWord(ccx,ccx.r[LONIB(regsDescr)],ccx.r[HINIB(regsDescr)],LITTLE_ENDIAN); break;}
		case 3: {writeWord(ccx,ccx.r[LONIB(regsDescr)]+data,ccx.r[HINIB(regsDescr)],LITTLE_ENDIAN); break;}
		case 4: {writeWord(ccx,data,ccx.r[HINIB(regsDescr)],LITTLE_ENDIAN); break;}
	}
	if(LONIB(addrMode)==3 || LONIB(addrMode)==2)
	{
		switch (HINIB(addrMode))
		{
			case 3: {ccx.r[LONIB(regsDescr)]-=2; break;}
			case 4: {ccx.r[LONIB(regsDescr)]+=2; break;}
		}
	}
}

void serveInterrupts(cpuContext& ccx)
{
	if((ccx.r[8]&PSW_I)==0)
	{
		if((ccx.r[8]&PSW_Tr)==0 && ccx.isr.count(2)!=0)
		{
			if(ccx.isr[2](ccx.peripheral[2]))
			{
				interruptHandler(ccx,2);
				return;
			}
		}
		if((ccx.r[8]&PSW_Tl)==0 && ccx.isr.count(3)!=0)
		{
			if(ccx.isr[3](ccx.peripheral[3]))
			{
				interruptHandler(ccx, 3);
				return;
			}
		}
	}
}

bool timerInt(void* cx)
{
	auto* tcx=(timerContext*)cx;
	if(tcx->raiseInt)
	{
		tcx->raiseInt=false;
		return true;
	}
	return false;
}

unsigned short timerRead(unsigned char adr,void* cx)
{
	auto* tcx=(timerContext*)cx;
	return tcx->reg;
}

void timerWrite(unsigned char adr,unsigned short data,void* cx)
{
	auto* tcx=(timerContext*)cx;
	if(adr==0x10)
	{
		tcx->reg=data;
		switch (tcx->reg)
		{
			case 0:  {tcx->period=500; break;}
			case 1:  {tcx->period=1000; break;}
			case 2:  {tcx->period=1500; break;}
			case 3:  {tcx->period=2000; break;}
			case 4:  {tcx->period=5000; break;}
			case 5:  {tcx->period=10000; break;}
			case 6:  {tcx->period=30000; break;}
			case 7:  {tcx->period=60000; break;}
			default: {tcx->period=0; break;}
		}
	}
}

void timerUpdate(timerContext& cx)
{
	unsigned long long ms=chrono::duration_cast< std::chrono::milliseconds >(chrono::system_clock::now().time_since_epoch()).count();
	if((ms-cx.lastInt)>cx.period && cx.period!=0)
	{
		cx.lastInt=ms;
		cx.raiseInt=true;
	}
}

bool terminalInt(void* cx)
{
	auto* tcx=(terminalContext*)cx;
	if(tcx->raiseInt)
	{
		tcx->raiseInt=false;
		return true;
	}
	return false;
}

unsigned short terminalRead(unsigned char adr,void* cx)
{
	auto* tcx=(terminalContext*)cx;
	if(adr==0x02)
	{
		return tcx->rx;
	}
	return 0;
}
void terminalWrite(unsigned char adr,unsigned short data,void* cx)
{
	auto* tcx=(terminalContext*)cx;
	if(adr==0x00)
	{
		putc(data,stdout);
		fflush(stdout);
	}
}

void terminalUpdate(terminalContext& cx)
{
	char c[2];
	int nread;
	nread=read(fileno(stdin),&c,1);
	if(nread>0)
	{
		cx.raiseInt=true;
		cx.rx=c[0];
	}
}

void (*instHandlers[16])(cpuContext& ccx,unsigned char instrDescr)={
		HALThandler, INThandler, IREThandler, CALLhandler, REThandler, JMPhandler, XCHGhandler, ARIThandler,
		LOGIChandler, SHIFThandler, LDhandler, SThandler, nullptr, nullptr, nullptr, nullptr};