/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	ke/apc.c
	
Abstract:
	This module implements the APC object and its support
	routines.
	
Author:
	iProgramInCpp - 21 February 2024
***/
#include <ke.h>

void KeInitializeApc(
	PKAPC Apc,
	PKTHREAD Thread,
	PKAPC_KERNEL_ROUTINE KernelRoutine,
	PKAPC_NORMAL_ROUTINE NormalRoutine,
	void* NormalContext,
	KPROCESSOR_MODE ApcMode)
{
	// The type of APC object is determined by the presence of the NormalRoutine.
	// If the NormalRoutine parameter is present, then a normal APC object is
	// initialized to run in the specified mode.
	// Otherwise, a special APC object is initialized for execution in kernel mode.
	Apc->Special = NormalRoutine == NULL;
	Apc->KernelRoutine = KernelRoutine;
	Apc->NormalRoutine = NormalRoutine;
	Apc->NormalContext = NormalContext;
	Apc->ApcMode = Apc->Special ? MODE_KERNEL : ApcMode;
	Apc->SystemArgument1 = NULL;
	Apc->SystemArgument2 = NULL;
	Apc->ListEntry.Flink = NULL;
	Apc->ListEntry.Blink = NULL;
	Apc->Enqueued = false;
	Apc->Thread = Thread;
	
	// Determine the tier of APC:
	if (Apc->Special)
		Apc->Tier = APC_QUEUE_SPECIAL;
	else if (Apc->ApcMode == MODE_KERNEL)
		Apc->Tier = APC_QUEUE_KERNEL;
	else
		Apc->Tier = APC_QUEUE_USER;
}

static bool KepShouldRequestApcInterrupt(PKAPC Apc)
{
	PKTHREAD Thread = Apc->Thread;
	
	// If the thread is already running special APCs, right away no
	if (Thread->ApcRunning[APC_QUEUE_SPECIAL])
		return false;
	
	// Special APCs can interrupt normal ones.
	if (Apc->Special)
		return true;
	
	// Similar logic for these.
	// XXX: This sucks, maybe deal with it later.
	if (Thread->ApcRunning[APC_QUEUE_KERNEL])
		return false;
	
	if (Apc->ApcMode == MODE_KERNEL)
		return true;
	
	if (Thread->ApcRunning[APC_QUEUE_USER])
		return false;
	
	return true;
}

bool KeInsertQueueApc(
	PKAPC Apc,
	void* SystemArgument1,
	void* SystemArgument2)
{
	// If the APC was already inserted:
	if (Apc->Enqueued)
		return false;
	
	Apc->SystemArgument1 = SystemArgument1;
	Apc->SystemArgument2 = SystemArgument2;
	
	// Insert into the queue:
	InsertTailList(&Apc->Thread->ApcQueue[Apc->Tier], &Apc->ListEntry);
	
	// Determine whether to request the APC interrupt
	if (KepShouldRequestApcInterrupt(Apc))
	{
		KeIssueSoftwareInterruptApcLevel();
	}
	
	return false;
}
