/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	ps/shutdown.c
	
Abstract:
	This module implements the shutdown procedure.
	
	The shutdown procedure performs several tasks:
	
	- Ensures every process on the system, except for the system process, is terminated
	
	- Unmaps every system cache view from the address space
	
	- Flushes all modified pages to their backing stores
	
	- Finally, it shuts down the system through the HAL.
	
	For now, every process is outright terminated, without opportunity to save work.  This will be
	rectified in the future by preventing regular user applications from using the OSShutDownSystem
	system service, instead using a central service (such as a window manager) that closes user
	programs safely before calling the OSShutDownSystem system service.
	
Author:
	iProgramInCpp - 29 March 2025
***/
#include "psp.h"
#include <cc.h>
#include <mm.h>

static KTHREAD PsShutDownWorkerThread;
static KEVENT PsShutDownEvent;

static bool PspTerminateProcessFilter(void* Pointer, UNUSED void* Context)
{
	PEPROCESS Process = (PEPROCESS) Pointer;
	
	// If this is the system process, then we *will* remove it from the handle table.
	// Makes things easier for us when we check that all *user* processes have exited.
	if (Process == PsGetSystemProcess())
	{
		DbgPrint("Erasing system process");
		return true;
	}
	
	// NOTE: We will *NOT* be removing the process from the handle table just yet.
	// We'll simply be marking it as terminated.  It'll automatically get removed when
	//
	// This kills *all* the threads from the current process, since the current thread
	// doesn't belong to this process' list of threads.
	DbgPrint("Terminating process '%s'...", Process->ImageName);
	
	KeTerminateOtherThreadsProcess(&Process->Pcb);
	
	return false;
}

static bool PspEraseProcessHandle(UNUSED void* Pointer, UNUSED void* Context)
{
	return true;
}

NO_RETURN
void PsShutDownWorker(UNUSED void* Context)
{
	DbgPrint("PsShutDownWorker active.  Waiting on PsShutDownEvent.");
	
	BSTATUS Status = KeWaitForSingleObject(&PsShutDownEvent, false, TIMEOUT_INFINITE, MODE_KERNEL);
	ASSERT(Status == STATUS_SUCCESS);
	
	DbgPrint("%s: Shutdown process initiated.", __func__);
	HalBeginShutdown();
	
	// Step 1: Terminate every process.
	//
	// Ideally, this will be done by asking the processes to terminate themselves nicely (sending
	// a signal), but we don't have such machinery yet so we'll just terminate them.
	
	ExFilterHandleTable(
		PspGetProcessHandleTable(),
		PspTerminateProcessFilter,
		PspEraseProcessHandle,
		NULL,  // FilterContext
		NULL   // KillContext
	);
	
	// Now we wait until the process list is empty.
	while (!ExIsEmptyHandleTable(PspGetProcessHandleTable()))
	{
		DbgPrint("Waiting 1 second for the process handle table to become empty...");
		PspDumpProcessList();
		OSSleep(1000);
	}
	
	DbgPrint("All the processes have been terminated.");
	
	// Step 2. Unmap the entire system view cache.
	CcUnmapAllViews();
	
	// Step 3. Put the modified page writer into motion.
	MmModifiedPageWriterShutDown();
	
	// Step 4. The system can be safely shut-down now.
	DbgPrint("The system is now ready for power-off.");
	HalPerformPoweroff(false);
}

void PsInitShutDownWorkerThread()
{
	// Initialize the shut down event.  When this event is signalled through KeSetEvent,
	// the associated thread is awoken and the shutdown process begins.
	KeInitializeEvent(&PsShutDownEvent, EVENT_NOTIFICATION, false);
	
	void* Stack = MmAllocateKernelStack();
	if (!Stack)
		KeCrash("Could not allocate kernel stack for shutdown worker thread.");
	
	KeInitializeThread2(
		&PsShutDownWorkerThread,
		Stack,
		KERNEL_STACK_SIZE,
		PsShutDownWorker,
		NULL,
		KeGetSystemProcess()
	);
	KeSetPriorityThread(&PsShutDownWorkerThread, PRIORITY_REALTIME);
	KeReadyThread(&PsShutDownWorkerThread);
}

BSTATUS OSShutDownSystem()
{
	// TODO: Normal user processes shouldn't be able to do this. Add security here (and, well,
	// frankly anywhere else for that matter) to prevent that.
	DbgPrint("OSShutDownSystem: Initiating system shut down.");
	
	KeSetEvent(&PsShutDownEvent, 5);
	
	return STATUS_SUCCESS;
}
