/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	mm/query.c
	
Abstract:
	This module implements the OSQueryVirtualMemoryInformation system
	service.  It allows a process to get the range covered by a mapping
	based on a pointer from within that mapping.
	
Author:
	iProgramInCpp - 5 January 2026
***/

#include "mi.h"

static PMMADDRESS_NODE MmpGetHeapNodeFromAddress(PMMHEAP Heap, uintptr_t VirtualAddress)
{
	PRBTREE_ENTRY Entry = LookUpItemApproximateRbTree(&Heap->Tree, VirtualAddress);
	if (!Entry)
		return NULL;
	
	PMMADDRESS_NODE AddressNode = CONTAINING_RECORD(Entry, MMADDRESS_NODE, Entry);
	if (VirtualAddress < AddressNode->StartVa ||
		VirtualAddress >= AddressNode->StartVa + AddressNode->Size * PAGE_SIZE)
		return NULL;
	
	return AddressNode;
}

BSTATUS OSQueryVirtualMemoryInformation(
	HANDLE ProcessHandle,
	PVIRTUAL_MEMORY_INFORMATION OutInformation,
	uintptr_t VirtualAddress
)
{
	if (VirtualAddress > MM_USER_SPACE_END)
		return STATUS_INVALID_PARAMETER;
	
	BSTATUS Status = STATUS_SUCCESS;
	PEPROCESS Process = PsGetCurrentProcess();
	PEPROCESS ProcessRestore = NULL;
	VIRTUAL_MEMORY_INFORMATION Information;
	memset(&Information, 0, sizeof Information);
	
	if (ProcessHandle != CURRENT_PROCESS_HANDLE)
	{
		Status = ExReferenceObjectByHandle(ProcessHandle, PsProcessObjectType, (void**) &Process);
		if (FAILED(Status))
			return Status;
		
		ProcessRestore = PsSetAttachedProcess(Process);
	}
	
	PMMVAD_LIST VadList = MmLockVadListProcess(Process);
	
	PMMVAD Vad = MmLookUpVadByAddress(VadList, VirtualAddress);
	if (Vad)
	{
		Information.Start = Vad->Node.StartVa;
		Information.Size = Vad->Node.Size * PAGE_SIZE;
		
		Information.Flags.Free = 0;
		Information.Flags.Private = Vad->Flags.Private;
		Information.Flags.Protection = Vad->Flags.Protection;
	}
	
	MmUnlockVadList(VadList);
	
	if (!Vad)
	{
		Information.Flags.Free = 1;
		
		Status = KeWaitForSingleObject(&Process->Heap.Mutex, true, TIMEOUT_INFINITE, MODE_KERNEL);
		if (FAILED(Status))
			goto Fail;
		
		PMMADDRESS_NODE AddressNode = MmpGetHeapNodeFromAddress(&Process->Heap, VirtualAddress);
		if (!AddressNode)
		{
			DbgPrint(
				"OSQueryVirtualMemoryInformation: Cannot find address %p in either VAD list or heap!",
				VirtualAddress
			);
			goto Fail;
		}
		
		Information.Start = AddressNode->StartVa;
		Information.Size = AddressNode->Size * PAGE_SIZE;
		
		KeReleaseMutex(&Process->Heap.Mutex);
	}
	
Fail:
	if (ProcessHandle != CURRENT_PROCESS_HANDLE)
	{
		PsSetAttachedProcess(ProcessRestore);
		ObDereferenceObject(Process);
	}
	
	if (SUCCEEDED(Status))
	{
		Status = MmSafeCopy(OutInformation, &Information, sizeof Information, KeGetPreviousMode(), true);
	}
	
	return Status;
}
