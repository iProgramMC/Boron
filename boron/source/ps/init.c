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
	LogMsg("%s:%d", __FILE__, __LINE__);
	// Initialize the object header.
	POBJECT_HEADER Hdr = &PspSystemProcessObject.ObjectHeader;
	LogMsg("%s:%d", __FILE__, __LINE__);
	Hdr->ObjectName = PspSystemProcessName;
	LogMsg("%s:%d", __FILE__, __LINE__);
	Hdr->Flags = OB_FLAG_PERMANENT;
	LogMsg("%s:%d", __FILE__, __LINE__);
	Hdr->NonPagedObjectHeader = &PspSystemProcessNpHeader;
	LogMsg("%s:%d", __FILE__, __LINE__);
	Hdr->BodySize = sizeof(EPROCESS);
	LogMsg("%s:%d", __FILE__, __LINE__);
	
#ifdef DEBUG
	Hdr->Signature = OBJECT_HEADER_SIGNATURE;
#endif
	
	PspSystemProcessNpHeader.ObjectType = NULL; // TBD in PsInitSystem
	LogMsg("%s:%d", __FILE__, __LINE__);
	PspSystemProcessNpHeader.NormalHeader = Hdr;
	LogMsg("%s:%d", __FILE__, __LINE__);
	
	// Initialize the kernel side process.
	BSTATUS Status = KeInitializeProcess(
		&PsSystemProcess.Pcb,
		PRIORITY_NORMAL,
		AFFINITY_ALL
	);
	LogMsg("%s:%d", __FILE__, __LINE__);
	
	if (FAILED(Status))
		KeCrashBeforeSMPInit("KeInitializeProcess failed!");
	
	LogMsg("%s:%d", __FILE__, __LINE__);
	// Use the new page mapping.
	KeSetCurrentPageTable(PsSystemProcess.Pcb.PageMap);
	LogMsg("%s:%d", __FILE__, __LINE__);
	
	MmInitializeVadList(&PsSystemProcess.VadList);
	LogMsg("%s:%d", __FILE__, __LINE__);
	
	// Initialize the heap with a default range.
	MmInitializeHeap(&PsSystemProcess.Heap, sizeof(MMVAD), INITIAL_BEG_VA, (INITIAL_END_VA - INITIAL_BEG_VA) / PAGE_SIZE);
	LogMsg("%s:%d", __FILE__, __LINE__);
	
	// Initialize the address lock.
	ExInitializeRwLock(&PsSystemProcess.AddressLock);
	LogMsg("%s:%d", __FILE__, __LINE__);
	
	// Initialize the handle table.
	// TODO: Defaults chosen arbitrarily.
	if (FAILED(ExCreateHandleTable(16, 16, 2048, 1, &PsSystemProcess.HandleTable)))
	{
		KeCrash("Ps: Could not create handle table for System process");
	}
	
	// Assign an image name.
	LogMsg("%s:%d", __FILE__, __LINE__);
	memset(PsSystemProcess.ImageName, 0, MAX_IMAGE_NAME);
	LogMsg("%s:%d", __FILE__, __LINE__);
	strcpy(PsSystemProcess.ImageName, PspSystemProcessName);
	LogMsg("%s:%d", __FILE__, __LINE__);
}

INIT
bool PsInitSystem()
{
	if (!PsCreateThreadType())
		return false;
	
	if (!PsCreateProcessType())
		return false;
	
	PspInitializeProcessList();
	
	PspSystemProcessNpHeader.ObjectType = PsProcessObjectType;
	
	return true;
}
