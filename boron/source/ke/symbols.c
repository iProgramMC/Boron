/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/symbols.c
	
Abstract:
	This module contains the implementation for the kernel
	symbol lookup routines.
	
Author:
	iProgramInCpp - 24 October 2023
***/
#include <ke.h>
#include <string.h>
#include <rtl/symdefs.h>

// TODO: The lookups could be optimized with a sorted list and a binary search.

uintptr_t DbgLookUpAddress(const char* Name)
{
	for (PCKSYMBOL Symbol = KiSymbolTable; Symbol != KiSymbolTableEnd; Symbol++)
	{
		if (strcmp(Symbol->Name, Name) == 0)
			return Symbol->Address;
	}
	
	return 0;
}

const char* DbgLookUpRoutineNameByAddressExact(uintptr_t Address)
{
	for (PCKSYMBOL Symbol = KiSymbolTable; Symbol != KiSymbolTableEnd; Symbol++)
	{
		if (Symbol->Address == Address)
			return Symbol->Name;
	}
	
	return NULL;
}

const char* DbgLookUpRoutineNameByAddress(uintptr_t Address, uintptr_t* BaseAddressOut)
{
	for (PCKSYMBOL Symbol = KiSymbolTable; Symbol != KiSymbolTableEnd; Symbol++)
	{
		if (Symbol->Address <= Address && Address < Symbol->Address + Symbol->Size)
		{
			*BaseAddressOut = Symbol->Address;
			return Symbol->Name;
		}
	}
	
	return NULL;
}
