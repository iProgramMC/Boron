/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ob/create.c
	
Abstract:
	This module implements create routines for the
	object manager.
	
Author:
	iProgramInCpp - 18 December 2023
***/
#include "obp.h"

// Allocate and initialize an empty object.
//
// This ONLY allocates the object storage and initializes default properties for the object.
// It is the responsibility of caller to add the object to a directory, and perform other
// operations on the object.
BSTATUS ObpAllocateObject(
	POBJECT_TYPE Type,
	const char* Name,
	size_t BodySize,
	void* ParseContext,
	int Flags,
	POBJECT_HEADER* OutObjectHeader)
{
	POBJECT_HEADER ObjectHeader = NULL;
	PNONPAGED_OBJECT_HEADER NonPagedObjectHeader = NULL;
	*OutObjectHeader = NULL;
	
	// Allocates the object itself.
	
	// If totally non paged:
	if (Flags & OB_FLAG_NONPAGED)
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
	ObjectHeader->ParseContext = ParseContext;
	
	// Set the referential fields to each other.
	ObjectHeader->NonPagedObjectHeader = NonPagedObjectHeader;
	NonPagedObjectHeader->NormalHeader = ObjectHeader;
	
	NonPagedObjectHeader->ObjectType = Type;
	
	// N.B. If type is NULL, we are in init.
	if (Type)
		Type->TotalObjectCount++;
	
	// We will have a pointer to this object, so set its pointer count to 1.
	NonPagedObjectHeader->PointerCount = 1;
	NonPagedObjectHeader->HandleCount  = 0;
	
	*OutObjectHeader = ObjectHeader;
	return STATUS_SUCCESS;
}

extern POBJECT_DIRECTORY ObpRootDirectory;

BSTATUS ObCreateObject(
	void** OutObject,
	POBJECT_DIRECTORY ParentDirectory,
	POBJECT_TYPE ObjectType,
	const char* ObjectName,
	int Flags,
	void* ParseContext,
	size_t BodySize)
{
	POBJECT_HEADER Hdr;
	BSTATUS Status;
	
	if (!OutObject || !ObjectType || !ObjectName || !BodySize)
		return STATUS_INVALID_PARAMETER;
	
	if (~Flags & OB_FLAG_NO_DIRECTORY)
	{	
		// If there is a root directory, then we should normalize the
		// parent directory and name, so that:
		// a) The name does not contain backslashes, and
		// b) The parent directory is not NULL.
		if (ObpRootDirectory)
		{	
			// Just roll with it for now and assume all's good
			Status = ObpNormalizeParentDirectoryAndName(
				&ParentDirectory,
				&ObjectName
			);
			
			if (FAILED(Status))
				return Status;
		}
	}
	
	// Allocate the object.
	Status = ObpAllocateObject(
		ObjectType,
		ObjectName,
		BodySize,
		ParseContext,
		Flags,
		&Hdr
	);
	
	if (FAILED(Status))
		return Status;
	
	*OutObject = Hdr->Body;
	
	// If the caller did not request this object to be added to a directory,
	// and ParentDirectory exists, add it to ParentDirectory.
	if ((~Flags & OB_FLAG_NO_DIRECTORY) && ParentDirectory)
	{
		// N.B. ObLinkObject uses the root directory mutex.
		Status = ObLinkObject(ParentDirectory, Hdr->Body);
	}
	
	return Status;
}
