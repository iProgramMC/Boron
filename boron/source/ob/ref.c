/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ob/ref.c
	
Abstract:
	This module implements reference counting for objects
	managed by the object manager.
	
Author:
	iProgramInCpp - 23 December 2023
***/
#include "obp.h"

void ObpDeleteObject(void* Object)
{
	//DbgPrint("Deleting object %p", Object);
	POBJECT_HEADER Hdr = OBJECT_GET_HEADER(Object);
	
	ObRefDbgPrint(
		"ObpDeleteObject(%p) (RA: %p) (%s, \"%s\")",
		Object,
		CallerAddress(),
		Hdr->NonPagedObjectHeader->ObjectType ? Hdr->NonPagedObjectHeader->ObjectType->TypeName : "[TYPELESS??]",
		Hdr->ObjectName
	);
	
	if (Hdr->Flags & OB_FLAG_PERMANENT)
		return;
	
	ASSERT(KeGetIPL() <= IPL_NORMAL);
	
	// Invoke the object type's delete function.
	OBJ_DELETE_FUNC DeleteMethod = Hdr->NonPagedObjectHeader->ObjectType->TypeInfo.Delete;
	//DbgPrint("Delete Method: %p", DeleteMethod);
	if (DeleteMethod)
		DeleteMethod(Object);
	
	ObpFreeObject(Hdr);
}

static KTHREAD    ObpReaperThread;
static KEVENT     ObpReaperEvent;
static KSPIN_LOCK ObpReaperLock; // TODO: A mutex would probably work here, but I'm not very confident
static LIST_ENTRY ObpReaperList;

static NO_RETURN void ObpReaperThreadRoutine(UNUSED void* Context)
{
	while (true)
	{
		KeWaitForSingleObject(&ObpReaperEvent, false, TIMEOUT_INFINITE, MODE_KERNEL);
		
		// ObpReaperEvent was pulsed, therefore we should start popping things off
		KIPL Ipl;
		
		while (true)
		{
			KeAcquireSpinLock(&ObpReaperLock, &Ipl);
			if (IsListEmpty(&ObpReaperList))
			{
				KeReleaseSpinLock(&ObpReaperLock, Ipl);
				break;
			}
			
			PLIST_ENTRY Entry = RemoveHeadList(&ObpReaperList);
			KeReleaseSpinLock(&ObpReaperLock, Ipl);
			
			POBJECT_HEADER Header = CONTAINING_RECORD(Entry, OBJECT_HEADER, ReapedListEntry);
			void* Body = Header->Body;
			
			ObpDeleteObject(Body);
		}
	}
}

INIT
bool ObpInitializeReaperThread()
{
	KeInitializeEvent(&ObpReaperEvent, EVENT_SYNCHRONIZATION, false);
	
	KeInitializeThread(
		&ObpReaperThread,
		MmAllocateKernelStack(), // KernelStack
		ObpReaperThreadRoutine,  // StartRoutine
		NULL,                    // StartContext
		KeGetSystemProcess()     // Process
	);
	
	InitializeListHead(&ObpReaperList);
	
	KeSetPriorityThread(&ObpReaperThread, PRIORITY_REALTIME);
	KeReadyThread(&ObpReaperThread);
	
	return true;
}

void ObpReapObject(void* Object)
{
	KIPL Ipl;
	KeAcquireSpinLock(&ObpReaperLock, &Ipl);
	
	POBJECT_HEADER Hdr = OBJECT_GET_HEADER(Object);
	InsertTailList(&ObpReaperList, &Hdr->ReapedListEntry);
	
	KeReleaseSpinLock(&ObpReaperLock, Ipl);
	
	KePulseEvent(&ObpReaperEvent, 1);
}

void* ObReferenceObjectByPointer(void* Object)
{
	POBJECT_HEADER Hdr = OBJECT_GET_HEADER(Object);
	
	ObRefDbgPrint(
		"ObReferenceObjectByPointer(%p) (RA: %p) (%s, \"%s\") -> %d",
		Object,
		CallerAddress(),
		Hdr->NonPagedObjectHeader->ObjectType ? Hdr->NonPagedObjectHeader->ObjectType->TypeName : "[TYPELESS??]",
		Hdr->ObjectName,
		Hdr->NonPagedObjectHeader->PointerCount + 1
	);
	
	ASSERTN(Hdr->Signature == OBJECT_HEADER_SIGNATURE);
	
	AtAddFetch(Hdr->NonPagedObjectHeader->PointerCount, 1);
	//DbgPrint("Ref  (%p) -> %d (%p)", Object, Hdr->NonPagedObjectHeader->PointerCount, CallerAddress());
	
	return Object;
}

void ObDereferenceObject(void* Object)
{
	POBJECT_HEADER Hdr = OBJECT_GET_HEADER(Object);
	
	ASSERTN(Hdr->Signature == OBJECT_HEADER_SIGNATURE);
	
	ObRefDbgPrint(
		"ObDereferenceObject(%p) (RA: %p) (%s, \"%s\") -> %d",
		Object,
		CallerAddress(),
		Hdr->NonPagedObjectHeader->ObjectType ? Hdr->NonPagedObjectHeader->ObjectType->TypeName : "[TYPELESS??]",
		Hdr->ObjectName,
		Hdr->NonPagedObjectHeader->PointerCount - 1
	);
	
	int OldCount = AtFetchAdd(Hdr->NonPagedObjectHeader->PointerCount, -1);
	//DbgPrint("Deref(%p) -> %d (%p)", Object, OldCount - 1, CallerAddress());
	
	ASSERT(OldCount >= 1 && "Underflow error");
	if (OldCount != 1)
		// Nothing to do, return
		return;
	
	// Ok, object has dropped its final reference, check if we should delete it.
	if (Hdr->Flags & OB_FLAG_PERMANENT)
		// Object is permanent, therefore it shouldn't be deleted.
		return;
	
	if (KeGetIPL() > IPL_NORMAL)
	{
		// Enqueue this object for deletion.
		ObpReapObject(Object);
		return;
	}
	
	ObpDeleteObject(Object);
}
