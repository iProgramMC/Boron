/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ob/object.c
	
Abstract:
	This module implements common functions to handle objects.
	
Author:
	iProgramInCpp - 8 December 2023
***/
#include "obp.h"

#ifdef DEBUG
BSTATUS ObiDebugObject(void* Object)
{
	POBJECT_HEADER Hdr = OBJECT_GET_HEADER(Object);
	POBJECT_TYPE Type = Hdr->NonPagedObjectHeader->ObjectType;
	
	if (!Type->TypeInfo.Debug)
		return STATUS_SUCCESS; // Unimplemented.
	
	return Type->TypeInfo.Debug(Object);
}
#endif

void ObpAddReferenceToObject(void* Object)
{
	POBJECT_HEADER Hdr = OBJECT_GET_HEADER(Object);
	
	Hdr->NonPagedObjectHeader->PointerCount++;
}

void ObiDereferenceByPointerObject(void* Object)
{
	POBJECT_HEADER Hdr = OBJECT_GET_HEADER(Object);
	
	Hdr->NonPagedObjectHeader->PointerCount--;
}
