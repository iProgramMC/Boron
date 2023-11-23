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

KPROCESS KiSystemProcess;

void KiSwitchToAddressSpaceProcess(PKPROCESS Process)
{
	KeSetCurrentPageTable(Process->PageMap);
}

// ------- Exposed API -------

PKPROCESS KeGetCurrentProcess()
{
	return KeGetCurrentThread()->Process;
}

PKPROCESS KeGetSystemProcess()
{
	return &KiSystemProcess;
}

PKPROCESS KeAllocateProcess()
{
	PKPROCESS Process = ExAllocateSmall(sizeof(KPROCESS));
	if (!Process)
		return NULL;
	
	memset(Process, 0, sizeof *Process);
	
	return Process;
}

void KeDeallocateProcess(PKPROCESS Process)
{
	ExFreeSmall(Process, sizeof(KPROCESS));
}

void KeInitializeProcess(PKPROCESS Process, int BasePriority, KAFFINITY BaseAffinity)
{
	KeInitializeDispatchHeader(&Process->Header, DISPATCH_PROCESS);
	
	Process->PageMap = MmCreatePageMapping(KeGetCurrentPageTable());
	
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

void KeDetachProcess(PKPROCESS Process)
{
	KIPL Ipl = KiLockDispatcher();
	
	ASSERT(!Process->Detached);
	Process->Detached = true;
	
	KiUnlockDispatcher(Ipl);
}
