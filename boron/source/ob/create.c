/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ob/init.c
	
Abstract:
	This module implements the initialization code for the
	object manager.
	
Author:
	iProgramInCpp - 7 December 2023
***/
#include "obp.h"

// Allocate and initialize an empty object.
BSTATUS ObiAllocateObject(
	POBJECT_TYPE Type,
	const char* Name,
	size_t BodySize,
	bool NonPaged,
	void* ParentDirectory,
	void* ParseContext,
	int Flags,
	POBJECT_HEADER* OutObjectHeader)
{
	POBJECT_HEADER ObjectHeader = NULL;
	PNONPAGED_OBJECT_HEADER NonPagedObjectHeader = NULL;
	*OutObjectHeader = NULL;
	
	// Allocates the object itself.
	
	// If totally non paged:
	if (NonPaged)
	{
		// It'll look as follows:
		// [NonPaged Object Header]
		// [Regular  Object Header]
		// [Object Body]
		
		size_t Size = sizeof(NONPAGED_OBJECT_HEADER) + sizeof(OBJECT_HEADER) + BodySize;
		NonPagedObjectHeader = MmAllocatePool(POOL_NONPAGED, Size);
		
		if (!NonPagedObjectHeader)
			// Womp womp.
			return STATUS_INSUFFICIENT_MEMORY;
		
		ObjectHeader = (POBJECT_HEADER) &NonPagedObjectHeader[1];
		
		// Of course, initialize it.
		memset (NonPagedObjectHeader, 0, Size);
		
		// Allocate and copy the name.
		size_t NameLength = strlen (Name);
		ObjectHeader->ObjectName = MmAllocatePool(POOL_NONPAGED, NameLength + 1);
		if (!ObjectHeader->ObjectName)
		{
			// Womp, womp. Deallocate everything and return.
			MmFreePool(NonPagedObjectHeader);
			return STATUS_INSUFFICIENT_MEMORY;
		}
		
		memcpy(ObjectHeader->ObjectName, Name, NameLength + 1);
	}
	else
	{
		// Regular allocation.
		ObjectHeader = MmAllocatePool(POOL_PAGED, sizeof(OBJECT_HEADER) + BodySize);
		if (!ObjectHeader)
			return STATUS_INSUFFICIENT_MEMORY;
		
		NonPagedObjectHeader = MmAllocatePool(POOL_NONPAGED, sizeof(NONPAGED_OBJECT_HEADER));
		if (!NonPagedObjectHeader)
		{
			MmFreePool(ObjectHeader);
			return STATUS_INSUFFICIENT_MEMORY;
		}
		
		// Clear the newly allocated memory.
		memset(NonPagedObjectHeader, 0, sizeof(NONPAGED_OBJECT_HEADER));
		memset(ObjectHeader, 0, sizeof(OBJECT_HEADER));
		
		// Allocate space for the name in paged pool.
		size_t NameLength = strlen (Name);
		ObjectHeader->ObjectName = MmAllocatePool(POOL_PAGED, NameLength + 1);
		if (!ObjectHeader->ObjectName)
		{
			// Oh no! Deallocate everything.
			MmFreePool(NonPagedObjectHeader);
			MmFreePool(ObjectHeader);
			return STATUS_INSUFFICIENT_MEMORY;
		}
		
		memcpy(ObjectHeader->ObjectName, Name, NameLength + 1);
	}
	
	// Initializes the object.
	ObjectHeader->Flags = Flags;
	ObjectHeader->ParentDirectory = ParentDirectory;
	ObjectHeader->ParseContext    = ParseContext;
	
	// Set the referential fields to each other.
	ObjectHeader->NonPagedObjectHeader = NonPagedObjectHeader;
	NonPagedObjectHeader->NormalHeader = ObjectHeader;
	
	NonPagedObjectHeader->ObjectType = Type;
	
	// N.B. If type is NULL, we are in init.
	if (Type)
		Type->TotalObjectCount++;
	
	if (ParentDirectory)
	{
		ObpAddObjectToDirectory(ParentDirectory, ObjectHeader);
	}
	
	// We will have a pointer to this object, so set its pointer count to 1.
	NonPagedObjectHeader->PointerCount = 1;
	NonPagedObjectHeader->HandleCount  = 0;
	
	*OutObjectHeader = ObjectHeader;
	return STATUS_SUCCESS;
}

BSTATUS ObiCreateObject(
	void** OutObject,
	POBJECT_DIRECTORY ParentDirectory,
	POBJECT_TYPE ObjectType,
	const char* ObjectName,
	int Flags,
	bool NonPaged,
	void* ParseContext,
	size_t BodySize)
{
	POBJECT_HEADER Hdr;
	BSTATUS Status;
	
	// Allocate the object.
	Status = ObiAllocateObject(
		ObjectType,
		ObjectName,
		BodySize,
		NonPaged,
		ParentDirectory,
		ParseContext,
		Flags,
		&Hdr
	);
	
	if (FAILED(Status))
		return Status;
	
	*OutObject = Hdr->Body;
	return STATUS_SUCCESS;
}
