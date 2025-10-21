/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	rtl/elf.h
	
Abstract:
	This header file defines function prototypes for
	ELF loading helpers.
	
Author:
	iProgramInCpp - 30 April 2024
***/
#pragma once

#include <elf.h>

// Struct not part of the ELF format, but part of the loader.
typedef struct ELF_DYNAMIC_INFO_tag
{
	const char *DynStrTable;
	PELF_SYMBOL DynSymTable;
	const void *PltRelocations;
	size_t      PltRelocationCount;
	ELF_RELA   *RelaEntries;
	size_t      RelaCount;
	ELF_REL    *RelEntries;
	size_t      RelCount;
	uintptr_t  *RelrEntries;
	size_t      RelrCount;
	bool        PltUsesRela;
	const char *StringTable;
	PELF_SYMBOL SymbolTable;
	size_t      SymbolTableSize;
	PELF_HASH_TABLE HashTable;
}
ELF_DYNAMIC_INFO, *PELF_DYNAMIC_INFO;

#ifdef __cplusplus
extern "C" {
#endif

bool RtlPerformRelocations(PELF_DYNAMIC_INFO DynInfo, uintptr_t LoadBase);

bool RtlParseDynamicTable(PELF_DYNAMIC_ITEM DynItem, PELF_DYNAMIC_INFO Info, uintptr_t LoadBase);

bool RtlLinkPlt(PELF_DYNAMIC_INFO DynInfo, uintptr_t LoadBase, UNUSED const char* FileName);

bool RtlUpdateGlobalOffsetTable(uintptr_t *Got, size_t Size, uintptr_t LoadBase);

void RtlFindSymbolTable(uint8_t* FileAddress, PELF_DYNAMIC_INFO DynInfo);

void RtlRelocateRelrEntries(PELF_DYNAMIC_INFO DynInfo, uintptr_t LoadBase);

uint32_t RtlElfHash(const char* Name);

BSTATUS RtlCheckValidity(PELF_HEADER Header);

#ifdef __cplusplus
}
#endif
