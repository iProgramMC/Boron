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
#include <ke.h>

void KeInitializeDispatchHeader(PKDISPATCH_HEADER Object)
{
	Object->Type = DISPOBJ_TIMER;
	Object->Signaled = false;
	
	InitializeListHead(&Object->WaitBlockList);
}

// Performs a wait on the current thread with the specified parameters.
// Returns the WaitStatus after waiting.
int KiPerformWaitThread(int WaitType, int WaitCount, PKDISPATCH_HEADER WaitObjects[], PKWAIT_BLOCK WaitBlockArray, bool WaitIsAlertable)
{
	KIPL Ipl = KeRaiseIPL(IPL_DPC);
	
	PKTHREAD Thread = KeGetCurrentThread();
	
	Thread->WaitBlockArray = WaitBlockArray;
	Thread->WaitType = WaitType;
	Thread->WaitCount = WaitCount;
	Thread->WaitIsAlertable = WaitIsAlertable;
	Thread->WaitStatus = STATUS_WAITING;
	
	if (WaitCount && WaitObjects && WaitBlockArray)
	{
		for (int i = 0; i < WaitCount; i++)
		{
			PKWAIT_BLOCK WaitBlock = &WaitBlockArray[i];
			
			WaitBlock->Thread = Thread;
			WaitBlock->Object = WaitObjects[i];
			
			// The thread's wait block should be part of the object's waiting object linked list.
			InsertTailList(&WaitBlock->Object->WaitBlockList, &WaitBlock->Entry);
		}
	}
	
	Thread->WaitStatus = STATUS_WAITING;
	Thread->Status = KTHREAD_STATUS_WAITING;
	
	KeLowerIPL(Ipl);
	
	KeYieldCurrentThread();
	
	if (Thread->WaitStatus == STATUS_WAITING)
		KeCrash("KiPerformWaitThread: still waiting");
	
	return Thread->WaitStatus;
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
	
	int Status;
	
	while (true)
	{
		Status = KiPerformWaitThread(WaitType, Count, Objects, WaitBlockArray, Alertable);
		
		if (Status != STATUS_KEEP_GOING)
			break;
		
		if (WaitType == WAIT_TYPE_ANY)
			KeCrash("KeWaitForMultipleObjects: got KEEP_GOING in wait type ANY");
		
		bool AllGood = true;
		
		for (int i = 0; i < Count; i++)
		{
			PKWAIT_BLOCK WaitBlock = &WaitBlockArray[i];
			
			if (!AtLoad(WaitBlock->Object->Signaled))
			{
				AllGood = false;
				break;
			}
		}
		
		if (AllGood)
			return STATUS_SUCCESS;
	}
	
	return Status;
}

int KeWaitForSingleObject(PKDISPATCH_HEADER Object, bool Alertable)
{
	return KeWaitForMultipleObjects(1, &Object, WAIT_TYPE_ANY, Alertable, NULL);
}

void KeSatisfyWaitBlock(PKWAIT_BLOCK WaitBlock)
{
	PKTHREAD Thread = WaitBlock->Thread;
	int Index = WaitBlock - Thread->WaitBlockArray;
	
	// If the wait mode is "ANY", set the wait status as
	// the thing that woke up the thread.
	if (Thread->WaitType == WAIT_TYPE_ANY)
		Thread->WaitStatus = STATUS_RANGE_WAIT + Index;
	// If it's "ALL", optimistically wake it up with the
	// status "KEEP_GOING" so that the loop can continue
	else if (Thread->WaitType == WAIT_TYPE_ALL)
		Thread->WaitStatus = STATUS_KEEP_GOING;
	
	KeWakeUpThread(Thread);
}

void KeSignalObject(PKDISPATCH_HEADER Object)
{
	Object->Signaled = true;
	
	PLIST_ENTRY Entry = Object->WaitBlockList.Flink;
	
	while (Entry != &Object->WaitBlockList)
	{
		PKWAIT_BLOCK WaitBlock = CONTAINING_RECORD(Entry, KWAIT_BLOCK, Entry);
		
		KeSatisfyWaitBlock(WaitBlock);
		
		Entry = Entry->Flink;
	}
}
