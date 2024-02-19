%{
	#include <cstdio>
	#include <cstring>
	#include "assembler.hpp"

	int regNum(char* str);
%}
%option noyywrap
%option yylineno
%option nodefault
%option warn
REG (r[0-7])|sp|pc|psw
INTLIT [1-9][0-9]*
HEXLIT 0[xX][0-9A-Fa-f]*
OCTLIT 0[0-7]*
LIT {INTLIT}|{HEXLIT}|{OCTLIT}
STRING ["].*["]
IDENT [a-zA-Z_\.][a-zA-Z_\.0-9]*
%%
{IDENT}:                {yylval.stringv = strdup(yytext); yylval.stringv[strlen(yylval.stringv)-1]='\0'; return LABEL;}
halt                    {return HALT;}
int                     {return INT;}
iret                    {return IRET;}
call                    {return CALL;}
ret                     {return RET;}
jmp                     {return JMP;}
jeq                     {return JEQ;}
jne                     {return JNE;}
jgt                     {return JGT;}
push                    {return PUSH;}
pop                     {return POP;}
xchg                    {return XCHG;}
add                     {return ADD;}
sub                     {return SUB;}
mul                     {return MUL;}
div                     {return DIV;}
cmp                     {return CMP;}
not                     {return NOT;}
and                     {return AND;}
or                      {return OR;}
xor                     {return XOR;}
test                    {return TEST;}
shl                     {return SHL;}
shr                     {return SHR;}
ldr                     {return LDR;}
str                     {return STR;}
\.global                {return GLOBAL;}
\.extern                {return EXTERN;}
\.section               {return SECTION;}
\.word                  {return WORD;}
\.skip                  {return SKIP;}
\.ascii                 {return ASCII;}
\.equ                   {return EQU;}
\.end                   {return END;}
{REG}                   {yylval.intv = regNum(yytext); return REG;}
{LIT}                   {sscanf(yytext, "%i", &yylval.intv); return LIT;}
{STRING}                {yylval.stringv = strdup(yytext); return STRING;}
{IDENT}                 {yylval.stringv = strdup(yytext); return SYM;}
\$                      {return '$';}
%                       {return '%';}
\[                      {return '[';}
\]                      {return ']';}
\(                      {return '(';}
\)                      {return ')';}
\+                      {return '+';}
\-                      {return '-';}
\*                      {return '*';}
,                       {return ',';}
[ \t\f]                 {}
#[^\n]*                 {}
\n                      {return ENDL;}
.                       {char errmsg[20] = "unexpected token: "; errmsg[18]=yytext[0]; errmsg[19]='\0'; yyerror(nullptr,errmsg);}
%%
int regNum(char* str)
{
	int val=-1;
	if(str[0]=='r' && strlen(str)==2)
	{
		val=str[1]-'0';
	}
	else if(strcmp("sp",str)==0)
	{
		val=6;
	}
	else if(strcmp("pc",str)==0)
	{
		val=7;
	}
	else if(strcmp("psw",str)==0)
	{
		val=8;
	}

	if(val<0 || val>8)
	{
		char* error=(char*)malloc(strlen(str)+strlen("Invalid register: "));
		error[0]='\0';
		strcat(error,"Invalid register: ");
		strcat(error,str);
		yyerror(error,nullptr);
		free(error);
		return -1;
	}

	return val;
}