/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ps/init.c
	
Abstract:
	This module implements the initialization code for the
	process manager in Boron.
	
Author:
	iProgramInCpp - 26 November 2023
***/
#include "psp.h"

// Initial Virtual Address Range
#define INITIAL_BEG_VA 0x0000000000001000
#define INITIAL_END_VA 0x00007FFFFFFFF000

NONPAGED_OBJECT_HEADER PspSystemProcessNpHeader;

// To keep the PsSystemProcess symbol working, we'll use a linker script hack.
//
// The reason we want the system process in a "fake" object is two-fold:
//
// - We need access to the system process BEFORE being able to simply create it
//   through the object manager (it launches ExpInitializeExecutive)
//
// - We want to be able to use CURRENT_PROCESS_HANDLE in calls such as OSCreateThread
//   
static_assert(sizeof(OBJECT_HEADER) <= 64);

typedef struct
{
	char Padding[64 - sizeof(OBJECT_HEADER)];
	OBJECT_HEADER ObjectHeader;
	EPROCESS Process;
}
SYSTEM_PROCESS_OBJECT;

SYSTEM_PROCESS_OBJECT PspSystemProcessObject;
extern EPROCESS PsSystemProcess;

POBJECT_TYPE
	PsThreadObjectType,
	PsProcessObjectType;

static char PspSystemProcessName[] = "System Process";

PEPROCESS PsGetSystemProcess()
{
	return &PsSystemProcess;
}

INIT
void PsInitSystemProcess()
{
	// Initialize the object header.
	POBJECT_HEADER Hdr = &PspSystemProcessObject.ObjectHeader;
	Hdr->ObjectName = PspSystemProcessName;
	Hdr->Flags = OB_FLAG_PERMANENT;
	Hdr->NonPagedObjectHeader = &PspSystemProcessNpHeader;
	Hdr->BodySize = sizeof(EPROCESS);
	
#ifdef DEBUG
	Hdr->Signature = OBJECT_HEADER_SIGNATURE;
#endif
	
	PspSystemProcessNpHeader.ObjectType = NULL; // TBD in PsInitSystem
	PspSystemProcessNpHeader.NormalHeader = Hdr;
	
	// Initialize the kernel side process.
	KeInitializeProcess(
		&PsSystemProcess.Pcb,
		PRIORITY_NORMAL,
		AFFINITY_ALL
	);
	
	// Use the new page mapping.
	KeSetCurrentPageTable(PsSystemProcess.Pcb.PageMap);
	
	MmInitializeVadList(&PsSystemProcess.VadList);
	
	// Initialize the heap with a default range.
	MmInitializeHeap(&PsSystemProcess.Heap, sizeof(MMVAD), INITIAL_BEG_VA, (INITIAL_END_VA - INITIAL_BEG_VA) / PAGE_SIZE);
	
	// Initialize the address lock.
	ExInitializeRwLock(&PsSystemProcess.AddressLock);
	
	// Initialize the handle table.
	// TODO: Defaults chosen arbitrarily.
	if (FAILED(ExCreateHandleTable(16, 16, 2048, 1, &PsSystemProcess.HandleTable)))
	{
		KeCrash("Ps: Could not create handle table for System process");
	}
}

INIT
bool PsInitSystem()
{
	if (!PsCreateThreadType())
		return false;
	
	if (!PsCreateProcessType())
		return false;
	
	PspSystemProcessNpHeader.ObjectType = PsProcessObjectType;
	
	// Test code here!
#ifdef DEBUG
	BSTATUS Status;
	void* Addr = NULL;
	
	Status = ExReferenceObjectByHandle(CURRENT_PROCESS_HANDLE, PsProcessObjectType, &Addr);
	if (FAILED(Status))
		KeCrash("TEST: ExReferenceObjectByHandle returned error code %d", Status);
	else
		LogMsg("TEST: ExReferenceObjectByHandle returned the address %p. PsSystemProcess is %p", Addr, &PsSystemProcess);
	
	ObDereferenceObject(Addr);
#endif
	
	return true;
}
