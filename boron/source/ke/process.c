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
	
	// Signal all threads that are waiting on this process.
	// Here it's simpler because this IS where the process is killed!
	Process->Header.Signaled = true;
	KiWaitTest(&Process->Header, 0);
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

BSTATUS KeInitializeProcess(PKPROCESS Process, int BasePriority, KAFFINITY BaseAffinity)
{
	KeInitializeDispatchHeader(&Process->Header, DISPATCH_PROCESS);
	
	Process->PageMap = MiCreatePageMapping();
	
	if (Process->PageMap == 0)
		return STATUS_INSUFFICIENT_MEMORY;
	
	InitializeListHead(&Process->ThreadList);
	
	Process->AccumulatedTime = 0;
	
	Process->DefaultPriority = BasePriority;
	
	Process->DefaultAffinity = BaseAffinity;
	
	Process->PebPointer = NULL;
	return STATUS_SUCCESS;
}

PKPROCESS KeSetAttachedProcess(PKPROCESS Process)
{
	PKTHREAD Thread = KeGetCurrentThread();
	PKPROCESS OldProcess = Thread->AttachedProcess;
	
	Thread->AttachedProcess = Process;
	
	KiSwitchToAddressSpaceProcess(Process ? Process : Thread->Process);
	return OldProcess;
}

void KeTerminateOtherThreadsProcess(PKPROCESS Process)
{
	// Lock the dispatcher so that the list of threads isn't
	// invalidated while we process it
	KIPL Ipl = KiLockDispatcher();
	
	// Exit every thread other than our own.
	PLIST_ENTRY ListEntry = Process->ThreadList.Flink;
	while (ListEntry != &Process->ThreadList)
	{
		PKTHREAD Thread = CONTAINING_RECORD(ListEntry, KTHREAD, EntryProc);
		
		if (Thread != KeGetCurrentThread())
			KiTerminateThread(Thread, 1);
		
		ListEntry = ListEntry->Flink;
	}
	
	// Unlock the dispatcher because we don't need it anymore.
	KiUnlockDispatcher(Ipl);
}
