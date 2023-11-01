/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/dispatch.c
	
Abstract:
	This header file contains the definitions for the
	event dispatcher.
	
Author:
	iProgramInCpp - 30 October 2023
***/
#include "ki.h"

KSPIN_LOCK KiDispatcherLock;

NO_DISCARD
KIPL KiLockDispatcher()
{
	KIPL Ipl;
	KeAcquireSpinLock(&KiDispatcherLock, &Ipl);
	return Ipl;
}

void KiUnlockDispatcher(KIPL Ipl)
{
	KeReleaseSpinLock(&KiDispatcherLock, Ipl);
}

#ifdef DEBUG

void KiAssertOwnDispatcherLock_(const char* FunctionName)
{
	if (!KiDispatcherLock.Locked)
		KeCrash("%s: dispatcher lock is unlocked", FunctionName);
}

#endif

void KeInitializeDispatchHeader(PKDISPATCH_HEADER Object, int Type)
{
	Object->Type     = Type;
	Object->Signaled = false;
	Object->ProcId   = KeGetCurrentPRCB()->Id;
	
	// Note. If a mutex, it's signaled by default until acquired.
	
	InitializeListHead(&Object->WaitBlockList);
}

int KeWaitForMultipleObjects(
	int Count,
	PKDISPATCH_HEADER Objects[],
	int WaitType,
	bool Alertable,
	PKWAIT_BLOCK WaitBlockArray)
{
	PKTHREAD Thread = KeGetCurrentThread();
	
	int Maximum = MAXIMUM_WAIT_BLOCKS;
	
	if (!WaitBlockArray)
	{
		WaitBlockArray = Thread->WaitBlocks;
		Maximum = THREAD_WAIT_BLOCKS;
	}
	
	if (Count > Maximum)
		KeCrash("KeWaitForMultipleObjects: Object count %d is bigger than the maximum wait blocks of %d", Count, Maximum); 
	
	// Raise the IPL so that we do not get interrupted while modifying the thread's wait parameters
	KIPL Ipl = KeRaiseIPL(IPL_DPC);
	
	Thread->WaitBlockArray = WaitBlockArray;
	Thread->WaitType = WaitType;
	Thread->WaitCount = Count;
	Thread->WaitIsAlertable = Alertable;
	Thread->WaitStatus = STATUS_WAITING;
	
	for (int i = 0; i < Count; i++)
	{
		PKWAIT_BLOCK WaitBlock = &WaitBlockArray[i];
		
		WaitBlock->Thread = Thread;
		WaitBlock->Object = Objects[i];
		
		// The thread's wait block should be part of the object's waiting object linked list.
		InsertTailList(&WaitBlock->Object->WaitBlockList, &WaitBlock->Entry);
	}
	
	Thread->Status = KTHREAD_STATUS_WAITING;
	
	KeLowerIPL(Ipl);
	
	KeYieldCurrentThread();
	
	return Thread->WaitStatus;
}

int KeWaitForSingleObject(PKDISPATCH_HEADER Object, bool Alertable)
{
	return KeWaitForMultipleObjects(1, &Object, WAIT_TYPE_ANY, Alertable, NULL);
}

void KiSatisfyWaitBlock(PKWAIT_BLOCK WaitBlock)
{
	KiAssertOwnDispatcherLock();
	
	PKTHREAD Thread = WaitBlock->Thread;
	int Index = WaitBlock - Thread->WaitBlockArray;
	
	int Result = -1;
	
	if (Thread->WaitType == WAIT_TYPE_ANY)
	{
		Result = STATUS_RANGE_WAIT + Index;
	}
	else if (Thread->WaitType == WAIT_TYPE_ALL)
	{
		// Optimistically assume we already can wake up the thread
		Result = STATUS_SUCCESS;
		
		// Check if all other objects are signaled as well
		for (int i = 0; i < Thread->WaitCount; i++)
		{
			PKWAIT_BLOCK WaitBlock = &Thread->WaitBlockArray[i];
			if (!WaitBlock->Object->Signaled)
			{
				Result = -1;
				break;
			}
		}
	}
	
	if (Result != -1)
		KiUnwaitThread(Thread, Result);
}

bool KiIsObjectSignaled(PKDISPATCH_HEADER Header)
{
	if (Header->Type == DISPATCH_MUTEX)
		return Header->Signaled == 0;
	
	// default case
	return Header->Signaled != 0;
}

// Set the signaled state of the object and do nothing else.
static void KiSetSignaled(PKDISPATCH_HEADER Object)
{
	switch (Object->Type)
	{
		case DISPATCH_MUTEX:
		case DISPATCH_SEMAPHORE:
			Object->Signaled--;
			break;
		
		default:
			Object->Signaled = true;
	}
}

void KiSignalObject(PKDISPATCH_HEADER Object)
{
	KiSetSignaled(Object);
	
	int SatisfiedCount = 0;
	
	while (!IsListEmpty(&Object->WaitBlockList))
	{
		PLIST_ENTRY Entry = Object->WaitBlockList.Flink;
		PKWAIT_BLOCK WaitBlock = CONTAINING_RECORD(Entry, KWAIT_BLOCK, Entry);
		KiSatisfyWaitBlock(WaitBlock);
		
		// Remove the entry
		RemoveHeadList(&Object->WaitBlockList);
		
		SatisfiedCount++;
		
		// If the object is a mutex and at least one object was satisfied, stop.
		if (SatisfiedCount == 1 && Object->Type == DISPATCH_MUTEX)
			return;
		
		// If the object is a semaphore, check for the Limit field of the semaphore.
	}
	
}

PKREGISTERS KiHandleSoftIpi(PKREGISTERS Regs)
{
	int Flags = KeGetPendingEvents();
	KeClearPendingEvents();
	
	KIPL Ipl = KiLockDispatcher();
	
	if (KiGetNextTimerExpiryTick() <= HalGetTickCount() + 100)
		KiDispatchTimerObjects();
	
	if (Flags & PENDING_DPCS)
		KiDispatchDpcs();
	
	if (Flags & PENDING_YIELD)
		KiPerformYield(Regs);
	
	if (KeGetCurrentPRCB()->Scheduler.NextThread)
		Regs = KiSwitchToNextThread();
	
	KiUnlockDispatcher(Ipl);
	
	return Regs;
}
