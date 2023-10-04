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

KREGISTERS* KeHandleDpcIpi(KREGISTERS* Regs)
{
	KiDispatchDpcs();
	return Regs;
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
	
	// Raise IRQL to prevent interrupts from using the incompletely
	// manipulated DPC queue.
	KIPL OldIpl = KeRaiseIPL(IPL_NOINTS);
	
	IsImportant = Dpc->Important;
	
	// Append the element into the queue.
	if (Dpc->Important)
		InsertHeadList(&Queue->List, &Dpc->List);
	else
		InsertTailList(&Queue->List, &Dpc->List);
	
	// Restore interrupts.
	KeLowerIPL(OldIpl);
	
	// If the DPC is important, send a self interrupt to dispatch
	// the DPCs right away! Otherwise, they can wait until the next
	// interrupt.
	if (IsImportant)
	{
		HalSendSelfIpi();
	}
}

// Dispatches all of the DPCs in a loop.
void KiDispatchDpcs()
{
	// Test if we're at DPC level. We'd better be, otherwise,
	// it's a violation of IPL stacking principles.
	KIPL OldIpl = KeRaiseIPL(IPL_DPC);
	
	// Raise the IPL to NOINTS level. We'll lower it down to DPC
	// level during the DPC handlers.
	KeRaiseIPL(IPL_NOINTS);
	
	PKDPC_QUEUE Queue = &KeGetCurrentPRCB()->DpcQueue;
	while (!IsListEmpty(&Queue->List))
	{
		PLIST_ENTRY Entry = RemoveHeadList(&Queue->List);
		if (Entry == &Queue->List)
			break;
		
		// Pop the DPC off of the list.
		PKDPC Dpc = CONTAINING_RECORD(Entry, KDPC, List);
		Dpc->Enqueued = false;
		
		KeLowerIPL(IPL_DPC);
		
		Dpc->Routine(Dpc,
		             Dpc->DeferredContext,
		             Dpc->SystemArgument1,
					 Dpc->SystemArgument2);
		
		// Note! Consider the Dpc object's pointer invalid from now on.
		// I know this is redundant, but better safe than sorry.
		Dpc = NULL;
		
		// Raise the IPL back to noints level to process the list again.
		KeRaiseIPL(IPL_NOINTS);
	}
	
	// Lower the IPL back to whatever it was before we raised the IPL to DPC level.
	KeLowerIPL(OldIpl);
}
