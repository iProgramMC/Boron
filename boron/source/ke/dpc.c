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
#include "ki.h"
#include <arch.h>
#include <hal.h>

void KeInitializeDpc(PKDPC Dpc, PKDEFERRED_ROUTINE Routine, void* Context)
{
	Dpc->List.Flink = Dpc->List.Blink = NULL;
	
	Dpc->Routine = Routine;
	
	Dpc->Enqueued = false;
	
	Dpc->Important = false;
	
	Dpc->DeferredContext = Context;
	
	Dpc->Initialized = true;
}

// IPL: Any.

// Note: This must be called BEFORE KeEnqueueDpc.  Calling this after enqueuing
// is undefined behavior because the DPC's routine may have executed already,
// rendering the DPC invalid.
void KeSetImportantDpc(PKDPC Dpc, bool Important)
{
	Dpc->Important = Important;
}

// IPL: Any.
void KeEnqueueDpc(PKDPC Dpc, void* SysArg1, void* SysArg2)
{
	PKDPC_QUEUE Queue = &KeGetCurrentPRCB()->DpcQueue;
	
	bool IsImportant = false;
	
	// Disable interrupts to prevent them from using the incompletely
	// manipulated DPC queue.
	bool Restore = KeDisableInterrupts();
	
	// If the DPC has been enqueued already, then return early.
	// This allows an interrupt handler to avoid checking whether the DPC was enqueued by itself.
	if (Dpc->Enqueued)
	{
		KeRestoreInterrupts(Restore);
		return;
	}
	
	Dpc->SystemArgument1 = SysArg1;
	Dpc->SystemArgument2 = SysArg2;
	
	Dpc->Enqueued = true;
	
	IsImportant = Dpc->Important;
	
	// Append the element into the queue.
	if (Dpc->Important)
		InsertHeadList(&Queue->List, &Dpc->List);
	else
		InsertTailList(&Queue->List, &Dpc->List);
	
	KeIssueSoftwareInterrupt(IPL_DPC);
	
	// Restore interrupts.
	KeRestoreInterrupts(Restore);
	
	// XXX: This is probably fine.
	if (IsImportant)
		KiDispatchSoftwareInterrupts(KeGetIPL());
}

// Dispatches all of the DPCs in a loop.
void KiDispatchDpcQueue()
{
	// In debug mode, test if we're at DPC level.
	ASSERT(KeGetIPL() == IPL_DPC);
	
	// Disable interrupts while we are working with the queue.
	//
	// This is because there are higher IPL interrupts that call KeEnqueueDpc.
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
		
		// Consider the DPC pointer invalid, so don't do anything with it after
		// invoking its routine.
#ifdef DEBUG
		Dpc = NULL;
#endif
		
		// Disable interrupts again to process the list.
		DISABLE_INTERRUPTS();
	}
	
	KeRestoreInterrupts(Restore);
}

void KiSetPendingQuantumEnd()
{
	bool Restore = KeDisableInterrupts();
	
	PKPRCB Prcb = KeGetCurrentPRCB();
	Prcb->YieldCurrentThread = true;
	KeIssueSoftwareInterrupt(IPL_DPC);
	
	KeRestoreInterrupts(Restore);
}

void KiDpcInterrupt()
{
	KiAcknowledgeSoftwareInterrupt(IPL_DPC);
	
	PKPRCB Prcb = KeGetCurrentPRCB();
	Prcb->Ipl = IPL_DPC;
	
	ENABLE_INTERRUPTS();
	
	KiDispatchTimerQueue();
	KiDispatchDpcQueue();
	
	if (Prcb->YieldCurrentThread)
		KiHandleQuantumEnd();
	
	DISABLE_INTERRUPTS();
}
