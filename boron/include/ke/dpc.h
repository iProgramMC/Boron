/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/dpc.h
	
Abstract:
	This module contains the definitions related to the
	deferred procedure call system.
	
Author:
	iProgramInCpp - 3 October 2023
***/
#ifndef BORON_KE_DPC_H
#define BORON_KE_DPC_H

#include <arch.h>

typedef struct KDPC_tag KDPC, *PKDPC;

typedef void(*PKDEFERRED_ROUTINE)(PKDPC Dpc, void* Context, void* SA1, void* SA2);

struct KDPC_tag
{
	LIST_ENTRY List;
	
	PKDEFERRED_ROUTINE Routine;
	
	void* DeferredContext;
	void* SystemArgument1;
	void* SystemArgument2;
	
	bool Initialized;
	bool Enqueued;
	bool Important;
};

typedef struct KDPC_QUEUE_tag
{
	LIST_ENTRY List;
}
KDPC_QUEUE, *PKDPC_QUEUE;

// Initialize a DPC object.
void KeInitializeDpc(PKDPC Dpc, PKDEFERRED_ROUTINE Routine, void* DeferredContext);

// Set the importance of the DPC object.
// Note! You *must* call this BEFORE enqueuing the DPC, otherwise the
// behavior is undefined. Even if the DPC is still enqueued, its place
// in the queue won't be changed.
void KeSetImportantDpc(PKDPC Dpc, bool Important);

// Enqueues the DPC object.
// Note! After calling this, consider the object invalid, until you know exactly
// that the DPC is valid again (not cleaned up by the deferred routine), and no
// longer held within the queue.
void KeEnqueueDpc(PKDPC Dpc, void* SysArg1, void* SysArg2);

#ifdef KERNEL
// Dispatch DPCs. Don't use if you're not the self IPI handler!
void KiDispatchDpcs();
#endif

#endif // BORON_KE_DPC_H
