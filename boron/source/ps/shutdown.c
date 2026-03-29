/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	ps/shutdown.c
	
Abstract:
	This module implements the shutdown procedure.
	
	The shutdown procedure performs several tasks:
	
	- Ensures every process on the system, except for the system process, is terminated
	
	- Unmaps every system cache view from system memory
	
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

static KTHREAD PsShutDownWorkerThread;
static KEVENT PsShutDownEvent;

NO_RETURN
void PsShutDownWorker(UNUSED void* Context)
{
	DbgPrint("PsShutDownWorker active.  Waiting on PsShutDownEvent.");
	
	BSTATUS Status = KeWaitForSingleObject(&PsShutDownEvent, false, TIMEOUT_INFINITE, MODE_KERNEL);
	ASSERT(Status == STATUS_SUCCESS);
	
	DbgPrint("%s: Shutdown process initiated.", __func__);
	
	
	
	
	
	DbgPrint("Well, there's nothing here...");
	KeTerminateThread(1);
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
