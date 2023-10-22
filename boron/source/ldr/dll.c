/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ldr/loader.c
	
Abstract:
	This module implements the driver DLL loader.
	
Author:
	iProgramInCpp - 22 October 2023
***/
#include "ldri.h"
#include "elf.h"

void LdriLoadDll(PLIMINE_FILE File)
{
	PELF_HEADER Header = (PELF_HEADER) File->address;
	
#ifdef DEBUG
	DbgPrint("Module %s:", File->path);
	DbgPrint("num phdrs: %d", Header->ProgramHeaderTableEntries);
	DbgPrint("num shdrs: %d", Header->SectionHeaderTableEntries);
	DbgPrint("offs phdr: %d", Header->ProgramHeaderTableOffset);
	DbgPrint("offs shdr: %d", Header->SectionHeaderTableOffset);
	DbgPrint("flags: %08x", Header->Flags);
	DbgPrint("hdrsz: %d", Header->HeaderSize);
	DbgPrint("etype: %d", Header->Type);
	DbgPrint("machi: %d", Header->Machine);
	DbgPrint("versn: %d", Header->Version);
#endif
}
