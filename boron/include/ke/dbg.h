/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/dbg.h
	
Abstract:
	This header file contains the definitions
	related to the DLL loader.
	
Author:
	iProgramInCpp - 28 October 2023
***/
#pragma once

#include <main.h>

#ifdef DEBUG

void DbgInit();

void DbgPrintString(const char* str);

void DbgPrintStringLocked(const char* str);

#else

#define DbgInit()
#define DbgPrintString(str)
#define DbgPrintStringLocked(str)

#endif

void DbgPrintStackTrace(uintptr_t Frame);

uintptr_t DbgLookUpAddress(const char* Name);

const char* DbgLookUpRoutineNameByAddressExact(uintptr_t Address);

const char* DbgLookUpRoutineNameByAddress(uintptr_t Address, uintptr_t* BaseAddressOut);
