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
BIG_MEMORY_HANDLE KiAllocateDefaultStack();

void KiSetupRegistersThread(PKTHREAD Thread); // Defined in arch

void KiPerformYield(PKREGISTERS);

bool KiNeedToSwitchThread();

PKREGISTERS KiSwitchToNextThread();

PKREGISTERS KiHandleSoftIpi(PKREGISTERS);

void KiUnwaitThread(PKTHREAD Thread, int Status);

uint64_t KiGetNextTimerExpiryTick();

uint64_t KiGetNextTimerExpiryItTick();

void KiDispatchTimerObjects(); // Called by the scheduler

NO_DISCARD KIPL KiLockDispatcher();

void KiUnlockDispatcher(KIPL OldIpl);

// Check if an object is signaled.
bool KiIsObjectSignaled(PKDISPATCH_HEADER Header);

// Alert waiting threads about a freshly signaled object.
PKTHREAD KiWaitTestAndGetWaiter(PKDISPATCH_HEADER Object);
void KiWaitTest(PKDISPATCH_HEADER Object);

// Define KiAssertOwnDispatcherLock
#ifdef DEBUG
void KiAssertOwnDispatcherLock_(const char* FunctionName);
#define KiAssertOwnDispatcherLock() KiAssertOwnDispatcherLock_(__func__)
#else
#define KiAssertOwnDispatcherLock()
#endif

void KiSwitchToAddressSpaceProcess(PKPROCESS Process);

NO_DISCARD BSTATUS KiInitializeThread(PKTHREAD Thread, BIG_MEMORY_HANDLE KernelStack, PKTHREAD_START StartRoutine, void* StartContext, PKPROCESS Process);

void KiOnKillProcess(PKPROCESS Process);

#endif//BORON_KE_KI_H
