/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/dpc.c
	
Abstract:
	This module contains the implementation of the
	DPC runner.
	
Author:
	iProgramInCpp - 3 October 2023
***/
#include <ke.h>
#include <arch.h>
#include <hal.h>
#include "ki.h"

extern int KiVectorDpcIpi;

void KeIssueSoftwareInterrupt()
{
	HalRequestIpi(0, HAL_IPI_SELF, KiVectorDpcIpi);
}

void KeInitializeDpc(PKDPC Dpc, PKDEFERRED_ROUTINE Routine, void* Context)
{
	Dpc->List.Flink = Dpc->List.Blink = NULL;
	
	Dpc->Routine = Routine;
	
	Dpc->Enqueued = false;
	
	Dpc->Important = false;
	
	Dpc->DeferredContext = Context;
	
	Dpc->Initialized = true;
}

int KeGetPendingEvents()
{
	return KeGetCurrentPRCB()->PendingEvents;
}

void KeClearPendingEvents()
{
	KeGetCurrentPRCB()->PendingEvents = 0;
}

void KeSetPendingEvent(int PendingEvent)
{
	KeGetCurrentPRCB()->PendingEvents |= PendingEvent;
}

// IPL: Any.

// Note! This must be called BEFORE KeEnqueueDpc! Calling this after enqueuing
// is undefined behavior because the DPC's routine may have executed already,
// rendering the DPC invalid!!
void KeSetImportantDpc(PKDPC Dpc, bool Important)
{
	Dpc->Important = Important;
}

// IPL: Any.
void KeEnqueueDpc(PKDPC Dpc, void* SysArg1, void* SysArg2)
{
	Dpc->SystemArgument1 = SysArg1;
	Dpc->SystemArgument2 = SysArg2;
	
	PKDPC_QUEUE Queue = &KeGetCurrentPRCB()->DpcQueue;
	
	bool IsImportant = false;
	
	// Disable interrupts to prevent them from using the incompletely
	// manipulated DPC queue.
	bool Restore = KeDisableInterrupts();
	
	IsImportant = Dpc->Important;
	
	// Append the element into the queue.
	if (Dpc->Important)
		InsertHeadList(&Queue->List, &Dpc->List);
	else
		InsertTailList(&Queue->List, &Dpc->List);
	
	KeSetPendingEvent(PENDING_DPCS);
	
	if (IsImportant)
		KeIssueSoftwareInterrupt();
	
	// Restore interrupts.
	KeRestoreInterrupts(Restore);
}

// Dispatches all of the DPCs in a loop.
void KiDispatchDpcs()
{
	// Test if we're at DPC level. We'd better be, otherwise,
	// it's a violation of IPL stacking principles.
	KIPL CurrentIpl = KeGetIPL();
	if (CurrentIpl != IPL_DPC)
		KeCrash("KiDispatchDpcs: Current IPL is not IPL_DPC");
	
	// Disable interrupts while we are working with the queue.
	bool Restore = KeDisableInterrupts();
	
	PKDPC_QUEUE Queue = &KeGetCurrentPRCB()->DpcQueue;
	while (!IsListEmpty(&Queue->List))
	{
		PLIST_ENTRY Entry = RemoveHeadList(&Queue->List);
		
		// Pop the DPC off of the list.
		PKDPC Dpc = CONTAINING_RECORD(Entry, KDPC, List);
		Dpc->Enqueued = false;
		
		ENABLE_INTERRUPTS();
		
		Dpc->Routine(Dpc,
		             Dpc->DeferredContext,
		             Dpc->SystemArgument1,
					 Dpc->SystemArgument2);
		
		// Note! Consider the Dpc object's pointer invalid from now on.
		// I know this is redundant, but better safe than sorry.
		Dpc = NULL;
		
		// Disable interrupts again to process the list.
		DISABLE_INTERRUPTS();
	}
	
	KeRestoreInterrupts(Restore);
}
