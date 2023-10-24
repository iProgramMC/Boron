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

#include <_limine.h>
#include <main.h>
#include <ke.h>
#include <mm.h>
#include <string.h>
#include <limreq.h>

typedef struct limine_file LIMINE_FILE, *PLIMINE_FILE;

typedef int(*PDLL_ENTRY_POINT)();

typedef struct
{
	const char*      Name; // should be == LimineFile->path
	PLIMINE_FILE     LimineFile;
	uintptr_t        ImageBase;
	PDLL_ENTRY_POINT EntryPoint;
}
LOADED_DLL, *PLOADED_DLL;

extern LOADED_DLL KeLoadedDLLs[];
extern int        KeLoadedDLLCount;

void LdriLoadDll(PLIMINE_FILE File);

uintptr_t LdrAllocateRange(size_t Size);

#endif//BORON_LDRI_H