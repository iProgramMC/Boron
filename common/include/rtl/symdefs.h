/***
	The Boron Operating System
	Copyright (C) 2023-2024 iProgramInCpp

Module name:
	rtl/symdefs.h
	
Abstract:
	This header file contains the structure definitions for the
	symbol table for Boron.
	
Author:
	iProgramInCpp - 20 October 2023
***/
#ifndef BORON_RTL_SYMDEFS_H
#define BORON_RTL_SYMDEFS_H

#include <main.h>

#ifdef KERNEL

// Note. Two qwords is how we define it in the assembly version
typedef struct KSYMBOL_tag
{
	uintptr_t Address;
	uintptr_t Size;
	const char* Name;
}
KSYMBOL, *PKSYMBOL;

typedef const KSYMBOL* PCKSYMBOL;

extern const KSYMBOL KiSymbolTable[];
extern const KSYMBOL KiSymbolTableEnd[];

#define KiSymbolTableSize (KiSymbolTableEnd - KiSymbolTable)

#endif

#endif//BORON_RTL_SYMDEFS_H
