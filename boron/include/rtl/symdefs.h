#ifndef BORON_RTL_SYMDEFS_H
#define BORON_RTL_SYMDEFS_H

#include <main.h>

typedef struct KSYMBOL_tag
{
	uint64_t Address;
	const char* Name;
}
KSYMBOL, *PKSYMBOL;

extern const KSYMBOL KiSymbolTable[];
extern const KSYMBOL KiSymbolTableEnd[];

#define KiSymbolTableSize (KiSymbolTableEnd - KiSymbolTable)

#endif//BORON_RTL_SYMDEFS_H
