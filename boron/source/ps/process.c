/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ps/process.h
	
Abstract:
	This module implements the process manager process
	object in Boron.
	
Author:
	iProgramInCpp - 30 August 2024
***/
#include "psp.h"

void PspDeleteProcess(void* ProcessV)
{
	UNUSED PEPROCESS Process = ProcessV;
	
	// TODO:
	// I suppose that, when a process is started, the reference count should be biased by one,
	// and when the process is finished, the reference count is decremented.  This way, PspDeleteProcess
	// would only be called if the process object has no more running threads.
	ASSERT(IsListEmpty(&Process->Pcb.ThreadList));
	
	DbgPrint("TODO: PspDeleteProcess");
	
	// This process is no longer running, so it's safe to do a bunch of things here.
	MmTearDownProcess(Process);
}

INIT
bool PsCreateProcessType()
{
	OBJECT_TYPE_INFO ObjectInfo;
	memset (&ObjectInfo, 0, sizeof ObjectInfo);
	
	ObjectInfo.NonPagedPool = true;
	ObjectInfo.MaintainHandleCount = true;
	
	ObjectInfo.Delete = PspDeleteProcess;
	
	BSTATUS Status = ObCreateObjectType(
		"Process",
		&ObjectInfo,
		&PsProcessObjectType
	);
	
	if (FAILED(Status))
	{
		DbgPrint("Could not create Process type: %d", Status);
		return false;
	}
	
	return true;
}

typedef struct
{
	PEPROCESS ParentProcess;
	bool InheritHandles;
}
PROCESS_INIT_CONTEXT;

BSTATUS PspInitializeProcessObject(void* ProcessV, void* Context)
{
	PEPROCESS Process = ProcessV;
	
	BSTATUS Status;
	PROCESS_INIT_CONTEXT* Pic = Context;
	bool InheritHandles = Pic->InheritHandles;
	PEPROCESS ParentProcess = Pic->ParentProcess;
	
	Process->Pcb.PageMap = 0;
	
	// Initialize the kernel side process.
	KeInitializeProcess(
		&Process->Pcb,
		PRIORITY_NORMAL,
		AFFINITY_ALL
	);
	
	// If the initial page map couldn't be created, throw an out of memory error.
	if (!Process->Pcb.PageMap)
		return STATUS_INSUFFICIENT_MEMORY;
	
	MmInitializeVadList(&PsSystemProcess.VadList);
	
	// Initialize the heap with a default range.
	MmInitializeHeap(&PsSystemProcess.Heap, sizeof(MMVAD), INITIAL_BEG_VA, (INITIAL_END_VA - INITIAL_BEG_VA) / PAGE_SIZE);
	
	// Initialize the address lock.
	ExInitializeRwLock(&PsSystemProcess.AddressLock);
	
	// Initialize the handle table.
	if (InheritHandles)
	{
		Status = ObDuplicateHandleTable(&Process->HandleTable, ParentProcess->HandleTable);
		if (FAILED(Status))
			goto Fail;
	}
	else
	{
		// TODO: Defaults chosen arbitrarily.
		Status = ExCreateHandleTable(16, 16, 2048, 1, &Process->HandleTable);
		if (FAILED(Status))
			goto Fail;
	}
	
	if (SUCCEEDED(Status))
		return Status;
	
Fail:
	if (Process->Pcb.PageMap)
		MmFreePhysicalPage(MmPhysPageToPFN(Process->Pcb.PageMap));
	
	Process->Pcb.PageMap = 0;
	return Status;
}

BSTATUS OSCreateProcess(PHANDLE OutHandle, POBJECT_ATTRIBUTES ObjectAttributes, HANDLE ParentProcessHandle, bool InheritHandles)
{
	PROCESS_INIT_CONTEXT Pic;
	Pic.InheritHandles = InheritHandles;
	Pic.ParentProcess = NULL;
	
	// If we are inheriting handles yet we have no parent to inherit from, this is
	// invalid, as we do not allow inheritance from the system process, to which
	// processes without a parent process handle get added.
	if (InheritHandles && !ParentProcessHandle)
		return STATUS_INVALID_PARAMETER;
	
	OBJECT_ATTRIBUTES Copy;
	BSTATUS Status = MmSafeCopy(&Copy, ObjectAttributes, sizeof(OBJECT_ATTRIBUTES), KeGetPreviousMode(), false);
	if (FAILED(Status))
		return Status;
	
	// The object may not have a name.
	// The Process object is quite expensive, as such, we do not allow it to be placed
	// in the object namespace with a name, because it may be left over in the object
	// namespace after it exits.
	if (Copy.ObjectNameLength || Copy.ObjectName)
		return STATUS_INVALID_PARAMETER;
	
	void* ParentProcessRef = NULL;
	
	if (ParentProcessHandle)
	{
		// Try and reference this parent process.
		Status = ExReferenceObjectByHandle(ParentProcessHandle, PsProcessObjectType, &ParentProcessRef);
		Pic.ParentProcess = ParentProcessRef;
		
		if (FAILED(Status))
			return Status;
	}
	
	Status = ExCreateObjectUserCall(OutHandle, ObjectAttributes, PsProcessObjectType, sizeof(EPROCESS), PspInitializeProcessObject, &Pic);
	
	ObDereferenceObject(ParentProcessRef);
	return Status;
}
