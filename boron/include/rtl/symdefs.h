#ifndef BORON_RTL_SYMDEFS_H
#define BORON_RTL_SYMDEFS_H

#include <main.h>

// Note. Two qwords is how we define it in the assembly version
typedef struct KSYMBOL_tag
{
	uint64_t Address;
	const char* Name;
}
KSYMBOL, *PKSYMBOL;

typedef const KSYMBOL* PCKSYMBOL;

extern const KSYMBOL KiSymbolTable[];
extern const KSYMBOL KiSymbolTableEnd[];

#define KiSymbolTableSize (KiSymbolTableEnd - KiSymbolTable)

#endif//BORON_RTL_SYMDEFS_H
