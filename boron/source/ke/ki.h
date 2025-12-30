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

// TODO: This needs heavy cleanup.

void KiInitLoaderParameterBlock();

NO_RETURN void KiCPUBootstrap(PLOADER_AP LoaderAp);

// ===== Tests =====
void KiPerformTests();

// ===== Threading =====
void* KiAllocateDefaultStack();

void KiSetupRegistersThread(PKTHREAD Thread); // Defined in arch

void KiPerformYield();

bool KiNeedToSwitchThread();

void KiSwitchToNextThread();

void KiUnwaitThread(PKTHREAD Thread, int Status, KPRIORITY Increment);

uint64_t KiGetNextTimerExpiryTick();

uint64_t KiGetNextTimerExpiryItTick();

void KiDispatchTimerObjects(); // Called by the scheduler

NO_DISCARD KIPL KiLockDispatcher();

void KiLockDispatcherWait();

void KiUnlockDispatcher(KIPL OldIpl);

// Check if an object is signaled.
bool KiIsObjectSignaled(PKDISPATCH_HEADER Header, PKTHREAD Thread);

// Alert waiting threads about a freshly signaled object.
PKTHREAD KiWaitTestAndGetWaiter(PKDISPATCH_HEADER Object, KPRIORITY Increment);
void KiWaitTest(PKDISPATCH_HEADER Object, KPRIORITY Increment);

void KiDispatchTimerQueue();

void KiApcInterrupt();

void KiDpcInterrupt();

void KeIssueSoftwareInterrupt(KIPL Ipl);

void KiAcknowledgeSoftwareInterrupt(KIPL Ipl);

void KiDispatchSoftwareInterrupts(KIPL NewIpl);

void KiHandleQuantumEnd();

void KiSetPendingQuantumEnd();

void KiOnKillProcess(PKPROCESS Process);

void KiReadyThread(PKTHREAD Thread);

void KiTerminateThread(PKTHREAD Thread, KPRIORITY Increment);

// Terminates the current thread and checks if its user mode stack
// should be freed.  This function does not return.
NO_RETURN void KiTerminateUserModeThread(KPRIORITY Increment);

void KiSwitchThreadStack(void** OldStack, void** NewStack);

// Same as KiSwitchThreadStack, but discards the old context.  Used during
// initialization of the scheduler when no thread was switched to before.
void KiSwitchThreadStackForever(void* NewStack);

// Gets the current scheduler.  IPL must be raised to IPL_DPC.
PKSCHEDULER KiGetCurrentScheduler();

// Define KiAssertOwnDispatcherLock
// Note: this has chances to fail if the current processor
// doesn't own it, but another processor does. However, it's
// still good as a debug check.
#ifdef DEBUG
void KiAssertOwnDispatcherLock_(const char* FunctionName);
#define KiAssertOwnDispatcherLock() KiAssertOwnDispatcherLock_(__func__)
#else
#define KiAssertOwnDispatcherLock()
#endif

void KiSwitchToAddressSpaceProcess(PKPROCESS Process);

void KiInitializeThread(PKTHREAD Thread, void* KernelStack, size_t KernelStackSize, PKTHREAD_START StartRoutine, void* StartContext, PKPROCESS Process);

bool KiCancelTimer(PKTIMER Timer);

void KiSwitchArchSpecificContext(PKTHREAD NewThread, PKTHREAD OldThread);

#ifdef TARGET_ARM
void KiSaveInterruptStacks(
	uintptr_t* AbortStack,
	uintptr_t* UndefinedStack,
	uintptr_t* IrqStack,
	uintptr_t* FiqStack
);
void KiRestoreInterruptStacks(
	uintptr_t AbortStack,
	uintptr_t UndefinedStack,
	uintptr_t IrqStack,
	uintptr_t FiqStack
);
#endif

#endif//BORON_KE_KI_H
