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
	DbgPrint("Deleting object %p", Object);
	POBJECT_HEADER Hdr = OBJECT_GET_HEADER(Object);
	
	if (Hdr->Flags & OB_FLAG_PERMANENT)
		return;
	
	ASSERT(KeGetIPL() <= IPL_NORMAL);
	
	// Invoke the object type's delete function.
	OBJ_DELETE_FUNC DeleteMethod = Hdr->NonPagedObjectHeader->ObjectType->TypeInfo.Delete;
	if (DeleteMethod)
		DeleteMethod(Object);
	
	// Free the memory that this object occupied.
	if (Hdr->Flags & OB_FLAG_NONPAGED)
	{
		// Free the nonpaged object header as one big block.
		MmFreePool (Hdr->NonPagedObjectHeader);
	}
	else
	{
		// Free the nonpaged object header, and then the regular object header.
		MmFreePool (Hdr->NonPagedObjectHeader);
		MmFreePool (Hdr);
	}
}

void ObReferenceObjectByPointer(void* Object)
{
	POBJECT_HEADER Hdr = OBJECT_GET_HEADER(Object);
	AtAddFetch(Hdr->NonPagedObjectHeader->PointerCount, 1);
}

void ObDereferenceObject(void* Object)
{
	POBJECT_HEADER Hdr = OBJECT_GET_HEADER(Object);
	
	int OldCount = AtFetchAdd(Hdr->NonPagedObjectHeader->PointerCount, -1);
	
	ASSERT(OldCount >= 1 && "Underflow error");
	if (OldCount != 1)
		// Nothing to do, return
		return;
	
	// Ok, object has dropped its final reference, check if we should delete it.
	if (Hdr->Flags & OB_FLAG_PERMANENT)
		// Object is permanent, therefore it shouldn't be deleted.
		return;
	
	DbgPrint("ObDereferenceObject: object %p lost its final reference TODO", Object);
	if (KeGetIPL() > IPL_NORMAL)
	{
		// Enqueue this object for deletion. TODO
		DbgPrint("ObDereferenceObject: Object %p lost its final reference at IPL %d! TODO", Object, KeGetIPL());
		return;
	}
	
	ObpDeleteObject(Object);
}
