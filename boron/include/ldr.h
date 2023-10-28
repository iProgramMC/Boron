/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ldr.h
	
Abstract:
	This header file contains the definitions
	related to the DLL loader.
	
Author:
	iProgramInCpp - 22 October 2023
***/
#ifndef NS64_LDR_H
#define NS64_LDR_H

#include <main.h>

typedef struct limine_file LIMINE_FILE, *PLIMINE_FILE;

typedef int(*PDLL_ENTRY_POINT)();

typedef struct
{
	const char*      Name; // should be == LimineFile->path
	PLIMINE_FILE     LimineFile;
	uintptr_t        ImageBase;
	size_t           ImageSize;
	PDLL_ENTRY_POINT EntryPoint;
	const char*      StringTable;
	void*            SymbolTable;
	size_t           SymbolTableSize;
}
LOADED_DLL, *PLOADED_DLL;

#ifdef KERNEL
extern LOADED_DLL KeLoadedDLLs[];
extern int        KeLoadedDLLCount;
#else
// Expose this if needed
#endif

void LdrInit();

void LdrInitializeHal();

void LdrInitializeDrivers();

const char* LdrLookUpRoutineNameByAddress(PLOADED_DLL LoadedDll, uintptr_t Address, uintptr_t* BaseAddress);

#endif//NS64_LDR_H
