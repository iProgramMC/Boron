/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ldr/ldri.h
	
Abstract:
	This header file contains forward declarations for
	DLL loader internals.
	
Author:
	iProgramInCpp - 22 October 2023
***/
#ifndef BORON_LDRI_H
#define BORON_LDRI_H

#include <ke.h>
#include <mm.h>
#include <ldr.h>
#include <string.h>
#include <limreq.h>
#include <elf.h>

// Struct not part of the ELF format, but part of the loader.
typedef struct ELF_DYNAMIC_INFO_tag
{
	const char *DynStrTable;
	PELF_SYMBOL DynSymTable;
	uintptr_t  *GlobalOffsetTable;
	size_t      GlobalOffsetTableSize;
	const void *PltRelocations;
	size_t      PltRelocationCount;
	ELF_RELA   *RelaEntries;
	size_t      RelaCount;
	ELF_REL    *RelEntries;
	size_t      RelCount;
	bool        PltUsesRela;
	const char *StringTable;
	PELF_SYMBOL SymbolTable;
	size_t      SymbolTableSize;
}
ELF_DYNAMIC_INFO, *PELF_DYNAMIC_INFO;

void LdriLoadDll(PLIMINE_FILE File);

uintptr_t LdrAllocateRange(size_t Size);

#endif//BORON_LDRI_H