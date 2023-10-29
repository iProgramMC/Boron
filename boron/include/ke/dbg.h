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

void DbgPrintStackTrace(uintptr_t Frame);

void DbgPrintString(const char* str);

uintptr_t DbgLookUpAddress(const char* Name);

const char* DbgLookUpRoutineNameByAddressExact(uintptr_t Address);

const char* DbgLookUpRoutineNameByAddress(uintptr_t Address, uintptr_t* BaseAddressOut);

#ifdef DEBUG

void DbgSetProgressBarSteps(int Steps);

void DbgAddToProgressBar();

void DbgBusyWaitABit();

#else

#define DbgSetProgressBarSteps()

#define DbgAddToProgressBar()

#define DbgBusyWaitABit()

#endif

