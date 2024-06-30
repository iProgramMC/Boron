/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ob/type.c
	
Abstract:
	This module implements the "object type" type for the
	object manager.
	
Author:
	iProgramInCpp - 18 December 2023
***/
#include "obp.h"

KMUTEX ObpObjectTypeMutex;

POBJECT_TYPE ObpObjectTypeType;
POBJECT_TYPE ObpDirectoryType;
POBJECT_TYPE ObpSymbolicLinkType;

extern POBJECT_DIRECTORY ObpObjectTypesDirectory;

void ObpEnterObjectTypeMutex()
{
	KeWaitForSingleObject(&ObpObjectTypeMutex, false, TIMEOUT_INFINITE);
}

void ObpLeaveObjectTypeMutex()
{
	KeReleaseMutex(&ObpObjectTypeMutex);
}

BSTATUS ObCreateObjectType(
	const char* TypeName,
	POBJECT_TYPE_INFO TypeInfo,
	POBJECT_TYPE* OutObjectType)
{
	// Check if the type name has any invalid characters.
	const char* TypeNameTemp = TypeName;
	
	POBJECT_HEADER Hdr;
	BSTATUS Status;
	
	while (*TypeNameTemp)
	{
		if (*TypeNameTemp == '\\')
			return STATUS_NAME_INVALID;
		
		TypeNameTemp++;
	}

	// Allocate the object.
	Status = ObpAllocateObject(
		ObpObjectTypeType,
		TypeName,
		sizeof(OBJECT_TYPE),
		NULL,
		OB_FLAG_KERNEL | OB_FLAG_NONPAGED | OB_FLAG_PERMANENT,
		&Hdr
	);
	
	if (FAILED(Status))
		return Status;
	
	POBJECT_TYPE NewType = (POBJECT_TYPE) Hdr->Body;
	
	NewType->TypeName = Hdr->ObjectName;
	
	ObpEnterObjectTypeMutex();
	
	// The first ever object type created MUST be the ObjectType object type.
	if (!ObpObjectTypeType)
	{
		ObpObjectTypeType = NewType;
		
		Hdr->NonPagedObjectHeader->ObjectType = NewType;
		NewType->TotalObjectCount = 1;
	}
	
	NewType->TypeInfo = *TypeInfo;
	
	ObpLeaveObjectTypeMutex();
	
	// If we have an object types directory, add it there.
	if (ObpObjectTypesDirectory)
		ObLinkObject(ObpObjectTypesDirectory, NewType);
	
	*OutObjectType = NewType;
	return STATUS_SUCCESS;
}

// ObjectType methods
#ifdef DEBUG
BSTATUS ObpDebugObjectType(void* Object)
{
	POBJECT_TYPE Type = Object;
	
	DbgPrint(
		"Object type - name '%s', total number of objects: %d",
		Type->TypeName,
		Type->TotalObjectCount
	);
	
	return STATUS_SUCCESS;
}
#endif

OBJECT_TYPE_INFO ObpObjectTypeTypeInfo =
{
	// InvalidAttributes
	0,
	// ValidAccessMask
	0,
	// NonPagedPool
	true,
	// MaintainHandleCount
	false,
	// Open
	NULL,
	// Close
	NULL,
	// Delete
	NULL,
	// Parse
	NULL,
	// Secure
	NULL,
#ifdef DEBUG
	// Debug
	ObpDebugObjectType,
#endif
};

extern OBJECT_TYPE_INFO ObpDirectoryTypeInfo;
extern OBJECT_TYPE_INFO ObpSymbolicLinkTypeInfo;

bool ObpInitializeBasicTypes()
{
	if (FAILED(ObCreateObjectType("Type", &ObpObjectTypeTypeInfo, &ObpObjectTypeType))) {
		DbgPrint("Failed to create Type object type");
		return false;
	}
	if (FAILED(ObCreateObjectType("Directory", &ObpDirectoryTypeInfo, &ObpDirectoryType))) {
		DbgPrint("Failed to create Directory object type");
		return false;
	}
	if (FAILED(ObCreateObjectType("SymbolicLink", &ObpSymbolicLinkTypeInfo, &ObpSymbolicLinkType))) {
		DbgPrint("Failed to create SymbolicLink object type");
		return false;
	}
	
	return true;
}

POBJECT_TYPE ObGetObjectType(void* Object)
{
	return OBJECT_GET_HEADER(Object)->NonPagedObjectHeader->ObjectType;
}
