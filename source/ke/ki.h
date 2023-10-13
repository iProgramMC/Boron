/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/ki.h
	
Abstract:
	This header file contains internal kernel definitions.
	
Author:
	iProgramInCpp - 26 September 2023
***/
#ifndef BORON_KE_KI_H
#define BORON_KE_KI_H

#include <ke.h>
#include <string.h>

// ===== Tests =====
void KiPerformTests();

// ===== Threading =====
EXMEMORY_HANDLE KiAllocateDefaultStack();

void KiSetupRegistersThread(PKTHREAD Thread); // Defined in arch

void KiEndThreadQuantum();

bool KiNeedToSwitchThread();

void KiSwitchToNextThread();

PKREGISTERS KiHandleSoftIpi(PKREGISTERS);

#endif//BORON_KE_KI_H
