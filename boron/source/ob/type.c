/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ob/type.c
	
Abstract:
	This module implements the "ObjectType" object type. It
	provides create type, .. functions.
	
Author:
	iProgramInCpp - 7 December 2023
***/
#include "obp.h"

POBJECT_TYPE ObpObjectTypeType;
POBJECT_TYPE ObpDirectoryType;
POBJECT_TYPE ObpSymbolicLinkType;

OBJECT_TYPE_INFO ObpObjectTypeTypeInfo;
OBJECT_TYPE_INFO ObpDirectoryTypeInfo;
OBJECT_TYPE_INFO ObpSymbolicLinkTypeInfo;

// ObjectType methods
BSTATUS ObpDebugObjectType(void* Object)
{
	POBJECT_TYPE Type = Object;
	POBJECT_HEADER Hdr = OBJECT_GET_HEADER(Type);
	
	DbgPrint(
		"Object type - name '%s', total number of objects: %d",
		Hdr->ObjectName,
		Type->TotalObjectCount
	);
	
	return STATUS_SUCCESS;
}



// Object type creation and initialization

void ObpInitializeObjectTypeInfos()
{
	POBJECT_TYPE_INFO O;
	O = &ObpObjectTypeTypeInfo;
	
	O->NonPagedPool = true;
	O->Debug = ObpDebugObjectType;
	
	O = &ObpDirectoryTypeInfo;
	O->NonPagedPool = true;
	// TODO
	
	O = &ObpSymbolicLinkTypeInfo;
	O->NonPagedPool = true;
	// TODO
}

BSTATUS ObiCreateObjectType(
	const char* TypeName,
	POBJECT_TYPE_INFO TypeInfo,
	POBJECT_TYPE* OutObjectType)
{
	POBJECT_HEADER Hdr;
	BSTATUS Status;

	// Allocate the object.
	Status = ObiAllocateObject(
		ObpObjectTypeType,
		TypeName,
		sizeof(OBJECT_TYPE),
		true,
		NULL,
		NULL,
		OB_FLAG_KERNEL | OB_FLAG_PERMANENT,
		&Hdr
	);
	
	if (Status != STATUS_SUCCESS)
		return Status;
	
	POBJECT_TYPE NewType = (POBJECT_TYPE) Hdr->Body;
	
	// The first ever object type created MUST be the ObjectType object type.
	if (!ObpObjectTypeType)
	{
		ObpObjectTypeType = NewType;
		
		Hdr->NonPagedObjectHeader->ObjectType = NewType;
		NewType->TotalObjectCount = 1;
	}
	
	NewType->TypeInfo = *TypeInfo;
	
	*OutObjectType = NewType;
	return STATUS_SUCCESS;
}
