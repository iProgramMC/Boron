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
#include <hal.h>
#include <string.h>

// ===== Tests =====
void KiPerformTests();

// ===== Threading =====
EXMEMORY_HANDLE KiAllocateDefaultStack();

void KiSetupRegistersThread(PKTHREAD Thread); // Defined in arch

void KiPerformYield(PKREGISTERS);

bool KiNeedToSwitchThread();

PKREGISTERS KiSwitchToNextThread();

PKREGISTERS KiHandleSoftIpi(PKREGISTERS);

void KiUnwaitThread(PKTHREAD Thread, int Status);

uint64_t KiGetNextTimerExpiryTick();

uint64_t KiGetNextTimerExpiryItTick();

void KiDispatchTimerObjects(); // Called by the scheduler

#endif//BORON_KE_KI_H
