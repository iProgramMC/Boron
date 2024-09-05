/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ps/thread.c
	
Abstract:
	This module implements the process manager thread
	object in Boron.
	
Author:
	iProgramInCpp - 30 August 2024
***/
#include "psp.h"

void PspDeleteThread(void* ThreadV)
{
	// This thread was dereferenced by everything.
	UNUSED PETHREAD Thread = ThreadV;
	
	// Free its stack.
	MmFreeThreadStack(Thread->Tcb.Stack.Top);
	
	// Remove our reference to the process object.
	ObDereferenceObject(Thread->Tcb.Process);
	
	// TODO: anything more?
}

INIT
bool PsCreateThreadType()
{
	OBJECT_TYPE_INFO ObjectInfo;
	memset (&ObjectInfo, 0, sizeof ObjectInfo);
	
	ObjectInfo.NonPagedPool = true;
	ObjectInfo.MaintainHandleCount = true;
	
	ObjectInfo.Delete = PspDeleteThread;
	
	BSTATUS Status = ObCreateObjectType(
		"Thread",
		&ObjectInfo,
		&PsThreadObjectType
	);
	
	if (FAILED(Status))
	{
		DbgPrint("Could not create Thread type: %d", Status);
		return false;
	}
	
	return true;
}

typedef struct
{
	PEPROCESS Process;
	PKTHREAD_START StartRoutine;
	void* StartContext;
}
THREAD_START_CONTEXT;

// This function is called at high IPL.
void PspTerminateThread(PKTHREAD Tcb)
{
	PETHREAD Thread = (PETHREAD) Tcb;
	
	// Reached when a Process Manager managed thread has terminated.
	
	// Release the self-reference.
	//
	// This should be a final action as the Thread object might only have 1 reference to itself,
	// meaning that after this point, the Thread object might go invalid!
	ObDereferenceObject(Thread);
	
	// TODO: anything more?
}

BSTATUS PspInitializeThreadObject(void* ThreadV, void* Context)
{
	BSTATUS Status;
	THREAD_START_CONTEXT* Tsc = Context;
	PEPROCESS Process = Tsc->Process;
	PKTHREAD_START StartRoutine = Tsc->StartRoutine;
	PETHREAD Thread = ThreadV;
	void* StartContext = Tsc->StartContext;
	
	// Allocate the thread's stack.
	void* ThreadStack = MmAllocateKernelStack();
	
	if (!ThreadStack)
		return STATUS_INSUFFICIENT_MEMORY;
	
	// Initialize the kernel core thread object.
	// TODO: After the removal of the Mm references in the kernel core the status should be removed as it can
	// only ever fail if unable to allocate the kernel stack.
	Status = KeInitializeThread(&Thread->Tcb, ThreadStack, StartRoutine, StartContext, &Process->Pcb);
	ASSERT(SUCCEEDED(Status));
	
	// Assign the Ps specific thread termination method.
	Thread->Tcb.TerminateMethod = PspTerminateThread;
	
	// On behalf of that thread, add a reference to the process object.
	// This reference will be removed when the thread exits.
	ObReferenceObjectByPointer(Process);
	
	// On behalf of that process, add a reference to this thread object.
	// This reference will be removed when the thread exits.
	//
	// Even if nothing else references the thread, the thread object keeps a reference to itself
	// during its execution.  This is to avoid the thread being disposed of by the object manager
	// while still executing.  When the thread is terminated (with KeTerminateThread), it will
	// remove this reference, and if it's orphaned, it will immediately be deleted.
	ObReferenceObjectByPointer(Thread);
	
	// TODO: Initialize other ETHREAD attributes.  Currently there's just the TCB.
	
	// Ready the thread.
	KeReadyThread(&Thread->Tcb);
	
	return STATUS_SUCCESS;
}

BSTATUS PsCreateSystemThread(
	PHANDLE OutHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	HANDLE ProcessHandle,
	PKTHREAD_START StartRoutine,
	void* StartContext)
{
	void* ProcessV;
	BSTATUS Status;
	
	if (ProcessHandle == HANDLE_NONE)
	{
		ProcessV = &PsSystemProcess;
		ObReferenceObjectByPointer(ProcessV);
	}
	else
	{
		Status = ExReferenceObjectByHandle(ProcessHandle, PsProcessObjectType, &ProcessV);
		if (FAILED(Status))
			return Status;
	}
	
	PEPROCESS Process = ProcessV;
	THREAD_START_CONTEXT Tsc;
	Tsc.Process = Process;
	Tsc.StartRoutine = StartRoutine;
	Tsc.StartContext = StartContext;
	
	Status = ExiCreateObjectUserCall(
		OutHandle,
		ObjectAttributes,
		PsThreadObjectType,
		sizeof(ETHREAD),
		PspInitializeThreadObject,
		&Tsc
	);
	
	ObDereferenceObject(ProcessV);
	return Status;
}
