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

void LdriLoadDll(PLIMINE_FILE File);

uintptr_t LdrAllocateRange(size_t Size);

#endif//BORON_LDRI_H