#ifndef SSPROJ_VLAH_HPP
#define SSPROJ_VLAH_HPP

#define VLAH_IDENT_SIZE 4
#define VLAH_MAGIC_0 'V'
#define VLAH_MAGIC_1 'L'
#define VLAH_MAGIC_2 'A'
#define VLAH_MAGIC_3 'H'

typedef struct
{
	unsigned char ident[VLAH_IDENT_SIZE];
	unsigned char sections;
} Vlah_header;

#define VLAH_SECTION_TYPE_DATA 1
#define VLAH_SECTION_TYPE_SYMTAB 2
#define VLAH_SECTION_TYPE_STRTAB 3
#define VLAH_SECTION_TYPE_RELA 4

typedef struct
{
	unsigned int name;
	unsigned int offset;
	unsigned short size;
	unsigned char type;
	unsigned char link;
} Vlah_section;

#define VLAH_SYMBOL_GLOBAL  128
#define VLAH_SYMBOL_LOCAL   0

#define VLAH_SYMBOL_NOTYPE 0
#define VLAH_SYMBOL_SECTION 3
#define VLAH_SYMBOL_EQU 4

#define VLAH_SYMBOL_UND     0
#define VLAH_SYMBOL_EXT     254
#define VLAH_SYMBOL_ABS     255

typedef struct
{
	unsigned int name;
	unsigned short value;
	unsigned short size;
	unsigned char info;
	unsigned char section;
} Vlah_symbol;

#define R_16_LE_DA 1
#define R_16_BE_DA 16
#define R_16_BE_PCA 18

typedef struct
{
	unsigned short offset;
	unsigned short symbol;
	short addend;
	unsigned char info;
} Vlah_rela;

#endif //SSPROJ_VLAH_HPP
