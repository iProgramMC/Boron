/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ob/ref.c
	
Abstract:
	This module implements referencing and dereferencing
	operations on objects.
	
Author:
	iProgramInCpp - 8 December 2023
***/
#include "obp.h"

void ObpAddReferenceToObject(void* Object)
{
	POBJECT_HEADER Hdr = OBJECT_GET_HEADER(Object);
	
	Hdr->NonPagedObjectHeader->PointerCount++;
}

void ObpRemoveReferenceFromObject(void* Object)
{
	POBJECT_HEADER Hdr = OBJECT_GET_HEADER(Object);
	
	Hdr->NonPagedObjectHeader->PointerCount--;
}

BSTATUS ObiReferenceObjectByPointer(
	void* Object,
	POBJECT_TYPE Type,
	KPROCESSOR_MODE AccessMode)
{
	POBJECT_HEADER Hdr = OBJECT_GET_HEADER(Object);
	
	if (Hdr->NonPagedObjectHeader->ObjectType != Type)
		return STATUS_TYPE_MISMATCH;
	
	if ((Hdr->Flags & OB_FLAG_KERNEL) && AccessMode != MODE_KERNEL)
		return STATUS_OBJECT_UNOWNED;
	
	Hdr->NonPagedObjectHeader->PointerCount++;
	
	return STATUS_SUCCESS;
}

void ObiDereferenceObject(void* Object)
{
	POBJECT_HEADER Hdr = OBJECT_GET_HEADER(Object);
	
	// TODO: We'd better hope that the object we are going to dereference isn't being fetched by anything else.
	
	if (--Hdr->NonPagedObjectHeader->PointerCount != 0)
		return;

	if (Hdr->Flags & OB_FLAG_PERMANENT)
		return;
	
	if (KeGetIPL() == IPL_NORMAL)
	{
		// Delete the object now.
		ObpDeleteObject(Object);
		return;
	}
	
	// TODO Enqueue the object onto a deletion queue.
	DbgPrint("ObDereferenceObject: %p has hit a pointer count of zero, but we're at a higher IPL", Object);
}
