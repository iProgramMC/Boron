/***
	The Boron Operating System
	Copyright (C) 2024-2025 iProgramInCpp

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
	if (Thread->Initialized)
		MmFreeThreadStack(Thread->Tcb.Stack.Top);
	
	// Remove our reference to the process object.
	if (Thread->Tcb.Process)
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
	bool CreateSuspended;
}
THREAD_INIT_CONTEXT;

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

static BSTATUS PspInitializeThread(PETHREAD Thread, PEPROCESS Process, PKTHREAD_START StartRoutine, void* StartContext, bool CreateSuspended)
{
	Thread->Initialized = false;
	
	// Allocate the thread's stack.
	void* ThreadStack = MmAllocateKernelStack();
	
	if (!ThreadStack)
		return STATUS_INSUFFICIENT_MEMORY;
	
	// Initialize the kernel core thread object.
	// TODO: After the removal of the Mm references in the kernel core the status should be removed as it can
	// only ever fail if unable to allocate the kernel stack.
	KeInitializeThread(&Thread->Tcb, ThreadStack, StartRoutine, StartContext, &Process->Pcb);
	
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
	if (!CreateSuspended)
		KeReadyThread(&Thread->Tcb);
	
	Thread->Initialized = true;
	
	return STATUS_SUCCESS;
}

BSTATUS PspInitializeThreadObject(void* ThreadV, void* Context)
{
	THREAD_INIT_CONTEXT* Tic = Context;
	PETHREAD Thread = ThreadV;
	
	return PspInitializeThread(Thread, Tic->Process, Tic->StartRoutine, Tic->StartContext, Tic->CreateSuspended);
}

BSTATUS PsCreateSystemThreadFast(
	PETHREAD* OutThread,
	PKTHREAD_START StartRoutine,
	void* StartContext,
	bool CreateSuspended)
{
	void* OutObject = NULL;
	BSTATUS Status = ObCreateObject(
		&OutObject,
		NULL,
		PsThreadObjectType,
		NULL,
		OB_FLAG_KERNEL | OB_FLAG_NONPAGED | OB_FLAG_NO_DIRECTORY,
		NULL,
		sizeof(ETHREAD)
	);
	
	if (FAILED(Status))
		return Status;
	
	PETHREAD Thread = OutObject;
	Status = PspInitializeThread(Thread, PsGetCurrentProcess(), StartRoutine, StartContext, CreateSuspended);
	if (FAILED(Status))
	{
		ObDereferenceObject(Thread);
		return Status;
	}
	
	*OutThread = Thread;
	return STATUS_SUCCESS;
}

BSTATUS PsCreateSystemThread(
	PHANDLE OutHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	HANDLE ProcessHandle,
	PKTHREAD_START StartRoutine,
	void* StartContext,
	bool CreateSuspended)
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
	THREAD_INIT_CONTEXT Tic;
	Tic.Process = Process;
	Tic.StartRoutine = StartRoutine;
	Tic.StartContext = StartContext;
	Tic.CreateSuspended = CreateSuspended;
	
	Status = ExCreateObjectUserCall(
		OutHandle,
		ObjectAttributes,
		PsThreadObjectType,
		sizeof(ETHREAD),
		PspInitializeThreadObject,
		&Tic,
		POOL_NONPAGED,
		false
	);
	
	ObDereferenceObject(ProcessV);
	return Status;
}

BSTATUS OSCreateThread(
	PHANDLE OutHandle,
	HANDLE ProcessHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	PKTHREAD_START ThreadStart,
	void* ThreadContext,
	bool CreateSuspended)
{
	// TODO: The user process should be able to allocate a stack.  But
	// then we should also have a way to free the current process' stack.
	// Maybe we should consider making the OSExitThread system call give
	// us the stack's top?
	
	if ((uintptr_t) ThreadStart >= MM_USER_SPACE_END && KeGetPreviousMode() != MODE_KERNEL)
		return STATUS_INVALID_PARAMETER;
	
	BSTATUS Status;
	
	PTHREAD_START_CONTEXT Context = MmAllocatePool(POOL_NONPAGED, sizeof(THREAD_START_CONTEXT));
	if (!Context)
		return STATUS_INSUFFICIENT_MEMORY;
	
	Context->InstructionPointer = ThreadStart;
	Context->UserContext = ThreadContext;
	
	Status = PsCreateSystemThread(
		OutHandle,
		ObjectAttributes,
		ProcessHandle,
		PspUserThreadStart,
		Context,
		CreateSuspended
	);
	
	if (FAILED(Status))
		MmFreePool(Context);
	
	return Status;
}

BSTATUS OSTerminateThread(HANDLE Handle)
{
	void* ThreadV;
	BSTATUS Status = ExReferenceObjectByHandle(Handle, PsThreadObjectType, &ThreadV);
	if (FAILED(Status))
		return Status;
	
	// TODO: Thread permissions
	
	PKTHREAD Thread = ThreadV;
	KeTerminateThread2(Thread, 1);
	
	return STATUS_SUCCESS;
}

BSTATUS OSSetSuspendedThread(HANDLE Handle, bool IsSuspended)
{
	void* ThreadV;
	BSTATUS Status = ExReferenceObjectByHandle(Handle, PsThreadObjectType, &ThreadV);
	if (FAILED(Status))
		return Status;
	
	// TODO: Thread permissions
	
	PKTHREAD Thread = ThreadV;
	KeSetSuspendedThread(Thread, IsSuspended);
	
	return STATUS_SUCCESS;
}

BSTATUS OSSleep(int Milliseconds)
{
	if (Milliseconds < 1)
		return STATUS_INVALID_PARAMETER;
	
	BSTATUS Status;
	PKTIMER Timer = &KeGetCurrentThread()->SleepTimer;
	KeInitializeTimer(Timer);
	KeSetTimer(Timer, Milliseconds, NULL);	
	
	Status = KeWaitForSingleObject(Timer, true, TIMEOUT_INFINITE, KeGetPreviousMode());
	
	KeCancelTimer(Timer);
	return Status;
}
