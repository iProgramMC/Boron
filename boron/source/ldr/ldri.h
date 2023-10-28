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

void LdriLoadDll(PLIMINE_FILE File);

uintptr_t LdrAllocateRange(size_t Size);

#endif//BORON_LDRI_H