/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ob/create.c
	
Abstract:
	This module implements the create functions for the
	object manager.
	
Author:
	iProgramInCpp - 26 November 2023
***/
#include "obp.h"

POBJECT_TYPE ObpTypeObjectType;
POBJECT_TYPE ObpDirectoryObjectType;
POBJECT_TYPE ObpSymLinkObjectType;

BSTATUS ObpAllocateObject(
	OATTRIBUTES Attributes,
	void* RootDirectory,
	void* ParseContext,
	KPROCESSOR_MODE OwnerShipMode,
	POBJECT_TYPE ObjectType,
	const char* ObjectName,
	size_t ObjectBodySize,
	POBJECT_HEADER* ObjectHeaderOut)
{
	PNONPAGED_OBJECT_HEADER NonPagedObjectHeader;
	POBJECT_HEADER ObjectHeader;
	
	// Allocate and initialize the object.
	
	// If the object type is not specified or specifies non-paged pool, then
	// perform the allocation, in one fell swoop, in the non paged pool.
	// Otherwise, allocate the non paged object header from non paged pool and
	// the object header and body from the paged pool.
	
	if (!ObjectType || ObjectType->Info.IsNonPagedPool)
	{
		NonPagedObjectHeader = MmAllocatePool(
			true,
			sizeof(NONPAGED_OBJECT_HEADER) + sizeof(OBJECT_HEADER) + ObjectBodySize);
		
		if (!NonPagedObjectHeader)
			return STATUS_INSUFFICIENT_MEMORY;
		
		ObjectHeader = (POBJECT_HEADER) &NonPagedObjectHeader[1];
	}
	else
	{
		NonPagedObjectHeader = MmAllocatePool(
			true,
			sizeof(NONPAGED_OBJECT_HEADER));
		
		if (!NonPagedObjectHeader)
			return STATUS_INSUFFICIENT_MEMORY;
		
		ObjectHeader = MmAllocatePool(
			false,
			sizeof(OBJECT_HEADER) + ObjectBodySize);
		
		if (!ObjectHeader)
		{
			MmFreePool(NonPagedObjectHeader);
			return STATUS_INSUFFICIENT_MEMORY;
		}
	}
	
	// Initialize the non paged object header.
	NonPagedObjectHeader->Object = &ObjectHeader->Body;
	NonPagedObjectHeader->PointerCount = 1;
	NonPagedObjectHeader->HandleCount = 0;
	NonPagedObjectHeader->Type = ObjectType;
	
	// Initialize the object header.
	ObjectHeader->Size = ObjectBodySize;
	ObjectHeader->Flags = OBH_FLAG_NEW_OBJECT;
	ObjectHeader->NonPagedHeader = NonPagedObjectHeader;
	
	if (OwnerShipMode == MODE_KERNEL)
		ObjectHeader->Flags |= OBH_FLAG_KERNEL_OBJECT;
	
	if (Attributes & OBJ_PERMANENT)
		ObjectHeader->Flags |= OBH_FLAG_PERMANENT_OBJECT;
	
	ObjectHeader->RootDirectory = RootDirectory;
	ObjectHeader->Attributes = Attributes;
	ObjectHeader->ParseContext = ParseContext;
	ObjectHeader->ObjectName = ObjectName;
	
	*ObjectHeaderOut = ObjectHeader;
	return STATUS_SUCCESS;
}

BSTATUS ObCreateObject(
	POBJECT_TYPE ObjectType,
	POBJECT_ATTRIBUTES ObjectAttributes,
	KPROCESSOR_MODE OwnerShipMode,
	void* ParseContext,
	size_t ObjectBodySize,
	void** Object)
{
	BSTATUS Status;
	POBJECT_HEADER ObjectHeader = NULL;
	
	// Allocate and initialize the object.
	Status = ObpAllocateObject(
		ObjectAttributes->Attributes,
		ObjectAttributes->RootDirectory,
		ParseContext,
		OwnerShipMode,
		ObjectType,
		ObjectAttributes->ObjectName,
		ObjectBodySize,
		&ObjectHeader);
	
	if (Status)
		return Status;
	
	*Object = &ObjectHeader->Body;
	return STATUS_SUCCESS;
}


BSTATUS ObCreateObjectType(
	const char* TypeName,
	POBJECT_TYPE_INITIALIZER Initializer,
	POBJECT_TYPE *ObjectType)
{
	// Return an error if invalid type attributes, or no type name specified.
	// Note that no type name is OK if the type directory object doesn't exist yet.
	
	if (!TypeName || !(*TypeName) || !Initializer ||
		(Initializer->MaintainHandleCount && (!Initializer->Open && !Initializer->Close)))
	{
		return STATUS_INVALID_PARAMETER;
	}
	
#ifdef DEBUG
	// Check if the type name contains backslashes. It'd better not!
	const char* TypeName2 = TypeName;
	while (*TypeName2)
	{
		if (*TypeName2 == OBJ_NAME_PATH_SEPARATOR)
			return STATUS_OBJNAME_INVALID;
		TypeName2++;
	}
#endif
	
	// Check if the type name already exists in the \ObjectTypes directory.
	// Return an error if it does, add the name to the directory otherwise.
	
	// Allocate a buffer for the type name.
	size_t TypeNameLength = strlen(TypeName);
	char* ObjectName = MmAllocatePool(POOL_NONPAGED, TypeNameLength + 1);
	
	if (!ObjectName)
		return STATUS_INSUFFICIENT_MEMORY;
	
	strcpy(ObjectName, TypeName);
	
	POBJECT_HEADER NewObjectTypeHeader;
	
	int Status = ObpAllocateObject(
		0,
		0,
		NULL,
		MODE_KERNEL,
		ObpTypeObjectType,
		ObjectName,
		sizeof(OBJECT_TYPE),
		&NewObjectTypeHeader);
	
	if (Status)
	{
		MmFreePool(ObjectName);
		return Status;
	}
	
	// It's of course a kernel permanent object.
	NewObjectTypeHeader->Flags |= OBH_FLAG_KERNEL_OBJECT | OBH_FLAG_PERMANENT_OBJECT;
	
	POBJECT_TYPE NewObjectType = (POBJECT_TYPE)NewObjectTypeHeader->Body;
	memset(NewObjectType, 0, sizeof *NewObjectType);
	
	NewObjectType->Name = ObjectName;
	
	if (!ObpTypeObjectType)
	{
		ObpTypeObjectType = NewObjectType;
		
		// Assign the type of this object.
		NewObjectTypeHeader->NonPagedHeader->Type = ObpTypeObjectType;
		NewObjectType->TotalObjectCount = 1;
	}
	
	// TODO
	
	return STATUS_SUCCESS;
}
