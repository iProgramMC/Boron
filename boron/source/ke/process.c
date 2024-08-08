/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/process.c
	
Abstract:
	This module implements the kernel process object. Functions
	to initialize, attach-to, detach-from, detach, allocate,
	deallocate, and get-current are provided.
	
Author:
	iProgramInCpp - 23 November 2023
***/
#include "ki.h"
#include <ps/process.h>

void KiSwitchToAddressSpaceProcess(PKPROCESS Process)
{
	KeSetCurrentPageTable(Process->PageMap);
}

void KiOnKillProcess(PKPROCESS Process)
{
	// Process was killed.  This is called from the last thread's DPC routine.
	// The thread's kernel stack is no longer used and we've long since switched
	// to a different process' address space.
	ASSERT(KeGetCurrentProcess() != Process);
	
	// Delete the page mapping.
	// TODO
	
	// Close any open resources.
	// TODO
	
	// Signal all threads that are waiting on this process.
	// Here it's simpler because this IS where the process is killed!
	Process->Header.Signaled = true;
	KiWaitTest(&Process->Header, 0);
	
	if (Process->Detached)
		Process->TerminateMethod(Process);
}

// ------- Exposed API -------

PKPROCESS KeGetCurrentProcess()
{
	return KeGetCurrentThread()->Process;
}

PKPROCESS KeGetSystemProcess()
{
	return &PsGetSystemProcess()->Pcb;
}

PKPROCESS KeAllocateProcess()
{
	PKPROCESS Process = MmAllocatePool(POOL_FLAG_NON_PAGED, sizeof(KPROCESS));
	if (!Process)
		return NULL;
	
	memset(Process, 0, sizeof *Process);
	
	return Process;
}

void KeDeallocateProcess(PKPROCESS Process)
{
	MmFreePool(Process);
}

void KeInitializeProcess(PKPROCESS Process, int BasePriority, KAFFINITY BaseAffinity)
{
	KeInitializeDispatchHeader(&Process->Header, DISPATCH_PROCESS);
	
	Process->PageMap = MiCreatePageMapping(KeGetCurrentPageTable());
	
	InitializeListHead(&Process->ThreadList);
	
	Process->AccumulatedTime = 0;
	
	Process->DefaultPriority = BasePriority;
	
	Process->DefaultAffinity = BaseAffinity;
}

void KeAttachToProcess(PKPROCESS Process)
{
	PKTHREAD Thread = KeGetCurrentThread();
	
	if (Thread->AttachedProcess)
		KeCrash("KeAttachToProcess: cannot attach to two processes");
	
	Thread->AttachedProcess = Process;
	
	KiSwitchToAddressSpaceProcess(Process);
}

void KeDetachFromProcess()
{
	PKTHREAD Thread = KeGetCurrentThread();
	
	if (!Thread->AttachedProcess)
		KeCrash("KeDetachFromProcess: cannot detach from nothing");
	
	Thread->AttachedProcess = NULL;
	
	KiSwitchToAddressSpaceProcess(Thread->Process);
}

void KeDetachProcess(PKPROCESS Process, PKPROCESS_TERMINATE_METHOD TerminateMethod)
{
	KIPL Ipl = KiLockDispatcher();
	
	ASSERT(!Process->Detached);
	Process->Detached = true;
	
	// TODO: Handle already terminated process
	
	if (!TerminateMethod)
		TerminateMethod = KeDeallocateProcess;
	
	Process->TerminateMethod = TerminateMethod;
	
	KiUnlockDispatcher(Ipl);
}
