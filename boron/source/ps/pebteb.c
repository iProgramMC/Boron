/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ps/pebteb.c
	
Abstract:
	This module implements functions related to the PEB and
	TEB of a process/thread.
	
Author:
	iProgramInCpp - 7 October 2025
***/
#include <ke.h>
#include <ps.h>

void* OSGetCurrentTeb()
{
	return KeGetCurrentThread()->TebPointer;
}

void* OSGetCurrentPeb()
{
	return KeGetCurrentProcess()->PebPointer;
}

// Sets the FS base for user mode.
BSTATUS OSSetCurrentTeb(void* Ptr)
{
	KeGetCurrentThread()->TebPointer = Ptr;
	
#ifdef TARGET_AMD64
	KeSetMSR(MSR_FS_BASE, (uint64_t) Ptr);
#endif
	
	return STATUS_SUCCESS;
}

// Sets the GS base for user mode.
BSTATUS OSSetCurrentPeb(void* Ptr)
{
	KeGetCurrentProcess()->PebPointer = Ptr;
	
#ifdef TARGET_AMD64
	// we assign the KERNELGSBASE MSR because that's what
	// will be swapped to when a return to user mode happens
	KeSetMSR(MSR_GS_BASE_KERNEL, (uint64_t) Ptr);
#endif
	
	return STATUS_SUCCESS;
}

// Sets the GS base for a process.
//
// If ProcessHandle is the current process' handle, then it assigns
// the PEB for this process. Otherwise, this system service must be
// used before creating the first thread.
BSTATUS OSSetPebProcess(HANDLE ProcessHandle, void* PebPtr)
{
	BSTATUS Status;
	PEPROCESS Process;
	void* ProcessV;
	
	if (ProcessHandle == CURRENT_PROCESS_HANDLE)
		return OSSetCurrentPeb(PebPtr);
	
	Status = ObReferenceObjectByHandle(ProcessHandle, PsProcessObjectType, &ProcessV);
	if (FAILED(Status))
		return Status;
	
	Process = ProcessV;
	if (IsListEmpty(&Process->Pcb.ThreadList))
		Process->Pcb.PebPointer = PebPtr;
	else
		Status = STATUS_STILL_RUNNING;
	
	ObDereferenceObject(ProcessV);
	return Status;
}
